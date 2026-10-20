#include "../primordial_loader/src/crashlog.h"
#include <windows.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Separate from C++ objects that need unwinding: verify the logger leaves
// exception dispatch to the application's ordinary frame-based handler.
DWORD raise_and_catch(DWORD code, DWORD count, const ULONG_PTR* args) {
    __try {
        RaiseException(code, 0, count, args);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }
    return 0;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
}

int main() {
    try {
        wchar_t executable[MAX_PATH];
        require(GetModuleFileNameW(nullptr, executable, MAX_PATH) != 0, "missing fixture path");
        const auto directory = std::filesystem::path(executable).parent_path();
        require(directory.filename() == L"crashlog probe", "fixture must own its log directory");
        const auto log = directory / L"primordial_crash.log";
        const auto dump = directory / L"primordial_crash.dmp";
        std::filesystem::remove(log);
        std::filesystem::remove(dump);
        // A notification must not overwrite an earlier useful diagnostic dump.
        { std::ofstream sentinel(dump, std::ios::binary); sentinel << "retained-dump"; }
        crash::install();
        const ULONG_PTR info[] = {0x1000, reinterpret_cast<ULONG_PTR>("fixture-thread"),
                                  GetCurrentThreadId(), 0};
        require(raise_and_catch(0x406D1388, 4, info) == 0x406D1388, "thread notification was consumed");
        require(raise_and_catch(0x40010006, 0, nullptr) == 0x40010006, "ANSI debug notification was consumed");
        require(raise_and_catch(0x4001000A, 0, nullptr) == 0x4001000A, "Unicode debug notification was consumed");
        require(read_file(log).empty(), "notifications produced exception reports");
        require(read_file(dump) == "retained-dump", "notification overwrote the previous dump");

        // The same code with an invalid protocol must retain diagnostics.
        const ULONG_PTR malformed[] = {0x1001, info[1], info[2], 0};
        require(raise_and_catch(0x406D1388, 4, malformed) == 0x406D1388, "malformed notification was consumed");
        require(read_file(log).find("THREAD_NAME_CODE") != std::string::npos, "malformed notification was hidden");
        const ULONG_PTR access[] = {8, 0x1234};
        require(raise_and_catch(EXCEPTION_ACCESS_VIOLATION, 2, access) == EXCEPTION_ACCESS_VIOLATION,
                "access violation dispatch changed");
        crash::remove();
        const auto text = read_file(log);
        require(text.find("ACCESS_VIOLATION") != std::string::npos, "access violation was hidden");
        require(text.find("executing 0x00001234") != std::string::npos, "execute access reported as a write");
        require(text.find("EXCEPTION OBSERVED @") != std::string::npos, "observation classification missing");
        require(text.find("CRASH @") == std::string::npos, "first-chance report asserts a crash");
        require(read_file(dump).starts_with("MDMP"), "non-notification minidump missing");
        puts("Exception notification and dispatch checks passed.");
        return 0;
    } catch (const std::exception& error) {
        crash::remove();
        fprintf(stderr, "FAILED: %s\n", error.what());
        return 1;
    }
}
