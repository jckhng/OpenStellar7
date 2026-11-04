#include "stellar7/frame_tail_work.hpp"
#include "stellar7/weapon_lamp_work.hpp"

#include <algorithm>
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
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

void sameOperations(const std::vector<NormalWorkOperation>& a, const std::vector<NormalWorkOperation>& b) {
    assert(a.size() == b.size());
    for (std::size_t index = 0; index < a.size(); ++index) {
        assert(a[index].kind == b[index].kind && a[index].count == b[index].count && a[index].value == b[index].value);
    }
}

FrameTailState state() {
    FrameTailState result{2, {100,-200,0}, 0, 0, {}, {}};
    for (const auto type : {1,11,29,29,29,29}) {
        result.actors.push_back({static_cast<std::int16_t>(type), {1000,2000,0}});
    }
    return result;
}

FrameTailGuards guards() {
    FrameTailGuards result{};
    result.armor = 600;
    result.keyboardHead = result.keyboardTail = 0x1e;
    result.missionActive = true;
    return result;
}

Level1PeriodicSpawnInput periodic(const std::uint16_t allocations) {
    return {2,2,10,allocations,6,{}};
}

Level1PrimaryFireInput fireInput(const Level1CombatEffectMembership membership) {
    Level1PrimaryFireInput result{};
    result.player.position = {100,-200,0};
    result.model = {256,0,134};
    result.allocationCount = membership.allocationCount;
    result.membership = Level1PrimaryFireMembership::combatEffects;
    result.combatEffects = membership;
    for (std::size_t slot = 0; slot < result.pool.size(); ++slot) {
        result.pool[slot].poolSlot = slot;
        result.pool[slot].stateBytes4dTo54[0] = 253;
    }
    return result;
}

void traversal() {
    const auto rng = StellarRng{};
    const auto g = guards();
    const auto original = state();
    const auto legacy = level1PeriodicCombatFrameTailOperations(original,g,rng,periodic(6),{});
    const auto identical = level1CombatEffectFrameTailOperations(original,g,rng,periodic(6),{}, {6,6,0});
    sameOperations(legacy.tail.afterEntryFade, identical.tail.afterEntryFade);

    //Main may retire FC from49F8; the old45D2 record still reaches0C6A.
    auto terminal = original;
    terminal.actors.back().type = -1;
    const auto result = level1CombatEffectFrameTailOperations(terminal,g,rng,periodic(5),{}, {5,5,0});
    sameOperations(legacy.tail.afterEntryFade,result.tail.afterEntryFade);
    assert(result.tail.state.actors.size() == terminal.actors.size());
    assert(result.tail.state.actors.back().type == -1);
    for (const auto& actor : result.tail.state.actors) { assert((actor.position == FixedPosition{900,2200,0})); }
    assert(result.tail.rng.calls() == rng.calls() && !result.periodic);
    assert(result.tail.audioEvents.empty());
    rejects([&] { level1PeriodicCombatFrameTailOperations(terminal,g,rng,periodic(5),{}); });
    rejects([&] { level1CombatEffectFrameTailOperations(terminal,g,rng,periodic(6),{}, {5,5,0}); });
    auto invalid = terminal;
    invalid.actors.back().type = 20;
    rejects([&] { level1CombatEffectFrameTailOperations(invalid,g,rng,periodic(5),{}, {5,5,0}); });

    auto wrapped = terminal;
    wrapped.loopTick = 16;
    wrapped.actors.back().position = {40000,-40000,0};
    auto wrappedBaseline = wrapped;
    wrappedBaseline.actors.back().type = 29;
    auto capped = periodic(5);
    capped.spawnedEnemies = 10;
    auto cappedBaseline = capped;
    cappedBaseline.allocationCount = 6;
    auto periodicGuards = g;
    periodicGuards.periodic = FrameTailPeriodicInput{0,2,10,10,10,rng.indexA(),rng.indexB(),std::uint8_t{0}};
    const auto wrappedResult = level1CombatEffectFrameTailOperations(wrapped,periodicGuards,rng,capped,{}, {5,5,0});
    const auto originalWrap = level1PeriodicCombatFrameTailOperations(wrappedBaseline,periodicGuards,rng,cappedBaseline,{});
    sameOperations(originalWrap.tail.afterEntryFade,wrappedResult.tail.afterEntryFade);
    assert((wrappedResult.tail.state.actors.back().position == FixedPosition{-25636,25736,0}));
    assert(wrappedResult.periodic && wrappedResult.periodic->outcome == Level1PeriodicSpawnOutcome::limit);
    assert(wrappedResult.tail.rng.calls() == rng.calls());

    //No category-dependent branch in0C7C..0D20: capacity applies per old type.
    auto maximum = terminal;
    maximum.actors.clear();
    for (unsigned index = 0; index < 10; ++index) { maximum.actors.push_back({1,{}}); }
    for (unsigned index = 0; index < 4; ++index) { maximum.actors.push_back({29,{}}); }
    for (unsigned index = 0; index < 8; ++index) { maximum.actors.push_back({23,{}}); }
    for (unsigned index = 0; index < 8; ++index) { maximum.actors.push_back({-1,{}}); }
    auto limit = periodic(30);
    limit.activeEnemies = limit.spawnedEnemies = 10;
    const auto bounded = level1CombatEffectFrameTailOperations(maximum,g,rng,limit,{}, {30,14,8});
    assert(bounded.tail.state.actors.size() == 30);
    maximum.actors.push_back({-1,{}});
    rejects([&] { level1CombatEffectFrameTailOperations(maximum,g,rng,limit,{}, {30,14,8}); });
    rejects([&] { level1CombatEffectFrameTailOperations(terminal,g,rng,periodic(5),{}, {5,15,0}); });
}

