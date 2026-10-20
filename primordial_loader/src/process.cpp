#include "process.h"
#include "config.h"
#include <tlhelp32.h>
#include <algorithm>

namespace proc {

uint32_t find_pid(const std::wstring& name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    uint32_t pid = 0;

    if (Process32FirstW(snap, &pe)) {
        do {
            std::wstring exeName(pe.szExeFile);
            // case-insensitive compare
            std::transform(exeName.begin(), exeName.end(), exeName.begin(), ::towlower);
            std::wstring target(name);
            std::transform(target.begin(), target.end(), target.begin(), ::towlower);
            if (exeName == target) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

Handle open(uint32_t pid) {
    return Handle(OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid));
}

void* alloc_at(HANDLE proc, uintptr_t addr, size_t size, DWORD protect) {
    return VirtualAllocEx(proc, reinterpret_cast<void*>(addr),
                          size, MEM_COMMIT | MEM_RESERVE, protect);
}

bool write(HANDLE proc, uintptr_t addr, const void* data, size_t size) {
    SIZE_T written = 0;
    return WriteProcessMemory(proc, reinterpret_cast<void*>(addr),
                              data, size, &written) && written == size;
}

void protect_sections(HANDLE proc, uintptr_t base) {
    DWORD old;
    // Header page: RW
    VirtualProtectEx(proc, reinterpret_cast<void*>(base), 0x1000, PAGE_READWRITE, &old);

    for (int i = 0; i < cfg::NUM_SECTIONS; ++i) {
        DWORD prot = PAGE_READWRITE;
        uint32_t c = cfg::SECTIONS[i].characteristics;
        bool x = (c & IMAGE_SCN_MEM_EXECUTE) != 0;
        bool r = (c & IMAGE_SCN_MEM_READ) != 0;
        bool w = (c & IMAGE_SCN_MEM_WRITE) != 0;

        if (x && w)      prot = PAGE_EXECUTE_READWRITE;
        else if (x && r)  prot = PAGE_EXECUTE_READ;
        else if (x)       prot = PAGE_EXECUTE;
        else if (r && w)  prot = PAGE_READWRITE;
        else if (r)       prot = PAGE_READONLY;

        VirtualProtectEx(proc, reinterpret_cast<void*>(base + cfg::SECTIONS[i].rva),
                         cfg::SECTIONS[i].vsize, prot, &old);
    }
}

bool wait_for_module(uint32_t pid, const char* mod_name, DWORD timeout_ms) {
    DWORD start = GetTickCount();
    for (;;) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap != INVALID_HANDLE_VALUE) {
            MODULEENTRY32 me{};
            me.dwSize = sizeof(me);
            if (Module32First(snap, &me)) {
                do {
                    if (_stricmp(me.szModule, mod_name) == 0) {
                        CloseHandle(snap);
                        return true;
                    }
                } while (Module32Next(snap, &me));
            }
            CloseHandle(snap);
        }
        if (GetTickCount() - start >= timeout_ms)
            return false;
        Sleep(500);
    }
}

HANDLE run_shellcode(HANDLE proc, const void* sc, size_t sc_size) {
    void* remote = VirtualAllocEx(proc, nullptr, sc_size,
                                  MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remote) return nullptr;

    SIZE_T written;
    WriteProcessMemory(proc, remote, sc, sc_size, &written);

    DWORD tid = 0;
    return CreateRemoteThread(proc, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(remote), nullptr, 0, &tid);
}

}
