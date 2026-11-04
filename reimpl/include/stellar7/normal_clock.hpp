#pragma once

#include "stellar7/audio_clock.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace stellar7 {

enum class NormalWorkKind { instructions, string, repeatString, port8Write, port16Write,
    cli, sti, popFlagsEnabled, keyboardDataRead, keyboardControlRead, keyboardControlWrite,
    keyboardConsumeHead, keyboardStoreScan, timerInstallClearMask, timerInstallSaveBiosVector,
    timerInstallBiosReload, timerInstallBiosCount, timerInstallDivisor, timerInstallPitControl,
    timerInstallPitLow, timerInstallPitHigh, timerInstallPicMask, timerInstallVector, timerInstallComplete,
    timerCallbackReload, timerCallbackCount, timerCallbackOffset, timerCallbackSegment, timerCallbackActivate,
    soundBlasterOplWrite, soundBlasterDriverEnable, soundBlasterSequencePublish, timerGlobalCount, startupCountdownPoll,
    startupLogoEvent, startupLogoCountdown, startupLogoFrameReturn, startupLogoOnce, startupLogoEndPoll,
    startupLogoStop, startupMenuCountdown, startupCreditsCountdown, startupCreditsFirstInput, startupMenuEntryCountdown, startupMenuDecision, startupBeginFadeRequest, startupBeginCountdown,
    startupBeginWait, startupBeginStop, gameplayPrimaryFireEvent, gameplayEnemyFireEvent, gameplayPlayerHitEvent, startupMenuInputDecision, gameplayPlayerCombatEvent, failureExplosionEvent, level1RetryMusicStop, gameplayTargetHitEvent, gameplayBonusPickupEvent, gameplayArrivalEvent, successMusicEntryStop, successMusicActivate, successCountdownStore, successMusicReturnStop, gameplayGuardianFireEvent, gameplayArrivalStopEvent, gameplayGuardianDefeatEvent, level2MusicEmptyStop, level2MusicActivate, level2DirectReset, gameplayLevel2PrimaryFireEvent, gameplayLevel2TargetHitEvent, gameplaySolidContactEvent, gameplayLevel2EnemyFireEvent, gameplayLevel2PlayerCombatEvent, failureLevel2ExplosionEvent, level2RetryMusicStop, level2RetryMusicEmptyStop, level2RetryMusicActivate, level2RetryDirectReset, gameplayLevel2EnemyType20Event, gameplayLevel2SolidContactEvent,
    gameplayLevel2ArrivalEvent, gameplayLevel2ArrivalStopEvent, gameplayLevel2GuardianDefeatEvent,
    playableJoystickDebounceStore, playableJoystickDebouncePoll,
    playableIrqWaitStore, playableIrqWaitPoll, playableStoryWaitStore, playableStoryWaitPoll, startupLogoEventSuppressed, startupMenuReentryCountdown, playableSuccessCountdownStore,
    playableStartupLogoCountdown, playableStartupLogoWaitStore, playableStartupLogoEmptyReturn, playableStartupMenuCountdown, playableStartupCreditsCountdown, playableStartupCreditsInput, playableStartupMenuEntryCountdown, playableStartupBeginFadeRequest, playableStartupBeginEmptyReturn, playableMenuNoAttractDecision,
    playableAttractMenuDecision, playableAttractCountdownStore, playableAttractCountdownRead,
    playableAttractWaitPoll, playableAttractCreditsPoll, playableAttractLogoOnce, playableAttractLogoEndPoll, playableJoystickBriefingRefresh,
    playableStartupCreditsPoll, playableStartupCreditsSkip };

struct NormalWorkOperation {
    NormalWorkKind kind;
    std::uint64_t count;
    std::uint16_t value = 0;
};

struct ScheduledSoundBlasterCallback {
    std::int64_t cycleOffset;
    SoundBlasterCallbackAdvance advance;
    SoundBlasterCallbackClock timing;
};

enum class VgaServiceKind { scanStart, scanLine };

