#pragma once

#include "stellar7/normal_clock.hpp"

#include <array>

namespace stellar7 {

enum class SoundBlasterInitializationEffectKind { bankByte, scratchByte, operatorByte,
    rhythmMode, amplitudeDepth, vibratoDepth, noteSelect, waveformMask,
    oplRegister, ready, accumulator, operation2Entry };
enum class SoundBlasterInitializationHostServiceKind { portableOplCapability };

struct SoundBlasterInitializationEffect {
    SoundBlasterInitializationEffectKind kind;
    std::uint16_t index;
    std::uint16_t value;
    std::uint16_t sourcePc; // Relative to the loaded Sound Blaster driver.
    std::uint64_t afterDispatch;
};

struct SoundBlasterInitializationHostService {
    SoundBlasterInitializationHostServiceKind kind;
    std::uint16_t sourcePc;
    std::uint64_t afterDispatch;
};

struct SoundBlasterInitializationState {
    std::vector<std::uint8_t> bank;
    std::array<std::uint8_t, 13> scratch{};
    std::array<std::array<std::uint8_t, 14>, 18> operators{};
    bool rhythmMode = false;
    bool amplitudeDepth = false;
    bool vibratoDepth = false;
    bool noteSelect = false;
    std::uint16_t waveformMask = 0;
    bool ready = false;
    std::uint16_t accumulator = 0;
    std::uint16_t operation2Entry = 0x2e67;
};

struct SoundBlasterInitializationWork {
    std::vector<NormalWorkOperation> operations;
    std::vector<SoundBlasterInitializationEffect> effects;
    std::vector<SoundBlasterInitializationHostService> hostServices;
    SoundBlasterInitializationState state;
};

// First SBL2E67..2ECD success; dispatcher/wrappers belong to the caller.
// Whole2A0C capability service is explicitly native and untimed.
SoundBlasterInitializationWork soundBlasterInitializationOperations(
    const std::vector<std::uint8_t>& instrumentResource);

}  // namespace stellar7

