// Same-architecture LoadLibraryA helper. Never maps or patches a payload image.
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <vector>

struct Handle {
    HANDLE value;
    explicit Handle(HANDLE h) : value(h) {}
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle&) = delete;
};
[[noreturn]] void fail(const char* operation) {
    throw std::runtime_error(std::string(operation) + ": Win32 error " + std::to_string(GetLastError()));
}
std::wstring full_path(const wchar_t* input) {
    const DWORD count = GetFullPathNameW(input, 0, nullptr, nullptr);
    if (!count) fail("GetFullPathNameW");
    std::vector<wchar_t> buffer(count);
    if (!GetFullPathNameW(input, count, buffer.data(), nullptr)) fail("GetFullPathNameW");
    return buffer.data();
}
uintptr_t module(DWORD pid, const std::wstring& name, bool exact_path) {
    HANDLE raw = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 10; ++attempt) {
        raw = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (raw != INVALID_HANDLE_VALUE || GetLastError() != ERROR_BAD_LENGTH) break;
        Sleep(10);
    }
    Handle snapshot(raw);
    if (raw == INVALID_HANDLE_VALUE) fail("Module snapshot");
    MODULEENTRY32W item{sizeof(item)};
    if (!Module32FirstW(raw, &item)) fail("Module32FirstW");
    do {
        if (!_wcsicmp(exact_path ? item.szExePath : item.szModule, name.c_str()))
            return reinterpret_cast<uintptr_t>(item.modBaseAddr);
    } while (Module32NextW(raw, &item));
    return 0;
}
int wmain(int argc, wchar_t** argv) {
    try {
        if (argc != 3 && argc != 4) {
            std::fputs("Usage: loadlibrary_injector.exe <pid> <absolute-dll-path> [wait-ms=15000]\n", stderr);
            return 2;
        }
        wchar_t* end = nullptr;
        const auto parsed = wcstoull(argv[1], &end, 10);
        if (!parsed || *end || parsed > MAXDWORD) throw std::runtime_error("Invalid PID");
        const DWORD pid = static_cast<DWORD>(parsed);
        DWORD wait_ms = 15000;
        if (argc == 4) {
            const auto wait = wcstoull(argv[3], &end, 10);
            if (*end || !wait || wait >= INFINITE) throw std::runtime_error("Invalid wait timeout");
            wait_ms = static_cast<DWORD>(wait);
        }
        const auto path = full_path(argv[2]);
        const DWORD attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
            throw std::runtime_error("DLL file does not exist");
        // Inspect PE headers without loading or executing the supplied DLL here.
        Handle target(OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
            PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | SYNCHRONIZE, FALSE, pid));
        if (!target.value) fail("OpenProcess");
        USHORT target_machine = 0, native_machine = 0, self_machine = 0, self_native = 0;
        if (!IsWow64Process2(target.value, &target_machine, &native_machine) ||
            !IsWow64Process2(GetCurrentProcess(), &self_machine, &self_native)) fail("IsWow64Process2");
        if ((target_machine ? target_machine : native_machine) !=
            (self_machine ? self_machine : self_native))
            throw std::runtime_error("Architecture mismatch: use the matching x86/x64 injector");
        Handle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
        if (file.value == INVALID_HANDLE_VALUE) fail("Open DLL");
        IMAGE_DOS_HEADER dos{}; DWORD got = 0;
        if (!ReadFile(file.value, &dos, sizeof(dos), &got, nullptr) || got != sizeof(dos) ||
            dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0) throw std::runtime_error("Invalid DLL DOS header");
        LARGE_INTEGER offset{}; offset.QuadPart = dos.e_lfanew;
        if (!SetFilePointerEx(file.value, offset, nullptr, FILE_BEGIN)) fail("Seek PE header");
        struct Pe { DWORD signature; IMAGE_FILE_HEADER coff; } pe{};
        if (!ReadFile(file.value, &pe, sizeof(pe), &got, nullptr) || got != sizeof(pe) ||
            pe.signature != IMAGE_NT_SIGNATURE || !(pe.coff.Characteristics & IMAGE_FILE_DLL) ||
            pe.coff.Machine != (self_machine ? self_machine : self_native))
            throw std::runtime_error("Input is not a matching-architecture PE DLL");
        if (const auto loaded = module(pid, path, true)) {
            std::printf("Already loaded: PID %lu base 0x%llX\n", pid, static_cast<unsigned long long>(loaded));
            return 0;
        }
        // LoadLibraryA is commonly forwarded into KernelBase. Locate its actual
        // owning local module, then translate its RVA to that target module.
        auto load = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryA");
        HMODULE owner = nullptr;
        if (!load || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(load), &owner))
            fail("Locate LoadLibraryA owner");
        wchar_t owner_path[32768];
        if (!GetModuleFileNameW(owner, owner_path, ARRAYSIZE(owner_path))) fail("Owner path");
        const wchar_t* slash = wcsrchr(owner_path, L'\\');
        const auto remote_owner = module(pid, slash ? slash + 1 : owner_path, false);
        if (!remote_owner) throw std::runtime_error("LoadLibraryA owner is absent in target");
        const auto remote_load = remote_owner + reinterpret_cast<uintptr_t>(load) - reinterpret_cast<uintptr_t>(owner);
        BOOL substituted = FALSE;
        const int length = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, path.c_str(), -1,
            nullptr, 0, nullptr, &substituted);
        if (!length || substituted) throw std::runtime_error("DLL path cannot be represented by LoadLibraryA");
        std::string ansi(length, '\0');
        if (!WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, path.c_str(), -1,
            ansi.data(), length, nullptr, &substituted) || substituted)
            throw std::runtime_error("DLL path conversion failed");
        void* remote = VirtualAllocEx(target.value, nullptr, ansi.size(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!remote) fail("Allocate DLL path");
        SIZE_T written = 0;
        if (!WriteProcessMemory(target.value, remote, ansi.data(), ansi.size(), &written) || written != ansi.size()) {
            const DWORD error = GetLastError(); VirtualFreeEx(target.value, remote, 0, MEM_RELEASE);
            SetLastError(error); fail("Write DLL path");
        }
        Handle thread(CreateRemoteThread(target.value, nullptr, 0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(remote_load), remote, 0, nullptr));
        if (!thread.value) {
            const DWORD error = GetLastError(); VirtualFreeEx(target.value, remote, 0, MEM_RELEASE);
            SetLastError(error); fail("Create LoadLibraryA thread");
        }
        const DWORD waited = WaitForSingleObject(thread.value, wait_ms);
        if (waited != WAIT_OBJECT_0) {
            // A debugger may have paused the thread. Never free its argument or
            // terminate it while it could still be executing LoadLibraryA.
            std::fprintf(stderr, "LoadLibraryA is pending (wait=%lu); retained path at 0x%llX. Resume debugger and inspect modules; do not retry blindly.\n",
                waited, static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(remote)));
            return 3;
        }
        DWORD result = 0;
        if (!GetExitCodeThread(thread.value, &result)) fail("GetExitCodeThread");
        VirtualFreeEx(target.value, remote, 0, MEM_RELEASE);
        const auto loaded = module(pid, path, true);
        // x64 HMODULE is wider than a thread exit code; module enumeration is authoritative.
        if (!loaded) throw std::runtime_error("DLL did not remain loaded; thread result " + std::to_string(result));
        std::printf("Loaded: PID %lu base 0x%llX\n", pid, static_cast<unsigned long long>(loaded));
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "ERROR: %s\n", e.what());
        return 1;
    }
}
