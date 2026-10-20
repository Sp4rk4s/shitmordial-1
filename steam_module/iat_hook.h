#pragma once
#include <windows.h>
#include <cstddef>
namespace iat {
// Repeat to cover newly loaded modules. Resolve originals before publishing hooks.
size_t hook(const char* function_name, void* original, void* detour);
}
