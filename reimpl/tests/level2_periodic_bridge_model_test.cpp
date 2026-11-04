#include "stellar7/gameplay.hpp"
#include "stellar7/level2_projectile_prefix_receipt.hpp"
#include "level2_control_prefix_fixture.hpp"
#include "score_work_fixture.inc"

#include <tuple>
#include <cstdlib>
#include <iostream>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void globals(Level1State& level) {
        level.levelIndex_=1;level.entities_.clear();level.selectedSystem_=-1;level.systemTimers_.fill(0);
        level.systemTimers_[3]=17; //48D4 preserves12D4; the six ability guards omit it.
        level.lastDestroyedType_=0;level.consecutiveDestroyedTypeCount_=0;
    }
    static void timer(Level1State& level,std::size_t index,std::int16_t value) {
        level.systemTimers_[index]=value;
    }
};
struct GameplayModelTestAccess {
    static void timer(GameplayModel& model,std::size_t index,std::int16_t value) {
        Level1ReloadTestAccess::timer(model.level1_,index,value);
    }
    //Synthetic source-shaped owner seam; no retained route or elapsed-clock claim.
    static GameplayModel prefix() {
        GameplayModel model;Level1ReloadTestAccess::globals(model.level1_);
        const auto ordinary=fixture();
        model.phase_=GameplayPhase::level2;model.activeCampaignLevel_=1;
        model.level2HasAdvanced_=true;model.level2TailConsumed_=true;
        model.level2LoopTick_=41;model.level2PlayerArmor_=680;model.level2Complete_=false;
        model.level2Player_=ordinary.player;model.level2Entities_=ordinary.actors;
        model.level2PrefixGuards_=GameplayModel::StationaryPrefixGuards{0,false,{false,false}};
        model.level2ActiveListOrder_=ordinary.activeOrder;model.level2AllocationListOrder_=ordinary.activeOrder;
        model.level2EntityActorPoolSlots_.clear();model.level2EntityFixedPoolSlots_.clear();
        model.level2EntityProjectilePoolSlots_.clear();model.level2EntityDeathEffectPoolSlots_.clear();
        model.level2ProjectilePoolActive_.fill(false);model.level2DeathEffectPoolActive_.fill(false);
        model.level2ActorPoolActive_.fill(false);
        std::size_t ordinarySlot=0,fixedSlot=0;
        for(std::size_t i=0;i<model.level2Entities_.size();++i) {
            auto& value=model.level2Entities_[i];value.allocationIndex=i;
            value.renderProfile=WorldActorRenderProfile::noHandles;value.animatedModelPresenterEnabled=false;
            const auto fixed=value.type==30;
            model.level2EntityActorPoolSlots_.push_back(fixed?model.level2ActorPoolSize_:ordinarySlot);
            model.level2EntityFixedPoolSlots_.push_back(fixed?fixedSlot:model.level2FixedObjectPoolSize_);
            model.level2EntityProjectilePoolSlots_.push_back(model.level2ProjectilePoolSize_);
            model.level2EntityDeathEffectPoolSlots_.push_back(model.level2DeathEffectPoolSize_);
            if(fixed) {model.level2FixedPool_[fixedSlot]=value;model.level2FixedPoolKnown_[fixedSlot++]=true;}
            else {model.level2ActorPool_[ordinarySlot]=value;model.level2ActorPoolActive_[ordinarySlot++]=true;}
        }
        for(std::size_t slot=0;slot<model.level2ProjectilePoolSize_;++slot) {
            auto& value=model.level2ProjectilePool_[slot];value={};value.stateBytes4dTo52[0]=253;
            value.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;value.animatedModelPresenterEnabled=false;
            model.level2DeathEffectPool_[slot]={};
        }
        model.level2Visibility_.resetLevel();
        model.level2ActiveListOrder_=model.level2Visibility_.refresh({},model.level2Entities_,
            model.level2AllocationListOrder_,{}).activeOrder;
        model.level2ObjectListOrder_=model.level2AllocationListOrder_;
        model.level2ActiveEnemyCount_=5;model.level2SpawnedEnemyCount_=5;model.level2Score_=0;
        model.level2AudioEvents_.clear();
        return model;
    }
    static void addShot(GameplayModel& model,const std::uint8_t life,const bool pending,
        const std::optional<std::size_t> target={}) {
        auto value=model.level2ProjectilePool_[0];value.allocationIndex=model.level2Entities_.size();
        value.type=23;value.collisionRadius=134;value.forwardVelocity=500;
        value.stateBytes4dTo52={253,life,0,27,0,41};value.ownerIsPlayer=true;
        value.position={-1000,-8000,0};value.collisionContactAllocationIndex=2;
        if(target) {
            const auto position=flatKinematicsOperations(model.level2Entities_.at(*target)).actor.position;
            Level1Entity moving{};moving.forwardVelocity=500;
            const auto delta=flatKinematicsOperations(moving).actor.position;
            value.position={position.x-delta.x,position.y-delta.y,position.z-delta.z};
        }
        model.level2ProjectilePool_[0]=value;model.level2ProjectilePoolActive_[0]=true;
        model.level2Entities_.push_back(value);
        model.level2EntityActorPoolSlots_.push_back(model.level2ActorPoolSize_);
        model.level2EntityFixedPoolSlots_.push_back(model.level2FixedObjectPoolSize_);
        model.level2EntityProjectilePoolSlots_.push_back(0);
        model.level2EntityDeathEffectPoolSlots_.push_back(model.level2DeathEffectPoolSize_);
        model.level2AllocationListOrder_.push_back(value.allocationIndex);
        if(pending) {model.level2Visibility_.allocationSucceeded();}
        else {model.level2ActiveListOrder_.push_back(value.allocationIndex);}
    }
    static void hostile(GameplayModel& model) {
        auto& shot=model.level2Entities_.back();
        shot.type=19;shot.collisionRadius=441;shot.ownerIsPlayer=false;shot.ownerAllocationIndex=1;
        model.level2ProjectilePool_[0]=shot;
    }
    static GameplayModel periodic(const std::int16_t shotType) {
        auto model=prefix();addShot(model,3,false);hostile(model);
        auto& owner=model.level2Entities_[1];
        owner.type=4;owner.collisionRadius=928;owner.stateBytes4dTo52={255,0,0,0,1,0};
        owner.yawVelocity=0;owner.weaponSlotCount=0;owner.forwardVelocity=0;
        model.level2ActorPool_[1]=owner;model.level2ActorPoolActive_[1]=false;
        auto& shot=model.level2Entities_[7];shot.type=shotType;
        shot.collisionRadius=shotType==20 ? 134 : 441;
        shot.collisionContactAllocationIndex=9;
        auto effect=Level1Entity{};effect.allocationIndex=8;effect.type=-1;
        effect.stateBytes4dTo52={252,1,0,0,5,0};
        effect.renderProfile=WorldActorRenderProfile::bitmapEffectCallback;
        effect.position={-16000,-16000,0};
        model.level2Entities_.push_back(effect);
        model.level2EntityActorPoolSlots_.push_back(model.level2ActorPoolSize_);
        model.level2EntityFixedPoolSlots_.push_back(model.level2FixedObjectPoolSize_);
        model.level2EntityProjectilePoolSlots_.push_back(model.level2ProjectilePoolSize_);
        model.level2EntityDeathEffectPoolSlots_.push_back(0);
        model.level2DeathEffectPool_[0]=effect;model.level2DeathEffectPoolActive_[0]=true;
        auto born=Level1Entity{};born.allocationIndex=9;born.type=1;born.collisionRadius=1342;
        born.stateBytes4dTo52={255,1,0,0,1,0};born.weaponSlotCount=2;born.weaponCooldown=8;
        born.renderProfile=WorldActorRenderProfile::noHandles;born.animatedModelPresenterEnabled=false;
        born.position={-30000,-30000,0};born.forwardVelocity=225;
        model.level2Entities_.push_back(born);
        model.level2EntityActorPoolSlots_.push_back(5);
        model.level2EntityFixedPoolSlots_.push_back(model.level2FixedObjectPoolSize_);
        model.level2EntityProjectilePoolSlots_.push_back(model.level2ProjectilePoolSize_);
        model.level2EntityDeathEffectPoolSlots_.push_back(model.level2DeathEffectPoolSize_);
        model.level2ActorPool_[5]=born;model.level2ActorPoolActive_[5]=true;
        //Move the retired physical owner past both pools; born FF also follows FD/FC.
        const std::vector<std::size_t> order{0,2,3,4,5,6,7,8,1,9};
        const auto reorder=[&](auto& values) {
            const auto old=values;
            for(std::size_t i=0;i<order.size();++i) {values[i]=old[order[i]];}
        };
        reorder(model.level2Entities_);reorder(model.level2EntityActorPoolSlots_);
        reorder(model.level2EntityFixedPoolSlots_);reorder(model.level2EntityProjectilePoolSlots_);
        reorder(model.level2EntityDeathEffectPoolSlots_);
        for(std::size_t i=0;i<order.size();++i) {model.level2Entities_[i].allocationIndex=i;}
        model.level2Entities_[6].ownerAllocationIndex=8;
        model.level2ProjectilePool_[0]=model.level2Entities_[6];
        model.level2ProjectilePool_[1].ownerAllocationIndex=8;
        model.level2ProjectilePool_[1].collisionContactAllocationIndex=9;
        model.level2AllocationListOrder_={0,1,2,3,4,5,6,7,9};
        model.level2ObjectListOrder_=model.level2AllocationListOrder_;
        model.level2ActiveListOrder_=model.level2AllocationListOrder_;
        model.level2ActiveEnemyCount_=5;model.level2SpawnedEnemyCount_=6;
        model.level2PlayerCollisionMomentum_=32;
        return model;
    }
    static void badLateContact(GameplayModel& model) {
        model.level2ProjectilePool_.back().collisionContactAllocationIndex=6;
    }
static auto actorFields(const Level1Entity& actor) {
    return std::tie(actor.allocationIndex,actor.flags,actor.type,actor.collisionRadius,
        actor.position.x,actor.position.y,actor.position.z,actor.pitch,actor.roll,actor.yaw,
        actor.pitchVelocity,actor.rollVelocity,actor.yawVelocity,actor.xVelocity,actor.yVelocity,
        actor.zVelocity,actor.forwardVelocity,actor.stateBytes4dTo52,actor.modelAnimationVariant,
        actor.primaryType11AnimationSelector,actor.behaviorTimer,actor.type12MissGate,
        actor.presenterDistance,actor.targetHeading,actor.weaponSlotCount,actor.weaponCooldown,
        actor.weaponSlotTimers,actor.collisionContactAllocationIndex,actor.collisionContactIsPlayer,
        actor.presentationModelAnimationVariant,actor.animatedModelPresenterEnabled,actor.ownerIsPlayer,
        actor.ownerAllocationIndex,actor.renderProfile);
}
    static void unchanged(const GameplayModel& model,const GameplayModel& before) {
        const auto sameActors=[](const auto& a,const auto& b) {
            assert(a.size()==b.size());
            for(std::size_t i=0;i<a.size();++i) {assert(actorFields(a[i])==actorFields(b[i]));}
        };
        sameActors(model.level2ActorPool_,before.level2ActorPool_);
        sameActors(model.level2FixedPool_,before.level2FixedPool_);
        sameActors(model.level2ProjectilePool_,before.level2ProjectilePool_);
        sameActors(model.level2DeathEffectPool_,before.level2DeathEffectPool_);
        sameActors(model.level2Entities_,before.level2Entities_);
        assert(actorFields(model.level2Player_)==actorFields(before.level2Player_));
        assert(model.level2ActorPoolActive_==before.level2ActorPoolActive_);
        assert(model.level2ProjectilePoolActive_==before.level2ProjectilePoolActive_);
        assert(model.level2DeathEffectPoolActive_==before.level2DeathEffectPoolActive_);
        assert(model.level2EntityActorPoolSlots_==before.level2EntityActorPoolSlots_);
        assert(model.level2EntityFixedPoolSlots_==before.level2EntityFixedPoolSlots_);
        assert(model.level2EntityProjectilePoolSlots_==before.level2EntityProjectilePoolSlots_);
        assert(model.level2EntityDeathEffectPoolSlots_==before.level2EntityDeathEffectPoolSlots_);
        assert(model.level2AllocationListOrder_==before.level2AllocationListOrder_);
        assert(model.level2ObjectListOrder_==before.level2ObjectListOrder_);
        assert(model.level2ActiveListOrder_==before.level2ActiveListOrder_);
        assert(model.level2AllocationRevision_==before.level2AllocationRevision_);
        assert(model.level2LoopTick_==before.level2LoopTick_);
        assert(model.level2TailConsumed_==before.level2TailConsumed_);
        assert(model.level2PlayerCollisionMomentum_==before.level2PlayerCollisionMomentum_);
        assert(model.level2PlayerArmor_==before.level2PlayerArmor_);
        assert(model.level2ActiveEnemyCount_==before.level2ActiveEnemyCount_);
        assert(model.level2Score_==before.level2Score_);
        assert(model.level2AudioEvents_.size()==before.level2AudioEvents_.size());
        assert(model.level2Visibility_.refreshRequested()==before.level2Visibility_.refreshRequested());
        assert(model.level2Rng_.calls()==before.level2Rng_.calls());
        auto rng=model.level2Rng_,prior=before.level2Rng_;
        for(std::size_t i=0;i<2*StellarRng::wordCount;++i) {assert(rng.next()==prior.next());}
    }
    static void periodicAliases(const GameplayModel& model) {
        assert(model.level2PlayerCollisionMomentum_==16);
        assert(model.level2Player_.forwardVelocity==32);
        const auto keys=model.recoveredLevel2RenderKeys();
        const auto owner=std::find(keys.begin(),keys.end(),1)-keys.begin();
        const auto born=std::find(keys.begin(),keys.end(),5)-keys.begin();
        assert(owner<keys.size() && born<keys.size());
        assert(model.level2Entities_[owner].type==4 && !model.level2Entities_[owner].active());
        assert(model.level2ProjectilePool_[0].ownerAllocationIndex==static_cast<std::size_t>(owner));
        assert(model.level2ProjectilePool_[0].collisionContactAllocationIndex==static_cast<std::size_t>(born));
        assert(model.level2ProjectilePool_[1].ownerAllocationIndex==static_cast<std::size_t>(owner));
        assert(model.level2ProjectilePool_[1].collisionContactAllocationIndex==static_cast<std::size_t>(born));
        assert(model.level2DeathEffectPoolActive_[0]);
        assert(model.level2EntityProjectilePoolSlots_[keys.size()-2]==0);
        assert(model.level2EntityDeathEffectPoolSlots_[keys.size()-1]==0);
        assert(std::find(model.level2AllocationListOrder_.begin(),model.level2AllocationListOrder_.end(),owner)
            ==model.level2AllocationListOrder_.end());
        for(std::size_t i=0;i<keys.size();++i) {assert(model.level2Entities_[i].allocationIndex==i);}
    }
    static void check(const GameplayModel& model,const bool projectile,const bool effect) {
        assert(model.level2ProjectilePoolActive_[0]==projectile);
        assert(model.level2DeathEffectPoolActive_[0]==effect);
        assert(!model.level2Visibility_.refreshRequested());
        for(std::size_t i=0;i<model.level2Entities_.size();++i) {
            const auto& value=model.level2Entities_[i];assert(value.allocationIndex==i);
            const auto ordinary=model.level2EntityActorPoolSlots_[i];
            const auto shot=model.level2EntityProjectilePoolSlots_[i];
            const auto bitmap=model.level2EntityDeathEffectPoolSlots_[i];
            if(ordinary<model.level2ActorPoolSize_) {
                assert(model.level2ActorPool_[ordinary].position==value.position);
            } else if(shot<model.level2ProjectilePoolSize_) {
                assert(model.level2ProjectilePool_[shot].position==value.position);
                assert(model.level2ProjectilePool_[shot].collisionContactAllocationIndex==2);
            } else if(bitmap<model.level2DeathEffectPoolSize_) {
                assert(model.level2DeathEffectPool_[bitmap].position==value.position);
                assert(value.renderProfile==WorldActorRenderProfile::bitmapEffectCallback);
            }
        }
    }
};
} // namespace stellar7

