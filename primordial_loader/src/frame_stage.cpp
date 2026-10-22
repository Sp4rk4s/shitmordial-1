#include "frame_stage.h"
#include "config.h"
#include "menu_ui.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cstdarg>
#include "../../third_party/minhook/include/MinHook.h"

namespace frame_stage {
namespace {
constexpr uintptr_t base = cfg::IMAGE_BASE;
template<class T> T& at(uint32_t rva) { return *reinterpret_cast<T*>(base + rva); }

struct RecvTable;
struct RecvProp {
    const char* name; int type, flags, buffer_size; bool inside_array; void* extra;
    RecvProp* array_prop; void* array_length_proxy; void* proxy; void* table_proxy;
    RecvTable* table; int offset, stride, elements; const char* parent_array;
};
static_assert(sizeof(RecvProp) == 0x3C);
struct RecvTable { RecvProp* props; int count; void* decoder; const char* name; };
struct ClientClass { void* create; void* create_event; const char* name; RecvTable* table; ClientClass* next; int id; };

void* client = nullptr;
volatile LONG prepared = 0;

void* factory(const char* module, const char* name) {
    using Create = void*(__cdecl*)(const char*, int*);
    auto fn = reinterpret_cast<Create>(GetProcAddress(GetModuleHandleA(module), "CreateInterface"));
    return fn ? fn(name, nullptr) : nullptr;
}

int find_prop(const RecvTable* table, const char* name) {
    for (int i = 0; i < table->count; ++i) {
        const auto& prop = table->props[i];
        if (!std::strcmp(prop.name, name)) return prop.offset;
        if (prop.table && prop.table->count) {
            int nested = find_prop(prop.table, name);
            if (nested >= 0) return prop.offset + nested;
        }
    }
    return -1;
}

RecvProp* top_prop(const char* table, const char* name) {
    using AllClasses = ClientClass*(__thiscall*)(void*);
    for (auto* cls = (*reinterpret_cast<AllClasses**>(client))[8](client); cls; cls = cls->next)
        if (cls->table && !std::strcmp(cls->table->name, table))
            for (int i = 0; i < cls->table->count; ++i)
                if (!std::strcmp(cls->table->props[i].name, name)) return &cls->table->props[i];
    return nullptr;
}

int netvar(const char* table, const char* name) {
    using AllClasses = ClientClass*(__thiscall*)(void*);
    auto* cls = (*reinterpret_cast<AllClasses**>(client))[8](client);
    for (; cls; cls = cls->next)
        if (cls->table && !std::strcmp(cls->table->name, table)) return find_prop(cls->table, name);
    return -1;
}

// Unique pattern match in a loaded module; '?' bytes are wildcards.
uintptr_t find(const char* module, const char* pattern, size_t length, size_t imm_at = SIZE_MAX) {
    auto* image = reinterpret_cast<const unsigned char*>(GetModuleHandleA(module));
    if (!image) return 0;
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(image + reinterpret_cast<const IMAGE_DOS_HEADER*>(image)->e_lfanew);
    const size_t size = nt->OptionalHeader.SizeOfImage;
    uintptr_t result = 0;
    for (size_t i = 0; i + length <= size; ++i) {
        size_t j = 0;
        while (j < length && (pattern[j] == '?' || static_cast<unsigned char>(pattern[j]) == image[i + j])) ++j;
        if (j != length) continue;
        uintptr_t value = reinterpret_cast<uintptr_t>(image + i);
        if (imm_at != SIZE_MAX) {
            std::memcpy(&value, image + i + imm_at, 4);
            // Matches loading an address outside the module are false positives.
            if (value < reinterpret_cast<uintptr_t>(image) || value >= reinterpret_cast<uintptr_t>(image) + size) continue;
        }
        if (result && result != value) return 0;
        result = value;
    }
    return result;
}

// Stale captured-process netvar cells (dword pointers to field offsets), repaired to point at these.
enum Field { weapons, item_definition, world_model_handle, model_index, viewmodel_weapon, quality,
             world_model_index, team, viewmodel, life_state, alive, viewmodel_owner,
             xuid_high, xuid_low, item_id_high, item_id_low, fallback_paint_kit, fallback_wear,
             wearables, body, account_id, field_count };
uint32_t fields[field_count]{};
struct Cell { uint32_t rva, size, stale; Field field; unsigned count; };
constexpr Cell cells[] = {
    {0x100900, 0x8EB, 0x2DA912B9, weapons, 1},
    {0x100900, 0x8EB, 0x2DA842AE, item_definition, 3},
    {0x100900, 0x8EB, 0x2D8F7135, world_model_handle, 1},
    {0x100900, 0x8EB, 0x2D8F1304, model_index, 2},
    {0x100900, 0x8EB, 0x2D8F8040, viewmodel_weapon, 1},
    {0x100900, 0x8EB, 0x2DAA0F95, quality, 1},
    {0x100900, 0x8EB, 0x2D8F6D3D, world_model_index, 1},
    {0x100900, 0x8EB, 0x2D92D648, team, 1},
    // Glove path 100C85..1011DB.
    {0x100900, 0x8EB, 0x2DA91EA7, wearables, 1},
    {0x100900, 0x8EB, 0x2D8F6120, body, 1},
    {0x100900, 0x8EB, 0x2DC9D617, item_id_high, 1},
    {0x100900, 0x8EB, 0x2D951F5E, xuid_high, 1},
    {0x100900, 0x8EB, 0x2D951EDF, xuid_low, 1},
    {0x0FB820, 0x0C2, 0x2D92D648, team, 2},
    {0x369340, 0x021, 0x2DAF03AF, viewmodel, 1},
    {0x36ABF0, 0x066, 0x2D9B7EE7, alive, 2},
    {0x36ABF0, 0x066, 0x2D8F9170, life_state, 1},
    // Original viewmodel m_nSequence proxy 157B60 (knife animation remap).
    {0x157B60, 0x652, 0x2D8F8438, viewmodel_owner, 1},
    {0x157B60, 0x652, 0x2D8F8040, viewmodel_weapon, 1},
    {0x157B60, 0x652, 0x2DA842AE, item_definition, 2},
    {0x157B60, 0x652, 0x2D8F1304, model_index, 2},
    // Original paint kit applier 1508E0 (detour of the item fallback-attribute update).
    {0x1508E0, 0x34A, 0x2D951F5E, xuid_high, 1},
    {0x1508E0, 0x34A, 0x2D951EDF, xuid_low, 1},
    {0x1508E0, 0x34A, 0x2DA842AE, item_definition, 1},
    {0x1508E0, 0x34A, 0x2DC9D617, item_id_high, 2},
    {0x1508E0, 0x34A, 0x2DD5310F, item_id_low, 3},
    {0x1508E0, 0x34A, 0x2D951FDD, fallback_paint_kit, 1},
    {0x1508E0, 0x34A, 0x2D9520DB, fallback_wear, 1},
};

unsigned occurrences(const Cell& cell) {
    unsigned n = 0;
    for (uint32_t i = 0; i + 4 <= cell.size; ++i)
        n += !std::memcmp(reinterpret_cast<void*>(base + cell.rva + i), &cell.stale, 4);
    return n;
}

void write_code(uint32_t rva, const void* bytes, size_t size) {
    auto* target = reinterpret_cast<void*>(base + rva);
    DWORD old = 0;
    VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &old);
    std::memcpy(target, bytes, size);
    VirtualProtect(target, size, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, size);
}

bool bytes_are(uint32_t rva, const char* expected, size_t size) {
    return !std::memcmp(reinterpret_cast<void*>(base + rva), expected, size);
}

// primordial_frame_stage.log beside csgo.exe.
void write_log(const char* line) {
    char path[MAX_PATH];
    DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
    while (length && path[length - 1] != '\\') --length;
    std::strcpy(path + length, "primordial_frame_stage.log");
    if (FILE* file = std::fopen(path, "a")) { std::fprintf(file, "%s\n", line); std::fclose(file); }
}

void say(const char* format, ...) {
    char line[512];
    va_list args; va_start(args, format); std::vsnprintf(line, sizeof(line), format, args); va_end(args);
    size_t n = std::strlen(line);
    while (n && line[n - 1] == '\n') line[--n] = 0;
    std::puts(line); write_log(line);
}

bool bind(uint32_t rva, uintptr_t value, const char* what) {
    if (!at<uintptr_t>(rva)) at<uintptr_t>(rva) = value;
    say("    %-22s RVA %06X -> %08X\n", what, rva, static_cast<unsigned>(at<uintptr_t>(rva)));
    return at<uintptr_t>(rva) != 0;
}

// Original installer 157A70 swaps DT_BaseViewModel m_nSequence's proxy to 157B60 and keeps
// {prop, original} at 7FB95C/7FB960; 157B60 forwards through 7FB960.
using RecvProxy = void(__cdecl*)(const void*, void*, void*);
RecvProp* sequence_prop = nullptr;
RecvProxy sequence_original = nullptr;
volatile LONG sequence_disabled = 0;

void __cdecl sequence_proxy(const void* data, void* object, void* out) {
    if (!sequence_disabled) {
        __try {
            reinterpret_cast<RecvProxy>(base + 0x157B60)(data, object, out);
            return;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            InterlockedExchange(&sequence_disabled, 1);
            sequence_prop->proxy = reinterpret_cast<void*>(sequence_original);
        }
    }
    sequence_original(data, object, out);
}

// Unrecovered captured-client helper called as (viewmodel, sequence) just before the game proxy,
// which itself applies the sequence; left as a no-op.
void __fastcall sequence_helper_stub(void*, void*, int) {}

bool install_sequence_proxy() {
    // Six `mov eax,K ; xor eax,K2 ; call eax` sites decode to the captured GetSequenceActivity 2DAC41A0.
    constexpr struct { uint32_t rva, key, mask; } activity_calls[] = {
        {0x157ED5, 0x36E2C110, 0x1B4E80B0}, {0x157F2D, 0x62539AEB, 0x4FFFDB4B}, {0x157FB0, 0xB2BB727B, 0x9F1733DB},
        {0x157FEA, 0xBC28E101, 0x9184A0A1}, {0x158058, 0x56D937AB, 0x7B75760B}, {0x158077, 0xA8A6E84E, 0x850AA9EE}};
    for (const auto& call : activity_calls)
        if (at<unsigned char>(call.rva) != 0xB8 || at<uint32_t>(call.rva + 1) != call.key || (call.key ^ call.mask) != 0x2DAC41A0u) {
            say("    sequence proxy activity call at %X does not match", call.rva);
            return false;
        }
    if (!bytes_are(0x1580C3, "\xB8\xA1\xA5\x45\x08", 5)) { say("    sequence proxy helper cache does not match"); return false; }
    // C_BaseAnimating::GetSequenceActivity(int): m_pStudioHdr seqdesc activity, -1 for sequence -1.
    const uintptr_t activity = find("client.dll", "\x55\x8B\xEC\x53\x8B\x5D\x08\x56\x8B\xF1\x83", 11);
    sequence_prop = top_prop("DT_BaseViewModel", "m_nSequence");
    if (!activity || !sequence_prop || !sequence_prop->proxy) {
        say("    sequence proxy setup failed: activity=%08X prop=%p", static_cast<unsigned>(activity), sequence_prop);
        return false;
    }
    for (const auto& call : activity_calls) {
        const uint32_t key = static_cast<uint32_t>(activity) ^ call.mask;
        write_code(call.rva + 1, &key, 4);
    }
    at<uintptr_t>(0x7FB9E0) = reinterpret_cast<uintptr_t>(&sequence_helper_stub);
    at<unsigned>(0x7FB9E4) |= 1;
    sequence_original = reinterpret_cast<RecvProxy>(sequence_prop->proxy);
    at<uintptr_t>(0x7FB95C) = reinterpret_cast<uintptr_t>(sequence_prop);
    at<uintptr_t>(0x7FB960) = reinterpret_cast<uintptr_t>(sequence_original);
    sequence_prop->proxy = reinterpret_cast<void*>(&sequence_proxy);
    say("    m_nSequence proxy %p -> original remap 157B60 (GetSequenceActivity %08X)", sequence_original, static_cast<unsigned>(activity));
    return true;
}

// 3751F0 resolves "#token" through a localization cache that only the protected startup fills;
// on a miss it returns an empty wstring, so the paint kit list (FBE50) gets blank names.
using Localize = void*(__cdecl*)(void*, const char*, size_t);
Localize localize_original = nullptr;
void* game_localize = nullptr;

void* __cdecl localize_hook(void* out, const char* token, size_t length) {
    void* result = localize_original(out, token, length);
    if (*reinterpret_cast<uint32_t*>(static_cast<char*>(out) + 16) == 0 && token && length && length < 256) {
        char name[256];
        std::memcpy(name, token, length);
        name[length] = 0;
        using Find = const wchar_t*(__thiscall*)(void*, const char*);
        if (const wchar_t* text = (*reinterpret_cast<Find**>(game_localize))[11](game_localize, name))
            reinterpret_cast<void(__thiscall*)(void*, const wchar_t*, size_t)>(base + 0x0AEC80)(out, text, std::wcslen(text));
    }
    return result;
}

bool install_localize_fallback() {
    game_localize = factory("localize.dll", "Localize_001");
    if (!game_localize) return false;
    // push ebp; mov ebp,esp; sub esp,10h; push esi; mov esi,[ebp+0Ch]; cmp byte [esi],'#'
    if (!bytes_are(0x3751F0, "\x55\x8B\xEC\x83\xEC\x10\x56\x8B\x75\x0C\x80\x3E\x23", 13)) { say("    localization lookup does not match"); return false; }
    void* target = reinterpret_cast<void*>(base + 0x3751F0);
    auto status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) return false;
    if (MH_CreateHook(target, reinterpret_cast<void*>(&localize_hook), reinterpret_cast<void**>(&localize_original)) != MH_OK) return false;
    if (MH_EnableHook(target) != MH_OK) { MH_RemoveHook(target); return false; }
    say("    localization fallback -> Localize_001::Find");
    return true;
}

