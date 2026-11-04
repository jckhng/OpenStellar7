#include "stellar7/frame_tail_work.hpp"
#include "stellar7/audio_direct_work.hpp"
#include "stellar7/weapon_lamp_work.hpp"

#include <algorithm>
#include <cassert>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif
#include <stdexcept>
#include <utility>

namespace {
using namespace stellar7;
using Kind = NormalWorkKind;

template<class Function>
void rejects(Function function) {
    bool rejected = false;
    try { function(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

void sameWork(const std::vector<NormalWorkOperation>& actual, const std::vector<NormalWorkOperation>& expected) {
    const auto canonical = [](const auto& operations) {
        std::vector<NormalWorkOperation> compact;
        for (const auto& operation : operations) {
            if (operation.kind == Kind::instructions && !compact.empty() && compact.back().kind == Kind::instructions) {
                compact.back().count += operation.count;
            } else {
                compact.push_back(operation);
            }
        }
        return compact;
    };
    const auto left = canonical(actual);
    const auto right = canonical(expected);
    assert(left.size() == right.size());
    for (std::size_t index = 0; index < right.size(); ++index) {
        assert(left[index].kind == right[index].kind && left[index].count == right[index].count);
        assert(left[index].value == right[index].value);
    }
}

std::vector<NormalWorkOperation> expectedAfter(const bool first, const std::array<bool, 9>& held,
    const std::vector<NormalWorkOperation>& remainder) {
    std::vector<NormalWorkOperation> result{{Kind::instructions, first ? 15U : 41U}};
    for (std::size_t query = 0; query < 8; ++query) {
        result.push_back({Kind::cli, 1});
        result.push_back({Kind::instructions, 7});
        result.push_back({Kind::sti, 1});
        result.push_back({Kind::instructions, (query == 7 ? 11U : 7U) + (held[query] ? 1U : 0U)});
    }
    for (const auto operation : {NormalWorkOperation{Kind::cli, 1}, {Kind::instructions, 8},
            {Kind::popFlagsEnabled, 1}, {Kind::instructions, 10}, {Kind::cli, 1},
            {Kind::instructions, 7}, {Kind::sti, 1}}) {
        result.push_back(operation);
    }
    result.insert(result.end(), remainder.begin(), remainder.end());
    return result;
}

FrameTailState ordinary() {
    return {2, {}, 0, 0, {}, {{1, {-2048, 2048, 0}}, {11, {8192, 8192, 1000}},
        {29, {16384, 16384, 0}}, {29, {16384, -16384, 0}},
        {29, {-16384, 16384, 0}}, {29, {-16384, -16384, 0}}}};
}

Level1TailFireAction action(const FrameTailState& state, const unsigned occupied) {
    Level1PrimaryFireInput input{};
    input.player.position = state.playerPosition;
    input.player.yawVelocity = state.yawVelocity;
    input.player.forwardVelocity = state.forwardVelocity;
    input.mainCooldown = state.weaponTimers[0];
    input.weaponTimers = {state.weaponTimers[1], state.weaponTimers[2]};
    input.allocationCount = 6 + occupied;
    input.model = {256, 0, 134};
    for (std::size_t slot = 0; slot < input.pool.size(); ++slot) {
        input.pool[slot].poolSlot = slot;
        input.pool[slot].stateBytes4dTo54[0] = 253;
        if (slot < occupied) {
            input.pool[slot].stateBytes4dTo54[1] = 64;
            input.allocatedPoolSlots.push_back(slot);
        }
    }
    auto work = level1PrimaryFireOperations(input);
    std::vector<NormalWorkOperation> lamp;
    if (work.launch) {
        lamp = weaponLampOperations(0, WeaponLampState::unavailable, {}, CockpitPageState{}).operations;
    }
    return {std::move(work), std::move(lamp)};
}

void sameRng(StellarRng actual, StellarRng expected) {
    assert(actual.calls() == expected.calls() && actual.indexA() == expected.indexA() && actual.indexB() == expected.indexB());
    assert(actual.last() == expected.last());
    for (std::size_t word = 0; word < 2 * StellarRng::wordCount; ++word) { assert(actual.next() == expected.next()); }
}
}

namespace {
struct Control {
    const char* name;
    std::uint16_t loop;
    std::array<std::uint8_t, 3> timers;
    bool held;
    unsigned occupied;
    std::array<std::uint64_t, 3> gaps;
    std::size_t readyCalls;
    std::array<std::uint8_t, 3> returned;
};
const Control controls[]{
    {"main-slot1-free",9,{1,24,0},false,1,{245,29,0},1,{0,23,0}},
    {"main-slot0-free",9,{1,0,24},false,1,{243,29,0},1,{0,0,23}},
    {"main-both-busy",9,{1,24,24},false,1,{272,0,0},0,{0,23,23}},
    {"slot0-ready",9,{0,1,24},false,1,{251,21,0},1,{0,0,23}},
    {"slot1-ready",9,{0,24,1},false,1,{262,10,0},1,{0,23,0}},
    {"slots-main-busy",9,{2,1,1},false,1,{273,0,0},0,{1,0,0}},
    {"main-and-both-slots",9,{1,1,1},false,1,{258,18,10},2,{0,0,0}},
    {"main-and-slot1",9,{1,0,1},false,1,{243,26,10},2,{0,0,0}},
    {"main-and-slot0",9,{1,1,0},false,1,{245,17,19},2,{0,0,0}},
    {"both-slots-ready",9,{0,1,1},false,1,{251,18,10},2,{0,0,0}},
    {"already-zero",9,{0,0,0},false,1,{261,0,0},0,{0,0,0}},
    {"held-main-expiry",9,{1,24,0},true,1,{260,29,0},1,{0,23,0}},
    {"relaunch",10,{0,23,0},true,1,{282,0,0},0,{8,22,31}},
    {"relaunch-other-expiry",10,{0,1,0},true,1,{285,0,0},0,{8,0,31}},
    {"eight-projectiles",11,{0,0,0},false,8,{422,0,0},0,{0,0,0}},
    {"periodic-double",16,{1,1,1},false,1,{426,18,10},2,{0,0,0}},
};

FrameTailState inputState(const Control& control) {
    auto state = ordinary();
    state.loopTick = control.loop;
    state.playerPosition = control.loop == 16 ? FixedPosition{50000,-50000,0} : FixedPosition{321,-654,0};
    state.weaponTimers = control.timers;
    for (unsigned shot = 0; shot < control.occupied; ++shot) {
        state.actors.push_back({23,{500+static_cast<std::int32_t>(100*shot),1500+static_cast<std::int32_t>(200*shot),0}});
    }
    return state;
}

std::vector<std::vector<NormalWorkOperation>> lamps(const std::size_t calls, const std::uint16_t initialDirty = 1) {
    std::vector<std::vector<NormalWorkOperation>> result;
    auto pages = CockpitPageState::fromRecoveredMenu();
    auto dirty = initialDirty;
    const IndexedBitmap bitmap{24,5,std::vector<std::uint8_t>(24*5,5)};
    for (std::size_t call = 0; call < calls; ++call) {
        const auto work = weaponLampOperations(dirty,WeaponLampState::ready,bitmap,pages);
        dirty = work.dirty;
        pages = work.pages;
        result.push_back(work.operations);
    }
    return result;
}

std::vector<NormalWorkOperation> remainder(const Control& control,
    const std::vector<std::vector<NormalWorkOperation>>& ready, const bool launched) {
    std::vector<NormalWorkOperation> result;
    if (launched) {
        result = {{Kind::instructions,73},{Kind::repeatString,6},{Kind::instructions,3},
            {Kind::string,1},{Kind::string,1},{Kind::string,1},{Kind::instructions,66},
            {Kind::repeatString,6},{Kind::gameplayPrimaryFireEvent,1,gameplayPrimaryFireEvent}};
    }
    result.push_back({Kind::instructions,control.gaps[0]});
    for (std::size_t call = 0; call < ready.size(); ++call) {
        result.insert(result.end(),ready[call].begin(),ready[call].end());
        result.push_back({Kind::instructions,control.gaps[call+1]});
    }
    return result;
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    StellarRng rng;
    for (unsigned draw = 0; draw < 28; ++draw) { rng.next(); }
    FrameTailGuards guards{};
    guards.armor = 680;
    guards.keyboardHead = guards.keyboardTail = 0x1e;
    guards.missionActive = true;
    for (const auto& control : controls) {
        const auto state = inputState(control);
        auto input = guards;
        input.heldKeys[8] = control.held;
        if (control.loop == 16) { input.periodic = FrameTailPeriodicInput{0,2,2,10,10,rng.indexA(),rng.indexB(),0}; }
        std::optional<Level1TailFireAction> fire;
        if (control.held) { fire = action(state,control.occupied); }
        const auto effective = fire ? std::array<std::uint8_t,3>{fire->work.mainCooldown,
            fire->work.weaponTimers[0],fire->work.weaponTimers[1]} : state.weaponTimers;
        assert(level1TailReadyLampCalls(effective) == control.readyCalls);
        const auto ready = lamps(control.readyCalls);
        const auto result = level1ProjectileFrameTailOperations(state,input,rng,fire,ready);
        sameWork(result.afterEntryFade,expectedAfter(false,input.heldKeys,remainder(control,ready,fire && fire->work.launch)));
        assert(result.beforeEntryFade.empty() && !result.entryFade && result.state.loopTick == control.loop+1);
        assert(result.state.weaponTimers == control.returned);
        assert(result.state.playerPosition == FixedPosition{});
        auto expectedRng = rng;
        if (control.loop == 16) { expectedRng.next(); }
        sameRng(result.rng,expectedRng);
        assert(result.state.actors.size() == 6+control.occupied);
        if (control.loop != 16) {
            assert((result.state.actors[6].position == FixedPosition{179,2154,0}));
        }
        if (fire && fire->work.launch) {
            assert(fire->work.launch->poolSlot == 1 && fire->work.launch->weaponSlot == 1);
            assert(fire->work.launch->projectile.position == state.playerPosition);
            assert(result.state.actors.size() == state.actors.size()); //New allocation stays outside the old list.
        }
        auto wrong = ready;
        wrong.push_back(lamps(1).front());
        rejects([&] { level1ProjectileFrameTailOperations(state,input,rng,fire,wrong); });
        if (!ready.empty()) {
            wrong = ready; wrong.pop_back();
            rejects([&] { level1ProjectileFrameTailOperations(state,input,rng,fire,wrong); });
            wrong = ready; wrong[0].clear();
            rejects([&] { level1ProjectileFrameTailOperations(state,input,rng,fire,wrong); });
            wrong = ready; wrong[0].push_back({Kind::gameplayPrimaryFireEvent,1,gameplayPrimaryFireEvent});
            rejects([&] { level1ProjectileFrameTailOperations(state,input,rng,fire,wrong); });
        }
    }

    //The same two caller positions accept actual first-change and unchanged
    //lamp kernels sequentially, including their port/REP ordering.
    const auto& simultaneous = controls[6];
    const auto state = inputState(simultaneous);
    const auto changing = lamps(2,0);
    assert(changing[0].size() > changing[1].size());
    const auto changed = level1ProjectileFrameTailOperations(state,guards,rng,{},changing);
    sameWork(changed.afterEntryFade,expectedAfter(false,guards.heldKeys,remainder(simultaneous,changing,false)));
    auto old = ordinary(); old.weaponTimers = {1,0,0};
    rejects([&] { level1FrameTailOperations(old,guards,rng); });
    old.actors[0].type = 8; old.actors[1].type = 13;
    for (std::size_t index = 2; index < old.actors.size(); ++index) { old.actors[index].type = 30; }
    rejects([&] { frameTailOperations(old,guards); });
    old = inputState(controls[14]);
    old.actors.push_back({23,{}});
    rejects([&] { level1ProjectileFrameTailOperations(old,guards,rng); });
}

