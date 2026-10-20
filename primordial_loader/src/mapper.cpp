#include "mapper.h"
#include "config.h"
#include "pe_builder.h"
#include "signatures.h"
#include "pattern_hook.h"
#include "imports.h"
#include "startup.h"
#include "tls.h"
#include "game_modules.h"
#include "../../shared/mapping.h"
#include <cstring>
#include <cstdio>

static_assert(sizeof(void*) == 4, "Primordial must be built for x86.");

namespace mapper {
namespace {
DWORD WINAPI run_entry(LPVOID parameter) {
    auto* image = static_cast<uint8_t*>(parameter);
    using Entry = BOOL (WINAPI*)(HINSTANCE, DWORD, LPVOID);
    const auto entry = reinterpret_cast<Entry>(image + cfg::ENTRY_RVA);
    // The configured entry has the DLL ABI, not LPTHREAD_START_ROUTINE's ABI.
    return entry(reinterpret_cast<HINSTANCE>(image), DLL_PROCESS_ATTACH, nullptr) ? 1 : 0;
}

bool owned_image() {
    uintptr_t cursor = cfg::IMAGE_BASE;
    const uintptr_t end = cursor + mapping::allocation_size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(cursor), &info, sizeof(info)) ||
            info.AllocationBase != mapping::base() || info.State != MEM_COMMIT ||
            info.Type != MEM_PRIVATE || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            return false;
        const auto next = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (next <= cursor) return false;
        cursor = next;
    }
    return true;
}
DWORD section_protection(uint32_t flags) {
    const bool execute = (flags & IMAGE_SCN_MEM_EXECUTE) != 0;
    const bool read = (flags & IMAGE_SCN_MEM_READ) != 0;
    const bool write = (flags & IMAGE_SCN_MEM_WRITE) != 0;
    if (execute) return write ? PAGE_EXECUTE_READWRITE : (read ? PAGE_EXECUTE_READ : PAGE_EXECUTE);
    return write ? PAGE_READWRITE : (read ? PAGE_READONLY : PAGE_NOACCESS);
}
}
Result inject() {
    if (!owned_image())
        return {false, "No owned image at 0x44340000. Restart Steam with the updated steam_module.dll before launching CS:GO.", 0};
    auto* handoff = mapping::handoff();
    if (!mapping::valid(*handoff, GetCurrentProcessId()))
        return {false, "Missing or incompatible Steam mapping handoff.", 0};
    const LONG previous = InterlockedCompareExchange(&handoff->state, mapping::patching, mapping::staged);
    if (previous == mapping::initialized) return {true, {}, cfg::IMAGE_BASE};
    if (previous != mapping::staged)
        return {false, "The image is already being initialized or previously failed. Restart CS:GO after a failure.", 0};
    const auto fail = [&](const std::string& error) -> Result {
        InterlockedExchange(&handoff->state, mapping::failed);
        return {false, error, cfg::IMAGE_BASE};
    };
    auto* image = static_cast<uint8_t*>(mapping::base());
    if (mapping::checksum(image, cfg::BINARY_SIZE) != handoff->checksum)
        return fail("Staged binary checksum mismatch; refusing to patch it.");
    static_assert(cfg::ENTRY_RVA + 16 <= cfg::BINARY_SIZE);
    bool entry_empty = true;
    for (size_t i = 0; i < 16; ++i) entry_empty &= image[cfg::ENTRY_RVA + i] == 0;
    if (entry_empty) return fail("Configured entry point is empty.");
    DWORD old = 0;
    if (!VirtualProtect(image, cfg::IMAGE_SIZE, PAGE_READWRITE, &old))
        return fail("Cannot make staged image writable (" + std::to_string(GetLastError()) + ").");
    const auto startup = startup::prepare(image, cfg::BINARY_SIZE);
    if (!startup.ok) return fail(startup.error);
    const auto modules = game_modules::bind(image, cfg::BINARY_SIZE);
    if (!modules.ok) return fail(modules.error);
    printf("[+] Rebound 12 game module resolver handles.\n");
    const auto tls = payload_tls::initialize(image, cfg::BINARY_SIZE);
    if (!tls.ok) return fail(tls.error);
    printf("[+] Registered static TLS using loader-assigned index %u.\n", tls.index);
    const auto imported = imports::apply_recovered(image, cfg::BINARY_SIZE);
    if (!imported.ok) return fail(imported.error);
    printf("[+] Resolved and applied %zu import fixups.\n", imported.count);
    // Preserve the previous mapper's payload modifications.
    pe::patch_auth(image, cfg::BINARY_SIZE);
    const auto header = pe::build_header();
    std::memcpy(image, header.data(), header.size());
    printf("[+] Applied auth/header patches; preserved captured cookie and reset verified CRT startup state.\n");
    if (!VirtualProtect(image, 0x1000, PAGE_READWRITE, &old))
        return fail("Cannot set header protection.");
    for (const auto& section : cfg::SECTIONS) {
        if (section.rva > cfg::IMAGE_SIZE || section.vsize > cfg::IMAGE_SIZE - section.rva)
            return fail("Configured section extends outside the mapped image.");
        if (!VirtualProtect(image + section.rva, section.vsize,
                            section_protection(section.characteristics), &old))
            return fail(std::string("Cannot protect section ") + section.name + ".");
    }
    if (!FlushInstructionCache(GetCurrentProcess(), image, cfg::IMAGE_SIZE))
        return fail("Cannot flush the patched instruction cache.");
    sig::dump_status();
    const int missing = sig::resolve_all();
    if (missing) printf("[!] %d diagnostic signatures failed to resolve.\n", missing);
    sig::dump_netvars();
    const auto hook = pattern_hook::install();
    if (!hook.ok) return fail(hook.error);
    DWORD thread_id = 0;
    HANDLE thread = CreateThread(nullptr, 0, run_entry, image, 0, &thread_id);
    if (!thread)
        return fail("Cannot create payload entry thread (" + std::to_string(GetLastError()) + ").");
    printf("[*] Entry thread %lu started for 0x%08X.\n", thread_id, cfg::IMAGE_BASE + cfg::ENTRY_RVA);
    // Keep the patcher's worker alive until startup completes, so its result and
    // handoff state reflect the entry's return value rather than thread creation.
    const DWORD waited = WaitForSingleObject(thread, INFINITE);
    DWORD entry_result = 0;
    const BOOL queried = waited == WAIT_OBJECT_0 && GetExitCodeThread(thread, &entry_result);
    const DWORD error = queried ? ERROR_SUCCESS : GetLastError();
    CloseHandle(thread);
    if (!queried)
        return fail("Cannot collect payload entry thread result (" + std::to_string(error) + ").");
    if (!entry_result)
        return fail("Payload entry point returned FALSE.");
    InterlockedExchange(&handoff->state, mapping::initialized);
    return {true, {}, cfg::IMAGE_BASE};
}
}
