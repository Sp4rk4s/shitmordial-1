#include "game_modules.h"
#include <windows.h>
#include <cstring>
#include <limits>

namespace game_modules {
namespace {
uintptr_t loaded_module(const char* name, void*) {
    return reinterpret_cast<uintptr_t>(GetModuleHandleA(name));
}
}
Result bind(uint8_t* image, size_t size, Lookup lookup, void* context) {
    if (!image) return {false, "Missing image for game module binding."};
    // The first DWORD is the persistent module-name hash; constructors rebuild
    // only the unordered_map at +8 and leave the hash/handle pair untouched.
    for (const auto& binding : bindings) {
        if (binding.rva > size || 40 > size - binding.rva)
            return {false, "Game resolver object is outside the image."};
        uint32_t hash = 0, handle = 0;
        std::memcpy(&hash, image + binding.rva, 4);
        std::memcpy(&handle, image + binding.rva + 4, 4);
        if (hash != binding.hash || handle)
            return {false, "Game resolver was not prepared: " + std::string(binding.name)};
    }
    std::array<uint32_t, bindings.size()> handles{};
    if (!lookup) lookup = loaded_module;
    for (size_t i = 0; i < bindings.size(); ++i) {
        const uintptr_t module = lookup(bindings[i].name, context);
        if (!module || module > (std::numeric_limits<uint32_t>::max)())
            return {false, "Required game module is not loaded: " + std::string(bindings[i].name)};
        handles[i] = static_cast<uint32_t>(module);
    }
    for (size_t i = 0; i < bindings.size(); ++i)
        std::memcpy(image + bindings[i].rva + 4, &handles[i], 4);
    return {true, {}};
}
}