void primaryMembership() {
    auto input = fireInput({2,1,1});
    const auto work = level1PrimaryFireOperations(input);
    assert(work.launch && work.launch->allocationIndex == 2 && work.allocationCount == 3);
    auto old = input;
    old.membership = Level1PrimaryFireMembership::initialSix;
    old.combatEffects.reset();
    rejects([&] { level1PrimaryFireOperations(old); });
    old.allocationCount = 6;
    const auto baseline = level1PrimaryFireOperations(old);
    sameOperations(work.beforeEvent14,baseline.beforeEvent14);
    sameOperations(work.afterEvent14BeforeLamp,baseline.afterEvent14BeforeLamp);
    sameOperations(work.afterLamp,baseline.afterLamp);
    auto invalid = input;
    invalid.combatEffects.reset();
    rejects([&] { level1PrimaryFireOperations(invalid); });
    invalid = input;
    invalid.combatEffects->allocationCount = 3;
    rejects([&] { level1PrimaryFireOperations(invalid); });
    invalid = input;
    invalid.combatEffects->effectAllocations = 0;
    rejects([&] { level1PrimaryFireOperations(invalid); });
    invalid = input;
    invalid.membership = Level1PrimaryFireMembership::periodicSpawned;
    rejects([&] { level1PrimaryFireOperations(invalid); });

    auto g = guards();
    g.heldKeys[8] = true;
    auto s = state();
    s.actors.back().type = -1;
    const Level1TailFireAction action{work,
        weaponLampOperations(0,WeaponLampState::unavailable,{},CockpitPageState{}).operations};
    const auto tail = level1CombatEffectFrameTailOperations(s,g,StellarRng{},periodic(3),{}, {2,1,1},action);
    assert(tail.tail.state.actors.size() == s.actors.size());
    assert((tail.tail.state.weaponTimers == std::array<std::uint8_t,3>{8,31,0}));
    assert(std::count_if(tail.tail.afterEntryFade.begin(),tail.tail.afterEntryFade.end(),[](const auto& op) {
        return op.kind == NormalWorkKind::gameplayPrimaryFireEvent && op.count == 1 && op.value == 14;
    }) == 1);
    auto forged = action;
    ++forged.work.allocationCount;
    ++forged.work.launch->allocationIndex;
    rejects([&] { level1CombatEffectFrameTailOperations(s,g,StellarRng{},periodic(3),{}, {2,1,1},forged); });
    auto cooldownInput = input;
    cooldownInput.mainCooldown = 2;
    const Level1TailFireAction cooldown{level1PrimaryFireOperations(cooldownInput),{}};
    s.weaponTimers[0] = 2;
    const auto waited = level1CombatEffectFrameTailOperations(s,g,StellarRng{},periodic(2),{}, {2,1,1},cooldown);
    assert(waited.tail.state.weaponTimers[0] == 1);
}
} // namespace

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    traversal();
    primaryMembership();
    return 0;
}
