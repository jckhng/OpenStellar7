#pragma once

#include "stellar7/level1_frame.hpp"

namespace stellar7 {

//Complete current49F8/late45D2 composition after ordinary-slot reuse. A live
//Type11 is optional; all existing per-record, FD/FC and cache checks remain.
//Requires retainedRoster allocation and allowRetiredInitial membership. Legacy
//frame entry points retain their initial-lift requirement.
Level1CombatFramePlan planLevel1RetainedRosterFrame(const GameplayAssets& assets,
    const Level1ProjectileRenderState& frame, const Level1CombatRenderContext& context,
    Level1PresentationState presentation, std::int16_t armor,
    const CockpitSettings& settings, const CockpitPageState& pages,
    const RadarState& radar, const IndexedPages& pixels);

} // namespace stellar7
