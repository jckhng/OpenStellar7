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
    const auto activate = [&](const std::size_t index, const std::int32_t depth) {
        auto& shot = pool[index];
        shot.type = 23;
        shot.collisionRadius = 134;
        shot.position = {0, depth, 0};
        shot.stateBytes4dTo54 = {0xfd, static_cast<std::uint8_t>(depth == 0 ? 64 : 63), 0, 0, 0, 0, 0, 0};
        shot.ownerIsPlayer = true;
        shot.forwardVelocity = 500;
    };
    activate(7, 0);
    activate(2, 500);
    std::vector<Level1ObjectRef> order{
        {Level1PresentationKind::projectile, 7}, {Level1PresentationKind::entity, 0},
        {Level1PresentationKind::entity, 2}, {Level1PresentationKind::projectile, 2},
        {Level1PresentationKind::entity, 3}, {Level1PresentationKind::entity, 4},
        {Level1PresentationKind::entity, 5}, {Level1PresentationKind::entity, 1}};
    Level1ProjectileRenderState frame{ground, rng, level.player(), actors, pool, order, 0};
    const CockpitSettings settings{90, {}};
    Level1PresentationState presentation{{140, 4}, false};
    auto pages = CockpitPageState::fromRecoveredMenu();
    IndexedPages pixels;
    RadarState radar(data.artGroups[3]);
    for (std::size_t index = 0; index < actors.size(); ++index) { radar.initializeAllocation(index); }
    for (std::size_t index = 0; index < pool.size(); ++index) { radar.initializeAllocation(16 + index); }
    pixels.apply(radar.initializeLevel1(680, {}, 0,
        {pages.cachePage(), pages.backPage(), pages.frontPage(), pages.backPage(), pages.cachePage()}));
    const auto two = planLevel1ProjectileFrame(data, frame, presentation, 680, settings, pages, radar, pixels);
    assert(two.scene.centers.size() == 8 && two.scene.animationStates[7]);
    assert(two.scene.polygonCounts[0] == 0 && two.scene.polygonCounts[3] == 1);
    assert(two.radarActors[0].allocation == 23 && two.radarActors[3].allocation == 18);
    assert(two.animationAfterMain.countdown == 5 && two.animationAfterRadar.countdown == 4);
    assert(two.animationAfterMain.variant == 1 && two.animationAfterRadar.variant == 1);
    assert(rng.calls() == 16);
    commitLevel1ProjectileFrame(two, frame, pages, radar, pixels);
    assert((radar.projection(23).cachedCenter == FixedPosition{0, 200000, 0}));
    assert((radar.projection(18).cachedCenter == FixedPosition{0, 200000, 500}));
    actors[1].stateBytes4dTo52[3] = two.animationAfterRadar.countdown;
    actors[1].modelAnimationVariant = two.animationAfterRadar.variant;
    presentation = two.presentationAfter;
    pages.flip();
    pages.beginFrame();
    for (std::size_t index = 0; index < pool.size(); ++index) { activate(index, index % 2 == 0 ? 0 : 500); }
    order.clear();
    for (std::size_t index = 0; index < 7; ++index) { order.push_back({Level1PresentationKind::projectile, index}); }
    order.push_back({Level1PresentationKind::entity, 1});
    for (const auto index : {0U, 2U, 3U, 4U, 5U}) { order.push_back({Level1PresentationKind::entity, index}); }
    order.push_back({Level1PresentationKind::projectile, 7});
    frame.loopTick = 1;
    const auto eight = planLevel1ProjectileFrame(data, frame, presentation, 680, settings, pages, radar, pixels);
    assert(eight.scene.centers.size() == 14 && eight.radarActors.size() == 14);
    assert(eight.scene.animationStates[7] && eight.animationAfterMain.countdown == 3
        && eight.animationAfterRadar.countdown == 2);
    std::size_t animated = 0;
    for (const auto& state : eight.scene.animationStates) { animated += state.has_value(); }
    assert(animated == 1);
    for (std::size_t ordinal = 0; ordinal < order.size(); ++ordinal) {
        const auto ref = order[ordinal];
        const auto identity = ref.kind == Level1PresentationKind::entity ? ref.index : 16 + ref.index;
        assert(eight.radarActors[ordinal].allocation == identity);
        if (ref.kind == Level1PresentationKind::projectile) {
            assert(eight.scene.polygonCounts[ordinal] == (ref.index % 2 == 0 ? 0U : 1U));
            assert(eight.radarActors[ordinal].callback == 0 && !eight.scene.animationStates[ordinal]);
        }
    }
    auto invalidOrder = order;
    invalidOrder.pop_back();
    const Level1ProjectileRenderState invalid{ground, rng, level.player(), actors, pool, invalidOrder, 1};
    rejects([&] { planLevel1ProjectileFrame(data, invalid, presentation, 680, settings, pages, radar, pixels); });
    invalidOrder = order;
    invalidOrder[0] = invalidOrder[1];
    rejects([&] { planLevel1ProjectileFrame(data, invalid, presentation, 680, settings, pages, radar, pixels); });
    invalidOrder = order;
    invalidOrder[0] = {Level1PresentationKind::projectile, pool.size()};
    rejects([&] { planLevel1ProjectileFrame(data, invalid, presentation, 680, settings, pages, radar, pixels); });
    const auto saved = pool[0];
    pool[0].poolSlot = 1;
    rejects([&] { planLevel1ProjectileFrame(data, frame, presentation, 680, settings, pages, radar, pixels); });
    pool[0] = saved;
    pool[0].position.x = 135; // Existing qualified cull domain is unchanged.
    rejects([&] { planLevel1ProjectileFrame(data, frame, presentation, 680, settings, pages, radar, pixels); });
    pool[0] = saved;
    pool[0].ownerIsPlayer = false;
    rejects([&] { planLevel1ProjectileFrame(data, frame, presentation, 680, settings, pages, radar, pixels); });
    pool[0] = saved;
    rejects([&] { radar.drawLevel1(680, {}, 0, eight.radarActors, eight.radarEntryPages, {}, data.models[11],
        eight.animationAfterMain); });
    auto excessive = eight.radarActors;
    excessive.push_back(excessive[0]);
    rejects([&] { radar.drawLevel1Projectile(680, {}, 0, excessive, eight.radarEntryPages, {}, data.models[11],
        eight.animationAfterMain); });
    std::vector<RadarActorInput> oversized;
    for (const auto& actor : eight.radarActors) {
        oversized.push_back({actor.actor, actor.radius, actor.type, actor.category,
            actor.callback, actor.renderProfile, 0, 0, 0});
    }
    oversized.push_back(oversized.front());
    const RadarRetainedState retained{680, radar.armorStrip(), eight.radarEntryPages};
    rejects([&] { level1ProjectileRadarOperations(retained, radar.view(), {}, 0, oversized, {},
        data.artGroups[3][0], {228, 138}, data.models[11], eight.animationAfterMain); });
    auto legacyOrder = level.activeListOrder();
    legacyOrder.push_back(6);
    const Level1RenderState legacy{ground, rng, level.player(), actors, legacyOrder, 1};
    rejects([&] { planLevel1Frame(data, legacy, presentation, 680, settings, pages, radar, pixels); });
}
