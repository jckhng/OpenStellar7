#pragma once

#include "playable_startup_clock_domain.hpp"

#include <stdexcept>

namespace stellar7 {

//Full allocated descriptor remains in sequenceSelection.prepared; callback represents B94==0.
inline SoundBlasterCallbackState playableStartupUnpublishedCallback(const SoundBlasterCallbackState& prepared) {
    if (prepared.domain != SoundBlasterCallbackDomain::sequence || !prepared.program
        || !prepared.driverEnabled || prepared.sequence.empty()) {
        throw std::invalid_argument("native unpublished startup requires a real prepared descriptor");
    }
    auto result = prepared;
    result.domain = SoundBlasterCallbackDomain::level2RetryNoHandle;
    result.active = false;
    result.sequence.clear();
    result.position = 0;
    result.wait = 0;
    result.operation = 0x1e;
    return result;
}

inline bool playableStartupBeginReturned(const SoundBlasterClockPhase& phase) {
    if (!playableStartupMenuProgram(phase) || !phase.sequenceSelection->beginCountdownReturned
        || phase.timer->globalCount != 0) {
        return false;
    }
    return phase.playableMusicOff ? phase.sequenceSelection->beginEmptyCurrentReturned
        : phase.sequenceSelection->beginCurrentCleared;
}

inline bool playableStartupLevel1Program(const SoundBlasterClockPhase& phase) {
    return playableStartupProgram(phase) && phase.sequenceSelection->songOrdinal == 2
        && phase.sequenceSelection->level1DirectResetCompleted;
}

inline SoundBlasterClockPhase playableStartupLevel1Preparation(const SoundBlasterClockPhase& incoming,
    const SoundBlasterCallbackState& prepared) {
    if (!playableStartupBeginReturned(incoming)) {
        throw std::invalid_argument("native Level1 allocation requires its actual returned Begin");
    }
    auto phase = incoming;
    phase.callback = phase.playableMusicOff ? playableStartupUnpublishedCallback(prepared) : prepared;
    phase.sequenceSelection = AudioClockSequenceSelectionPhase{prepared};
    phase.sequenceSelection->songOrdinal = 2;
    return phase;
}

} // namespace stellar7
