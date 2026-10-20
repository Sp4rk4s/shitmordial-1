#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
namespace startup {
struct Result { bool ok; std::string error; };
// Only for a fresh, checksum-verified copy of this exact dump, before entry.
Result prepare(uint8_t* image, size_t size);
}
