#include "stellar7/recovered_level2_model.hpp"
#include "stellar7/level1_primary_fire_work.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr auto noEntity=static_cast<std::size_t>(-1);
constexpr std::size_t sharedModelCount=44;
constexpr std::size_t maximumLevel1EntityRecords=17;
constexpr std::size_t ordinaryPoolSize=16;
constexpr std::size_t fixedResetCount=8;
constexpr std::size_t projectilePoolSize=8;
constexpr std::size_t fixedProjectionBase=16;
constexpr std::size_t projectileProjectionBase=32;
constexpr std::size_t effectProjectionBase=40;

Level1Entity projectileProjection(const Level1Projectile& source) {
    Level1Entity result{};result.allocationIndex=source.poolSlot;result.flags=source.flags;result.type=source.type;
    result.collisionRadius=source.collisionRadius;result.position=source.position;result.pitch=source.pitch;result.roll=source.roll;result.yaw=source.yaw;
    result.pitchVelocity=source.pitchVelocity;result.rollVelocity=source.rollVelocity;result.yawVelocity=source.yawVelocity;
    result.xVelocity=source.xVelocity;result.yVelocity=source.yVelocity;result.zVelocity=source.zVelocity;result.forwardVelocity=source.forwardVelocity;
    std::copy_n(source.stateBytes4dTo54.begin(),result.stateBytes4dTo52.size(),result.stateBytes4dTo52.begin());
    result.presenterDistance=source.presenterDistance;result.targetHeading=source.targetHeading;
    result.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;result.animatedModelPresenterEnabled=false;
    return result;
}

void admitUnmaterializedResetRecord(Level1Entity& actor) {
    if(actor.renderProfile!=WorldActorRenderProfile::unknown) {return;}
    const auto category=actor.stateBytes4dTo52[0];
    const auto resetCategory=category==0 || category>=252;
    const auto resetFlags=actor.flags==0 || (category==252 && actor.flags==0x88);
    if(actor.type!=0 || !resetFlags || !resetCategory || actor.collisionRadius!=0 || !(actor.position==FixedPosition{})
        || actor.pitch!=0 || actor.roll!=0 || actor.yaw!=0 || actor.pitchVelocity!=0 || actor.rollVelocity!=0
        || actor.yawVelocity!=0 || actor.xVelocity!=0 || actor.yVelocity!=0 || actor.zVelocity!=0
        || actor.forwardVelocity!=0 || actor.presenterDistance!=0 || actor.primaryType11AnimationSelector
        || std::any_of(actor.stateBytes4dTo52.begin()+1,actor.stateBytes4dTo52.end(),[](auto v) {return v!=0;})) {
        throw std::invalid_argument("unknown populated physical reset record");
    }
    //The initial Level1 loader's49F4 already proves null selectors for these
    //unmaterialized slots. This transfers that proof, not original-state bytes.
    actor.renderProfile=WorldActorRenderProfile::noHandles;actor.animatedModelPresenterEnabled=false;
}
void admitLevel1EffectSelector(Level1Entity& actor) {
    if(actor.renderProfile==WorldActorRenderProfile::unknown && actor.type==-1
        && actor.stateBytes4dTo52[0]==252 && (actor.flags==0x88 || actor.flags==0x8c)
        && actor.collisionRadius==0 && !actor.primaryType11AnimationSelector
        && actor.pitch==0 && actor.roll==0 && actor.yaw==0 && actor.pitchVelocity==0
        && actor.rollVelocity==0 && actor.yawVelocity==0 && actor.xVelocity==0
        && actor.yVelocity==0 && actor.zVelocity==0 && actor.forwardVelocity==0) {
        //FC starts with null selectors in49F4. Its partial constructor and45EA
        //presenter never allocate a model selector, including after retirement.
        actor.renderProfile=WorldActorRenderProfile::noHandles;
        actor.animatedModelPresenterEnabled=false;
    }
    admitUnmaterializedResetRecord(actor);
}
void admitInitialLevel1Selector(Level1Entity& actor) {
    if(actor.renderProfile==WorldActorRenderProfile::unknown
        && ((actor.stateBytes4dTo52[0]==255 && (actor.type==1 || (actor.type==11 && actor.primaryType11AnimationSelector)))
            || (actor.stateBytes4dTo52[0]==254 && actor.type==29))) {
        //Legacy typed records omit the null callback profile. Type11's separate
        //selector bool remains authoritative for the source13AC heap release.
        actor.renderProfile=WorldActorRenderProfile::noHandles;actor.animatedModelPresenterEnabled=false;
    }
}
}