struct VgaServiceReceipt {
    VgaServiceKind kind;
    std::int64_t cycleOffset;
    double scheduledMilliseconds;
    double servicedMilliseconds;
    double frameStartMilliseconds;
    std::uint32_t realStart;
    std::uint32_t line;
    std::uint32_t linesTotal;
};

enum class KeyboardPort { data, control };

struct KeyboardPortReceipt {
    KeyboardPort port;
    std::int64_t cycleOffset;
    std::uint8_t value;
};

struct KeyboardIrqReceipt {
    std::uint8_t scan;
    SoundBlasterClockStep entry;
    SoundBlasterClockPhase entryPhase;
    SoundBlasterClockStep preIret;
    SoundBlasterClockPhase preIretPhase;
    SoundBlasterClockStep completed;
    SoundBlasterClockPhase completedPhase;
    std::uint8_t controlRead;
};

enum class SoundBlasterForegroundKind { oplWrite, driverEnable, sequencePublish, runtimeOplWrite, directState, repeatOnce,
    stopComplete, sequenceClear, fadeRequest, sequenceEnd, gameplayDirectState, gameplayEnemyDirectState, gameplayPlayerHitDirectState, gameplayPlayerCombatDirectState, gameplayTargetImpactDirectState, gameplaySystemBonusDirectState, gameplayArrivalDirectState, successStopComplete, successCurrentClear, successActivationState, successSequencePublish, gameplayGuardianDefeatDirectState, level2ActivationState, level2SequencePublish, level2ResetState, gameplayLevel2DirectState, gameplayLevel2TargetHitDirectState, gameplayEnemyType20DirectState, gameplaySolidContactDirectState, gameplayLevel2EnemyDirectState, gameplayLevel2PlayerCombatDirectState, failureLevel2PlayerHitDirectState, level2RetryStopComplete, level2RetryCurrentClear, level2RetryActivationState, level2RetrySequencePublish, level2RetryResetState, gameplayLevel2EnemyType20DirectState, gameplayLevel2SolidContactDirectState,
    gameplayLevel2ArrivalDirectState, gameplayLevel2ArrivalStopState, gameplayLevel2GuardianDefeatDirectState };

struct SoundBlasterForegroundEffect {
    SoundBlasterForegroundKind kind;
    std::int64_t cycleOffset;
    std::uint16_t value;
    std::shared_ptr<const SoundBlasterCallbackState> directState;
};

struct NormalWorkTiming {
    SoundBlasterClockStep total;
    SoundBlasterClockPhase phase;
    std::uint64_t stringIterations = 0;
    std::uint64_t stringRestarts = 0;
    std::vector<ScheduledSoundBlasterCallback> callbacks;
    std::vector<VgaServiceReceipt> vgaServices;
    std::vector<KeyboardPortReceipt> keyboardReads;
    std::vector<KeyboardIrqReceipt> keyboardIrqs;
    std::vector<SoundBlasterForegroundEffect> soundBlasterEffects;
    std::uint64_t countdownPolls = 0;
    std::optional<std::uint16_t> countdownLastRead;
    bool countdownReturned = false;
    std::uint64_t sequenceEndPolls = 0;
    std::optional<std::uint16_t> sequenceEndLastRead;
    bool sequenceEndReturned = false;
    std::optional<std::uint16_t> creditsCountdownRead;
    std::optional<std::uint16_t> menuDecisionCountdownRead;
};

// Fixed normal5000, REP cap4, ISA8/16 writes. Unsupported IRQ/device service rejects.
// Returns the retained phase before the next dispatch, without importing a boundary.
NormalWorkTiming normalWorkTiming(const SoundBlasterClockPhase& phase,
    const std::vector<NormalWorkOperation>& operations, std::int64_t horizonCycles);

// Physical packet enqueue at the current owned phase; no guest dispatch/time.
// Enabled IRQ1 service remains unsupported; this does not admit gameplay input.
SoundBlasterClockPhase queueUpKey(const SoundBlasterClockPhase& phase, KeyboardEdge edge);

}  // namespace stellar7
