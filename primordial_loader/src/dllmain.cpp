#include "mapper.h"
#include "crashlog.h"
#include "config.h"
#include "menu_ui.h"
#include "frame_stage.h"
#include <windows.h>
#include <algorithm>
#include <cstdio>

static FILE* s_console = nullptr;

// Team select and map change crash fix: make the matched function `mov al,1; ret 4`.
static void apply_crash_fix() {
    constexpr unsigned char pattern[]{0x55,0x8B,0xEC,0x56,0x8B,0xF1,0x33,0xC0,0x57,0x8B,0x7D,0x08};
    constexpr unsigned char patch[]{0xB0,0x01,0xC2,0x04,0x00};
    for (auto name : {L"client.dll", L"engine.dll", L"server.dll", L"studiorender.dll", L"materialsystem.dll"}) {
        auto* base = reinterpret_cast<unsigned char*>(GetModuleHandleW(name));
        if (!base) { printf("[!] Crash fix: %ls not loaded.\n", name); continue; }
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
        auto* end = base + nt->OptionalHeader.SizeOfImage;
        auto* hit = std::search(base, end, std::begin(pattern), std::end(pattern));
        bool ok = hit != end && WriteProcessMemory(GetCurrentProcess(), hit, patch, sizeof(patch), nullptr);
        printf(ok ? "[+] Crash fix applied in %ls at %p.\n" : "[!] Crash fix: pattern not found/patched in %ls.\n", name, hit);
    }
}

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
    apply_crash_fix();

    auto result = mapper::inject();

    if (result.ok) {
        printf("\n[+] Payload entry completed.\n");
        printf("    Base: 0x%08X\n", static_cast<uint32_t>(result.base));
        printf("    Entry: 0x%08X\n\n", cfg::IMAGE_BASE + cfg::ENTRY_RVA);
        if (menu_ui::install(static_cast<HMODULE>(hModule))) {
            printf("[*] Menu hooks installed; waiting for the first UI paint.\n");
            printf("[*] Preparing FrameStageNotify hook:\n");
            printf(frame_stage::install() ? "[+] FrameStageNotify hooked; original knife changer runs at stage 2.\n"
                                          : "[!] FrameStageNotify hook not installed.\n");
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
