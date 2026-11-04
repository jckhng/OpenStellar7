#pragma once

#include "stellar7/audio_clock.hpp"
#include "stellar7/level2_music_work.hpp"

namespace stellar7 {

inline bool retainedLevel1Program(const SoundBlasterClockPhase& phase) {
    return phase.sequenceSelection && phase.sequenceSelection->published
        && phase.sequenceSelection->songOrdinal == 2
        && phase.sequenceSelection->level1DirectResetCompleted
        && phase.callback && phase.callback->domain == SoundBlasterCallbackDomain::sequence
        && phase.callback->program;
}

inline bool retainedLevel2Program(const SoundBlasterClockPhase& phase) {
    return phase.sequenceSelection && phase.sequenceSelection->published
        && phase.sequenceSelection->songOrdinal == level2MusicIdentity
        && phase.sequenceSelection->level1DirectResetCompleted
        && phase.successAudio && phase.successAudio->stage == SuccessAudioStage::returned
        && phase.callback && phase.callback->domain == SoundBlasterCallbackDomain::sequence
        && phase.callback->program;
}

inline bool retainedLevel2RetryPaletteProgram(const SoundBlasterClockPhase& phase) {
    return phase.level2RetryMusic && phase.level2RetryMusic->stage == AudioClockLevel2RetryMusicStage::playing
        && phase.sequenceSelection && phase.sequenceSelection->published
        && phase.sequenceSelection->songOrdinal == level2MusicIdentity
        && !phase.sequenceSelection->level1DirectResetCompleted
        && phase.callback && phase.callback->domain == SoundBlasterCallbackDomain::sequence
        && phase.callback->program && phase.sequenceSelection->prepared.sequence == phase.callback->sequence
        && phase.successAudio && phase.successAudio->stage == SuccessAudioStage::returned;
}

inline std::int64_t retainedCampaignPaletteHorizon(const SoundBlasterClockPhase& phase,
    const std::int64_t ordinaryHorizon) {
    return retainedLevel1Program(phase) || retainedLevel2Program(phase) || retainedLevel2RetryPaletteProgram(phase)
        ? 2 * ordinaryHorizon : ordinaryHorizon;
}

inline std::int64_t retainedLevel1PaletteHorizon(const SoundBlasterClockPhase& phase,
    const std::int64_t ordinaryHorizon) {
    return retainedLevel1Program(phase) ? 2 * ordinaryHorizon : ordinaryHorizon;
}

} // namespace stellar7