// Item fallback-attribute update (client, this-only): inventory item by 64-bit item id, else
// m_nFallbackPaintKit/Seed/Wear/StatTrak -> attributes -> UpdateGeneratedMaterial. Original detour
// 1508E0 forces item id -1/-1 for owned weapons around it; its original pointer lives at 7FB70C.
volatile LONG paint_kit_disabled = 0;
void __fastcall paint_kit_guard(void* item, void* edx) {
    using Fastcall = void(__fastcall*)(void*, void*);
    if (!paint_kit_disabled) {
        __try {
            reinterpret_cast<Fastcall>(base + 0x1508E0)(item, edx);
            return;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            InterlockedExchange(&paint_kit_disabled, 1);
            say("[!] Original paint kit applier faulted (0x%08lX); disabled.", GetExceptionCode());
        }
    }
    at<Fastcall>(0x7FB70C)(item, edx);
}

bool install_paint_kit_hook() {
    if (!bytes_are(0x1508E0, "\x55\x8B\xEC\x81\xEC\x84\x01\x00\x00\x53\x56\x8B", 12)) { say("    paint kit applier does not match"); return false; }
    const uintptr_t target = find("client.dll",
        "\x55\x8B\xEC\x83\xE4\xF8\x83\xEC\x0C\x53\x56\x8B\xF1\x8B\x86????\x8D\x8E????\x57\x56\xFF\x50\x10\x83\xBE????\x01", 37);
    if (!target) { say("    item fallback update pattern not found"); return false; }
    auto status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) return false;
    if (MH_CreateHook(reinterpret_cast<void*>(target), reinterpret_cast<void*>(&paint_kit_guard), reinterpret_cast<void**>(base + 0x7FB70C)) != MH_OK) return false;
    if (MH_EnableHook(reinterpret_cast<void*>(target)) != MH_OK) { MH_RemoveHook(reinterpret_cast<void*>(target)); return false; }
    say("    item fallback update %08X -> original paint kit applier 1508E0", static_cast<unsigned>(target));
    return true;
}

// Original LockCursor detour 154730: while the menu reports input focus (80869B, written by the
// menu frame) it tail-calls surface UnlockCursor (slot 66), else the original via 7FB878.
// Chams. Original detours: SceneEnd 154690 (calls the original through 7FB854, then the SceneEnd
// chams pass 134B50) and DrawModelExecute 152EF0 (original through 7DC950). Both run only once the
// menu is ready (material lists exist); a fault disables that hook's payload side.
volatile LONG scene_end_failed = 0, draw_model_failed = 0;
using SceneEnd = void(__fastcall*)(void*, void*);
using DrawModel = void(__fastcall*)(void*, void*, void*, void*, void*, void*);

int render_fault(EXCEPTION_POINTERS* e, volatile LONG* failed, const char* what) {
    InterlockedExchange(failed, 1);
    const auto address = reinterpret_cast<uintptr_t>(e->ExceptionRecord->ExceptionAddress);
    say("[!] %s faulted %08lX at %08X; chams path disabled", what, e->ExceptionRecord->ExceptionCode,
        static_cast<unsigned>(address >= base && address < base + cfg::BINARY_SIZE ? address - base : address));
    return EXCEPTION_EXECUTE_HANDLER;
}

void __fastcall scene_end_hook(void* self, void* edx) {
    if (primordial_menu_state == menu_ui::ready && !scene_end_failed) {
        __try { reinterpret_cast<SceneEnd>(base + 0x154690)(self, edx); return; }
        __except (render_fault(GetExceptionInformation(), &scene_end_failed, "SceneEnd")) { return; }
    }
    reinterpret_cast<SceneEnd>(at<uintptr_t>(0x7FB854))(self, edx);
}

void __fastcall draw_model_hook(void* self, void* edx, void* context, void* state, void* info, void* bones) {
    if (primordial_menu_state == menu_ui::ready && !draw_model_failed) {
        __try { reinterpret_cast<DrawModel>(base + 0x152EF0)(self, edx, context, state, info, bones); return; }
        __except (render_fault(GetExceptionInformation(), &draw_model_failed, "DrawModelExecute")) { return; }
    }
    reinterpret_cast<DrawModel>(at<uintptr_t>(0x7DC950))(self, edx, context, state, info, bones);
}

// C_BaseEntity::InterpolateServerEntities (void): original detour 377A00 zeroes cl_extrapolate, runs the
// original through 825F9C, then rebuilds the local player's animation/bone copy that local chams draw with.
volatile LONG interpolate_failed = 0;
void __cdecl interpolate_hook() {
    if (primordial_menu_state == menu_ui::ready && !interpolate_failed) {
        __try { reinterpret_cast<void(__cdecl*)()>(base + 0x377A00)(); return; }
        __except (render_fault(GetExceptionInformation(), &interpolate_failed, "InterpolateServerEntities")) { return; }
    }
    reinterpret_cast<void(__cdecl*)()>(at<uintptr_t>(0x825F9C))();
}

bool install_render_hooks() {
    if (!bytes_are(0x154690, "\xFF\x15\x54\xB8\xB3\x44\xE9", 7) || !bytes_are(0x152EF0, "\x55\x8B\xEC\x51\x53\x8B\x5D\x10", 8)) {
        say("    chams detours do not match");
        return false;
    }
    // SceneEnd pass ends with an unidentified call (captured 2DC6C5F0) on interface slot 4DE200, which is
    // never bound; skip it: mov eax,K / nop / xor eax,K2 / mov ecx,[slot] / call eax.
    if (!bytes_are(0x13501D, "\xB8", 1) || !bytes_are(0x135022, "\x90\x35", 2) || !bytes_are(0x13502E, "\xFF\xD0", 2)) { say("    SceneEnd tail call does not match"); return false; }
    static const unsigned char nops[19] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
                                           0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    write_code(0x13501D, nops, sizeof(nops));
    void* scene_end = (*reinterpret_cast<void***>(at<uintptr_t>(0x4DE1AC)))[9];
    void* draw_model = (*reinterpret_cast<void***>(at<uintptr_t>(0x4DE1A0)))[21];
    auto status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) return false;
    if (MH_CreateHook(scene_end, reinterpret_cast<void*>(&scene_end_hook), reinterpret_cast<void**>(base + 0x7FB854)) != MH_OK ||
        MH_CreateHook(draw_model, reinterpret_cast<void*>(&draw_model_hook), reinterpret_cast<void**>(base + 0x7DC950)) != MH_OK)
        return false;
    if (MH_EnableHook(scene_end) != MH_OK || MH_EnableHook(draw_model) != MH_OK) return false;
    void* interpolate = reinterpret_cast<void*>(find("client.dll", "\x55\x8B\xEC\x83\xEC\x1C\x8B\x0D????\x53\x56\x57\x8B\xB1\x0C\x10\x00\x00", 21));
    if (!interpolate || !bytes_are(0x377A00, "\x55\x8B\xEC\x64\xA1\x00\x00\x00\x00", 9)) { say("    InterpolateServerEntities not found; local chams stay off"); return true; }
    if (MH_CreateHook(interpolate, reinterpret_cast<void*>(&interpolate_hook), reinterpret_cast<void**>(base + 0x825F9C)) != MH_OK ||
        MH_EnableHook(interpolate) != MH_OK)
        return false;
    say("    InterpolateServerEntities %p -> 377A00", interpolate);
    say("    SceneEnd %p -> 154690, DrawModelExecute %p -> 152EF0", scene_end, draw_model);
    return true;
}

