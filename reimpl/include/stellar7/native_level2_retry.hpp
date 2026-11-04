#pragma once
#include "stellar7/native_level2_transition.hpp"
namespace stellar7 {
//Current retry return receipt only; the original success transition is preserved.
struct NativeLevel2RetryRun {
    std::uint64_t firstSample=0;
    std::uint16_t sourceLoopTick=0;
    std::uint64_t sourceGlobalTick=0;
    std::uint64_t destinationGlobalTick=0;
    Level1RetryWork control;
    RecoveredLevel2PreloopWork preloop;
    std::vector<ClockedPresentationWork> work;
    std::vector<ClockedPaletteWork> palettes;
    std::vector<RetryWorkSegment> providers;
    std::vector<Level2SelectorReleaseReceipt> selectorReleases;
    std::vector<ScoreFontProviderRead> scoreFontProviders;
    std::vector<std::size_t> projectionResetKeys;
    std::vector<NativeLevel2TransitionStage> stages;
    std::vector<std::int16_t> samples;
};
} //namespace stellar7
