#pragma once
#include "../primordial_loader/src/config.h"
#include <windows.h>
#include <cstddef>
#include <cstdint>

// Shared wire layout: identical in the Steam and x86 game processes.
namespace mapping {
inline constexpr uint32_t magic = 0x504D4150; // PMAP
inline constexpr uint32_t version = 2; // corrected original image base/startup profile
inline constexpr uint32_t metadata_rva = cfg::IMAGE_SIZE;
inline constexpr uint32_t allocation_size = metadata_rva + 0x1000;
enum State : LONG { staged = 1, patching, initialized, failed };

struct alignas(8) Handoff {
    uint32_t signature;
    uint32_t protocol;
    uint32_t process_id;
    uint32_t image_base;
    uint32_t binary_size;
    uint32_t image_size;
    volatile LONG state;
    uint32_t reserved;
    uint64_t checksum;
};
static_assert(sizeof(Handoff) == 40);
static_assert(offsetof(Handoff, checksum) == 32);
static_assert(cfg::BINARY_SIZE <= cfg::IMAGE_SIZE);

inline uint64_t checksum(const uint8_t* data, size_t size) {
    uint64_t value = 14695981039346656037ull;
    for (size_t i = 0; i < size; ++i) {
        value ^= data[i];
        value *= 1099511628211ull;
    }
    return value;
}
inline bool valid(const Handoff& h, DWORD pid) {
    return h.signature == magic && h.protocol == version &&
        h.process_id == pid && h.image_base == cfg::IMAGE_BASE &&
        h.binary_size == cfg::BINARY_SIZE && h.image_size == cfg::IMAGE_SIZE;
}
inline void* base() {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(cfg::IMAGE_BASE));
}
inline Handoff* handoff() {
    return reinterpret_cast<Handoff*>(static_cast<uintptr_t>(cfg::IMAGE_BASE) + metadata_rva);
}
}
