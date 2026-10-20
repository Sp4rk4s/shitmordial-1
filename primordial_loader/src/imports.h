#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace imports {
enum class Kind { relative, absolute, keyed };
struct Patch {
    uint32_t rva;
    Kind kind;
    const char* module;
    const char* symbol;
    uint32_t original;
    uint32_t key = 0;
};
struct Result { bool ok = false; size_t count = 0; std::string error; };
using Resolver = uintptr_t (*)(const char*, const char*, void*);

// Validate every site and resolve every export before writing any payload bytes.
// The caller must provide the existing, writable mapping and original-byte guards.
Result apply(uint8_t* image, size_t size, std::span<const Patch> patches,
             Resolver resolver = nullptr, void* context = nullptr);

// The supplied 0x3CB10000-based map is retained for validation, not assumed valid.
// On the current dump this returns an opcode mismatch and leaves the image intact.
Result apply_submitted(uint8_t* image, size_t size);

// Actual dump-specific VMP table: encrypted-name metadata was decoded offline and
// each destination, key and original encoded operand is guarded at runtime.
Result apply_recovered(uint8_t* image, size_t size);
}
