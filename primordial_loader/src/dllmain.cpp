#include "mapper.h"
#include "crashlog.h"
#include "config.h"
#include "menu_ui.h"
#include <windows.h>
#include <cstdio>

static FILE* s_console = nullptr;

static void open_console() {
    if (AllocConsole()) {
        freopen_s(&s_console, "CONOUT$", "w", stdout);
        FILE* f;
        freopen_s(&f, "CONOUT$", "w", stderr);
    }
}

static DWORD WINAPI init_thread(LPVOID hModule) {
    open_console();

    printf("=== Primordial ===\n");
    printf("Image base:  0x%08X\n", cfg::IMAGE_BASE);
    printf("Entry RVA:   0x%08X\n", cfg::ENTRY_RVA);
    printf("Binary size: 0x%X (%u bytes)\n\n", cfg::BINARY_SIZE, cfg::BINARY_SIZE);

    crash::install();
    printf("Crash log: %ls\n", crash::log_path().c_str());
    printf("Crash handler active in csgo.exe process.\n\n");

    auto result = mapper::inject();

    if (result.ok) {
        printf("\n[+] Payload entry completed.\n");
        printf("    Base: 0x%08X\n", static_cast<uint32_t>(result.base));
        printf("    Entry: 0x%08X\n\n", cfg::IMAGE_BASE + cfg::ENTRY_RVA);
        if (menu_ui::install(static_cast<HMODULE>(hModule))) {
            printf("[*] Menu hooks installed; waiting for the first UI paint.\n");
        } else {
            printf("[!] Menu UI installation failed. Check primordial_menu.log in the game directory.\n");
        }
    } else {
        printf("\n[!] Failed: %s\n", result.error.c_str());
        crash::remove();
        FreeLibraryAndExitThread(static_cast<HMODULE>(hModule), 1);
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        // The matching MinHook build uses static CRT. Keep thread notifications
        // enabled so that CRT per-thread initialization and cleanup still run.
        // Run on a new thread — DllMain can't do heavy work
        CloseHandle(CreateThread(nullptr, 0, init_thread, hModule, 0, nullptr));
    }
    return TRUE;
}
