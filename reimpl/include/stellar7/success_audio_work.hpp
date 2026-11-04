#pragma once

#include "stellar7/presentation_clock.hpp"

namespace stellar7 {

enum class SuccessAudioChild { entryStop, activation, countdownStore, returnStop };
inline constexpr std::uint16_t successRawCountdown = 0x03e0;

//Earlier0086 native resource provider: binds existing decoded SSM0, no guest
//allocation/time claim. Driver/voices/timer remain their existing single owners.
SoundBlasterClockPhase bindSuccessAudioResource(const SoundBlasterClockPhase& phase,
    const std::vector<std::uint8_t>& ownedSequence);

//51A6 callees exclude outer6F60/7204 CALLF. Activation owns6F65..6F79
//including02E6/driver; countdown owns exactly7072 and its atomic source store.
std::vector<NormalWorkOperation> successAudioOperations(SuccessAudioChild child);
NormalWorkTiming successAudioTiming(const SoundBlasterClockPhase& phase,
    SuccessAudioChild child, std::int64_t horizonCycles = 120000);
ClockedPresentationWork renderSuccessAudio(PresentationClock& clock,
    const SoundBlasterClockPhase& phase, SuccessAudioChild child,
    std::int64_t horizonCycles = 120000);

} // namespace stellar7
