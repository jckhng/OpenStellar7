#pragma once

#include "stellar7/render_work.hpp"

namespace stellar7 {

// Flat4C46 with signed steering[-512,512] and speed[-200,200], including resets.
// Shares the recovered execution with the narrower groundClippedExecution API.
GroundExecutionWork groundSteeringExecution(const GroundControlState& state, const StellarRng& rng,
    const std::array<IndexedBitmap, 8>& bitmaps, Viewport viewport);

//Same4C46 owner with Level1 group4/frames7..14's8-wide source-sprite thin crops.
//Legacy execution APIs retain their existing clipping admission.
GroundExecutionWork level1GroundSpriteExecution(const GroundControlState& state,const StellarRng& rng,
    const std::array<IndexedBitmap,8>& bitmaps,Viewport viewport);

//Same4C46 owner with Level2 group4/frames15..22:24x6 and8x1 thin source crops.
//The original descriptor and payload are validated before the private crop view.
GroundExecutionWork level2GroundSpriteExecution(const GroundControlState& state,const StellarRng& rng,
    const std::array<IndexedBitmap,8>& bitmaps,Viewport viewport);

} // namespace stellar7
