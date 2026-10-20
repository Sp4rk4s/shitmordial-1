#pragma once
#include <cstdint>
#include <string>
#include <windows.h>

namespace steam {

// Inject steam_module.dll into steam.exe so it hooks CreateProcessW.
// dll_path is the full path to the built steam_module.dll.
// Returns true if injection succeeded (or steam.exe wasn't found — non-fatal).
bool inject_module(const std::string& dll_path);

// Wait for csgo.exe to appear (polls every 500ms). Returns PID, 0 on timeout.
uint32_t wait_for_game(DWORD timeout_ms = 60000);

// Reserve IMAGE_BASE in the target before other modules claim it.
// Call as early as possible after the process is found.
// Returns the HANDLE to the target (caller owns it), or null on failure.
// On success, 0x44000000 is reserved+committed and ready for writing.
HANDLE reserve_early(uint32_t pid);

// Query the state of IMAGE_BASE in the target.
// Returns: 0 = free, 1 = reserved by us (MEM_COMMIT), -1 = occupied by something else.
int probe_address(HANDLE proc);

}
