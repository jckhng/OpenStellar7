#pragma once

#include "stellar7/normal_clock.hpp"

namespace stellar7 {

// Declared native mode-13/PIC/PIT/BIOS epoch; no original DOS phase input.
SoundBlasterClockPhase nativeTimerInstallPhase();

// 0824:41AA -> 41B3, excluding caller ADD SP,2. Vector services execute the
// bounded default DOS guest stub in the clock, with semantic host vector effects.
std::vector<NormalWorkOperation> timerInstallOperations();

}  // namespace stellar7