bool install_cursor_hook() {
    void* surface = factory("vguimatsurface.dll", "VGUI_Surface031");
    if (!surface || !bind(0x4DE15C, reinterpret_cast<uintptr_t>(surface), "VGUI_Surface031")) return false;
    auto** vtable = *reinterpret_cast<unsigned char***>(surface);
    // cmp byte [..],0 ; mov edx,[ecx+2A4h] ; mov ecx,[..] ; mov byte [cursor_locked], 0/1
    auto is_cursor_setter = [](const unsigned char* fn, unsigned char locked) {
        return fn[0] == 0x80 && fn[1] == 0x3D && fn[7] == 0x8B && fn[8] == 0x91 && fn[19] == 0xC6 && fn[20] == 0x05 && fn[25] == locked;
    };
    if (!is_cursor_setter(vtable[66], 0) || !is_cursor_setter(vtable[67], 1)) { say("    surface Unlock/LockCursor slots do not match"); return false; }
    if (!bytes_are(0x154730, "\x56\x57\x8B\xF2\x8B\xF9\xE8", 7)) { say("    LockCursor detour does not match"); return false; }
    auto status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) return false;
    if (MH_CreateHook(vtable[67], reinterpret_cast<void*>(base + 0x154730), reinterpret_cast<void**>(base + 0x7FB878)) != MH_OK) return false;
    if (MH_EnableHook(vtable[67]) != MH_OK) { MH_RemoveHook(vtable[67]); return false; }
    say("    LockCursor %p -> original detour 154730", vtable[67]);
    return true;
}

// Original IsButtonDown detour 1529A0: while the menu is open it reports buttons up unless the
// payload's own poller (flag 80869A) is querying; original via 7FB794.
// The original poller 3582C0 (keybind dispatch, button state machine) raises 80869A while it reads
// IsButtonDown. The menu window procedure withholds mouse/keyboard messages from the game while the
// menu is open, so the input system has no state then; answer the poller from the real key state.
unsigned button_vk[114]{};
using IsButtonDown = bool(__fastcall*)(void*, void*, int);
bool __fastcall is_button_down_hook(void* self, void* edx, int code) {
    if (at<unsigned char>(0x80869A)) {
        if (code < 0 || code >= 114 || !button_vk[code]) return false;
        DWORD process = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &process);
        return process == GetCurrentProcessId() && (GetAsyncKeyState(static_cast<int>(button_vk[code])) & 0x8000) != 0;
    }
    return reinterpret_cast<IsButtonDown>(base + 0x1529A0)(self, edx, code);
}

bool install_input_hook() {
    void* input = factory("inputsystem.dll", "InputSystemVersion001");
    if (!input) return false;
    auto** vtable = *reinterpret_cast<unsigned char***>(input);
    // ButtonCodeToVirtualKey (slot 46) for keys 0..106; mouse buttons 107..111; wheel 112/113 have no key state.
    using ToVirtualKey = unsigned(__thiscall*)(void*, int);
    for (int code = 0; code < 107; ++code) button_vk[code] = reinterpret_cast<ToVirtualKey>(vtable[46])(input, code);
    constexpr unsigned mouse[] = {VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2};
    for (int i = 0; i < 5; ++i) button_vk[107 + i] = mouse[i];
    if (button_vk[72] != VK_INSERT) { say("    ButtonCodeToVirtualKey mismatch (%u)", button_vk[72]); return false; }
    // push ebp; mov ebp,esp; mov edx,[ebp+8]; mov eax,1; push esi; mov esi,ecx; mov ecx,edx; and ecx,1Fh; sar edx,5
    constexpr unsigned char is_button_down[] = {0x55,0x8B,0xEC,0x8B,0x55,0x08,0xB8,0x01,0x00,0x00,0x00,0x56,0x8B,0xF1,0x8B,0xCA,0x83,0xE1,0x1F,0xC1,0xFA,0x05};
    void* target = nullptr;
    for (int i = 0; i < 40; ++i)
        if (!std::memcmp(vtable[i], is_button_down, sizeof(is_button_down))) { target = vtable[i]; break; }
    if (!target) { say("    IsButtonDown not found in InputSystemVersion001"); return false; }
    if (!bytes_are(0x1529A0, "\x55\x8B\xEC\x56\x57\x8B\xF2\x8B\xF9\xE8", 10)) { say("    IsButtonDown detour does not match"); return false; }
    auto status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) return false;
    if (MH_CreateHook(target, reinterpret_cast<void*>(&is_button_down_hook), reinterpret_cast<void**>(base + 0x7FB794)) != MH_OK) return false;
    if (MH_EnableHook(target) != MH_OK) { MH_RemoveHook(target); return false; }
    say("    IsButtonDown %p -> original detour 1529A0 (key state while polling)", target);
    return true;
}

// The original injector pointed each netvar operand cell at the offset immediate inside the
// captured client.dll (RecvProp static initialisers, or field uses in client code). These cells
// were matched to the same initialiser in the installed client by name and position; the value
// is re-checked at runtime and the cell is left untouched if it differs.
struct InitCell { uint32_t stale, client_rva, value; };
constexpr InitCell init_cells[] = {
    {0x2D8F6120, 0x006120, 0xA20},  {0x2D8F619F, 0x00619F, 0xF0},   {0x2D8F629D, 0x00629D, 0x2A00},
    {0x2D8F6481, 0x006481, 0x3248}, {0x2D8F6500, 0x006500, 0x324C}, {0x2D8F6D3D, 0x006D3D, 0x3254},
    {0x2D8F6DBC, 0x006DBC, 0x3258}, {0x2D8F6EBA, 0x006EBA, 0x3240}, {0x2D8F6F39, 0x006F39, 0x3274},
    {0x2D8F7037, 0x007037, 0x327C}, {0x2D8F7135, 0x007135, 0x3264}, {0x2D8F768D, 0x00764D, 0x29B0},
    {0x2D8F778B, 0x00774B, 0x104},  {0x2D8F8040, 0x007F80, 0x29D8}, {0x2D8F81BD, 0x0080FD, 0x28C0},
    {0x2D8F83B9, 0x0082F9, 0x29D4}, {0x2D8F8438, 0x008378, 0x29DC}, {0x2D8F8634, 0x008574, 0x29A0},
    {0x2D8F9FF0, 0x009E30, 0x9D8},  {0x2D8FA06F, 0x009EAF, 0x9DC},  {0x2D8FAA71, 0x00A8B1, 0x2D80},
    {0x2D8FBEC0, 0x00BCC0, 0x108},  {0x2D8FC13B, 0x00BF3B, 0x3440}, {0x2D8FC7AE, 0x00C5AE, 0x33D4},
    {0x2D8FC9AA, 0x00C7AA, 0x31DC}, {0x2D8FCDDE, 0x00CBDE, 0x9FC},  {0x2D906360, 0x015F30, 0x138},
    {0x2D90655C, 0x01612C, 0x268},  {0x2D92EDAD, 0x03D79D, 0x29A4}, {0x2D92EE2C, 0x03D81C, 0x29B8},
    {0x2D92EEAB, 0x03D89B, 0x29BC}, {0x2D92EFA9, 0x03D999, 0x29C4}, {0x2D9473C0, 0x056420, 0x33E0},
    {0x2D94743F, 0x05649F, 0x33E1}, {0x2D9474BE, 0x05651E, 0x33E2}, {0x2D94753D, 0x05659D, 0x33E4},
    {0x2D9475BC, 0x05661C, 0x33E8}, {0x2D94763B, 0x05669B, 0x33EC}, {0x2D9492E0, 0x05835E, 0x33B8},
    {0x2D94935F, 0x0583DD, 0x3350}, {0x2D9493DE, 0x05845C, 0x3354}, {0x2D949659, 0x0586D7, 0x3361},
    {0x2D951EDF, 0x06116F, 0x31D0}, {0x2D951F5E, 0x0611EE, 0x31D4}, {0x2D951FDD, 0x06126D, 0x31D8},
    {0x2D9520DB, 0x06136B, 0x31E0}, {0x2D95215A, 0x0613EA, 0x31E4},
};
// Cells identified from how the payload uses them; resolved by name through the RecvTables.
struct NamedCell { uint32_t stale; const char* table; const char* prop; };
constexpr NamedCell named_cells[] = {
    {0x2D8F003C, "DT_CSPlayer", "m_hGroundEntity"},       {0x2D8F1304, "DT_CSPlayer", "m_nModelIndex"},
    {0x2D8F1D4F, "DT_CSPlayer", "m_vecVelocity[0]"},      {0x2D8F9170, "DT_CSPlayer", "m_lifeState"},
    {0x2D92D648, "DT_CSPlayer", "m_iTeamNum"},            {0x2D942183, "DT_CSPlayer", "m_hOwnerEntity"},
    {0x2D9B7EE7, "DT_CSPlayerResource", "m_bAlive"},      {0x2D9CB32C, "DT_CSPlayer", "m_nWaterLevel"},
    {0x2DA842AE, "DT_BaseAttributableItem", "m_iItemDefinitionIndex"},
    {0x2DA91016, "DT_CSPlayer", "m_hActiveWeapon"},       {0x2DA912B9, "DT_CSPlayer", "m_hMyWeapons"},
    {0x2DA91EA7, "DT_CSPlayer", "m_hMyWearables"},        {0x2DA9DC0C, "DT_CSPlayer", "m_iObserverMode"},
    {0x2DA9E8ED, "DT_CSPlayer", "m_bHasHeavyArmor"},      {0x2DAA0F95, "DT_BaseAttributableItem", "m_iEntityQuality"},
    {0x2DAA0FB0, "DT_CSPlayer", "m_flDuckAmount"},        {0x2DAA0FBC, "DT_CSPlayer", "m_flDuckSpeed"},
    {0x2DAF0360, "DT_CSPlayer", "m_hObserverTarget"},     {0x2DAF03AF, "DT_CSPlayer", "m_hViewModel[0]"},
    {0x2DB1B81B, "DT_CSPlayer", "m_ArmorValue"},          {0x2DB5D56F, "DT_CSPlayer", "m_nSurvivalTeam"},
    {0x2DBDB5D3, "DT_CSPlayer", "m_bIsScoped"},           {0x2DC99E53, "DT_Inferno", "m_fireCount"},
    {0x2DC9D617, "DT_BaseAttributableItem", "m_iItemIDHigh"},
    {0x2DCA482E, "DT_CSPlayer", "m_angEyeAngles[0]"},     {0x2DCA4E1F, "DT_CSPlayer", "m_bGunGameImmunity"},
    {0x2DCA5885, "DT_CSPlayer", "m_bHasHelmet"},          {0x2DD5310F, "DT_BaseAttributableItem", "m_iItemIDLow"},
    // Compared with 8/9/10 (noclip/ladder/observer); radar reveal writes 1; player-resource arrays.
    {0x2D942073, "DT_CSPlayer", "m_MoveType"},            {0x2DA8BB5F, "DT_CSPlayer", "m_bSpotted"},
    {0x2DB22972, "DT_CSPlayerResource", "m_iTeam"},       {0x2D908E0C, "DT_CSPlayerResource", "m_bConnected"},
    // Player record save/restore (0D0670/0D1C70) and setup-bones wrapper fields.
    {0x2D8F152D, "DT_CSPlayer", "m_angRotation"},         {0x2DA8D9E0, "DT_CSPlayer", "m_flPoseParameter"},
    {0x2DCA5E64, "DT_CSPlayer", "m_flLowerBodyYawTarget"}, {0x2DA86F46, "DT_CSPlayer", "m_vecMins"},
    {0x2D9F6F70, "DT_CSPlayer", "m_vecMaxs"},             {0x2DCA59EB, "DT_CSPlayer", "m_hRagdoll"},
    {0x2D92D7C5, "DT_CSRagdoll", "m_flAbsYaw"},           {0x2DCA5E46, "DT_CSPlayer", "m_bIsWalking"},
    {0x2DCBF6E9, "DT_CSPlayerResource", "m_nActiveCoinRank"}, {0x2DAB8192, "DT_CSPlayer", "m_flModelScale"},
    {0x2DC5439E, "DT_CSPlayer", "m_flStamina"},
    // Molotov fire (CInferno) arrays, smoke state and the player health fallback (grenade ESP / danger warning).
    {0x2D8FDCEF, "DT_Inferno", "m_fireXDelta"},           {0x2DCF9EEF, "DT_Inferno", "m_fireYDelta"},
    {0x2DA810AC, "DT_Inferno", "m_fireZDelta"},           {0x2DD12A93, "DT_Inferno", "m_bFireIsBurning"},
    {0x2D8F50A8, "DT_BaseCSGrenadeProjectile", "m_vecVelocity"},          {0x2D946E00, "DT_SmokeGrenadeProjectile", "m_bDidSmokeEffect"}, {0x2D8F0043, "DT_CSPlayer", "m_iHealth"},
    {0x2D9042CE, "DT_CSPlayer", "m_vecBaseVelocity"}, {0x2DC9A333, "DT_Inferno", "m_nFireEffectTickBegin"},
    {0x2D90AAE0, "DT_PlantedC4", "m_bBombTicking"},       {0x2D9A9D98, "DT_CSPlayer", "m_bIsPlayerGhost"},  // chams skip
    // ESP flags: ping ("{}ms"), money ("${}"), defuse kit ("DEF").
    {0x2D9029D9, "DT_CSPlayerResource", "m_iPing"},       {0x2DC2101D, "DT_CSPlayer", "m_iAccount"},
    {0x2DC9F1F7, "DT_CSPlayer", "m_bHasDefuser"},
    // Can-shoot checks (0F4290).
    {0x2DCA49CD, "DT_CSPlayer", "m_iPlayerState"},        {0x2DCA40A6, "DT_CSPlayer", "m_iShotsFired"},
    {0x2DCA496A, "DT_CSPlayer", "m_bWaitForNoAttack"},    {0x2DCA4D12, "DT_CSPlayer", "m_bIsDefusing"},
    {0x2D949E5F, "DT_WeaponAK47", "m_iBurstShotsRemaining"},
};
uint32_t named_values[sizeof(named_cells) / sizeof(named_cells[0])]{};
uint32_t eflags_offset = 0;  // m_iEFlags (not networked); cell 2D9FA51A, cleared of EFL_DIRTY_ABSVELOCITY