void periodicBridgeChecks() {
    using namespace stellar7;
    const Level2PeriodicOrdinaryProfile periodic{44,{0,1342},{0,928},{0,441},{0,134},{}};
    const Level2HostileProjectileModel hostile{44,{0,441}};
    for(const auto type:{17,19,20}) {
        auto model=GameplayModelTestAccess::periodic(static_cast<std::int16_t>(type));
        auto invalid=model;
        GameplayModelTestAccess::badLateContact(invalid);
        const auto invalidBefore=invalid;
        const auto invalidWork=invalid.recoveredLevel2PeriodicProjectilePrefix(font(),
            CockpitPageState::fromRecoveredMenu(),periodic,&hostile);
        rejects([&] {invalid.commitRecoveredLevel2ProjectilePrefix(invalidWork);});
        GameplayModelTestAccess::unchanged(invalid,invalidBefore);
        const auto input=model.recoveredLevel2PeriodicProjectilePrefixInput();
        assert(input.prefix.entities.size()==8 && input.prefix.impulse==32);
        assert(input.prefix.projectiles[0].ownerAllocationIndex==6);
        assert(input.prefix.entities[6].type==4 && !input.prefix.entities[6].active());
        assert(input.prefix.entities[7].type==1);
        rejects([&] {model.recoveredLevel2ProjectilePrefixInput();});
        const auto work=model.recoveredLevel2PeriodicProjectilePrefix(font(),CockpitPageState::fromRecoveredMenu(),
            periodic,&hostile);
        assert(work.projectileReceipt->work.damage.combat.prefix.impulse==16);
        assert(work.projectileReceipt->work.damage.combat.prefix.player.forwardVelocity==32);
        auto impulseReceipt=std::make_shared<Level2ProjectilePrefixReceipt>(*work.projectileReceipt);
        ++impulseReceipt->work.damage.combat.prefix.impulse;
        auto impulseForgery=work;impulseForgery.projectileReceipt=impulseReceipt;
        rejects([&] {model.commitRecoveredLevel2ProjectilePrefix(impulseForgery);});
        assert(model.recoveredLevel2PeriodicProjectilePrefixInput().prefix.impulse==32);
        auto forged=work;++forged.operations.front().count;
        rejects([&] {model.commitRecoveredLevel2ProjectilePrefix(forged);});
        assert(model.recoveredLevel2RenderKeys()[8]==1);
        model.commitRecoveredLevel2ProjectilePrefix(work);
        GameplayModelTestAccess::periodicAliases(model);
        assert(model.recoveredLevel2PeriodicProjectilePrefixInput().prefix.impulse==16);
        rejects([&] {model.recoveredLevel2ProjectilePrefixInput();});
    }
}

int main() {
#ifdef _MSC_VER
    _set_error_mode(_OUT_TO_STDERR);
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    try {periodicBridgeChecks();return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
