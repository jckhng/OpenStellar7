#pragma once

#include "stellar7/audio_work.hpp"
#include "stellar7/driver_sequence_work.hpp"

namespace stellar7 {

//Bounded native parser domain; the first SNG1 loop group contains37 messages.
inline constexpr std::size_t soundBlasterProgramEventLimit = 64;

// Bounded mode0 B/C/E/9 group or positive wait, direct expiry and source fade.
// Includes terminal repeat0/cursor/wait retention and the next0494 reentry.
// Explicit FC/once EOF is admitted; repeating EOF remains unsupported.
// Uses owned immutable bank/volume data and retained voice/latch storage.
SoundBlasterCallbackAdvance soundBlasterProgramCallbackAdvance(const SoundBlasterCallbackState& state);

SoundBlasterProgramState soundBlasterProgramInitialState(
    const std::vector<std::uint8_t>& bank, const std::vector<std::uint8_t>& volume,
    const std::array<std::array<std::uint8_t, 14>, 18>& operators,
    const DriverSequenceState& sequenceState);

SoundBlasterProgramState soundBlasterProgramSequenceState(
    const SoundBlasterProgramState& incoming, const DriverSequenceState& sequenceState);

// Success-only0494 no-current branch, including1C6B retained direct timers.
SoundBlasterCallbackAdvance soundBlasterSuccessNoHandleAdvance(const SoundBlasterCallbackState& state);
SoundBlasterCallbackState soundBlasterSuccessDeselectState(const SoundBlasterCallbackState& state);

// Same0494 no-current/1C6B body, with explicit retained Level2 retry ownership.
SoundBlasterCallbackAdvance soundBlasterLevel2RetryNoHandleAdvance(const SoundBlasterCallbackState& state);
SoundBlasterCallbackState soundBlasterLevel2RetryDeselectState(const SoundBlasterCallbackState& state);

//206B partial writes; preserves the selected handle, markers, velocities and operators.
SoundBlasterCallbackState soundBlasterDirectResetState(const SoundBlasterCallbackState& incoming);
SoundBlasterCallbackState soundBlasterPlayableDirectResetState(const SoundBlasterCallbackState& incoming);

}  // namespace stellar7
