#pragma once
#include "../shared/mapping.h"
#include <string>
#include <vector>

namespace staging {
struct Result {
    bool ok = false;
    bool existing = false;
    std::string error;
};
// Copy the raw dump only. All payload modifications belong to Primordial.
Result map(HANDLE process, const std::vector<uint8_t>& payload);
}