RecoveredLevel1SuccessTailWork GameplayModel::advanceRecoveredLevel1SuccessTail(const GameplayAssets& assets,
    const CockpitPageState& pages,FrameTailGuards guards,const std::uint8_t enemyFlag) {
    auto& state=level1_;
    if(phase_!=GameplayPhase::successPresentation || state.levelIndex_!=0 || state.missionActive_ || !state.successState_
        || state.failureState_ || state.failurePending_ || state.exitState_ || state.successContactPending_
        || recoveredSuccessPause_ || recoveredSuccessReturned_ || state.playerArmor_<0 || state.selectedSystem_!=-1
        || state.entities_.size()>maximumLevel1EntityRecords
        || state.postWaveObjectAllocationIndex_>=state.entities_.size() || level2PickupIndex_>=state.entities_.size()
        || state.entities_[state.postWaveObjectAllocationIndex_].type!=44
        || state.entities_[state.postWaveObjectAllocationIndex_].active()
        || state.entities_[level2PickupIndex_].type!=25 || !state.entities_[level2PickupIndex_].active()
        || state.spawnedEnemyCount_<10 || state.playerCollisionMomentum_!=0 || ticks_<recoveredLevel1TickBase_
        || static_cast<std::uint16_t>(ticks_-recoveredLevel1TickBase_)!=state.loopTick_) {
        throw std::invalid_argument("inactive Level1 tail requires its actual resumed guardian success");
    }
    const auto publicRef=[](const auto& ref) {
        switch(ref.kind) {
        case Level1State::ObjectKind::entity:return Level1ObjectRef{Level1PresentationKind::entity,ref.index};
        case Level1State::ObjectKind::projectile:return Level1ObjectRef{Level1PresentationKind::projectile,ref.index};
        case Level1State::ObjectKind::deathEffect:return Level1ObjectRef{Level1PresentationKind::deathEffect,ref.index};
        }throw std::invalid_argument("invalid old Level1 source reference");
    };
    std::vector<Level1ObjectRef> oldOrder;
    FrameTailState input{state.loopTick_,state.player_.position,state.player_.yawVelocity,state.player_.forwardVelocity,
        {state.playerWeaponCooldown_,state.playerProjectileTimers_[0],state.playerProjectileTimers_[1]}, {}};
    for(const auto& ref:state.objectListOrder_) {
        oldOrder.push_back(publicRef(ref));
        if(ref.kind==Level1State::ObjectKind::entity) {const auto& actor=state.entities_.at(ref.index);input.actors.push_back({actor.type,actor.position});}
        else if(ref.kind==Level1State::ObjectKind::projectile) {const auto& shot=state.projectiles_.at(ref.index);input.actors.push_back({shot.type,shot.position});}
        else {const auto& effect=state.deathEffects_.at(ref.index).entity;input.actors.push_back({effect.type,effect.position});}
    }
    guards.armor=state.playerArmor_;guards.firstFrame=false;guards.success=true;guards.missionActive=false;
    const std::array<std::size_t,6> abilityIndices{0,1,2,4,5,6};
    for(std::size_t i=0;i<abilityIndices.size();++i) {guards.abilities[i]=static_cast<std::uint16_t>(state.systemTimers_[abilityIndices[i]]);}
    if((state.loopTick_&15U)==0) {
        guards.periodic=FrameTailPeriodicInput{0,state.activeEnemyCount_,state.spawnedEnemyCount_,10,
            campaignSpawnThreshold(0,static_cast<std::uint8_t>(state.activeEnemyCount_)),state.rng_.indexA(),state.rng_.indexB(),enemyFlag};
    }else {guards.periodic.reset();}
    Level1PeriodicSpawnInput pool{state.activeEnemyCount_,state.spawnedEnemyCount_,10,
        static_cast<std::uint16_t>(state.allocationList_.size()),state.entities_.size(),{}};
    std::size_t ordinarySlot=0;
    for(std::size_t i=0;i<state.entities_.size();++i) {
        const auto& actor=state.entities_[i];if(actor.stateBytes4dTo52[0]!=255) {continue;}
        if(ordinarySlot==pool.ordinaryPool.size()) {throw std::invalid_argument("Level1 ordinary physical pool overflow");}
        const auto selector=actor.primaryType11AnimationSelector || actor.renderProfile==WorldActorRenderProfile::heapSelectorDeferredAnimation
            || actor.renderProfile==WorldActorRenderProfile::heapSelectorNoCallback;
        pool.ordinaryPool[ordinarySlot++]={actor.stateBytes4dTo52[1],selector,i};
    }
    Level1CombatEffectMembership membership{state.allocationList_.size(),0,0};
    for(const auto& ref:state.allocationList_) {
        if(ref.kind==Level1State::ObjectKind::entity) {
            ++membership.ordinaryAllocations;
            if(state.entities_.at(ref.index).type==42) {++membership.bonusAllocations;membership.profile=Level1CombatEffectProfile::systemBonus;}
        }else if(ref.kind==Level1State::ObjectKind::deathEffect) {++membership.effectAllocations;}
    }
    std::optional<WeaponLampWork> lamp;
    const auto requestLamp=[&](WeaponLampState requested) {
        const auto dirty=weaponIndicatorVisible_?static_cast<std::uint16_t>(weaponReadyIndicator_):std::uint16_t{0xffff};
        auto next=weaponLampOperations(dirty,requested,assets.artGroups.at(4).at(requested==WeaponLampState::ready?40:39),lamp?lamp->pages:pages);
        const auto operations=next.operations;
        if(lamp) {
            lamp->effects.insert(lamp->effects.end(),next.effects.begin(),next.effects.end());
            lamp->operations.insert(lamp->operations.end(),next.operations.begin(),next.operations.end());lamp->pages=next.pages;lamp->dirty=next.dirty;
        }else {lamp=std::move(next);}
        weaponIndicatorVisible_=true;weaponReadyIndicator_=requested==WeaponLampState::ready;return operations;
    };
    std::optional<Level1TailFireAction> fire;
    if(guards.heldKeys[8]) {
        std::vector<std::size_t> allocatedShots;
        for(const auto& ref:state.allocationList_) {if(ref.kind==Level1State::ObjectKind::projectile) {allocatedShots.push_back(ref.index);}}
        const auto& model=assets.models.at(23);
        auto firing=level1PrimaryFireOperations({state.player_,state.projectiles_,state.playerWeaponCooldown_,state.playerProjectileTimers_,0,
            state.allocationList_.size(),allocatedShots,state.nativeInitialListPending_,level1ShotOrigin_,
            {assets.sharedModelRegistry[0],model.scaleShift,model.collisionRadius},Level1ProjectileAllocationDomain::retained,
            Level1PrimaryFireMembership::deployment,membership});
        if(firing.launch) {
            requestLamp(WeaponLampState::unavailable);const auto slot=firing.launch->poolSlot;
            if(!state.firePrimary() || state.allocationList_.size()!=firing.allocationCount) {throw std::logic_error("inactive fire lost canonical allocation");}
            state.projectiles_.at(slot)=firing.launch->projectile;state.nativeInitialListPending_=firing.refreshRequested;level1ShotOrigin_=firing.shotOrigin;
        }
        fire=Level1TailFireAction{std::move(firing),lamp?lamp->operations:std::vector<NormalWorkOperation>{}};
    }
    const auto timers=fire?std::array<std::uint8_t,3>{fire->work.mainCooldown,fire->work.weaponTimers[0],fire->work.weaponTimers[1]}:input.weaponTimers;
    std::vector<std::vector<NormalWorkOperation>> ready;
    for(std::size_t i=0;i<level1TailReadyLampCalls(timers);++i) {ready.push_back(requestLamp(WeaponLampState::ready));}
    pool.allocationCount=static_cast<std::uint16_t>(state.allocationList_.size());
    const Level1ArrivalInput arrival{enemyFlag,{},state.postWaveObjectAllocationIndex_,state.player_,pool};
    auto work=level1SuccessFrameTailOperations(input,guards,state.rng_,arrival,assets.models,membership,fire,ready);
    auto& tail=work.periodic.tail;
    if(work.arrival && work.arrival->birth) {throw std::logic_error("retained guardian did not suppress a second arrival");}
    state.audioEvents_.insert(state.audioEvents_.end(),tail.audioEvents.begin(),tail.audioEvents.end());
    state.player_.position=tail.state.playerPosition;state.player_.yawVelocity=tail.state.yawVelocity;state.player_.forwardVelocity=tail.state.forwardVelocity;
    state.playerWeaponCooldown_=tail.state.weaponTimers[0];state.playerProjectileTimers_={tail.state.weaponTimers[1],tail.state.weaponTimers[2]};
    for(std::size_t i=0;i<oldOrder.size();++i) {
        const auto ref=oldOrder[i];const auto position=tail.state.actors.at(i).position;
        if(ref.kind==Level1PresentationKind::entity) {state.entities_.at(ref.index).position=position;}
        else if(ref.kind==Level1PresentationKind::projectile) {state.projectiles_.at(ref.index).position=position;}
        else {state.deathEffects_.at(ref.index).entity.position=position;}
    }
    state.rng_=tail.rng;state.loopTick_=tail.state.loopTick;state.hasAdvanced_=true;
    state.ordinaryTailPending_=false;state.ordinaryTailConsumed_=true;state.pendingPostBoundaryInput_={};
    if(tail.lowArmorWarningRequested) {level2ArmorWarningActive_=true;}
    ++ticks_;phase_=GameplayPhase::level1SuccessTailReturn;
    return {std::move(work),std::move(lamp)};
}

