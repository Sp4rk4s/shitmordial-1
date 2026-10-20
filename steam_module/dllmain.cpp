#include "staging.h"
#include "iat_hook.h"
#include <tlhelp32.h>
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>

namespace {
std::vector<uint8_t> payload;
decltype(&CreateProcessW) original_create_w = nullptr;
decltype(&CreateProcessA) original_create_a = nullptr;

void log(const std::string& message) {
    const std::string line = "[steam_module] " + message + "\n";
    OutputDebugStringA(line.c_str());
    std::fputs(line.c_str(), stdout);
}

template<class Char> bool game_command(const Char* application, const Char* command) {
    const bool has_application = application && *application;
    const Char* value = has_application ? application : command;
    if (!value) return false;
    while (*value == Char(' ') || *value == Char('\t')) ++value;
    const bool quoted = *value == Char('"');
    if (quoted) ++value;
    const Char* begin = value;
    while (*value) {
        if (quoted && *value == Char('"')) break;
        if (!quoted && !has_application && (*value == Char(' ') || *value == Char('\t'))) break;
        ++value;
    }
    std::basic_string<Char> path(begin, value);
    const auto slash = path.find_last_of(std::basic_string<Char>{Char('\\'), Char('/')});
    if (slash != path.npos) path.erase(0, slash + 1);
    std::string name;
    for (const Char c : path) {
        if (c < 0 || c > 127) return false;
        name.push_back(static_cast<char>(c >= Char('A') && c <= Char('Z') ? c + Char('a' - 'A') : c));
    }
    return name == "csgo.exe" || name == "csgo";
}

bool is_game(HANDLE process) {
    wchar_t path[32768];
    DWORD length = ARRAYSIZE(path);
    if (!QueryFullProcessImageNameW(process, 0, path, &length)) return false;
    const wchar_t* name = std::wcsrchr(path, L'\\');
    return _wcsicmp(name ? name + 1 : path, L"csgo.exe") == 0;
}

void stage(HANDLE process) {
    if (!is_game(process)) return;
    USHORT machine = 0, native = 0;
    if (!IsWow64Process2(process, &machine, &native) ||
        (machine != IMAGE_FILE_MACHINE_I386 &&
         !(machine == IMAGE_FILE_MACHINE_UNKNOWN && native == IMAGE_FILE_MACHINE_I386))) {
        log("Rejected a non-x86 csgo.exe (PID " + std::to_string(GetProcessId(process)) + ").");
        return;
    }
    const auto result = staging::map(process, payload);
    if (!result.ok) log("PID " + std::to_string(GetProcessId(process)) + ": " + result.error);
    else if (!result.existing)
        log("Staged raw binary at 0x44340000 in PID " + std::to_string(GetProcessId(process)) +
            ". Inject primordial_loader.dll when the game is ready.");
}

void finish_launch(PROCESS_INFORMATION* process, DWORD original_flags) noexcept {
    // Staging errors must not leave a normally launched game suspended.
    try { stage(process->hProcess); }
    catch (...) { OutputDebugStringA("[steam_module] Staging failed; continuing game launch.\n"); }
    if (!(original_flags & CREATE_SUSPENDED) && ResumeThread(process->hThread) == DWORD(-1))
        OutputDebugStringA("[steam_module] Failed to resume the game's primary thread.\n");
}

BOOL WINAPI create_w(LPCWSTR app, LPWSTR command, LPSECURITY_ATTRIBUTES pa,
    LPSECURITY_ATTRIBUTES ta, BOOL inherit, DWORD flags, LPVOID environment,
    LPCWSTR directory, LPSTARTUPINFOW startup, LPPROCESS_INFORMATION process) {
    bool target = false;
    try { target = game_command(app, command); } catch (...) {}
    const BOOL result = original_create_w(app, command, pa, ta, inherit,
        target ? flags | CREATE_SUSPENDED : flags, environment, directory, startup, process);
    const DWORD error = GetLastError();
    if (result && target && process) finish_launch(process, flags);
    SetLastError(error);
    return result;
}

BOOL WINAPI create_a(LPCSTR app, LPSTR command, LPSECURITY_ATTRIBUTES pa,
    LPSECURITY_ATTRIBUTES ta, BOOL inherit, DWORD flags, LPVOID environment,
    LPCSTR directory, LPSTARTUPINFOA startup, LPPROCESS_INFORMATION process) {
    bool target = false;
    try { target = game_command(app, command); } catch (...) {}
    const BOOL result = original_create_a(app, command, pa, ta, inherit,
        target ? flags | CREATE_SUSPENDED : flags, environment, directory, startup, process);
    const DWORD error = GetLastError();
    if (result && target && process) finish_launch(process, flags);
    SetLastError(error);
    return result;
}

struct Watched { DWORD pid; HANDLE process; };

void scan(std::vector<Watched>& watched) {
    for (auto it = watched.begin(); it != watched.end();) {
        if (WaitForSingleObject(it->process, 0) != WAIT_TIMEOUT) {
            CloseHandle(it->process);
            it = watched.erase(it);
        } else ++it;
    }
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) do {
        if (_wcsicmp(entry.szExeFile, L"csgo.exe") != 0) continue;
        bool seen = false;
        for (const auto& item : watched) if (item.pid == entry.th32ProcessID) seen = true;
        if (seen) continue;
        HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
            PROCESS_VM_READ | PROCESS_VM_WRITE | SYNCHRONIZE, FALSE, entry.th32ProcessID);
        if (!process) continue;
        watched.push_back({entry.th32ProcessID, process});
        stage(process);
    } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
}

