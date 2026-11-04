#include "stellar7/timer_work.hpp"

#include <stdexcept>

namespace stellar7 {
namespace {

constexpr std::uint64_t setupDispatches = 19;
constexpr std::uint64_t finalMaskDispatches = 3;
constexpr std::uint64_t activeSlotDispatches = 8;
constexpr std::uint64_t inactiveSlotDispatches = 3;
constexpr std::uint64_t expiredSlotDispatches = 2;
constexpr std::uint64_t localExitDispatches = 16;
constexpr std::uint64_t biosExitDispatches = 15;
constexpr unsigned unrolledSlots = 5;

}  // namespace

std::uint64_t timerIrqWrapperDispatches(
    const std::uint16_t activeMask, const std::uint16_t expiredMask, const TimerIrqExit exit
) {
    if ((expiredMask & activeMask) != expiredMask) {
        throw std::invalid_argument("expired timer slots must be active");
    }
    auto dispatches = setupDispatches + finalMaskDispatches
        + (exit == TimerIrqExit::localIret ? localExitDispatches : biosExitDispatches);
    auto remaining = activeMask;
    unsigned slot = 0;
    while (remaining != 0) {
        if ((remaining & 1) != 0) {
            dispatches += activeSlotDispatches;
            if ((expiredMask & (1U << slot)) != 0) {
                dispatches += expiredSlotDispatches;
            }
        } else {
            dispatches += inactiveSlotDispatches;
        }
        remaining >>= 1;
        ++slot;
        if (slot % unrolledSlots == 0) {
            ++dispatches;
        }
    }
    return dispatches;
}

}  // namespace stellar7
