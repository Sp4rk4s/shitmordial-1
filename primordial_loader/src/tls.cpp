#include "tls.h"
#include <windows.h>
#include <intrin.h>
#include <cstring>
#include <mutex>

namespace payload_tls {
namespace {
std::mutex guard;
HMODULE carrier = nullptr;
uint32_t assigned_index = 0;
constexpr size_t directory_rva = 0x461100;
constexpr size_t template_rva = 0x4646D4;
constexpr size_t index_rva = 0x826264;
constexpr uint32_t directory[] = {
    0x447A46D4, 0x447A46DC, 0x44B66264, 0x44750204, 0, 0x00300000
};
bool valid_block(uint32_t index) noexcept {
    if (index == TLS_OUT_OF_INDEXES || index > 0xFFFF) return false;
    __try {
        auto** slots = reinterpret_cast<void**>(__readfsdword(0x2C));
        auto* block = slots ? static_cast<unsigned char*>(slots[index]) : nullptr;
        return block && *reinterpret_cast<uint64_t*>(block) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool block_values(uint32_t index, uint32_t* output) noexcept {
    if (index == TLS_OUT_OF_INDEXES || index > 0xFFFF) return false;
    __try {
        auto** slots = reinterpret_cast<void**>(__readfsdword(0x2C));
        if (!slots || !slots[index]) return false;
        auto* values = static_cast<uint32_t*>(slots[index]);
        output[0] = values[0]; output[1] = values[1];
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
std::string block_status(uint32_t index) {
    uint32_t values[2]{};
    if (!block_values(index, values)) return "missing/unreadable block for index " + std::to_string(index);
    return "index " + std::to_string(index) + ", template words " +
        std::to_string(values[0]) + "," + std::to_string(values[1]);
}
}
Result initialize(uint8_t* image, size_t size) {
    std::lock_guard lock(guard);
    if (!image || size < index_rva + 4 ||
        std::memcmp(image + directory_rva, directory, sizeof(directory)) ||
        *reinterpret_cast<const uint64_t*>(image + template_rva) != 0 ||
        *reinterpret_cast<const uint32_t*>(image + 0x410204) != 0 ||
        *reinterpret_cast<const uint32_t*>(image + index_rva) != 0)
        return {false, 0, "Recovered static TLS profile does not match the dump."};
    if (!carrier) {
        HMODULE owner = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&initialize), &owner))
            return {false, 0, "Cannot locate patcher for static TLS support."};
        wchar_t filename[32768];
        const DWORD count = GetModuleFileNameW(owner, filename, ARRAYSIZE(filename));
        if (!count || count >= ARRAYSIZE(filename))
            return {false, 0, "Cannot read patcher path for static TLS support."};
        std::wstring path(filename, count);
        const auto slash = path.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
            return {false, 0, "Invalid patcher path for static TLS support."};
        path.resize(slash + 1);
        path += L"primordial_tls.dll";
        HMODULE loaded = LoadLibraryExW(path.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!loaded)
            return {false, 0, "Cannot load primordial_tls.dll beside the patcher (" +
                std::to_string(GetLastError()) + ")."};
        using GetIndex = DWORD (__cdecl*)();
        auto get_index = reinterpret_cast<GetIndex>(GetProcAddress(loaded, "primordial_tls_get_index"));
        if (!get_index || !valid_block(get_index())) {
            const auto detail = get_index ? block_status(get_index()) : "missing index export";
            FreeLibrary(loaded);
            return {false, 0, "Static TLS support module has no valid empty thread block: " + detail};
        }
        assigned_index = get_index();
        // Retain this module for the lifetime of the mapped payload and all its
        // threads. Windows owns per-thread allocation, including future threads.
        carrier = loaded;
    }
    std::memcpy(image + index_rva, &assigned_index, sizeof(assigned_index));
    return {true, assigned_index, {}};
}
}
