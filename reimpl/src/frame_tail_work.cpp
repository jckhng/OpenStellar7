#include "stellar7/frame_tail_work.hpp"
#include "stellar7/level2_projectile_prefix_work.hpp"
#include "stellar7/level2_first_tail_work.hpp"
#include "stellar7/level2_periodic_tail_work.hpp"
#include "stellar7/audio_direct_work.hpp"
#include "stellar7/campaign_loader.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::uint16_t periodicTickMask = 15;
constexpr std::uint16_t heldPeriodicTick = periodicTickMask + 1;
constexpr std::uint16_t firstCappedHeldPeriodicTick = heldPeriodicTick * 2;
constexpr std::uint16_t lastCappedHeldPeriodicTick = heldPeriodicTick * 4;
constexpr std::int16_t heldPeriodicVelocity = 120;
constexpr std::size_t forwardKeyIndex = 4;
constexpr std::size_t reverseKeyIndex = 1;
constexpr std::size_t leftKeyIndex = 6;
constexpr std::size_t rightKeyIndex = 7;
constexpr std::size_t fireKeyIndex = 8;
constexpr std::int16_t yawIncrement = 80;
constexpr std::int16_t yawLimit = 512;
constexpr std::int16_t forwardIncrement = 10;
constexpr std::int16_t forwardLimit = 200;
constexpr std::int32_t worldHalfPage = 0x8000;
constexpr std::int32_t worldPage = 0x10000;
constexpr std::uint16_t maximumRngIndex = (StellarRng::wordCount - 1) * 2;
constexpr std::uint16_t firstKeyboardBufferOffset = 0x1e;
constexpr std::uint16_t keyboardWrapOffset = 0x3c;
constexpr std::uint16_t keyboardEntryBytes = 2;
constexpr std::uint16_t forwardMakeScan = 0x48;
constexpr std::uint64_t unmatchedActionCount = 18;
constexpr std::uint64_t instructionsPerUnmatchedAction = 8;
constexpr std::int16_t level1TankType = 1;
constexpr std::int16_t level1AircraftType = 11;
constexpr std::int16_t level1FixedType = 29;
constexpr std::int16_t level1BonusType = 42;
constexpr std::size_t level1BonusEntityCapacity = 15;
constexpr std::int16_t level1ProjectileType = 23;
constexpr std::int16_t level1EnemyProjectileType = 17;
constexpr std::size_t level1ActorCount = 6;
constexpr std::size_t level2OrdinaryCapacity = 16;
constexpr std::size_t level2AllocationCapacity = 32;
constexpr std::int16_t level2PostWaveSpawnLimit = 14;
constexpr std::uint64_t entityResetBytes = 99;
constexpr std::int16_t warningArmor = 200;
constexpr std::uint16_t warningTickMask = 63;
enum class KeyboardQueueDomain { ordinaryOrCappedPeriodic, otherPeriodic };
enum class TailDomain { level2, level2Controls, level2Combat, level1, level1Projectile, level1Combat, level1PlayerLifecycle };
enum class TailEntry { ordinary, firstLevel2 };

bool enemyProjectileDomain(const TailDomain domain) {
    return domain == TailDomain::level1Combat || domain == TailDomain::level1PlayerLifecycle;
}

bool airborneProjectileDomain(const Level1CombatEffectMembership* effects) {
    return effects && (effects->projectileAllocationDomain==Level1ProjectileAllocationDomain::retainedAirborne
        || effects->projectileAllocationDomain==Level1ProjectileAllocationDomain::retainedRoster);
}

bool projectileDomain(const TailDomain domain) {
    return domain == TailDomain::level1Projectile || enemyProjectileDomain(domain) || domain==TailDomain::level2Combat;
}

enum class TailCompletion { ordinary, rewardLive, successReturn };
struct PeriodicSpawnTailContext {
    const Level1PeriodicSpawnInput& input;
    const std::vector<Model3d>& models;
    std::optional<Level1PeriodicSpawnWork> work;
    std::vector<NormalWorkOperation> beforeProvider;
    const Level1CombatEffectMembership* combatEffects = nullptr;
    const Level1ArrivalInput* arrival = nullptr;
    std::optional<Level1ArrivalWork> arrivalWork;
    bool deployment = false;
    TailCompletion completion = TailCompletion::ordinary;
    std::vector<Level1TailSelectorRelease> selectorReleases;
};

struct Level2PeriodicTailContext {
    const Level2PeriodicTailPlanner& plan;
    std::optional<Level2PeriodicSpawnWork> work;
    std::optional<std::size_t> operationOffset;
    const Level2ArrivalTailPlanner& arrival;
    std::optional<Level1ArrivalWork> arrivalWork;
    std::optional<Level2PeriodicCacheReset> arrivalCacheReset;
};

struct PlayerLifecycleTailContext {
    bool missionActive = true;
    bool failureRequested = false;
    bool lowArmorWarningRequested = false;
    std::vector<Level1AudioEvent> audioEvents;
    NormalWorkKind warningEventKind = NormalWorkKind::gameplayPlayerCombatEvent;
};

void plain(FrameTailWork& work, const std::uint64_t count) {
    work.operations.push_back({NormalWorkKind::instructions, count});
}

void keyQuery(FrameTailWork& work) {
    work.operations.push_back({NormalWorkKind::cli, 1});
    plain(work, 7); //0438..0447.
    work.operations.push_back({NormalWorkKind::sti, 1});
    plain(work, 1); //044a RETF.
}

void damp(FrameTailWork& work, std::int16_t& velocity, const std::int16_t quantum) {
    const auto magnitude = velocity < 0 ? -static_cast<int>(velocity) : velocity;
    if (magnitude < quantum) {
        velocity = 0;
        plain(work, 2); // Storezero, JMP.
        return;
    }
    if (velocity > 0) {
        velocity = static_cast<std::int16_t>(velocity - quantum);
        plain(work, 4); // CMP,Jcc,SUB,JMP.
        return;
    }
    velocity = static_cast<std::int16_t>(velocity + quantum);
    plain(work, 3); // CMP,Jcc,ADD.
}

void level1Direction(FrameTailWork& work, const FrameTailGuards& guards, const std::uint64_t prefixDispatches) {
    plain(work, prefixDispatches); //9ECD..9F3D, or9ED7..9F3D after composed fire.
    plain(work, 2); //9F42..9F46: joystick disabled.
    plain(work, 2); //9F62..9F66: left has priority over right.
    if (guards.heldKeys[leftKeyIndex]) {
        work.state.yawVelocity += yawIncrement;
        plain(work, 3);
    } else {
        plain(work, 2); //9F72..9F76.
        if (guards.heldKeys[rightKeyIndex]) {
            work.state.yawVelocity -= yawIncrement;
            plain(work, 3);
        } else {
            plain(work, 9); //9F82..9F96.
            damp(work, work.state.yawVelocity, yawIncrement);
        }
    }
    plain(work, 9); //9FAB..9FBF.
    if (work.state.yawVelocity > yawLimit) {
        work.state.yawVelocity = yawLimit;
        plain(work, 7);
    } else if (work.state.yawVelocity < -yawLimit) {
        work.state.yawVelocity = -yawLimit;
        plain(work, 6);
    }
    plain(work, 4); //9FD5..9FE3: no turbo or joystick.
    plain(work, 2); //A005..A009: down has priority over up.
    if (guards.heldKeys[reverseKeyIndex]) {
        work.state.forwardVelocity -= forwardIncrement;
        plain(work, 3);
    } else {
        plain(work, 2); //A016..A01A.
        if (guards.heldKeys[forwardKeyIndex]) {
            work.state.forwardVelocity += forwardIncrement;
            plain(work, 2);
        }
    }
    plain(work, 9); //A024..A038.
    if (work.state.forwardVelocity > forwardLimit) {
        work.state.forwardVelocity = forwardLimit;
        plain(work, 8);
    } else if (work.state.forwardVelocity < -forwardLimit) {
        work.state.forwardVelocity = -forwardLimit;
        plain(work, 7);
    }
}

std::int32_t subtract(const std::int32_t left, const std::int32_t right) {
    const auto value = static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right);
    return value <= 0x7fffffffU ? static_cast<std::int32_t>(value)
        : static_cast<std::int32_t>(static_cast<std::int64_t>(value) - 0x100000000LL);
}

void normalize(FrameTailWork& work, std::int32_t& coordinate) {
    const auto high = coordinate >> 16;
    const auto low = static_cast<std::uint16_t>(coordinate);
    if (high > 0) {
        plain(work, 6);
    } else if (high == 0) {
        plain(work, low <= worldHalfPage ? 7 : 8);
    } else if (high < -1) {
        plain(work, 7);
    } else {
        plain(work, low >= worldHalfPage ? 7 : 9);
    }
    if (coordinate > worldHalfPage) {
        coordinate = subtract(coordinate, worldPage);
    } else if (coordinate < -worldHalfPage) {
        coordinate = subtract(coordinate, -worldPage);
    }
}

