#include <windows.h>

// A normal, loader-managed DLL reserves exactly the recovered payload template.
// It has no CRT, callbacks or executable payload code.
#pragma section(".tls", read, write)
#pragma section(".rdata$T", read)
extern "C" {
__declspec(allocate(".tls")) unsigned char payload_template[8] = {};
volatile DWORD payload_tls_index = TLS_OUT_OF_INDEXES;
const PIMAGE_TLS_CALLBACK payload_tls_callbacks[] = {nullptr};
extern __declspec(allocate(".rdata$T")) const IMAGE_TLS_DIRECTORY32 _tls_used = {
    reinterpret_cast<DWORD>(payload_template),
    reinterpret_cast<DWORD>(payload_template + sizeof(payload_template)),
    reinterpret_cast<DWORD>(&payload_tls_index),
    reinterpret_cast<DWORD>(payload_tls_callbacks),
    0, IMAGE_SCN_ALIGN_4BYTES
};
DWORD __cdecl primordial_tls_get_index() { return payload_tls_index; }
BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    // Keep a real import: on the tested Windows loader an importless DLL's
    // TLS directory was skipped, leaving AddressOfIndex untouched.
    return reason != DLL_PROCESS_ATTACH || GetCurrentProcessId() != 0;
}
}
