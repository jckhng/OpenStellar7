#pragma once

#include "stellar7/success_palette_clock.hpp"
#include <optional>

namespace stellar7 {

enum class SuccessPaletteResidentStage { entry, waitRetrace, upload, finished };

// Retained source locals for one resident child, never a new clock/device owner.
struct SuccessPaletteResidentCursor {
    SuccessPaletteState state;
    SuccessPaletteExecution execution = SuccessPaletteExecution::advance;
    SuccessPaletteCompletionSource completion = SuccessPaletteCompletionSource::soundHandle;
    SuccessPaletteResidentStage stage = SuccessPaletteResidentStage::entry;
};

struct SuccessPaletteStepTiming {
    SuccessPaletteResidentCursor cursor;
    PaletteClockTiming palette;
    // Absent at a source checkpoint; present only after706C/70EB or before7204.
    std::optional<SuccessPaletteBoundary> boundary;
};

struct ClockedSuccessPaletteStepWork {
    SuccessPaletteResidentCursor cursor;
    ClockedPaletteWork palette;
    std::optional<SuccessPaletteBoundary> boundary;
};

// Bounded foreground checkpointing with the same retained IRQ/VGA/DAC owners.
// A yield never repeats7078 status reads or70AD..70D6 oscillator writes.
// Existing whole-child successPaletteClockTiming remains independently strict.
SuccessPaletteStepTiming successPaletteStepTiming(const SoundBlasterClockPhase& phase,
    const SuccessPaletteResidentCursor& cursor, const PaletteWorkState& palette,
    std::uint64_t foregroundBudget, std::int64_t horizonCycles);

} // namespace stellar7