bool completeLevel2PeriodicAdmitted(const FrameTailGuards& guards) {
    constexpr std::uint8_t level=1;
    const auto limit=campaignLevelDefinition(level,0).table.enemyLimit;
    if(!guards.periodic) {return false;}
    const auto& input=*guards.periodic;
    return input.campaignLevel==level && input.activeEnemies>=0 && input.activeEnemies<=input.spawnedEnemies
        && input.spawnedEnemies>=0 && input.spawnedEnemies<=level2PostWaveSpawnLimit && input.enemyLimit==limit
        && input.spawnThreshold==campaignSpawnThreshold(level,static_cast<std::uint8_t>(input.activeEnemies))
        && input.enemyFlag && *input.enemyFlag<=1 && input.rngIndexA<=maximumRngIndex && input.rngIndexB<=maximumRngIndex
        && input.rngIndexA%2==0 && input.rngIndexB%2==0;
}

bool periodicAdmitted(const FrameTailGuards& guards, const TailDomain domain = TailDomain::level2,
    const Level1PlayerCombatDomain playerDomain = Level1PlayerCombatDomain::survivingOnly) {
    constexpr std::uint8_t level2 = 1; // Zero-based ANTARES profile.
    constexpr std::int16_t initialEnemies = 5;
    constexpr std::uint8_t level2EnemyLimit = 12;
    constexpr std::int16_t warningArmor = 200;
    if (!guards.periodic || (playerDomain==Level1PlayerCombatDomain::survivingOnly && guards.armor < warningArmor)) {
        return false;
    }
    const auto& input = *guards.periodic;
    const auto activeAdmitted=domain==TailDomain::level2Combat
        ? input.activeEnemies>=0 && input.activeEnemies<=initialEnemies
            && campaignSpawnThreshold(level2,static_cast<std::uint8_t>(input.activeEnemies))==0
        : input.activeEnemies==initialEnemies;
    return input.campaignLevel == level2 && activeAdmitted
        && input.spawnedEnemies == initialEnemies && input.enemyLimit == level2EnemyLimit
        && input.spawnThreshold == 0 && input.enemyFlag && *input.enemyFlag == 0
        && input.rngIndexA <= maximumRngIndex && input.rngIndexB <= maximumRngIndex
        && input.rngIndexA % 2 == 0 && input.rngIndexB % 2 == 0;
}

bool keyboardQueueAdmitted(const FrameTailGuards& guards, const KeyboardQueueDomain domain,
    const bool forwardHeld) {
    switch (guards.keyboardQueue) {
    case FrameTailKeyboardQueue::empty:
        return guards.keyboardHead == guards.keyboardTail;
    case FrameTailKeyboardQueue::forwardMake:
        return domain == KeyboardQueueDomain::ordinaryOrCappedPeriodic && forwardHeld
            && guards.keyboardHead >= firstKeyboardBufferOffset
            && guards.keyboardHead <= keyboardWrapOffset
            && guards.keyboardHead % keyboardEntryBytes == 0
            && guards.keyboardTail == (guards.keyboardHead == keyboardWrapOffset
                ? firstKeyboardBufferOffset : guards.keyboardHead + keyboardEntryBytes);
    }
    return false;
}

bool sameWork(const std::vector<NormalWorkOperation>& actual,
    const std::vector<NormalWorkOperation>& expected) {
    const auto compact = [](const auto& operations) {
        std::vector<NormalWorkOperation> result;
        for (const auto& operation : operations) {
            if (operation.count == 0 || operation.value != 0) { return std::vector<NormalWorkOperation>{}; }
            if (operation.kind == NormalWorkKind::instructions && !result.empty()
                && result.back().kind == operation.kind) {
                result.back().count += operation.count;
            } else {
                result.push_back(operation);
            }
        }
        return result;
    };
    const auto left = compact(actual);
    const auto right = compact(expected);
    return !left.empty() && left.size() == right.size()
        && std::equal(left.begin(), left.end(), right.begin(), [](const auto& a, const auto& b) {
            return a.kind == b.kind && a.count == b.count && a.value == b.value;
        });
}

void validateFire(const FrameTailState& state, const Level1TailFireAction& action,
    const PeriodicSpawnTailContext* periodicSpawn = nullptr, const TailDomain domain = TailDomain::level1Projectile,
    const Level2PeriodicTailContext* completeLevel2 = nullptr) {
    using Kind = NormalWorkKind;
    const auto level2=domain==TailDomain::level2Combat;
    const auto& fire = action.work;
    const auto effects = periodicSpawn ? periodicSpawn->combatEffects : nullptr;
    const auto bonus = effects && effects->profile == Level1CombatEffectProfile::systemBonus;
    const auto maximumCombatAllocations = (periodicSpawn && periodicSpawn->deployment ? 16U : periodicSpawn && periodicSpawn->arrival ? 15U : 14U) + (bonus ? 1U : 0U) + (periodicSpawn && periodicSpawn->completion!=TailCompletion::ordinary?1U:0U)
        + Level1State::projectilePoolSize + Level1State::deathEffectPoolSize;
    const auto maximumAllocationCount = completeLevel2 ? level2AllocationCapacity
        : level2 ? 7U+Level1State::projectilePoolSize+Level1State::deathEffectPoolSize
        : effects ? maximumCombatAllocations : periodicSpawn
        ? static_cast<std::size_t>(periodicSpawn->input.activeEnemies + 4) + Level1State::projectilePoolSize
        : level1ActorCount + Level1State::projectilePoolSize;
    if (periodicSpawn && (periodicSpawn->input.activeEnemies < 0
        || periodicSpawn->input.activeEnemies > (periodicSpawn->deployment?12:periodicSpawn->arrival?11:10)
        || fire.allocationCount != periodicSpawn->input.allocationCount
        || fire.allocationCount > maximumAllocationCount)) {
        throw std::invalid_argument("primary receipt must match the post-fire periodic registry");
    }
    if (fire.outcome != Level1PrimaryFireOutcome::launched) {
        std::uint64_t dispatches = 0;
        switch (fire.outcome) {
        case Level1PrimaryFireOutcome::cooldown:
            if (state.weaponTimers[0] != 0) { dispatches = 14; }
            break;
        case Level1PrimaryFireOutcome::weaponSlotsBusy:
            if (state.weaponTimers[0] == 0 && state.weaponTimers[1] != 0 && state.weaponTimers[2] != 0) {
                dispatches = 29;
            }
            break;
        case Level1PrimaryFireOutcome::poolFull:
            if (state.weaponTimers[0] == 0 && (state.weaponTimers[1] == 0 || state.weaponTimers[2] == 0)) {
                dispatches = state.weaponTimers[1] == 0 && state.weaponTimers[2] == 0 ? 173 : 101;
            }
            break;
        default: break;
        }
        if (dispatches == 0 || fire.launch || fire.mainCooldown != state.weaponTimers[0]
            || fire.weaponTimers != std::array<std::uint8_t, 2>{state.weaponTimers[1], state.weaponTimers[2]}
            || !sameWork(fire.beforeEvent14, {{Kind::instructions, dispatches}})
            || !fire.afterEvent14BeforeLamp.empty() || !fire.afterLamp.empty() || !action.lampOperations.empty()) {
            throw std::invalid_argument("malformed Level1 fire failure action");
        }
        return;
    }
    if (!fire.launch || state.weaponTimers[0] != 0) {
        throw std::invalid_argument("primary launch requires a ready cooldown and its old active list");
    }
    const auto& launch = *fire.launch;
    const auto& shot = launch.projectile;
    const auto weaponSlot = state.weaponTimers[1] == 0 ? 0U : 1U;
    auto timers = std::array<std::uint8_t, 2>{state.weaponTimers[1], state.weaponTimers[2]};
    if (timers[weaponSlot] != 0 || launch.weaponSlot != weaponSlot || launch.poolSlot >= Level1State::projectilePoolSize
        || shot.poolSlot != launch.poolSlot || shot.type != level1ProjectileType
        || (level2 ? (shot.flags&~4U)!=0 : shot.flags!=0)
        || shot.collisionRadius != 134 || !(shot.position == state.playerPosition)
        || shot.pitch != 0 || shot.roll != 0 || shot.zVelocity != 0
        || (!level2 && (shot.pitchVelocity != 0 || shot.rollVelocity != 0
            || shot.yawVelocity != 0 || shot.xVelocity != 0 || shot.yVelocity != 0))
        || shot.forwardVelocity != 500 || !shot.ownerIsPlayer
        || shot.ownerAllocationIndex != static_cast<std::size_t>(-1)
        || (level2 ? shot.stateBytes4dTo54[0]!=253 || shot.stateBytes4dTo54[1]!=64
                || shot.stateBytes4dTo54[2]!=0 || shot.stateBytes4dTo54[4]!=0
            : shot.stateBytes4dTo54 != std::array<std::uint8_t, 8>{253, 64, 0, 0, 0, 0, 0, 0})
        || (!effects && !level2 && fire.allocationCount < state.actors.size() + 1) || fire.allocationCount > maximumAllocationCount
        || launch.allocationIndex + 1 != fire.allocationCount || !fire.refreshRequested
        || !(fire.shotOrigin == state.playerPosition) || fire.mainCooldown != 9) {
        throw std::invalid_argument("malformed Level1 primary launch receipt");
    }
    timers[weaponSlot] = 32;
    const std::vector<NormalWorkOperation> expected{{Kind::instructions, 55 + 6 * weaponSlot + 7 * launch.poolSlot},
        {Kind::repeatString, 6}, {Kind::instructions, 3}, {Kind::string, 1}, {Kind::string, 1},
        {Kind::string, 1}, {Kind::instructions, 66}, {Kind::repeatString, 6}};
    if (fire.weaponTimers != timers || !sameWork(fire.beforeEvent14, expected)
        || !sameWork(fire.afterEvent14BeforeLamp, {{Kind::instructions, 3}})
        || !sameWork(fire.afterLamp, {{Kind::instructions, 6}}) || action.lampOperations.empty()
        || std::any_of(action.lampOperations.begin(), action.lampOperations.end(), [](const auto& operation) {
            return operation.count == 0 || (operation.kind != Kind::instructions && operation.kind != Kind::string
                && operation.kind != Kind::repeatString && operation.kind != Kind::port8Write && operation.kind != Kind::port16Write);
        })) {
        throw std::invalid_argument("malformed Level1 fire/lamp work ownership");
    }
}
}

