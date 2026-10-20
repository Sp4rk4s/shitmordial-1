#include "../steam_module/staging.h"
#include "../steam_module/iat_hook.h"
#include "../primordial_loader/src/mapper.h"
#include "../primordial_loader/src/pattern_hook.h"
#include "../primordial_loader/src/imports.h"
#include "../primordial_loader/src/startup.h"
#include "../primordial_loader/src/tls.h"
#include "../primordial_loader/src/game_modules.h"
namespace game_modules { Result bind(uint8_t*, size_t, Lookup, void*) { return {true, {}}; } }
namespace startup { Result prepare(uint8_t*, size_t) { return {true, {}}; } }
namespace payload_tls { Result initialize(uint8_t*, size_t) { return {true, 0, {}}; } }
#include <cstdio>
#include <cstring>
#include <stdexcept>

// Signature diagnostics are unrelated to mapping and require actual game modules.
namespace sig {
void dump_status() {}
int resolve_all() { return 0; }
void dump_netvars() {}
}

// Mapping fixtures are not the real scanner. Its actual MinHook integration is
// exercised separately rather than patching arbitrary synthetic bytes here.
namespace pattern_hook {
Result install() { return {true, {}}; }
}

// The submitted import profile targets different bytes. Import validation and
// actual fixups are tested separately; these fixtures only test mapper lifecycle.
namespace imports {
Result apply_recovered(uint8_t*, size_t) { return {true, 0, {}}; }
}

