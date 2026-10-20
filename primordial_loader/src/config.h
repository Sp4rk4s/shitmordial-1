#pragma once
#include <cstdint>

namespace cfg {
    constexpr uint32_t IMAGE_BASE       = 0x44340000; // original absolute-address base; IDA is still at 0x44000000
    constexpr uint32_t BINARY_SIZE      = 0x140C000;  // file size (copy this much)
    constexpr uint32_t IMAGE_SIZE       = 0x1411000;  // synthetic SizeOfImage plus committed CRT padding page
    constexpr uint32_t ENTRY_RVA        = 0x3CA002;  // DllMainCRTStartup
    constexpr uint32_t PATTERN_SCAN_RVA = 0x21E560;  // verified module pattern scanner, x86 cdecl
    constexpr uint32_t DLLMAIN_RVA      = 0x14F360;  // user DllMain (skips CRT)
    constexpr uint32_t AUTH_CHECK_RVA   = 0xB3CE96;  // auth/license check to bypass

    // Section layout from memory dump analysis
    struct Section {
        const char* name;
        uint32_t rva;
        uint32_t vsize;
        uint32_t raw_offset;
        uint32_t raw_size;
        uint32_t characteristics;
    };

    constexpr Section SECTIONS[] = {
        // Original section headers are missing. The previous guessed .rdata
        // range contains executable bytes (including RVA 0x40B000). Preserve
        // access across the captured image until real boundaries are recovered.
        { ".dump", 0x001000, 0x140F000, 0x001000, BINARY_SIZE - 0x1000, 0xE0000020 },
    };
    constexpr int NUM_SECTIONS = sizeof(SECTIONS) / sizeof(SECTIONS[0]);

    // CRT import resolution globals (the binary's own resolver uses these addresses)
    constexpr uint32_t IMPORT_RESOLVER_OBJ_1 = 0x4481DF78; // kernel32/ntdll resolver object
    constexpr uint32_t IMPORT_RESOLVER_OBJ_2 = 0x4481E068; // secondary resolver object

    // Security cookie location
    constexpr uint32_t SECURITY_COOKIE      = 0x4481B628;
    constexpr uint32_t SECURITY_COOKIE_COMP = 0x4481B624;
}