std::size_t level1TailReadyLampCalls(std::array<std::uint8_t, 3> timers) noexcept {
    std::size_t calls = 0;
    if (timers[0] != 0 && --timers[0] == 0 && (timers[1] == 0 || timers[2] == 0)) { ++calls; }
    for (std::size_t slot = 1; slot < timers.size(); ++slot) {
        if (timers[slot] != 0 && --timers[slot] == 0 && timers[0] == 0) { ++calls; }
    }
    return calls;
}

static FrameTailWork tailOperations(const FrameTailState& state, const FrameTailGuards& guards,
    const TailDomain domain, StellarRng* rng, std::vector<NormalWorkOperation>* beforeEntryFade,
    const Level1TailFireAction* fire = nullptr,
    const std::vector<std::vector<NormalWorkOperation>>* readyLampOperations = nullptr,
    PlayerLifecycleTailContext* lifecycle = nullptr,
    PeriodicSpawnTailContext* periodicSpawn = nullptr, TailEntry entry = TailEntry::ordinary,
    Level2PeriodicTailContext* completeLevel2 = nullptr) {
    constexpr std::size_t activeListCapacity = 32;
    const auto returning=periodicSpawn && periodicSpawn->completion==TailCompletion::successReturn;
    const auto combat=domain==TailDomain::level2Combat;
    const auto level2 = domain == TailDomain::level2 || domain == TailDomain::level2Controls || combat;
    const auto controls = domain == TailDomain::level2Controls || combat;
    const auto periodic = (state.loopTick & periodicTickMask) == 0;
    const auto forwardHeld = guards.heldKeys[forwardKeyIndex];
    auto otherKeys = guards.heldKeys;
    otherKeys[forwardKeyIndex] = false;
    const auto heldPeriodic = state.loopTick == heldPeriodicTick
        && state.forwardVelocity == heldPeriodicVelocity && state.yawVelocity == 0
        && guards.keyboardQueue == FrameTailKeyboardQueue::empty;
    const auto cappedHeldPeriodic = state.loopTick >= firstCappedHeldPeriodicTick
        && state.loopTick <= lastCappedHeldPeriodicTick && state.forwardVelocity == forwardLimit
        && state.yawVelocity == 0;
    const auto queueDomain = periodic && !cappedHeldPeriodic
        ? KeyboardQueueDomain::otherPeriodic : KeyboardQueueDomain::ordinaryOrCappedPeriodic;
    if ((level2 && periodic
            && ((!controls && forwardHeld && !heldPeriodic && !cappedHeldPeriodic) || !(completeLevel2 ? completeLevel2PeriodicAdmitted(guards) : periodicAdmitted(guards,domain,lifecycle ? Level1PlayerCombatDomain::expiryAndLoss : Level1PlayerCombatDomain::survivingOnly))))
        || state.scheduler != FrameTailScheduler::none
        || state.playerPosition.z != 0
        || state.yawVelocity < -512 || state.yawVelocity > 512
        || state.forwardVelocity < -forwardLimit || state.forwardVelocity > forwardLimit
        || state.actors.size() > activeListCapacity || (!lifecycle && guards.armor < 0)
        || (level2 && guards.firstFrame && entry != TailEntry::firstLevel2)
        || guards.extraUpdate || guards.contactSlow
        || guards.success!=returning || guards.missionActive==returning || guards.joystickEnabled
        || !keyboardQueueAdmitted(guards, queueDomain, forwardHeld)
        || (domain == TailDomain::level2
            && std::any_of(otherKeys.begin(), otherKeys.end(), [](bool held) { return held; }))
        || (controls && (guards.heldKeys[0] || guards.heldKeys[2] || guards.heldKeys[3]
            || guards.heldKeys[5] || (!combat && guards.heldKeys[fireKeyIndex])))
        || std::any_of(guards.abilities.begin(), guards.abilities.end(), [](auto value) { return value != 0; })
        || (!projectileDomain(domain)
            && std::any_of(state.weaponTimers.begin(), state.weaponTimers.end(), [](auto value) { return value == 1; }))
        || std::any_of(state.actors.begin(), state.actors.end(), [domain, level2, combat, periodicSpawn, completeLevel2](const auto& actor) {
            return level2
                ? actor.type != 8 && actor.type != 13 && actor.type != 30
                    && !(completeLevel2 && (actor.type==1 || actor.type==4 || actor.type==7 || actor.type==44 || actor.type==17 || actor.type==20))
                    && !(combat && (actor.type==23 || actor.type==19 || actor.type==-1))
                : actor.type != level1TankType && actor.type != level1AircraftType && actor.type != level1FixedType
                    && !(projectileDomain(domain) && actor.type == level1ProjectileType)
                    && !(enemyProjectileDomain(domain) && (actor.type == level1EnemyProjectileType
                        || (periodicSpawn && airborneProjectileDomain(periodicSpawn->combatEffects) && actor.type==20)))
                    && !(periodicSpawn && actor.type == 3)
                    && !(periodicSpawn && periodicSpawn->arrival && actor.type == 7)
                    && !(periodicSpawn && periodicSpawn->deployment && (actor.type==44 || actor.type==22))
                    && !(periodicSpawn && periodicSpawn->combatEffects && actor.type == -1)
                    && !(periodicSpawn && periodicSpawn->combatEffects
                        && periodicSpawn->combatEffects->profile == Level1CombatEffectProfile::systemBonus
                        && actor.type == level1BonusType)
                    && !(periodicSpawn && periodicSpawn->completion!=TailCompletion::ordinary && actor.type==25);
        })) {
        throw std::invalid_argument("unsupported source frame-tail path");
    }
    FrameTailWork work{state, {}, guards.keyboardHead, 0};
    plain(work, 4); //0312 CALL;0f72 CMP,JZ,RET.
    plain(work, 18); //0315 CALL; zero918e timers and grounded height.
    if (guards.firstFrame && (!level2 || entry == TailEntry::firstLevel2)) {
        plain(work, 6); //0318..032C: guards, clearDI and CALL9D9E.
        *beforeEntryFade = std::move(work.operations);
        work.operations.clear();
        plain(work, 1); //032F CALL9E10, after the excluded9D9E callee.
    } else {
        plain(work, 5); //0318..032F: no extra/first-frame calls; CALL9E10.
    }
    plain(work, 9); //9e10..9e1e, includes CALL8bd0.
    plain(work, 2); //8bd0 PUSH SI,SUB SI.
    for (std::size_t query = 0; query < 8; ++query) {
        plain(work, 3); // Scanargument and far CALL.
        keyQuery(work);
        plain(work, 3); // Stackcleanup,OR,JZ.
        if (guards.heldKeys[query]) {
            plain(work, 1); //8BD0 resident direction accumulation.
        }
    }
    plain(work, 3); //8c6b..8c6e.
    plain(work, 3); //9e21..9e27, includes03ee CALLF.
    plain(work, 1); //03ee PUSHF saves enabled IF after the preceding queries.
    work.operations.push_back({NormalWorkKind::cli, 1});
    if (guards.keyboardQueue == FrameTailKeyboardQueue::forwardMake) {
        const auto readerWrap = guards.keyboardHead == keyboardWrapOffset;
        if (guards.sourceKeyboardEffects) {
            plain(work, readerWrap ? 13 : 12); //03f0..040e;0410 reloads ring start at wrap.
            work.operations.push_back({NormalWorkKind::keyboardConsumeHead, 1});
            plain(work, 1); //041a POP ES.
        } else {
            plain(work, readerWrap ? 15 : 14); // Nonempty reader through041a.
        }
    } else {
        plain(work, 8); //03f0..0402 empty-ring branch,041a POP ES.
    }
    work.operations.push_back({NormalWorkKind::popFlagsEnabled, 1});
    plain(work, 1); //041c RETF.
    if (guards.sourceKeyboardEffects) {
        work.operations.push_back({NormalWorkKind::keyboardStoreScan, 1,
            guards.keyboardQueue == FrameTailKeyboardQueue::forwardMake ? forwardMakeScan : std::uint16_t{0}});
        plain(work, 5); //9e30..9e39, joystick disabled.
    } else {
        plain(work, 6); //9e2c..9e39, joystick disabled.
    }
    plain(work, 3); //9ec4..9ec8 space query CALLF.
    keyQuery(work);
    if (fire) {
        plain(work, 3 + 1); //9ECD..9ED2 and9ED4 CALL05D4.
        const auto append = [&](const auto& operations) {
            work.operations.insert(work.operations.end(), operations.begin(), operations.end());
        };
        append(fire->work.beforeEvent14);
        if (fire->work.outcome == Level1PrimaryFireOutcome::launched) {
            work.operations.push_back({combat ? NormalWorkKind::gameplayLevel2PrimaryFireEvent
                : NormalWorkKind::gameplayPrimaryFireEvent, 1, gameplayPrimaryFireEvent});
            append(fire->work.afterEvent14BeforeLamp);
            append(fire->lampOperations);
            append(fire->work.afterLamp);
        }
        work.state.weaponTimers = {fire->work.mainCooldown, fire->work.weaponTimers[0], fire->work.weaponTimers[1]};
    }
    const auto level1DirectionHeld = domain != TailDomain::level2
        && (forwardHeld || guards.heldKeys[reverseKeyIndex]
            || guards.heldKeys[leftKeyIndex] || guards.heldKeys[rightKeyIndex]);
    if (level1DirectionHeld) {
        level1Direction(work, guards, fire ? 6 : 9);
    } else if (forwardHeld) {
        if (guards.keyboardQueue == FrameTailKeyboardQueue::forwardMake) {
            plain(work, 5); //9ecd..9edc: scan/action setup.
            plain(work, 3); //9ede MOV AX, PUSH AX, CALL0437.
            keyQuery(work); //9ee2..9ee6: Alt38 query; released in this bounded profile.
            plain(work, 2); //9ee7 ADD SP, 9eea OR AX.
            plain(work, 1); //9eec JZ.
            plain(work, 2); //9f0a SUB DI, 9f0c MOV BX.
            // Source163e..1660 searches eighteen eight-instruction actions; none match.
            plain(work, unmatchedActionCount * instructionsPerUnmatchedAction);
            plain(work, 1); //9f20 JMP.
            plain(work, 4); //9f2f CMP, 9f34 JZ, 9f39 CMP, 9f3d JNZ.
            work.keyboardHead = guards.keyboardTail;
            work.consumedScan = forwardMakeScan;
        } else {
            plain(work, 9); //9ecd..9f3d: nonzero direction, no9f3f JMP.
        }
        plain(work, 6); //9f42,9f46,9f62,9f66,9f72,9f76: keyboard/no yaw keys.
        plain(work, 9); //9f82..9f96 yaw magnitude branch.
        damp(work, work.state.yawVelocity, 80);
        plain(work, 9); //9fab..9fbf: damped magnitude <=512, no clamp.
        plain(work, 10); //9fd5..a020: no turbo/joystick/reverse; forward add10.
        work.state.forwardVelocity = static_cast<std::int16_t>(work.state.forwardVelocity + forwardIncrement);
        plain(work, 9); //a024..a038: forward magnitude compare200.
        if (work.state.forwardVelocity > forwardLimit) {
            work.state.forwardVelocity = forwardLimit;
            plain(work, 8); //a03a..a04f positive clamp andJMPa0ad.
        }
    } else {
        plain(work, fire ? 7 : 10); //9ECD..9F3F, or9ED7..9F3F after composed fire.
        plain(work, 11); //a052..a06d forward magnitude branch.
        damp(work, work.state.forwardVelocity, forwardIncrement);
        plain(work, 9); //a083..a097 yaw magnitude branch.
        damp(work, work.state.yawVelocity, 80);
    }
    plain(work, 9); //a0ad..a0ec no turbo/contact slowdown; RET.
    plain(work, 6); //0332 CALL0c6a and0c6a..0c72.
    if (state.actors.empty()) {
        plain(work, 1); //0c74 JMP.
    } else {
        plain(work, 1); //0c77 MOV DI.
        for (std::size_t index = 0; index < work.state.actors.size(); ++index) {
            auto& actor = work.state.actors[index];
            if(((periodicSpawn && periodicSpawn->arrival) || completeLevel2) && actor.type==7) {
                plain(work,index+1==work.state.actors.size()?11:12); //0C7A..0C8B +D17 loop.
                continue;
            }
            actor.position.x = subtract(actor.position.x, state.playerPosition.x);
            actor.position.y = subtract(actor.position.y, state.playerPosition.y);
            plain(work, index + 1 == work.state.actors.size() ? 22 : 23);
            if (periodic) {
                normalize(work, actor.position.x);
                normalize(work, actor.position.y);
            }
        }
    }
    work.state.playerPosition.x = 0;
    work.state.playerPosition.y = 0;
    plain(work, 15); //0d23..0d2a and0dbd..0ddb: player recenter andRET.
    if(periodic && completeLevel2) {
        plain(work,3); //0335 TEST/033A JNZ/033C CALL0DDC.
        auto recentered=work.state;
        ++recentered.loopTick; //Host return metadata only; guest03D1 has not executed here.
        completeLevel2->work=completeLevel2->plan(recentered);
        const auto& child=completeLevel2->work->operations;
        if(child.empty() || std::any_of(child.begin(),child.end(),[](const auto& operation) {return operation.count==0;})) {
            throw std::invalid_argument("Level2 periodic planner omitted its whole0DDC child");
        }
        const auto& input=*guards.periodic;
        const auto spawned=completeLevel2->work->spawn
            ? completeLevel2->work->spawn->spawnedEnemies : input.spawnedEnemies;
        if(!completeLevel2->arrival && *input.enemyFlag==1 && spawned>=input.enemyLimit) {
            throw std::invalid_argument("Level2 periodic tail reached the unsupported arrival constructor");
        }
        completeLevel2->operationOffset=work.operations.size();
        work.operations.insert(work.operations.end(),child.begin(),child.end());
        if(completeLevel2->arrival) {
            plain(work,1); //033F CALL46C8 after the complete0DDC child.
            completeLevel2->arrivalWork=completeLevel2->arrival(recentered,*completeLevel2->work,*input.enemyFlag);
            for(const auto& step:completeLevel2->arrivalWork->steps) {
                if(const auto* operations=std::get_if<std::vector<NormalWorkOperation>>(&step)) {
                    const auto base=work.operations.size();
                    work.operations.insert(work.operations.end(),operations->begin(),operations->end());
                    if(completeLevel2->arrivalWork->birth && !completeLevel2->arrivalCacheReset) {
                        const auto reset=std::find_if(operations->begin(),operations->end(),[](const auto& operation) {
                            return operation.kind==NormalWorkKind::repeatString && operation.count==entityResetBytes;
                        });
                        if(reset!=operations->end()) {
                            const auto offset=static_cast<std::size_t>(reset-operations->begin())+1;
                            //49F4's return precedes43DF in the following plain packet.
                            auto& packet=work.operations.at(base+offset);
                            if(packet.kind!=NormalWorkKind::instructions || packet.count<4) {
                                throw std::invalid_argument("Level2 arrival lost its49F4 reset boundary");
                            }
                            const auto remaining=packet.count-4;packet.count=4;
                            work.operations.insert(work.operations.begin()+base+offset+1,{NormalWorkKind::instructions,remaining});
                            completeLevel2->arrivalCacheReset=Level2PeriodicCacheReset{
                                base+offset+1,0x43df,completeLevel2->arrivalWork->birth->poolSlot};
                        }
                    }
                } else if(std::holds_alternative<Level1ArrivalAudioChild>(step)) {
                    work.operations.push_back({NormalWorkKind::gameplayLevel2ArrivalEvent,1,gameplayArrivalEvent});
                    if(lifecycle) {lifecycle->audioEvents.push_back({state.loopTick,gameplayArrivalEvent});}
                } else {throw std::invalid_argument("Level2 arrival excludes a prior selector owner");}
            }
        } else {plain(work,1+(*input.enemyFlag==0?13:20));}
    } else if (periodic && periodicSpawn) {
        plain(work, 3); //0335 TEST/033A JNZ/033C CALL0DDC.
        auto spawn = periodicSpawn->arrival && periodicSpawn->input.spawnedEnemies>=10
            ? level1PostWavePeriodicOperations(periodicSpawn->input,*rng,periodicSpawn->models)
             : periodicSpawn->input.selectorReuse==Level1SelectorReuseDomain::retainedType11
                ? level1RetainedPeriodicSpawnOperations(periodicSpawn->input,*rng,periodicSpawn->models)
                : level1PeriodicSpawnOperations(periodicSpawn->input, *rng, periodicSpawn->models);
        if(spawn.selectorRelease) {
            periodicSpawn->selectorReleases.push_back({Level1TailSelectorSegment::afterEntryFade,
                work.operations.size()+spawn.selectorRelease->operationOffset,spawn.selectorRelease->call});
        }
        work.operations.insert(work.operations.end(), spawn.beforeProvider.begin(), spawn.beforeProvider.end());
        if (spawn.provider) {
            for(auto& release:periodicSpawn->selectorReleases) {
                release.segment=Level1TailSelectorSegment::beforePeriodicProvider;
            }
            periodicSpawn->beforeProvider = std::move(work.operations);
            work.operations.clear();
        }
        work.operations.insert(work.operations.end(), spawn.afterProvider.begin(), spawn.afterProvider.end());
        *rng = spawn.rng;
        periodicSpawn->work = std::move(spawn);
        if(periodicSpawn->arrival) {
            plain(work,1); //033F CALL46C8.
            auto incoming=*periodicSpawn->arrival;
            std::vector<Level1Entity> projectedEntities;
            if(incoming.pool.selectorReuse==Level1SelectorReuseDomain::retainedType11) {
                validateLevel1SelectorProjection(incoming.pool);
                projectedEntities=*incoming.pool.retainedEntities;
                incoming.pool.retainedEntities=&projectedEntities;
            }
            incoming.player.position=work.state.playerPosition;
            if(periodicSpawn->work && periodicSpawn->work->spawn) {
                const auto& birth=*periodicSpawn->work->spawn;
                incoming.pool.activeEnemies=birth.activeEnemies;
                incoming.pool.spawnedEnemies=birth.spawnedEnemies;
                incoming.pool.allocationCount=static_cast<std::uint16_t>(birth.appendedAllocationIndex+1);
                incoming.pool.ordinaryPool.at(birth.poolSlot)={birth.actor.stateBytes4dTo52[1],
                    birth.actor.type==11,birth.actor.allocationIndex};
                if(incoming.pool.selectorReuse==Level1SelectorReuseDomain::retainedType11) {
                    if(birth.actor.allocationIndex==projectedEntities.size()) {projectedEntities.push_back(birth.actor);}
                    else {projectedEntities.at(birth.actor.allocationIndex)=birth.actor;}
                }
                if(birth.actor.allocationIndex==incoming.pool.nextEntityIndex) { ++incoming.pool.nextEntityIndex; }
            }
            auto arrival=incoming.pool.selectorReuse==Level1SelectorReuseDomain::retainedType11
                ? level1RetainedArrivalOperations(incoming,*rng,periodicSpawn->models)
                : level1ArrivalOperations(incoming,*rng,periodicSpawn->models);
            for(const auto& step:arrival.steps) {
                if(const auto* child=std::get_if<std::vector<NormalWorkOperation>>(&step)) {
                    work.operations.insert(work.operations.end(),child->begin(),child->end());
                } else if(const auto* release=std::get_if<Level1SelectorReleaseCall>(&step)) {
                    periodicSpawn->selectorReleases.push_back({Level1TailSelectorSegment::afterEntryFade,
                        work.operations.size(),*release});
                } else {
                    work.operations.push_back({NormalWorkKind::gameplayArrivalEvent,1,gameplayArrivalEvent});
                    if(lifecycle) { lifecycle->audioEvents.push_back({state.loopTick,gameplayArrivalEvent}); }
                }
            }
            *rng=arrival.rng;periodicSpawn->arrivalWork=std::move(arrival);
        } else { plain(work, 1 + 13); } //033F CALL46C8 and enemyFlag0 callee.
        if (!lifecycle) { plain(work, 9); } //0342..037E ordinary armor/cooldown guards.
    } else if (periodic) {
        const auto& input = *guards.periodic;
        plain(work, (lifecycle ? 57 : 66) + (input.rngIndexA == 0 ? 1 : 0) + (input.rngIndexB == 0 ? 1 : 0));
        //0335..037e includes CALL0ddc/RNG20, CALL46c8/RET13 and armor>=200.
        if (!level2) {
            const auto draw = rng->next();
            if (static_cast<std::uint8_t>(draw) < input.spawnThreshold) {
                if ((draw >> 8) != 0) {
                    throw std::invalid_argument("unsupported Level1 tail spawn branch");
                }
                plain(work, 2); //0E07/0E0B: zero high byte skips enemy selection.
            }
        } else {
            work.state.scheduler = FrameTailScheduler::zeroThresholdDraw;
        }
    } else {
        plain(work, lifecycle ? 2 : 9); //0335/033A, or alive path through037E.
    }
    if (lifecycle) {
        if (periodic) {
            //57 above stops after033F/46C8; warning precedes the armor sign test.
            plain(work, 2); //0342 CMP armor200/0348 JNL.
            if (guards.armor < warningArmor) {
                plain(work, 2); //034A TEST loop63/034F JNZ.
                if ((state.loopTick & warningTickMask) == 0) {
                    lifecycle->lowArmorWarningRequested = true;
                    plain(work, 1); //0351 writes129C=1.
                    work.operations.push_back({lifecycle->warningEventKind, 1, gameplayPlayerCombatEvent});
                    lifecycle->audioEvents.push_back({state.loopTick, gameplayPlayerCombatEvent});
                }
            }
        }
        plain(work, 2); //0360 CMP armor0/0365 JNL.
        if (guards.armor < 0) {
            lifecycle->missionActive = false;
            lifecycle->failureRequested = true;
            plain(work, 2); //0367 writes1299=0;036C writes12C2=1.
        }
        plain(work, 5); //0372..037E shared cooldown guard.
    }
    std::size_t readyOrdinal = 0;
    const auto readyLamp = [&]() {
        plain(work, 3); //0394..0398 or03C1..03C5: argument, PUSH, CALL7BAE.
        const auto& operations = readyLampOperations->at(readyOrdinal++);
        work.operations.insert(work.operations.end(), operations.begin(), operations.end());
        plain(work, 1); //039B or03C8 argument cleanup.
    };
    if (work.state.weaponTimers[0] != 0) {
        --work.state.weaponTimers[0];
        plain(work, 2); //0380 DEC,0382 JNZ.
        if (work.state.weaponTimers[0] == 0) {
            plain(work, 3); //0384..038C: slot0 availability before slot decrements.
            if (work.state.weaponTimers[1] != 0) { plain(work, 2); } //038E/0392 slot1 test.
            if (work.state.weaponTimers[1] == 0 || work.state.weaponTimers[2] == 0) { readyLamp(); }
        }
    }
    plain(work, 2); //039e..03a0.
    for (std::size_t slot = 1; slot < work.state.weaponTimers.size(); ++slot) {
        plain(work, 6); //03a3..03b1.
        if (work.state.weaponTimers[slot] != 0) {
            --work.state.weaponTimers[slot];
            plain(work, 2); //03b3 DEC,03b5 JNZ.
            if (work.state.weaponTimers[slot] == 0) {
                plain(work, 3); //03B7..03BF: shared cooldown after main decrement.
                if (work.state.weaponTimers[0] == 0) { readyLamp(); }
            }
        }
        plain(work, 3); //03cb..03cf.
    }
    ++work.state.loopTick;
    plain(work, lifecycle && !lifecycle->missionActive ? 5 : 6); //03D1..03E6; alive adds03E8 JMP.
    return work;
}