// Stale captured-client functions reached through `mov r32,K1 ... xor r32,K2` pairs.
struct StaleFunction { uint32_t stale; const char* pattern; size_t length; const char* name; };
constexpr StaleFunction stale_functions[] = {
    {0x2DAD5270, "\x55\x8B\xEC\x83\xE4\xF8\x51\x53\x56\x57\x8B\xF1\xE8????\x8B\x7D\x08\xF3\x0F\x10\x07\x0F\x2E\x86", 26, "SetAbsOrigin"},
    {0x2DAD5430, "\x55\x8B\xEC\x83\xE4\xF8\x83\xEC\x64\x53\x56\x57\x8B\xF1\xE8", 15, "SetAbsAngles"},
    {0x2DA83BD0, "\x56\x8B\xF1\x83\xBE\x50\x29\x00\x00\x00\x75\x14\x8B\x46\x04\x8D\x4E\x04\xFF\x50\x20\x85\xC0\x74\x07\x8B\xCE\xE8????\x8B\x86\x50\x29\x00\x00\x85\xC0\x74\x05\x83\x38\x00\x75\x02\x33\xC0\x5E\xC3", 51, "GetModelPtr"},
    {0x2DAD3870, "\x55\x8B\xEC\x83\xEC?\x53\x56\x8B\xF1\x57\x80\xBE?????\x75?\x8B\x06", 22, "OnLatchInterpolatedVariables"},
    {0x2DD06CA0, "\x56\x6A\x01\x68????\x8B\xF1", 10, "CCSGOPlayerAnimState::Reset"},
    {0x2DA9C230, "\x55\x8B\xEC\x83\xE4\xF0\x83\xEC\x7C\x56\x52\x51\x8D\x4C\x24\x38", 16, "UTIL_TraceLine"},
    {0x2DCB8890, "\x56\x8B\xF1\x85\xF6\x74\x31", 7, "C_CSPlayer::HasC4"},
    {0x2DABCC20, "\x55\x8B\xEC\x83\xEC\x48\x53\x8B\x5D\x08\x89\x4D\xF4", 13, "C_BaseAnimating::AttachmentHelper"},
    {0x2DBF7650, "\x55\x8B\xEC\x51\x56\x8B\xF1\x85\xF6\x74\x68", 11, "IsBreakableEntity"},
    {0x2DAC41A0, "\x55\x8B\xEC\x53\x8B\x5D\x08\x56\x8B\xF1\x83", 11, "GetSequenceActivity"},
    {0x2DD07130, "\x55\x8B\xEC\x83\xE4\xF8\x83\xEC\x18\x56\x57\x8B\xF9\xF3\x0F\x11\x54\x24\x0C", 19, "CCSGOPlayerAnimState::Update"},
};

