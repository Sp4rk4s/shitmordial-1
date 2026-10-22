#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
namespace startup {
struct Result { bool ok; std::string error; };
// Valve item-definition-index constants (WEAPON_DEAGLE=1 .. WEAPON_BUMPMINE=85, gloves 5028..5035)
// registered by name in 19C730. They live in BSS and nothing unprotected rewrites them, so
// keep the captured values through the BSS clear. Keyed by the weapon/knife category map (FBE50).
struct ConstantRun { uint32_t rva, count; };
inline constexpr ConstantRun item_definition_constants[] = {
    {0x5840DC, 5}, {0x7DBC5C, 55}, {0x7DBD40, 8}, {0x7DBF40, 1}, {0x7FC4B4, 26}};
// Only for a fresh, checksum-verified copy of this exact dump, before entry.
Result prepare(uint8_t* image, size_t size);
}
