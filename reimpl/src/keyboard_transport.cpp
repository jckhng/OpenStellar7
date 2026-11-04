#include "stellar7/keyboard_transport.hpp"

#include <cmath>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::uint16_t extendedPrefix = 0xe0;
constexpr std::uint16_t upMakeScan = 0x48;
constexpr std::uint16_t breakScanBit = 0x80;
constexpr std::uint8_t port61LowMask = 0x0f;
constexpr std::uint8_t port61PitOutputBit = 0x20;
constexpr std::uint8_t port61RefreshBit = 0x10;
constexpr double refreshPeriodMilliseconds = 0.030;
constexpr double refreshHighThresholdMilliseconds = 0.015;
constexpr std::uint32_t upPacketWords = 2;

void validate(const KeyboardTransportState& state) {
    if (state.domain != KeyboardTransportDomain::normalVgaSet1Up
        || state.position >= keyboardBufferWords || state.used > keyboardBufferWords
        || state.repeatPause != keyboardRepeatPauseMilliseconds
        || state.repeatRate != keyboardRepeatRateMilliseconds
        || (state.repeatKey != KeyboardTransportKey::none && state.repeatKey != KeyboardTransportKey::up)
        || (state.repeatKey == KeyboardTransportKey::none && state.repeatWait != 0)
        || (state.repeatKey == KeyboardTransportKey::up
            && (state.repeatWait == 0 || state.repeatWait > state.repeatPause))
        || (state.port61Data & port61LowMask) != 0
        || (state.scheduled && (state.used == 0 || state.port60Pending))) {
        throw std::invalid_argument("unsupported fixed Up keyboard transport state");
    }
    for (std::uint32_t offset = 0; offset < state.used; ++offset) {
        const auto scan = state.buffer[(state.position + offset) % keyboardBufferWords];
        if (scan != extendedPrefix && scan != upMakeScan && scan != (upMakeScan | breakScanBit)) {
            throw std::invalid_argument("unsupported active keyboard packet byte");
        }
    }
}

void append(KeyboardTransportState& state, const std::uint16_t scan) {
    const auto slot = (state.position + state.used) % keyboardBufferWords;
    state.buffer[slot] = scan;
    ++state.used;
}

void schedule(KeyboardTransportChange& change) {
    if (!change.state.scheduled && !change.state.port60Pending && change.state.used != 0) {
        change.state.scheduled = true;
        change.intent.scheduleTransfer = true;
    }
}
}

KeyboardTransportChange keyboardUpEdge(const KeyboardTransportState& state, const KeyboardEdge edge) {
    validate(state);
    if (edge != KeyboardEdge::press && edge != KeyboardEdge::release) {
        throw std::invalid_argument("unsupported keyboard edge");
    }
    if (state.used > keyboardBufferWords - upPacketWords) {
        // The source clears an overflowing buffer; that branch is not admitted.
        throw std::invalid_argument("Up packet exceeds admitted keyboard buffer capacity");
    }
    KeyboardTransportChange change{state, {}};
    auto& next = change.state;
    if (edge == KeyboardEdge::press) {
        next.repeatWait = next.repeatKey == KeyboardTransportKey::up ? next.repeatRate : next.repeatPause;
        next.repeatKey = KeyboardTransportKey::up;
    } else if (next.repeatKey == KeyboardTransportKey::up) {
        next.repeatKey = KeyboardTransportKey::none;
        next.repeatWait = 0;
    }
    append(next, extendedPrefix);
    schedule(change); // AddBuffer schedules immediately after the first byte.
    append(next, edge == KeyboardEdge::press ? upMakeScan : upMakeScan | breakScanBit);
    return change;
}

KeyboardTransportChange keyboardTransfer(const KeyboardTransportState& state) {
    validate(state);
    if (!state.scheduled || state.used == 0 || state.port60Pending) {
        throw std::invalid_argument("keyboard transfer outside scheduled pending-free domain");
    }
    KeyboardTransportChange change{state, {}};
    auto& next = change.state;
    next.scheduled = false;
    next.port60Data = static_cast<std::uint8_t>(next.buffer[next.position]);
    next.port60Pending = true;
    next.position = static_cast<std::uint32_t>((next.position + 1) % keyboardBufferWords);
    --next.used;
    change.intent.raiseIrq1 = true;
    return change;
}

KeyboardTransportRead keyboardRead60(const KeyboardTransportState& state) {
    validate(state);
    KeyboardTransportChange change{state, {}};
    change.state.port60Pending = false;
    change.intent.lowerIrq1 = true;
    schedule(change);
    return {change, state.port60Data};
}

std::uint8_t keyboardRead61(const KeyboardTransportState& state, const double fullGuestMilliseconds) {
    validate(state);
    if (!std::isfinite(fullGuestMilliseconds) || fullGuestMilliseconds < 0) {
        throw std::invalid_argument("unsupported keyboard port61 guest time");
    }
    const auto refresh = std::fmod(fullGuestMilliseconds, refreshPeriodMilliseconds)
        > refreshHighThresholdMilliseconds ? port61RefreshBit : 0;
    return static_cast<std::uint8_t>((state.port61Data & port61LowMask) | port61PitOutputBit | refresh);
}

KeyboardTransportChange keyboardWrite61(const KeyboardTransportState& state, const std::uint8_t value) {
    validate(state);
    if ((value & port61LowMask) != 0) {
        throw std::invalid_argument("keyboard port61 write changes admitted gate/speaker/low bits");
    }
    KeyboardTransportChange change{state, {}};
    change.state.port61Data = value;
    return change;
}

KeyboardTransportChange keyboardTick(const KeyboardTransportState& state) {
    validate(state);
    if (state.repeatWait == 1) {
        return keyboardUpEdge(state, KeyboardEdge::press);
    }
    KeyboardTransportChange change{state, {}};
    if (change.state.repeatWait != 0) {
        --change.state.repeatWait;
    }
    return change;
}

} // namespace stellar7