Level1TransitionWork GameplayModel::recoveredLevel2TransitionOperations(const CockpitPageState& pages,const std::uint16_t introduction) const {
    if(phase_!=GameplayPhase::level1SuccessTailReturn || !level1_.ordinaryTailConsumed_ || level1_.missionActive_) {
        throw std::invalid_argument("Level2 transition requires its completed inactive source tail");
    }
    Level1TransitionProfile profile{};profile.levelIndex=level1_.levelIndex_;profile.score=level1_.score_;profile.armor=level1_.playerArmor_;
    profile.introduction=introduction;profile.selectedSystem=level1_.selectedSystem_;profile.systemLevels=level1_.systemStatusLevels_;profile.systemTimers=level1_.systemTimers_;
    profile.frontPage=pages.frontPage();profile.backPage=pages.backPage();profile.cachePage=pages.cachePage();profile.successReturned=level1_.successState_;
    profile.missionActive=level1_.missionActive_;profile.deathRequested=level1_.failureState_;profile.exitRequested=level1_.exitState_;
    return level1SuccessTransitionOperations(profile);
}

void GameplayModel::beginRecoveredLevel2Load() {
    if(phase_!=GameplayPhase::level1SuccessTailReturn || level1_.levelIndex_!=0 || !level1_.ordinaryTailConsumed_ || level1_.missionActive_
        || !level1_.successState_ || level1_.failureState_ || level1_.exitState_) {
        throw std::invalid_argument("Level2 loader requires the normal Level1 campaign return");
    }
    level1_.levelIndex_=1;activeCampaignLevel_=1;level2LoopTick_=level1_.loopTick_;
    level2Player_=level1_.player_;level2PlayerArmor_=level1_.playerArmor_;level2Score_=level1_.score_;
    level2ActiveEnemyCount_=level1_.activeEnemyCount_;level2SpawnedEnemyCount_=level1_.spawnedEnemyCount_;
    //The uninterrupted0475..4E12 caller block also reaches019F's fade clear
    //before its next child. No provider reads the transient0469 fade1 here.
    campaignExitFadeTicks_=0;damageFlashActive_=false;damageFlashAge_=0;
    level2Rng_=level1_.rng_;level2Ground_=level1_.ground_;
    phase_=GameplayPhase::level2LoaderReturn;
}

