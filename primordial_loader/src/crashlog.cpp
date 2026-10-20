#include "crashlog.h"
#include "config.h"
#include <dbghelp.h>
#include <cstdio>
#include <ctime>
#include <shlobj.h>

#pragma comment(lib, "dbghelp.lib")

namespace crash {

static PVOID s_handler = nullptr;
static std::wstring s_log_path;
static FILE* s_log_file = nullptr;

static const char* exception_name(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:      return "ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_BREAKPOINT:            return "BREAKPOINT";
    case EXCEPTION_SINGLE_STEP:           return "SINGLE_STEP";
    case EXCEPTION_ILLEGAL_INSTRUCTION:   return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_PRIV_INSTRUCTION:      return "PRIV_INSTRUCTION";
    case EXCEPTION_STACK_OVERFLOW:        return "STACK_OVERFLOW";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return "FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_OVERFLOW:          return "FLT_OVERFLOW";
    case EXCEPTION_GUARD_PAGE:            return "GUARD_PAGE";
    case EXCEPTION_IN_PAGE_ERROR:         return "IN_PAGE_ERROR";
    case 0x406D1388:                      return "THREAD_NAME_CODE";
    default:                              return "UNKNOWN";
    }
}

static const char* region_name(uint32_t rva) {
    if (rva < 0x1000)    return "[PE header]";
    // The original section boundaries are unknown. Report the configured
    // replacement section instead of the superseded .text/.rdata guesses.
    for (const auto& section : cfg::SECTIONS)
        if (rva >= section.rva && rva - section.rva < section.vsize)
            return section.name;
    return "[out of image]";
}

static void write_log(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    printf("%s", buf);
    OutputDebugStringA(buf);
    if (s_log_file) {
        fputs(buf, s_log_file);
        fflush(s_log_file);
    }
}

static void dump_context(CONTEXT* ctx) {
    write_log("  EAX=%08X  EBX=%08X  ECX=%08X  EDX=%08X\n",
              ctx->Eax, ctx->Ebx, ctx->Ecx, ctx->Edx);
    write_log("  ESI=%08X  EDI=%08X  EBP=%08X  ESP=%08X\n",
              ctx->Esi, ctx->Edi, ctx->Ebp, ctx->Esp);
    write_log("  EIP=%08X  EFLAGS=%08X\n", ctx->Eip, ctx->EFlags);
}

static void dump_stack(CONTEXT* ctx, HANDLE proc) {
    write_log("\n  Stack (ESP=0x%08X):\n", ctx->Esp);
    uint32_t stack[32];
    SIZE_T read = 0;
    if (ReadProcessMemory(proc, reinterpret_cast<void*>(ctx->Esp),
                          stack, sizeof(stack), &read)) {
        int count = static_cast<int>(read / 4);
        for (int i = 0; i < count; ++i) {
            const char* tag = "";
            uint32_t val = stack[i];
            if (val >= cfg::IMAGE_BASE && val < cfg::IMAGE_BASE + cfg::BINARY_SIZE) {
                uint32_t rva = val - cfg::IMAGE_BASE;
                char tmp[64];
                snprintf(tmp, sizeof(tmp), " <- primordial+0x%X (%s)", rva, region_name(rva));
                write_log("    [ESP+%02X] %08X%s\n", i * 4, val, tmp);
            } else {
                write_log("    [ESP+%02X] %08X\n", i * 4, val);
            }
        }
    }
}

static void dump_code_bytes(uint32_t eip, HANDLE proc) {
    uint8_t code[32];
    SIZE_T read = 0;
    if (ReadProcessMemory(proc, reinterpret_cast<void*>(eip), code, sizeof(code), &read) && read > 0) {
        write_log("\n  Code at EIP:\n    ");
        for (SIZE_T i = 0; i < read && i < 16; ++i)
            write_log("%02X ", code[i]);
        write_log("\n");
    }
}

