#include "iat_hook.h"
#include <tlhelp32.h>
#include <cstdint>
#include <cstring>

namespace iat {
namespace {
bool inside(size_t offset, size_t length, size_t size) {
    return offset <= size && length <= size - offset;
}
size_t patch_module(HMODULE module, size_t size, const char* function, void* original, void* detour) {
    auto* image = reinterpret_cast<uint8_t*>(module);
    if (size < sizeof(IMAGE_DOS_HEADER)) return 0;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 ||
        !inside(dos->e_lfanew, sizeof(IMAGE_NT_HEADERS), size)) return 0;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC ||
        nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_IMPORT) return 0;
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress || !inside(dir.VirtualAddress, dir.Size, size)) return 0;
    size_t patched = 0;
    for (size_t off = 0; inside(off, sizeof(IMAGE_IMPORT_DESCRIPTOR), dir.Size); off += sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
        const auto* descriptor = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(image + dir.VirtualAddress + off);
        if (!descriptor->Name) break;
        if (!descriptor->FirstThunk) continue;
        for (size_t index = 0;; ++index) {
            const size_t delta = index * sizeof(IMAGE_THUNK_DATA);
            if (!inside(size_t(descriptor->FirstThunk) + delta, sizeof(IMAGE_THUNK_DATA), size)) break;
            auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(image + descriptor->FirstThunk + delta);
            if (!thunk->u1.Function) break;
            bool match = reinterpret_cast<void*>(thunk->u1.Function) == original;
            if (descriptor->OriginalFirstThunk) {
                if (!inside(size_t(descriptor->OriginalFirstThunk) + delta, sizeof(IMAGE_THUNK_DATA), size)) break;
                const auto* lookup = reinterpret_cast<const IMAGE_THUNK_DATA*>(image + descriptor->OriginalFirstThunk + delta);
                if (!IMAGE_SNAP_BY_ORDINAL(lookup->u1.Ordinal)) {
                    const auto rva = static_cast<size_t>(lookup->u1.AddressOfData);
                    if (inside(rva, sizeof(WORD) + 1, size)) {
                        const auto* name = reinterpret_cast<const char*>(image + rva + sizeof(WORD));
                        if (std::memchr(name, 0, size - rva - sizeof(WORD))) match = std::strcmp(name, function) == 0;
                    }
                }
            }
            if (!match || reinterpret_cast<void*>(thunk->u1.Function) == detour) continue;
            DWORD old = 0;
            if (!VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &old)) continue;
            void* found = InterlockedCompareExchangePointer(
                reinterpret_cast<void* volatile*>(&thunk->u1.Function), detour, original);
            DWORD ignored = 0;
            VirtualProtect(&thunk->u1.Function, sizeof(void*), old, &ignored);
            if (found == original) ++patched;
        }
    }
    return patched;
}
size_t guarded_patch(HMODULE module, size_t size, const char* name, void* original, void* detour) {
    __try { return patch_module(module, size, name, original, detour); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
}
size_t hook(const char* name, void* original, void* detour) {
    if (!name || !original || !detour) return 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    size_t patched = 0;
    if (Module32FirstW(snapshot, &entry)) do {
        HMODULE module = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                reinterpret_cast<LPCWSTR>(entry.modBaseAddr), &module)) {
            patched += guarded_patch(module, entry.modBaseSize, name, original, detour);
            FreeLibrary(module);
        }
    } while (Module32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    return patched;
}
}
