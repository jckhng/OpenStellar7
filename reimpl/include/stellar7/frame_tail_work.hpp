#pragma once

#include "stellar7/level1.hpp"
#include "stellar7/level1_primary_fire_work.hpp"
#include "stellar7/level1_player_combat.hpp"
#include "stellar7/normal_clock.hpp"
#include "stellar7/level1_periodic_spawn_work.hpp"
#include "stellar7/level1_arrival_work.hpp"

#include <array>
#include <optional>
#include <vector>

namespace stellar7 {

struct FrameTailActor {
    std::int16_t type;
    FixedPosition position;
};

enum class FrameTailScheduler { none, zeroThresholdDraw };
enum class FrameTailKeyboardQueue { empty, forwardMake };

struct FrameTailState {
    std::uint16_t loopTick;
    FixedPosition playerPosition;
    std::int16_t yawVelocity;
    std::int16_t forwardVelocity;
    std::array<std::uint8_t, 3> weaponTimers;
    std::vector<FrameTailActor> actors;
    FrameTailScheduler scheduler = FrameTailScheduler::none;
};

struct FrameTailPeriodicInput {
    std::uint8_t campaignLevel;
    std::int16_t activeEnemies;
    std::int16_t spawnedEnemies;
    std::uint8_t enemyLimit;
    std::uint8_t spawnThreshold;
    std::uint16_t rngIndexA;
    std::uint16_t rngIndexB;
    std::optional<std::uint8_t> enemyFlag;
};

struct FrameTailGuards {
    std::int16_t armor;
    //12ce,12d0,12d2,12d6,12d8,12da; admitted source path requires zero.
    std::array<std::uint16_t, 6> abilities;
    // Scan4f,50,51,47,48,49,4b,4d and39.
    std::array<bool, 9> heldKeys;
    std::uint16_t keyboardHead;
    std::uint16_t keyboardTail;
    bool joystickEnabled;
    bool firstFrame;
    bool extraUpdate;
    bool contactSlow;
    bool success;
    bool missionActive;
    std::optional<FrameTailPeriodicInput> periodic;
    FrameTailKeyboardQueue keyboardQueue = FrameTailKeyboardQueue::empty;
    bool sourceKeyboardEffects = false;
};

struct FrameTailWork {
    FrameTailState state;
    std::vector<NormalWorkOperation> operations;
    std::uint16_t keyboardHead = 0;
    std::uint16_t consumedScan = 0;
};

//8552/0312 through03e8 JMP; excludes next01d0. No clock or model tick.
// Released/forward resident keys; empty or one Up word; no abilities/expiry/death/success.
// Periodic forward admits raw16/velocity120/empty and raw32/48/64/velocity200,
// yaw0, with empty or one Up word, including the ring wrap.
// An explicit forwardMake queue record is a
// controlled foreground contract; it does not implement physical keyboard delivery.
// Periodic work admits only Level2 counts5/5, limit12, threshold0, armor>=200,
// returned enemyFlag0.
// CLI/STI/POPF markers each own one dispatch; shared clock owns their IF effects.
FrameTailWork frameTailOperations(const FrameTailState& state, const FrameTailGuards& guards);

// Explicit Level2 arrow controls; source priorities/increments/clamps and the
// existing counts5/5 zero-threshold periodic scheduler. No first fade or fire.
FrameTailWork level2ControlFrameTailOperations(const FrameTailState& state, const FrameTailGuards& guards);

enum class Level1TailSelectorSegment { beforePeriodicProvider, afterEntryFade };
struct Level1TailSelectorRelease {
    Level1TailSelectorSegment segment;
    std::size_t operationOffset;
    Level1SelectorReleaseCall call;
};
struct Level1FrameTailWork {
    FrameTailState state;
    StellarRng rng;
    std::vector<NormalWorkOperation> beforeEntryFade;
    std::vector<NormalWorkOperation> afterEntryFade;
    bool entryFade;
    std::uint16_t keyboardHead;
    std::uint16_t consumedScan;
    bool missionActive = true;
    bool failureRequested = false;
    //Write receipt for129C=1; false does not clear retained warning state.
    bool lowArmorWarningRequested = false;
    std::vector<Level1AudioEvent> audioEvents;
    //Excluded native DCCA children, each before the indexed operation in its segment.
    std::vector<Level1TailSelectorRelease> selectorReleases;
};

// Level1 tails0312->03E8 with empty BDA and resident Up/Down/Left/Right keys.
// No fire, abilities, timer expiry, projectiles, death or success. Left wins
// right; down wins up. Ordinary uint16 loop values include periodic/wrap paths.
// First beforeEntryFade ends with032C CALL9D9E (28 dispatches); the caller owns
// the complete9D9E callee/palette work. AfterEntryFade resumes at032F CALL9E10.
// Later work is entirely afterEntryFade. The live RNG advances once at each
// periodic0DDC; counts2/2, limit10, low>=threshold10 or high0 admit no spawn. No model tick,
// palette, callback or clock work is included. Returned scheduler staysnone.
Level1FrameTailWork level1FrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards, const StellarRng& rng);

