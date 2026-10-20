#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

namespace crash {

// Observe exceptions without consuming them. Recognized debug notifications
// are omitted; an observation alone does not establish an unhandled crash.
void install();

// Remove the handler.
void remove();

// Path to the crash log file (set after install()).
const std::wstring& log_path();

}
