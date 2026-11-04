#pragma once

#include "stellar7/gameplay.hpp"

namespace stellar7 {

//2184(1): complete28A0 sky/ground/stars followed by2A1A perspective grid.
//Pixels only; restore this BEFORE preview models for every model page, evenindex0.
std::vector<IndexedPageEffect> briefingStripEffects(std::int16_t index,
    const GameplayAssets& assets, std::uint16_t backPage);

} // namespace stellar7
