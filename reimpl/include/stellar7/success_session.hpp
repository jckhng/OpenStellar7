#pragma once

#include "stellar7/guardian_reward_work.hpp"
#include "stellar7/radar_state.hpp"
#include "stellar7/render_work.hpp"
#include "stellar7/sight_work.hpp"
#include "stellar7/success_control_work.hpp"
#include "stellar7/success_palette_step.hpp"

namespace stellar7 {

// Model-owned plan/commit: old list, actual callbacks and presentation fields.
// Operations retain the existing helpers' ordinary caller counts (1/2/1).
struct SuccessDisplayWork {
    CockpitWork cockpit;
    SightWork sight;
    RadarWork radar;
    RadarState radarState;
};

struct NativeSuccessState {
    SuccessControlState control;
    Level1RewardSuccessChild caller;
    std::optional<SuccessPaletteResidentCursor> paletteContinuation;
};

struct NativeSuccessRun {
    std::uint64_t firstSample = 0;
    bool entered = false;
    bool finished = false;
    bool suspended = false;
    NativeSuccessState state;
    std::vector<SuccessControlChild> children;
    std::vector<ClockedPresentationWork> work;
    std::vector<ClockedSuccessPaletteWork> palettes;
    std::vector<std::int16_t> samples;
};

} // namespace stellar7
