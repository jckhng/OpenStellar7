#include "stellar7/level1_primary_fire_work.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace stellar7 {
bool level1RetainedEmitterIdentity(const std::vector<Level1Entity>& entities,const std::size_t index) {
    constexpr std::size_t initialOrdinaryRecords=2;
    constexpr std::uint8_t ordinaryCategory=255;
    return index<entities.size() && (index<initialOrdinaryRecords || index>=Level1State::entityCount)
        && entities[index].allocationIndex==index && entities[index].stateBytes4dTo52[0]==ordinaryCategory;
}
namespace {
constexpr std::int16_t primaryProjectileType = 23;
constexpr std::uint16_t primaryProjectileRadius = 134;
constexpr std::uint8_t projectileCategory = 253;
constexpr std::uint8_t primaryLifetime = 64;
constexpr std::uint8_t primaryWeaponTimer = 32;
constexpr std::uint8_t primaryCooldown = 9;
constexpr std::int16_t primarySpeed = 500;
constexpr std::size_t ordinaryEntityCount = Level1State::entityCount;

void instructions(std::vector<NormalWorkOperation>& work, const std::uint64_t count) {
    work.push_back({NormalWorkKind::instructions, count});
}

bool fresh(const Level1Projectile& shot) {
    return shot.flags == 0 && shot.type == 0 && shot.collisionRadius == 0
        && shot.position == FixedPosition{} && shot.pitch == 0 && shot.roll == 0 && shot.yaw == 0
        && shot.pitchVelocity == 0 && shot.rollVelocity == 0 && shot.yawVelocity == 0
        && shot.xVelocity == 0 && shot.yVelocity == 0 && shot.zVelocity == 0 && shot.forwardVelocity == 0
        && shot.stateBytes4dTo54 == std::array<std::uint8_t, 8>{projectileCategory, 0, 0, 0, 0, 0, 0, 0}
        && !shot.ownerIsPlayer && shot.ownerAllocationIndex == std::numeric_limits<std::size_t>::max()
        && shot.presenterDistance == 0 && shot.targetHeading == 0;
}

bool retained(const Level1Projectile& shot) {
    const auto primary = shot.type == primaryProjectileType && shot.collisionRadius == primaryProjectileRadius
        && shot.ownerIsPlayer && shot.ownerAllocationIndex == std::numeric_limits<std::size_t>::max();
    const auto hostile = shot.type == 17 && shot.collisionRadius == 441
        && !shot.ownerIsPlayer && shot.ownerAllocationIndex == 0;
    return (primary || hostile) && shot.flags == 0 && shot.position.z == 0 && shot.pitch == 0 && shot.roll == 0
        && shot.pitchVelocity == 0 && shot.rollVelocity == 0 && shot.yawVelocity == 0
        && shot.xVelocity == 0 && shot.yVelocity == 0 && shot.zVelocity == 0 && shot.forwardVelocity == primarySpeed
        && shot.stateBytes4dTo54 == std::array<std::uint8_t, 8>{projectileCategory, 0, 0, 0, 0, 0, 0, 0};
}

bool retainedAirborneProjectile(const Level1Projectile& shot) {
    return shot.type==20 && shot.collisionRadius==134 && !shot.ownerIsPlayer
        && shot.ownerAllocationIndex<16 && shot.flags==0 && shot.pitch==0
        && shot.position.z>=0 && shot.position.z<=1000
        && shot.pitchVelocity==0 && shot.rollVelocity==0 && shot.yawVelocity==0
        && shot.xVelocity==0 && shot.yVelocity==0 && shot.zVelocity==0 && shot.forwardVelocity==primarySpeed
        && shot.stateBytes4dTo54==std::array<std::uint8_t,8>{projectileCategory,0,0,0,0,0,0,0};
}

bool retainedRosterProjectile(const Level1Projectile& shot,const std::vector<Level1Entity>& entities) {
    if (shot.type==primaryProjectileType) {return retained(shot);}
    const auto airborne=shot.type==20 && shot.collisionRadius==134;
    const auto flat=shot.type==17 && shot.collisionRadius==441;
    return (flat || airborne) && !shot.ownerIsPlayer && level1RetainedEmitterIdentity(entities,shot.ownerAllocationIndex)
        && shot.flags==0 && shot.pitch==0 && (airborne || shot.roll==0)
        && shot.position.z>=0 && shot.position.z<=(airborne ? 1000 : 0)
        && shot.pitchVelocity==0 && shot.rollVelocity==0 && shot.yawVelocity==0
        && shot.xVelocity==0 && shot.yVelocity==0 && shot.zVelocity==0 && shot.forwardVelocity==primarySpeed
        && shot.stateBytes4dTo54==std::array<std::uint8_t,8>{projectileCategory,0,0,0,0,0,0,0};
}

bool retainedGuardianProjectile(const Level1Projectile& shot) {
    return shot.type==22 && shot.collisionRadius==134 && !shot.ownerIsPlayer
        && shot.ownerAllocationIndex<16 && (shot.flags&~4U)==0
        && shot.stateBytes4dTo54[0]==253 && shot.stateBytes4dTo54[1]==0
        && shot.stateBytes4dTo54[2]==1 && shot.stateBytes4dTo54[4]==0
        && shot.forwardVelocity==500 && shot.zVelocity==0;
}

bool resetLevel2Projectile(const Level1Projectile& shot) {
    return (shot.flags & ~4U) == 0 && shot.stateBytes4dTo54[0] == projectileCategory
        && shot.stateBytes4dTo54[1] == 0
        && (shot.type == 0 || shot.type == 17 || shot.type == 22 || shot.type == primaryProjectileType);
}

void validate(const Level1PrimaryFireInput& input) {
    const auto roster=input.allocationDomain==Level1ProjectileAllocationDomain::retainedRoster;
    const auto deployment=input.membership==Level1PrimaryFireMembership::deployment;
    const auto effects = input.membership == Level1PrimaryFireMembership::combatEffects || deployment;
    const auto dynamic = input.membership == Level1PrimaryFireMembership::periodicSpawned;
    const auto periodic = input.membership == Level1PrimaryFireMembership::level2Periodic;
    const auto level2Combat = input.membership == Level1PrimaryFireMembership::level2Combat || periodic;
    const auto level2 = input.membership == Level1PrimaryFireMembership::level2Constructor || level2Combat;
    constexpr std::size_t level2ConstructorCount = 7;
    const auto ordinaryCount = input.allocationCount >= input.allocatedPoolSlots.size()
        ? input.allocationCount - input.allocatedPoolSlots.size() : 0;
    constexpr std::size_t maximumOrdinaryRecords = 14;
    if (level2Combat && (!input.combatEffects
        || input.combatEffects->profile!=Level1CombatEffectProfile::ordinary || input.combatEffects->bonusAllocations!=0
        || input.combatEffects->allocationCount!=input.allocationCount
        || input.combatEffects->ordinaryAllocations>(periodic ? 18U : level2ConstructorCount)
        || input.combatEffects->effectAllocations>Level1State::deathEffectPoolSize
        || input.combatEffects->ordinaryAllocations+input.combatEffects->effectAllocations
            +input.allocatedPoolSlots.size()!=input.allocationCount)) {
        throw std::invalid_argument("invalid Level2 combat primary registry");
    }
    if (effects) {
        const auto bonus = input.combatEffects
            && input.combatEffects->profile == Level1CombatEffectProfile::systemBonus;
        if (!input.combatEffects || input.combatEffects->allocationCount != input.allocationCount
            || (input.combatEffects->profile != Level1CombatEffectProfile::ordinary && !bonus)
            || input.combatEffects->bonusAllocations > (bonus ? 1U : 0U)
            || input.combatEffects->bonusAllocations > input.combatEffects->ordinaryAllocations
            || input.combatEffects->ordinaryAllocations > maximumOrdinaryRecords + (deployment ? 2U : 0U) + input.combatEffects->bonusAllocations
            || input.combatEffects->effectAllocations > Level1State::deathEffectPoolSize
            || input.combatEffects->ordinaryAllocations + input.combatEffects->effectAllocations
                + input.allocatedPoolSlots.size() != input.allocationCount) {
            throw std::invalid_argument("invalid combat-effect primary registry");
        }
    }
    if ((input.membership != Level1PrimaryFireMembership::initialSix && !dynamic && !effects && !level2)
        || (!effects && !level2Combat && (input.combatEffects || (level2 ? ordinaryCount != level2ConstructorCount : dynamic
            ? ordinaryCount < ordinaryEntityCount || ordinaryCount > maximumOrdinaryRecords
            : ordinaryCount != ordinaryEntityCount)))
        || (level2 && (input.allocationDomain==Level1ProjectileAllocationDomain::retainedAirborne || roster))
        || (roster ? input.retainedEmitterStorage==nullptr : input.retainedEmitterStorage!=nullptr)
        || input.abilityTicks != 0
        || (input.allocationDomain != Level1ProjectileAllocationDomain::freshOnly
            && input.allocationDomain != Level1ProjectileAllocationDomain::retained
            && input.allocationDomain != Level1ProjectileAllocationDomain::retainedAirborne
            && !roster)) {
        throw std::invalid_argument("unsupported Level1 primary fire profile");
    }
    std::array<bool, Level1State::projectilePoolSize> seen{};
    for (const auto slot : input.allocatedPoolSlots) {
        if (slot >= seen.size() || seen[slot]) {
            throw std::invalid_argument("invalid primary fire pool membership");
        }
        seen[slot] = true;
    }
    if (input.allocatedPoolSlots.size() > input.allocationCount) {
        throw std::invalid_argument("primary fire membership exceeds allocations");
    }
    for (std::size_t slot = 0; slot < input.pool.size(); ++slot) {
        if (input.pool[slot].poolSlot != slot) {
            throw std::invalid_argument("primary fire requires ordered pool identities");
        }
    }
}
} // namespace