FrameTailWork frameTailOperations(const FrameTailState& state, const FrameTailGuards& guards) {
    return tailOperations(state, guards, TailDomain::level2, nullptr, nullptr);
}

FrameTailWork level2ControlFrameTailOperations(const FrameTailState& state, const FrameTailGuards& guards) {
    return tailOperations(state,guards,TailDomain::level2Controls,nullptr,nullptr);
}


static FrameTailWork level2CombatTailOperations(const FrameTailState& state,const FrameTailGuards& guards,
    const std::optional<Level1TailFireAction>& fire,
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations,
    PlayerLifecycleTailContext* lifecycle, Level2PeriodicTailContext* completeLevel2 = nullptr) {
    using Kind=NormalWorkKind;
    std::array<std::size_t,5> counts{};
    for(const auto& actor:state.actors) {
        const auto index=actor.type==8 || (completeLevel2 && (actor.type==1 || actor.type==4 || actor.type==7 || actor.type==44)) ? 0
            : actor.type==13 ? 1 : actor.type==30 ? 2
            : actor.type==23 || actor.type==19 || (completeLevel2 && (actor.type==17 || actor.type==20)) ? 3
            : actor.type==-1 ? 4 : 5;
        if(index==5) {throw std::invalid_argument("Level2 combat tail excludes another actor family");}
        ++counts[index];
    }
    //Keep the qualified signed-HUD result domain for the expanded periodic roster.
    if(guards.firstFrame || guards.armor<(lifecycle ? -level2MaximumFrameDamage : 200) || guards.armor>680
        || (completeLevel2 ? counts[0]+counts[1]>level2OrdinaryCapacity : counts[0]>3) || counts[1]>(completeLevel2 ? maximumLevel2PeriodicLiveType13 : 2) || counts[2]>2 || counts[3]>Level1State::projectilePoolSize
        || counts[4]>Level1State::deathEffectPoolSize || guards.heldKeys[fireKeyIndex]!=fire.has_value()) {
        throw std::invalid_argument("Level2 combat tail requires actual mixed OLD and current Space state");
    }
    if(fire) {validateFire(state,*fire,nullptr,TailDomain::level2Combat,completeLevel2);}
    const auto timers=fire ? std::array<std::uint8_t,3>{fire->work.mainCooldown,fire->work.weaponTimers[0],fire->work.weaponTimers[1]}
        : state.weaponTimers;
    if(readyLampOperations.size()!=level1TailReadyLampCalls(timers)
        || std::any_of(readyLampOperations.begin(),readyLampOperations.end(),[](const auto& operations) {
            return operations.empty() || std::any_of(operations.begin(),operations.end(),[](const auto& operation) {
                return operation.count==0 || (operation.kind!=Kind::instructions && operation.kind!=Kind::string
                    && operation.kind!=Kind::repeatString && operation.kind!=Kind::port8Write && operation.kind!=Kind::port16Write);
            });
        })) {throw std::invalid_argument("Level2 ready lamp children differ from source timer requests");}
    return tailOperations(state,guards,TailDomain::level2Combat,nullptr,nullptr,
        fire?&*fire:nullptr,&readyLampOperations,lifecycle,nullptr,TailEntry::ordinary,completeLevel2);
}

