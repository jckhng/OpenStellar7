#pragma once

#include "stellar7/audio_clock.hpp"

namespace stellar7 {

//Explicit native startup/menu authorization; never an original publication receipt.
//Root adds sticky bool phase.playableStartup and current bool phase.playableMusicOff.
inline bool playableStartupProgram(const SoundBlasterClockPhase& phase) {
    if (!phase.playableStartup || !phase.sequenceSelection || !phase.callback || !phase.callback->program
        || !phase.soundBlasterInitialization || !phase.soundBlasterInitialization->driverEnabled
        || !phase.callback->driverEnabled || !phase.timer || phase.timer->activeMask != 1
        || !phase.timerInstall || phase.timerInstall->stage != AudioClockTimerInstallStage::installed
        || !phase.devices || !phase.devices->fixedPitAndMaskedDma) {
        return false;
    }
    const auto& prepared = phase.sequenceSelection->prepared;
    if (prepared.domain != SoundBlasterCallbackDomain::sequence || !prepared.driverEnabled
        || !prepared.program || prepared.sequence.empty() || phase.sequenceSelection->songOrdinal > 2) {
        return false;
    }
    const auto& current = *phase.callback;
    if (phase.playableMusicOff) {
        return current.domain == SoundBlasterCallbackDomain::level2RetryNoHandle
            && !current.active && current.sequence.empty() && current.position == 0 && current.wait == 0;
    }
    if (current.domain == SoundBlasterCallbackDomain::sequence) {
        return current.sequence == prepared.sequence;
    }
    //An actual selected51A6 clear retains native allocation ownership after cold0->ON.
    return (current.domain == SoundBlasterCallbackDomain::noHandle
            || current.domain == SoundBlasterCallbackDomain::level2RetryNoHandle)
        && phase.sequenceSelection->beginCurrentCleared && !current.active
        && current.sequence.empty() && current.position == 0 && current.wait == 0;
}

inline bool playableStartupMenuProgram(const SoundBlasterClockPhase& phase) {
    return playableStartupProgram(phase) && phase.sequenceSelection->songOrdinal == 1
        && phase.sequenceSelection->menuEntryCountdownWritten
        && !phase.sequenceSelection->level1DirectResetCompleted;
}

inline bool playableUnpublishedLevel1Program(const SoundBlasterClockPhase& phase) {
    return playableStartupProgram(phase) && phase.playableMusicOff
        && phase.sequenceSelection->songOrdinal == 2
        && phase.sequenceSelection->level1DirectResetCompleted;
}

} // namespace stellar7
