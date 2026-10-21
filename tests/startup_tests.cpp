#include "../primordial_loader/src/startup.h"
#include "../primordial_loader/src/config.h"
#include "../primordial_loader/src/game_modules.h"
#include <fstream>
#include <iterator>
#include <vector>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <array>
#include <windows.h>
#include <climits>

void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
uint32_t word(const std::vector<uint8_t>& image, size_t offset) {
    uint32_t value; std::memcpy(&value, image.data() + offset, 4); return value;
}
uintptr_t fake_module(const char*, void* context) {
    return 0x60000000u + 0x10000u * ++*static_cast<unsigned*>(context);
}
uintptr_t missing_module(const char*, void* context) {
    return ++*static_cast<unsigned*>(context) == 2 ? 0 : 0x60000000u;
}
struct MissingLateModule { unsigned calls=0; const char* missing; };
uintptr_t missing_late_module(const char* name, void* context) {
    auto& probe=*static_cast<MissingLateModule*>(context);++probe.calls;
    return std::strcmp(name,probe.missing)?0x60000000u:0;
}

// Execute just the verified 42-byte bounds decision, with synthetic success/
// failure returns instead of the payload's string routines. This reproduces
// the native unsigned-underflow bug without invoking payload initialization.
int selector_accepts(const std::vector<uint8_t>& image, uint32_t count, int index) {
    constexpr uint8_t prefix[] = {0x53, 0x57, 0x8B, 0x4C, 0x24, 0x0C, 0x8B, 0x7C, 0x24, 0x10};
    constexpr uint8_t accepted[] = {0xB8, 1, 0, 0, 0, 0x5F, 0x5B, 0xC2, 8, 0};
    constexpr uint8_t rejected[] = {0x33, 0xC0, 0x5F, 0x5B, 0xC2, 8, 0};
    std::array<uint8_t, sizeof(prefix) + 0x4A + sizeof(rejected)> code;
    code.fill(0x90);
    std::memcpy(code.data(), prefix, sizeof(prefix));
    std::memcpy(code.data() + sizeof(prefix), image.data() + 0x34356A, 42);
    std::memcpy(code.data() + sizeof(prefix) + 42, accepted, sizeof(accepted));
    std::memcpy(code.data() + sizeof(prefix) + 0x4A, rejected, sizeof(rejected));
    std::array<uint8_t, 0x160> object{};
    const uint32_t begin = 0x1000, end = begin + count * 28;
    std::memcpy(object.data() + 0x150, &begin, 4);
    std::memcpy(object.data() + 0x154, &end, 4);
    void* memory = VirtualAlloc(nullptr, code.size(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    require(memory != nullptr, "selector arithmetic probe allocation failed");
    std::memcpy(memory, code.data(), code.size());
    DWORD old_protection = 0;
    if (!VirtualProtect(memory, code.size(), PAGE_EXECUTE_READ, &old_protection) ||
        !FlushInstructionCache(GetCurrentProcess(), memory, code.size())) {
        VirtualFree(memory, 0, MEM_RELEASE);
        throw std::runtime_error("selector arithmetic probe protection failed");
    }
    using Decision = int(__stdcall*)(void*, int);
    const int answer = reinterpret_cast<Decision>(memory)(object.data(), index);
    VirtualFree(memory, 0, MEM_RELEASE);
    return answer;
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "dump path required");
        std::ifstream input(argv[1], std::ios::binary);
        const std::vector<uint8_t> original((std::istreambuf_iterator<char>(input)), {});
        require(original.size() == cfg::BINARY_SIZE, "wrong dump size");
        // Absolute SEH handler and constructor pointer independently agree on
        // the original base; the old base pointed at unrelated instructions.
        require(word(original, 0x3CA351) == cfg::IMAGE_BASE + 0x3CA395, "SEH handler base mismatch");
        const auto constructor = word(original, 0x40E5AC) - cfg::IMAGE_BASE;
        require(constructor == 0x6CCD8 && original[constructor] == 0x68 &&
            original[constructor + 11] == 0xC3, "constructor base mismatch");
        auto image = original;
        require(startup::prepare(image.data(), image.size()).ok, "valid CRT profile rejected");
        require(word(image, 0x826238) == 0 && word(image, 0x82623C) == 0 &&
            !image[0x826241] && word(image, 0x826268) == 0, "CRT state not reset");
        require(word(original, 0x8086E8) == 29 && word(image, 0x8086E8) == 0,
            "captured interface count not reset");
        require(word(original, 0x809564) == 11 && word(image, 0x809564) == 0,
            "captured worker count not reset");
        for (size_t i = 0x826244; i < 0x82625C; ++i) require(!image[i], "onexit table not cleared");
        require(word(image, 0x4DB634) == 0 && word(image, 0x4DB644) == 0 &&
            word(image, 0x4DB648) == 15, "captured alphabet string was not made empty");
        // Exercise this MSVC runtime's string assignment with the repaired
        // representation. The initializer can now allocate fresh storage.
        static_assert(sizeof(std::string) == 24, "verified x86 release string layout required");
        std::string alphabet;
        std::memcpy(&alphabet, image.data() + 0x4DB634, sizeof(alphabet));
        const char expected[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";
        alphabet.assign(expected, 63);
        require(alphabet == expected && alphabet.size() == 63, "repaired alphabet assignment failed");
        // Exercise genuine MSVC ownership operations, without executing dump
        // instructions: these formerly used old-process heap addresses.
        static_assert(sizeof(std::wstring) == 24);
        constexpr size_t menu_strings[] = {0x4B33B4, 0x4B3D38, 0x4B349C, 0x4B3584,
                                           0x4B366C, 0x4B3D14, 0x4B3C44, 0x4B3C74, 0x4B3C14};
        for (const size_t offset : menu_strings) {
            require(word(image, offset) == 0 && word(image, offset + 16) == 0 &&
                word(image, offset + 20) == 7, "menu wstring was not made empty");
            std::wstring title;
            std::memcpy(&title, image.data() + offset, sizeof(title));
            title.assign(L"primordial");
            require(title == L"primordial", "fresh menu title ownership failed");
        }
        using MenuItem = std::array<uint32_t, 5>;
        static_assert(sizeof(MenuItem) == 20);
        static_assert(sizeof(std::vector<MenuItem>) == 12);
        std::vector<MenuItem> menu_items;
        std::memcpy(&menu_items, image.data() + 0x4B3484, sizeof(menu_items));
        require(menu_items.empty() && menu_items.capacity() == 0, "captured menu storage survived");
        menu_items.push_back({1, 2, 3, 4, 5});
        require(menu_items[0][4] == 5, "fresh menu vector ownership failed");
        require(original[0x343592] == 0x77 && image[0x343592] == 0x7F,
            "selector empty-vector branch was not repaired");
        require(selector_accepts(original, 0, 0) == 1 && selector_accepts(image, 0, 0) == 0,
            "native selector zero-count regression not reproduced/repaired");
        for (const uint32_t count : {0u, 1u, 2u, 17u, 128u, 65536u}) {
            for (const int index : {-1, 0, int(count) - 1, int(count), int(count) + 1, INT_MAX}) {
                const int expected_acceptance = index >= 0 && uint32_t(index) < count;
                require(selector_accepts(image, count, index) == expected_acceptance,
                    "repaired native selector decision differs from actual element bounds");
                if (count)
                    require(selector_accepts(original, count, index) == expected_acceptance,
                        "selector patch changed a valid nonempty-vector decision");
            }
        }
        for (size_t i = 0; i < image.size(); ++i) {
            const bool loader_input = i >= 0x4DDCE0 && i < 0x4DDEE5;
            const bool in_bss = i >= 0x4DDBE0 && i < 0x826654;
            bool module_hash = false;
            for (const auto& binding : game_modules::bindings)
                module_hash |= i >= binding.rva && i < binding.rva + 4;
            bool menu_string = false;
            for (const size_t offset : menu_strings)
                menu_string |= i >= offset && i < offset + 24;
            if (i == 0x4DDEE4) { require(image[i] == 1, "ac-130 mode byte not enabled"); continue; }
            const bool changed = (in_bss && !loader_input && !module_hash) ||
                (i >= 0x4DB634 && i < 0x4DB64C) ||
                (i >= 0x4B3484 && i < 0x4B3490) || menu_string || i == 0x343592;
            if (in_bss && !loader_input && !module_hash) require(image[i] == 0, "captured BSS not cleared");
            if (!changed) require(image[i] == original[i], "unrelated captured state changed");
        }
        auto prepared = image;
        unsigned calls = 0;
        require(!game_modules::bind(image.data(), image.size(), missing_module, &calls).ok &&
            image == prepared && calls == 2, "missing module caused partial resolver writes");
        for(const char* name:{"panorama.dll","localize.dll"}) {
            MissingLateModule probe{0,name};
            require(!game_modules::bind(image.data(),image.size(),missing_late_module,&probe).ok &&
                image==prepared && probe.calls>=13,"late missing module caused partial resolver writes");
        }
        calls = 0;
        require(game_modules::bind(image.data(), image.size(), fake_module, &calls).ok &&
            calls == game_modules::bindings.size(), "game module binding failed");
        require(word(image,0x4DE108)==0x35153F28 && word(image,0x4DE130)==0xFC70FD90 &&
            word(image,0x4DE10C)==0x600D0000 && word(image,0x4DE134)==0x600E0000,
            "panorama/localize resolver inputs not preserved and rebound");
        for (size_t i = 0; i < game_modules::bindings.size(); ++i) {
            const auto& binding = game_modules::bindings[i];
            require(word(image, binding.rva) == binding.hash &&
                word(image, binding.rva + 4) == 0x60000000u + 0x10000u * (i + 1),
                "game module hash/handle mismatch");
            for (size_t j = 8; j < 40; ++j)
                require(!image[binding.rva + j], "captured export cache pointer survived");
        }
        calls = 0;
        auto bound = image;
        require(!game_modules::bind(image.data(), image.size(), fake_module, &calls).ok &&
            calls == 0 && image == bound, "duplicate binding changed image");
        require(!startup::prepare(image.data(), image.size()).ok, "startup reset allowed twice");
        image = original; image[0x826238] ^= 1;
        auto snapshot = image;
        require(!startup::prepare(image.data(), image.size()).ok && image == snapshot,
            "bad state partially changed the image");
        image = original; image[0x4DB628] ^= 1; snapshot = image;
        require(!startup::prepare(image.data(), image.size()).ok && image == snapshot,
            "bad cookie partially changed the image");
        image = original; image[0x4DB634] ^= 1; snapshot = image;
        require(!startup::prepare(image.data(), image.size()).ok && image == snapshot,
            "bad alphabet profile partially changed the image");
        for(size_t offset:{size_t(0x4DDF78),size_t(0x4DE108),size_t(0x4DE130)}) {
            image = original; image[offset] ^= 1; snapshot = image;
            require(!startup::prepare(image.data(), image.size()).ok && image == snapshot,
                "bad resolver hash partially changed the image");
        }
        image = original; image[0x343592] ^= 1; snapshot = image;
        require(!startup::prepare(image.data(), image.size()).ok && image == snapshot,
            "bad selector instruction profile partially changed the image");
        for (const size_t offset : {size_t(0x4B33B4), size_t(0x4B3484), size_t(0x4B3D38),
                                  size_t(0x4B349C), size_t(0x4B3584), size_t(0x4B366C),
                                  size_t(0x4B3D14), size_t(0x4B3C44), size_t(0x4B3C74), size_t(0x4B3C14)}) {
            image = original; image[offset] ^= 1; snapshot = image;
            require(!startup::prepare(image.data(), image.size()).ok && image == snapshot,
                "bad menu ownership profile partially changed the image");
        }
        require(!startup::prepare(image.data(), 0x1000).ok, "short image accepted");
        std::puts("Guarded startup and isolated native selector bounds verified; payload entry not executed.");
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
