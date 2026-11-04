#pragma once

#include "stellar7/level1_solid_contact_work.hpp"
#include "stellar7/target_hit_work.hpp"
#include "stellar7/score_work.hpp"

#include "stellar7/level1.hpp"
#include "stellar7/level1_object_ref.hpp"
#include "stellar7/level1_ordinary_family.hpp"
#include "stellar7/level1_projectile_allocation.hpp"
#include "stellar7/level1_player_combat.hpp"
#include "stellar7/palette_clock.hpp"
#include "stellar7/normal_clock.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace stellar7 {

struct Level2ProjectilePrefixReceipt;

struct KinematicsWork {
    Level1Entity actor;
    std::vector<NormalWorkOperation> operations;
};

// 3b3a entry through RET; excludes caller CALL. Updated pitch/roll must be zero.
KinematicsWork flatKinematicsOperations(const Level1Entity& actor);

struct ActorOverlapWork {
    bool overlaps;
    std::vector<NormalWorkOperation> operations;
};

// 3a1c entry through RET; source inclusive box test, not Euclidean distance.
ActorOverlapWork actorOverlapOperations(const Level1Entity& first,
    const Level1Entity& second);

struct ActorContactWrite {
    std::size_t actorIndex;
    std::size_t subjectIndex;
};

struct ActorContactWork {
    std::size_t contactIndex;
    std::vector<ActorContactWrite> contactWrites;
    std::vector<NormalWorkOperation> operations;
};

// 0c0a entry through RET. The subject is a member of the supplied actor storage.
ActorContactWork actorContactOperations(const std::vector<Level1Entity>& actors,
    const std::vector<std::size_t>& activeOrder, std::size_t subjectIndex);

struct StationaryLevel2PrefixInput {
    Level1Entity player;
    std::vector<Level1Entity> actors;
    std::vector<std::size_t> activeOrder;
    FixedPosition lastListCamera;
    bool refreshRequested;
    std::uint16_t fadeTicks;
    bool specialActorPresent;
    std::int16_t impulse;
    bool active;
    std::array<bool, 2> projectileHandlePresent;
};

struct StationaryLevel2PrefixWork {
    Level1Entity player;
    std::vector<Level1Entity> actors;
    std::vector<std::size_t> activeOrder;
    std::vector<NormalWorkOperation> operations;
    std::shared_ptr<const Level2ProjectilePrefixReceipt> projectileReceipt{};
};

// Strict stationary compatibility wrapper for the retained Level2 prefix.
StationaryLevel2PrefixWork stationaryLevel2PrefixOperations(
    const StationaryLevel2PrefixInput& input);

// Main01d0 through BEFORE02a3 in the bounded moving no-rebuild domain.
// The original stationary API remains available with its strict camera check.
StationaryLevel2PrefixWork retainedLevel2PrefixOperations(
    const StationaryLevel2PrefixInput& input);

// Explicit flat steering domain; same seven constructor actors/no rebuild or shots.
// Player yaw is unrestricted; yawVelocity remains within source +/-512.
StationaryLevel2PrefixWork controlledLevel2PrefixOperations(
    const StationaryLevel2PrefixInput& input);

struct Level1PrefixInput {
    Level1Entity player;
    std::vector<Level1Entity> actors;
    std::vector<std::size_t> allocationOrder;
    std::vector<std::size_t> activeOrder;
    FixedPosition lastListCamera;
    bool refreshRequested;
    StellarRng rng;
    std::uint16_t fadeTicks;
    bool specialActorPresent;
    std::int16_t impulse;
    bool active;
    std::array<bool, 2> projectileHandlePresent;
};

struct Level1PrefixWork {
    Level1Entity player;
    std::vector<Level1Entity> actors;
    std::vector<std::size_t> activeOrder;
    FixedPosition lastListCamera;
    bool refreshRequested;
    StellarRng rng;
    std::vector<NormalWorkOperation> operations;
};

