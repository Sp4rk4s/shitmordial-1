#pragma once
#include <cstdint>
#include <string>
#include <windows.h>

namespace proc {

struct Handle {
    HANDLE h = nullptr;
    Handle() = default;
    explicit Handle(HANDLE h) : h(h) {}
    ~Handle() { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& o) noexcept : h(o.h) { o.h = nullptr; }
    operator HANDLE() const { return h; }
    explicit operator bool() const { return h && h != INVALID_HANDLE_VALUE; }
};

// Find PID by process name (e.g. L"csgo.exe")
uint32_t find_pid(const std::wstring& name);

// Open process with full access
Handle open(uint32_t pid);

// Allocate memory at a specific address in the target
void* alloc_at(HANDLE proc, uintptr_t addr, size_t size, DWORD protect = PAGE_EXECUTE_READWRITE);

// Write memory to target
bool write(HANDLE proc, uintptr_t addr, const void* data, size_t size);

// Set memory protection per section
void protect_sections(HANDLE proc, uintptr_t base);

// Execute shellcode in remote process
HANDLE run_shellcode(HANDLE proc, const void* sc, size_t sc_size);

// Wait for a module to appear in the target process (timeout in ms, 0 = no wait).
// Returns true if found.
bool wait_for_module(uint32_t pid, const char* mod_name, DWORD timeout_ms = 30000);

}
