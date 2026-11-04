#pragma once

#include "stellar7/gameplay.hpp"
#include "stellar7/cockpit_state.hpp"
#include "stellar7/indexed_pages.hpp"
#include "stellar7/radar_state.hpp"
#include "stellar7/sight_work.hpp"
#include "stellar7/world_scene_work.hpp"
#include "stellar7/combat_scene_work.hpp"

namespace stellar7 {

enum class Level2FrameDomain { retainedForward, flatControls };

struct Level2FramePlan {
    GroundExecutionWork ground;
    WorldViewSetupWork world;
    WorldSceneWork scene;
    SightWork sight;
    CockpitWork cockpit;
    RadarWork radar;
    std::array<IndexedBitmap, 8> groundBitmaps;
    CockpitWorkBitmaps cockpitBitmaps;
    std::vector<RadarStateActor> radarActors;
    RadarPages radarEntryPages;
    CockpitPageState pages;
    RadarState radarState;
    IndexedPages pixels;
    std::vector<NormalWorkOperation> operations;
    std::optional<Level1CombatSceneWork> combat{};
};

// Complete84e3->8518 planning on copies; no clock advance or shared-state commit.
Level2FramePlan planLevel2Frame(const GameplayAssets& assets, const Level2RenderState& frame,
    std::uint16_t loopTick, std::int16_t armor, const HorizonWorkState& horizon,
    const CockpitSettings& settings, const CockpitPageState& pages,
    const RadarState& radar, const IndexedPages& pixels);

Level2FramePlan planLevel2Frame(const GameplayAssets& assets, const Level2RenderState& frame,
    std::uint16_t loopTick, std::int16_t armor, const HorizonWorkState& horizon,
    const CockpitSettings& settings, const CockpitPageState& pages,
    const RadarState& radar, const IndexedPages& pixels,
    Level2FrameDomain domain);


//Genuine retained CACHE=A400/display=A000,A800 and current physical pool keys.
Level2FramePlan planRecoveredLevel2ConstructorFrame(const GameplayAssets& assets, const Level2RenderState& frame,
    std::uint16_t loopTick, std::int16_t armor, const HorizonWorkState& horizon,
    const CockpitSettings& settings, const CockpitPageState& pages,
    const RadarState& radar, const IndexedPages& pixels, Level2FrameDomain domain,
    const std::vector<std::size_t>& physicalKeys);

// Commit the successful render once; display role swap belongs to the later flip.
void commitLevel2Frame(const Level2FramePlan& plan, Level2RenderState frame,
    CockpitPageState& pages, RadarState& radar, IndexedPages& pixels);

}

namespace stellar7 {
Level2FramePlan planRecoveredLevel2CombatFrame(const GameplayAssets& assets, const Level2RenderState& frame,
    std::uint16_t loopTick, std::int16_t armor, const HorizonWorkState& horizon,
    const CockpitSettings& settings, const CockpitPageState& pages,
    const RadarState& radar, const IndexedPages& pixels,
    const std::vector<std::size_t>& physicalKeys, const std::vector<Level1ObjectRef>& allocationOrder,
    Level2RadarDomain domain = Level2RadarDomain::combat);

Level2FramePlan planRecoveredLevel2PeriodicFrame(const GameplayAssets& assets, const Level2RenderState& frame,
    std::uint16_t loopTick, std::int16_t armor, const HorizonWorkState& horizon,
    const CockpitSettings& settings, const CockpitPageState& pages,
    const RadarState& radar, const IndexedPages& pixels,
    const std::vector<std::size_t>& physicalKeys, const std::vector<Level1ObjectRef>& allocationOrder,
    Level2RadarDomain domain = Level2RadarDomain::periodicLifecycle);
}