RecoveredLevel2PoolResetWork GameplayModel::resetRecoveredLevel2Pools(const std::vector<Model3d>& shared,const std::vector<Model3d>& local) {
    if(phase_!=GameplayPhase::level2LoaderReturn || level1_.missionActive_ || !level1_.successState_ || shared.size()!=sharedModelCount
        || level1_.entities_.size()>maximumLevel1EntityRecords
        || shared.front().scaleShift!=0 || campaignModelRadius(shared,local,0)!=723) {
        throw std::invalid_argument("Level2 pool reset requires its retained48D4 entry");
    }
    Level2PoolResetInput input{};input.ordinary=level2ActorPool_;input.fixed=level2FixedPool_;input.effects=level2DeathEffectPool_;
    for(auto& actor:input.ordinary) {admitUnmaterializedResetRecord(actor);}
    for(std::size_t i=0;i<fixedResetCount;++i) {admitUnmaterializedResetRecord(input.fixed[i]);}
    std::size_t ordinary=0,fixed=0;
    for(std::size_t index=0;index<level1_.entities_.size();++index) {
        auto actor=level1_.entities_[index];
        if(actor.allocationIndex!=index) {throw std::invalid_argument("Level1 cache identity is not its canonical entity index");}
        admitInitialLevel1Selector(actor);
        if(actor.stateBytes4dTo52[0]==255) {
            if(ordinary==ordinaryPoolSize) {throw std::invalid_argument("Level1 ordinary physical mapping overflow");}
            input.ordinary[ordinary++]=actor;
        }else if(actor.stateBytes4dTo52[0]==254) {
            if(fixed==fixedResetCount) {throw std::invalid_argument("Level1 fixed mapping exceeds its known reset slots");}
            input.fixed[fixed++]=actor;
        }else {throw std::invalid_argument("Level1 entity lost its physical pool category");}
    }
    for(std::size_t i=0;i<projectilePoolSize;++i) {input.projectiles[i]=projectileProjection(level1_.projectiles_[i]);input.effects[i]=level1_.deathEffects_[i].entity;admitLevel1EffectSelector(input.effects[i]);}
    input.player=level1_.player_;input.armor=level1_.playerArmor_;input.systemTimers=level1_.systemTimers_;
    return commitRecoveredLevel2PoolReset(level2PoolReset(input,shared.front()));
}

