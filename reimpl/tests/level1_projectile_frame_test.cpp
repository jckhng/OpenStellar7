#include "stellar7/level1_frame.hpp"
#include "stellar7/level1_projectile_render_work.hpp"

#include <cassert>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;

template<class Function>
void rejects(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

GameplayAssets assets() {
    GameplayAssets result;
    result.horizonFrames.assign(4, {256, 16, std::vector<std::uint8_t>(256 * 16, 7)});
    result.artGroups.resize(5);
    result.artGroups[3] = {{120, 56, std::vector<std::uint8_t>(120 * 56, 7)},
        {4, 1, {0, 0, 0, 0}}, {120, 18, std::vector<std::uint8_t>(120 * 18, 3)}};
    result.artGroups[4].assign(15, {4, 1, {0, 0, 0, 0}});
    result.artGroups[4][6].pixels.back() = 9;
    for (std::size_t index = 7; index < 15; ++index) {
        result.artGroups[4][index] = {8, 1, std::vector<std::uint8_t>(8, 13)};
    }
    result.models.resize(30);
    result.models[1].collisionRadius = 1342;
    result.models[11].collisionRadius = 748;
    result.models[11].flags = 1;
    result.models[11].animationLimits = {8};
    result.models[29].collisionRadius = 862;
    ModelGroup shotGroup{};
    shotGroup.animationOffset = 0;
    shotGroup.vertexBlock = 23;
    shotGroup.vertices = {{{0, -2048, 0}}, {{-50, -100, 42}}, {{50, -100, 0}}, {{0, 110, -42}}};
    ModelPrimitive shotPrimitive{};
    shotPrimitive.shapes = {{0x81, {8, 8, 1, 1}, 0, {1, 2, 3}}};
    shotGroup.primitives = {shotPrimitive, shotPrimitive};
    result.models[23].collisionRadius = 134;
    result.models[23].flags = 1;
    result.models[23].lods = {{30, 128, {shotGroup}}};
    result.modelColorTables.resize(2);
    for (std::size_t index = 0; index < 256; ++index) {
        result.modelColorTables[0][index] = static_cast<std::uint8_t>(index);
    }
    result.sharedModelRegistry[0] = 256;
    result.sharedModelRegistry[1] = 256;
    return result;
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    auto data = assets();
    Level1State level;
    level.reloadAfterFailure(StellarRng{});
    auto actors = level.entities();
    for (auto& actor : actors) {
        actor.position.y = -16384;
        actor.targetHeading = 0x4000;
    }
    auto ground = level.ground();
    auto rng = level.rng();
    std::array<Level1Projectile, Level1State::projectilePoolSize> pool{};
    for (std::size_t index = 0; index < pool.size(); ++index) { pool[index].poolSlot = index; }
    constexpr std::size_t slot = 3;
    auto& shot = pool[slot];
    shot.type = 23;
    shot.collisionRadius = 134;
    shot.stateBytes4dTo54 = {0xfd, 64, 0, 0, 0, 0, 0, 0};
    shot.ownerIsPlayer = true;
    shot.forwardVelocity = 500;
    std::vector<Level1ObjectRef> order{{Level1PresentationKind::projectile, slot}};
    for (std::size_t index = 0; index < actors.size(); ++index) {
        order.push_back({Level1PresentationKind::entity, index});
    }
    Level1ProjectileRenderState frame{ground, rng, level.player(), actors, pool, order, 0};
    const CockpitSettings settings{90, {}};
    Level1PresentationState presentation{{140, 4}, false};
    auto pages = CockpitPageState::fromRecoveredMenu();
    IndexedPages pixels;
    RadarState radar(data.artGroups[3]);
    for (std::size_t index = 0; index < actors.size(); ++index) { radar.initializeAllocation(index); }
    radar.initializeAllocation(16 + slot);
    pixels.apply(radar.initializeLevel1(680, {}, 0,
        {pages.cachePage(), pages.backPage(), pages.frontPage(), pages.backPage(), pages.cachePage()}));
    const auto first = planLevel1ProjectileFrame(data, frame, presentation, 680, settings, pages, radar, pixels);
    assert(rng.calls() == 16 && first.ground.rng.calls() == 28);
    assert(first.scene.centers.size() == 7 && !first.scene.culled[0]);
    assert(first.scene.polygonCounts[0] == 0 && first.scene.pixels.empty());
    assert(first.animationAfterMain.countdown == 5 && first.animationAfterRadar.countdown == 4);
    assert(first.animationAfterMain.variant == 1 && first.animationAfterRadar.variant == 1);
    assert(first.radarActors[0].allocation == 16 + slot && first.radarActors[0].category == -3);
    assert(first.radarActors[0].renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback);
    assert(first.radar.cpuWorkQualified && first.radar.actors.size() == 7);
    const auto dispatches = [](const auto& operations) {
        std::uint64_t count = 0;
        for (const auto& op : operations) {
            count += op.kind == NormalWorkKind::instructions ? op.count : 1;
        }
        return count;
    };
    SightRefreshInput refresh{{}, 0};
    for (const auto reference : order) {
        refresh.targets.push_back(reference.kind == Level1PresentationKind::projectile
            ? SightTargetInput{shot.type, shot.targetHeading}
            : SightTargetInput{actors[reference.index].type, actors[reference.index].targetHeading});
    }
    assert(dispatches(first.sight.operations) == dispatches(sightOperations(0, false,
        {24, 294, 24, 145}, 95, refresh).operations));
    assert(!first.sight.alternatePhase);
    const auto originCull = level1ProjectileModelCullOperations({});
    assert(originCull.clipped && originCull.projectedRadius == 134 && dispatches(originCull.operations) == 620);
    const auto movedCull = level1ProjectileModelCullOperations({0, 500, 0});
    assert(!movedCull.clipped && movedCull.projectedRadius == 46 && dispatches(movedCull.operations) == 622);
    rejects([&] { level1ProjectileModelCullOperations({0, 20000, 0}); });
    const WorldModelViewInput originView{makeFixedMatrixQ14(0, 0, 0), {}};
    const auto originSetup = level1ProjectileModelSetupOperations({}, {}, 134, data.models[23], 256, 256, originView);
    assert(dispatches(originSetup.operations) == 246 && originSetup.groupOrder == std::vector<std::size_t>{0});
    assert(originSetup.groupDepths.empty());
    rejects([&] { worldModelSetupOperations({}, {}, 134, 23, data.models[23], 256, 256, originView); });
    commitLevel1ProjectileFrame(first, frame, pages, radar, pixels);
    assert(rng.calls() == 28 && shot.stateBytes4dTo54[1] == 64 && shot.position.y == 0);
    actors[1].stateBytes4dTo52[3] = first.animationAfterRadar.countdown;
    actors[1].modelAnimationVariant = first.animationAfterRadar.variant;
    shot.stateBytes4dTo54[1] = 63;
    shot.position.y = 500;
    pages.flip();
    pages.beginFrame();
    frame.loopTick = 1;
    const auto moved = planLevel1ProjectileFrame(data, frame, first.presentationAfter, 680, settings, pages, radar, pixels);
    assert(moved.scene.polygonCounts[0] == 1 && !moved.scene.pixels.empty());
    assert(moved.animationAfterMain.countdown == 3 && moved.animationAfterRadar.countdown == 2);
    assert(shot.position.y == 500 && shot.stateBytes4dTo54[1] == 63);
    auto invalidOrder = order;
    invalidOrder[0] = {Level1PresentationKind::entity, 6};
    const Level1ProjectileRenderState invalid{ground, rng, level.player(), actors, pool, invalidOrder, 1};
    rejects([&] { planLevel1ProjectileFrame(data, invalid, presentation, 680, settings, pages, radar, pixels); });
    invalidOrder[0] = {Level1PresentationKind::deathEffect, 0};
    rejects([&] { planLevel1ProjectileFrame(data, invalid, presentation, 680, settings, pages, radar, pixels); });
    invalidOrder[0] = order[0];
    invalidOrder[1] = order[0];
    rejects([&] { planLevel1ProjectileFrame(data, invalid, presentation, 680, settings, pages, radar, pixels); });
    const auto valid = shot;
    for (const auto badType : {19, 22}) {
        shot.type = static_cast<std::int16_t>(badType);
        rejects([&] { planLevel1ProjectileFrame(data, frame, presentation, 680, settings, pages, radar, pixels); });
    }
    shot = valid;
    shot.ownerIsPlayer = false;
    rejects([&] { planLevel1ProjectileFrame(data, frame, presentation, 680, settings, pages, radar, pixels); });
    shot = valid;
    shot.stateBytes4dTo54[1] = 0;
    rejects([&] { planLevel1ProjectileFrame(data, frame, presentation, 680, settings, pages, radar, pixels); });
    shot = valid;
    shot.stateBytes4dTo54[2] = 1;
    rejects([&] { planLevel1ProjectileFrame(data, frame, presentation, 680, settings, pages, radar, pixels); });
    shot = valid;
    shot.position.z = -1;
    rejects([&] { planLevel1ProjectileFrame(data, frame, presentation, 680, settings, pages, radar, pixels); });
    shot = valid;
    pool[0] = valid;
    pool[0].poolSlot = 0;
    rejects([&] { planLevel1ProjectileFrame(data, frame, presentation, 680, settings, pages, radar, pixels); });
    pool[0] = {};
    const std::vector<std::size_t> seven{0, 1, 2, 3, 4, 5, 6};
    const Level1RenderState old{ground, rng, level.player(), actors, seven, 1};
    rejects([&] { planLevel1Frame(data, old, presentation, 680, settings, pages, radar, pixels); });
    rejects([&] { radar.drawLevel1(680, {}, 0, moved.radarActors, moved.radarEntryPages, {},
        data.models[11], moved.animationAfterMain); });
}
