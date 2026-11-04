#pragma once
#include "stellar7/level1.hpp"

namespace stellar7 {
inline constexpr std::uint64_t destructionLifetime=12;
// A confirmed death observation, independent of the original bitmap-effect pool.
struct DestructionCue {
    std::uint64_t id,born;
    std::size_t level;
    FixedPosition position;
    std::int16_t type;
    std::uint16_t radius,event;
};
}