RecoveredLevel2PoolResetWork GameplayModel::commitRecoveredLevel2PoolReset(Level2PoolResetWork work) {
    level2ActorPool_=work.state.ordinary;level2FixedPool_=work.state.fixed;level2ProjectilePool_=work.state.projectiles;level2DeathEffectPool_=work.state.effects;
    level2ActorPoolActive_.fill(false);level2ProjectilePoolActive_.fill(false);level2ProjectilePoolWasActiveAtBoundaryStart_.fill(false);
    level2DeathEffectPoolActive_.fill(false);level2DeathEffectPoolPendingPresenterRetirement_.fill(false);
    for(std::size_t i=0;i<fixedResetCount;++i) {level2FixedPoolKnown_[i]=true;level2FixedPoolEverUsed_[i]=false;}
    level2Entities_.clear();level2ActiveListOrder_.clear();level2ObjectListOrder_.clear();level2AllocationListOrder_.clear();
    level2EntityActorPoolSlots_.clear();level2EntityFixedPoolSlots_.clear();level2EntityProjectilePoolSlots_.clear();level2EntityDeathEffectPoolSlots_.clear();
    level2PendingTerminalEffectPresentations_.clear();level2PendingTerminalEffectSlots_.clear();level2RetiredEffectSemanticEntities_.clear();
    level2RetiredEffectSemanticActiveOrder_.clear();level2RetiredEffectSemanticActivePositions_.clear();level2EffectSemanticActivePositionOverrides_.clear();
    level2RetiredEffectRebuildReported_=false;level2RetiredEffectSemanticOrderRebuilt_=false;
    level2Player_=work.state.player;level2Player_.allocationIndex=7;level2PlayerArmor_=work.state.armor;level2Score_=level1_.score_;
    level1_.player_=level2Player_;level1_.playerArmor_=work.state.armor;level1_.systemTimers_=work.state.systemTimers;
    level1_.consecutiveDestroyedTypeCount_=0;level1_.playerWeaponCooldown_=0;level1_.playerProjectileTimers_={};
    level1_.completionObjectAllocationIndex_=noEntity;level1_.postWaveObjectAllocationIndex_=noEntity;
    level1_.missionActive_=true;level1_.successState_=false;level1_.successContactPending_=false;level1_.retainSuccessContact_=false;
    level1_.successContactIndex_=noEntity;level1_.successContactOrder_.clear();level1_.successContactProjectileActiveAtStart_.fill(false);
    level1_.failureState_=false;level1_.failurePending_=false;level1_.failureTailConsumed_=false;level1_.exitState_=false;
    level1_.allocationList_.clear();level1_.objectListOrder_.clear();level1_.activeListOrder_.clear();level1_.semanticActiveListOrder_.clear();
    level1_.entities_.clear(); //The source records now belong to the existing Level2 physical pools.
    level1_.projectiles_={};
    for(std::size_t i=0;i<level1_.projectiles_.size();++i) {level1_.projectiles_[i].poolSlot=i;level1_.projectiles_[i].stateBytes4dTo54[0]=253;}
    level1_.deathEffects_={};
    for(std::size_t i=0;i<level1_.deathEffects_.size();++i) {level1_.deathEffects_[i].entity=work.state.effects[i];}
    level1_.activeEnemyCount_=0;level1_.spawnedEnemyCount_=0;
    level1_.ordinaryTailPending_=false;level1_.ordinaryTailConsumed_=false;level1_.pendingPostBoundaryInput_={};
    level2CompletionObjectIndex_=noEntity;level2PostWaveObjectIndex_=noEntity;level2PickupIndex_=noEntity;
    level2SecondaryPickupKnownNull_=true;level5TransientStateKnownInactive_=true;level2Complete_=false;level3CompletionByPickup_=false;
    level2ActiveEnemyCount_=0;level2SpawnedEnemyCount_=0;level2PlayerCollisionMomentum_=0;level2Type46ContactSlowdown_=false;
    level1_.playerCollisionMomentum_=0;level2PendingInput_={};level2HasAdvanced_=false;level2TailConsumed_=false;
    level2PrefixGuards_=StationaryPrefixGuards{0,false,{false,false}};campaignExitFadeTicks_=0;
    damageFlashActive_=false;damageFlashAge_=0;campaignSuccess_=false;
    level2EntryPaletteActive_=true;successAudioOwned_=false;successAudioActive_=false;presentationFrame_=0;
    level5SuccessContinuation_.reset();level2SuccessContinuation_.reset();level5Constructor_.reset();
    //1296 is not written by48D4. Constructor allocation will establish its
    //new true request; do not fabricate a3768 camera/list update here.
    level2Visibility_.invalidate();snapshotLevel2Presentation();
    std::vector<std::size_t> keys;
    for(std::size_t i=0;i<ordinaryPoolSize;++i) {keys.push_back(i);}
    for(std::size_t i=0;i<fixedResetCount;++i) {keys.push_back(fixedProjectionBase+i);}
    for(std::size_t i=0;i<projectilePoolSize;++i) {keys.push_back(projectileProjectionBase+i);}
    for(std::size_t i=0;i<projectilePoolSize;++i) {keys.push_back(effectProjectionBase+i);}
    return {std::move(work),std::move(keys)};
}

