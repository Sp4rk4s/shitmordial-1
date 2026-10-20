#include "pattern_hook.h"
#include "config.h"
#include <MinHook.h>
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <mutex>

static_assert(sizeof(void*) == 4, "The verified scanner ABI is x86.");

namespace pattern_hook {
namespace {
// IDA assembly: module at [ebp+8], unused/reserved at [ebp+0Ch],
// pattern at [ebp+10h], result in EAX, plain ret (caller cleans the stack).
using Scan = uintptr_t (__cdecl*)(const char*, uintptr_t, const char*);
Scan original = nullptr;
void* target = nullptr;
bool initialized_here = false;
std::mutex lifecycle;
volatile LONG call_count = 0;
constexpr uint8_t prefix[] = {
    0x55, 0x8B, 0xEC, 0x64, 0xA1, 0, 0, 0, 0,
    0x6A, 0xFF, 0x68, 0xED, 0xB1, 0x71, 0x44
};

void copy_text(char* output, size_t capacity, const char* input) noexcept {
    if (!input) { strcpy_s(output, capacity, "<null>"); return; }
    __try {
        size_t i = 0;
        for (; i + 1 < capacity && input[i]; ++i) {
            const unsigned char c = static_cast<unsigned char>(input[i]);
            output[i] = c >= 32 && c < 127 ? static_cast<char>(c) : '.';
        }
        output[i] = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        strcpy_s(output, capacity, "<unreadable>");
    }
}

uintptr_t __cdecl detour(const char* module, uintptr_t reserved, const char* pattern) {
    const uintptr_t result = original(module, reserved, pattern);
    const DWORD error = GetLastError();
    const LONG number = InterlockedIncrement(&call_count);
    char module_text[128], pattern_text[256], line[640];
    copy_text(module_text, sizeof(module_text), module);
    copy_text(pattern_text, sizeof(pattern_text), pattern);
    snprintf(line, sizeof(line), "[pattern_scan #%ld] module=\"%s\" pattern=\"%s\" reserved=0x%08X result=0x%08X\n",
        number, module_text, pattern_text, static_cast<unsigned>(reserved), static_cast<unsigned>(result));
    OutputDebugStringA(line);
    std::fputs(line, stdout);
    SetLastError(error);
    return result;
}

Result status(const char* operation, MH_STATUS code) {
    return {false, std::string(operation) + ": " + MH_StatusToString(code)};
}

bool verified_prefix(const void* address) noexcept {
    __try { return std::memcmp(address, prefix, sizeof(prefix)) == 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
}

Result install() {
    std::lock_guard lock(lifecycle);
    if (target) return {true, {}};
    auto* address = reinterpret_cast<void*>(cfg::IMAGE_BASE + cfg::PATTERN_SCAN_RVA);
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(address, &region, sizeof(region)) || region.State != MEM_COMMIT ||
        region.AllocationBase != reinterpret_cast<void*>(cfg::IMAGE_BASE) ||
        region.Type != MEM_PRIVATE || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
        !(region.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) ||
        !verified_prefix(address))
        return {false, "Pattern scanner is absent, non-executable, or does not match the verified dump at 0x4455E560."};

    auto code = MH_Initialize();
    if (code != MH_OK && code != MH_ERROR_ALREADY_INITIALIZED) return status("MH_Initialize", code);
    initialized_here = code == MH_OK;
    void* trampoline = nullptr;
    code = MH_CreateHook(address, reinterpret_cast<void*>(&detour), &trampoline);
    if (code != MH_OK) {
        if (initialized_here) MH_Uninitialize();
        initialized_here = false;
        return status("MH_CreateHook", code);
    }
    original = reinterpret_cast<Scan>(trampoline);
    code = MH_EnableHook(address);
    if (code != MH_OK) {
        MH_RemoveHook(address);
        original = nullptr;
        if (initialized_here) MH_Uninitialize();
        initialized_here = false;
        return status("MH_EnableHook", code);
    }
    // A payload may create threads which continue scanning after entry returns.
    // Keep detour code resident even if the initialization worker reports failure.
    HMODULE pinned = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&detour), &pinned)) {
        MH_DisableHook(address);
        MH_RemoveHook(address);
        original = nullptr;
        if (initialized_here) MH_Uninitialize();
        initialized_here = false;
        return {false, "Cannot keep the pattern-hook module resident."};
    }
    target = address;
    printf("[+] MinHook pattern scanner installed at 0x%08X (logging and forwarding).\n",
        cfg::IMAGE_BASE + cfg::PATTERN_SCAN_RVA);
    return {true, {}};
}

Result remove() {
    std::lock_guard lock(lifecycle);
    if (!target) return {true, {}};
    auto code = MH_DisableHook(target);
    if (code != MH_OK) return status("MH_DisableHook", code);
    code = MH_RemoveHook(target);
    if (code != MH_OK) return status("MH_RemoveHook", code);
    target = nullptr;
    original = nullptr;
    if (initialized_here) {
        code = MH_Uninitialize();
        if (code != MH_OK) return status("MH_Uninitialize", code);
        initialized_here = false;
    }
    return {true, {}};
}

uint32_t calls() { return static_cast<uint32_t>(InterlockedCompareExchange(&call_count, 0, 0)); }
}
