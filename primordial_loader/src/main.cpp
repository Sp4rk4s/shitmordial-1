#include "mapper.h"
#include "crashlog.h"
#include "steam.h"
#include "config.h"
#include <windows.h>
#include <string>
#include <cstdio>

static void log(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    OutputDebugStringA(buf);
    // Also write to a console if attached
    printf("%s\n", buf);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR lpCmdLine, int) {
    // Attach console for logging
    if (AllocConsole()) {
        FILE* f;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
    }

    log("=== Primordial Loader ===");
    log("Image base:  0x%08X", cfg::IMAGE_BASE);
    log("Entry RVA:   0x%08X", cfg::ENTRY_RVA);
    log("Binary size: 0x%X (%u bytes)", cfg::BINARY_SIZE, cfg::BINARY_SIZE);
    log("");

    // Parse optional PID from command line
    uint32_t pid = 0;
    if (lpCmdLine && lpCmdLine[0]) {
        pid = static_cast<uint32_t>(wcstoul(lpCmdLine, nullptr, 10));
        if (pid) log("Using provided PID: %u", pid);
    }

    crash::install();
    log("Crash log: %ls", crash::log_path().c_str());
    log("Note: crash handler covers this loader only — csgo.exe crashes show in its own logs.");
    log("");

    // Try to inject steam_module.dll into steam.exe for early address reservation.
    // Look for it next to our exe.
    {
        char exe_path[MAX_PATH];
        GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
        std::string dir(exe_path);
        auto slash = dir.rfind('\\');
        if (slash != std::string::npos) dir.resize(slash + 1);
        std::string dll_path = dir + "steam_module.dll";

        DWORD attr = GetFileAttributesA(dll_path.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES)
            steam::inject_module(dll_path);
        else
            log("[*] steam_module.dll not found next to loader — skipping steam hook.");
        log("");
    }

    auto result = mapper::inject(pid);

    if (result.ok) {
        log("");
        log("[+] Injection successful!");
        log("    Base: 0x%08X", static_cast<uint32_t>(result.base));
        log("    Entry executed at: 0x%08X", cfg::IMAGE_BASE + cfg::ENTRY_RVA);
        log("    Crash handler active — logs to primordial_crash.log");
        MessageBoxA(nullptr,
            "Primordial injected successfully.\n"
            "Crash logger is active — check primordial_crash.log if the game crashes.",
            "Primordial Loader", MB_OK | MB_ICONINFORMATION);
    } else {
        log("");
        log("[!] Injection failed: %s", result.error.c_str());
        crash::remove();
        MessageBoxA(nullptr, result.error.c_str(), "Primordial Loader - Error", MB_OK | MB_ICONERROR);
    }

    return result.ok ? 0 : 1;
}
