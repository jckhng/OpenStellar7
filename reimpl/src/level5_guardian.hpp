#pragma once

#include "stellar7/level1.hpp"

#include <functional>

namespace stellar7 {

// A440 owns callback disposition; the caller owns ACEC allocation and event delivery.
void advanceLevel5GuardianBehavior(Level1Entity& actor, std::uint16_t sourceTick,
    const std::function<void(Level1Entity&)>& fire);

}  // namespace stellar7
