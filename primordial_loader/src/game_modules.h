#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace game_modules {
struct Binding { uint32_t rva; uint32_t hash; const char* name; };
inline constexpr std::array<Binding, 14> bindings{{
    {0x4DDF28, 0x293F84BD, "shaderapidx9.dll"},
    {0x4DDF50, 0x19C2E8FC, "gameoverlayrenderer.dll"},
    {0x4DDF78, 0x990704D7, "vstdlib.dll"},
    {0x4DDFA0, 0xD0849ACF, "vguimatsurface.dll"},
    {0x4DDFC8, 0x6C5A4374, "client.dll"},
    {0x4DDFF0, 0xE26D30E8, "server.dll"},
    {0x4DE018, 0x38450191, "engine.dll"},
    {0x4DE040, 0x87F9FB5B, "materialsystem.dll"},
    {0x4DE068, 0x2935FA11, "tier0.dll"},
    {0x4DE090, 0xE40E165D, "studiorender.dll"},
    {0x4DE0B8, 0x4943F878, "inputsystem.dll"},
    {0x4DE0E0, 0x3F8D8A40, "vgui2.dll"},
    {0x4DE108, 0x35153F28, "panorama.dll"},
    {0x4DE130, 0xFC70FD90, "localize.dll"},
}};
struct Result { bool ok; std::string error; };
using Lookup = uintptr_t (*)(const char*, void*);
// After startup::prepare, before constructors. Never load a missing game DLL.
Result bind(uint8_t* image, size_t size, Lookup lookup = nullptr, void* context = nullptr);
}