// Every `mov r32,K1 ... xor r32,K2` site (IDA, instruction level) decoding to a captured client
// (2D8F0000+) or engine (15910000+) address: {mov rva, xor imm32 rva, decoded}. Sites still stale
// after re-keying are pointed at 0 so a call faults inside its guarded routine (logged, disabled)
// instead of running whatever now sits at the old address.
struct StaleSite { uint32_t mov, xor_imm, stale; };
constexpr StaleSite stale_sites[] = {
    {0x0A1FD1, 0x0A1FD8, 0x2DFD2BA0}, {0x0A2A1D, 0x0A2A24, 0x15940730}, {0x0A2A3A, 0x0A2A41, 0x15F65648}, {0x0A2A63, 0x0A2A6A, 0x1594A7F0},
    {0x0B21A0, 0x0B21A7, 0x2DC4D990}, {0x0B27B3, 0x0B27BA, 0x2DC4D990}, {0x0C0D67, 0x0C0D75, 0x15A10CB0}, {0x0CCB0C, 0x0CCB1E, 0x15A10CB0},
    {0x0CCCCC, 0x0CCCD9, 0x15A10CB0}, {0x0CCD49, 0x0CCD5D, 0x15A10CB0}, {0x0CD02B, 0x0CD037, 0x15A10CB0}, {0x0CD078, 0x0CD07F, 0x15A10CB0},
    {0x0CD0B0, 0x0CD0BC, 0x15A10CB0}, {0x0CD391, 0x0CD39D, 0x15A10CB0}, {0x0CD3DE, 0x0CD3E5, 0x15A10CB0}, {0x0CD416, 0x0CD422, 0x15A10CB0},
    {0x0D1CC2, 0x0D1CC9, 0x2DAD5270}, {0x0D1D58, 0x0D1D5F, 0x2DAD5430}, {0x0D3EF2, 0x0D3EF9, 0x2DAD5270}, {0x0D4AD5, 0x0D4ADC, 0x2DA83BD0},
    {0x0D54EA, 0x0D54F3, 0x2DA83BD0}, {0x0D5859, 0x0D5860, 0x2DA83BD0}, {0x0D6500, 0x0D6507, 0x2DAD5270}, {0x0EC8C6, 0x0EC8CF, 0x2DA83BD0},
    {0x0EDE9E, 0x0EDEA5, 0x2DAD5270}, {0x0EDF97, 0x0EDF9E, 0x2DD07130}, {0x0EF3D7, 0x0EF3DE, 0x2DAD5270}, {0x0EF4D0, 0x0EF4D7, 0x2DD07130},
    {0x0F0CC6, 0x0F0CCD, 0x2DD06CA0}, {0x0F12C0, 0x0F12CD, 0x15B11260}, {0x0F15D8, 0x0F15E5, 0x15B11260}, {0x0F1D86, 0x0F1D8D, 0x2DAD3870},
    {0x0F32E8, 0x0F32EF, 0x2DA9C230}, {0x0F4B64, 0x0F4B6B, 0x2DAD5820}, {0x0F4C6A, 0x0F4C71, 0x2DC194B0}, {0x0F4D23, 0x0F4D2A, 0x159EE210},
    {0x0F4FEB, 0x0F4FF2, 0x2DC19E80}, {0x0F5016, 0x0F501D, 0x2DC19E80}, {0x0F54B2, 0x0F54B9, 0x2DC19E80}, {0x0F5DC1, 0x0F5DC8, 0x15B29C00},
    {0x0F678F, 0x0F6796, 0x1599A320}, {0x1009ED, 0x1009F7, 0x2DBB50F0}, {0x100E93, 0x100E9C, 0x2DAD5270}, {0x101048, 0x10104F, 0x2DFE33E0},
    {0x10A40D, 0x10A416, 0x2DA83BD0}, {0x10B464, 0x10B46D, 0x2DCFAF10}, {0x112847, 0x11284E, 0x15A10CB0}, {0x115477, 0x11547E, 0x2E09B1E0},
    {0x117B09, 0x117B10, 0x2DA83BD0}, {0x117C58, 0x117C64, 0x2DA83BD0}, {0x119503, 0x11950A, 0x2DA83BD0}, {0x11B71C, 0x11B723, 0x2DAA29F0},
    {0x11B8E5, 0x11B8EC, 0x2DAA29F0}, {0x1290D3, 0x1290DD, 0x159E9360}, {0x1293C1, 0x1293C8, 0x2DC4AA90}, {0x13501D, 0x135024, 0x2DC6C5F0},
    {0x13831C, 0x138323, 0x2DA9C230}, {0x1385B2, 0x1385B9, 0x2DA83BD0}, {0x139217, 0x13921E, 0x2DCB8890}, {0x139D61, 0x139D68, 0x2DCB8890},
    {0x13AE40, 0x13AE47, 0x2DA83BD0}, {0x13E909, 0x13E914, 0x2DA9C230}, {0x13F2D9, 0x13F2E0, 0x2DA9C230}, {0x13FC3A, 0x13FC41, 0x2DA9C230},
    {0x13FCD0, 0x13FCD7, 0x2DA9C230}, {0x14DB29, 0x14DB32, 0x15A41910}, {0x1502E1, 0x1502E8, 0x2DC39660}, {0x1502FD, 0x150304, 0x2DC39660},
    {0x15306C, 0x153075, 0x2DA83BD0}, {0x1530A3, 0x1530AC, 0x2DA83BD0}, {0x153B28, 0x153B2F, 0x2DA83BD0}, {0x153D35, 0x153D3C, 0x2E09B900},
    {0x15432A, 0x154331, 0x2DF6FF80}, {0x1544DB, 0x1544E2, 0x2DAD5430}, {0x1563F8, 0x156401, 0x2DAD5270}, {0x157ED5, 0x157EDF, 0x2DAC41A0},
    {0x157F2D, 0x157F34, 0x2DAC41A0}, {0x157FB0, 0x157FB9, 0x2DAC41A0}, {0x157FEA, 0x157FF1, 0x2DAC41A0}, {0x158058, 0x15805F, 0x2DAC41A0},
    {0x158077, 0x15807E, 0x2DAC41A0}, {0x1580C3, 0x1580CA, 0x2DAA3240}, {0x2BAE67, 0x2BAE6E, 0x2DAA29F0}, {0x2BB927, 0x2BB92E, 0x2DA9C230},
    {0x35638C, 0x356395, 0x15BFA8C0}, {0x357F4A, 0x357F51, 0x15B40F00}, {0x362DA1, 0x362DA8, 0x2DABCC20}, {0x365213, 0x36521A, 0x2DBF7650},
    {0x367D33, 0x367D43, 0x2DB71CA0}, {0x367DEF, 0x367DF6, 0x2DB71CA0}, {0x36841D, 0x368424, 0x2DA9C230}, {0x368466, 0x36846D, 0x2DA9C230},
    {0x368AE4, 0x368AEB, 0x2DA83BD0}, {0x369413, 0x36941A, 0x2DA83BD0}, {0x369623, 0x36962A, 0x2DA83BD0}, {0x3696D3, 0x3696DA, 0x2DA83BD0},
    {0x369786, 0x369790, 0x2DA83BD0}, {0x36A661, 0x36A668, 0x2E09C3A0}, {0x36A671, 0x36A678, 0x2DA83BD0}, {0x36A683, 0x36A68A, 0x2DA83BD0},
    {0x36A707, 0x36A70E, 0x2DA892D0}, {0x36A72D, 0x36A734, 0x2DA83BD0}, {0x36A8C2, 0x36A8C9, 0x2DABAA30}, {0x36B12F, 0x36B136, 0x2DA9C230},
    {0x36B23C, 0x36B243, 0x2DA9C230}, {0x36B3D9, 0x36B3E0, 0x2DAC41A0}, {0x36B460, 0x36B46C, 0x2DAC41A0}, {0x36B5A2, 0x36B5A9, 0x2DC1C200},
    {0x36BAC3, 0x36BACA, 0x2DA83BD0}, {0x36BAEC, 0x36BAF5, 0x2DC41A00}, {0x36BB04, 0x36BB11, 0x2DA88E10}, {0x36BB86, 0x36BB8D, 0x2DA9A3F0},
    {0x36BB98, 0x36BB9F, 0x2DCFAE60}, {0x36BC05, 0x36BC0C, 0x2DA9A3F0}, {0x36BC33, 0x36BC3A, 0x2DA9A3F0}, {0x36BC4C, 0x36BC53, 0x2DD0B2A0},
    {0x36BE73, 0x36BE7F, 0x2E028840}, {0x36BECE, 0x36BEDA, 0x2E028840}, {0x36BF1E, 0x36BF2A, 0x2E028840}, {0x36E256, 0x36E25D, 0x2DBE2740},
    {0x36E6B2, 0x36E6B9, 0x2DD06CA0}, {0x36E7EC, 0x36E7FF, 0x2DD07130}, {0x36EA0E, 0x36EA15, 0x2DAD5430}, {0x36EB65, 0x36EB6C, 0x2DD0B2A0},
    {0x36EDAE, 0x36EDB5, 0x2DAD5430}, {0x36F038, 0x36F03F, 0x2DC19B00}, {0x36F45F, 0x36F466, 0x2DBE2740}, {0x36F66C, 0x36F673, 0x2DBE2740},
    {0x36F73D, 0x36F749, 0x2DD07130}, {0x36F99A, 0x36F9A1, 0x2DAD5430}, {0x36FB16, 0x36FB1D, 0x2DAD5430}, {0x36FBED, 0x36FBF9, 0x2DD07130},
    {0x3700D4, 0x3700DB, 0x2DAD5430}, {0x370555, 0x37055E, 0x2DA83BD0}, {0x37093C, 0x370949, 0x2DD07130}, {0x370C0D, 0x370C14, 0x2DAD5430},
    {0x371111, 0x371118, 0x2DA9A3F0}, {0x3715B6, 0x3715C8, 0x2DAD5430}, {0x371944, 0x371950, 0x2DAD5270}, {0x371BE5, 0x371C05, 0x2DD07130},
    {0x371EAC, 0x371EB3, 0x2DAD5430}, {0x372068, 0x37206F, 0x2DAD5430}, {0x3720EE, 0x3720F8, 0x2DAD5430}, {0x372A7A, 0x372A81, 0x2DAD5270},
    {0x372B95, 0x372B9C, 0x2DAD5270}, {0x372C91, 0x372C9A, 0x2DA83BD0}, {0x372D68, 0x372D6F, 0x2DAD5270}, {0x372D97, 0x372D9E, 0x2DAD5430},
    {0x372F97, 0x372F9E, 0x2DAD5270}, {0x372FB5, 0x372FBC, 0x2DAD5430}, {0x37492F, 0x374936, 0x15B2A290}, {0x375D8A, 0x375D93, 0x2DC1BB40},
    {0x376170, 0x376177, 0x2DC4D990}, {0x37620B, 0x376214, 0x2DC4D990}, {0x3774FC, 0x377503, 0x2DA9C230}, {0x37755B, 0x377562, 0x2DA9C230},
    {0x378667, 0x37866E, 0x2DBB50F0}, {0x37867F, 0x378686, 0x2DDDF8F0}, {0x3799CE, 0x3799D7, 0x2DBE2740},
};

constexpr size_t stale_count = sizeof(stale_functions) / sizeof(stale_functions[0]) + 3;

// C_BaseAnimating::LookupAttachment(name): the payload calls it on the entity; the final client's
// copies take the IClientRenderable (entity + 4) instead. __fastcall + stack arg = thiscall ABI.
uintptr_t lookup_attachment = 0;
int __fastcall lookup_attachment_adapter(char* entity, void*, const char* name) {
    return reinterpret_cast<int(__thiscall*)(void*, const char*)>(lookup_attachment)(entity + 4, name);
}

unsigned rekey_stale_functions() {
    uint32_t stale[stale_count]{};
    uintptr_t current[stale_count]{};
    for (size_t k = 0; k + 3 < stale_count; ++k) {
        stale[k] = stale_functions[k].stale;
        current[k] = find("client.dll", stale_functions[k].pattern, stale_functions[k].length);
        if (!current[k]) say("    %s pattern not found; its calls stay stale", stale_functions[k].name);
    }
    // Captured engine: IEngineTrace slot 26 (occlusion test: entindex, camera box, target box, out),
    // called non-virtually on EngineTraceClient004 by the player cache's visibility flag.
    stale[stale_count - 2] = 0x15B11260;
    current[stale_count - 2] = (*reinterpret_cast<uintptr_t**>(at<uintptr_t>(0x4DE180)))[26];
    lookup_attachment = find("client.dll", "\x55\x8B\xEC\x83\xB9\x4C\x29\x00\x00\x00\x56\x8D\x71\xFC", 14);
    if (!lookup_attachment) say("    LookupAttachment pattern not found; its calls stay stale");
    // CTraceFilterSimple::ShouldHitEntity (payload filters tail-jump to it): slot 0 of the
    // filter vtable UTIL_TraceLine stores with `mov [esp+2C], imm32` at +0x39.
    const uintptr_t trace_line = find("client.dll", "\x55\x8B\xEC\x83\xE4\xF0\x83\xEC\x7C\x56\x52\x51\x8D\x4C\x24\x38", 16);
    stale[stale_count - 3] = 0x2DC4D990;
    current[stale_count - 3] = trace_line && *reinterpret_cast<const uint32_t*>(trace_line + 0x39) == 0x2C2444C7
        ? **reinterpret_cast<uintptr_t* const*>(trace_line + 0x3D) : 0;
    if (!current[stale_count - 3]) say("    CTraceFilterSimple vtable not found; its calls stay stale");
    stale[stale_count - 1] = 0x2DABAA30;
    current[stale_count - 1] = lookup_attachment ? reinterpret_cast<uintptr_t>(&lookup_attachment_adapter) : 0;
    auto* image = reinterpret_cast<unsigned char*>(base);
    unsigned rekeyed = 0;
    for (uint32_t i = 0; i + 70 <= 0x4DDBE0; ++i) {
        if (image[i] < 0xB8 || image[i] > 0xBF) continue;
        const unsigned reg = image[i] - 0xB8u;
        uint32_t k1;
        std::memcpy(&k1, image + i + 1, 4);
        for (uint32_t j = i + 5; j < i + 64; ++j) {
            uint32_t k2 = 0, at_k2 = 0;
            if (reg == 0 && image[j] == 0x35) at_k2 = j + 1;
            else if (image[j] == 0x81 && image[j + 1] == 0xF0 + reg) at_k2 = j + 2;
            if (!at_k2) continue;
            std::memcpy(&k2, image + at_k2, 4);
            for (size_t k = 0; k < stale_count; ++k)
                if (current[k] && (k1 ^ k2) == stale[k]) {
                    const uint32_t key = static_cast<uint32_t>(current[k]) ^ k2;
                    write_code(i + 1, &key, 4);
                    ++rekeyed;
                }
            break;
        }
    }
    unsigned trapped = 0;
    for (const auto& site : stale_sites) {
        uint32_t k1, k2;
        std::memcpy(&k1, image + site.mov + 1, 4);
        std::memcpy(&k2, image + site.xor_imm, 4);
        if ((k1 ^ k2) != site.stale) continue;
        write_code(site.mov + 1, &k2, 4);  // decodes to 0
        ++trapped;
    }
    say("    trapped %u unidentified stale addresses", trapped);
    return rekeyed;
}

