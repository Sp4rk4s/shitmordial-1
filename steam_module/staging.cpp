#include "staging.h"
#include <mutex>

namespace staging {
namespace {
std::mutex stage_mutex;
bool write(HANDLE process, void* destination, const void* source, size_t size) {
    SIZE_T written = 0;
    return WriteProcessMemory(process, destination, source, size, &written) && written == size;
}
}
Result map(HANDLE process, const std::vector<uint8_t>& payload) {
    std::lock_guard lock(stage_mutex);
    if (payload.size() != cfg::BINARY_SIZE)
        return {false, false, "Raw binary size does not match config.h."};
    const DWORD pid = GetProcessId(process);
    if (!pid) return {false, false, "Cannot query target process ID."};
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQueryEx(process, mapping::base(), &region, sizeof(region)))
        return {false, false, "Cannot query fixed-base address."};
    if (region.State != MEM_FREE) {
        mapping::Handoff h{};
        SIZE_T read = 0;
        MEMORY_BASIC_INFORMATION metadata{};
        if (region.AllocationBase == mapping::base() && region.Type == MEM_PRIVATE &&
            VirtualQueryEx(process, mapping::handoff(), &metadata, sizeof(metadata)) &&
            metadata.AllocationBase == mapping::base() && metadata.State == MEM_COMMIT &&
            ReadProcessMemory(process, mapping::handoff(), &h, sizeof(h), &read) &&
            read == sizeof(h) && mapping::valid(h, pid) &&
            h.state >= mapping::staged && h.state <= mapping::failed &&
            h.checksum == mapping::checksum(payload.data(), payload.size())) {
            if (h.state == mapping::failed)
                return {false, true, "Primordial initialization previously failed; restart this game process."};
            return {true, true, {}};
        }
        return {false, false, "0x44340000 is occupied by an allocation we do not own."};
    }
    void* memory = VirtualAllocEx(process, mapping::base(), mapping::allocation_size,
                                  MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!memory)
        return {false, false, "Fixed-base allocation failed (" + std::to_string(GetLastError()) + ")."};
    const auto fail = [&](const char* operation) -> Result {
        const DWORD error = GetLastError();
        VirtualFreeEx(process, memory, 0, MEM_RELEASE);
        return {false, false, std::string(operation) + " (" + std::to_string(error) + ")."};
    };
    if (memory != mapping::base()) return fail("Allocation returned the wrong base");
    if (!write(process, memory, payload.data(), payload.size())) return fail("Raw binary copy failed");
    // Publish only after the raw image is available, beyond the CRT padding page.
    mapping::Handoff h{mapping::magic, mapping::version, pid, cfg::IMAGE_BASE,
        cfg::BINARY_SIZE, cfg::IMAGE_SIZE, mapping::staged, 0,
        mapping::checksum(payload.data(), payload.size())};
    if (!write(process, mapping::handoff(), &h, sizeof(h))) return fail("Handoff publication failed");
    return {true, false, {}};
}
}
