// One-shot x32dbg settings adapter. Loaded into the debugger, never the game.
// Its GUI is hidden/unavailable to UI Automation; use the documented bridge API
// to handle the legacy thread-naming notification without masking real faults.
#include <windows.h>
#include <string>

DWORD WINAPI configure(void*) {
    HMODULE bridge = GetModuleHandleW(L"x32bridge.dll");
    if (!bridge) return 1;
    using Get = bool (*)(const char*, const char*, char*);
    using Set = bool (*)(const char*, const char*, const char*);
    using Flush = bool (*)();
    using Updated = void (*)();
    auto get = reinterpret_cast<Get>(GetProcAddress(bridge, "BridgeSettingGet"));
    auto set = reinterpret_cast<Set>(GetProcAddress(bridge, "BridgeSettingSet"));
    auto flush = reinterpret_cast<Flush>(GetProcAddress(bridge, "BridgeSettingFlush"));
    auto updated = reinterpret_cast<Updated>(GetProcAddress(bridge, "DbgSettingsUpdated"));
    if (!get || !set || !flush || !updated) return 2;
    char buffer[65536]{}; // MAX_SETTING_SIZE from x64dbg's bridgemain.h
    if (!get("Exceptions", "IgnoreRange", buffer)) return 3;
    std::string filters(buffer);
    if (filters.find("406D1388") != std::string::npos || filters.find("406d1388") != std::string::npos)
        return 4; // Preserve any existing specific user policy.
    if (!filters.empty()) filters += ',';
    filters += "406D1388-406D1388:nobreak:log:debugger";
    if (filters.size() >= sizeof(buffer) || !set("Exceptions", "IgnoreRange", filters.c_str())) return 5;
    updated();
    return flush() ? 0 : 6;
}
BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        HANDLE thread = CreateThread(nullptr, 0, configure, nullptr, 0, nullptr);
        if (!thread) return FALSE;
        CloseHandle(thread);
    }
    return TRUE;
}
