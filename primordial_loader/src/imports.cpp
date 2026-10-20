#include "imports.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <cstdlib>

static_assert(sizeof(void*) == 4, "Import operands and export addresses are x86.");

namespace imports {
namespace {
const Patch submitted[] = {
#include "imports_manifest.inc"
};
const Patch recovered[] = {
#include "imports_vmp_manifest.inc"
};

std::string site_error(const Patch& patch, const char* reason) {
    char address[40];
    snprintf(address, sizeof(address), " at RVA 0x%08X", patch.rva);
    return std::string(patch.module) + "!" + patch.symbol + address + ": " + reason;
}

struct Modules {
    struct Module { std::string name; HMODULE handle; };
    std::vector<Module> loaded;
    bool retain = false;
    ~Modules() {
        if (!retain) for (const auto& module : loaded) FreeLibrary(module.handle);
    }
    uintptr_t resolve(const char* name, const char* symbol) {
        HMODULE handle = nullptr;
        for (const auto& module : loaded) if (module.name == name) handle = module.handle;
        if (!handle) {
            if (!GetModuleHandleExA(0, name, &handle))
                handle = LoadLibraryExA(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!handle) return 0;
            loaded.push_back({name, handle});
        }
        // GetProcAddress also supports the exported locale-id data symbols.
        const char* export_name = symbol;
        if (symbol[0] == '#') {
            char* end = nullptr;
            const unsigned long ordinal = std::strtoul(symbol + 1, &end, 10);
            if (end == symbol + 1 || *end || !ordinal || ordinal > 65535) return 0;
            export_name = MAKEINTRESOURCEA(static_cast<WORD>(ordinal));
        }
        return reinterpret_cast<uintptr_t>(GetProcAddress(handle, export_name));
    }
};
}

Result apply(uint8_t* image, size_t size, std::span<const Patch> patches,
             Resolver resolver, void* context) {
    if (!image) return {false, 0, "Import mapping requires an existing image."};
    for (size_t i = 0; i < patches.size(); ++i) {
        const auto& patch = patches[i];
        if (!patch.module || !patch.symbol) return {false, 0, "Import record is missing its module or export name."};
        if (patch.rva > size || sizeof(uint32_t) > size - patch.rva)
            return {false, 0, site_error(patch, "patch extends outside the mapped binary")};
        if (patch.kind == Kind::relative && (!patch.rva || image[patch.rva - 1] != 0xE8))
            return {false, 0, site_error(patch, "expected an E8 displacement operand; source image/base does not match")};
        uint32_t original = 0;
        std::memcpy(&original, image + patch.rva, sizeof(original));
        if (original != patch.original)
            return {false, 0, site_error(patch, "original operand/slot bytes do not match the profile")};
        for (size_t j = 0; j < i; ++j) {
            const uint64_t left = patch.rva, right = patches[j].rva;
            if (left < right + 4 && right < left + 4)
                return {false, 0, site_error(patch, "overlapping import records")};
        }
    }

    Modules modules;
    std::vector<uint32_t> values;
    values.reserve(patches.size());
    for (const auto& patch : patches) {
        const uintptr_t target = resolver ? resolver(patch.module, patch.symbol, context) : modules.resolve(patch.module, patch.symbol);
        if (!target) return {false, 0, site_error(patch, "export resolution failed; no import patches were written")};
        const auto operand = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(image + patch.rva));
        const uint32_t value = patch.kind == Kind::relative
            ? static_cast<uint32_t>(target) - (operand + 4u)
            : static_cast<uint32_t>(target) - (patch.kind == Kind::keyed ? patch.key : 0u);
        values.push_back(value);
    }
    for (size_t i = 0; i < patches.size(); ++i)
        std::memcpy(image + patches[i].rva, &values[i], sizeof(values[i]));
    modules.retain = true; // The mapped image can use these exports until process exit.
    return {true, patches.size(), {}};
}

Result apply_submitted(uint8_t* image, size_t size) {
    return apply(image, size, submitted);
}

Result apply_recovered(uint8_t* image, size_t size) {
    return apply(image, size, recovered);
}
}