// Primordial's job pool (split helper 362AC0 divides work by the count at 809564); its protected
// init starts clamp(cores - 1, 1, 12) detached workers 362C20(index) and is never run here.
DWORD WINAPI job_worker(void* index) {
    reinterpret_cast<void(__cdecl*)(int)>(base + 0x362C20)(static_cast<int>(reinterpret_cast<intptr_t>(index)));
    return 0;
}

void start_job_workers() {
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    int count = static_cast<int>(info.dwNumberOfProcessors) - 1;
    count = count < 1 ? 1 : count > 12 ? 12 : count;
    at<int>(0x809564) = count;
    for (int i = 0; i < count; ++i)
        if (HANDLE thread = CreateThread(nullptr, 0, job_worker, reinterpret_cast<void*>(static_cast<intptr_t>(i)), 0, nullptr))
            CloseHandle(thread);
    say("    job workers: %d", count);
}

// Client class ids compared against ClientClass::m_ClassID; filled by a protected level-init path.
struct ClassId { uint32_t rva; const char* name; };
constexpr ClassId class_ids[] = {
    {0x57ED7C, "CCSPlayer"}, {0x7DC94C, "CBaseWeaponWorldModel"}, {0x7DC8D8, "CC4"}, {0x7DC8D4, "CPlantedC4"},
    // Grenade checks (3652B0 tests these four; 807C28/807C34 are only ever tested together).
    {0x7DC8DC, "CInferno"}, {0x580F68, "CPlantedC4"}, {0x807C20, "CSmokeGrenadeProjectile"}, {0x807CA4, "CBaseCSGrenadeProjectile"},
    {0x807C28, "CMolotovProjectile"}, {0x807C34, "CDecoyProjectile"},
    // Molotov warning: "trace hit the world" check (375FD0) before placing the icon at the landing point.
    {0x807CF4, "CWorld"}};

// Primordial's own netvar map (RVA 826198): FNV-1a("ClientClass.m_prop") -> {RecvProp*, 1, type, offset},
// read with unordered_map::at (throws on a miss). Its RecvTable walker only runs from protected init,
// so the entries the payload looks up are filled here.
struct MapNetvar { const char* cls; const char* prop; };
constexpr MapNetvar map_netvars[] = {
    {"CCSGameRulesProxy", "m_bWarmupPeriod"}, {"CCSGameRulesProxy", "m_flRestartRoundTime"},
    {"CCSGameRulesProxy", "m_bFreezePeriod"}, {"CCSPlayerResource", "m_bombsiteCenterA"},
    {"CCSPlayerResource", "m_bombsiteCenterB"}, {"CCSPlayerResource", "m_bHasHelmet"},
    {"CCSPlayerResource", "m_bHasDefuser"}, {"CCSPlayerResource", "m_iArmor"}, {"CPlayerResource", "m_iHealth"}};

RecvProp* find_prop_ptr(const RecvTable* table, const char* name, int& offset) {
    for (int i = 0; i < table->count; ++i) {
        auto& prop = table->props[i];
        if (!std::strcmp(prop.name, name)) { offset += prop.offset; return &prop; }
        if (prop.table && prop.table->count) {
            int nested = offset + prop.offset;
            if (auto* found = find_prop_ptr(prop.table, name, nested)) { offset = nested; return found; }
        }
    }
    return nullptr;
}

void fill_netvar_map() {
    using AllClasses = ClientClass*(__thiscall*)(void*);
    using Insert = uintptr_t*(__thiscall*)(void*, uintptr_t*, const uint32_t*);
    unsigned filled = 0;
    for (const auto& entry : map_netvars) {
        char name[96];
        std::snprintf(name, sizeof(name), "%s.%s", entry.cls, entry.prop);
        uint32_t key = 0x811C9DC5u;
        for (const char* c = name; *c; ++c) key = (key ^ static_cast<unsigned char>(*c)) * 0x1000193u;
        for (auto* cls = (*reinterpret_cast<AllClasses**>(client))[8](client); cls; cls = cls->next) {
            if (!cls->name || !cls->table || std::strcmp(cls->name, entry.cls)) continue;
            int offset = 0;
            RecvProp* prop = find_prop_ptr(cls->table, entry.prop, offset);
            if (!prop) break;
            uintptr_t node = 0;
            reinterpret_cast<Insert>(base + 0x37AB10)(reinterpret_cast<void*>(base + 0x826198), &node, &key);
            auto* value = reinterpret_cast<uint32_t*>(node + 0xC);
            value[0] = reinterpret_cast<uint32_t>(prop);
            value[1] = 1;
            value[3] = static_cast<uint32_t>(offset);
            ++filled;
            break;
        }
    }
    say("    netvar map: %u/%u filled", filled, static_cast<unsigned>(sizeof(map_netvars) / sizeof(map_netvars[0])));
}

// m_ClassID is assigned from the server's class table on connect, so this runs from the FSN hook
// until CCSPlayer has a non-zero id.
void fill_class_ids() {
    using AllClasses = ClientClass*(__thiscall*)(void*);
    for (const auto& id : class_ids)
        for (auto* cls = (*reinterpret_cast<AllClasses**>(client))[8](client); cls; cls = cls->next)
            if (cls->name && !std::strcmp(cls->name, id.name)) { at<int>(id.rva) = cls->id; break; }
    say("    class ids: CCSPlayer=%d CBaseWeaponWorldModel=%d CC4=%d CPlantedC4=%d",
        at<int>(0x57ED7C), at<int>(0x7DC94C), at<int>(0x7DC8D8), at<int>(0x7DC8D4));
}

// Rewrites every disp32 operand (A1 moffs or modrm mod=00 rm=101) holding a stale cell.
unsigned patch_cells_everywhere(const uint32_t* stale, const uint32_t* target, size_t count) {
    auto* image = reinterpret_cast<unsigned char*>(base);
    unsigned patched = 0;
    for (uint32_t i = 1; i + 4 <= 0x4DDBE0; ++i) {
        uint32_t value;
        std::memcpy(&value, image + i, 4);
        if ((value >> 24) != 0x2D && (value >> 24) != 0x2E) continue;
        const unsigned char before = image[i - 1];
        if (before != 0xA1 && (before & 0xC7) != 0x05) continue;
        for (size_t k = 0; k < count; ++k)
            if (stale[k] == value) { write_code(i, &target[k], 4); ++patched; break; }
    }
    return patched;
}

bool repair_all_cells() {
    auto* client_base = reinterpret_cast<const unsigned char*>(GetModuleHandleA("client.dll"));
    constexpr size_t init_count = sizeof(init_cells) / sizeof(init_cells[0]);
    constexpr size_t named_count = sizeof(named_cells) / sizeof(named_cells[0]);
    uint32_t stale[init_count + named_count + 1], target[init_count + named_count + 1];
    size_t count = 0;
    for (const auto& cell : init_cells) {
        uint32_t value = 0;
        std::memcpy(&value, client_base + cell.client_rva, 4);
        if (value != cell.value) { say("    cell %08X: client immediate %X != %X, skipped", cell.stale, value, cell.value); continue; }
        stale[count] = cell.stale;
        target[count++] = reinterpret_cast<uint32_t>(client_base + cell.client_rva);
    }
    for (size_t i = 0; i < named_count; ++i) {
        int offset = netvar(named_cells[i].table, named_cells[i].prop);
        // m_MoveType is a datamap field, not networked; it sits right after m_nRenderMode.
        if (offset <= 0 && !std::strcmp(named_cells[i].prop, "m_MoveType")) offset = netvar("DT_BaseEntity", "m_nRenderMode") + 1;
        if (offset <= 0) { say("    cell %08X: netvar %s not found, skipped", named_cells[i].stale, named_cells[i].prop); continue; }
        named_values[i] = static_cast<uint32_t>(offset);
        stale[count] = named_cells[i].stale;
        target[count++] = reinterpret_cast<uint32_t>(&named_values[i]);
    }
    // m_iEFlags from InvalidatePhysicsRecursive: `or dword ptr [eax+disp32], 8000h`.
    if (auto* ipr = reinterpret_cast<const unsigned char*>(find("client.dll", "\x55\x8B\xEC\x83\xE4\xF8\x83\xEC\x0C\x53\x8B\x5D\x08\x8B\xC3\x56\x83\xE0\x04", 19)))
        for (int i = 0; i < 0x100 && !eflags_offset; ++i)
            if (ipr[i] == 0x81 && ipr[i + 1] == 0x88 && !std::memcmp(ipr + i + 6, "\x00\x80\x00\x00", 4)) std::memcpy(&eflags_offset, ipr + i + 2, 4);
    if (eflags_offset) { stale[count] = 0x2D9FA51A; target[count++] = reinterpret_cast<uint32_t>(&eflags_offset); }
    else say("    m_iEFlags offset not found; cell 2D9FA51A stays stale");
    say("    repaired %u netvar operands across the payload (%u cells, m_iEFlags=%X)",
        patch_cells_everywhere(stale, target, count), static_cast<unsigned>(count), eflags_offset);
    say("    re-keyed %u stale client function calls", rekey_stale_functions());
    fill_netvar_map();
    start_job_workers();
    return true;
}

