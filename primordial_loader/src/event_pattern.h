#pragma once
#include <cstdint>
#include <cstring>

namespace event_pattern {
// Original 18-byte compiled signature, not the ASCII pattern_scan interface.
struct Pattern { std::uint8_t bytes[18], padding[2]; std::uint32_t mask; };
static_assert(sizeof(Pattern)==24);
inline constexpr std::uint8_t old_bytes[]={0x55,0x8B,0xEC,0x83,0xE4,0xF8,0x83,0xEC,0x0C,0x53,0x8B,0xD9,0x56,0x57,0x89,0x5C,0x24,0x0C};
inline constexpr std::uint8_t new_bytes[]={0x55,0x8B,0xEC,0x83,0xE4,0xF8,0x83,0xEC,0x0C,0x8B,0xC1,0x53,0x56,0x57,0x8D,0xB0,0x98,0x00};
// Only this exact old signature, zero mask and a wholly contained engine code
// range qualify. All other patterns/inputs keep the original scanner behavior.
inline bool adapt(const Pattern& input, std::uintptr_t start, std::uint32_t length,
                  std::uintptr_t lower, std::uintptr_t upper, Pattern& output) noexcept {
    if(input.mask || std::memcmp(input.bytes,old_bytes,18) ||
       start<lower || start>=upper || !length || length>upper-start) return false;
    output=input;
    std::memcpy(output.bytes,new_bytes,18);
    return true;
}
// Returns false on profile mismatch. Does not run any gameplay initializer,
// install game-event hooks, register listeners, or claim feature readiness.
bool install() noexcept;
}
extern "C" __declspec(dllexport) volatile long primordial_event_pattern_replacements;
