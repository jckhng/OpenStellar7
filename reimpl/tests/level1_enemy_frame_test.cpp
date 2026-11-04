#include "stellar7/level1_frame.hpp"
#include "stellar7/level1_projectile_render_work.hpp"
#include "stellar7/raster_line_work.hpp"

#include <cassert>
#include <stdexcept>
#include <utility>

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

std::vector<std::pair<int, std::uint64_t>> canonical(const std::vector<NormalWorkOperation>& operations) {
    std::vector<std::pair<int, std::uint64_t>> result;
    for (const auto& operation : operations) {
        const auto kind = static_cast<int>(operation.kind);
        if (operation.kind == NormalWorkKind::instructions && !result.empty() && result.back().first == kind) {
            result.back().second += operation.count;
        } else {
            result.emplace_back(kind, operation.count);
        }
    }
    return result;
}

void combatLineControls() {
    const Viewport viewport{24, 294, 24, 145};
    const auto control0 = level1CombatLinePolygonOperations({{{-24, 82}, {-51, 80}}}, viewport);
    assert((canonical(control0.operations) == std::vector<std::pair<int, std::uint64_t>>{{0, 19}, {2, 2}, {0, 3}, {2, 2}, {0, 11}, {1, 1}, {0, 48}}));
    assert((control0.pixels == std::vector<ProjectedPoint>{}));
    const auto control2 = level1CombatLinePolygonOperations({{{40, 84}, {12, 80}}}, viewport);
    assert((canonical(control2.operations) == std::vector<std::pair<int, std::uint64_t>>{{0, 19}, {2, 2}, {0, 3}, {2, 2}, {0, 11}, {1, 1}, {0, 88}, {3, 1}, {0, 18}, {3, 1}, {0, 4}, {1, 1}, {0, 14}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 17}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 17}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 17}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 26}}));
    assert((control2.pixels == std::vector<ProjectedPoint>{{24, 81}, {25, 81}, {26, 81}, {27, 82}, {28, 82}, {29, 82}, {30, 82}, {31, 82}, {32, 83}, {33, 83}, {34, 83}, {35, 83}, {36, 83}, {37, 83}, {38, 84}, {39, 84}, {40, 84}}));
    const auto control3 = level1CombatLinePolygonOperations({{{320, 90}, {280, 80}}}, viewport);
    assert((canonical(control3.operations) == std::vector<std::pair<int, std::uint64_t>>{{0, 19}, {2, 2}, {0, 3}, {2, 2}, {0, 11}, {1, 1}, {0, 93}, {3, 1}, {0, 18}, {3, 1}, {0, 4}, {1, 1}, {0, 14}, {3, 1}, {0, 4}, {1, 1}, {0, 17}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 17}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 17}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 17}, {3, 1}, {0, 4}, {1, 1}, {0, 15}, {3, 1}, {0, 4}, {1, 1}, {0, 26}}));
    assert((control3.pixels == std::vector<ProjectedPoint>{{280, 80}, {281, 80}, {282, 81}, {283, 81}, {284, 81}, {285, 81}, {286, 82}, {287, 82}, {288, 82}, {289, 83}, {290, 83}, {291, 83}, {292, 83}, {293, 84}, {294, 84}}));
    rejects([&] { collapsedPolygonOperations({{-24, 82}, {-51, 80}}, viewport, PolygonClipMode::Enabled); });
    rejects([&] { rasterLineOperations({12, 80}, {40, 84}, viewport, PolygonClipMode::Enabled); });
    rejects([&] { level1CombatLinePolygonOperations({{{16384, 80}, {40, 84}}}, viewport); });
    rejects([&] { level1CombatLinePolygonOperations({{{12, 80}, {40, 84}}}, {0, 319, 0, 199}); });
}