//01D0 through BEFORE02A3: six live Level1 allocations, flat moving/turning player,
//Type1/11 non-contact behaviors and list retention/rebuild. Projectile creation
//at995E, other allocations, pitched motion and wide list displacements reject.
//The caller applies source style1/draw=back; no clock, rendering or model tick here.
Level1PrefixWork level1PrefixOperations(const Level1PrefixInput& input);

struct Level1ProjectilePrefixInput {
    Level1Entity player;
    std::vector<Level1Entity> entities;
    std::array<Level1Projectile, Level1State::projectilePoolSize> projectiles;
    std::vector<Level1ObjectRef> allocationOrder;
    std::vector<Level1ObjectRef> activeOrder;
    FixedPosition lastListCamera;
    bool refreshRequested;
    StellarRng rng;
    std::uint16_t fadeTicks;
    bool specialActorPresent;
    std::int16_t impulse;
    bool active;
    //128E/1290 oscillator handles, independent of live projectile pool membership.
    std::array<bool, 2> projectileHandlePresent;
};

struct Level1ProjectilePrefixWork {
    Level1Entity player;
    std::vector<Level1Entity> entities;
    std::array<Level1Projectile, Level1State::projectilePoolSize> projectiles;
    std::vector<Level1ObjectRef> allocationOrder;
    std::vector<Level1ObjectRef> activeOrder;
    FixedPosition lastListCamera;
    bool refreshRequested;
    StellarRng rng;
    std::vector<NormalWorkOperation> operations;
    std::int16_t impulse = 0;
    std::vector<Level1ContactReceipt> contacts{};
};

//01D0 through BEFORE02A3, six entities plus one to eight player Type23 shots.
//The old45D2 list owns update/AI/sort; late3768 rebuilds from the separate49F8
//list. At most its last fresh append may await refresh. No hit/expiry/ground/hostile path.
//Returns typed storage and identities; does not mutate the model or render.
Level1ProjectilePrefixWork level1ProjectilePrefixOperations(const Level1ProjectilePrefixInput& input);

struct Level1EnemyProjectileModel {
    std::uint8_t scaleShift;
    std::uint16_t baseRadius;
};

enum class Level1ActorContactDomain { none, ordinaryActors, ordinaryPlayer };

enum class Level1OrdinaryMembership { initialSix, periodicSpawned };

struct Level1EnemyPrefixProfile {
    std::uint16_t loopTick;
    std::uint16_t firstRegistryCapacity;
    Level1EnemyProjectileModel model17;
    //Type20 metadata is consumed only by explicit retainedAirborne allocation domain.
    Level1EnemyProjectileModel model20;
    Level1ProjectileAllocationDomain allocationDomain = Level1ProjectileAllocationDomain::freshOnly;
    Level1ActorContactDomain contactDomain = Level1ActorContactDomain::none;
    //Variable ordinary membership; class1 table choice is separately explicit.
    Level1OrdinaryMembership membership = Level1OrdinaryMembership::initialSix;
    Level1OrdinaryFamily ordinaryFamily = Level1OrdinaryFamily::type1;
};

struct Level1EnemyPrefixWork {
    Level1ProjectilePrefixWork prefix;
    std::vector<Level1AudioEvent> audioEvents;
};

//Retained01D0..02A3 combat domain: zero to eight primary/Type17 pool shots,
//98FA/995E allocations, owner-aware no-hit scans and late updated49F8 rebuild.
//Retained allocation also admits inactive flatFD Type17/23 pool records and
//pending primary appends with carried presenter fields; default remains fresh.
//Ordinary actor contacts opt into ordered+5F writes, callback3 bypass and
//callback1 avoidance/flat backout; default rejects contact and carried+5F.
//Returned state is the sole semantic update; audioEvents are observations only.
//Typed enemy-fire operations own9A1B..9A1F and the complete audio dispatch.
//retainedAirborne additionally owns Type11->20 birth/banked movement, lifetime,
//owner-aware contact, damage56 and retirement. Legacy freshOnly/retained reject20.
//Pitched/inherited nonzero motion and unsupported altitude remain rejected.
Level1EnemyPrefixWork level1EnemyPrefixOperations(const Level1ProjectilePrefixInput& input,
    const Level1EnemyPrefixProfile& profile);

