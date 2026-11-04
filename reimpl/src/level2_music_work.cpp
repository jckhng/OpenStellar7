#include "stellar7/level2_music_work.hpp"
#include "stellar7/driver_sequence_work.hpp"
#include "stellar7/audio_program_work.hpp"
#include "stellar7/audio_direct_work.hpp"
#include <stdexcept>

namespace stellar7 {
SoundBlasterClockPhase bindLevel2MusicResource(const SoundBlasterClockPhase& phase,
    const std::vector<std::uint8_t>& ownedSequence) {
    if (!phase.successAudio || phase.successAudio->stage!=SuccessAudioStage::returned
        || !phase.callback || !phase.callback->program || !phase.timer || !phase.sequenceSelection
        || phase.sequenceSelection->songOrdinal!=3 || !phase.sequenceSelection->published
        || phase.callback->domain!=SoundBlasterCallbackDomain::successNoHandle
        || phase.callback->program->handleVolume!=15
        || phase.sequenceSelection->prepared.domain!=SoundBlasterCallbackDomain::sequence
        || !phase.sequenceSelection->prepared.active || phase.sequenceSelection->prepared.sequence.empty()) {
        throw std::invalid_argument("Level2 resource binding requires the completed retained success owner");
    }
    soundBlasterSequenceOperations(ownedSequence,DriverSequenceAdmission::inheritedChannels);
    auto result=phase;
    result.sequenceSelection->prepared.sequence=ownedSequence;
    result.sequenceSelection->songOrdinal=level2MusicIdentity;
    result.sequenceSelection->published=false;
    result.sequenceSelection->countdownWritten=false;
    result.sequenceSelection->level1DirectResetCompleted=false;
    return result;
}
std::vector<NormalWorkOperation> level2MusicOperations(const Level2MusicChild child) {
    switch(child) {
    case Level2MusicChild::emptyStop:return {{NormalWorkKind::level2MusicEmptyStop,1}};
    case Level2MusicChild::activation:return {{NormalWorkKind::level2MusicActivate,1}};
    case Level2MusicChild::directReset:return {{NormalWorkKind::level2DirectReset,1}};
    }
    throw std::invalid_argument("unsupported Level2 music child");
}
NormalWorkTiming level2MusicTiming(const SoundBlasterClockPhase& phase,const Level2MusicChild child,
    const std::int64_t horizonCycles) {
    return normalWorkTiming(phase,level2MusicOperations(child),horizonCycles);
}
ClockedPresentationWork renderLevel2Music(PresentationClock& clock,const SoundBlasterClockPhase& phase,
    const Level2MusicChild child,const std::int64_t horizonCycles) {
    return clock.renderWork(phase,level2MusicOperations(child),horizonCycles);
}
} //namespace stellar7

namespace stellar7 {
SoundBlasterClockPhase beginLevel2RetryMusic(const SoundBlasterClockPhase& phase) {
    if (!phase.callback || !phase.callback->program || !phase.timer || !phase.sequenceSelection
        || phase.callback->domain!=SoundBlasterCallbackDomain::sequence
        || phase.sequenceSelection->songOrdinal!=level2MusicIdentity || !phase.sequenceSelection->published
        || !phase.sequenceSelection->level1DirectResetCompleted
        || phase.sequenceSelection->prepared.sequence!=phase.callback->sequence
        || !phase.successAudio || phase.successAudio->stage!=SuccessAudioStage::returned
        || phase.interrupts!=AudioClockInterrupts::enabled
        || (phase.level2RetryMusic && phase.level2RetryMusic->stage!=AudioClockLevel2RetryMusicStage::reset)) {
        throw std::invalid_argument("Level2 retry entry requires its retained selected destination owner");
    }
    soundBlasterStopAdvance(*phase.callback,SoundBlasterStopProfile::retainedDirectTimers);
    auto result=phase;result.level2RetryMusic.emplace();return result;
}
SoundBlasterClockPhase bindLevel2RetryMusicResource(const SoundBlasterClockPhase& phase,
    const std::vector<std::uint8_t>& ownedSequence) {
    if (!phase.level2RetryMusic || phase.level2RetryMusic->stage!=AudioClockLevel2RetryMusicStage::emptyStopped
        || !phase.callback || phase.callback->domain!=SoundBlasterCallbackDomain::level2RetryNoHandle
        || !phase.callback->program || !phase.sequenceSelection || !phase.sequenceSelection->published
        || phase.sequenceSelection->songOrdinal!=level2MusicIdentity
        || ownedSequence!=phase.sequenceSelection->prepared.sequence) {
        throw std::invalid_argument("Level2 retry provider requires its stopped same-campaign owned resource");
    }
    soundBlasterSequenceOperations(ownedSequence,DriverSequenceAdmission::inheritedChannels);
    auto result=phase;
    result.sequenceSelection->prepared.sequence=ownedSequence;
    result.sequenceSelection->published=false;result.sequenceSelection->countdownWritten=false;
    result.sequenceSelection->level1DirectResetCompleted=false;
    result.level2RetryMusic->stage=AudioClockLevel2RetryMusicStage::resourceBound;
    return result;
}
std::vector<NormalWorkOperation> level2RetryMusicOperations(const Level2RetryMusicChild child) {
    switch(child) {
    case Level2RetryMusicChild::stop:return {{NormalWorkKind::level2RetryMusicStop,1}};
    case Level2RetryMusicChild::emptyStop:return {{NormalWorkKind::level2RetryMusicEmptyStop,1}};
    case Level2RetryMusicChild::activation:return {{NormalWorkKind::level2RetryMusicActivate,1}};
    case Level2RetryMusicChild::directReset:return {{NormalWorkKind::level2RetryDirectReset,1}};
    }
    throw std::invalid_argument("unsupported Level2 retry music child");
}
NormalWorkTiming level2RetryMusicTiming(const SoundBlasterClockPhase& phase,const Level2RetryMusicChild child,
    const std::int64_t horizonCycles) {
    return normalWorkTiming(phase,level2RetryMusicOperations(child),horizonCycles);
}
ClockedPresentationWork renderLevel2RetryMusic(PresentationClock& clock,const SoundBlasterClockPhase& phase,
    const Level2RetryMusicChild child,const std::int64_t horizonCycles) {
    return clock.renderWork(phase,level2RetryMusicOperations(child),horizonCycles);
}
} //namespace stellar7
