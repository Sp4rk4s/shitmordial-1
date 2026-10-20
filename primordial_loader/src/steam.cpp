#include "steam.h"
#include "config.h"
#include <tlhelp32.h>
#include <cstdio>

namespace steam {

// Find a process by name, return PID or 0
static uint32_t find_pid(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    uint32_t pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

bool inject_module(const std::string& dll_path) {
    uint32_t pid = find_pid(L"steam.exe");
    if (pid == 0) {
        printf("[*] steam.exe not found — skipping steam module injection.\n");
        printf("    (If CS:GO is already running, the loader will reserve the address directly.)\n");
        return false;
    }
    printf("[*] Found steam.exe (PID %u), injecting steam_module.dll...\n", pid);

    HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!proc) {
        printf("    [-] OpenProcess on steam.exe failed (error %u). Run as admin.\n", GetLastError());
        return false;
    }

    // Allocate memory in steam.exe for the DLL path
    size_t path_size = dll_path.size() + 1;
    void* remote_path = VirtualAllocEx(proc, nullptr, path_size,
                                        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_path) {
        printf("    [-] VirtualAllocEx in steam.exe failed\n");
        CloseHandle(proc);
        return false;
    }

    SIZE_T written;
    WriteProcessMemory(proc, remote_path, dll_path.c_str(), path_size, &written);

    // CreateRemoteThread with LoadLibraryA
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    auto load_lib = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(k32, "LoadLibraryA"));

    HANDLE thread = CreateRemoteThread(proc, nullptr, 0, load_lib, remote_path, 0, nullptr);
    if (!thread) {
        printf("    [-] CreateRemoteThread in steam.exe failed (error %u)\n", GetLastError());
        VirtualFreeEx(proc, remote_path, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    WaitForSingleObject(thread, 10000);

    // Check if it loaded
    DWORD exit_code = 0;
    GetExitCodeThread(thread, &exit_code);
    CloseHandle(thread);
    VirtualFreeEx(proc, remote_path, 0, MEM_RELEASE);
    CloseHandle(proc);

    if (exit_code == 0) {
        printf("    [-] LoadLibraryA returned NULL — DLL failed to load in steam.exe\n");
        return false;
    }

    printf("    [+] steam_module.dll injected into steam.exe (base=0x%X)\n", exit_code);
    return true;
}

uint32_t wait_for_game(DWORD timeout_ms) {
    printf("[*] Waiting for csgo.exe...\n");
    DWORD start = GetTickCount();
    for (;;) {
        uint32_t pid = find_pid(L"csgo.exe");
        if (pid) {
            printf("    [+] csgo.exe found (PID %u)\n", pid);
            return pid;
        }
        if (GetTickCount() - start >= timeout_ms)
            return 0;
        Sleep(500);
    }
}

HANDLE reserve_early(uint32_t pid) {
    HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!proc) {
        printf("    [-] OpenProcess failed (error %u). Run as admin.\n", GetLastError());
        return nullptr;
    }

    // Check if already reserved (by our steam module hook)
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQueryEx(proc, reinterpret_cast<void*>(cfg::IMAGE_BASE), &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT &&
            mbi.AllocationBase == reinterpret_cast<void*>(cfg::IMAGE_BASE)) {
            printf("    [+] 0x%X already reserved (steam module did it)\n", cfg::IMAGE_BASE);
            return proc;
        }
        if (mbi.State != MEM_FREE) {
            printf("    [-] 0x%X occupied by something else (state=0x%X, size=0x%llX)\n",
                   cfg::IMAGE_BASE, mbi.State, static_cast<unsigned long long>(mbi.RegionSize));
            printf("    [-] Restart CS:GO for a clean address space.\n");
            CloseHandle(proc);
            return nullptr;
        }
    }

    // Reserve it ourselves (fallback if steam module wasn't injected)
    void* alloc = VirtualAllocEx(proc, reinterpret_cast<void*>(cfg::IMAGE_BASE),
                                  cfg::BINARY_SIZE, MEM_RESERVE | MEM_COMMIT,
                                  PAGE_EXECUTE_READWRITE);
    if (!alloc) {
        printf("    [-] VirtualAllocEx failed (error %u)\n", GetLastError());
        CloseHandle(proc);
        return nullptr;
    }

    if (reinterpret_cast<uintptr_t>(alloc) != cfg::IMAGE_BASE) {
        printf("    [-] Got 0x%p instead of 0x%X\n", alloc, cfg::IMAGE_BASE);
        VirtualFreeEx(proc, alloc, 0, MEM_RELEASE);
        CloseHandle(proc);
        return nullptr;
    }

    printf("    [+] Reserved 0x%X (fallback — no steam module)\n", cfg::IMAGE_BASE);
    return proc;
}

int probe_address(HANDLE proc) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQueryEx(proc, reinterpret_cast<void*>(cfg::IMAGE_BASE), &mbi, sizeof(mbi)))
        return 0;
    if (mbi.State == MEM_FREE)
        return 0;
    if (mbi.State == MEM_COMMIT && mbi.AllocationBase == reinterpret_cast<void*>(cfg::IMAGE_BASE))
        return 1;
    return -1;
}

}