struct Level1TailFireAction {
    Level1PrimaryFireWork work;
    std::vector<NormalWorkOperation> lampOperations;
};

//Explicit Level2 mixed OLD, current Space and complete7BAE children. Positive
//spawn-threshold/low-armor branches remain unsupported; old wrappers stay strict.
FrameTailWork level2CombatFrameTailOperations(const FrameTailState& state, const FrameTailGuards& guards,
    const std::optional<Level1TailFireAction>& fire = {},
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations = {});

struct Level2LifecycleTailWork {
    FrameTailWork work;
    bool missionActive = true;
    bool failureRequested = false;
    bool lowArmorWarningRequested = false;
    std::vector<Level1AudioEvent> audioEvents;
};

//Explicit Level2 source lifecycle: armor[-896,680], same mixed OLD/controls/fire.
//Scheduler receipt stays zeroThresholdDraw; the existing model owns its RNG draw.
//Warning3 precedes signed loss test; zero is alive. Timers/recenter/loop complete.
//False warning receipt does not clear129C. No model/audio/clock mutation.
Level2LifecycleTailWork level2LifecycleFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards, const std::optional<Level1TailFireAction>& fire = {},
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations = {});

//Source0380..03CF request count, in main/slot0/slot1 order. Input timers are
//after the optional fire action and before these decrements; no state is changed.
std::size_t level1TailReadyLampCalls(std::array<std::uint8_t, 3> timers) noexcept;

//Separate mixed-list domain: six ordinary Level1 references plus up to eight23.
//Space must match fire presence. A successful launch uses the OLD active refs;
//the new projectile is not recentered until it enters the rebuilt active list.
//Owns9ED4 CALL, supplied05D4 work,0710..0717 event14 and complete supplied7BAE
//callee once. Returned fire timers feed the ordinary0380..03CF decrements.
//readyLampOperations contains exactly one complete7BAE callee per source ready
//request (zero to two), with sequential dirty state; no caller work is included.
//No allocation/model mutation, audio execution, lamp pixels or page-role commit.
Level1FrameTailWork level1ProjectileFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards, const StellarRng& rng,
    const std::optional<Level1TailFireAction>& fire = {},
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations = {});

//Same tail ownership with Type17 enemy shots in the complete old active list.
//The source0C6A recenter pass treats17 like the admitted entities and Type23.
//Default admits surviving armor200..680. expiryAndLoss admits ordinary low/lethal
//armor, event3 warning,0367/036C flags and the terminal03E6 branch to03EB.
//Stops before03EB on loss; destruction/Continue callers belong to the caller.
Level1FrameTailWork level1CombatFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards, const StellarRng& rng,
    const std::optional<Level1TailFireAction>& fire = {},
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations = {},
    Level1PlayerCombatDomain combatDomain = Level1PlayerCombatDomain::survivingOnly);

struct Level1PeriodicTailWork {
    Level1FrameTailWork tail;
    std::optional<Level1PeriodicSpawnWork> periodic;
    std::vector<NormalWorkOperation> beforePeriodicProvider;
};

//Explicit scheduler domain; previous APIs retain their no-spawn guards.
//Old active refs contain four fixed29 plus live ordinary1/3/11 and up to eight
//17/23. A spawn appends only to the allocation registry; its new actor must not
//be added to the current recenter list. Caller commits the returned receipt once.
//Primary fire occurs first inside9E10; periodicInput.allocationCount is the
//post-fire registry count and must match fire.work.allocationCount when supplied.
//The source033C periodic call then appends the ordinary birth after that shot.
//When periodic.provider exists, order is tail.beforeEntryFade/palette (if first),
//beforePeriodicProvider, native provider, tail.afterEntryFade. Otherwise retain
//the ordinary tail segment order. periodic child vectors are receipt metadata,
//already composed in these parent segments; do not execute them a second time.
Level1PeriodicTailWork level1PeriodicCombatFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards, const StellarRng& rng,
    const Level1PeriodicSpawnInput& periodicInput, const std::vector<Model3d>& sharedModels,
    const std::optional<Level1TailFireAction>& fire = {},
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations = {},
    Level1PlayerCombatDomain combatDomain = Level1PlayerCombatDomain::expiryAndLoss);

