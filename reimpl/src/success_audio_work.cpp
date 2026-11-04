#include "stellar7/success_audio_work.hpp"
#include "stellar7/driver_sequence_work.hpp"
#include <stdexcept>

namespace stellar7 {
SoundBlasterClockPhase bindSuccessAudioResource(const SoundBlasterClockPhase& phase,
    const std::vector<std::uint8_t>& ownedSequence) {
    if (phase.successAudio || !phase.callback || !phase.callback->program || !phase.timer
        || !phase.sequenceSelection || phase.sequenceSelection->songOrdinal != 2
        || !phase.sequenceSelection->published || !phase.sequenceSelection->level1DirectResetCompleted
        || phase.callback->domain != SoundBlasterCallbackDomain::sequence
        || phase.callback->program->handleVolume != 15 || phase.callback->program->repeatCount != 0xffff) {
        throw std::invalid_argument("success resource binding requires retained Level1 and loaded-volume15");
    }
    soundBlasterSequenceOperations(ownedSequence,DriverSequenceAdmission::successInheritedChannels);
    auto result = phase;
    result.successAudio = AudioClockSuccessAudioPhase{ownedSequence};
    return result;
}
std::vector<NormalWorkOperation> successAudioOperations(const SuccessAudioChild child) {
    switch (child) {
    case SuccessAudioChild::entryStop: return {{NormalWorkKind::successMusicEntryStop,1}};
    case SuccessAudioChild::activation: return {{NormalWorkKind::successMusicActivate,1}};
    case SuccessAudioChild::countdownStore: return {{NormalWorkKind::successCountdownStore,1,successRawCountdown}};
    case SuccessAudioChild::returnStop: return {{NormalWorkKind::successMusicReturnStop,1}};
    }
    throw std::invalid_argument("unsupported success audio child");
}
NormalWorkTiming successAudioTiming(const SoundBlasterClockPhase& phase,
    const SuccessAudioChild child, const std::int64_t horizonCycles) {
    return normalWorkTiming(phase,successAudioOperations(child),horizonCycles);
}
ClockedPresentationWork renderSuccessAudio(PresentationClock& clock,
    const SoundBlasterClockPhase& phase, const SuccessAudioChild child,
    const std::int64_t horizonCycles) {
    return clock.renderWork(phase,successAudioOperations(child),horizonCycles);
}
} // namespace stellar7