// 354170 caches ConVar* by FNV-1a(name) in the unordered_map at 807E88; only the protected startup
// filled it. All 72 keys used by the payload are these convar names.
bool fill_convar_cache() {
    static constexpr const char* names[] = {
        "sv_maxusrcmdprocessticks", "cl_interp", "sv_clockcorrection_msecs", "fog_enableskybox", "fog_maxdensityskybox",
        "zoom_sensitivity_ratio_mouse", "sv_accelerate", "weapon_molotov_maxdetonateslope", "sv_maxspeed",
        "cl_extrapolate_amount", "fog_maxdensity", "mp_taser_recharge_time", "r_drawdecals", "view_recoil_tracking",
        "sv_noclipspeed", "sv_airaccelerate", "ff_damage_reduction_grenade_self", "game_type", "sv_force_transmit_ents",
        "sv_enablebunnyhopping", "mp_forcecamera", "sv_timebetweenducks", "host_timescale", "cl_crosshair_sniper_width",
        "mat_fullbright", "sv_gravity", "inferno_flame_lifetime", "ff_damage_reduction_other", "cl_accountprivacysetting1",
        "host_limitlocal", "cl_csm_max_shadow_dist", "cl_lagcompensation", "weapon_accuracy_nospread",
        "ff_damage_reduction_grenade", "occlusion_test_camera_margins", "weapon_air_spread_scale", "r_jiggle_bones",
        "fog_enable", "cl_pred_optimize", "sv_jump_impulse", "cl_csm_rot_override", "sv_noclipaccelerate", "cl_extrapolate",
        "ff_damage_reduction_bullets", "occlusion_test_jump_margin", "sv_maxunlag", "cl_cmdrate", "weapon_recoil_scale",
        "r_3dsky", "sv_stopspeed", "sv_friction", "cl_interp_ratio", "mp_solid_teammates", "viewmodel_recoil",
        "sv_force_transmit_players", "molotov_throw_detonate_time", "sv_cheats", "cl_use_new_headbob", "r_aspectratio",
        "mp_teammates_are_enemies", "ff_damage_bullet_penetration", "violence_hblood", "game_mode", "cl_foot_contact_shadows",
        "sv_accelerate_use_weapon_speed", "cl_updaterate", "cl_csm_rot_z", "cl_csm_rot_y", "cl_csm_rot_x",
        "sv_clip_penetration_traces_to_players", "fog_override", "net_earliertempents"};
    void* cvar = factory("vstdlib.dll", "VEngineCvar007");
    if (!cvar) return false;
    using FindVar = void*(__thiscall*)(void*, const char*);
    using Lookup = void*(__cdecl*)(uint32_t);
    using Find = uint32_t*(__thiscall*)(void*, uint32_t*, const uint32_t*, uint32_t);
    auto fnv = [](const unsigned char* bytes, size_t length) {
        uint32_t hash = 0x811C9DC5u;
        for (size_t i = 0; i < length; ++i) hash = (hash ^ bytes[i]) * 0x1000193u;
        return hash;
    };
    unsigned found = 0;
    for (const char* name : names) {
        const uint32_t key = fnv(reinterpret_cast<const unsigned char*>(name), std::strlen(name));
        reinterpret_cast<Lookup>(base + 0x354170)(key);  // inserts a null entry on a miss
        uint32_t result[2]{};
        reinterpret_cast<Find>(base + 0x150420)(reinterpret_cast<void*>(base + 0x807E88), result, &key,
                                               fnv(reinterpret_cast<const unsigned char*>(&key), 4));
        if (!result[1]) continue;
        void* convar = (*reinterpret_cast<FindVar**>(cvar))[16](cvar, name);
        // Removed in the final build; the occlusion eye box reads it unchecked, so use the general margin.
        if (!convar && !std::strcmp(name, "occlusion_test_camera_margins"))
            convar = (*reinterpret_cast<FindVar**>(cvar))[16](cvar, "occlusion_test_margins");
        *reinterpret_cast<void**>(result[1] + 0xC) = convar;
        found += convar != nullptr;
    }
    say("    convar cache: %u/%u resolved", found, static_cast<unsigned>(sizeof(names) / sizeof(names[0])));
    return true;
}

bool prepare() {
    client = factory("client.dll", "VClient018");
    if (!client) return false;

    const struct { Field field; const char* table; const char* name; } lookups[] = {
        {weapons, "DT_CSPlayer", "m_hMyWeapons"},
        {item_definition, "DT_BaseAttributableItem", "m_iItemDefinitionIndex"},
        {world_model_handle, "DT_WeaponCSBase", "m_hWeaponWorldModel"},
        {model_index, "DT_CSPlayer", "m_nModelIndex"},
        {viewmodel_weapon, "DT_BaseViewModel", "m_hWeapon"},
        {quality, "DT_BaseAttributableItem", "m_iEntityQuality"},
        {world_model_index, "DT_WeaponCSBase", "m_iWorldModelIndex"},
        {team, "DT_CSPlayer", "m_iTeamNum"},
        {viewmodel, "DT_CSPlayer", "m_hViewModel[0]"},
        {life_state, "DT_CSPlayer", "m_lifeState"},
        {alive, "DT_CSPlayerResource", "m_bAlive"},
        {viewmodel_owner, "DT_BaseViewModel", "m_hOwner"},
        {xuid_high, "DT_BaseAttributableItem", "m_OriginalOwnerXuidHigh"},
        {xuid_low, "DT_BaseAttributableItem", "m_OriginalOwnerXuidLow"},
        {item_id_high, "DT_BaseAttributableItem", "m_iItemIDHigh"},
        {item_id_low, "DT_BaseAttributableItem", "m_iItemIDLow"},
        {fallback_paint_kit, "DT_BaseAttributableItem", "m_nFallbackPaintKit"},
        {fallback_wear, "DT_BaseAttributableItem", "m_flFallbackWear"},
        {wearables, "DT_CSPlayer", "m_hMyWearables"},
        {body, "DT_CSPlayer", "m_nBody"},
        {account_id, "DT_BaseAttributableItem", "m_iAccountID"},
    };
    for (const auto& lookup : lookups) {
        int offset = netvar(lookup.table, lookup.name);
        if (lookup.field == viewmodel && offset < 0) offset = netvar(lookup.table, "m_hViewModel");
        say("    netvar %-24s = 0x%X\n", lookup.name, offset);
        if (offset <= 0) return false;
        fields[lookup.field] = static_cast<uint32_t>(offset);
    }

    // Original handler resolves these through the protected interface table; it never runs here.
    auto* engine_state = reinterpret_cast<uintptr_t*>(find("engine.dll", "\xA1????\x33\xD2\x6A\x00\x6A\x00\x33\xC9\x89\xB0", 15, 1));
    const uintptr_t hud = find("client.dll", "\xB9????\xE8????\x8B\x5D\x08", 13, 1);
    const uintptr_t weapon_system = find("client.dll", "\x8B\x35????\xFF\x10\x0F\xB7\xC0", 11, 2);
    const uintptr_t player_resource = find("client.dll", "\x8B\x3D????\x85\xFF\x0F\x84????\x81\xC7", 16, 2);
    const uintptr_t find_hud_element = find("client.dll", "\x55\x8B\xEC\x53\x8B\x5D\x08\x56\x57\x8B\xF9\x33\xF6\x39\x77\x28", 16);
    const uintptr_t clear_hud_weapons = find("client.dll", "\x55\x8B\xEC\x51\x53\x56\x8B\x75\x08\x8B\xD9\x57\x6B\xFE\x34", 15);
    if (!engine_state || !*engine_state || !hud || !weapon_system || !player_resource || !find_hud_element || !clear_hud_weapons) {
        say("    pattern scan failed: state=%p hud=%08X ws=%08X pr=%08X find=%08X clear=%08X\n", engine_state,
            static_cast<unsigned>(hud), static_cast<unsigned>(weapon_system), static_cast<unsigned>(player_resource),
            static_cast<unsigned>(find_hud_element), static_cast<unsigned>(clear_hud_weapons));
        return false;
    }
    if (!bind(0x4DE198, *engine_state, "ClientState") || !bind(0x4DE1D0, hud, "CHud") ||
        !bind(0x4DE190, reinterpret_cast<uintptr_t>(factory("engine.dll", "VModelInfoClient004")), "VModelInfoClient004") ||
        !bind(0x4DE208, weapon_system, "WeaponSystem") || !bind(0x4DE1B0, player_resource, "PlayerResource ptr") ||
        !bind(0x4DE168, reinterpret_cast<uintptr_t>(client), "VClient018") ||
        // Slot 0 (renderable,0,0,-1,-1) = CreateRenderableHandle, slot 1 (handle) = RemoveRenderable.
        !bind(0x4DE204, reinterpret_cast<uintptr_t>(factory("client.dll", "ClientLeafSystem002")), "ClientLeafSystem002") ||
        // Slots 3/5 (ClipRayToEntity/TraceRay), 13 (ScreenPosition), 3/5 (GetPhysicsProperties/GetSurfaceData).
        !bind(0x4DE180, reinterpret_cast<uintptr_t>(factory("engine.dll", "EngineTraceClient004")), "EngineTraceClient004") ||
        !bind(0x4DE184, reinterpret_cast<uintptr_t>(factory("engine.dll", "VDebugOverlay004")), "VDebugOverlay004") ||
        !bind(0x4DE18C, reinterpret_cast<uintptr_t>(factory("vphysics.dll", "VPhysicsSurfaceProps001")), "VPhysicsSurfaceProps001") ||
        // CInput: +0xAD m_fCameraInThirdPerson, +0xF4 m_pCommands (150 x 0x64).
        !bind(0x4DE1A8, find("client.dll", "\xB9????\xF3\x0F\x11\x04\x24\xFF\x50\x10", 13, 1), "CInput") ||
        // ISpatialPartition (slot 13 EnumerateElementsInSphere), IVModelRender (slot 1 ForcedMaterialOverride),
        // IVRenderView (slots 4/6 SetBlend/SetColorModulation), IPrediction (+8 m_bInPrediction).
        !bind(0x4DE1B8, reinterpret_cast<uintptr_t>(factory("engine.dll", "SpatialPartition001")), "SpatialPartition001") ||
        !bind(0x4DE1A0, reinterpret_cast<uintptr_t>(factory("engine.dll", "VEngineModel016")), "VEngineModel016") ||
        !bind(0x4DE1AC, reinterpret_cast<uintptr_t>(factory("engine.dll", "VEngineRenderView014")), "VEngineRenderView014") ||
        !bind(0x4DE194, reinterpret_cast<uintptr_t>(factory("client.dll", "VClientPrediction001")), "VClientPrediction001") ||
        // Address of the C_CSGameRules pointer (dwGameRulesProxy).
        !bind(0x4DE1B4, find("client.dll", "\xA1????\x85\xC0\x0F\x84????\x80\xB8?????\x74\x7A", 22, 1), "GameRules ptr"))
        return false;
    for (uint32_t slot = 0x4DE15C; slot <= 0x4DE208; slot += 4)
        if (!at<uintptr_t>(slot)) say("    interface slot RVA %X is still null", slot);
    // SetAbsOrigin(this, const Vector&): compares against m_vecAbsOrigin first. C_EconWearable::Equip(this, player).
    const uintptr_t set_abs_origin = find("client.dll",
        "\x55\x8B\xEC\x83\xE4\xF8\x51\x53\x56\x57\x8B\xF1\xE8????\x8B\x7D\x08\xF3\x0F\x10\x07\x0F\x2E\x86", 26);
    const uintptr_t equip_wearable = find("client.dll", "\x55\x8B\xEC\x83\xEC\x10\x53\x8B\x5D\x08\x57\x8B\xF9", 13);
    if (!set_abs_origin || !equip_wearable) {
        say("    glove helper scan failed: origin=%08X equip=%08X", static_cast<unsigned>(set_abs_origin), static_cast<unsigned>(equip_wearable));
        return false;
    }

    for (const auto& cell : cells)
        if (occurrences(cell) != cell.count) { say("    cell %08X count mismatch at RVA %X\n", cell.stale, cell.rva); return false; }
    // mov eax,K ; xor eax,93864482h -> FindHudElement.  mov ecx,K ; mov edx,4C95A326h -> cached ClearHudWeapons.
    // Glove path: mov eax,K ; xor eax,0E7F57F6h -> SetAbsOrigin.  mov esi,K ; xor esi,0E5F6D8B3h -> cached Equip.
    if (!bytes_are(0x1009ED, "\xB8\x72\x14\x3D\xBE", 5) || !bytes_are(0x1009F6, "\x35\x82\x44\x86\x93", 5) ||
        !bytes_are(0x100A1D, "\xB9\x66\x4B\x74\x61", 5) || !bytes_are(0x100A23, "\xBA\x26\xA3\x95\x4C", 5) ||
        !bytes_are(0x100E93, "\xB8\x86\x05\xD2\x23", 5) || !bytes_are(0x100E9B, "\x35\xF6\x57\x7F\x0E", 5) ||
        !bytes_are(0x101048, "\xBE\x53\xEB\x08\xC8", 5) || !bytes_are(0x10104D, "\x81\xF6\xB3\xD8\xF6\xE5", 6)) {
        say("    handler bytes do not match the verified profile");
        return false;
    }

    for (const auto& cell : cells) {
        const uint32_t address = reinterpret_cast<uint32_t>(&fields[cell.field]);
        for (uint32_t i = 0; i + 4 <= cell.size; ++i)
            if (!std::memcmp(reinterpret_cast<void*>(base + cell.rva + i), &cell.stale, 4)) write_code(cell.rva + i, &address, 4);
    }
    const uint32_t find_key = static_cast<uint32_t>(find_hud_element) ^ 0x93864482u;
    const uint32_t clear_key = static_cast<uint32_t>(clear_hud_weapons) ^ 0x4C95A326u;
    write_code(0x1009EE, &find_key, 4);
    write_code(0x100A1E, &clear_key, 4);
    const uint32_t origin_key = static_cast<uint32_t>(set_abs_origin) ^ 0x0E7F57F6u;
    const uint32_t equip_key = static_cast<uint32_t>(equip_wearable) ^ 0xE5F6D8B3u;
    write_code(0x100E94, &origin_key, 4);
    write_code(0x101049, &equip_key, 4);
    // The glove path's m_iAccountID cell lives in the payload (captured value 0x2FD8), cleared with BSS.
    at<uint32_t>(0x8059A8) = fields[account_id];
    return install_sequence_proxy() && install_localize_fallback() && install_paint_kit_hook() && install_cursor_hook() && install_input_hook() && install_render_hooks() &&
           repair_all_cells() && fill_convar_cache();
}

