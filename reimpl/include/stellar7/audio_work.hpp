#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <optional>

#include "stellar7/audio_program_state.hpp"

namespace stellar7 {

// Active unended sequence, mode0, positive wait and no fade. Expiring direct
// timers require separate voice/device work. Includes callback RETF, not IRQ CALLF.
std::uint64_t soundBlasterWaitDispatches(const std::array<std::uint16_t, 3>& directTimers);

struct SoundBlasterOplWork {
    std::uint64_t dispatches;
    std::uint64_t addressDispatches;
    std::uint64_t dataDispatches;
    std::uint64_t portReads;
    std::uint64_t portWrites;
};

// Resident writer includes RET, not caller CALL. Hardware delay is phase-dependent.
SoundBlasterOplWork soundBlasterOplWork();

enum class SoundBlasterFrequencyGate { Open, Suppressed };
enum class SoundBlasterLevelLatch { Unlatched, Latched };

struct SoundBlasterTimerWork {
    std::uint64_t dispatches;
    std::uint64_t portReads;
    std::uint64_t portWrites;
    std::array<std::uint16_t, 3> timers;
    std::uint8_t expiredMask;
};

// Operation8 timer helper includes RET, not caller CALL. Expiring voices must
// be untuned when the frequency gate is open. Device/PIC delays are excluded.
SoundBlasterTimerWork soundBlasterDirectTimerWork(
    const std::array<std::uint16_t, 3>& timers,
    SoundBlasterFrequencyGate gate,
    SoundBlasterLevelLatch latch
);

struct SoundBlasterKeyOffState {
    std::array<std::uint8_t, 9> channels;
    std::array<std::uint8_t, 9> notes;
    std::array<std::uint8_t, 9> tuning;
    std::array<std::uint8_t, 16> channelDeferrals;
    std::array<std::uint16_t, 3> directTimers;
};

struct SoundBlasterExplicitKeyOff {
    std::uint8_t channel;
    std::uint8_t note;
    std::uint8_t delay;
};

enum class SoundBlasterWorkKind { plain, frequencyLow, frequencyHigh, carrierLevel, registerWrite };
enum class SoundBlasterCallbackKind { wait, explicitKeyOff, explicitNoteOn, noHandle, programControl, programWait, successNoHandle, level2RetryNoHandle };
enum class SoundBlasterCallbackDomain { sequence, noHandle, successNoHandle, level2RetryNoHandle };
inline constexpr std::uint64_t soundBlasterNoHandleDispatches = 23 + 21 + 14;
inline constexpr std::uint16_t soundBlasterInitializationWriteCount = 158;

struct SoundBlasterWorkBlock {
    SoundBlasterWorkKind kind;
    std::uint64_t dispatches;
    int voice;
    std::uint8_t registerNumber = 0;
    std::uint8_t value = 0;
};

struct SoundBlasterKeyOffEventWork {
    int selectedVoice = -1;
    std::uint64_t parserDispatches = 0;
    std::uint64_t handlerDispatches = 0;
    std::uint64_t scanDispatches = 0;
    std::uint64_t deltaDispatches = 0;
    std::uint64_t eventStartDispatches = 0;
    std::uint64_t frequencyStartDispatches = 0;
};

struct SoundBlasterKeyOffWork {
    std::uint64_t dispatches = 0;
    std::uint64_t portReads = 0;
    std::uint64_t portWrites = 0;
    SoundBlasterTimerWork timerWork{};
    std::uint16_t returnedWait = 0;
    std::uint64_t consumedBytes = 0;
    std::vector<SoundBlasterKeyOffEventWork> events;
    std::vector<SoundBlasterWorkBlock> blocks;
};

// Active/unended mode0, no fade/wait: explicit 0x9n zero-velocity messages,
// simple deltas, nonexpiring timers, untuned/nondeferred matched voices.
// Includes callback RETF, not IRQ CALLF. OPL blocks include writer RET;
// preceding plain work includes its CALL. Hardware/scheduler charges excluded.
SoundBlasterKeyOffWork soundBlasterExplicitKeyOffWork(
    const SoundBlasterKeyOffState& state,
    const std::vector<SoundBlasterExplicitKeyOff>& events,
    SoundBlasterFrequencyGate gate,
    SoundBlasterLevelLatch latch
);

struct SoundBlasterNoteOnState {
    std::array<std::uint8_t, 9> channels;
    std::array<std::uint8_t, 9> sounding;
    std::array<std::uint8_t, 9> tuning;
    std::array<std::uint8_t, 16> channelEnabled;
    std::array<std::uint8_t, 16> channelVoiceCursors;
    std::array<std::uint16_t, 3> directTimers;
};

struct SoundBlasterExplicitNoteOn {
    std::uint8_t channel;
    std::uint8_t note;
    std::uint8_t velocity;
    std::uint8_t delay;
};

struct SoundBlasterNoteOnEventWork {
    int selectedVoice = -1;
    std::uint64_t eventStartDispatches = 0;
    std::uint64_t frequencyStartDispatches = 0;
};

struct SoundBlasterNoteOnWork {
    std::uint64_t dispatches = 0;
    std::uint64_t portReads = 0;
    std::uint64_t portWrites = 0;
    SoundBlasterTimerWork timerWork{};
    std::uint16_t returnedWait = 0;
    std::uint64_t consumedBytes = 0;
    std::array<std::uint8_t, 16> channelVoiceCursors{};
    std::vector<SoundBlasterNoteOnEventWork> events;
    std::vector<SoundBlasterWorkBlock> blocks;
};

// Positive explicit notes, simple deltas, no fade/expiry. Selects an inactive
// owned voice or the original no-owner cursor path; active voice stealing rejects.
SoundBlasterNoteOnWork soundBlasterExplicitNoteOnWork(
    const SoundBlasterNoteOnState& state,
    const std::vector<SoundBlasterExplicitNoteOn>& events,
    SoundBlasterFrequencyGate gate,
    SoundBlasterLevelLatch latch
);

struct SoundBlasterCallbackState {
    SoundBlasterKeyOffState voices{};
    std::array<std::uint8_t, 9> velocities{};
    std::array<std::uint8_t, 9> sounding{};
    std::array<std::uint8_t, 16> channelEnabled{};
    std::array<std::uint8_t, 16> channelVoiceCursors{};
    std::vector<std::uint8_t> sequence;
    std::size_t position = 0;
    std::uint16_t wait = 0;
    std::uint16_t fade = 0;
    std::uint8_t mode = 0;
    std::uint8_t operation = 2;
    std::uint8_t runningStatus = 0;
    bool active = true;
    SoundBlasterFrequencyGate gate = SoundBlasterFrequencyGate::Open;
    SoundBlasterLevelLatch latch = SoundBlasterLevelLatch::Unlatched;
    SoundBlasterCallbackDomain domain = SoundBlasterCallbackDomain::sequence;
    bool driverEnabled = true;
    std::optional<SoundBlasterProgramState> program;
};

struct SoundBlasterCallbackAdvance {
    SoundBlasterCallbackState state;
    SoundBlasterCallbackKind kind;
    std::uint64_t dispatches = 0;
    std::vector<SoundBlasterWorkBlock> blocks;
};

// Initialized driver with no selected handle and zero direct timers, or wait
// and ordered explicit key-off/positive explicit or running notes, including
// mixed groups and untuned active-owner replacement. Running key-off rejects;
// the explicit-note work API remains inactive-only.
// Returns the incoming-state-derived work and semantic state for the next IRQ.
SoundBlasterCallbackAdvance soundBlasterCallbackAdvance(const SoundBlasterCallbackState& state);

}  // namespace stellar7