FrameTailWork level2CombatFrameTailOperations(const FrameTailState& state,const FrameTailGuards& guards,
    const std::optional<Level1TailFireAction>& fire,
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations) {
    return level2CombatTailOperations(state,guards,fire,readyLampOperations,nullptr);
}

Level2LifecycleTailWork level2LifecycleFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards,const std::optional<Level1TailFireAction>& fire,
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations) {
    PlayerLifecycleTailContext lifecycle;
    lifecycle.missionActive=guards.missionActive;
    lifecycle.warningEventKind=NormalWorkKind::gameplayLevel2PlayerCombatEvent;
    auto work=level2CombatTailOperations(state,guards,fire,readyLampOperations,&lifecycle);
    return {std::move(work),lifecycle.missionActive,lifecycle.failureRequested,
        lifecycle.lowArmorWarningRequested,std::move(lifecycle.audioEvents)};
}

Level2PeriodicFrameTailWork level2PeriodicFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards,const Level2PeriodicTailPlanner& plan,
    const std::optional<Level1TailFireAction>& fire,
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations,const Level2ArrivalTailPlanner& arrival) {
    if(!plan) {throw std::invalid_argument("Level2 periodic tail requires its canonical projection planner");}
    Level2PeriodicTailContext complete{plan,{},{},arrival,{},{}};
    PlayerLifecycleTailContext lifecycle;
    lifecycle.missionActive=guards.missionActive;
    lifecycle.warningEventKind=NormalWorkKind::gameplayLevel2PlayerCombatEvent;
    auto work=level2CombatTailOperations(state,guards,fire,readyLampOperations,&lifecycle,&complete);
    Level2LifecycleTailWork result{std::move(work),lifecycle.missionActive,lifecycle.failureRequested,
        lifecycle.lowArmorWarningRequested,std::move(lifecycle.audioEvents)};
    return {std::move(result),std::move(complete.work),complete.operationOffset,
        std::move(complete.arrivalWork),complete.arrivalCacheReset};
}

