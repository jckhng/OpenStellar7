#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>

namespace stellar7 {
inline constexpr std::int32_t maximumPlayableJumpAltitude = 2530; //220 launch,10 gravity.
enum class SystemPanelStyle { unavailable, ready, active };
struct SystemBorder { std::size_t system; bool selected; };
struct SystemCopy { std::size_t system; };
struct SystemPanel { std::size_t system; SystemPanelStyle style; std::uint8_t remainingCharges; };
struct SystemAudio { std::uint16_t event; };
enum class DecoyStripMode { restore, visible };
struct SystemDecoyStrip { DecoyStripMode mode; };
enum class DecoyPaletteMode { decoy, restore };
struct SystemDecoyPalette { DecoyPaletteMode mode; };
using PlayableSystemEffect=std::variant<SystemBorder,SystemCopy,SystemPanel,SystemAudio,
    SystemDecoyStrip,SystemDecoyPalette>;
} //namespace stellar7
