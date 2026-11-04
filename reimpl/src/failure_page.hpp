#pragma once

#include "stellar7/gameplay.hpp"
#include "stellar7/indexed_render.hpp"

namespace stellar7 {
IndexedScreen renderPlayableLevel2ViewportIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const EffectProjector& effectProjector = {}, const BeamProjectionSink& beamProjection = {});
//Private native provider; the public source renderer keeps its original domain.
IndexedScreen renderPlayableFailurePageIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const IndexedScreen& destination, Viewport backgroundClip);
IndexedScreen renderPlayableLevel2ViewportIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const EffectProjector& effectProjector, const BeamProjectionSink& beamProjection, std::int16_t detail);
IndexedScreen renderPlayableFailurePageIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const IndexedScreen& destination, Viewport backgroundClip, std::int16_t detail);
} //namespace stellar7
