#pragma once

#include "stellar7/level1_ordinary_family.hpp"

#include "stellar7/render_work.hpp"
#include "stellar7/level1_world_work.hpp"

#include <optional>

namespace stellar7 {

enum class Level2RadarDomain { combat, playerLifecycle, periodicLifecycle };
enum class RadarPageLayout { compatibility, recoveredStartup };
enum class RadarPixelDomain { retained, playableCampaign };

struct RadarActorInput {
    WorldActorWorkInput actor;
    std::uint32_t radius;
    std::int16_t type;
    std::int16_t category;
    std::uint16_t callback;
    WorldActorRenderProfile renderProfile;
    std::int16_t projectedX;
    std::int16_t projectedY;
    std::uint16_t projectedRadius;
};

struct RadarTraversalProfile {
    std::uint16_t beforeCallback;
    std::uint16_t afterCallback;
};

struct RadarActorOutput {
    FixedPosition center;
    FixedPosition cachedCenter;
    std::uint8_t flags;
    std::int16_t projectedX;
    std::int16_t projectedY;
    std::uint16_t projectedRadius;
    bool projected;
};

struct RadarTraversalWork {
    WorldViewSetupWork setup;
    std::vector<RadarActorOutput> actors;
    std::vector<NormalWorkOperation> operations;
};

// b996 entry through near RET; excludes outer698a CALL and all marker/panel work.
// Stationary radar profile, flat ordinary actors, zero local/global callbacks.
RadarTraversalWork radarTraversalOperations(const WorldViewSetupInput& view,
    const std::vector<RadarActorInput>& actors, RadarTraversalProfile profile);

// Original VGA segment selectors; these are not byte offsets or startup parity.
struct RadarPages {
    std::uint16_t cached;
    std::uint16_t back;
    std::uint16_t front;
    std::uint16_t draw;
    std::uint16_t copy;
};

struct RadarRetainedState {
    std::int16_t armor;
    std::optional<std::int16_t> armorStrip;
    RadarPages pages;
};

struct RadarMarkerColors {
    std::uint8_t ordinary;
    std::uint8_t fixed;
    std::uint8_t projectile = 238;
    std::uint8_t effect = 78;
};

struct RadarPixelWrite {
    ProjectedPoint point;
    std::uint8_t color;
    std::uint16_t page;
};

struct RadarPageCopy {
    std::uint16_t sourcePage;
    std::uint16_t destinationPage;
    ProjectedPoint position;
    std::uint16_t coveredWidth;
    std::uint16_t height;
};

struct RadarWork {
    WorldViewSetupWork setup;
    std::vector<RadarActorOutput> actors;
    std::vector<NormalWorkOperation> operations;
    std::vector<RadarPixelWrite> pixels;
    RadarRetainedState state;
    Viewport viewport;
    std::uint8_t enemyFlag;
    std::uint8_t rasterStyle; // DS:1fdb marker mode, distinct from world line style at1fdc.
    std::uint8_t drawColor;
    std::uint8_t fillColor;
    std::uint8_t radarPass;
    // Applied after all pixel writes, reading the actual retained source page.
    std::optional<RadarPageCopy> pageCopy;
    bool cpuWorkQualified = true;
};

// Caller8515 CALL through698a RET: zero yaw, original panel, noHandles, retained armor strip0..42.
// Actors return restored flags and cleared projectedY; pixels retain original write order.
RadarWork radarOperations(const RadarRetainedState& state, const WorldViewSetupInput& view,
    FixedPosition playerPosition, std::uint16_t playerYaw, const std::vector<RadarActorInput>& actors,
    RadarTraversalProfile profile, const IndexedBitmap& panel, RadarMarkerColors colors);

// Native pixel projection only. Returns no CPU-work operations or timing credit.
RadarWork radarPresentationPixels(const RadarRetainedState& state, const WorldViewSetupInput& view,
    FixedPosition playerPosition, std::uint16_t playerYaw, const std::vector<RadarActorInput>& actors,
    RadarTraversalProfile profile, const IndexedBitmap& panel, RadarMarkerColors colors,
    RadarPageLayout layout = RadarPageLayout::compatibility,
    RadarPixelDomain domain = RadarPixelDomain::retained);

struct Level1RadarWork {
    RadarWork radar;
    Level1AnimationState animation;
};

// Same source ownership as radarOperations, with the actual A400 cache/A000,A800
// display roles. Initialization owns the empty-list draw and no actor callback.
RadarWork level1RadarInitializationOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view, FixedPosition playerPosition, std::uint16_t playerYaw,
    const IndexedBitmap& panel, RadarMarkerColors colors);

