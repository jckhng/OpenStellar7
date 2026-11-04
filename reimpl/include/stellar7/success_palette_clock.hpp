#pragma once

#include "stellar7/presentation_clock.hpp"

namespace stellar7 {

enum class SuccessPaletteCompletionSource { soundHandle, timer };
enum class SuccessPaletteBoundary { initialUploadComplete, repeat, presentationComplete };

struct SuccessPaletteClockTiming {
    SuccessPaletteState state;
    PaletteClockTiming palette;
    SuccessPaletteBoundary boundary = SuccessPaletteBoundary::initialUploadComplete;
};

// Initial:705C..706C through516B, stops before706F CALL8484.
// Advance:7078..70EB through516B, or stops at7204 before CALL51A6.
// Graphics-enabled path only; sound/timer reads borrow the live clock owners.
// Resident516B reads its stack palette; persistent source/output banks survive.
SuccessPaletteClockTiming successPaletteClockTiming(const SoundBlasterClockPhase& phase,
    const SuccessPaletteState& state, const PaletteWorkState& palette,
    SuccessPaletteExecution execution, SuccessPaletteCompletionSource completion,
    std::int64_t horizonCycles);

struct ClockedSuccessPaletteWork {
    SuccessPaletteState state;
    ClockedPaletteWork palette;
    SuccessPaletteBoundary boundary;
};

} // namespace stellar7
