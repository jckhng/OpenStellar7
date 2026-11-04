#pragma once

#include <cstdint>

namespace stellar7 {

enum class TimerIrqExit { localIret, biosChain };

// Original timer wrapper only: includes CALLF/JMPF but excludes their callees,
// port delays, hardware interrupt entry and scheduler charges.
std::uint64_t timerIrqWrapperDispatches(
    std::uint16_t activeMask, std::uint16_t expiredMask, TimerIrqExit exit
);

}  // namespace stellar7
