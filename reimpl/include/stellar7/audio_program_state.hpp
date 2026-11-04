#pragma once

#include "stellar7/enemy20_audio_work.hpp"
#include "stellar7/bomb7_audio_work.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace stellar7 {

struct SoundBlasterVolumeRow {
    std::uint16_t volume;
    std::array<std::uint8_t, 64> values;
};

inline bool operator==(const SoundBlasterVolumeRow& first, const SoundBlasterVolumeRow& second) noexcept {
    return first.volume == second.volume && first.values == second.values;
}

inline bool operator!=(const SoundBlasterVolumeRow& first, const SoundBlasterVolumeRow& second) noexcept {
    return !(first == second);
}

struct SoundBlasterProgramVoice {
    std::uint8_t program = 0; // Source8C deferred-note marker, distinct from channel4C.
    std::uint8_t tuningMagnitude = 0;
    std::uint16_t operatorScale = 0;
    std::uint16_t operatorFallback = 0;
    std::uint16_t carrierScale = 0;
    std::uint16_t carrierFallback = 0;
    std::uint16_t operatorDisabled = 0;
};

struct SoundBlasterProgramState {
    std::vector<std::uint8_t> instrumentBank;
    std::vector<std::uint8_t> volumeTable;
    // Immutable resource-backed row reached through event6's caller-volume alias.
    std::optional<SoundBlasterVolumeRow> playerHitVolumeRow;
    std::optional<SoundBlasterEnemy20CallerWindow> enemy20CallerWindow;
    std::optional<SoundBlasterEnemy20McbByte> enemy20McbByte;
    std::optional<SoundBlasterBomb7CallerRow> bomb7CallerRow;
    std::array<std::uint8_t, 16> channelPrograms{};
    std::array<SoundBlasterProgramVoice, 9> voices{};
    std::array<std::array<std::uint8_t, 14>, 18> operators{};
    std::uint16_t handleStatus = 0;
    std::uint16_t repeatCount = 0xffff;
    std::uint16_t handleVolume = 15;
    std::uint16_t waveformMask = 32;
    std::uint16_t accumulator = 127;
    std::uint16_t fadeDivider = 20;
    std::uint16_t loopMarker = 0; // SourceB4 offset relative to the payload.
    std::uint8_t amplitudeDepth = 0;
    std::uint8_t vibratoDepth = 0;
    std::uint8_t noteSelect = 0;
};

}  // namespace stellar7
