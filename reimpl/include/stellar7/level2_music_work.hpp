#pragma once

#include "stellar7/presentation_clock.hpp"

namespace stellar7 {
// Logical destination identity; physical campaign allocation remains synth slot2.
inline constexpr std::uint8_t level2MusicIdentity = 4;
inline constexpr std::size_t level2MusicBodyOffset = 36;
enum class Level2MusicChild { emptyStop, activation, directReset };
// Bind only after the root-owned6E6C destination provider, using its owned bytes.
SoundBlasterClockPhase bindLevel2MusicResource(const SoundBlasterClockPhase& phase,
    const std::vector<std::uint8_t>& ownedSequence);
std::vector<NormalWorkOperation> level2MusicOperations(Level2MusicChild child);
NormalWorkTiming level2MusicTiming(const SoundBlasterClockPhase& phase, Level2MusicChild child,
    std::int64_t horizonCycles = 120000);
ClockedPresentationWork renderLevel2Music(PresentationClock& clock,
    const SoundBlasterClockPhase& phase, Level2MusicChild child, std::int64_t horizonCycles = 120000);
// Root invokes entry only for the actual ContinueYes/cleanup path on current song4.
// Resource release/6E6C remain ordered native providers, with no guessed guest work.
enum class Level2RetryMusicChild { stop, emptyStop, activation, directReset };
SoundBlasterClockPhase beginLevel2RetryMusic(const SoundBlasterClockPhase& phase);
SoundBlasterClockPhase bindLevel2RetryMusicResource(const SoundBlasterClockPhase& phase,
    const std::vector<std::uint8_t>& ownedSequence);
std::vector<NormalWorkOperation> level2RetryMusicOperations(Level2RetryMusicChild child);
NormalWorkTiming level2RetryMusicTiming(const SoundBlasterClockPhase& phase, Level2RetryMusicChild child,
    std::int64_t horizonCycles = 120000);
ClockedPresentationWork renderLevel2RetryMusic(PresentationClock& clock,
    const SoundBlasterClockPhase& phase, Level2RetryMusicChild child, std::int64_t horizonCycles = 120000);
} //namespace stellar7
