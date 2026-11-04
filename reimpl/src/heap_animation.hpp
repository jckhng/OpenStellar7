#pragma once

#include "stellar7/level1.hpp"

namespace stellar7 {
inline constexpr std::uint8_t type11AnimationLimit = 8;

void advanceHeapAnimation(Level1Entity& entity, unsigned limit, unsigned selectors=1) noexcept;
void snapshotHeapAnimation(Level1Entity& entity) noexcept;
} // namespace stellar7