Level1PrimaryFireWork level1PrimaryFireOperations(const Level1PrimaryFireInput& input) {
    validate(input);
    Level1PrimaryFireWork result{Level1PrimaryFireOutcome::cooldown, {}, input.mainCooldown,
        input.weaponTimers, input.allocationCount, input.refreshRequested, input.priorShotOrigin, {}, {}, {}};
    auto& work = result.beforeEvent14;
    instructions(work, 8); //05D4..05E4, through cooldown branch.
    if (input.mainCooldown != 0) {
        instructions(work, 1 + 5); //05E6 and0723..0728.
        return result;
    }
    instructions(work, 3 + 1); //Ability0 guard/JMP0674, clear weapon index.
    result.outcome = Level1PrimaryFireOutcome::weaponSlotsBusy;
    for (std::size_t weaponSlot = 0; weaponSlot < input.weaponTimers.size(); ++weaponSlot) {
        instructions(work, 3); //0676..067E.
        if (input.weaponTimers[weaponSlot] == 0) {
            result.outcome = Level1PrimaryFireOutcome::poolFull;
            instructions(work, 1); //0680 CALL44BA.
            instructions(work, 6); //44BA..44C4, including the unused stack-local load.
            auto poolSlot = input.pool.size();
            for (std::size_t slot = 0; slot < input.pool.size(); ++slot) {
                instructions(work, 3); //44C7..44CD reads lifetime only.
                if (!input.pool[slot].active()) {
                    poolSlot = slot;
                    break;
                }
                instructions(work, 4); //44CF..44D5 next pointer/limit branch.
            }
            if (poolSlot == input.pool.size()) {
                instructions(work, 1 + 5); //44D7 and44FE..4504.
                instructions(work, 3); //0683..0687 failed allocation.
            } else {
                const auto& incoming = input.pool[poolSlot];
                const auto admitted = fresh(incoming)
                    || (input.allocationDomain != Level1ProjectileAllocationDomain::freshOnly
                        && ((input.allocationDomain==Level1ProjectileAllocationDomain::retainedRoster
                            ? retainedRosterProjectile(incoming,*input.retainedEmitterStorage) : retained(incoming))
                            || (input.allocationDomain==Level1ProjectileAllocationDomain::retainedAirborne
                                && retainedAirborneProjectile(incoming)) || (input.membership==Level1PrimaryFireMembership::deployment
                            && retainedGuardianProjectile(incoming)) || (input.membership==Level1PrimaryFireMembership::level2Constructor
                            && resetLevel2Projectile(incoming)) || ((input.membership==Level1PrimaryFireMembership::level2Combat
                            || input.membership==Level1PrimaryFireMembership::level2Periodic)
                            && (incoming.flags&~4U)==0 && incoming.stateBytes4dTo54[0]==projectileCategory
                            && incoming.stateBytes4dTo54[1]==0)));
                if (!admitted
                    || std::find(input.allocatedPoolSlots.begin(), input.allocatedPoolSlots.end(), poolSlot)
                        != input.allocatedPoolSlots.end()) {
                    throw std::invalid_argument("primary fire selected a reused projectile record");
                }
                if (input.model.firstBlockCapacity <= primaryProjectileType
                    || input.model.scaleShift != 0 || input.model.baseRadius != primaryProjectileRadius) {
                    throw std::invalid_argument("unsupported primary fire allocation/model lookup");
                }
                instructions(work, 12); //44DA..44FC: partial clears,1296,49F8 append/count, return.
                instructions(work, 3 + 15); //0683..0687;0696..06C4 through position copy setup.
                work.push_back({NormalWorkKind::repeatString, 6}); //06C7 position.
                instructions(work, 3); //06C9..06D0 angle copy setup.
                for (std::size_t angle = 0; angle < 3; ++angle) {
                    work.push_back({NormalWorkKind::string, 1}); //06D3/06D4/06D5.
                }
                instructions(work, 4); //06D6..06DD type and D03C CALLF.
                instructions(work, 6); //D03C..D046 lookup argument/CALLF.
                instructions(work, 27); //C05C resident first non-null block, type23 within capacity.
                instructions(work, 15); //D04B..D075, non-null header, scaleShift0.
                instructions(work, 14); //06E2..070B, radius/speed/owner and4858 copy setup.
                work.push_back({NormalWorkKind::repeatString, 6}); //070E echo position.

                auto shot = incoming;
                shot.stateBytes4dTo54[4] = 0; //44DA +51.
                shot.zVelocity = 0; //44DE +3A; remaining motion is inherited.
                shot.stateBytes4dTo54[2] = 0;
                shot.stateBytes4dTo54[1] = primaryLifetime;
                shot.position = input.player.position;
                shot.pitch = input.player.pitch;
                shot.roll = input.player.roll;
                shot.yaw = input.player.yaw;
                shot.type = primaryProjectileType;
                shot.collisionRadius = input.model.baseRadius;
                shot.forwardVelocity = primarySpeed;
                shot.ownerIsPlayer = true;
                shot.ownerAllocationIndex = std::numeric_limits<std::size_t>::max();
                result.outcome = Level1PrimaryFireOutcome::launched;
                result.launch = Level1PrimaryFireLaunch{weaponSlot, poolSlot, input.allocationCount, shot};
                result.mainCooldown = primaryCooldown;
                result.weaponTimers[weaponSlot] = primaryWeaponTimer;
                ++result.allocationCount;
                result.refreshRequested = true;
                result.shotOrigin = input.player.position;
                instructions(result.afterEvent14BeforeLamp, 3); //071A/071C/071D CALL7BAE(0).
                instructions(result.afterLamp, 1 + 5); //0720 cleanup and0723..0728 RET.
                return result;
            }
        }
        instructions(work, 3); //0689..068D increments/tests weapon slot.
    }
    instructions(work, 5); //068F..0694 RET; no event or lamp.
    return result;
}

} // namespace stellar7
