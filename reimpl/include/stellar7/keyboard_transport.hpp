#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace stellar7 {

inline constexpr std::size_t keyboardBufferWords = 96;
// Preserve the source float before promotion into the clock's event time.
inline constexpr float keyboardTransferDelayMilliseconds = 0.300f;
inline constexpr std::uint32_t keyboardRepeatPauseMilliseconds = 500;
inline constexpr std::uint32_t keyboardRepeatRateMilliseconds = 33;

// Certifies VGA, active scanning, Set1/translation, IRQ1 enabled, gate2 off,
// no reset, 8042/AUX response, command, pending host key or held modifier.
enum class KeyboardTransportDomain { unsupported, normalVgaSet1Up };
enum class KeyboardEdge { press, release };
enum class KeyboardTransportKey : std::uint16_t { none = 0, up = 84 };

struct KeyboardTransportState {
    KeyboardTransportDomain domain = KeyboardTransportDomain::unsupported;
    std::array<std::uint16_t, keyboardBufferWords> buffer{};
    std::uint32_t position = 0;
    std::uint32_t used = 0;
    std::uint8_t port60Data = 0;
    bool port60Pending = false;
    bool scheduled = false;
    KeyboardTransportKey repeatKey = KeyboardTransportKey::none;
    std::uint32_t repeatWait = 0;
    std::uint32_t repeatPause = keyboardRepeatPauseMilliseconds;
    std::uint32_t repeatRate = keyboardRepeatRateMilliseconds;
    std::uint8_t port61Data = 0;
};

struct KeyboardTransportIntent {
    bool scheduleTransfer = false;
    bool raiseIrq1 = false;
    bool lowerIrq1 = false;
};

struct KeyboardTransportChange {
    KeyboardTransportState state;
    KeyboardTransportIntent intent;
};

struct KeyboardTransportRead {
    KeyboardTransportChange change;
    std::uint8_t value;
};

// No CPU charge, PIC mutation or absolute deadline belongs to this driver.
// Root must bind domain certification to the qualified initial full profile.
KeyboardTransportChange keyboardUpEdge(const KeyboardTransportState& state, KeyboardEdge edge);
KeyboardTransportChange keyboardTransfer(const KeyboardTransportState& state);
KeyboardTransportRead keyboardRead60(const KeyboardTransportState& state);
std::uint8_t keyboardRead61(const KeyboardTransportState& state, double fullGuestMilliseconds);
KeyboardTransportChange keyboardWrite61(const KeyboardTransportState& state, std::uint8_t value);
KeyboardTransportChange keyboardTick(const KeyboardTransportState& state);

} // namespace stellar7
