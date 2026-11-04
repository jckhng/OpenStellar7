#pragma once

#include "stellar7/audio_clock.hpp"

namespace stellar7 {
inline constexpr std::uint16_t menuCountdownRawIrqs=30000;
inline bool retainedMenuState(const SoundBlasterClockPhase& phase, const SoundBlasterCallbackState& callback) {
    return phase.sequenceSelection
        && phase.sequenceSelection->songOrdinal == 1
        && phase.sequenceSelection->menuEntryCountdownWritten
        && !phase.sequenceSelection->level1DirectResetCompleted
        && callback.domain == SoundBlasterCallbackDomain::sequence && callback.program
        && phase.sequenceSelection->prepared.sequence == callback.sequence;
}

inline bool retainedMenuProgram(const SoundBlasterClockPhase& phase) {
    return phase.callback && retainedMenuState(phase,*phase.callback)
        && phase.sequenceSelection->published && !phase.sequenceSelection->beginCurrentCleared;
}

inline bool retainedMenuPrediction(const SoundBlasterClockPhase& completed, const SoundBlasterCallbackState& live) {
    if (!retainedMenuState(completed,live)) {return false;}
    const auto& selection=*completed.sequenceSelection;
    //A packet can contain a selected-menu IRQ before its final51BD current clear.
    //Only prediction uses this completed receipt; Clock always checks live state.
    return selection.beginCurrentCleared ? selection.beginCountdownReturned : selection.published;
}

inline bool retainedMenuEof(const SoundBlasterCallbackState& state) {
    constexpr std::uint16_t sequenceHeaderBytes=34;
    return state.domain==SoundBlasterCallbackDomain::sequence && state.driverEnabled && state.program
        && !state.active && state.program->handleStatus==0xffff && state.program->repeatCount==0xffff
        && state.wait==0 && state.runningStatus==0xfc && state.operation==2
        && state.program->loopMarker>=sequenceHeaderBytes
        && state.position==static_cast<std::size_t>(state.program->loopMarker)+2
        && state.position<state.sequence.size();
}

//Private retained SNG1 admission; caller proves menu publication/resource owner.
SoundBlasterCallbackAdvance soundBlasterMenuCallbackAdvance(const SoundBlasterCallbackState& state);
} // namespace stellar7
