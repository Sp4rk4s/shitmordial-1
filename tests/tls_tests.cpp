#include "../primordial_loader/src/tls.h"
#include <windows.h>
#include <intrin.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
uint32_t* block(uint32_t index) {
    auto** slots = reinterpret_cast<void**>(__readfsdword(0x2C));
    return slots ? static_cast<uint32_t*>(slots[index]) : nullptr;
}
struct Probe { HANDLE ready; HANDLE run; uint32_t index; uint32_t value; };
DWORD WINAPI probe(void* argument) {
    auto& p = *static_cast<Probe*>(argument);
    SetEvent(p.ready);
    if (WaitForSingleObject(p.run, 15000) != WAIT_OBJECT_0) return 1;
    auto* data = block(p.index);
    if (!data || data[0] || data[1]) return 2;
    data[1] = p.value;
    return data[1] == p.value ? 0 : 3;
}
void finish(HANDLE thread) {
    require(WaitForSingleObject(thread, 15000) == WAIT_OBJECT_0, "TLS probe timed out");
    DWORD result = 0;
    require(GetExitCodeThread(thread, &result) && result == 0, "TLS isolation probe failed");
    CloseHandle(thread);
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "dump path required");
        std::ifstream input(argv[1], std::ios::binary);
        std::vector<uint8_t> image((std::istreambuf_iterator<char>(input)), {});
        require(image.size() > 0x826268, "dump unavailable");
        auto bad = image;
        bad[0x461100] ^= 1;
        const auto snapshot = bad;
        require(!payload_tls::initialize(bad.data(), bad.size()).ok && bad == snapshot,
            "TLS profile rejection partially wrote the image");
        require(!payload_tls::initialize(image.data(), 4096).ok, "short TLS image accepted");
        Probe existing{CreateEventW(nullptr, TRUE, FALSE, nullptr),
            CreateEventW(nullptr, TRUE, FALSE, nullptr), 0, 0x5678};
        HANDLE old_thread = CreateThread(nullptr, 0, probe, &existing, 0, nullptr);
        require(old_thread && WaitForSingleObject(existing.ready, 15000) == WAIT_OBJECT_0,
            "cannot create pre-existing thread");
        const auto result = payload_tls::initialize(image.data(), image.size());
        require(result.ok, result.error.c_str());
        uint32_t assigned = 0;
        std::memcpy(&assigned, image.data() + 0x826264, 4);
        require(assigned == result.index, "payload TLS index not bound");
        auto* data = block(result.index);
        require(data && !data[0] && !data[1], "calling thread TLS template invalid");
        data[1] = 0x1234;
        existing.index = result.index;
        SetEvent(existing.run);
        finish(old_thread);
        require(data[1] == 0x1234, "existing thread shared caller TLS");
        Probe future{CreateEventW(nullptr, TRUE, FALSE, nullptr),
            CreateEventW(nullptr, TRUE, TRUE, nullptr), result.index, 0x9ABC};
        HANDLE new_thread = CreateThread(nullptr, 0, probe, &future, 0, nullptr);
        require(new_thread != nullptr, "cannot create future thread");
        finish(new_thread);
        require(data[1] == 0x1234, "new thread shared caller TLS");
        for (HANDLE event : {existing.ready, existing.run, future.ready, future.run}) CloseHandle(event);
        std::puts("Static TLS initialized independently for current, existing and future threads.");
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
