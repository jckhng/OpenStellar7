#pragma once

#include "stellar7/indexed_pages.hpp"
#include "stellar7/level1.hpp"
#include "stellar7/palette_clock.hpp"
#include "stellar7/resource_codec.hpp"

namespace stellar7 {
inline constexpr std::size_t playableDecoyPaletteColors = 128;
using PlayableDecoyPaletteBytes = std::array<std::uint8_t,
    playableDecoyPaletteColors * vgaPaletteComponents>;
enum class PlayableDecoyStrip { restore, visible };

//Semantic99-byte copy: portable allocation/list metadata is not a source byte.
void copyPlayableDecoy(Level1Entity& proxy, const Level1Entity& player) noexcept;
//Successful05D4 andAA26 copy only+08..+13, even when I is inactive.
void refreshPlayableDecoyPosition(Level1Entity& proxy, const Level1Entity& player) noexcept;
//0D23..0DB9: while active, recenter and normalize XY on every tail.
void recenterPlayableDecoy(Level1Entity& proxy, FixedPosition player) noexcept;
//Dereference at each OLD visit; AA26 can change the selected proxy mid-walk.
const Level1Entity& playableBehaviorTarget(const Level1Entity& player,
    const Level1Entity& proxy, std::int16_t decoyTimer) noexcept;
void refreshPlayableTarget(Level1Entity& actor, const Level1Entity& target) noexcept;

PlayableDecoyPaletteBytes playableDecoyPaletteBytes() noexcept;
//6922: opaque frame4 to BACK or FRONT->BACK, then BACK->CACHE; no FRONT write.
std::vector<IndexedPageEffect> playableDecoyStripEffects(PlayableDecoyStrip mode,
    const std::vector<IndexedBitmap>& artGroup3, std::uint16_t front,
    std::uint16_t back, std::uint16_t cache);
} //namespace stellar7
