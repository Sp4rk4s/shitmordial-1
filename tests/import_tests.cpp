#include "../primordial_loader/src/imports.h"
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
uint32_t read(const uint8_t* image, size_t rva) {
    uint32_t value;
    std::memcpy(&value, image + rva, 4);
    return value;
}
uintptr_t fake_resolve(const char*, const char*, void* context) {
    auto* calls = static_cast<unsigned*>(context);
    ++*calls;
    return 0xF00DCAFEu;
}
uintptr_t fail_second(const char*, const char*, void* context) {
    auto* calls = static_cast<unsigned*>(context);
    return ++*calls == 1 ? 0xF00DCAFEu : 0;
}
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "path to original dump required");
        auto* image = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
        require(image != nullptr, "fixture allocation failed");
        image[0] = 0xE8;
        image[5] = 0xC3;
        const std::array<imports::Patch, 2> patches{{
            {1, imports::Kind::relative, "kernel32.dll", "GetTickCount", 0},
            {16, imports::Kind::absolute, "kernel32.dll", "GetCurrentProcessId", 0}
        }};
        std::vector<uint8_t> snapshot(image, image + 4096);
        unsigned calls = 0;
        auto result = imports::apply(image, 4096, patches, fake_resolve, &calls);
        require(result.ok && result.count == 2 && calls == 2, "fake import mapping failed");
        const auto next = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(image + 5));
        require(read(image, 1) + next == 0xF00DCAFEu, "rel32 did not use the runtime operand address plus four");
        require(read(image, 16) == 0xF00DCAFEu, "absolute export pointer incorrect");

        std::memcpy(image, snapshot.data(), snapshot.size());
        const std::array<imports::Patch, 1> keyed{{
            {16, imports::Kind::keyed, "kernel32.dll", "GetTickCount", 0, 0x13579BDFu}
        }};
        calls = 0;
        result = imports::apply(image, 4096, keyed, fake_resolve, &calls);
        require(result.ok && read(image, 16) + 0x13579BDFu == 0xF00DCAFEu,
                "VMP keyed import did not subtract its per-record key");

        std::memcpy(image, snapshot.data(), snapshot.size());
        calls = 0;
        result = imports::apply(image, 4096, patches, fail_second, &calls);
        require(!result.ok && calls == 2 && std::memcmp(image, snapshot.data(), 4096) == 0,
                "failed export lookup partially patched the image");

        auto invalid = patches;
        invalid[1].original = 1;
        calls = 0;
        require(!imports::apply(image, 4096, invalid, fake_resolve, &calls).ok && calls == 0 &&
            std::memcmp(image, snapshot.data(), 4096) == 0, "original-byte mismatch was not checked before export resolution");
        invalid = patches;
        invalid[0].rva = 4094;
        require(!imports::apply(image, 4096, invalid).ok, "out-of-range write accepted");
        invalid = patches;
        invalid[1].rva = 2;
        invalid[1].original = 0;
        require(!imports::apply(image, 4096, invalid).ok, "overlapping writes accepted");
        image[0] = 0x90;
        require(!imports::apply(image, 4096, patches).ok, "non-call operand accepted as rel32");
        std::memcpy(image, snapshot.data(), snapshot.size());

        // Resolve actual x86 exports and execute a corrected E8 call in a synthetic
        // stub; no real payload code is called.
        result = imports::apply(image, 4096, patches);
        require(result.ok && result.count == 2, result.error.c_str());
        require(FlushInstructionCache(GetCurrentProcess(), image, 4096) != FALSE, "cache flush failed");
        using Tick = DWORD (__cdecl*)();
        const DWORD before = GetTickCount();
        const DWORD tick = reinterpret_cast<Tick>(image)();
        require(tick - before < 1000, "native relative export call failed");
        using Pid = DWORD (WINAPI*)();
        require(reinterpret_cast<Pid>(read(image, 16))() == GetCurrentProcessId(), "native absolute export call failed");
        VirtualFree(image, 0, MEM_RELEASE);

        // Validate the submitted profile against the exact raw artifact, read as
        // data only. The known incompatible profile must not change any bytes.
        std::ifstream file(argv[1], std::ios::binary);
        require(file.good(), "cannot read dump for the profile audit");
        std::vector<uint8_t> dump{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        const auto original = dump;
        result = imports::apply_submitted(dump.data(), dump.size());
        require(!result.ok && result.error.find("expected an E8") != std::string::npos,
                "incompatible submitted profile was accepted");
        require(dump == original && result.count == 0, "profile rejection changed the dump");
        std::printf("Expected submitted-profile rejection: %s\n", result.error.c_str());

        result = imports::apply_recovered(dump.data(), dump.size());
        require(result.ok && result.count == 339, result.error.c_str());
        const auto tick_export = reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetTickCount"));
        require(read(dump.data(), 0xE8DC34) + 0x50F35ABAu == tick_export,
                "recovered GetTickCount encoded slot is incorrect");
        const auto gpa_export = reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetProcAddress"));
        require(read(dump.data(), 0x40E00C) == gpa_export, "recovered unkeyed IAT slot is incorrect");
        require(std::memcmp(dump.data() + 0x10AD0CF, original.data() + 0x10AD0CF, 0x1074) == 0,
                "import metadata was overwritten");
        std::printf("PASS: native resolution of 339 recovered imports, VMP keyed values, guarded rel32/absolute writes and submitted-map rejection\n");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
