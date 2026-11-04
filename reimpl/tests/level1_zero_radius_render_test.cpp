#include "stellar7/level1_projectile_render_work.hpp"
#include "stellar7/world_scene_work.hpp"
#include "stellar7/radar_work.hpp"

#include <cassert>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;

template<class Action> void rejects(Action action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

std::uint64_t instructions(const std::vector<NormalWorkOperation>& operations) {
    std::uint64_t result = 0;
    for (const auto& operation : operations) {
        assert(operation.kind == NormalWorkKind::instructions);
        result += operation.count;
    }
    return result;
}

void quantization() {
    for (const std::int32_t depth : {17000, 17286}) {
        const FixedPosition center{0, depth, 0};
        const auto combat = level1CombatProjectileModelCullOperations(center, 23);
        const auto primary = level1ProjectileModelCullOperations(center);
        assert(!combat.culled && !combat.clipped && combat.projectedRadius == 1);
        assert(instructions(combat.operations) == 622);
        assert(combat.operations.size() == primary.operations.size());
        for (std::size_t index = 0; index < combat.operations.size(); ++index) {
            assert(combat.operations[index].kind == primary.operations[index].kind
                && combat.operations[index].count == primary.operations[index].count);
        }
    }
    for (const std::int32_t depth : {17500, 17287}) {
        const FixedPosition center{0, depth, 0};
        const auto combat = level1CombatProjectileModelCullOperations(center, 23);
        assert(combat.culled && !combat.clipped && combat.projectedRadius == 0);
        assert(combat.operations.size() == 1 && instructions(combat.operations) == 27);
        rejects([&] { level1ProjectileModelCullOperations(center); });
    }
    rejects([&] { level1ProjectileModelCullOperations({0, 20000, 0}); });
    rejects([&] { level1CombatProjectileModelCullOperations({135, 17500, 0}, 23); });
    rejects([&] { level1CombatProjectileModelCullOperations({0, 17500, 1}, 23); });
    rejects([&] { level1CombatProjectileModelCullOperations({0, 32634, 0}, 23); });
    rejects([&] { level1CombatProjectileModelCullOperations({0, 17500, 0}, 20); });
}

void clippedLift() {
    struct Case {
        FixedPosition center;
        std::uint16_t radius;
        bool clipped;
        std::array<std::uint64_t, 5> operations;
    };
    // Original upper-plane low/high-word branches, including actual staged37.
    const std::array<Case, 4> cases{{
        {{-155, 4416, 1000}, 26, false, {65, 232, 74, 214, 27}},
        {{-155, 4415, 1000}, 26, true, {65, 232, 74, 214, 27}},
        {{-530, 4372, 1000}, 26, true, {65, 232, 74, 214, 27}},
        {{653, 3000, 1000}, 42, true, {61, 232, 70, 214, 24}}}};
    for (const auto& input : cases) {
        const auto cull = level1CombatClippedLiftModelCullOperations(input.center);
        assert(!cull.culled && cull.clipped == input.clipped && cull.projectedRadius == input.radius);
        assert(cull.operations.size() == input.operations.size());
        for (std::size_t index = 0; index < input.operations.size(); ++index) {
            assert(cull.operations[index].kind == NormalWorkKind::instructions
                && cull.operations[index].count == input.operations[index]);
        }
        if (input.clipped) {
            rejects([&] { level1CombatLiftModelCullOperations(input.center); });
        }
    }
    rejects([&] { worldModelCullOperations({-530, 4372, 1000}, 748); });
    rejects([&] { level1CombatLiftModelCullOperations({-530, 4372, 999}); });
    rejects([&] { level1CombatClippedLiftModelCullOperations({-530, 4372, 1001}); });
    rejects([&] { level1CombatClippedLiftModelCullOperations({653, 1500, 1000}); });
    rejects([&] { level1CombatClippedLiftModelCullOperations({653, 800, 1000}); });

    ModelGroup group{};
    group.animationOffset = -1;
    group.vertices = {{{0, 0, 0}}};
    Model3d model{};
    model.collisionRadius = 748;
    model.lods = {{255, 128, {group, group}}};
    const FixedPosition center{-530, 4372, 1000};
    const WorldModelViewInput view{makeFixedMatrixQ14(0, 0, 0), center};
    constexpr std::array<std::uint16_t, 3> angles{0, 62056, 24936};
    const auto setup = level1CombatClippedLiftModelSetupOperations(center, angles, 26, model, 256, 256, view);
    assert(setup.matrix.mode == 2 && setup.translation == center && setup.selectedLodIndex == 0);
    assert((setup.groupOrder == std::vector<std::size_t>{0, 1}));
    rejects([&] { level1CombatLiftModelSetupOperations(center, angles, 26, model, 256, 256, view); });
}

void outsideUpperLift() {
    struct Case {
        FixedPosition center;
        std::uint16_t radius;
        std::uint64_t suffix;
    };
    // Actual retained41..43 crosses CEA7's signed upper-plane branch at42.
    const std::array<Case, 3> cases{{
        {{-1216, 3534, 1000}, 34, 27},
        {{-1374, 3314, 1000}, 37, 24},
        {{-1526, 3090, 1000}, 40, 24}}};
    for (const auto& input : cases) {
        const auto cull = level1CombatClippedLiftModelCullOperations(input.center);
        const std::array<std::uint64_t, 5> expected{65, 214, 74, 214, input.suffix};
        assert(!cull.culled && cull.clipped && cull.projectedRadius == input.radius);
        assert(cull.operations.size() == expected.size());
        for (std::size_t index = 0; index < expected.size(); ++index) {
            assert(cull.operations[index].kind == NormalWorkKind::instructions
                && cull.operations[index].count == expected[index]);
        }
        const auto legacy = level1CombatLiftModelCullOperations(input.center);
        assert(instructions(legacy.operations) == 594);
    }
    // Outside-axis right clipping retains its existing source suffix.
    const auto right = level1CombatClippedLiftModelCullOperations({2000, 3000, 1000});
    const auto legacyRight = level1CombatLiftModelCullOperations({2000, 3000, 1000});
    assert(right.clipped && right.operations.back().count == 20);
    assert(instructions(right.operations) == instructions(legacyRight.operations));
    rejects([&] { level1CombatLiftModelCullOperations({-1374, 3314, 999}); });
    rejects([&] { level1CombatClippedLiftModelCullOperations({-1374, 3314, -1}); });
}

void liftAltitude() {
    struct Case {
        std::int32_t altitude;
        bool clipped;
        std::uint64_t firstProjection;
        std::uint64_t suffix;
    };
    // Signed Z-radius projection changes at748; the upper-plane sign is separate.
    const std::array<Case, 5> cases{{
        {0, false, 232, 27}, {747, true, 232, 27}, {748, true, 214, 27},
        {749, true, 214, 27}, {1000, true, 214, 24}}};
    Model3d model{};
    ModelGroup group{};
    group.animationOffset = -1;
    group.vertices = {{{0, 0, 0}}};
    model.collisionRadius = 748;
    model.lods = {{255, 128, {group, group}}};
    constexpr std::array<std::uint16_t, 3> angles{0, 61742, 26833};
    for (const auto& input : cases) {
        const FixedPosition center{-1526, 3090, input.altitude};
        const auto cull = level1CombatClippedLiftModelCullOperations(center);
        const std::array<std::uint64_t, 5> expected{65, input.firstProjection, 74, 214, input.suffix};
        assert(!cull.culled && cull.clipped == input.clipped && cull.projectedRadius == 40);
        assert(cull.operations.size() == expected.size());
        for (std::size_t index = 0; index < expected.size(); ++index) {
            assert(cull.operations[index].kind == NormalWorkKind::instructions
                && cull.operations[index].count == expected[index]);
        }
        const WorldModelViewInput view{makeFixedMatrixQ14(0, 0, 0), center};
        const auto setup = level1CombatClippedLiftModelSetupOperations(center, angles, 40, model, 256, 256, view);
        assert(setup.translation == center && setup.matrix.mode == 2);
        if (input.altitude != 1000) {
            rejects([&] { level1CombatLiftModelCullOperations(center); });
            rejects([&] { level1CombatLiftModelSetupOperations(center, angles, 40, model, 256, 256, view); });
        }
    }
    // Frame44 source-prefix expectation from owned43 input, before native44 observation.
    const FixedPosition contactCenter{-1672, 2862, 950};
    const auto contact = level1CombatClippedLiftModelCullOperations(contactCenter);
    assert(!contact.culled && contact.clipped && contact.projectedRadius == 45);
    assert(instructions(contact.operations) == 587 && contact.operations.back().count == 20);
    const WorldModelViewInput contactView{makeFixedMatrixQ14(0, 0, 0), contactCenter};
    const auto setup = level1CombatClippedLiftModelSetupOperations(contactCenter, angles, 45, model, 256, 256, contactView);
    assert(setup.translation == contactCenter && setup.matrix.mode == 2);
    rejects([&] { level1CombatLiftModelSetupOperations(contactCenter, angles, 45, model, 256, 256, contactView); });
    rejects([&] { level1CombatClippedLiftModelCullOperations({-1526, 3090, 1001}); });
    rejects([&] { level1CombatClippedLiftModelCullOperations({-1526, 3090, -1}); });
}

void liftRadarAltitude() {
    const WorldViewSetupInput view{{0, 0, 200000}, {0xc000, 0, 0}, {},
        0x88, 7, {112, 205, 146, 180}, {256, -256, 362}};
    const IndexedBitmap panel{120, 56, std::vector<std::uint8_t>(120 * 56, 7)};
    Model3d model{};
    model.flags = 1;
    model.animationLimits = {8};
    const RadarRetainedState state{600, 37, {0xa400, 0xa000, 0xa800, 0xa000, 0xa400}};
    std::vector<RadarActorInput> actors{
        {{0, {-1841, 1961, 0}}, 1342, 1, -1, 0, WorldActorRenderProfile::noHandles, 0, 0, 0},
        {{0, {-1672, 2862, 950}}, 748, 11, -1, 0x130c,
            WorldActorRenderProfile::heapSelectorDeferredAnimation, 0, 0, 0},
        {{0, {16384, 16384, 0}}, 862, 29, -2, 0, WorldActorRenderProfile::noHandles, 0, 0, 0},
        {{0, {16384, -16384, 0}}, 862, 29, -2, 0, WorldActorRenderProfile::noHandles, 0, 0, 0},
        {{0, {-16384, 16384, 0}}, 862, 29, -2, 0, WorldActorRenderProfile::noHandles, 0, 0, 0},
        {{0, {-16384, -16384, 0}}, 862, 29, -2, 0, WorldActorRenderProfile::noHandles, 0, 0, 0},
        {{0, {}}, 134, 23, -3, 0, WorldActorRenderProfile::inlineSelectorNoCallback, 0, 0, 0}};
    for (const auto altitude : {0, 747, 748, 749, 950, 1000}) {
        actors[1].actor.position.z = altitude;
        const auto work = level1CombatRadarOperations(state, view, {}, 0, actors, {}, panel,
            {228, 138}, model, {5, 5, 4});
        assert(work.radar.cpuWorkQualified && !work.radar.operations.empty());
        assert(work.animation.countdown == 4 && work.animation.variant == 4);
        const auto& lift = work.radar.actors[1];
        assert((lift.center == FixedPosition{-1672, 200000 - altitude, 2862}));
        assert(lift.cachedCenter == lift.center && lift.projected && lift.flags == 0);
        assert(lift.projectedX == 157 && lift.projectedY == 0 && lift.projectedRadius == 0);
        assert(!work.radar.pageCopy && work.radar.state.armorStrip == 37);
        if (altitude != 1000) {
            rejects([&] { level1ProjectileRadarOperations(state, view, {}, 0, actors, {}, panel,
                {228, 138}, model, {5, 5, 4}); });
            auto entities = actors;
            entities.pop_back();
            rejects([&] { level1RadarOperations(state, view, {}, 0, entities, {}, panel,
                {228, 138}, model, {5, 5, 4}); });
        }
    }
    for (const auto altitude : {-1, 1001}) {
        actors[1].actor.position.z = altitude;
        rejects([&] { level1CombatRadarOperations(state, view, {}, 0, actors, {}, panel,
            {228, 138}, model, {5, 5, 4}); });
    }
}

void zeroScene() {
    GameplayAssets assets;
    assets.models.resize(30);
    assets.models[23].collisionRadius = 134;
    assets.models[11].collisionRadius = 748;
    assets.models[11].flags = 1;
    assets.models[11].animationLimits = {8};
    assets.models[29].collisionRadius = 862;
    // No LOD data: the source returns before model lookup/setup for all three.
    const std::vector<WorldSceneActor> actors{
        {{0, {0, 17500, 0}}, {}, 23, 134, 0, 0, std::nullopt, std::array<std::uint8_t, 6>{}},
        {{0, {8192, -16384, 1000}}, {}, 11, 748, 0x130c, 0, Level1AnimationState{5, 3, 1}, std::nullopt},
        {{0, {16384, -16384, 0}}, {}, 29, 862, 0, 0, std::nullopt, std::nullopt}};
    const auto view = worldViewSetupOperations({{}, {}, {}, 0x88, 7,
        {24, 294, 24, 145}, {256, -256, 362}}).view;
    const auto scene = worldSceneOperations(assets, view, actors, {256, 256, 0, 0},
        {256, -256, 362}, WorldSceneMatrixProfile::level1Combat);
    // Original B9A5..return: zero-shot early return, one130C, then ordinary return.
    assert(instructions(scene.operations) == 810);
    assert(scene.pixels.empty());
    assert((scene.culled == std::vector<bool>{true, true, true}));
    assert((scene.polygonCounts == std::vector<std::size_t>{0, 0, 0}));
    assert(scene.centers.size() == 3 && scene.centers[0].y == 17500 && scene.centers[2].x == 16384);
    assert(!scene.animationStates[0] && !scene.animationStates[2]);
    assert(scene.animationStates[1] && scene.animationStates[1]->countdown == 2
        && scene.animationStates[1]->variant == 1);
    assert(actors[1].heapAnimation->countdown == 3 && actors[0].actor.position.y == 17500);
    rejects([&] { worldSceneOperations(assets, view, actors, {256, 256, 0, 0},
        {256, -256, 362}, WorldSceneMatrixProfile::level1Projectile); });
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    quantization();
    clippedLift();
    outsideUpperLift();
    liftAltitude();
    liftRadarAltitude();
    zeroScene();
}
