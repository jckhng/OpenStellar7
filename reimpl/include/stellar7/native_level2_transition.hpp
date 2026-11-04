#pragma once
#include "stellar7/recovered_level2_model.hpp"
#include "stellar7/score_work.hpp"
#include "stellar7/presentation_clock.hpp"

namespace stellar7 {
struct NativeLevel2TransitionStage {
    RetryWorkKind kind;
    std::uint16_t sourceEntry;
    std::uint64_t sampleEnd;
};
struct NativeLevel2TransitionRun {
    std::uint64_t firstSample = 0;
    std::uint16_t sourceLoopTick = 0;
    std::uint64_t sourceGlobalTick = 0;
    std::uint64_t destinationGlobalTick = 0;
    Level1TransitionWork control;
    std::vector<ClockedPresentationWork> work;
    std::vector<ClockedPaletteWork> palettes;
    //Native provider intervals remain unqualified, not zero-time guest claims.
    std::vector<RetryWorkSegment> providers;
    std::vector<Level2SelectorReleaseReceipt> selectorReleases;
    std::vector<ScoreFontProviderRead> scoreFontProviders;
    std::vector<std::size_t> projectionResetKeys;
    //Diagnostic return receipt only; GameplayModel remains the canonical owner.
    RecoveredLevel2PreloopWork preloop;
    std::vector<NativeLevel2TransitionStage> stages;
    std::vector<std::int16_t> samples;
};
} //namespace stellar7