namespace {
constexpr uint32_t counter_rva = 0x4F1000;
DWORD entry_thread_id = 0;
unsigned entry_calls = 0;
BOOL entry_arguments_valid = FALSE;

BOOL WINAPI observe_entry(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    entry_thread_id = GetCurrentThreadId();
    ++entry_calls;
    entry_arguments_valid = instance == reinterpret_cast<HINSTANCE>(mapping::base()) &&
        reason == DLL_PROCESS_ATTACH && reserved == nullptr;
    return entry_arguments_valid;
}

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

std::vector<uint8_t> sample(bool entry_result = true, bool empty = false) {
    std::vector<uint8_t> bytes(cfg::BINARY_SIZE, 0x5A);
    // Synthetic x86 stdcall entry: increment a counter, return BOOL, pop 3 args.
    const uint8_t code[] = {0xFF, 0x05, 0, 0, 0, 0, 0xB8, 0, 0, 0, 0, 0xC2, 0x0C, 0};
    std::memcpy(bytes.data() + cfg::ENTRY_RVA, code, sizeof(code));
    const uint32_t counter = cfg::IMAGE_BASE + counter_rva;
    const uint32_t result = entry_result ? 1 : 0;
    std::memcpy(bytes.data() + cfg::ENTRY_RVA + 2, &counter, sizeof(counter));
    std::memcpy(bytes.data() + cfg::ENTRY_RVA + 7, &result, sizeof(result));
    std::memset(bytes.data() + counter_rva, 0, 4);
    if (empty) std::memset(bytes.data() + cfg::ENTRY_RVA, 0, 16);
    return bytes;
}

void verify_initialized() {
    auto* image = static_cast<uint8_t*>(mapping::base());
    require(mapping::handoff()->state == mapping::initialized, "handoff not initialized");
    require(*reinterpret_cast<uint32_t*>(image + counter_rva) == 1, "entry did not execute exactly once");
    require(*reinterpret_cast<uint16_t*>(image) == IMAGE_DOS_SIGNATURE, "header not restored");
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(image + dos->e_lfanew);
    require(nt->OptionalHeader.AddressOfEntryPoint == cfg::ENTRY_RVA, "entry RVA changed");
    require(nt->OptionalHeader.ImageBase == cfg::IMAGE_BASE, "image base changed");
    const uint8_t auth[] = {0xB8, 1, 0, 0, 0, 0xC3};
    require(std::memcmp(image + cfg::AUTH_CHECK_RVA, auth, sizeof(auth)) == 0, "auth patch lost");
    require(*reinterpret_cast<uint32_t*>(cfg::SECURITY_COOKIE) == 0x5A5A5A5A, "captured cookie changed");
    require(*reinterpret_cast<uint32_t*>(cfg::SECURITY_COOKIE_COMP) == 0x5A5A5A5A, "captured complement changed");
    require(*reinterpret_cast<uint32_t*>(cfg::IMPORT_RESOLVER_OBJ_1) == 0x5A5A5A5A, "dump BSS/resolver state was zeroed");
    MEMORY_BASIC_INFORMATION info{};
    require(VirtualQuery(image + 0x400000, &info, sizeof(info)) != 0 && info.Protect == PAGE_EXECUTE_READWRITE,
            "captured code page is not executable");
    require(VirtualQuery(image + 0x1000, &info, sizeof(info)) != 0 && info.Protect == PAGE_EXECUTE_READWRITE,
            "text protection not preserved");
}

void success() {
    const auto bytes = sample();
    const auto staged = staging::map(GetCurrentProcess(), bytes);
    require(staged.ok && !staged.existing, staged.error.c_str());
    require(std::memcmp(mapping::base(), bytes.data(), bytes.size()) == 0, "Steam changed raw payload bytes");
    require(mapper::inject().ok, "initialization failed");
    verify_initialized();
    require(mapper::inject().ok, "repeat initialization failed");
    require(staging::map(GetCurrentProcess(), bytes).existing, "repeat staging lost ownership");
    verify_initialized();
}

void entry_thread() {
    auto bytes = sample();
    // Tail-call a test DLL entry with the wrapper's unchanged stack arguments.
    const uint8_t code[] = {0xB8, 0, 0, 0, 0, 0xFF, 0xE0};
    std::memcpy(bytes.data() + cfg::ENTRY_RVA, code, sizeof(code));
    const auto address = reinterpret_cast<uint32_t>(&observe_entry);
    std::memcpy(bytes.data() + cfg::ENTRY_RVA + 1, &address, sizeof(address));
    require(staging::map(GetCurrentProcess(), bytes).ok, "staging failed");
    require(mapper::inject().ok, "thread entry failed");
    require(entry_arguments_valid, "DLL entry arguments changed");
    require(entry_thread_id && entry_thread_id != GetCurrentThreadId(), "entry ran on the patcher thread");
    require(entry_calls == 1 && mapper::inject().ok && entry_calls == 1, "entry thread started twice");
}

void lifecycle() {
    wchar_t executable[32768];
    require(GetModuleFileNameW(nullptr, executable, ARRAYSIZE(executable)) != 0, "cannot find test executable");
    for (int launch = 0; launch < 2; ++launch) {
        std::wstring command = L"\"" + std::wstring(executable) + L"\" child";
        STARTUPINFOW startup{sizeof(startup)};
        PROCESS_INFORMATION process{};
        require(CreateProcessW(executable, command.data(), nullptr, nullptr, FALSE,
            CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != FALSE, "cannot create child");
        const auto staged = staging::map(process.hProcess, sample());
        if (!staged.ok) {
            TerminateProcess(process.hProcess, 1);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            throw std::runtime_error(staged.error);
        }
        require(ResumeThread(process.hThread) != DWORD(-1), "cannot resume child");
        CloseHandle(process.hThread);
        const DWORD waited = WaitForSingleObject(process.hProcess, 15000);
        if (waited != WAIT_OBJECT_0) TerminateProcess(process.hProcess, 1);
        DWORD exit_code = 1;
        GetExitCodeProcess(process.hProcess, &exit_code);
        CloseHandle(process.hProcess);
        require(waited == WAIT_OBJECT_0 && exit_code == 0, "child staging/initialization failed");
    }
}

ULONGLONG WINAPI fake_ticks() { return 123456789ull; }
// MSVC treats IAT slots as constant and can reuse one load across calls; force a fresh read.
__declspec(noinline) ULONGLONG ticks_via_iat() { return GetTickCount64(); }
void hook_test() {
    void* original = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetTickCount64"));
    void* replacement = reinterpret_cast<void*>(&fake_ticks);
    require(iat::hook("GetTickCount64", original, replacement) > 0, "no imports hooked");
    const auto ticks = ticks_via_iat();
    iat::hook("GetTickCount64", replacement, original);
    require(ticks == 123456789ull, "hook was not invoked");
    require(ticks_via_iat() != 123456789ull, "import not restored");
}
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "scenario required");
        const std::string scenario = argv[1];
        if (scenario == "success") success();
        else if (scenario == "entry-thread") entry_thread();
        else if (scenario == "missing") {
            require(!mapper::inject().ok, "patcher accepted a missing mapping");
            MEMORY_BASIC_INFORMATION info{};
            require(VirtualQuery(mapping::base(), &info, sizeof(info)) != 0 && info.State == MEM_FREE,
                    "patcher allocated or remapped the missing image");
        }
        else if (scenario == "unstaged") {
            MEMORY_BASIC_INFORMATION info{};
            require(VirtualQuery(mapping::base(), &info, sizeof(info)) != 0 && info.State == MEM_FREE,
                    "unrelated or failed launch was staged");
        } else if (scenario == "child") {
            const auto result = mapper::inject();
            require(result.ok, result.error.c_str());
            verify_initialized();
        } else if (scenario == "lifecycle") lifecycle();
        else if (scenario == "iat") hook_test();
        else if (scenario == "foreign") {
            require(VirtualAlloc(mapping::base(), 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == mapping::base(), "cannot prepare collision");
            *static_cast<uint32_t*>(mapping::base()) = 0x12345678;
            require(!staging::map(GetCurrentProcess(), sample()).ok, "foreign memory accepted by Steam");
            require(!mapper::inject().ok, "foreign memory accepted by mapper");
            require(*static_cast<uint32_t*>(mapping::base()) == 0x12345678, "foreign memory modified");
        } else if (scenario == "corrupt" || scenario == "entry-false" || scenario == "empty-entry") {
            const auto bytes = sample(scenario != "entry-false", scenario == "empty-entry");
            require(staging::map(GetCurrentProcess(), bytes).ok, "staging failed");
            if (scenario == "corrupt") static_cast<uint8_t*>(mapping::base())[0x2000] ^= 1;
            const auto result = mapper::inject();
            require(!result.ok && !result.error.empty(), "bad image accepted");
            require(mapping::handoff()->state == mapping::failed, "failure not published");
            require(!mapper::inject().ok, "failed image reinitialized");
            require(!staging::map(GetCurrentProcess(), bytes).ok, "failed image overwritten");
            if (scenario != "entry-false")
                require(static_cast<uint8_t*>(mapping::base())[cfg::AUTH_CHECK_RVA] == 0x5A, "validation failed after patching");
        } else throw std::runtime_error("unknown scenario");
        std::printf("PASS: %s\n", argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
