#pragma once
#include "stellar7/arrival_prefix_work.hpp"
#include "stellar7/guardian_deployment_work.hpp"
#include "stellar7/guardian_reward_work.hpp"
namespace stellar7 {
struct Level1GuardianRewardPrefixContext {
    std::optional<std::size_t> rewardEntityIndex;
    std::uint16_t campaignIndex = 0;
};
struct Level1DeploymentPrefixContext {
    std::optional<std::size_t> craftEntityIndex;
    std::optional<std::size_t> guardianEntityIndex;
    Level1PeriodicSpawnInput pool;
    const std::vector<Model3d>& models;
    const Level1GuardianRewardPrefixContext* reward = nullptr;
};
struct Level1PrefixDeploymentChild {
    std::size_t operationOffset;
    std::variant<Level1DeploymentHeapChild,Level1DeploymentAudioChild,Level1GuardianDeathAudio,Level1RewardSuccessChild> child;
};
struct Level1PrefixDeploymentObservation {
    std::size_t entityIndex;
    std::optional<Level1PeriodicSpawnReceipt> guardianBirth;
    std::optional<std::size_t> launchedSlot;
    bool craftRetired = false;
};
struct Level1RewardSuccessPause {
    std::size_t selectedEntityIndex;
    std::size_t rewardEntityIndex;
    std::uint16_t incomingLoopTick;
    std::uint16_t sourceResume = 0x0809;
    std::size_t oldListCursor = 0;
};
struct Level1DeploymentPrefixWork {
    Level1BonusPickupPrefixWork prefix;
    std::vector<Level1PrefixDeploymentChild> children;
    std::vector<Level1PrefixDeploymentObservation> observations;
    std::vector<Level1Type42BonusReceipt> rewardBirths;
    std::optional<std::size_t> rewardEntityIndex;
    std::optional<Level1RewardContactWork> rewardContact;
    //When present, prefix is the0802 entry projection, not a post-success plan.
    //Keep its canonical records until the child returns; resume replans from
    //the retained model after success-owned display observations are applied.
    std::optional<Level1RewardSuccessPause> successPause;
};
//04D8 whole Type22 child, excluding0784 caller. Same returned owners as prefix;
//used for bounded component checks and by the shared prefix builder internally.
Level1TargetPrefixWork level1GuardianProjectileUpdateOperations(const Level1TargetPrefixInput& input,
    const Level1TargetPrefixProfile& profile,std::size_t slot,const Level1DeploymentPrefixContext& deployment);

//Explicit deployment01D0..02A3 profile. All previous prefix APIs stay strict.
//Children execute BEFORE flat operationOffset in returned order. Their caller
//intervals are absent from flat work; no synthetic zero-time dispatch is added.
//Late3768 includes births after the OLD physics/behavior traversal. Canonical
//caller commits returned pools/list/RNG once and initializes only actual4370
//birth projection ancestry. Legacy semantic Type44 update is not invoked.
Level1DeploymentPrefixWork level1DeploymentPrefixOperations(const Level1TargetPrefixInput& input,
    const Level1TargetPrefixProfile& profile,const BitmapFont& scoreFont,
    const CockpitPageState& pages,std::uint16_t firstBlockCapacity,const Model3d& model42,
    const Level1BonusPickupContext& pickup,const Level1DeploymentPrefixContext& deployment);
//Explicit whole lethal/reward continuation; previous deployment API still rejects
//lethal class5 andType25. Same physical pools/cache/RNG/list owners are returned.
Level1DeploymentPrefixWork level1GuardianRewardPrefixOperations(const Level1TargetPrefixInput& input,
    const Level1TargetPrefixProfile& profile,const BitmapFont& scoreFont,
    const CockpitPageState& pages,std::uint16_t firstBlockCapacity,const Model3d& model42,
    const Level1BonusPickupContext& pickup,const Level1DeploymentPrefixContext& deployment,
    const Level1GuardianRewardPrefixContext& reward);
//Pure0809..0312 continuation. postChild is current owned state after6F52 and
//caller cleanup, not a saved pre-modal/final frame. Does not repeat player
//kinematics/contact, reenter success, or import a later state. Complete0784
//remainder and0A9A once, then stop at inactive0312 without main/radar.
Level1DeploymentPrefixWork level1GuardianRewardResumeOperations(const Level1TargetPrefixInput& postChild,
    const Level1TargetPrefixProfile& profile,const BitmapFont& scoreFont,
    const CockpitPageState& pages,std::uint16_t firstBlockCapacity,const Model3d& model42,
    const Level1BonusPickupContext& pickup,const Level1DeploymentPrefixContext& deployment,
    const Level1GuardianRewardPrefixContext& reward,const Level1RewardSuccessPause& pause);
} //namespace stellar7