using Original = void(__fastcall*)(void*, void*, int);
Original original = nullptr;
volatile LONG disabled = 0;

// Prints the inputs 100900 uses for the held knife whenever they change.
void trace_knife() {
    using Thiscall = void*(__thiscall*)(void*);
    using FromHandle = void*(__thiscall*)(void*, uint32_t);
    using Category = int(__cdecl*)(int);
    using ModelIndex = int(__thiscall*)(void*, const char*);
    auto* local = reinterpret_cast<Thiscall>(base + 0xA06C0)(reinterpret_cast<void*>(base + 0x4DE210));
    if (!local) return;
    void* list = at<void*>(0x4DE170);
    auto handle_entity = [&](uint32_t handle) { return (*reinterpret_cast<FromHandle**>(list))[4](list, handle); };
    static char last[512];
    char line[512];
    int n = std::snprintf(line, sizeof(line), "[knife] enable=%u team=%d", at<unsigned char>(0x54CED0),
        *reinterpret_cast<int*>(reinterpret_cast<char*>(local) + fields[team]));
    for (int i = 0; i < 64 && n < 400; ++i) {
        auto* weapon = static_cast<char*>(handle_entity(*reinterpret_cast<uint32_t*>(static_cast<char*>(local) + fields[weapons] + i * 4)));
        if (!weapon) continue;
        const short def = *reinterpret_cast<short*>(weapon + fields[item_definition]);
        const int slot = reinterpret_cast<Category>(base + 0xFB820)(def);
        if (slot < 0 || slot >= 2) continue;
        const int selected = at<int>(0x550708 + slot * 0x15C);
        n += std::snprintf(line + n, sizeof(line) - n, " | def=%d slot=%d selected=%d model=%d", def, slot, selected,
            *reinterpret_cast<int*>(weapon + fields[model_index]));
        const auto begin = at<uintptr_t>(0x7DBC1C), end = at<uintptr_t>(0x7DBC20);
        if (selected > 0 && begin + selected * 0x34 <= end) {
            auto* entry = reinterpret_cast<char*>(begin + (selected - 1) * 0x34);
            auto* path = *reinterpret_cast<uint32_t*>(entry + 0x18) >= 16 ? *reinterpret_cast<const char**>(entry + 4) : entry + 4;
            void* info = at<void*>(0x4DE190);
            n += std::snprintf(line + n, sizeof(line) - n, " entry_def=%d path=%.80s index=%d", *reinterpret_cast<int*>(entry), path,
                (*reinterpret_cast<ModelIndex**>(info))[2](info, path));
        }
    }
    if (std::strcmp(line, last)) { std::puts(line); write_log(line); std::strcpy(last, line); }
}

void run_trace() {
    __try { trace_knife(); } __except (EXCEPTION_EXECUTE_HANDLER) { std::puts("[knife] trace faulted"); }
}

void run_stage_handler() {
    __try {
        reinterpret_cast<void(__cdecl*)()>(base + 0x100900)();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&disabled, 1);
        say("[!] Original stage-2 handler faulted (0x%08lX); disabled.\n", GetExceptionCode());
    }
}

// Original routines run with no arguments; each is isolated so an unresolved dependency disables
// only that routine, logged with its fault location.
struct Routines { const char* label; const uint32_t* rvas; size_t count; volatile LONG* disabled; };

int routine_fault(EXCEPTION_POINTERS* e, const Routines& set, size_t index) {
    const auto address = reinterpret_cast<uintptr_t>(e->ExceptionRecord->ExceptionAddress);
    const bool in_payload = address >= base && address < base + cfg::BINARY_SIZE;
    say("[!] %s %06X faulted %08lX at %s%06X reading %08X; disabled", set.label, set.rvas[index],
        e->ExceptionRecord->ExceptionCode, in_payload ? "payload+" : "", static_cast<unsigned>(in_payload ? address - base : address),
        static_cast<unsigned>(e->ExceptionRecord->NumberParameters >= 2 ? e->ExceptionRecord->ExceptionInformation[1] : 0));
    InterlockedExchange(&set.disabled[index], 1);
    return EXCEPTION_EXECUTE_HANDLER;
}

void run_routine(const Routines& set, size_t index) {
    __try {
        reinterpret_cast<void(__cdecl*)()>(base + set.rvas[index])();
    } __except (routine_fault(GetExceptionInformation(), set, index)) {
    }
}

void run_routines(const Routines& set) {
    for (size_t i = 0; i < set.count; ++i)
        if (!set.disabled[i]) run_routine(set, i);
}

// Original detour 14F740, FRAME_RENDER_START (5): these before the original, two more after.
// 14B9C0 (stale cell 2DC6FDE6) is left out.
constexpr uint32_t render_start_before[] = {0x14BE10, 0x14B940, 0x14B8C0, 0x14BBA0, 0x14B830,
                                            0x14C080, 0x14DAA0, 0x107AD0, 0x370BA0, 0x0B32A0, 0x13AC30};
constexpr uint32_t render_start_after[] = {0x14B7A0, 0x0F22B0};
volatile LONG before_disabled[sizeof(render_start_before) / sizeof(render_start_before[0])]{};
volatile LONG after_disabled[sizeof(render_start_after) / sizeof(render_start_after[0])]{};
const Routines render_before{"render-start", render_start_before, sizeof(render_start_before) / sizeof(render_start_before[0]), before_disabled};
const Routines render_after{"render-start-post", render_start_after, sizeof(render_start_after) / sizeof(render_start_after[0]), after_disabled};
// FRAME_NET_UPDATE_END (4), after the original: player record cache (ESP data), then two more.
constexpr uint32_t net_update_end[] = {0x0F0B30, 0x370670, 0x1348F0};
volatile LONG net_update_disabled[sizeof(net_update_end) / sizeof(net_update_end[0])]{};
const Routines net_update{"net-update-end", net_update_end, sizeof(net_update_end) / sizeof(net_update_end[0]), net_update_disabled};

// Stage 2 runs the original then 100900. The detour's stage 4/5 viewmodel cycle/anim-time
// save/restore is not run: two of its cells are not yet verified and it writes through them.
void __fastcall hook(void* self, void* edx, int stage) {
    const bool ready = primordial_menu_state == menu_ui::ready;
    if (stage == 5 && ready) run_routines(render_before);
    original(self, edx, stage);
    if (stage == 5 && ready) run_routines(render_after);
    if (stage == 2 && !disabled && ready) { run_trace(); run_stage_handler(); }
    if (stage == 4 && ready) {
        if (!at<int>(0x57ED7C)) fill_class_ids();
        if (at<int>(0x57ED7C)) run_routines(net_update);
    }
}
}

bool install() noexcept {
    __try {
        say("[frame_stage] install");
        if (!prepare()) { say("[frame_stage] prepare failed"); return false; }
        void* target = (*reinterpret_cast<void***>(client))[37];
        auto status = MH_Initialize();
        if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) return false;
        if (MH_CreateHook(target, reinterpret_cast<void*>(&hook), reinterpret_cast<void**>(&original)) != MH_OK) return false;
        if (MH_EnableHook(target) != MH_OK) { MH_RemoveHook(target); return false; }
        say("[frame_stage] FrameStageNotify hooked at %p", target);
        prepared = 1;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

namespace {
// Original paint detour 154790 calls these visual feature roots (no arguments) in this order
// before the menu frame 32BA40; each is isolated so one unresolved dependency disables only it.
// Left out until their stale cells are resolved (a stale cell reads client code as an offset):
// 14B9C0 (stale cell 2DC6FDE6) stays out.
constexpr uint32_t visual_routines[] = {
    0x163A20, 0x0F06C0, 0x130460, 0x13ACC0, 0x130B50, 0x0E4390, 0x13EAD0, 0x149790, 0x0F6730, 0x0C4620,
    0x0B5680, 0x0B1250, 0x0CD800, 0x0F64E0, 0x0CB240, 0x10BE80, 0x106B90, 0x0F4860, 0x14E120, 0x10BE90};
volatile LONG visual_disabled[sizeof(visual_routines) / sizeof(visual_routines[0])]{};
const Routines visuals{"visual", visual_routines, sizeof(visual_routines) / sizeof(visual_routines[0]), visual_disabled};
}

void run_visuals() noexcept {
    if (prepared) run_routines(visuals);
}
}
