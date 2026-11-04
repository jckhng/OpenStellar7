#include "stellar7/level1_primary_fire_work.hpp"

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
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

void sameShot(const Level1Projectile& first, const Level1Projectile& second) {
    assert(first.poolSlot == second.poolSlot && first.flags == second.flags);
    assert(first.type == second.type && first.collisionRadius == second.collisionRadius);
    assert(first.position == second.position);
    assert(first.pitch == second.pitch && first.roll == second.roll && first.yaw == second.yaw);
    assert(first.pitchVelocity == second.pitchVelocity && first.rollVelocity == second.rollVelocity);
    assert(first.yawVelocity == second.yawVelocity && first.xVelocity == second.xVelocity);
    assert(first.yVelocity == second.yVelocity && first.zVelocity == second.zVelocity);
    assert(first.forwardVelocity == second.forwardVelocity);
    assert(first.stateBytes4dTo54 == second.stateBytes4dTo54);
    assert(first.ownerIsPlayer == second.ownerIsPlayer && first.ownerAllocationIndex == second.ownerAllocationIndex);
    assert(first.presenterDistance == second.presenterDistance && first.targetHeading == second.targetHeading);
}

std::vector<NormalWorkOperation> canonical(const std::vector<NormalWorkOperation>& operations) {
    std::vector<NormalWorkOperation> result;
    for (const auto& operation : operations) {
        if (operation.kind == NormalWorkKind::instructions && !result.empty()
            && result.back().kind == operation.kind) {
            result.back().count += operation.count;
        } else {
            result.push_back(operation);
        }
    }
    return result;
}

void sameWork(const std::vector<NormalWorkOperation>& operations,
    const std::vector<NormalWorkOperation>& expected) {
    const auto actual = canonical(operations);
    assert(actual.size() == expected.size());
    for (std::size_t index = 0; index < actual.size(); ++index) {
        assert(actual[index].kind == expected[index].kind && actual[index].count == expected[index].count);
        assert(actual[index].value == expected[index].value);
    }
}