void combatLiftControls() {
    const auto crossing = level1CombatLiftModelCullOperations({653, 4760, 1000});
    assert(!crossing.culled && !crossing.clipped && crossing.projectedRadius == 23);
    assert(crossing.operations.size() == 5);
    const std::array<std::uint64_t, 5> expected{61, 232, 70, 214, 27};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        assert(crossing.operations[index].kind == NormalWorkKind::instructions
            && crossing.operations[index].count == expected[index]);
    }
    const auto preceding = level1CombatLiftModelCullOperations({866, 4927, 1000});
    assert(canonical(preceding.operations) == canonical(worldModelCullOperations({866, 4927, 1000}, 748).operations));
    rejects([&] { worldModelCullOperations({653, 4760, 1000}, 748); });
    rejects([&] { level1CombatLiftModelCullOperations({653, 4760, 999}); });
    rejects([&] { level1CombatLiftModelCullOperations({653, 800, 1000}); });
    rejects([&] { level1CombatLiftModelCullOperations({653, 3000, 1000}); });
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
    ModelGroup enemyGroup{};
    enemyGroup.animationOffset = -1;
    enemyGroup.vertexBlock = 17;
    enemyGroup.vertices = {{{0, 0, 0}}, {{0, -240, 30}}, {{0, 440, 30}},
        {{0, -240, 0}}, {{0, 440, 0}}};
    ModelPrimitive enemyPrimitive{};
    enemyPrimitive.shapes = {{0x80, {95, 95, 14, 14}, 255, {1, 2}},
        {0x80, {95, 95, 14, 14}, 255, {3, 4}}};
    enemyGroup.primitives = {enemyPrimitive};
    result.models[17].collisionRadius = 441;
    result.models[17].lods = {{0, 128, {enemyGroup}}};
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
    combatLineControls();
    combatLiftControls();
    auto data = assets();
    Level1State level;
    level.reloadAfterFailure(StellarRng{});
    auto actors = level.entities();
    for (auto& actor : actors) { actor.position.y = -16384; }
    auto ground = level.ground();
    auto rng = level.rng();
    std::array<Level1Projectile, Level1State::projectilePoolSize> pool{};
    for (std::size_t index = 0; index < pool.size(); ++index) { pool[index].poolSlot = index; }
    auto& enemy = pool[2];
    enemy.type = 17;
    enemy.collisionRadius = 441;
    enemy.position = {-1841, 1961, 0};
    enemy.yaw = 41052;
    enemy.forwardVelocity = 500;
    enemy.stateBytes4dTo54 = {0xfd, 54, 0, 0, 0, 0, 0, 0};
    enemy.ownerAllocationIndex = 0;
    auto& primary = pool[0];
    primary.type = 23;
    primary.collisionRadius = 134;
    primary.forwardVelocity = 500;
    primary.stateBytes4dTo54 = {0xfd, 64, 0, 0, 0, 0, 0, 0};
    primary.ownerIsPlayer = true;
    std::vector<Level1ObjectRef> order;
    for (std::size_t index = 0; index < actors.size(); ++index) { order.push_back({Level1PresentationKind::entity, index}); }
    order.push_back({Level1PresentationKind::projectile, 2});
    order.push_back({Level1PresentationKind::projectile, 0});
    Level1ProjectileRenderState frame{ground, rng, level.player(), actors, pool, order, 29};
    const CockpitSettings settings{90, {}};
    Level1PresentationState presentation{{140, 4}, false};
    auto pages = CockpitPageState::fromRecoveredMenu();
    IndexedPages pixels;
    RadarState radar(data.artGroups[3]);
    for (std::size_t index = 0; index < actors.size(); ++index) { radar.initializeAllocation(index); }
    radar.initializeAllocation(18);
    radar.initializeAllocation(16);
    pixels.apply(radar.initializeLevel1(680, {}, 0,
        {pages.cachePage(), pages.backPage(), pages.frontPage(), pages.backPage(), pages.cachePage()}));
    rejects([&] { planLevel1ProjectileFrame(data, frame, presentation, 680, settings, pages, radar, pixels); });
    const auto launch = planLevel1CombatFrame(data, frame, presentation, 680, settings, pages, radar, pixels);
    assert(launch.scene.polygonCounts[6] == 2 && !launch.scene.pixels.empty());
    for (const auto& pixel : launch.scene.pixels) { assert(pixel.color == 95); }
    assert(launch.radarActors[6].allocation == 18 && launch.radarActors[6].type == 17);
    assert(launch.radarActors[6].renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback);
    assert(!launch.scene.animationStates[6] && launch.animationAfterMain.countdown == 5
        && launch.animationAfterRadar.countdown == 4);
    assert(enemy.position.x == -1841 && enemy.stateBytes4dTo54[1] == 54);
    const auto cull = level1CombatProjectileModelCullOperations(enemy.position, 17);
    assert(!cull.culled && cull.clipped && cull.projectedRadius == 37);
    const WorldModelViewInput view{makeFixedMatrixQ14(0, 0, 0), enemy.position};
    const auto setup = level1CombatProjectileModelSetupOperations(enemy.position, {0, 0, enemy.yaw},
        cull.projectedRadius, 17, data.models[17], 256, 256, view);
    assert(setup.selectedLodIndex == 0 && setup.groupOrder == std::vector<std::size_t>{0});
    assert(setup.matrix.mode == 1 && setup.groupDepths.empty());
    rejects([&] { level1ProjectileModelSetupOperations(enemy.position, {0, 0, enemy.yaw},
        cull.projectedRadius, data.models[17], 256, 256, view); });
    rejects([&] { radar.drawLevel1Projectile(680, {}, 0, launch.radarActors, launch.radarEntryPages, {},
        data.models[11], launch.animationAfterMain); });
    commitLevel1ProjectileFrame(launch, frame, pages, radar, pixels);
    assert((radar.projection(18).cachedCenter == FixedPosition{-1841, 200000, 1961}));
    actors[1].stateBytes4dTo52[3] = launch.animationAfterRadar.countdown;
    actors[1].modelAnimationVariant = launch.animationAfterRadar.variant;
    enemy.position = {-1485, 1610, 0};
    enemy.stateBytes4dTo54[1] = 53;
    pages.flip();
    pages.beginFrame();
    frame.loopTick = 30;
    const auto adjacent = planLevel1CombatFrame(data, frame, launch.presentationAfter, 680, settings, pages, radar, pixels);
    assert(adjacent.scene.polygonCounts[6] == 2 && !adjacent.scene.pixels.empty());
    assert(adjacent.animationAfterMain.countdown == 3 && adjacent.animationAfterRadar.countdown == 2);
    assert((adjacent.radarState.projection(18).cachedCenter == FixedPosition{-1485, 200000, 1610}));
    assert(enemy.position.x == -1485 && enemy.stateBytes4dTo54[1] == 53);
    const auto valid = enemy;
    enemy.ownerAllocationIndex = 1;
    rejects([&] { planLevel1CombatFrame(data, frame, presentation, 680, settings, pages, radar, pixels); });
    enemy = valid;
    enemy.ownerIsPlayer = true;
    rejects([&] { planLevel1CombatFrame(data, frame, presentation, 680, settings, pages, radar, pixels); });
    enemy = valid;
    enemy.stateBytes4dTo54[1] = 55;
    rejects([&] { planLevel1CombatFrame(data, frame, presentation, 680, settings, pages, radar, pixels); });
    enemy = valid;
    enemy.roll = 1;
    rejects([&] { planLevel1CombatFrame(data, frame, presentation, 680, settings, pages, radar, pixels); });
    enemy = valid;
    enemy.type = 20;
    enemy.collisionRadius = 134;
    enemy.stateBytes4dTo54[1] = 60;
    rejects([&] { planLevel1CombatFrame(data, frame, presentation, 680, settings, pages, radar, pixels); });
    enemy = valid;
    rejects([&] { level1CombatProjectileModelCullOperations({-1485, 1610, 1}, 17); });
    rejects([&] { level1CombatProjectileModelCullOperations(valid.position, 20); });
    auto invalidOrder = order;
    invalidOrder.back() = invalidOrder[6];
    const Level1ProjectileRenderState duplicate{ground, rng, level.player(), actors, pool, invalidOrder, 30};
    rejects([&] { planLevel1CombatFrame(data, duplicate, presentation, 680, settings, pages, radar, pixels); });
}