Level2FirstFrameTailWork level2FirstFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards) {
    constexpr std::size_t actorCount = 7;
    const auto countType = [&](std::int16_t type) {
        return std::count_if(state.actors.begin(),state.actors.end(),
            [type](const auto& actor) { return actor.type == type; });
    };
    if (!guards.firstFrame || state.loopTick != 0 || state.actors.size() != actorCount
        || countType(8) != 3 || countType(13) != 2 || countType(30) != 2
        || guards.armor < 200 || guards.armor > 680 || !periodicAdmitted(guards)
        || std::any_of(guards.heldKeys.begin(),guards.heldKeys.end(),[](bool held) { return held; })
        || std::any_of(state.weaponTimers.begin(),state.weaponTimers.end(),[](auto timer) { return timer != 0; })) {
        throw std::invalid_argument("first Level2 tail requires its released source constructor display");
    }
    std::vector<NormalWorkOperation> beforeFade;
    auto tail = tailOperations(state,guards,TailDomain::level2,nullptr,&beforeFade,
        nullptr,nullptr,nullptr,nullptr,TailEntry::firstLevel2);
    return {std::move(tail),std::move(beforeFade)};
}

static Level1FrameTailWork level1TailOperations(const FrameTailState& state,
    const FrameTailGuards& guards, const StellarRng& incomingRng, const TailDomain domain,
    const Level1TailFireAction* fire,
    const std::vector<std::vector<NormalWorkOperation>>* readyLampOperations = nullptr,
    PeriodicSpawnTailContext* periodicSpawn = nullptr) {
    constexpr std::int16_t initialArmor = 680;
    constexpr std::uint8_t initialEnemyLimit = 10;
    constexpr std::uint8_t initialSpawnThreshold = 10;
    constexpr std::int16_t initialEnemies = 2;
    constexpr auto minimumLifecycleArmor = -40 * static_cast<int>(Level1State::projectilePoolSize);
    const auto lifecycleDomain = domain == TailDomain::level1PlayerLifecycle;
    const auto rewardDomain=periodicSpawn && periodicSpawn->completion!=TailCompletion::ordinary;
    auto unsupportedKeys = guards.heldKeys;
    for (const auto key : {forwardKeyIndex, reverseKeyIndex, leftKeyIndex, rightKeyIndex}) {
        unsupportedKeys[key] = false;
    }
    const auto projectileCount = static_cast<std::size_t>(std::count_if(state.actors.begin(), state.actors.end(),
        [domain,periodicSpawn](const auto& actor) { return actor.type == level1ProjectileType
            || (enemyProjectileDomain(domain) && (actor.type == level1EnemyProjectileType
                || (periodicSpawn && airborneProjectileDomain(periodicSpawn->combatEffects) && actor.type==20)))
            || (periodicSpawn && periodicSpawn->deployment && actor.type==22); }));
    if (projectileDomain(domain)) { unsupportedKeys[fireKeyIndex] = false; }
    const auto ordinaryCount = periodicSpawn ? periodicSpawn->input.activeEnemies : initialEnemies;
    const auto expectedActors = static_cast<std::size_t>(ordinaryCount + 4) + projectileCount;
    const auto effects = periodicSpawn ? periodicSpawn->combatEffects : nullptr;
    if (effects && effects->projectileAllocationDomain!=Level1ProjectileAllocationDomain::freshOnly
        && effects->projectileAllocationDomain!=Level1ProjectileAllocationDomain::retained
        && effects->projectileAllocationDomain!=Level1ProjectileAllocationDomain::retainedAirborne
        && effects->projectileAllocationDomain!=Level1ProjectileAllocationDomain::retainedRoster) {
        throw std::invalid_argument("unsupported combat tail projectile domain");
    }
    const auto countType = [&](const auto type) {
        return static_cast<std::size_t>(std::count_if(state.actors.begin(), state.actors.end(),
            [type](const auto& actor) { return actor.type == type; }));
    };
    const auto bonusProfile = effects && effects->profile == Level1CombatEffectProfile::systemBonus;
    const auto bonusCount = countType(level1BonusType);
    const auto effectCount = countType(-1);
    const auto rewardCount=countType(25);
    const auto fixedCount = countType(level1FixedType);
    const auto enemyCount = countType(level1TankType) + countType(3) + countType(level1AircraftType);
    const auto arrival = periodicSpawn && periodicSpawn->arrival;
    const auto craftCount=countType(7);
    const auto guardianCount=countType(44);
    const auto deployment=periodicSpawn && periodicSpawn->deployment;
    const auto boundedEffectList = enemyCount+craftCount+guardianCount <= initialEnemyLimit+static_cast<unsigned>(arrival)+static_cast<unsigned>(deployment)
        && craftCount <= static_cast<unsigned>(arrival) && guardianCount<=static_cast<unsigned>(deployment) && fixedCount <= 4
        && bonusCount <= (bonusProfile ? 1U : 0U) && rewardCount <= (rewardDomain?1U:0U)
        && projectileCount <= Level1State::projectilePoolSize && effectCount <= Level1State::deathEffectPoolSize
        && state.actors.size() == enemyCount + craftCount + guardianCount + fixedCount + bonusCount + rewardCount + projectileCount + effectCount;
    if ((periodicSpawn && (ordinaryCount < 0 || ordinaryCount > initialEnemyLimit+static_cast<int>(arrival)+static_cast<int>(deployment)
            || periodicSpawn->input.spawnedEnemies < ordinaryCount
            || periodicSpawn->input.spawnedEnemies > initialEnemyLimit+(arrival?2:0)
            || periodicSpawn->input.enemyLimit != initialEnemyLimit))
        || (guards.firstFrame && state.loopTick != 0)
        || (effects ? !boundedEffectList : periodicSpawn ? projectileCount > Level1State::projectilePoolSize || state.actors.size() != expectedActors
            : domain == TailDomain::level1 ? state.actors.size() != level1ActorCount
                : projectileCount > Level1State::projectilePoolSize || state.actors.size() != level1ActorCount + projectileCount)
        || (enemyProjectileDomain(domain)
            ? guards.armor < (lifecycleDomain ? minimumLifecycleArmor : warningArmor) || guards.armor > initialArmor
            : guards.armor != initialArmor)
        || guards.keyboardQueue != FrameTailKeyboardQueue::empty
        || std::any_of(unsupportedKeys.begin(), unsupportedKeys.end(), [](const auto held) { return held; })
        || (!effects && (periodicSpawn
            ? std::count_if(state.actors.begin(), state.actors.end(), [](const auto& actor) {
                return actor.type == level1TankType || actor.type == 3 || actor.type == level1AircraftType;
            }) != ordinaryCount
            : std::count_if(state.actors.begin(), state.actors.end(), [](const auto& actor) { return actor.type == level1TankType; }) != 1
                || std::count_if(state.actors.begin(), state.actors.end(), [](const auto& actor) { return actor.type == level1AircraftType; }) != 1))
        || (!effects && std::count_if(state.actors.begin(), state.actors.end(), [](const auto& actor) { return actor.type == level1FixedType; }) != 4)
        || incomingRng.indexA() > maximumRngIndex || incomingRng.indexB() > maximumRngIndex
        || incomingRng.indexA() % 2 != 0 || incomingRng.indexB() % 2 != 0) {
        throw std::invalid_argument("unsupported Level1 tail profile");
    }
    if ((state.loopTick & periodicTickMask) == 0) {
        if (!guards.periodic || guards.periodic->campaignLevel != 0
            || guards.periodic->activeEnemies != ordinaryCount
            || guards.periodic->spawnedEnemies != (periodicSpawn ? periodicSpawn->input.spawnedEnemies : initialEnemies)
            || guards.periodic->enemyLimit != initialEnemyLimit
            || guards.periodic->spawnThreshold != (periodicSpawn
                ? campaignSpawnThreshold(0, static_cast<std::uint8_t>(ordinaryCount)) : initialSpawnThreshold)
            || guards.periodic->rngIndexA != incomingRng.indexA() || guards.periodic->rngIndexB != incomingRng.indexB()
            || guards.periodic->enemyFlag != std::optional<std::uint8_t>{arrival ? periodicSpawn->arrival->enemyFlag : std::uint8_t{0}}) {
            throw std::invalid_argument("unsupported Level1 tail scheduler");
        }
    }
    auto rng = incomingRng;
    std::vector<NormalWorkOperation> beforeEntryFade;
    PlayerLifecycleTailContext lifecycle;
    lifecycle.missionActive=guards.missionActive;
    auto work = tailOperations(state, guards, domain, &rng, &beforeEntryFade, fire, readyLampOperations,
        lifecycleDomain ? &lifecycle : nullptr, periodicSpawn);
    return {std::move(work.state), std::move(rng), std::move(beforeEntryFade), std::move(work.operations),
        guards.firstFrame, work.keyboardHead, work.consumedScan, lifecycle.missionActive,
        lifecycle.failureRequested, lifecycle.lowArmorWarningRequested, std::move(lifecycle.audioEvents),
        periodicSpawn ? std::move(periodicSpawn->selectorReleases) : std::vector<Level1TailSelectorRelease>{}};
}