enum class Level1DamagePaletteMode { residentVga, unsupported };

struct Level1DamagePrefixProfile {
    Level1EnemyPrefixProfile enemy;
    std::int16_t armor;
    std::uint16_t missionComplete = 0;
    std::uint16_t shieldTicks = 0;
    std::uint16_t abilityTicks = 0;
    Level1DamagePaletteMode paletteMode = Level1DamagePaletteMode::residentVga;
    Level1PlayerCombatDomain combatDomain = Level1PlayerCombatDomain::survivingOnly;
};

struct Level1PrefixPaletteStep {
    //Before this operation index: source CALL/resident11 have completed.
    //The caller executes the loaded077A leaf; resident return3 follow in work.
    std::size_t operationOffset;
    PaletteRequest request;
};

struct Level1DamagePrefixWork {
    Level1EnemyPrefixWork combat;
    std::int16_t armor;
    std::uint16_t fadeTicks;
    std::vector<Level1PrefixPaletteStep> paletteSteps;
};

//Damage-enabled01D0..02A3: surviving ordinary Type17 hit,481A swap-back retirement,
//terminal old-list AI/sort, late updated allocation rebuild and carried132C fade.
//input.fadeTicks is canonical; palette steps exclude loaded077A and own its
//resident1B38 prefix/return once. Event6 marker owns0E7F..0E86 and dispatch.
//expiryAndLoss also admits life1 Type17/23 final updates and initial armor0..680.
//All old-list work continues after lethal damage; only the later tail sets
//1299/12C2. Shield/ability/completion and other impacts remain rejected.
Level1DamagePrefixWork level1DamagePrefixOperations(const Level1ProjectilePrefixInput& input,
    const Level1DamagePrefixProfile& profile);

struct Level1TargetPrefixInput {
    Level1ProjectilePrefixInput prefix;
    std::array<Level1DeathEffect,Level1State::deathEffectPoolSize> effects;
    Level1TargetBonusHistory bonus;
    std::int16_t activeEnemies;
    std::uint32_t score;
};
struct Level1TargetPrefixProfile {
    Level1DamagePrefixProfile damage;
    std::uint16_t difficulty;
    Level1TargetHitDomain targetDomain = Level1TargetHitDomain::ordinaryTank;
};
struct Level1PrefixScoreStep {
    //The whole7970 child executes before this flat prefix operation. Its
    //operations/font providers/effects are excluded from flat prefix work.
    std::size_t operationOffset;
    ScoreWork work;
};
struct Level1PrefixTargetHit {
    std::size_t projectileSlot;
    std::size_t targetIndex;
    std::size_t operationOffset;
    Level1TargetHitWork work;
};
struct Level1TargetPrefixWork {
    Level1DamagePrefixWork damage;
    std::array<Level1DeathEffect,Level1State::deathEffectPoolSize> effects;
    Level1TargetBonusHistory bonus;
    std::int16_t activeEnemies;
    std::uint32_t score;
    std::vector<Level1PrefixScoreStep> scoreSteps;
    //Observations only; their ordinary work/audio is already in flat prefix.
    std::vector<Level1PrefixTargetHit> targetHits;
};

//Damage-enabled prefix plus ordinary Type23 target1/29 impact, FC birth and
//Type1 award20. Explicit initialOrdinary adds Type11/class3/award50 and its
//inactive retained storage. Fixed29 has no score. Old45D2 retains terminal
//update/AI/sort; only49F8
//changes immediately. Source score child executes once at each ordered marker.
//FC physics is skipped; its main0AC8/radar callbacks remain later render owners.
Level1TargetPrefixWork level1TargetPrefixOperations(const Level1TargetPrefixInput& input,
    const Level1TargetPrefixProfile& profile, const BitmapFont& scoreFont,
    const CockpitPageState& pages);

} // namespace stellar7