DWORD WINAPI worker(LPVOID parameter) {
    const auto module = static_cast<HMODULE>(parameter);
    wchar_t host[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, host, ARRAYSIZE(host))) return 1;
    const auto* name = std::wcsrchr(host, L'\\');
    if (_wcsicmp(name ? name + 1 : host, L"steam.exe") != 0) {
        OutputDebugStringA("[steam_module] Load this DLL into the parent steam.exe process.\n");
        return 1;
    }
    // Import hooks reference this DLL until Steam exits.
    HMODULE pinned = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&worker), &pinned)) return 1;
    if (AllocConsole()) {
        FILE* stream = nullptr;
        freopen_s(&stream, "CONOUT$", "w", stdout);
    }
    try {
        HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(101), RT_RCDATA);
        HGLOBAL loaded = resource ? LoadResource(module, resource) : nullptr;
        const auto* bytes = loaded ? static_cast<const uint8_t*>(LockResource(loaded)) : nullptr;
        const DWORD size = resource ? SizeofResource(module, resource) : 0;
        if (!bytes || size != cfg::BINARY_SIZE) {
            log("Embedded primordial.bin is missing or has the wrong size.");
            return 1;
        }
        payload.assign(bytes, bytes + size);
        HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
        original_create_w = reinterpret_cast<decltype(original_create_w)>(GetProcAddress(kernel, "CreateProcessW"));
        original_create_a = reinterpret_cast<decltype(original_create_a)>(GetProcAddress(kernel, "CreateProcessA"));
        if (!original_create_w || !original_create_a) return 1;
        std::vector<Watched> watched;
        log("Ready. Steam stages the unmodified dump on each launch; Primordial applies all patches.");
        for (;;) {
            const auto count = iat::hook("CreateProcessW", reinterpret_cast<void*>(original_create_w), reinterpret_cast<void*>(&create_w)) +
                iat::hook("CreateProcessA", reinterpret_cast<void*>(original_create_a), reinterpret_cast<void*>(&create_a));
            if (count) log("Installed " + std::to_string(count) + " launch import hooks.");
            scan(watched);
            Sleep(500);
        }
    } catch (...) {
        OutputDebugStringA("[steam_module] Mapping worker failed; restart Steam to retry.\n");
        return 1;
    }
}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, worker, module, 0, nullptr);
        if (!thread) return FALSE;
        CloseHandle(thread);
    }
    return TRUE;
}
