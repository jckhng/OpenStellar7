#pragma once

#include "stellar7/level1.hpp"

#include <array>
#include <optional>

namespace stellar7 {
inline constexpr std::size_t playableBombFixedRecordCount=16; //4506 DS45B0..45CE.

//Caller owns the actual FE pool and source49F8 membership.
//Unknown history is not equivalent to source full-pool failure.
std::optional<std::size_t> playableBombSlot(
    const std::array<Level1Entity,playableBombFixedRecordCount>& records,
    const std::array<bool,playableBombFixedRecordCount>& known);

//0A25 reads beyond the logical ordinary point table into adjacent immutable DS words.
std::uint16_t playableBombContactScore(std::int16_t originalType);

Level1Entity preparePlayableBombActor(const Level1Entity& retained,const Level1Entity& player,
    std::uint32_t model43Radius);
} //namespace stellar7
