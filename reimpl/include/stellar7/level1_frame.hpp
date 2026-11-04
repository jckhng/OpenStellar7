#pragma once

#include "stellar7/cockpit_state.hpp"
#include "stellar7/bonus_pickup_prefix_work.hpp"
#include "stellar7/deployment_prefix_work.hpp"
#include "stellar7/combat_scene_work.hpp"
#include "stellar7/gameplay.hpp"
#include "stellar7/indexed_pages.hpp"
#include "stellar7/level1_object_ref.hpp"
#include "stellar7/radar_state.hpp"
#include "stellar7/sight_work.hpp"
#include "stellar7/world_scene_work.hpp"
#include "stellar7/weapon_lamp_work.hpp"

#include <optional>

namespace stellar7 {

struct Level1PresentationState {
    CockpitIndicatorState indicator;
    bool alternateSightPhase;
};

struct Level1FramePlan {
    HorizonExecutionWork horizon;
    GroundExecutionWork ground;
    WorldViewSetupWork world;
    WorldSceneWork scene;
    SightWork sight;
    CockpitWork cockpit;
    RadarWork radar;
    Level1AnimationState animationAfterMain;
    Level1AnimationState animationAfterRadar;
    Level1PresentationState presentationAfter;
    std::array<IndexedBitmap, 8> groundBitmaps;
    CockpitWorkBitmaps cockpitBitmaps;
    std::vector<RadarStateActor> radarActors;
    RadarPages radarEntryPages;
    CockpitPageState pages;
    RadarState radarState;
    IndexedPages pixels;
    std::vector<IndexedPageEffect> effects;
    std::vector<NormalWorkOperation> operations;
    std::optional<StellarRng> rngAfterScene;
    std::vector<Level1BeamObservation> beams;
    std::vector<std::optional<Level1AnimationState>> animationsAfterRadar;
};

//02A3 through BEFORE02D8, on copies: first/adjacent Level1 render, before display.
//Input ground/RNG precede4C46; source callbacks are planned once, not committed to actors.
Level1FramePlan planLevel1Frame(const GameplayAssets& assets, const Level1RenderState& frame,
    Level1PresentationState presentation, std::int16_t armor, const CockpitSettings& settings,
    const CockpitPageState& pages, const RadarState& radar, const IndexedPages& pixels);

struct Level1ProjectileRenderState {
    Level1GroundState& ground;
    StellarRng& rng;
    const Level1Entity& player;
    const std::vector<Level1Entity>& entities;
    const std::array<Level1Projectile, Level1State::projectilePoolSize>& projectiles;
    const std::vector<Level1ObjectRef>& order;
    std::uint16_t loopTick;
};

// Same02A3..02D8 boundary: six entities and1..projectilePoolSize live primary slots.
Level1FramePlan planLevel1ProjectileFrame(const GameplayAssets& assets, const Level1ProjectileRenderState& frame,
    Level1PresentationState presentation, std::int16_t armor, const CockpitSettings& settings,
    const CockpitPageState& pages, const RadarState& radar, const IndexedPages& pixels);

// Explicit combat domain: primary Type23 and flat tank-owned Type17 pool records.
Level1FramePlan planLevel1CombatFrame(const GameplayAssets& assets, const Level1ProjectileRenderState& frame,
    Level1PresentationState presentation, std::int16_t armor, const CockpitSettings& settings,
    const CockpitPageState& pages, const RadarState& radar, const IndexedPages& pixels);

//Explicit periodic Type1 membership: initial six plus up to eight additional
//ordinary tanks, with the existing single primaryType11 and Type17/23 pool.
//Type3 and additional Type11 render/model commit remain explicit frontiers.
Level1FramePlan planLevel1PeriodicCombatFrame(const GameplayAssets& assets,
    const Level1ProjectileRenderState& frame, Level1PresentationState presentation,
    std::int16_t armor, const CockpitSettings& settings, const CockpitPageState& pages,
    const RadarState& radar, const IndexedPages& pixels);

enum class Level1CombatBonusMembership { none, oneType42 };
enum class Level1CombatArrivalMembership { none, approachCraft, deployment };

enum class Level1CombatLiftMembership { required, allowRetiredInitial };

struct Level1CombatRenderContext {
    const std::vector<Level1ObjectRef>& allocationOrder;
    //Only slots referenced by the late45D2 order; values are projections of
    //existing FC storage/cache, including untouched heading/projection fields.
    std::vector<Level1CombatEffectView> effects;
    //Only the explicit initial-Type11 hit domain may omit its retained dead lift.
    Level1CombatLiftMembership liftMembership = Level1CombatLiftMembership::required;
    Level1CombatBonusMembership bonusMembership = Level1CombatBonusMembership::none;
    Level1CombatArrivalMembership arrivalMembership=Level1CombatArrivalMembership::none;
    Level1OrdinaryFamily ordinaryFamily=Level1OrdinaryFamily::type1;
    Level1ProjectileAllocationDomain projectileAllocationDomain=Level1ProjectileAllocationDomain::freshOnly;
};
struct Level1CombatFramePlan {
    Level1FramePlan frame;
    std::vector<Level1CombatEffectObservation> effects;
    std::vector<Level1ObjectRef> allocationOrderAfterMain;
};

//Explicit ordinary damage domain: retained dead pool records may remain in
//entities, but only current49F8/late45D2 refs enter the main traversal. Main FC
//retirement updates the registry sequentially; terminal FC remains in the same
//radar list, with callback0 and no second age advance. Old APIs stay strict.
//Returned child operations/pixels are already composed in frame; apply only
//effect state/removal observations once, then retain final frame.radar flags
//and canonical frame.radarState cache. Do not initialize/reproject reused slots.
Level1CombatFramePlan planLevel1EffectCombatFrame(const GameplayAssets& assets,
    const Level1ProjectileRenderState& frame, const Level1CombatRenderContext& context,
    Level1PresentationState presentation, std::int16_t armor, const CockpitSettings& settings,
    const CockpitPageState& pages, const RadarState& radar, const IndexedPages& pixels);

//Successful render commit; semantic main/radar animation and presentation remain model-owned.
void commitLevel1Frame(const Level1FramePlan& plan, Level1RenderState frame,
    CockpitPageState& pages, RadarState& radar, IndexedPages& pixels);

void commitLevel1ProjectileFrame(const Level1FramePlan& plan, Level1ProjectileRenderState frame,
    CockpitPageState& pages, RadarState& radar, IndexedPages& pixels);

enum class Level1GameplayFrameBoundary { completeFrame, successPaused, successReturned };

struct Level1GameplayFrameWork {
    Level1PrefixWork prefix;
    Level1FramePlan render;
    Level1FrameTailWork tail;
    // projectilePrefix and prefix describe mutually exclusive input compositions.
    std::optional<Level1ProjectilePrefixWork> projectilePrefix;
    std::optional<WeaponLampWork> lamp;
    std::vector<Level1PrefixPaletteStep> prefixPalettes;
    //Child operations are already composed in the tail segments.
    std::optional<Level1PeriodicSpawnWork> periodic;
    std::vector<NormalWorkOperation> beforePeriodicProvider;
    std::vector<Level1PrefixScoreStep> prefixScores;
    std::vector<Level1PrefixDrawingStep> prefixDrawings;
    std::optional<Level1ArrivalWork> arrival;
    std::vector<Level1PrefixDeploymentChild> prefixDeploymentChildren;
    //Non-complete results carry prefix owners only; render/tail stay unconsumed.
    Level1GameplayFrameBoundary boundary = Level1GameplayFrameBoundary::completeFrame;
    std::optional<Level1RewardSuccessPause> successPaused;
};

}