Level1FrameTailWork level1FrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards, const StellarRng& incomingRng) {
    return level1TailOperations(state, guards, incomingRng, TailDomain::level1, nullptr);
}

static Level1FrameTailWork level1ArmedTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards, const StellarRng& incomingRng, const std::optional<Level1TailFireAction>& fire,
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations, const TailDomain domain,
    PeriodicSpawnTailContext* periodicSpawn = nullptr) {
    if (guards.heldKeys[fireKeyIndex] != fire.has_value()) {
        throw std::invalid_argument("Level1 Space state must match its primary fire action");
    }
    if (fire) { validateFire(state, *fire, periodicSpawn); }
    const auto timers = fire
        ? std::array<std::uint8_t, 3>{fire->work.mainCooldown, fire->work.weaponTimers[0], fire->work.weaponTimers[1]}
        : state.weaponTimers;
    if (readyLampOperations.size() != level1TailReadyLampCalls(timers)
        || std::any_of(readyLampOperations.begin(), readyLampOperations.end(), [](const auto& operations) {
            return operations.empty() || std::any_of(operations.begin(), operations.end(), [](const auto& operation) {
                return operation.count == 0 || (operation.kind != NormalWorkKind::instructions && operation.kind != NormalWorkKind::string
                    && operation.kind != NormalWorkKind::repeatString && operation.kind != NormalWorkKind::port8Write
                    && operation.kind != NormalWorkKind::port16Write);
            });
        })) {
        throw std::invalid_argument("ready lamp callees must match source timer requests exactly");
    }
    return level1TailOperations(state, guards, incomingRng, domain, fire ? &*fire : nullptr,
        &readyLampOperations, periodicSpawn);
}

Level1FrameTailWork level1ProjectileFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards, const StellarRng& incomingRng, const std::optional<Level1TailFireAction>& fire,
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations) {
    return level1ArmedTailOperations(state,guards,incomingRng,fire,readyLampOperations,TailDomain::level1Projectile);
}

Level1FrameTailWork level1CombatFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards, const StellarRng& incomingRng, const std::optional<Level1TailFireAction>& fire,
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations,
    const Level1PlayerCombatDomain combatDomain) {
    if (combatDomain != Level1PlayerCombatDomain::survivingOnly && combatDomain != Level1PlayerCombatDomain::expiryAndLoss) {
        throw std::invalid_argument("unsupported Level1 player-combat tail domain");
    }
    return level1ArmedTailOperations(state,guards,incomingRng,fire,readyLampOperations,
        combatDomain == Level1PlayerCombatDomain::expiryAndLoss ? TailDomain::level1PlayerLifecycle : TailDomain::level1Combat);
}


