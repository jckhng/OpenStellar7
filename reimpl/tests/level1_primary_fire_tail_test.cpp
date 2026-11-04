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
    std::vector<NormalWorkOperation> compact;
    for (const auto& operation : actual) {
        if (operation.kind == Kind::instructions && !compact.empty() && compact.back().kind == Kind::instructions) {
            compact.back().count += operation.count;
        } else {
            compact.push_back(operation);
        }
    }
    assert(compact.size() == expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        assert(compact[index].kind == expected[index].kind && compact[index].count == expected[index].count);
        assert(compact[index].value == expected[index].value);
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

Level1TailFireAction action(const FrameTailState& state, const unsigned occupied,
    const Level1Projectile* retired = nullptr) {
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
    if (retired) {
        input.pool.at(occupied) = *retired;
        input.allocationDomain = Level1ProjectileAllocationDomain::retained;
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
    struct Control {
        std::uint16_t tick, held;
        std::int16_t yaw, speed;
        FixedPosition player;
        std::array<std::uint8_t, 3> timers;
        bool first;
        unsigned occupied;
        std::uint64_t terminal;
        std::array<std::uint8_t, 3> returnedTimers;
        std::int16_t returnedYaw, returnedSpeed;
    };
    //Complete original0312->03E8, excluding0710..0717 as the typed audio owner.
    const Control controls[]{
        {2, 0x100, 0, 0, {321, -654, 0}, {}, false, 0, 257, {8, 31, 0}, 0, 0},
        {3, 0x182, -450, -195, {321, -654, 0}, {}, false, 0, 281, {8, 31, 0}, -512, -200},
        {4, 0x100, 80, 10, {321, -654, 0}, {8, 31, 0}, false, 1, 284, {7, 30, 0}, 0, 0},
        {5, 0, -80, -10, {321, -654, 0}, {7, 30, 0}, false, 1, 267, {6, 29, 0}, 0, 0},
        {16, 0, 0, 0, {50000, -50000, 0}, {6, 29, 0}, false, 1, 433, {5, 28, 0}, 0, 0},
        {6, 0x100, 0, 0, {}, {0, 2, 3}, false, 0, 272, {0, 1, 2}, 0, 0},
        {7, 0x100, 0, 0, {}, {}, false, 8, 412, {}, 0, 0},
        {0, 0x100, 0, 0, {}, {}, true, 0, 398, {8, 31, 0}, 0, 0},
    };
    for (const auto& control : controls) {
        auto state = ordinary();
        state.loopTick = control.tick;
        state.playerPosition = control.player;
        state.yawVelocity = control.yaw;
        state.forwardVelocity = control.speed;
        state.weaponTimers = control.timers;
        if (control.occupied == 1) { state.actors.push_back({23, {500, 1500, 0}}); }
        auto input = guards;
        input.firstFrame = control.first;
        for (std::size_t key = 0; key < input.heldKeys.size(); ++key) { input.heldKeys[key] = (control.held & (1U << key)) != 0; }
        if ((state.loopTick & 15) == 0) { input.periodic = FrameTailPeriodicInput{0, 2, 2, 10, 10, rng.indexA(), rng.indexB(), 0}; }
        std::optional<Level1TailFireAction> fire;
        if (input.heldKeys[8]) { fire = action(state, control.occupied); }
        const auto result = level1ProjectileFrameTailOperations(state, input, rng, fire);
        const auto launched = fire && fire->work.launch;
        std::vector<NormalWorkOperation> remainder;
        if (launched) {
            remainder = {{Kind::instructions, 60}, {Kind::repeatString, 6}, {Kind::instructions, 3},
                {Kind::string, 1}, {Kind::string, 1}, {Kind::string, 1}, {Kind::instructions, 66},
                {Kind::repeatString, 6}, {Kind::gameplayPrimaryFireEvent, 1, gameplayPrimaryFireEvent}};
            assert(fire->work.launch->projectile.position == state.playerPosition);
            assert(result.state.actors.size() == 6); //The just-allocated shot is outside the old list.
        }
        remainder.push_back({Kind::instructions, control.terminal});
        sameWork(result.afterEntryFade, expectedAfter(control.first, input.heldKeys, remainder));
        if (control.first) { sameWork(result.beforeEntryFade, {{Kind::instructions, 28}}); }
        else { assert(result.beforeEntryFade.empty()); }
        assert(result.entryFade == control.first && result.state.loopTick == static_cast<std::uint16_t>(control.tick + 1));
        assert(result.state.weaponTimers == control.returnedTimers);
        assert(result.state.yawVelocity == control.returnedYaw && result.state.forwardVelocity == control.returnedSpeed);
        assert(result.state.playerPosition == FixedPosition{});
        assert(result.keyboardHead == input.keyboardHead && result.consumedScan == 0);
        auto expectedRng = rng;
        if ((state.loopTick & 15) == 0) { expectedRng.next(); }
        sameRng(result.rng, expectedRng);
        if (control.occupied == 1 && state.loopTick != 16) {
            assert((result.state.actors[6].position == FixedPosition{179, 2154, 0}));
        }
        if (state.loopTick == 16) {
            assert((result.state.actors[6].position == FixedPosition{16036, -14036, 0}));
        }
        if (fire || state.actors.size() == 7) {
            rejects([&] { level1FrameTailOperations(state, input, rng); });
        }
    }

    auto state = ordinary();
    auto input = guards;
    input.heldKeys[8] = true;
    const auto valid = action(state, 0);
    Level1Projectile retired{};
    retired.type = 17;
    retired.collisionRadius = 441;
    retired.forwardVelocity = 500;
    retired.stateBytes4dTo54[0] = 253;
    retired.ownerAllocationIndex = 0;
    retired.presenterDistance = 306;
    retired.targetHeading = 42176;
    const auto reused = action(state, 0, &retired);
    assert(reused.work.launch->projectile.presenterDistance == retired.presenterDistance);
    assert(reused.work.launch->projectile.targetHeading == retired.targetHeading);
    const auto reusedTail = level1ProjectileFrameTailOperations(state, input, rng, reused);
    const auto freshTail = level1ProjectileFrameTailOperations(state, input, rng, valid);
    assert(reusedTail.afterEntryFade.size() == freshTail.afterEntryFade.size());
    for (std::size_t index = 0; index < reusedTail.afterEntryFade.size(); ++index) {
        const auto& actual = reusedTail.afterEntryFade[index];
        const auto& expected = freshTail.afterEntryFade[index];
        assert(actual.kind == expected.kind && actual.count == expected.count && actual.value == expected.value);
    }
    assert(reusedTail.state.weaponTimers == freshTail.state.weaponTimers);
    rejects([&] { level1ProjectileFrameTailOperations(state, input, rng); });
    input.heldKeys[8] = false;
    rejects([&] { level1ProjectileFrameTailOperations(state, input, rng, valid); });
    input.heldKeys[8] = true;
    for (unsigned mutation = 0; mutation < 14; ++mutation) {
        auto changed = valid;
        auto invalid = state;
        if (mutation == 0) { changed.work.launch.reset(); }
        if (mutation == 1) { changed.work.outcome = static_cast<Level1PrimaryFireOutcome>(99); }
        if (mutation == 2) { changed.work.mainCooldown = 8; }
        if (mutation == 3) { changed.work.weaponTimers[0] = 31; }
        if (mutation == 4) { changed.work.launch->projectile.type = 22; }
        if (mutation == 5) { changed.work.launch->projectile.position.x = 1; }
        if (mutation == 6) { changed.work.beforeEvent14.front().count += 1; }
        if (mutation == 7) { changed.work.afterEvent14BeforeLamp.front().count += 1; }
        if (mutation == 8) { changed.work.afterLamp.clear(); }
        if (mutation == 9) { changed.lampOperations.clear(); }
        if (mutation == 10) { changed.lampOperations.push_back({Kind::gameplayPrimaryFireEvent, 1, gameplayPrimaryFireEvent}); }
        if (mutation == 11) { invalid.actors.push_back({23, {}}); }
        if (mutation == 12) { invalid.weaponTimers[0] = 1; }
        if (mutation == 13) { invalid.actors[0].type = 8; }
        rejects([&] { level1ProjectileFrameTailOperations(invalid, input, rng, changed); });
    }
    for (std::size_t shot = 0; shot <= Level1State::projectilePoolSize; ++shot) {
        state.actors.push_back({23, {}});
    }
    input.heldKeys[8] = false;
    rejects([&] { level1ProjectileFrameTailOperations(state, input, rng); });
}
