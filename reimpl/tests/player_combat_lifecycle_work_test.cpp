#include "stellar7/frame_tail_work.hpp"
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
constexpr auto lifecycle = Level1PlayerCombatDomain::expiryAndLoss;

template<class Function>
void rejects(Function function) {
    bool rejected = false;
    try { function(); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

Level1ProjectilePrefixInput input() {
    auto state = std::make_unique<Level1State>();
    state->reloadAfterFailure(StellarRng{});
    Level1ProjectilePrefixInput result{state->player(),state->entities(),state->projectiles(),{}, {},{},
        false,state->rng(),0,false,0,true,{false,false}};
    for (std::size_t index = 0; index < Level1State::entityCount; ++index) {
        const Level1ObjectRef ref{Level1PresentationKind::entity,index};
        result.allocationOrder.push_back(ref);
        result.activeOrder.push_back(ref);
    }
    result.entities[0].forwardVelocity = 0;
    result.entities[0].stateBytes4dTo52[5] = 3;
    result.entities[0].weaponCooldown = 255;
    result.entities[1].behaviorTimer = 20;
    return result;
}

void shot(Level1ProjectilePrefixInput& value,const std::size_t slot,const std::int16_t type,
    const FixedPosition position,const std::uint8_t lifetime) {
    auto& projectile = value.projectiles[slot];
    projectile.type = type;
    projectile.collisionRadius = type == 23 ? 134 : 441;
    projectile.position = position;
    projectile.forwardVelocity = 500;
    projectile.stateBytes4dTo54[1] = lifetime;
    projectile.ownerIsPlayer = type == 23;
    projectile.ownerAllocationIndex = type == 23 ? static_cast<std::size_t>(-1) : 0;
    projectile.presenterDistance = 1234;
    projectile.targetHeading = 4321;
    const Level1ObjectRef ref{Level1PresentationKind::projectile,slot};
    value.allocationOrder.push_back(ref);
    value.activeOrder.push_back(ref);
}

Level1DamagePrefixProfile profile(const std::int16_t armor) {
    Level1DamagePrefixProfile value{{1,256,{0,441},{0,134}},armor};
    value.combatDomain = lifecycle;
    return value;
}

std::uint64_t instructionCount(const Level1FrameTailWork& value) {
    std::uint64_t count = 0;
    for (const auto operation : value.afterEntryFade) {
        if (operation.kind == NormalWorkKind::instructions) { count += operation.count; }
    }
    return count;
}

FrameTailState tailState(const std::uint16_t loop) {
    const auto prefix = input();
    FrameTailState result{loop,{},0,0,{0,0,0},{}};
    for (const auto& actor : prefix.entities) { result.actors.push_back({actor.type,actor.position}); }
    return result;
}
} // namespace

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    auto value = input();
    shot(value,0,23,{0,5000,0},1);
    auto strict = profile(680);
    strict.combatDomain = Level1PlayerCombatDomain::survivingOnly;
    rejects([&]() { level1DamagePrefixOperations(value,strict); });
    rejects([&]() { level1ProjectilePrefixOperations(value); });
    const auto expired = level1DamagePrefixOperations(value,profile(680));
    const auto& dead = expired.combat.prefix.projectiles[0];
    assert(dead.stateBytes4dTo54[1] == 0 && dead.position == (FixedPosition{0,5500,0}));
    assert(dead.type == 23 && dead.ownerIsPlayer && dead.presenterDistance == 5500 && dead.targetHeading == 32768);
    assert(expired.combat.prefix.allocationOrder.size() == 6 && expired.combat.prefix.activeOrder.size() == 6);
    assert(expired.combat.audioEvents.empty() && expired.paletteSteps.empty());

    value = input();
    shot(value,0,17,{5000,5000,0},1);
    const auto hostile = level1DamagePrefixOperations(value,profile(680));
    assert(hostile.combat.prefix.projectiles[0].position == (FixedPosition{5000,5500,0}));
    assert(hostile.combat.prefix.projectiles[0].stateBytes4dTo54[1] == 0);
    assert(hostile.armor == 680 && hostile.combat.audioEvents.empty());

    value = input();
    shot(value,0,23,{0,5000,0},1);
    shot(value,1,23,{0,10000,0},1);
    for (const auto reversed : {false,true}) {
        auto ordered = value;
        if (reversed) { std::swap(ordered.activeOrder[6],ordered.activeOrder[7]); }
        const auto result = level1DamagePrefixOperations(ordered,profile(680));
        assert(result.combat.prefix.allocationOrder.size() == 6 && result.combat.prefix.activeOrder.size() == 6);
        assert(result.combat.prefix.projectiles[0].presenterDistance == 5500);
        assert(result.combat.prefix.projectiles[1].presenterDistance == 10500);
    }

    for (const auto beforeOwner : {false,true}) {
        value = input();
        value.entities[0].yaw = 0xa000;
        value.entities[0].weaponCooldown = 0;
        shot(value,0,23,{0,5000,0},1);
        if (beforeOwner) { std::rotate(value.activeOrder.begin(),value.activeOrder.end()-1,value.activeOrder.end()); }
        auto selected = profile(680);
        selected.enemy.allocationDomain = Level1ProjectileAllocationDomain::retained;
        const auto result = level1DamagePrefixOperations(value,selected);
        const auto& reused = result.combat.prefix.projectiles[0];
        assert(reused.type == 17 && reused.stateBytes4dTo54[1] == 54 && reused.ownerAllocationIndex == 0);
        assert(reused.position == value.entities[0].position);
        assert(reused.presenterDistance == (beforeOwner ? 5500U
            : approximatePlanarDistance(value.player.position,reused.position)));
        assert(result.combat.prefix.allocationOrder.size() == 7);
        assert(result.combat.audioEvents.size() == 1 && result.combat.audioEvents[0].eventId == 2);
    }

    value = input();
    for (std::size_t slot = 0; slot < Level1State::projectilePoolSize; ++slot) {
        shot(value,slot,17,{0,-500,0},1);
    }
    const auto lethal = level1DamagePrefixOperations(value,profile(0));
    assert(lethal.armor == -320 && lethal.fadeTicks == 8);
    assert(lethal.combat.audioEvents.size() == Level1State::projectilePoolSize);
    assert(lethal.paletteSteps.size() == Level1State::projectilePoolSize);
    assert(lethal.combat.prefix.allocationOrder.size() == 6);
    assert(std::all_of(lethal.combat.audioEvents.begin(),lethal.combat.audioEvents.end(),
        [](const auto event) { return event.eventId == 6; }));
    rejects([&]() { level1DamagePrefixOperations(value,profile(-1)); });

    StellarRng rng;
    while (static_cast<std::uint8_t>(StellarRng(rng).next()) < 10) { rng.next(); }
    FrameTailGuards guards{199,{}, {},0x1e,0x1e,false,false,false,false,false,true,
        FrameTailPeriodicInput{0,2,2,10,10,rng.indexA(),rng.indexB(),0}};
    const auto state = tailState(64);
    rejects([&]() { level1CombatFrameTailOperations(state,guards,rng); });
    const auto warning = level1CombatFrameTailOperations(state,guards,rng,{}, {},lifecycle);
    assert(warning.missionActive && !warning.failureRequested && warning.lowArmorWarningRequested);
    assert(warning.audioEvents.size() == 1 && warning.audioEvents[0].loopTick == 64 && warning.audioEvents[0].eventId == 3);
    assert(std::count_if(warning.afterEntryFade.begin(),warning.afterEntryFade.end(),[](const auto operation) {
        return operation.kind == NormalWorkKind::gameplayPlayerCombatEvent && operation.count == 1 && operation.value == 3;
    }) == 1);
    guards.armor = 0;
    const auto alive = level1CombatFrameTailOperations(state,guards,rng,{}, {},lifecycle);
    assert(alive.missionActive && !alive.failureRequested);
    guards.armor = -40;
    const auto failed = level1CombatFrameTailOperations(state,guards,rng,{}, {},lifecycle);
    assert(!failed.missionActive && failed.failureRequested && failed.state.loopTick == 65);
    assert(failed.lowArmorWarningRequested && failed.audioEvents.size() == 1);
    //0367/036C add two writes; terminal03E6 omits the ordinary03E8 JMP.
    assert(instructionCount(failed) == instructionCount(alive) + 1);
    const auto nonperiodic = level1CombatFrameTailOperations(tailState(63),guards,rng,{}, {},lifecycle);
    assert(nonperiodic.failureRequested && !nonperiodic.lowArmorWarningRequested && nonperiodic.audioEvents.empty());
    guards.armor = -321;
    rejects([&]() { level1CombatFrameTailOperations(state,guards,rng,{}, {},lifecycle); });
}
