#pragma once
#include "stellar7/level1_arrival_work.hpp"
#include "stellar7/level1_object_ref.hpp"
#include "stellar7/type42_bonus_work.hpp"
#include <variant>
namespace stellar7 {
enum class GuardianDeploymentDomain { level1, level2 };
enum class Level1DeploymentHeapKind { releaseSelector, allocateSelector };
struct Level1DeploymentHeapChild {
    Level1DeploymentHeapKind kind;
    std::size_t entityIndex;
    std::uint16_t requestedBytes;
    std::uint16_t sourceEntry;
    bool guestElapsedQualified = false;
};
enum class Level1DeploymentAudioKind { directEvent6, stopEvent17 };
struct Level1DeploymentAudioChild {
    Level1DeploymentAudioKind kind;
    std::uint16_t eventId;
    std::uint16_t callerBegin;
    std::uint16_t callerEnd;
    std::uint16_t callerDispatches;
};
using Level1DeploymentStep = std::variant<std::vector<NormalWorkOperation>,Level1DeploymentHeapChild,Level1DeploymentAudioChild>;
struct Level1GuardianBirthWork {
    StellarRng rng;
    std::optional<Level1PeriodicSpawnReceipt> birth;
    std::vector<Level1DeploymentStep> steps;
};
//477A..4818; existing guardian returns without touching the pool/RNG. Native
//DCCA/DCC6 intervals are explicit, unqualified providers; surrounding source
//instructions and zeroing execute once. Model44 is shared44 then local0.
//Level2 admits the first post-wave13->14 birth with one local selector byte.
Level1GuardianBirthWork level1GuardianConstructorOperations(const Level1Entity& craft,
    const std::optional<std::size_t>& guardian,const Level1PeriodicSpawnInput& pool,
    const StellarRng& rng,const std::vector<Model3d>& models,
    GuardianDeploymentDomain domain = GuardianDeploymentDomain::level1);
struct Level1Type7DeploymentInput {
    Level1Entity actor;
    std::optional<std::size_t> guardianEntityIndex;
    Level1PeriodicSpawnInput pool;
    StellarRng rng;
    std::vector<Level1ObjectRef> allocationOrder;
};
struct Level1Type7DeploymentWork {
    Level1Entity actor;
    StellarRng rng;
    std::optional<Level1PeriodicSpawnReceipt> guardianBirth;
    std::vector<Level1ObjectRef> allocationOrder;
    bool craftRetired = false;
    bool refreshRequested = false;
    std::vector<Level1DeploymentStep> steps;
};
bool isLevel1Type7DeploymentState(const Level1Entity& actor) noexcept;
//Complete A418/AF8C states0..4, excluding9298 caller. The supplied pool/list
//are call-time projections. Return receipts are the sole mutation, never a
//second semantic updater. B071..B075 stop17 and B050 constructor stay ordered.
Level1Type7DeploymentWork level1Type7DeploymentOperations(const Level1Type7DeploymentInput& input,
    const std::vector<Model3d>& models,
    GuardianDeploymentDomain domain = GuardianDeploymentDomain::level1);
enum class Level1GuardianBehaviorDomain { live, inheritedRetired };
struct Level1GuardianBehaviorInput {
    Level1Entity actor;
    Level1Entity player;
    std::uint16_t loopTick;
    std::array<Level1Projectile,Level1State::projectilePoolSize> projectiles;
    std::vector<Level1ObjectRef> allocationOrder;
    Level1GuardianBehaviorDomain domain = Level1GuardianBehaviorDomain::live;
};
struct Level1GuardianBehaviorWork {
    Level1Entity actor;
    std::array<Level1Projectile,Level1State::projectilePoolSize> projectiles;
    std::vector<Level1ObjectRef> allocationOrder;
    std::optional<std::size_t> launchedSlot;
    bool refreshRequested = false;
    std::vector<Level1DeploymentStep> steps;
};
//A440..A50C including ACEC/44BA and inherited-motion3B3A, excluding A418.
//A440's pool-full DS004E write is unsupported: reject transactionally before
//any result is committed. No field clearing beyond actual44BA/ACEC stores.
Level1GuardianBehaviorWork level1GuardianBehaviorOperations(const Level1GuardianBehaviorInput& input,
    const Model3d& model22);

enum class Level2GuardianEmission { actor48, pulse46 };
struct Level2GuardianBehaviorInput {
    Level1Entity actor;
    StellarRng rng;
    bool craftPresent = false;
    bool retainedContactPresent = false;
};
struct Level2GuardianBehaviorWork {
    Level1Entity actor;
    StellarRng rng;
    std::optional<Level2GuardianEmission> emission;
    std::vector<NormalWorkOperation> beforeEmission;
    std::vector<NormalWorkOperation> afterEmissionSuccess;
    std::vector<NormalWorkOperation> afterEmissionFailure;
};
//Whole A92E, excluding its required AE10/4506 child. The request is emitted at
//the actual CALL boundary; append exactly one returned continuation after that
//child. Includes the one B39E draw even for a retained dead OLD pointer. Radius
//1147 belongs to the physical actor across44/45; model45's radius is not copied.
Level2GuardianBehaviorWork level2GuardianBehaviorOperations(const Level2GuardianBehaviorInput& input);

struct Level2GuardianPulseInput {
    Level1Entity source;
    std::array<Level1BonusFixedSlot,level1BonusFixedPoolSize> fixedSlots{};
    std::size_t nextEntityIndex = 0;
    std::vector<Level1ObjectRef> allocationOrder;
};
struct Level2GuardianFixedBirth {
    std::size_t poolSlot;
    std::size_t entityIndex;
    std::size_t appendedAllocationIndex;
    bool materializeEntity;
    Level1Entity actor;
};
struct Level2GuardianPulseWork {
    std::optional<Level2GuardianFixedBirth> birth;
    std::vector<Level1ObjectRef> allocationOrder;
    std::vector<NormalWorkOperation> operations;
};
//4506(46) plus A9FA's position copy is represented by the birth; the caller's
//copy instructions remain in afterEmissionSuccess. Partial FE reset retains
//all unspecified fields. Full pool is a source no-op; unknown bytes reject.
Level2GuardianPulseWork level2GuardianPulseOperations(const Level2GuardianPulseInput& input,
    const Model3d& model46);

struct Level2GuardianActorEmissionInput {
    Level1Entity source;
    Level1PeriodicSpawnInput pool;
    StellarRng rng;
    std::uint32_t visualRandomState;
};
struct Level2GuardianActorEmissionWork {
    StellarRng rng;
    std::uint32_t visualRandomState;
    std::optional<Level1PeriodicSpawnReceipt> birth;
    std::vector<Level1DeploymentStep> steps;
};
//Whole AE10(source,0), including provisional4370(13) and E078. No allocation
//means no random draws. Heap selector release remains an ordered child. Actor
//radius559 survives the48/47 resource-model changes (whose radii are291).
Level2GuardianActorEmissionWork level2GuardianActorEmissionOperations(
    const Level2GuardianActorEmissionInput& input,const std::vector<Model3d>& models);
} //namespace stellar7
