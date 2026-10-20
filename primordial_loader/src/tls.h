#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
namespace payload_tls {
struct Result { bool ok; uint32_t index; std::string error; };
Result initialize(uint8_t* image, size_t size);
}
