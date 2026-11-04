#pragma once

#include "stellar7/gameplay.hpp"

#include <cstdint>
#include <vector>

namespace stellar7 {

// Import one verified 64 KiB runtime DS image at a Level 3 main-loop
// boundary. This is a target-local validation/recording bridge; normal
// portable campaign startup remains independent of runtime captures.
void loadLevel3NativeCheckpoint(
    GameplayModel& model,
    const std::vector<std::uint8_t>& data
);

}  // namespace stellar7