static LONG CALLBACK handler(EXCEPTION_POINTERS* ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;

    // Skip non-fatal exceptions
    if (code == EXCEPTION_BREAKPOINT && ep->ContextRecord->Eip < cfg::IMAGE_BASE)
        return EXCEPTION_CONTINUE_SEARCH;
    // Skip C++ exceptions (0xE06D7363 = "msc")
    if (code == 0xE06D7363)
        return EXCEPTION_CONTINUE_SEARCH;
    // ANSI/Unicode OutputDebugString notifications, not crash reports.
    if (code == 0x40010006 || code == 0x4001000A)
        return EXCEPTION_CONTINUE_SEARCH;
    // Microsoft's legacy x86 SetThreadName protocol. Leave dispatch intact so
    // the application's own SEH handler receives it. A malformed record still
    // gets diagnostic logging, rather than hiding every use of this code.
    if (code == 0x406D1388 && ep->ExceptionRecord->ExceptionFlags == 0 &&
        ep->ExceptionRecord->NumberParameters == 4 &&
        ep->ExceptionRecord->ExceptionInformation[0] == 0x1000 &&
        ep->ExceptionRecord->ExceptionInformation[3] == 0)
        return EXCEPTION_CONTINUE_SEARCH;

    uint32_t eip = ep->ContextRecord->Eip;
    uint32_t rva = eip - cfg::IMAGE_BASE;
    bool in_payload = (eip >= cfg::IMAGE_BASE && eip < cfg::IMAGE_BASE + cfg::BINARY_SIZE);

    time_t now = time(nullptr);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    write_log("\n========================================\n");
    write_log("EXCEPTION OBSERVED @ %s\n", timebuf);
    write_log("  Vectored observation; fatality is not determined here.\n");
    write_log("  Exception: 0x%08X (%s)\n", code, exception_name(code));
    write_log("  Address:   0x%08X", eip);
    if (in_payload)
        write_log(" (primordial+0x%X, %s)", rva, region_name(rva));
    write_log("\n");

    if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
        const auto operation = ep->ExceptionRecord->ExceptionInformation[0];
        const char* op = operation == 0 ? "reading" : operation == 1 ? "writing" :
                         operation == 8 ? "executing" : "accessing";
        uint32_t target = static_cast<uint32_t>(ep->ExceptionRecord->ExceptionInformation[1]);
        write_log("  Fault:     %s 0x%08X", op, target);
        if (target >= cfg::IMAGE_BASE && target < cfg::IMAGE_BASE + cfg::BINARY_SIZE)
            write_log(" (primordial+0x%X)", target - cfg::IMAGE_BASE);
        write_log("\n");
    }

    write_log("\n  Registers:\n");
    dump_context(ep->ContextRecord);

    HANDLE proc = GetCurrentProcess();
    dump_code_bytes(eip, proc);
    dump_stack(ep->ContextRecord, proc);

    // Write minidump
    {
        std::wstring dmp_path = s_log_path;
        auto dot = dmp_path.rfind(L'.');
        if (dot != std::wstring::npos) dmp_path.resize(dot);
        dmp_path += L".dmp";

        HANDLE hFile = CreateFileW(dmp_path.c_str(), GENERIC_WRITE, 0, nullptr,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION mei;
            mei.ThreadId = GetCurrentThreadId();
            mei.ExceptionPointers = ep;
            mei.ClientPointers = FALSE;
            MiniDumpWriteDump(proc, GetCurrentProcessId(), hFile,
                              MiniDumpWithDataSegs, &mei, nullptr, nullptr);
            CloseHandle(hFile);
            write_log("\n  Minidump: %ls\n", dmp_path.c_str());
        }
    }

    write_log("========================================\n\n");
    return EXCEPTION_CONTINUE_SEARCH;
}

void install() {
    // Log file next to our exe
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    s_log_path = path;
    auto slash = s_log_path.rfind(L'\\');
    if (slash != std::wstring::npos) s_log_path.resize(slash + 1);
    s_log_path += L"primordial_crash.log";

    s_log_file = _wfopen(s_log_path.c_str(), L"a");
    s_handler = AddVectoredExceptionHandler(1, handler);

    char msg[512];
    snprintf(msg, sizeof(msg), "[*] Crash logger installed -> %ls\n",  s_log_path.c_str());
    printf("%s", msg);
    OutputDebugStringA(msg);
}

void remove() {
    if (s_handler) {
        RemoveVectoredExceptionHandler(s_handler);
        s_handler = nullptr;
    }
    if (s_log_file) {
        fclose(s_log_file);
        s_log_file = nullptr;
    }
}

const std::wstring& log_path() { return s_log_path; }

}
