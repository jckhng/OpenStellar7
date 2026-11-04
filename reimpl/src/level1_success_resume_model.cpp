#include "stellar7/level1_frame.hpp"
#include "level1_reward_model_bank.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::size_t noEntity = static_cast<std::size_t>(-1);
constexpr std::size_t systemPanelGroup = 4;
constexpr std::size_t tankShotType = 17;
constexpr std::size_t aircraftShotType = 20;
constexpr std::size_t systemBonusType = 42;
constexpr unsigned damageFadeLength = 8;

}

Level1GameplayFrameWork GameplayModel::resumeRecoveredLevel1Success(const GameplayAssets& assets,
    const CockpitSettings& settings,const CockpitPageState& pages,const RadarState& radar,
    const IndexedPages& pixels) {
    auto& state=level1_;
    if (phase_!=GameplayPhase::successPresentation || !recoveredSuccessPause_ || !recoveredSuccessReturned_
        || state.levelIndex_!=0 || !state.missionActive_
        || state.failureState_ || state.successState_ || state.exitState_
        || ticks_<recoveredLevel1TickBase_
        || static_cast<std::uint16_t>(ticks_-recoveredLevel1TickBase_)!=state.loopTick_
        || level2PickupIndex_!=recoveredSuccessPause_->rewardEntityIndex
        || state.postWaveObjectAllocationIndex_==noEntity
        || state.postWaveObjectAllocationIndex_>=state.entities_.size()
        || state.entities_[state.postWaveObjectAllocationIndex_].type!=44
        || assets.models.size()<=systemBonusType || assets.artGroups.size()<=systemPanelGroup
        || (damageFlashActive_ && damageFlashAge_>=damageFadeLength)) {
        throw std::invalid_argument("recovered Level1 success resume requires its returned source parent");
    }
    const auto publicRef=[](const auto object) -> Level1ObjectRef {
        switch (object.kind) {
        case Level1State::ObjectKind::entity: return {Level1PresentationKind::entity,object.index};
        case Level1State::ObjectKind::projectile: return {Level1PresentationKind::projectile,object.index};
        case Level1State::ObjectKind::deathEffect: return {Level1PresentationKind::deathEffect,object.index};
        default: throw std::invalid_argument("success resume has an unsupported source object");
        }
    };
    const auto privateRef=[](const Level1ObjectRef object) -> Level1State::ObjectReference {
        switch (object.kind) {
        case Level1PresentationKind::entity: return {Level1State::ObjectKind::entity,object.index};
        case Level1PresentationKind::projectile: return {Level1State::ObjectKind::projectile,object.index};
        case Level1PresentationKind::deathEffect: return {Level1State::ObjectKind::deathEffect,object.index};
        default: throw std::invalid_argument("success resume returned an unsupported source object");
        }
    };
    std::vector<Level1ObjectRef> allocations,oldOrder;
    for (const auto object:state.allocationList_) { allocations.push_back(publicRef(object)); }
    for (const auto object:state.objectListOrder_) { oldOrder.push_back(publicRef(object)); }
    Level1PeriodicSpawnInput pool{state.activeEnemyCount_,state.spawnedEnemyCount_,
        static_cast<std::uint8_t>(state.maximumEnemyCount_),static_cast<std::uint16_t>(allocations.size()),
        state.entities_.size(),{}};
    std::size_t ordinarySlot=0;
    for (std::size_t index=0;index<state.entities_.size();++index) {
        const auto& actor=state.entities_[index];
        if (actor.stateBytes4dTo52[0]!=0xff) { continue; }
        if (ordinarySlot==pool.ordinaryPool.size()) {
            throw std::invalid_argument("success resume exceeds the source ordinary pool");
        }
        const auto hasAnimation=actor.primaryType11AnimationSelector
            || actor.renderProfile==WorldActorRenderProfile::heapSelectorDeferredAnimation
            || actor.renderProfile==WorldActorRenderProfile::heapSelectorNoCallback;
        pool.ordinaryPool[ordinarySlot++]={actor.stateBytes4dTo52[1],hasAnimation,index};
    }
    const auto fadeTicks=static_cast<std::uint16_t>(damageFlashActive_ ? damageFadeLength-damageFlashAge_ : 0);
    const Level1TargetPrefixInput input{{state.player_,state.entities_,state.projectiles_,allocations,oldOrder,
        state.projectionListOrigin_,state.nativeInitialListPending_,state.rng_,fadeTicks,false,
        state.playerCollisionMomentum_,state.missionActive_,{true,false}},state.deathEffects_,
        {state.lastDestroyedType_,state.consecutiveDestroyedTypeCount_},state.activeEnemyCount_,state.score_};
    const auto& tankShot=assets.models[tankShotType];
    const auto& aircraftShot=assets.models[aircraftShotType];
    const auto modelCapacity = level1RewardModelCapacity(assets);
    const auto type3=std::any_of(state.entities_.begin(),state.entities_.end(),
        [](const auto& actor){return actor.type==3;});
    const Level1TargetPrefixProfile profile{{{state.loopTick_,modelCapacity,
        {tankShot.scaleShift,tankShot.collisionRadius},{aircraftShot.scaleShift,aircraftShot.collisionRadius},
        Level1ProjectileAllocationDomain::retainedRoster,Level1ActorContactDomain::ordinaryPlayer,
        Level1OrdinaryMembership::periodicSpawned,
        type3 ? Level1OrdinaryFamily::type1And3 : Level1OrdinaryFamily::type1},state.playerArmor_,0,0,0,
        Level1DamagePaletteMode::residentVga,Level1PlayerCombatDomain::expiryAndLoss},
        static_cast<std::uint16_t>(settings.options[4]),type3 ? Level1TargetHitDomain::periodicClass1 : Level1TargetHitDomain::initialOrdinary};
    const Level1BonusPickupContext pickup{state.systemStatusLevels_,state.systemTimers_,state.selectedSystem_,
        assets.artGroups[systemPanelGroup]};
    const auto craft=state.completionObjectAllocationIndex_==noEntity ? std::optional<std::size_t>{}
        : std::optional<std::size_t>{state.completionObjectAllocationIndex_};
    auto resumed=level1GuardianRewardResumeOperations(input,profile,assets.scoreFont,pages,
        modelCapacity,assets.models[systemBonusType],pickup,
        {craft,state.postWaveObjectAllocationIndex_,pool,assets.models},{level2PickupIndex_,0},*recoveredSuccessPause_);
    if (!resumed.rewardContact || resumed.rewardContact->active || resumed.rewardContact->campaignIndex!=0
        || resumed.successPause || resumed.rewardEntityIndex!=std::optional<std::size_t>{level2PickupIndex_}
        || !resumed.rewardBirths.empty()) {
        throw std::logic_error("success resume lost its inactive0312 source boundary");
    }
    auto& pickupResult=resumed.prefix;
    auto& target=pickupResult.bonus.prefix;
    auto& damage=target.damage;
    auto& combat=damage.combat;
    auto& prefix=combat.prefix;
    auto returnedRadar=radar;
    for (const auto& birth:pickupResult.bonus.bonusBirths) {
        if (!birth.resetProjection) { continue; }
        const auto& reset=*birth.resetProjection;
        if (reset.sourceResetEntry!=0x49f4 || reset.firstResetSlotCount!=8
            || reset.fixedPoolSlot>=reset.firstResetSlotCount || !birth.receipt.materializeEntity
            || reset.entityIndex!=birth.receipt.entityIndex || reset.entityIndex>=prefix.entities.size()
            || prefix.entities[reset.entityIndex].type!=systemBonusType) {
            throw std::logic_error("success resume bonus lacks source projection reset ancestry");
        }
        returnedRadar.initializeAllocation(reset.entityIndex);
    }
    auto returnedPages=pages;
    auto returnedPixels=pixels;
    std::size_t scoreIndex=0,drawingIndex=0;
    while (scoreIndex<target.scoreSteps.size() || drawingIndex<pickupResult.drawingSteps.size()) {
        const auto drawingFirst=drawingIndex<pickupResult.drawingSteps.size()
            && (scoreIndex==target.scoreSteps.size()
                || pickupResult.drawingSteps[drawingIndex].operationOffset<=target.scoreSteps[scoreIndex].operationOffset);
        if (drawingFirst) {
            returnedPixels.apply(pickupResult.drawingSteps[drawingIndex++].effects);
            continue;
        }
        returnedPixels.apply(target.scoreSteps[scoreIndex++].work.effects);
        returnedPages.selectScoreCopy();
    }
    if (pickupResult.presentation) { returnedPages=pickupResult.presentation->pages; }
    state.player_=prefix.player;
    state.playerCollisionMomentum_=prefix.impulse;
    state.entities_=prefix.entities;
    state.projectiles_=prefix.projectiles;
    state.rng_=prefix.rng;
    state.projectionListOrigin_=prefix.lastListCamera;
    state.nativeInitialListPending_=prefix.refreshRequested;
    state.deathEffects_=target.effects;
    state.lastDestroyedType_=target.bonus.lastType;
    state.consecutiveDestroyedTypeCount_=target.bonus.consecutive;
    state.activeEnemyCount_=target.activeEnemies;
    state.score_=target.score;
    state.playerArmor_=damage.armor;
    state.systemStatusLevels_=pickupResult.systems;
    state.systemTimers_=pickupResult.timers;
    state.selectedSystem_=pickupResult.selectedSystem;
    if (damageFlashActive_ || damage.fadeTicks!=0) {
        damageFlashActive_=damage.fadeTicks!=0;
        damageFlashAge_=damageFadeLength-damage.fadeTicks;
    }
    state.audioEvents_.insert(state.audioEvents_.end(),combat.audioEvents.begin(),combat.audioEvents.end());
    state.allocationList_.clear();
    for (const auto object:prefix.allocationOrder) { state.allocationList_.push_back(privateRef(object)); }
    state.objectListOrder_.clear();
    state.activeListOrder_.clear();
    state.semanticActiveListOrder_.clear();
    for (const auto object:prefix.activeOrder) {
        state.objectListOrder_.push_back(privateRef(object));
        if (object.kind!=Level1PresentationKind::entity) { continue; }
        state.activeListOrder_.push_back(object.index);
        state.semanticActiveListOrder_.push_back(state.entities_.at(object.index).allocationIndex);
    }
    state.missionActive_=resumed.rewardContact->active;
    state.successState_=true;
    recoveredSuccessPause_.reset();
    recoveredSuccessReturned_=false;
    Level1FramePlan retained{{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
        returnedPages,std::move(returnedRadar),std::move(returnedPixels),{}, {}};
    return {{},std::move(retained),{},std::move(prefix),{},std::move(damage.paletteSteps),{}, {},
        std::move(target.scoreSteps),std::move(pickupResult.drawingSteps),{},std::move(resumed.children),
        Level1GameplayFrameBoundary::successReturned,{}};
}
} // namespace stellar7
