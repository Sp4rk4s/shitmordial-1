#pragma once
#include "menu_profile_data.h"
#include <cstddef>
#include <cstdint>

namespace menu_profile {
// Compare exact instruction bytes after applying only declared PE HIGHLOW fixups.
// actual points at a function prefix, never at code to execute.
bool matches(const SourceGuard& guard, const std::uint8_t* actual,
             std::size_t available, std::uint32_t load_base) noexcept;
bool verify_module_files() noexcept;
}