void launchWork(const Level1PrimaryFireWork& work, const std::uint64_t firstDispatches) {
    using Kind = NormalWorkKind;
    //Pinned05D4 control excludes0710..0717 and the7BAE callee.
    sameWork(work.beforeEvent14, {{Kind::instructions, firstDispatches}, {Kind::repeatString, 6},
        {Kind::instructions, 3}, {Kind::string, 1}, {Kind::string, 1}, {Kind::string, 1},
        {Kind::instructions, 66}, {Kind::repeatString, 6}});
    sameWork(work.afterEvent14BeforeLamp, {{Kind::instructions, 3}});
    sameWork(work.afterLamp, {{Kind::instructions, 6}});
}
} // namespace

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    auto state = std::make_unique<Level1State>();
    state->reloadAfterFailure(StellarRng{});
    const Level1PrimaryFireInput initial{state->player(), state->projectiles(), state->playerWeaponCooldown(),
        state->playerProjectileTimers(), 0, Level1State::entityCount, {}, false, {9, 8, 7}, {256, 0, 134}};
    const auto oldActive = state->activeListOrder();
    const auto planned = level1PrimaryFireOperations(initial);
    assert(planned.outcome == Level1PrimaryFireOutcome::launched && planned.launch);
    assert(planned.launch->weaponSlot == 0 && planned.launch->poolSlot == 0);
    assert(planned.launch->allocationIndex == Level1State::entityCount);
    assert(planned.allocationCount == Level1State::entityCount + 1 && planned.refreshRequested);
    assert(planned.mainCooldown == 9 && planned.weaponTimers[0] == 32 && planned.weaponTimers[1] == 0);
    assert(planned.shotOrigin == initial.player.position);
    launchWork(planned, 55);
    for (std::size_t slot = 0; slot < initial.pool.size(); ++slot) {
        sameShot(initial.pool[slot], state->projectiles()[slot]);
    }
    assert(state->firePrimary());
    sameShot(planned.launch->projectile, state->projectiles()[0]);
    assert(state->playerWeaponCooldown() == planned.mainCooldown);
    assert(state->playerProjectileTimers() == planned.weaponTimers);
    assert(state->activeListOrder() == oldActive); //Allocation append does not insert into45D2.

    auto changed = initial;
    changed.pool = state->projectiles();
    changed.allocatedPoolSlots = {0};
    changed.allocationCount = Level1State::entityCount + 1;
    changed.mainCooldown = state->playerWeaponCooldown();
    changed.weaponTimers = state->playerProjectileTimers();
    const auto held = level1PrimaryFireOperations(changed);
    assert(held.outcome == Level1PrimaryFireOutcome::cooldown && !held.launch);
    assert(held.mainCooldown == 9 && held.weaponTimers[0] == 32);
    sameWork(held.beforeEvent14, {{NormalWorkKind::instructions, 14}});
    assert(held.afterEvent14BeforeLamp.empty() && held.afterLamp.empty());

    changed = initial;
    changed.weaponTimers = {1, 2};
    const auto busy = level1PrimaryFireOperations(changed);
    assert(busy.outcome == Level1PrimaryFireOutcome::weaponSlotsBusy && !busy.launch);
    assert(busy.weaponTimers == changed.weaponTimers && !busy.refreshRequested);
    sameWork(busy.beforeEvent14, {{NormalWorkKind::instructions, 29}});
    assert(busy.afterEvent14BeforeLamp.empty() && busy.afterLamp.empty());

    changed = initial;
    changed.weaponTimers[0] = 7;
    changed.player.position = {1234, -5678, 19};
    changed.player.pitch = 0x1234;
    changed.player.roll = 0xabcd;
    changed.player.yaw = 0x5678;
    const auto secondWeapon = level1PrimaryFireOperations(changed);
    assert(secondWeapon.launch->weaponSlot == 1 && secondWeapon.weaponTimers[0] == 7);
    assert(secondWeapon.launch->projectile.position == changed.player.position);
    assert(secondWeapon.launch->projectile.pitch == changed.player.pitch);
    assert(secondWeapon.launch->projectile.roll == changed.player.roll);
    assert(secondWeapon.launch->projectile.yaw == changed.player.yaw);
    launchWork(secondWeapon, 61);

    changed = initial;
    for (std::size_t slot = 0; slot < changed.pool.size(); ++slot) {
        const auto selected = level1PrimaryFireOperations(changed);
        assert(selected.launch->poolSlot == slot);
        assert(selected.launch->allocationIndex == Level1State::entityCount + slot);
        launchWork(selected, 55 + 7 * slot);
        changed.pool[slot].stateBytes4dTo54[1] = 64;
        changed.pool[slot].flags = 19; //Earlier occupied records are opaque and untouched.
        changed.allocatedPoolSlots.push_back(slot);
        ++changed.allocationCount;
    }
    const auto full = level1PrimaryFireOperations(changed);
    assert(full.outcome == Level1PrimaryFireOutcome::poolFull && !full.launch);
    assert(full.allocationCount == changed.allocationCount && !full.refreshRequested);
    sameWork(full.beforeEvent14, {{NormalWorkKind::instructions, 173}});
    assert(full.afterEvent14BeforeLamp.empty() && full.afterLamp.empty());
    changed.weaponTimers[0] = 7;
    sameWork(level1PrimaryFireOperations(changed).beforeEvent14, {{NormalWorkKind::instructions, 101}});

    changed = initial;
    changed.abilityTicks = 1;
    rejects([&]() { level1PrimaryFireOperations(changed); });
    changed = initial;
    changed.pool[0].flags = 1;
    rejects([&]() { level1PrimaryFireOperations(changed); });
    changed = initial;
    changed.pool[0].zVelocity = 1;
    rejects([&]() { level1PrimaryFireOperations(changed); });
    changed = initial;
    changed.pool[0].stateBytes4dTo54[4] = 1;
    rejects([&]() { level1PrimaryFireOperations(changed); });
    changed = initial;
    changed.pool[0].poolSlot = 1;
    rejects([&]() { level1PrimaryFireOperations(changed); });
    changed = initial;
    changed.allocatedPoolSlots = {0};
    ++changed.allocationCount;
    rejects([&]() { level1PrimaryFireOperations(changed); });
    changed = initial;
    changed.model.scaleShift = 1;
    rejects([&]() { level1PrimaryFireOperations(changed); });
    changed = initial;
    changed.model.firstBlockCapacity = 23;
    rejects([&]() { level1PrimaryFireOperations(changed); });
    changed = initial;
    changed.model.baseRadius = 135;
    rejects([&]() { level1PrimaryFireOperations(changed); });
}
