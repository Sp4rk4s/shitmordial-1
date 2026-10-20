// Exercise the production hook functions in a disposable host, without starting
// its Steam-only worker. The fake csgo.exe is our synthetic x86 mapping harness.
#define DllMain steam_module_test_entry
#include "../steam_module/dllmain.cpp"
#undef DllMain
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<uint8_t> synthetic_payload() {
    std::vector<uint8_t> bytes(cfg::BINARY_SIZE, 0x5A);
    const uint8_t code[] = {0xFF, 0x05, 0, 0, 0, 0, 0xB8, 1, 0, 0, 0, 0xC2, 0x0C, 0};
    std::memcpy(bytes.data() + cfg::ENTRY_RVA, code, sizeof(code));
    const uint32_t counter = cfg::IMAGE_BASE + 0x4F1000;
    std::memcpy(bytes.data() + cfg::ENTRY_RVA + 2, &counter, sizeof(counter));
    std::memset(bytes.data() + 0x4F1000, 0, 4);
    return bytes;
}

std::string ansi(const std::wstring& value) {
    const int count = WideCharToMultiByte(CP_ACP, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(count, '\0');
    WideCharToMultiByte(CP_ACP, 0, value.c_str(), -1, result.data(), count, nullptr, nullptr);
    result.pop_back();
    return result;
}
}

int wmain(int argc, wchar_t** argv) {
    PROCESS_INFORMATION process{};
    try {
        require(argc == 3, "probe and scenario required");
        std::wstring probe = argv[1];
        const std::wstring scenario = argv[2];
        payload = synthetic_payload();
        HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
        original_create_w = reinterpret_cast<decltype(original_create_w)>(GetProcAddress(kernel, "CreateProcessW"));
        original_create_a = reinterpret_cast<decltype(original_create_a)>(GetProcAddress(kernel, "CreateProcessA"));
        require(original_create_w && original_create_a, "originals missing");
        require(iat::hook("CreateProcessW", reinterpret_cast<void*>(original_create_w), reinterpret_cast<void*>(&create_w)) > 0,
                "no Unicode launch imports hooked");
        require(iat::hook("CreateProcessA", reinterpret_cast<void*>(original_create_a), reinterpret_cast<void*>(&create_a)) > 0,
                "no ANSI launch imports hooked");
        bool expect_staged = true;
        if (scenario == L"staging-failure") { payload.clear(); expect_staged = false; }
        if (scenario == L"unrelated") {
            const auto directory = probe.substr(0, probe.find_last_of(L"\\/"));
            probe = directory + L"/../mapping_tests.exe";
            expect_staged = false;
        }
        std::wstring command = L"\"" + probe + (expect_staged ? L"\" child" : L"\" unstaged");
        const DWORD flags = CREATE_NO_WINDOW | (scenario == L"suspended" ? CREATE_SUSPENDED : 0);
        BOOL created = FALSE;
        if (scenario == L"normal-a") {
            const auto application = ansi(probe);
            auto arguments = ansi(command);
            STARTUPINFOA startup{sizeof(startup)};
            created = CreateProcessA(application.c_str(), arguments.data(), nullptr, nullptr, FALSE,
                flags, nullptr, nullptr, &startup, &process);
        } else {
            STARTUPINFOW startup{sizeof(startup)};
            created = CreateProcessW(scenario == L"command-only" ? nullptr : probe.c_str(), command.data(),
                nullptr, nullptr, FALSE, flags, nullptr, nullptr, &startup, &process);
        }
        require(created != FALSE, "cannot create probe");
        if (scenario == L"suspended") {
            require(WaitForSingleObject(process.hProcess, 50) == WAIT_TIMEOUT, "caller suspension flag lost");
            mapping::Handoff h{};
            SIZE_T count = 0;
            require(ReadProcessMemory(process.hProcess, mapping::handoff(), &h, sizeof(h), &count) &&
                    count == sizeof(h) && mapping::valid(h, process.dwProcessId) && h.state == mapping::staged,
                    "raw image not staged before child ran");
            require(ResumeThread(process.hThread) == 1, "caller suspension count changed");
        }
        require(WaitForSingleObject(process.hProcess, 15000) == WAIT_OBJECT_0, "child failed to resume/exit");
        DWORD code = 1;
        require(GetExitCodeProcess(process.hProcess, &code) && code == 0, "child validation failed");
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        std::printf("PASS: launch hook\n");
        return 0;
    } catch (const std::exception& error) {
        if (process.hProcess) {
            TerminateProcess(process.hProcess, 1);
            CloseHandle(process.hProcess);
        }
        if (process.hThread) CloseHandle(process.hThread);
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
