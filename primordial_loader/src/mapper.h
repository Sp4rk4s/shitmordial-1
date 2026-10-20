#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

namespace mapper {

struct Result {
    bool ok = false;
    std::string error;
    uintptr_t base = 0;
};

// Patch only the existing Steam-staged image, then invoke its DLL entry on a new
// thread and wait for its result. Never load/remap the binary; repeats are no-ops.
Result inject();

}
