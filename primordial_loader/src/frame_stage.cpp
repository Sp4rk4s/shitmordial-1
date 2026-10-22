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
bool install_input_hook() {
    void* input = factory("inputsystem.dll", "InputSystemVersion001");
    if (!input) return false;
    auto** vtable = *reinterpret_cast<unsigned char***>(input);
    // push ebp; mov ebp,esp; mov edx,[ebp+8]; mov eax,1; push esi; mov esi,ecx; mov ecx,edx; and ecx,1Fh; sar edx,5
    constexpr unsigned char is_button_down[] = {0x55,0x8B,0xEC,0x8B,0x55,0x08,0xB8,0x01,0x00,0x00,0x00,0x56,0x8B,0xF1,0x8B,0xCA,0x83,0xE1,0x1F,0xC1,0xFA,0x05};
    void* target = nullptr;
    for (int i = 0; i < 40; ++i)
        if (!std::memcmp(vtable[i], is_button_down, sizeof(is_button_down))) { target = vtable[i]; break; }
    if (!target) { say("    IsButtonDown not found in InputSystemVersion001"); return false; }
    if (!bytes_are(0x1529A0, "\x55\x8B\xEC\x56\x57\x8B\xF2\x8B\xF9\xE8", 10)) { say("    IsButtonDown detour does not match"); return false; }
    auto status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) return false;
    if (MH_CreateHook(target, reinterpret_cast<void*>(base + 0x1529A0), reinterpret_cast<void**>(base + 0x7FB794)) != MH_OK) return false;
    if (MH_EnableHook(target) != MH_OK) { MH_RemoveHook(target); return false; }
    say("    IsButtonDown %p -> original detour 1529A0", target);
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
        !bind(0x4DE204, reinterpret_cast<uintptr_t>(factory("client.dll", "ClientLeafSystem002")), "ClientLeafSystem002"))
        return false;
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
    return install_sequence_proxy() && install_localize_fallback() && install_paint_kit_hook() && install_cursor_hook() && install_input_hook();
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

// Original detour 14F740: other stages first call the original, then stage 2 runs 100900.
void __fastcall hook(void* self, void* edx, int stage) {
    original(self, edx, stage);
    if (stage == 2 && !disabled && primordial_menu_state == menu_ui::ready) { run_trace(); run_stage_handler(); }
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
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
}
