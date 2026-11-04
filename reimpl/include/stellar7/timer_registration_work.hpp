#pragma once

#include "stellar7/normal_clock.hpp"

namespace stellar7 {

// Resident26B1..26FA, fresh slot0 with period4 and callback1EBC:0494.
// Excludes the caller CALL and argument cleanup.
std::vector<NormalWorkOperation> timerRegistrationOperations();

}  // namespace stellar7
