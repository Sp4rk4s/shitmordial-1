#include "startup.h"
#include "config.h"
#include "game_modules.h"
#include <cstring>

namespace startup {
Result prepare(uint8_t* image, size_t size) {
    constexpr size_t state = 0x826238;
    constexpr uint8_t captured[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x3C, 0x77, 0xD2, 0x9B, 0x3C, 0x77, 0xFE, 0xA5, 0x3C, 0x77, 0xD2, 0xAB, 0x50, 0x36, 0x4A, 0x02, 0x50, 0x36, 0x4A, 0x02, 0x50, 0x36, 0x4A, 0x02, 0x05, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 };
    constexpr size_t cookie = cfg::SECURITY_COOKIE - cfg::IMAGE_BASE;
    constexpr size_t alphabet = 0x4DB634;
    // Verified menu-model ownership fields outside linker BSS. Menu creation
    // move-assigns into these objects, which would otherwise free/reuse heaps
    // from the captured process. Never free their old pointers here.
    struct WideOwnershipProfile { size_t offset; uint32_t captured[6]; };
    constexpr WideOwnershipProfile menu_strings[] = {
        {0x4B33B4, {0x43E85588, 0, 0, 0, 10, 15}},
        {0x4B3D38, {0x416BB3C8, 0x00650076, 0, 0, 0, 15}},
        // Auxiliary models move-assign these strings during menu construction.
        // Native release helper RVA A11A0 frees heap storage when capacity>=8.
        {0x4B349C, {0x42326890, 0, 0, 0, 10, 15}},
        {0x4B3584, {0x42326638, 0, 0, 0, 8, 15}},
        {0x4B366C, {0x423268E0, 0, 0, 0, 8, 15}},
        // Config initialization replaces its path; periodic menu refresh clears
        // the second label through the same verified wstring storage helper.
        {0x4B3D14, {0x41C66620, 0, 0, 0, 18, 23}},
        {0x4B3C44, {0x417EADA0, 0x3C386838, 0x0097D52C, 0x443E11D5, 19, 23}},
    };
    constexpr size_t selector_guard = 0x34356A;
    constexpr uint8_t captured_selector_guard[] = {
        0x85, 0xFF, 0x78, 0x46, 0x8B, 0x99, 0x50, 0x01, 0x00, 0x00,
        0xB8, 0x93, 0x24, 0x49, 0x92, 0x8B, 0x89, 0x54, 0x01, 0x00, 0x00,
        0x2B, 0xCB, 0xF7, 0xE9, 0x03, 0xD1, 0xC1, 0xFA, 0x04,
        0x8B, 0xC2, 0xC1, 0xE8, 0x1F, 0x48, 0x03, 0xC2, 0x3B, 0xF8, 0x77, 0x20,
    };
    constexpr size_t menu_vector = 0x4B3484;
    constexpr uint32_t captured_menu_vector[] = {0x41433B70, 0x41433BE8, 0x41433BE8};
    // Linker GCTL contribution: .bss RVA 0x4DDBE0, size 0x348A74.
    // Constructors can omit zero stores because ordinary PE loading supplies
    // zeroed BSS. Captured counts/once flags must not survive a fresh startup.
    constexpr size_t bss = 0x4DDBE0;
    constexpr size_t bss_size = 0x348A74;
    constexpr size_t loader_input = 0x4DDCE0;
    constexpr size_t loader_input_size = 0x205; // username[512], id, mode byte
    constexpr size_t ac130_mode = loader_input + 0x204;
    constexpr uint8_t captured_alphabet[] = {
        0xD8, 0x76, 0x1F, 0x43, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0x3F, 0, 0, 0, 0x3F, 0, 0, 0
    };
    static_assert(cookie == 0x4DB628);
    if (!image || size < bss + bss_size || size < cookie + 4)
        return {false, "CRT startup profile is outside the image."};
    uint32_t value = 0, complement = 0;
    std::memcpy(&value, image + cookie, 4);
    std::memcpy(&complement, image + cookie - 4, 4);
    if ((value ^ complement) != 0xFFFFFFFFu || !(value & 0xFFFF0000u) ||
        value == 0xBB40E64Eu || std::memcmp(image + state, captured, sizeof(captured)) ||
        std::memcmp(image + alphabet, captured_alphabet, sizeof(captured_alphabet)))
        return {false, "Captured CRT state/cookie does not match the verified startup profile."};
    if (std::memcmp(image + menu_vector, captured_menu_vector, sizeof(captured_menu_vector)))
        return {false, "Captured menu ownership state does not match the verified startup profile."};
    for (const auto& profile : menu_strings)
        if (std::memcmp(image + profile.offset, profile.captured, sizeof(profile.captured)))
            return {false, "Captured menu ownership state does not match the verified startup profile."};
    if (std::memcmp(image + selector_guard, captured_selector_guard, sizeof(captured_selector_guard)))
        return {false, "Menu selector bounds instructions do not match the verified profile."};
    for (const auto& binding : game_modules::bindings) {
        uint32_t hash = 0;
        std::memcpy(&hash, image + binding.rva, 4);
        if (hash != binding.hash)
            return {false, "Captured game module hash does not match the verified startup profile."};
    }
    // count==0 gives count-1==-1. The captured JA treats that as UINT_MAX and
    // wrongly accepts index0. Indices are already checked for negativity;
    // JG rejects this empty case and preserves every valid 32-bit vector bound.
    image[0x343592] = 0x7F;
    // The alphabet initializer (RVA 0x4680) calls string::assign directly.
    // The dump holds a 63-byte string backed by the previous process's heap.
    // Restore the verified MSVC x86 empty-string layout before constructors;
    // never free or attempt to reuse the captured pointer 0x431F76D8.
    std::memset(image + alphabet, 0, sizeof(captured_alphabet));
    const uint32_t inline_capacity = 15;
    std::memcpy(image + alphabet + 20, &inline_capacity, sizeof(inline_capacity));
    // These objects are MSVC x86 wstrings: 16-byte union, size, capacity.
    // The 20-byte-element vector's verified release helper only frees its
    // storage and clears the three pointers. New contents are assigned by the
    // menu constructor; preserve the surrounding settings and scalar fields.
    const uint32_t wide_inline_capacity = 7;
    for (const auto& profile : menu_strings) {
        std::memset(image + profile.offset, 0, 24);
        std::memcpy(image + profile.offset + 20, &wide_inline_capacity, 4);
    }
    std::memset(image + menu_vector, 0, sizeof(captured_menu_vector));
    uint8_t settings[loader_input_size];
    std::memcpy(settings, image + loader_input, sizeof(settings));
    std::memset(image + bss, 0, bss_size);
    std::memcpy(image + loader_input, settings, sizeof(settings));
    // Mode byte = ac-130 build tier. Attach helper A1E72 copies it to RVA 4DE245,
    // read by the title-bar tag (351AB0) and tier-gated feature paths.
    image[ac130_mode] = 1;
    // These initialized resolver inputs reside inside the linker BSS range.
    // Retain identifiers, discard captured HMODULEs and all container pointers.
    for (const auto& binding : game_modules::bindings)
        std::memcpy(image + binding.rva, &binding.hash, 4);
    // Keep the original cookie: captured CRT pointers may be encoded with it.
    return {true, {}};
}
}
