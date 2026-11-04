#pragma once

#include "stellar7/audio_work.hpp"

namespace stellar7 {

// Owned retained Level1 sequence only; admits FC and its next0494 FFFF restart.
// Reuses the selected program, channel, voice, operator and direct-timer owners.
SoundBlasterCallbackAdvance soundBlasterLevel1CallbackAdvance(const SoundBlasterCallbackState& state);

// Caller proves retained logical destination4 and its actual owned Level2 resource.
// Same FC/restart kernel; default callback and Level1 ownership stay unchanged.
SoundBlasterCallbackAdvance soundBlasterLevel2CallbackAdvance(const SoundBlasterCallbackState& state);
// Explicit native campaign providers retain either the selected MIDI owner or
// its deselected program/direct timers while scene speech is active.
SoundBlasterCallbackAdvance soundBlasterPlayableCallbackAdvance(const SoundBlasterCallbackState& state);

}  // namespace stellar7