//Explicit FC membership profile. Context describes live pre-fire49F8; periodic
//input describes post-fire49F8. The complete old45D2 list is recentered once,
//including removed targets and terminal FC. No callback/age work is added.
//Caller validates typed identities/order; this leaf validates bounded type counts.
//systemBonus explicitly adds one FE42 (including a terminal old-list record
//independent of live bonusAllocations), at most15 entity indices0..14 and31 old
//refs. Ordinary defaults remain strict. Old recenter still precedes periodic
//spawn; a new periodic entityindex15 rejects before any caller commit.
Level1PeriodicTailWork level1CombatEffectFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards, const StellarRng& rng,
    const Level1PeriodicSpawnInput& periodicInput, const std::vector<Model3d>& sharedModels,
    const Level1CombatEffectMembership& membership,
    const std::optional<Level1TailFireAction>& fire = {},
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations = {},
    Level1PlayerCombatDomain combatDomain = Level1PlayerCombatDomain::expiryAndLoss);

struct Level1ArrivalTailWork {
    Level1PeriodicTailWork periodic;
    //Metadata only: child operations/audio marker already appear in tail order.
    std::optional<Level1ArrivalWork> arrival;
};
//Explicit arrival domain (spawned0..12). Existing old refs include at most
//one Type7, skipped by0C85..0C8B; no subtraction/normalization for that record.
//033C normal0DDC belowquota/limit-return at10..12 precedes033F/46C8.
//A periodic birth updates the constructor input first. It reads recentered XY.
//Arrival child emits timed gameplayArrivalEvent once; root commits receipt only.
Level1ArrivalTailWork level1ArrivalFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards,const StellarRng& rng,const Level1ArrivalInput& arrival,
    const std::vector<Model3d>& models,const Level1CombatEffectMembership& membership,
    const std::optional<Level1TailFireAction>& fire = {},
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations = {},
    Level1PlayerCombatDomain combatDomain = Level1PlayerCombatDomain::expiryAndLoss);

//Deployment extends only this explicit arrival tail profile with one44 and
//Type22 shots.0C6A skips7, recentering44/22 normally.46C8 retains real craft/
//guardian gates, and reached post-wave0DDC is the existing limit path.
Level1ArrivalTailWork level1DeploymentFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards,const StellarRng& rng,const Level1ArrivalInput& arrival,
    const std::vector<Model3d>& models,const Level1CombatEffectMembership& membership,
    const std::optional<Level1TailFireAction>& fire = {},
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations = {},
    Level1PlayerCombatDomain combatDomain = Level1PlayerCombatDomain::expiryAndLoss);

//Explicit live guardian reward family; canonical128E must be known to caller.
//Admits at most one Type25 in the COMPLETE old list (birth may not yet appear).
//Default deployment domain stays strict. Same0312..03E8 active tail/children.
Level1ArrivalTailWork level1RewardFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards,const StellarRng& rng,const Level1ArrivalInput& arrival,
    const std::vector<Model3d>& models,const Level1CombatEffectMembership& membership,
    const std::optional<Level1TailFireAction>& fire = {},
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations = {},
    Level1PlayerCombatDomain combatDomain = Level1PlayerCombatDomain::expiryAndLoss);

//Explicit Type25GREEN return at0312 AFTER the paused0784/0A9A continuation.
//missionActive=false/success=true; emits mandatory input/recenter/periodic/timer
//tail once and stops03EB. No player motion integration, display, oscillator or
//cleanup. Same source children/fire/ready-lamp ownership as deployment tail.
Level1ArrivalTailWork level1SuccessFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards,const StellarRng& rng,const Level1ArrivalInput& arrival,
    const std::vector<Model3d>& models,const Level1CombatEffectMembership& membership,
    const std::optional<Level1TailFireAction>& fire = {},
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations = {});

}
