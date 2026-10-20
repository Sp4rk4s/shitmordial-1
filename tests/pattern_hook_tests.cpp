#include "../primordial_loader/src/config.h"
#include "../primordial_loader/src/pattern_hook.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
using Scan = uintptr_t (__cdecl*)(const char*, uintptr_t, const char*);
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        // The hook must not allocate an image or touch unrelated/mismatched code.
        require(!pattern_hook::install().ok, "hook accepted an absent image");
        void* base = VirtualAlloc(reinterpret_cast<void*>(cfg::IMAGE_BASE), cfg::IMAGE_SIZE,
            MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        require(base == reinterpret_cast<void*>(cfg::IMAGE_BASE), "cannot create synthetic mapping");
        auto* scanner = static_cast<uint8_t*>(base) + cfg::PATTERN_SCAN_RVA;
        std::memset(scanner, 0x90, 64);
        require(!pattern_hook::install().ok, "hook accepted mismatched scanner bytes");
        require(scanner[0] == 0x90, "rejected scanner was modified");

        // Same first 16 bytes as the observed scanner, including its FS read and
        // absolute exception-handler immediate. The synthetic body XORs all three
        // stack arguments and uses a plain ret to verify the full cdecl ABI.
        const uint8_t code[] = {
            0x55, 0x8B, 0xEC, 0x64, 0xA1, 0, 0, 0, 0,
            0x6A, 0xFF, 0x68, 0xED, 0xB1, 0x71, 0x44,
            0x8B, 0x45, 0x08, 0x33, 0x45, 0x0C, 0x33, 0x45, 0x10,
            0x8B, 0xE5, 0x5D, 0xC3
        };
        std::memcpy(scanner, code, sizeof(code));
        require(FlushInstructionCache(GetCurrentProcess(), scanner, sizeof(code)) != FALSE, "cache flush failed");
        const auto scan = reinterpret_cast<Scan>(scanner);
        const char* module = "synthetic.dll";
        const char* pattern = "55 8B ?? ? C3";
        const uintptr_t reserved = 0x12345678;
        const uintptr_t expected = reinterpret_cast<uintptr_t>(module) ^ reserved ^ reinterpret_cast<uintptr_t>(pattern);
        require(scan(module, reserved, pattern) == expected, "synthetic original is invalid");

        const auto installed = pattern_hook::install();
        require(installed.ok, installed.error.c_str());
        require(scanner[0] == 0xE9, "MinHook did not publish the detour");
        require(pattern_hook::install().ok, "repeat installation failed");
        SetLastError(0x3456);
        require(scan(module, reserved, pattern) == expected, "trampoline changed arguments or result");
        require(GetLastError() == 0x3456, "diagnostic logging changed the original last-error value");
        const uintptr_t zero_reserved = reinterpret_cast<uintptr_t>(module) ^ reinterpret_cast<uintptr_t>(pattern);
        require(scan(module, zero_reserved, pattern) == 0, "zero scan result changed");
        require(pattern_hook::calls() == 2, "detour call count is wrong");

        // Invalid string pointers must not make diagnostic logging crash; the
        // fixture intentionally treats them as numbers instead of dereferencing.
        const auto* unreadable = reinterpret_cast<const char*>(1);
        require(scan(unreadable, 0, nullptr) == 1, "logging changed unreadable/null pointer forwarding");
        require(pattern_hook::calls() == 3, "unreadable pointer call not recorded");
        const auto removed = pattern_hook::remove();
        require(removed.ok, removed.error.c_str());
        require(std::memcmp(scanner, code, sizeof(code)) == 0, "original scanner bytes not restored");
        require(scan(module, reserved, pattern) == expected && pattern_hook::calls() == 3,
                "removed detour still invoked");
        require(pattern_hook::remove().ok, "repeat removal failed");
        VirtualFree(base, 0, MEM_RELEASE);
        std::printf("PASS: actual MinHook trampoline, cdecl forwarding, logging and removal\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