Level1PeriodicTailWork level1PeriodicCombatFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards, const StellarRng& incomingRng,
    const Level1PeriodicSpawnInput& periodicInput, const std::vector<Model3d>& sharedModels,
    const std::optional<Level1TailFireAction>& fire,
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations,
    const Level1PlayerCombatDomain combatDomain) {
    if (combatDomain != Level1PlayerCombatDomain::survivingOnly && combatDomain != Level1PlayerCombatDomain::expiryAndLoss) {
        throw std::invalid_argument("unsupported Level1 periodic player-combat domain");
    }
    PeriodicSpawnTailContext context{periodicInput, sharedModels, {}, {}};
    auto tail = level1ArmedTailOperations(state, guards, incomingRng, fire, readyLampOperations,
        combatDomain == Level1PlayerCombatDomain::expiryAndLoss ? TailDomain::level1PlayerLifecycle : TailDomain::level1Combat,
        &context);
    return {std::move(tail), std::move(context.work), std::move(context.beforeProvider)};
}

Level1PeriodicTailWork level1CombatEffectFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards, const StellarRng& incomingRng,
    const Level1PeriodicSpawnInput& periodicInput, const std::vector<Model3d>& sharedModels,
    const Level1CombatEffectMembership& membership,
    const std::optional<Level1TailFireAction>& fire,
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations,
    const Level1PlayerCombatDomain combatDomain) {
    constexpr std::size_t maximumOrdinary = 14;
    const auto bonus = membership.profile == Level1CombatEffectProfile::systemBonus;
    const auto namedCount = membership.ordinaryAllocations + membership.effectAllocations;
    const auto launched = fire && fire->work.outcome == Level1PrimaryFireOutcome::launched;
    if ((combatDomain != Level1PlayerCombatDomain::survivingOnly
            && combatDomain != Level1PlayerCombatDomain::expiryAndLoss)
        || (membership.profile != Level1CombatEffectProfile::ordinary && !bonus)
        || membership.bonusAllocations > (bonus ? 1U : 0U)
        || membership.bonusAllocations > membership.ordinaryAllocations
        || membership.ordinaryAllocations > maximumOrdinary + membership.bonusAllocations
        || (bonus && periodicInput.nextEntityIndex > level1BonusEntityCapacity)
        || membership.effectAllocations > Level1State::deathEffectPoolSize
        || membership.allocationCount < namedCount
        || membership.allocationCount - namedCount > Level1State::projectilePoolSize
        || periodicInput.allocationCount != membership.allocationCount + (launched ? 1U : 0U)
        || (launched && membership.allocationCount - namedCount == Level1State::projectilePoolSize)
        || (fire && fire->work.allocationCount != periodicInput.allocationCount)) {
        throw std::invalid_argument("invalid combat-effect tail registry");
    }
    PeriodicSpawnTailContext context{periodicInput, sharedModels, {}, {}, &membership};
    auto tail = level1ArmedTailOperations(state, guards, incomingRng, fire, readyLampOperations,
        combatDomain == Level1PlayerCombatDomain::expiryAndLoss ? TailDomain::level1PlayerLifecycle : TailDomain::level1Combat,
        &context);
    if (bonus && context.work && context.work->spawn
        && (context.work->spawn->actor.allocationIndex >= level1BonusEntityCapacity
            || membership.ordinaryAllocations >= level1BonusEntityCapacity)) {
        throw std::invalid_argument("periodic bonus tail would exceed entity indices0..14");
    }
    return {std::move(tail), std::move(context.work), std::move(context.beforeProvider)};
}

static Level1ArrivalTailWork arrivalTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards,const StellarRng& rng,const Level1ArrivalInput& arrival,
    const std::vector<Model3d>& models,const Level1CombatEffectMembership& membership,
    const std::optional<Level1TailFireAction>& fire,
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations,
    const Level1PlayerCombatDomain combatDomain,const bool deployment,
    const TailCompletion completion=TailCompletion::ordinary) {
    const auto bonus=membership.profile==Level1CombatEffectProfile::systemBonus;
    const auto named=membership.ordinaryAllocations+membership.effectAllocations;
    const auto launched=fire && fire->work.outcome==Level1PrimaryFireOutcome::launched;
    const auto craftCount=std::count_if(state.actors.begin(),state.actors.end(),[](const auto& a){return a.type==7;});
    if((combatDomain!=Level1PlayerCombatDomain::survivingOnly && combatDomain!=Level1PlayerCombatDomain::expiryAndLoss)
        || (membership.profile!=Level1CombatEffectProfile::ordinary && !bonus)
        || !(arrival.player.position==state.playerPosition)
        || arrival.player.yawVelocity!=state.yawVelocity || arrival.player.forwardVelocity!=state.forwardVelocity
        || (!deployment && arrival.guardianEntityIndex) || arrival.pool.spawnedEnemies<0 || arrival.pool.spawnedEnemies>12
        || craftCount>1 || (craftCount!=0 && !arrival.craftEntityIndex)
        || membership.bonusAllocations>(bonus?1U:0U) || membership.bonusAllocations>membership.ordinaryAllocations
        || membership.ordinaryAllocations>(deployment?16U:15U)+membership.bonusAllocations
            +(completion!=TailCompletion::ordinary?1U:0U)
        || membership.effectAllocations>Level1State::deathEffectPoolSize || membership.allocationCount<named
        || membership.allocationCount-named>Level1State::projectilePoolSize
        || arrival.pool.allocationCount!=membership.allocationCount+(launched?1U:0U)
        || (fire && fire->work.allocationCount!=arrival.pool.allocationCount)) {
        throw std::invalid_argument("unsupported post-wave arrival tail membership");
    }
    PeriodicSpawnTailContext context{arrival.pool,models,{},{},&membership,&arrival,{},deployment,completion};
    auto tail=level1ArmedTailOperations(state,guards,rng,fire,readyLampOperations,
        combatDomain==Level1PlayerCombatDomain::expiryAndLoss?TailDomain::level1PlayerLifecycle:TailDomain::level1Combat,&context);
    return {{std::move(tail),std::move(context.work),std::move(context.beforeProvider)},std::move(context.arrivalWork)};
}

Level1ArrivalTailWork level1ArrivalFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards,const StellarRng& rng,const Level1ArrivalInput& arrival,
    const std::vector<Model3d>& models,const Level1CombatEffectMembership& membership,
    const std::optional<Level1TailFireAction>& fire,
    const std::vector<std::vector<NormalWorkOperation>>& ready,const Level1PlayerCombatDomain domain) {
    return arrivalTailOperations(state,guards,rng,arrival,models,membership,fire,ready,domain,false);
}
Level1ArrivalTailWork level1DeploymentFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards,const StellarRng& rng,const Level1ArrivalInput& arrival,
    const std::vector<Model3d>& models,const Level1CombatEffectMembership& membership,
    const std::optional<Level1TailFireAction>& fire,
    const std::vector<std::vector<NormalWorkOperation>>& ready,const Level1PlayerCombatDomain domain) {
    return arrivalTailOperations(state,guards,rng,arrival,models,membership,fire,ready,domain,true);
}

Level1ArrivalTailWork level1RewardFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards,const StellarRng& rng,const Level1ArrivalInput& arrival,
    const std::vector<Model3d>& models,const Level1CombatEffectMembership& membership,
    const std::optional<Level1TailFireAction>& fire,
    const std::vector<std::vector<NormalWorkOperation>>& ready,const Level1PlayerCombatDomain domain) {
    if(!guards.missionActive || guards.success || guards.firstFrame || !arrival.guardianEntityIndex
        || arrival.pool.spawnedEnemies<10) {
        throw std::invalid_argument("reward tail requires its retained live Level1 guardian/reward family");
    }
    return arrivalTailOperations(state,guards,rng,arrival,models,membership,fire,ready,domain,true,TailCompletion::rewardLive);
}

Level1ArrivalTailWork level1SuccessFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards,const StellarRng& rng,const Level1ArrivalInput& arrival,
    const std::vector<Model3d>& models,const Level1CombatEffectMembership& membership,
    const std::optional<Level1TailFireAction>& fire,
    const std::vector<std::vector<NormalWorkOperation>>& ready) {
    if(guards.missionActive || !guards.success || guards.firstFrame || guards.armor<0
        || !arrival.guardianEntityIndex || arrival.pool.spawnedEnemies<10) {
        throw std::invalid_argument("inactive success tail requires its retained Level1 guardian return");
    }
    return arrivalTailOperations(state,guards,rng,arrival,models,membership,fire,ready,
        Level1PlayerCombatDomain::expiryAndLoss,true,TailCompletion::successReturn);
}

}
