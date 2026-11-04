#pragma once

#include "stellar7/render_work.hpp"
#include "stellar7/gameplay.hpp"
#include "stellar7/level1_loader_work.hpp"

namespace stellar7 {

// Native playable prerequisite: original settings51..82 retain the indicator
// and panorama; settings51..70 omit the cockpit and horizon overlays.
// The existing source-work entry points retain their qualified full-detail domain.
CockpitWork playableCockpitOperations(const CockpitWorkState& state,
    const CockpitWorkBitmaps& bitmaps);
HorizonExecutionWork playableHorizonExecution(const HorizonWorkState& state,
    const std::vector<IndexedBitmap>& frames);
HorizonExecutionWork playableLevel2HorizonExecution(const HorizonWorkState& state,
    const std::vector<IndexedBitmap>& frames);

//Explicit native full detail capability; bounded entry points above retain51..82.
CockpitWork playableFullCockpitOperations(const CockpitWorkState& state, const CockpitWorkBitmaps& bitmaps);
HorizonExecutionWork playableFullHorizonExecution(const HorizonWorkState& state,
    const std::vector<IndexedBitmap>& frames);
HorizonExecutionWork playableFullLevel2HorizonExecution(const HorizonWorkState& state,
    const std::vector<IndexedBitmap>& frames);
IndexedScreen renderFullDetailPlayableLevel2ViewportIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const EffectProjector& effectProjector, const BeamProjectionSink& beamProjection, std::int16_t detail);
IndexedScreen renderFullDetailPlayableFailurePageIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const IndexedScreen& destination, Viewport backgroundClip, std::int16_t detail);

enum class NativeLevel1BeginPages { ordinary, storyTransferred };
Level1LoaderPages planStoryLevel1LoaderPages(const GameplayAssets& assets, const GameplayModel& model,
    const CockpitPageState& pages, const IndexedPages& pixels, std::int16_t detail);

Level1LoaderPages planFullDetailPlayableLevel1LoaderPages(const GameplayAssets& assets, const GameplayModel& model,
    const CockpitPageState& pages, const IndexedPages& pixels, std::int16_t detail,
    Level1LoaderPageDomain domain = Level1LoaderPageDomain::freshBegin,
    NativeLevel1BeginPages beginPages = NativeLevel1BeginPages::ordinary);

} // namespace stellar7
