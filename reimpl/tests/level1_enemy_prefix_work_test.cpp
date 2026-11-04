#include "stellar7/gameplay.hpp"
#include "stellar7/simulation_prefix_work.hpp"

#include <algorithm>
#include <cassert>
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
    Level1ProjectilePrefixInput input{state.player(),state.entities(),state.projectiles(),{}, {},{},
        false,state.rng(),0,false,0,true,{false,false}};
    for (std::size_t index = 0; index < Level1State::entityCount; ++index) {
        input.allocationOrder.push_back({Level1PresentationKind::entity,index});
    }
    for (const auto index : state.activeListOrder()) {
        input.activeOrder.push_back({Level1PresentationKind::entity,index});
    }
    return input;
}

std::vector<NormalWorkOperation> canonical(const std::vector<NormalWorkOperation>& input) {
    std::vector<NormalWorkOperation> result;
    for (const auto operation : input) {
        if (!result.empty() && operation.kind == NormalWorkKind::instructions && result.back().kind == operation.kind) {
            result.back().count += operation.count;
        } else { result.push_back(operation); }
    }
    return result;
}

void equalOperations(const std::vector<NormalWorkOperation>& input,const std::vector<NormalWorkOperation>& expected) {
    const auto actual = canonical(input);
    assert(actual.size() == expected.size());
    for (std::size_t index = 0; index < actual.size(); ++index) {
        assert(actual[index].kind == expected[index].kind && actual[index].count == expected[index].count
            && actual[index].value == expected[index].value);
    }
}

Level1ProjectilePrefixInput carry(const Level1ProjectilePrefixWork& value) {
    return {value.player,value.entities,value.projectiles,value.allocationOrder,value.activeOrder,
        value.lastListCamera,value.refreshRequested,value.rng,0,false,0,true,{false,false}};
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    using Kind = NormalWorkKind;
    const Level1EnemyPrefixProfile profile{1,256,{0,441},{0,134}};
    auto input = initial();
    input.entities[0].yaw = 0xa000;
    input.entities[0].yawVelocity = 0;
    input.entities[0].forwardVelocity = 0;
    rejects([&]() { level1ProjectilePrefixOperations(input); });
    const auto first = level1EnemyPrefixOperations(input,profile);
    const auto& work = first.prefix;
    assert(first.audioEvents.size() == 1 && first.audioEvents[0].loopTick == 1 && first.audioEvents[0].eventId == 2);
    assert(work.allocationOrder.size() == 7 && work.activeOrder == work.allocationOrder);
    const auto& shot = work.projectiles[0];
    assert(shot.type == 17 && shot.collisionRadius == 441 && shot.poolSlot == 0 && shot.flags == 0);
    assert((shot.position == FixedPosition{-2048,2048,0}) && shot.yaw == 0xa000);
    assert(!shot.ownerIsPlayer && shot.ownerAllocationIndex == 0 && shot.forwardVelocity == 500);
    assert(shot.stateBytes4dTo54[1] == 54 && shot.presenterDistance == 0 && shot.targetHeading == 0);
    assert(work.entities[0].weaponCooldown == 8 && work.entities[0].weaponSlotTimers[0] == 54);
    assert(work.entities[0].weaponSlotTimers[1] == 0 && work.entities[0].behaviorTimer == 1);
    assert(work.entities[0].stateBytes4dTo52[5] == 0 && work.rng.calls() == 32);
    equalOperations(work.operations,{{Kind::instructions,1923},{Kind::repeatString,6},{Kind::instructions,4},
        {Kind::string,1},{Kind::string,1},{Kind::string,1},{Kind::instructions,78},
        {Kind::gameplayEnemyFireEvent,1,2},{Kind::instructions,2877},{Kind::repeatString,6},{Kind::instructions,407}});

    const auto following = level1EnemyPrefixOperations(carry(work),{2,256,{0,441},{0,134}});
    assert(following.audioEvents.empty());
    assert((following.prefix.projectiles[0].position == FixedPosition{-1695,1694,0}));
    assert(following.prefix.projectiles[0].stateBytes4dTo54[1] == 53);
    assert(following.prefix.projectiles[0].presenterDistance == 2330 && following.prefix.projectiles[0].targetHeading == 40976);
    equalOperations(following.prefix.operations,{{Kind::instructions,6110}});
    //The first500 step overlaps its tank owner;075E ignores that owner and continues.
    rejects([&]() { level1ProjectilePrefixOperations(carry(work)); });

    input = initial();
    input.entities[0].yaw = 0;
    input.entities[0].yawVelocity = 0;
    input.entities[0].forwardVelocity = 0;
    auto& lift = input.entities[1];
    lift.position = {8192,8192,1000};
    lift.roll = 0;
    lift.yaw = 0x6000;
    lift.yawVelocity = 0;
    lift.stateBytes4dTo52[5] = 3;
    rejects([&]() { level1EnemyPrefixOperations(input,profile); });
    input.entities[0].yaw = 0xa000;
    rejects([&]() { level1EnemyPrefixOperations(input,profile); });

    auto type20 = carry(work);
    type20.projectiles[0].type = 20;
    type20.projectiles[0].collisionRadius = 134;
    type20.projectiles[0].ownerAllocationIndex = 1;
    rejects([&]() { level1EnemyPrefixOperations(type20,profile); });

    auto changed = carry(work);
    changed.projectiles[0].ownerAllocationIndex = 1;
    rejects([&]() { level1EnemyPrefixOperations(changed,profile); });
    changed = carry(work);
    changed.projectiles[0].stateBytes4dTo54[1] = 1;
    rejects([&]() { level1EnemyPrefixOperations(changed,profile); });
    changed = carry(work);
    changed.projectiles[0].position = {0,-500,0};
    changed.projectiles[0].yaw = 0;
    rejects([&]() { level1EnemyPrefixOperations(changed,profile); });
    changed = carry(work);
    changed.projectiles[0].position.z = -301;
    rejects([&]() { level1EnemyPrefixOperations(changed,profile); });
    changed = initial();
    changed.entities[0].yaw = 0xa000;
    changed.entities[0].yawVelocity = 0;
    changed.entities[0].forwardVelocity = 0;
    changed.projectiles[0].type = 17; //Inactive but previously used records are not constructor-reset.
    rejects([&]() { level1EnemyPrefixOperations(changed,profile); });
    changed = initial();
    changed.entities[0].yaw = 0xa000;
    changed.entities[0].yawVelocity = 0;
    changed.entities[0].forwardVelocity = 0;
    rejects([&]() { level1EnemyPrefixOperations(changed,{1,17,{0,441},{0,134}}); });
}