Level2LoaderActors GameplayModel::constructRecoveredLevel2Actors(const std::vector<Model3d>& shared,const std::vector<Model3d>& local) {
    if(phase_!=GameplayPhase::level2LoaderReturn || !level1_.missionActive_ || !level2PrefixGuards_ || !level2Entities_.empty()) {
        throw std::invalid_argument("Level2 constructors require their sole reset pool incarnation");
    }
    auto work=level2LoaderActors(level2Rng_,shared,local);
    bindCampaignModels(1,shared,local,CampaignModelBinding::sourceMetadata);
    for(const auto& receipt:work.actors) {
        auto actor=receipt.actor;const auto index=level2Entities_.size();actor.allocationIndex=index;level2Entities_.push_back(actor);
        level2EntityActorPoolSlots_.push_back(level2ActorPoolSize_);level2EntityFixedPoolSlots_.push_back(level2FixedObjectPoolSize_);
        level2EntityProjectilePoolSlots_.push_back(level2ProjectilePoolSize_);level2EntityDeathEffectPoolSlots_.push_back(level2DeathEffectPoolSize_);
        if(receipt.pool==Level2LoaderPool::ordinary) {
            level2ActorPool_.at(receipt.slot)=actor;level2ActorPoolActive_.at(receipt.slot)=true;
            ++level2ActorIncarnations_.at(receipt.slot);level2EntityActorPoolSlots_.back()=receipt.slot;
        }else {
            level2FixedPool_.at(receipt.slot)=actor;level2FixedPoolKnown_.at(receipt.slot)=true;level2FixedPoolEverUsed_.at(receipt.slot)=true;
            level2EntityFixedPoolSlots_.back()=receipt.slot;++level2FixedAllocationRevision_;
        }
        level2AllocationListOrder_.push_back(index);++level2AllocationRevision_;++level2NonProjectileAllocationRevision_;
    }
    level2Rng_=work.rng;level2ActiveEnemyCount_=work.activeEnemies;level2SpawnedEnemyCount_=work.spawnedEnemies;
    level1_.activeEnemyCount_=work.activeEnemies;level1_.spawnedEnemyCount_=work.spawnedEnemies;
    level2Visibility_.resetLevel();snapshotLevel2Presentation();return work;
}

