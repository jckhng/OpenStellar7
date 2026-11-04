#pragma once

#include "stellar7/level1.hpp"
#include "stellar7/level1_combat_effect_membership.hpp"
#include "stellar7/level1_projectile_allocation.hpp"
#include "stellar7/normal_clock.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace stellar7 {

struct Level1PrimaryFireModelLookup {
    std::uint16_t firstBlockCapacity;
    std::uint8_t scaleShift;
    std::uint16_t baseRadius;
};

enum class Level1PrimaryFireMembership { initialSix, periodicSpawned, combatEffects, deployment, level2Constructor, level2Combat, level2Periodic };

struct Level1PrimaryFireInput {
    Level1Entity player;
    std::array<Level1Projectile, Level1State::projectilePoolSize> pool;
    std::uint8_t mainCooldown;
    std::array<std::uint8_t, 2> weaponTimers;
    std::uint16_t abilityTicks;
    std::size_t allocationCount;
    std::vector<std::size_t> allocatedPoolSlots;
    bool refreshRequested;
    FixedPosition priorShotOrigin;
    //Resident non-null first registry block and its owned model23 header.
    Level1PrimaryFireModelLookup model;
    Level1ProjectileAllocationDomain allocationDomain = Level1ProjectileAllocationDomain::freshOnly;
    Level1PrimaryFireMembership membership = Level1PrimaryFireMembership::initialSix;
    std::optional<Level1CombatEffectMembership> combatEffects;
    //Borrowed only for explicit retainedRoster validation; never retained by work/model.
    const std::vector<Level1Entity>* retainedEmitterStorage = nullptr;
};

enum class Level1PrimaryFireOutcome { cooldown, weaponSlotsBusy, poolFull, launched };

struct Level1PrimaryFireLaunch {
    std::size_t weaponSlot;
    std::size_t poolSlot;
    std::size_t allocationIndex;
    Level1Projectile projectile;
};

struct Level1PrimaryFireWork {
    Level1PrimaryFireOutcome outcome;
    std::optional<Level1PrimaryFireLaunch> launch;
    std::uint8_t mainCooldown;
    std::array<std::uint8_t, 2> weaponTimers;
    std::size_t allocationCount;
    bool refreshRequested;
    FixedPosition shotOrigin;
    std::vector<NormalWorkOperation> beforeEvent14;
    std::vector<NormalWorkOperation> afterEvent14BeforeLamp;
    std::vector<NormalWorkOperation> afterLamp;
};

//05D4 entry through RET; excludes caller9ED4 CALL. Ordinary ability0 only.
//The event owner supplies0710..0717 entirely. Middle owns071A/071C/071D CALL;
//7BAE callee is excluded; afterLamp owns0720 cleanup and0723..0728 return.
//Failure work is entirely beforeEvent14. Retained allocation also admits inactive
//flatFD Type17/23 records, preserving their source-untouched fields.
//Explicit retainedAirborne also admits inactive Type20 at altitude0..1000,
//retaining source-untouched bank/cache/owner through partial44BA/05D4 writes.
//Explicit deployment membership additionally admits retired Type22 FD records,
//preserving source-untouched carried motion/history. Default remains strict.
//retainedRoster additionally validates stale17/20+61 through borrowed physicalFF
//storage, independently of the current emitter's type, health or live-list entry.
//Receipt supplies the source-written pool record to the recovered owner;
//firePrimary remains the sole allocation/timer/event-history mutation.
//44BA appends at allocationIndex/sets refresh; the old45D2 list is untouched.
Level1PrimaryFireWork level1PrimaryFireOperations(const Level1PrimaryFireInput& input);

} // namespace stellar7
