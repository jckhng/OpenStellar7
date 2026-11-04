#include "stellar7/recovered_level2_model.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::size_t sharedModelCount=44;
constexpr std::uint16_t playerRadius=723;

bool sameRng(StellarRng first,StellarRng second) noexcept {
    if(first.indexA()!=second.indexA() || first.indexB()!=second.indexB()
        || first.last()!=second.last() || first.calls()!=second.calls()) {return false;}
    for(std::size_t i=0;i<StellarRng::wordCount;++i) {
        if(first.next()!=second.next()) {return false;}
    }
    return true;
}
}

bool GameplayModel::recoveredLevel2ContinueReady() const noexcept {
    return failureFromLevel2_ && activeCampaignLevel_==1 && level1_.levelIndex_==1
        && level2FailureTailFinished_ && level2HasAdvanced_ && level2TailConsumed_
        && level2PlayerArmor_<0 && !level2Complete_ && failureSourceLoopTick_
        && recoveredFailureControl_ && recoveredFailureControl_->entered && recoveredFailureControl_->finished
        && recoveredFailureControl_->display==failureExplosionFrames && failureDebris_.empty()
        && level1_.entities_.empty() && !level1_.successState_;
}

void GameplayModel::beginRecoveredLevel2Retry() {
    if(phase_!=GameplayPhase::continuePrompt || !recoveredLevel2ContinueReady()
        || recoveredContinueDecision_!=ContinueDecision::yes || remainingContinues_==0
        || level1_.selectedSystem_!=-1
        || std::any_of(level1_.systemStatusLevels_.begin(),level1_.systemStatusLevels_.end(),
            [](const auto level) {return level<=0;})
        || !sameRng(failureRng_,level2Rng_)) {
        throw std::invalid_argument("Level2 retry requires its actual finished Yes and failure-return owners");
    }
    //Root ranks the actual score/current FAME through040C before these writes.
    level1_.failureState_=false; //040F.
    level1_.failurePending_=false;level1_.failureTailConsumed_=false;
    level2Score_=0;level1_.score_=0; //0417/041A.
    --remainingContinues_;--level1_.levelIndex_; //041D/041E.
    level2PlayerArmor_=680;level1_.playerArmor_=680; //0422.
    level2PlayerCollisionMomentum_=0;level1_.playerCollisionMomentum_=0; //046F.
    ++level1_.levelIndex_; //0475: the same Level2 index1.
    campaignExitFadeTicks_=0;damageFlashActive_=false;damageFlashAge_=0; //019F.
    //Project the current source12C8/417C retained by failure7307/3768.
    //No increment, list rebuild, camera write or new RNG execution occurs here.
    level2LoopTick_=*failureSourceLoopTick_;level1_.loopTick_=level2LoopTick_;
    level1_.projectionListOrigin_=recoveredFailureControl_->lastListCamera;
    level1_.missionActive_=false; //Projection of the already consumed lethal1299 clear.
    level2Rng_=failureRng_;
    recoveredContinueDecision_.reset();
    //Invalidate the previous loader observation, without advancing the oscillator.
    lastCockpitInitialization_.displaySteps=0;
    phase_=GameplayPhase::level2LoaderReturn;
}

RecoveredLevel2PoolResetWork GameplayModel::resetRecoveredLevel2RetryPools(
    const std::vector<Model3d>& shared,const std::vector<Model3d>& local) {
    if(phase_!=GameplayPhase::level2LoaderReturn || !failureFromLevel2_ || activeCampaignLevel_!=1
        || level1_.levelIndex_!=1 || !level2FailureTailFinished_ || !failureSourceLoopTick_
        || level1_.missionActive_ || level1_.successState_ || level1_.failureState_ || recoveredContinueDecision_
        || lastCockpitInitialization_.displaySteps!=0 || level2PlayerArmor_!=680 || level2Score_!=0
        || !sameRng(failureRng_,level2Rng_) || shared.size()!=sharedModelCount
        || shared.front().scaleShift!=0 || campaignModelRadius(shared,local,0)!=playerRadius) {
        throw std::invalid_argument("Level2 retry reset requires its current same-level48D4 entry");
    }
    Level2PoolResetInput input{};
    input.ordinary=level2ActorPool_;input.fixed=level2FixedPool_;
    input.projectiles=level2ProjectilePool_;input.effects=level2DeathEffectPool_;
    input.player=level2Player_;input.armor=level2PlayerArmor_;input.systemTimers=level1_.systemTimers_;
    //All physical live and inactive records enter the same48D4 body. Upper8
    //fixed records survive; no Level1 logical/pool projection is performed.
    return commitRecoveredLevel2PoolReset(level2PoolReset(input,shared.front()));
}

Level2LoaderPageInput GameplayModel::prepareRecoveredLevel2RetryLoaderPages(const CockpitSettings& settings) {
    if(phase_!=GameplayPhase::level2LoaderReturn || !failureFromLevel2_ || activeCampaignLevel_!=1
        || level1_.levelIndex_!=1 || !level2FailureTailFinished_ || !level1_.missionActive_
        || level2HasAdvanced_ || level2TailConsumed_ || !level2PrefixGuards_
        || level2Entities_.size()!=7 || level2AllocationListOrder_.size()!=7
        || !level2ActiveListOrder_.empty() || !level2ObjectListOrder_.empty()
        || lastCockpitInitialization_.levelIndex!=1 || lastCockpitInitialization_.displaySteps!=0
        || settings.viewSize<=50) {
        throw std::invalid_argument("Level2 retry pages require their current unconsumed constructor return");
    }
    return recordRecoveredLevel2LoaderPages(settings);
}
} //namespace stellar7
