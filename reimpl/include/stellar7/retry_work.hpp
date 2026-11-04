#pragma once

#include "stellar7/continue_work.hpp"
#include "stellar7/highscore.hpp"

namespace stellar7 {

enum class RetryWorkKind {
    ordinary,
    releasePaletteProvider,
    detachLevelResourceProvider,
    releaseDetachedResourceProvider,
    releaseMusic,
    stopPendingSound,
    selectScoreFontProvider,
    levelResourceBootstrapProvider,
    modelResourcesProvider,
    levelMusicResourcesProvider,
    modelReloadProvider,
    retainedConstructorCompletionProvider,
    loaderPages,
    musicActivation,
    palette,
    directReset,
    scoreHudProvider,
    retainedGroundInitializationProvider,
    statusHudProvider,
    controllerRebindProvider
};

struct RetryWorkSegment {
    RetryWorkKind kind = RetryWorkKind::ordinary;
    std::uint16_t sourceCs = 0x0824;
    std::uint16_t sourceEntry = 0;
    std::uint16_t sourceResume = 0;
    std::vector<NormalWorkOperation> operations;
    std::optional<PaletteRequest> palette;
    //Provider omissions are not zero-cost or strict guest elapsed-time claims.
    bool guestElapsedQualified = true;
};

struct Level1RetryProfile {
    ContinueOutcome decision = ContinueOutcome::pending;
    std::uint16_t levelIndex = 0;
    std::uint32_t score = 0;
    std::uint16_t remainingContinues = 0;
    std::uint16_t introduction = 0;
    //The caller supplies its CURRENT owned decoded table. Missing is unsupported.
    std::optional<HighscoreTable> currentHighscores;
    bool globalExitRequested = false;
    bool currentSequencePublished = true;
    std::int16_t selectedSystem = -1;
    std::array<std::int8_t,7> systemLevels{};
    std::array<std::int16_t,7> systemTimers{};
    std::uint16_t frontPage = 0;
    std::uint16_t backPage = 0;
    std::uint16_t cachePage = 0;
};

struct Level1RetryObservation {
    std::uint16_t levelIndex;
    std::uint16_t remainingContinues;
    std::int16_t armor;
    std::uint32_t score;
    std::uint16_t loopTick;
    std::uint16_t fadeTicks;
    std::uint16_t ability;
    std::array<std::int16_t,7> systemTimers;
    std::uint16_t introduction;
    std::uint16_t frontPage;
    std::uint16_t backPage;
    std::uint16_t cachePage;
    bool firstEntryDisplay;
    bool oldPresenterListEmpty;
    bool listRefreshRequested;
};

struct Level1RetryWork {
    std::vector<RetryWorkSegment> segments;
    Level1RetryObservation observation;
    //References into segments, preserving provider/child order for execution.
    std::size_t modelReloadSegment;
    std::size_t loaderPagesSegment;
    std::size_t controllerRebindSegment;
};

//040C rank-miss through BEFORE01D0, Level1 index0/nonnegative signed32 score.
//The CURRENT table must miss that actual input; equality misses. Qualifying
//scores retain the strict unsupported name-editor rejection. No table writes.
//Only after330A returns do source0417/041A clear score; observation.score is0.
//No model/RNG/page/audio/clock mutation. Calls existing retryLevel1 exactly once
//through the model provider; the ground step observes that construction, never reruns it.
//All child CALL instructions are owned here except activation5098..50AC and
//direct-reset01A8..01B1, whose existing full-scope helpers own their callers.
Level1RetryWork level1RankMissRetryOperations(const Level1RetryProfile& profile);

//Same040C..BEFORE01D0 caller, explicitly current Level2 index1/introduction0.
//The Level1-prefixed profile/result names describe the shared historical ABI.
//Current score/table rank before score clear;041E decrements1->0 and0475
//increments0->1. The observation is the final same-level loader return.
//Execute modelReloadProvider against the CURRENT Level2 physical pools, then
//retainedConstructorCompletionProvider with continuing failure-return RNG.
//Loader pages and music children require explicit retained Level2 retry
//ancestry; their existing source scopes do not authorize Level1/cold reuse.
//Nonzero introduction, rank hit, No/cancel/exhaustion remain unsupported.
Level1RetryWork level2RankMissRetryOperations(const Level1RetryProfile& profile);

} // namespace stellar7