// Initial mixed1/11/29 list; consumes the live post-main130C state exactly once.
Level1RadarWork level1RadarOperations(const RadarRetainedState& state, const WorldViewSetupInput& view,
    FixedPosition playerPosition, std::uint16_t playerYaw, const std::vector<RadarActorInput>& actors,
    RadarTraversalProfile profile, const IndexedBitmap& panel, RadarMarkerColors colors,
    const Model3d& type11Model, Level1AnimationState animation);

// Six retained entities plus1..projectilePoolSize Type23/FD inline primary shots.
Level1RadarWork level1ProjectileRadarOperations(const RadarRetainedState& state, const WorldViewSetupInput& view,
    FixedPosition playerPosition, std::uint16_t playerYaw, const std::vector<RadarActorInput>& actors,
    RadarTraversalProfile profile, const IndexedBitmap& panel, RadarMarkerColors colors,
    const Model3d& type11Model, Level1AnimationState animation);

// Explicit combat list: primary Type23 and flat tank Type17 FD records.
Level1RadarWork level1CombatRadarOperations(const RadarRetainedState& state, const WorldViewSetupInput& view,
    FixedPosition playerPosition, std::uint16_t playerYaw, const std::vector<RadarActorInput>& actors,
    RadarTraversalProfile profile, const IndexedBitmap& panel, RadarMarkerColors colors,
    const Model3d& type11Model, Level1AnimationState animation);

struct Level1DynamicRadarWork {
    RadarWork radar;
    //Aligned with the actor input order; present exactly for each Type11.
    std::vector<std::optional<Level1AnimationState>> animations;
};

//Explicit variable1/11 ordinary list with four29 and zero..eight17/23.
//Each Type11 consumes its own live post-main callback state once in source order.
//Existing source-work/nativePixels APIs and their guards remain unchanged.

Level1DynamicRadarWork level1Type3RadarOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view,FixedPosition playerPosition,std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors,RadarTraversalProfile profile,
    const IndexedBitmap& panel,RadarMarkerColors colors,const Model3d& type11Model,
    const Model3d* guardianModel,std::vector<std::optional<Level1AnimationState>> animations,
    Level1OrdinaryRenderFamily family);

Level1DynamicRadarWork level1DynamicCombatRadarOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view, FixedPosition playerPosition, std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors, RadarTraversalProfile profile,
    const IndexedBitmap& panel, RadarMarkerColors colors, const Model3d& type11Model,
    std::vector<std::optional<Level1AnimationState>> animations);


//Explicit live combat membership after target removal, with up to eight FC
//records. Their0AC8 radar-pass earlyout never ages/draws/retires a second time.
//Other guards and per-Type11 callback ownership match the dynamic combat entry.
Level1DynamicRadarWork level1CombatEffectRadarOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view, FixedPosition playerPosition, std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors, RadarTraversalProfile profile,
    const IndexedBitmap& panel, RadarMarkerColors colors, const Model3d& type11Model,
    std::vector<std::optional<Level1AnimationState>> animations);

//Explicit one-Type42 FE inline/callback0 domain. No bonus aging or model raster in radar.
Level1DynamicRadarWork level1BonusCombatRadarOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view, FixedPosition playerPosition, std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors, RadarTraversalProfile profile,
    const IndexedBitmap& panel, RadarMarkerColors colors, const Model3d& type11Model,
    std::vector<std::optional<Level1AnimationState>> animations);

Level1DynamicRadarWork level1ArrivalCombatRadarOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view, FixedPosition playerPosition, std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors, RadarTraversalProfile profile,
    const IndexedBitmap& panel, RadarMarkerColors colors, const Model3d& type11Model,
    std::vector<std::optional<Level1AnimationState>> animations);

Level1DynamicRadarWork level1DeploymentCombatRadarOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view,FixedPosition playerPosition,std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors,RadarTraversalProfile profile,
    const IndexedBitmap& panel,RadarMarkerColors colors,const Model3d& type11Model,
    const Model3d& guardianModel,std::vector<std::optional<Level1AnimationState>> animations);

}

namespace stellar7 {
RadarWork level2ConstructorRadarOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view, FixedPosition playerPosition, std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors, RadarTraversalProfile profile,
    const IndexedBitmap& panel, RadarMarkerColors colors);
}

namespace stellar7 {
RadarWork level2CombatRadarOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view, FixedPosition playerPosition, std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors, RadarTraversalProfile profile,
    const IndexedBitmap& panel, RadarMarkerColors colors,
    Level2RadarDomain domain = Level2RadarDomain::combat);
}
