#include "stellar7/gameplay.hpp"
#include "stellar7/simulation_prefix_work.hpp"

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
    auto state = std::make_unique<Level1State>(model->level1State());
    assert(state->firePrimary());
    Level1ProjectilePrefixInput input{state->player(),state->entities(),state->projectiles(),{}, {},{},
        true,state->rng(),0,false,0,true,{false,false}};
    input.entities[0].forwardVelocity = 0;
    input.entities[0].weaponCooldown = 255;
    for (std::size_t index = 0; index < Level1State::entityCount; ++index) {
        input.allocationOrder.push_back({Level1PresentationKind::entity,index});
        input.activeOrder.push_back({Level1PresentationKind::entity,index});
    }
    auto& shot = input.projectiles[1];
    shot.type = 17;
    shot.collisionRadius = 441;
    shot.position = {0,-500,0};
    shot.forwardVelocity = 500;
    shot.stateBytes4dTo54 = {253,54,0,0,0,0,0,0};
    shot.ownerAllocationIndex = 0;
    input.allocationOrder.push_back({Level1PresentationKind::projectile,1});
    input.allocationOrder.push_back({Level1PresentationKind::projectile,0});
    input.activeOrder.push_back({Level1PresentationKind::projectile,1});
    return input;
}

Level1ProjectilePrefixInput carry(const Level1DamagePrefixWork& work) {
    const auto& prefix = work.combat.prefix;
    return {prefix.player,prefix.entities,prefix.projectiles,prefix.allocationOrder,prefix.activeOrder,
        prefix.lastListCamera,prefix.refreshRequested,prefix.rng,work.fadeTicks,0,0,true,{false,false}};
}

void equalOperations(const std::vector<NormalWorkOperation>& operations,const std::size_t first,
    const std::size_t last,const std::vector<NormalWorkOperation>& expected) {
    std::vector<NormalWorkOperation> actual;
    for (auto index = first; index < last; ++index) {
        const auto operation = operations[index];
        if (!actual.empty() && operation.kind == NormalWorkKind::instructions && actual.back().kind == operation.kind) {
            actual.back().count += operation.count;
        } else { actual.push_back(operation); }
    }
    assert(actual.size() == expected.size());
    for (std::size_t index = 0; index < actual.size(); ++index) {
        assert(actual[index].kind == expected[index].kind && actual[index].count == expected[index].count
            && actual[index].value == expected[index].value);
    }
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
    const auto input = initial();
    Level1DamagePrefixProfile profile{{1,256,{0,441},{0,134}},680};
    rejects([&]() { level1EnemyPrefixOperations(input,profile.enemy); });
    const auto hit = level1DamagePrefixOperations(input,profile);
    const auto& work = hit.combat.prefix;
    assert(hit.armor == 640 && hit.fadeTicks == 8 && hit.paletteSteps.size() == 1);
    assert(hit.combat.audioEvents.size() == 1 && hit.combat.audioEvents[0].loopTick == 1
        && hit.combat.audioEvents[0].eventId == 6);
    const auto& palette = hit.paletteSteps[0];
    assert(palette.request.start == 128 && palette.request.count == 128
        && palette.request.targetColor == 238 && palette.request.intensity == 15);
    equalOperations(work.operations,0,palette.operationOffset,
        {{Kind::instructions,1715},{Kind::gameplayPlayerHitEvent,1,6},{Kind::instructions,32}});
    equalOperations(work.operations,palette.operationOffset,work.operations.size(),
        {{Kind::instructions,3926},{Kind::repeatString,6},{Kind::instructions,403}});
    assert(work.allocationOrder.size() == 7 && work.allocationOrder == work.activeOrder && !work.refreshRequested);
    assert((work.allocationOrder.back() == Level1ObjectRef{Level1PresentationKind::projectile,0}));
    const auto& retired = work.projectiles[1];
    assert(!retired.active() && retired.type == 17 && retired.collisionRadius == 441 && retired.poolSlot == 1);
    assert(retired.position == FixedPosition{} && retired.forwardVelocity == 500
        && retired.ownerAllocationIndex == 0 && !retired.ownerIsPlayer);
    assert(work.projectiles[0].active() && work.projectiles[0].stateBytes4dTo54[1] == 64
        && work.projectiles[0].presenterDistance == 0);

    const auto followingInput = carry(hit);
    profile.enemy.loopTick = 2;
    profile.armor = hit.armor;
    rejects([&]() { level1EnemyPrefixOperations(followingInput,profile.enemy); });
    const auto following = level1DamagePrefixOperations(followingInput,profile);
    assert(following.armor == 640 && following.fadeTicks == 7 && following.combat.audioEvents.empty());
    assert(following.paletteSteps.size() == 1);
    const auto& nextPalette = following.paletteSteps[0];
    assert(nextPalette.request.start == 0 && nextPalette.request.count == 256
        && nextPalette.request.targetColor == 238 && nextPalette.request.intensity == 35);
    equalOperations(following.combat.prefix.operations,0,nextPalette.operationOffset,{{Kind::instructions,29}});
    equalOperations(following.combat.prefix.operations,nextPalette.operationOffset,
        following.combat.prefix.operations.size(),{{Kind::instructions,5893}});
    assert((following.combat.prefix.projectiles[0].position == FixedPosition{0,500,0}));
    assert(following.combat.prefix.projectiles[0].stateBytes4dTo54[1] == 63);
    const auto& stale = following.combat.prefix.projectiles[1];
    assert(stale.position == retired.position && stale.type == retired.type
        && stale.collisionRadius == retired.collisionRadius && stale.stateBytes4dTo54 == retired.stateBytes4dTo54
        && stale.ownerAllocationIndex == retired.ownerAllocationIndex && stale.presenterDistance == retired.presenterDistance
        && stale.targetHeading == retired.targetHeading && stale.forwardVelocity == retired.forwardVelocity);

    auto changed = profile;
    changed.armor = 39;
    rejects([&]() { level1DamagePrefixOperations(input,changed); });
    changed = profile; changed.shieldTicks = 1;
    rejects([&]() { level1DamagePrefixOperations(input,changed); });
    changed = profile; changed.abilityTicks = 1;
    rejects([&]() { level1DamagePrefixOperations(input,changed); });
    changed = profile; changed.missionComplete = 1;
    rejects([&]() { level1DamagePrefixOperations(input,changed); });
    changed = profile; changed.paletteMode = Level1DamagePaletteMode::unsupported;
    rejects([&]() { level1DamagePrefixOperations(input,changed); });
    auto invalid = followingInput;
    invalid.fadeTicks = 9;
    rejects([&]() { level1DamagePrefixOperations(invalid,profile); });
}
