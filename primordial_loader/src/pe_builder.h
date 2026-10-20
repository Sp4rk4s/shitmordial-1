#pragma once
#include <cstdint>
#include <vector>
#include <windows.h>

namespace pe {

// Build a minimal PE32 header for the raw dump so it can be recognized as a DLL.
// Returns the 0x1000-byte header that replaces the destroyed first page.
std::vector<uint8_t> build_header();

// Patch the auth check at AUTH_CHECK_RVA to "mov eax,1; ret" in the given image buffer.
void patch_auth(uint8_t* image, size_t size);

}
