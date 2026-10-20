#pragma once
#include <cstdint>
#include <string>

namespace pattern_hook {
struct Result { bool ok = false; std::string error; };

// Install a logging/forwarding detour on the verified scanner in the existing
// mapping. This never loads a payload or changes scan arguments/results.
Result install();
// For quiescent test/maintenance use: callers must not be executing the scanner.
Result remove();
uint32_t calls();
}
