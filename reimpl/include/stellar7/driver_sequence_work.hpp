#pragma once

#include "stellar7/normal_clock.hpp"

namespace stellar7 {

enum class DriverSequenceEffectKind { owner, channelVoiceCount, channelProgram,
    channelCursor, channelDeferral, channelEnabled, voiceMarker, velocity, note,
    sounding, tuning, voiceAuxiliary, voiceWord, directResetWord, directMode,
    handleMode, handleStatus, handlePosition, channelCount, sequenceMode,
    descriptorOffset, wait, parseOperation, frequencyLatch, fade, fadeDivider,
    repeatPosition, accumulator };

struct DriverSequenceEffect {
    DriverSequenceEffectKind kind;
    std::uint16_t index;
    std::uint16_t value;
    std::uint16_t sourcePc;
    std::uint64_t afterDispatch;
};

struct DriverSequenceChannel {
    std::optional<std::uint16_t> voiceCount, program, cursor, deferral, enabled;
};

struct DriverSequenceVoice {
    std::optional<std::uint16_t> owner, marker, velocity, note, sounding, tuning, auxiliary;
    // Source record words F5/121/14D; unproven meanings remain structural.
    std::array<std::optional<std::uint16_t>, 3> words{};
};

struct DriverSequenceState {
    std::array<DriverSequenceChannel, 16> channels{};
    // Source reset also writes auxiliary record9; physical OPL voices remain0..8.
    std::array<DriverSequenceVoice, 10> voices{};
    std::array<std::array<std::optional<std::uint16_t>, 2>, 3> directResetWords{};
    std::optional<std::uint16_t> directMode, handleMode, handleStatus, handlePosition;
    std::optional<std::uint16_t> channelCount, sequenceMode, descriptorOffset;
    std::optional<std::uint16_t> wait, parseOperation, frequencyLatch, fade, fadeDivider;
    std::optional<std::uint16_t> repeatPosition, accumulator;
};

struct DriverSequenceWork {
    std::vector<NormalWorkOperation> operations;
    std::vector<DriverSequenceEffect> effects;
    // Unwritten fields are absent and must preserve the inherited live value.
    DriverSequenceState state;
};

// SBL206B..2160 including RET; excludes dispatcher and its CALL.
DriverSequenceWork soundBlasterDirectResetOperations();

// SBL17E5..19A3 type0, including its nested206B; excludes dispatcher/wrapper.
enum class DriverSequenceAdmission { freshChannels, inheritedChannels, successInheritedChannels };
DriverSequenceWork soundBlasterSequenceOperations(const std::vector<std::uint8_t>& sequence,
    DriverSequenceAdmission admission = DriverSequenceAdmission::freshChannels);

// Apply one qualified source store; preserves every absent field.
DriverSequenceWork driverSequenceEffectWork(const DriverSequenceEffect& effect);
SoundBlasterCallbackState applyDriverSequenceEffect(const SoundBlasterCallbackState& state,
    const DriverSequenceEffect& effect);

SoundBlasterCallbackState applyDriverSequenceCallbackState(
    const SoundBlasterCallbackState& incoming, const DriverSequenceWork& work);

// Prepared selected state for dry-run admission; publishing current remains separate.
SoundBlasterCallbackState soundBlasterSequenceCallbackState(
    const SoundBlasterCallbackState& noHandle, const std::vector<std::uint8_t>& sequence,
    const DriverSequenceWork& result);

}  // namespace stellar7
