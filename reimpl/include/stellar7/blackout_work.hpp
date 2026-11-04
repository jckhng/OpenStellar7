#pragma once

#include "stellar7/indexed_pages.hpp"
#include "stellar7/palette_clock.hpp"

namespace stellar7 {

inline constexpr std::size_t blackoutPaletteSteps = 16;

enum class BlackoutDriver : std::uint16_t { modeX = 8 };
enum class BlackoutClip : std::uint8_t { disabled = 0, enabled = 1 };
enum class BlackoutPageLayout { compatibility, recoveredStartup };

struct BlackoutSequencerState {
    std::uint8_t index;
    std::uint8_t mapMask;
};

struct BlackoutSurfaceState {
    BlackoutDriver driver = BlackoutDriver::modeX;
    std::uint16_t scale = 0;
    std::uint16_t remap = 0;
    std::uint8_t rasterStyle = 1;
    std::uint8_t paletteEnabled = 1;
    BlackoutClip clipping = BlackoutClip::enabled;
    std::uint8_t firstColor = 0;
    std::uint8_t secondColor = 0;
    std::optional<BlackoutSequencerState> sequencer;
};

struct BlackoutWorkState {
    PaletteWorkState palette;
    BlackoutSurfaceState surface;
};

struct BlackoutPaletteTiming {
    NormalWorkTiming before;
    PaletteClockTiming palette;
    std::uint8_t intensity;
};

struct BlackoutClockTiming {
    BlackoutWorkState state;
    std::array<BlackoutPaletteTiming, blackoutPaletteSteps> steps;
    NormalWorkTiming clear;
};

struct BlackoutSourcePlan {
    std::array<std::uint64_t, blackoutPaletteSteps> beforePalette;
    std::vector<NormalWorkOperation> clear;
};

// Source dispatches shared by strict execution and the native sample adapter.
BlackoutSourcePlan blackoutSourcePlan(const BlackoutSurfaceState& surface, std::uint16_t frontPage,
    BlackoutPageLayout layout = BlackoutPageLayout::compatibility);

//9d4c entry through RET, including sixteen1b38/077a calls and6622/2e25 clear.
// Each source-return slice retains phase;7509 caller and earlier heap work excluded.
BlackoutClockTiming blackoutClockTiming(const SoundBlasterClockPhase& phase,
    const BlackoutWorkState& state, std::uint16_t frontPage, std::int64_t horizonCycles,
    BlackoutPageLayout layout = BlackoutPageLayout::compatibility);

// Full-front indexed effect only; other physical pages/unknown masks retain.
void applyBlackoutClear(IndexedPages& pages, std::uint16_t frontPage,
    BlackoutPageLayout layout = BlackoutPageLayout::compatibility);

}