Level2LoaderPageInput GameplayModel::prepareRecoveredLevel2LoaderPages(const CockpitSettings& settings) {
    if(phase_!=GameplayPhase::level2LoaderReturn || level2Entities_.size()!=7 || lastCockpitInitialization_.levelIndex==1 || settings.viewSize<=50) {
        throw std::invalid_argument("Level2 pages require unconsumed two-cockpit loader observations");
    }
    return recordRecoveredLevel2LoaderPages(settings);
}

Level2LoaderPageInput GameplayModel::recordRecoveredLevel2LoaderPages(const CockpitSettings& settings) {
    if(settings.viewSize<=50) {throw std::invalid_argument("Level2 loader requires its ordinary cockpit size");}
    lastCockpitInitialization_.levelIndex=1;lastCockpitInitialization_.displaySteps=2;
    lastCockpitInitialization_.states[0]={cockpitIndicatorX_,cockpitIndicatorVelocity_};
    for(std::size_t i=1;i<=2;++i) {advanceCockpitIndicator();lastCockpitInitialization_.states[i]={cockpitIndicatorX_,cockpitIndicatorVelocity_};}
    lastCockpitInitialization_.states.back()=lastCockpitInitialization_.states[2];
    return {lastCockpitInitialization_,level2Player_,level2PlayerArmor_};
}

void GameplayModel::completeRecoveredLevel2MusicActivation() {
    if(phase_!=GameplayPhase::level2LoaderReturn || level2Entities_.size()!=7 || lastCockpitInitialization_.levelIndex!=1
        || lastCockpitInitialization_.displaySteps!=2) {throw std::invalid_argument("Level2 activation commit requires completed loader pages");}
    level2LoopTick_=0;level1_.loopTick_=0;phase_=GameplayPhase::level2MusicReturn;
}

RecoveredLevel2PreloopWork GameplayModel::completeRecoveredLevel2Preloop() {
    if(phase_!=GameplayPhase::level2MusicReturn || level2LoopTick_!=0 || level2HasAdvanced_ || level2TailConsumed_
        || !level2ActiveListOrder_.empty() || level2Entities_.size()!=7 || level2AllocationListOrder_.size()!=7 || !level2PrefixGuards_) {
        throw std::invalid_argument("Level2 preloop requires its real unadvanced loader return");
    }
    const bool retry=failureFromLevel2_;
    if(retry && (!level2FailureTailFinished_ || !failureSourceLoopTick_ || !recoveredFailureControl_
        || !recoveredFailureControl_->entered || !recoveredFailureControl_->finished
        || recoveredFailureControl_->display!=failureExplosionFrames || !failureDebris_.empty()
        || recoveredContinueDecision_)) {
        throw std::invalid_argument("Level2 retry preloop lost its completed failure ancestry");
    }
    auto ground=level2RetainedGroundInitializationOperations(level2Rng_);level2Rng_=ground.rng;level2Ground_=ground.ground;
    const auto& guard=*level2PrefixGuards_;
    StationaryLevel2PrefixInput prefix{level2Player_,level2Entities_,{},level1_.projectionListOrigin_,true,
        guard.fadeTicks,guard.specialActorPresent,level2PlayerCollisionMomentum_,true,guard.effectHandles};
    RecoveredLevel2PreloopWork result{std::move(ground),std::move(prefix),level2AllocationListOrder_,true};
    phase_=GameplayPhase::level2PreloopReturn;
    if(retry) {
        failureFromLevel2_=false;level2FailureTailFinished_=false;failureSourceLoopTick_.reset();
        recoveredFailureControl_.reset();failureAudioEvents_.clear();
    }
    return result;
}
} //namespace stellar7
