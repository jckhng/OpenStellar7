#pragma once

#include "stellar7/success_control_work.hpp"

namespace stellar7 {

//6F52 entry through the initial705C resident child; suffix remains pending.
SuccessControlWork successInitialEntryOperations(const SuccessControlInput& input);
//Only after initialUploadComplete:706F copy and7072 countdown, then7078.
SuccessControlWork successInitialPaletteReturnOperations(const SuccessControlInput& input,
    const SuccessPaletteClockTiming& returned);

} // namespace stellar7
