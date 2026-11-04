#include "stellar7/gameplay.hpp"
#include "stellar7/level1_primary_fire_work.hpp"
#include "stellar7/simulation_prefix_work.hpp"

#include <cassert>
#include <limits>
#include <memory>
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
    try { function(); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

struct Ground final : Level1FrameRenderer {
    void render(Level1RenderState state) override {
        state.ground.advance(state.player.yawVelocity,state.player.forwardVelocity,state.rng);
    }
};

Level1ProjectilePrefixInput initial() {
    auto model = std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    Ground ground;
    model->tickRetainedLevel1({},ground);
    model->finishLevel1Frame();
    const auto& state = model->level1State();
    Level1ProjectilePrefixInput result{state.player(),state.entities(),state.projectiles(),{}, {},{},
        false,state.rng(),0,false,0,true,{false,false}};
    for (std::size_t index = 0; index < Level1State::entityCount; ++index) {
        result.allocationOrder.push_back({Level1PresentationKind::entity,index});
    }
    for (const auto index : state.activeListOrder()) {
        result.activeOrder.push_back({Level1PresentationKind::entity,index});
    }
    return result;
}

Level1Projectile retired(const std::int16_t type) {
    const auto primary = type == 23;
    Level1Projectile shot;
    shot.type = type;
    shot.collisionRadius = primary ? 134 : 441;
    shot.position = {-417,557,0};
    shot.yaw = 41052;
    shot.forwardVelocity = 500;
    shot.stateBytes4dTo54[0] = 253;
    shot.ownerIsPlayer = primary;
    shot.ownerAllocationIndex = primary ? std::numeric_limits<std::size_t>::max() : 0;
    shot.presenterDistance = 713;
    shot.targetHeading = 39456;
    return shot;
}

Level1ProjectilePrefixInput carry(const Level1ProjectilePrefixWork& work) {
    return {work.player,work.entities,work.projectiles,work.allocationOrder,work.activeOrder,
        work.lastListCamera,work.refreshRequested,work.rng,0,false,0,true,{false,false}};
}

void sameOperations(const std::vector<NormalWorkOperation>& a,const std::vector<NormalWorkOperation>& b) {
    assert(a.size() == b.size());
    for (std::size_t index = 0; index < a.size(); ++index) {
        assert(a[index].kind == b[index].kind && a[index].count == b[index].count && a[index].value == b[index].value);
    }
}
} // namespace

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    const auto retainedDomain = Level1ProjectileAllocationDomain::retained;
    auto input = initial();
    input.entities[0].yaw = 0xa000;
    input.entities[0].yawVelocity = 0;
    input.entities[0].forwardVelocity = 0;
    const Level1EnemyPrefixProfile fresh{1,256,{0,441},{0,134}};
    auto retainedProfile = fresh;
    retainedProfile.allocationDomain = retainedDomain;
    const auto freshEnemy = level1EnemyPrefixOperations(input,fresh);
    for (const auto type : {17,23}) {
        input.projectiles[0] = retired(static_cast<std::int16_t>(type));
        rejects([&]() { level1EnemyPrefixOperations(input,fresh); });
        const auto enemy = level1EnemyPrefixOperations(input,retainedProfile);
        const auto& shot = enemy.prefix.projectiles[0];
        assert(shot.type == 17 && shot.collisionRadius == 441 && !shot.ownerIsPlayer && shot.ownerAllocationIndex == 0);
        assert(shot.presenterDistance == 713 && shot.targetHeading == 39456);
        assert(shot.stateBytes4dTo54[1] == 54 && shot.position == input.entities[0].position);
        assert(enemy.prefix.allocationOrder.back() == (Level1ObjectRef{Level1PresentationKind::projectile,0}));
        sameOperations(enemy.prefix.operations,freshEnemy.prefix.operations);
    }
    auto invalid = input;
    invalid.projectiles[0].pitch = 1;
    rejects([&]() { level1EnemyPrefixOperations(invalid,retainedProfile); });
    invalid = input;
    invalid.projectiles[0].type = 20;
    rejects([&]() { level1EnemyPrefixOperations(invalid,retainedProfile); });
    invalid = input;
    invalid.projectiles[0].poolSlot = 1;
    rejects([&]() { level1EnemyPrefixOperations(invalid,retainedProfile); });

    auto state = std::make_unique<Level1State>();
    state->reloadAfterFailure(StellarRng{});
    Level1PrimaryFireInput primary{state->player(),state->projectiles(),0,{0,0},0,
        Level1State::entityCount,{},false,{}, {256,0,134}};
    const auto freshPrimary = level1PrimaryFireOperations(primary);
    primary.pool[0] = retired(17);
    rejects([&]() { level1PrimaryFireOperations(primary); });
    primary.allocationDomain = retainedDomain;
    const auto launch = level1PrimaryFireOperations(primary);
    const auto& shot = launch.launch->projectile;
    assert(shot.type == 23 && shot.collisionRadius == 134 && shot.ownerIsPlayer);
    assert(shot.ownerAllocationIndex == std::numeric_limits<std::size_t>::max());
    assert(shot.presenterDistance == 713 && shot.targetHeading == 39456);
    assert(shot.position == primary.player.position && shot.yaw == primary.player.yaw);
    sameOperations(launch.beforeEvent14,freshPrimary.beforeEvent14);
    sameOperations(launch.afterEvent14BeforeLamp,freshPrimary.afterEvent14BeforeLamp);
    sameOperations(launch.afterLamp,freshPrimary.afterLamp);

    auto pending = initial();
    pending.entities[0].weaponCooldown = 255;
    pending.projectiles[0] = shot;
    pending.allocationOrder.push_back({Level1PresentationKind::projectile,0});
    pending.refreshRequested = true;
    rejects([&]() { level1ProjectilePrefixOperations(pending); });
    rejects([&]() { level1EnemyPrefixOperations(pending,fresh); });
    const auto first = level1EnemyPrefixOperations(pending,retainedProfile);
    assert(first.prefix.projectiles[0].stateBytes4dTo54[1] == 64);
    assert(first.prefix.projectiles[0].presenterDistance == 713 && first.prefix.projectiles[0].targetHeading == 39456);
    const auto second = level1EnemyPrefixOperations(carry(first.prefix),retainedProfile);
    assert(second.prefix.projectiles[0].stateBytes4dTo54[1] == 63);
    assert(second.prefix.projectiles[0].position == (FixedPosition{0,500,0}));
    assert(second.prefix.projectiles[0].presenterDistance == 500 && second.prefix.projectiles[0].targetHeading == 32768);

    auto badPrimary = primary;
    badPrimary.pool[0].flags = 1;
    rejects([&]() { level1PrimaryFireOperations(badPrimary); });
    badPrimary = primary;
    badPrimary.pool[0].ownerAllocationIndex = 1;
    rejects([&]() { level1PrimaryFireOperations(badPrimary); });
    badPrimary = primary;
    badPrimary.allocatedPoolSlots = {0};
    ++badPrimary.allocationCount;
    rejects([&]() { level1PrimaryFireOperations(badPrimary); });
}
