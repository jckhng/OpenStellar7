#pragma once
#include "stellar7/gameplay.hpp"
#include "stellar7/presentation_clock.hpp"
#include "stellar7/level2_preloop_prefix_work.hpp"

namespace stellar7 {
struct NativeLevel2FrameRun {
    std::uint64_t firstSample = 0;
    std::uint16_t loopTick = 0;
    std::uint16_t returnedLoopTick = 0;
    bool firstFrame = false;
    GameplayInput input{};
    std::uint32_t prefixRngCalls = 0;
    std::uint32_t groundRngCalls = 0;
    std::uint32_t returnedRngCalls = 0;
    std::optional<Level2PreloopVisibilityReceipt> firstVisibility;
    std::vector<ClockedPresentationWork> work;
    std::optional<ClockedPageFlipWork> flip;
    std::vector<ClockedPaletteWork> palettes;
    std::vector<std::int16_t> samples;
    std::vector<ScoreFontProviderRead> scoreFontProviders;
    std::vector<Level1AudioEvent> audioEvents;
    std::size_t lampCalls=0;
};
} //namespace stellar7
