#include "stellar7/gameplay.hpp"
#include "stellar7/level2_projectile_prefix_receipt.hpp"
#include "level2_control_prefix_fixture.hpp"
#include "score_work_fixture.inc"
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
    static GameplayModel periodicKill(const std::int16_t type) {
        auto model=prefix();const auto index=type==8 ? 1U : 0U;
        auto& target=model.level2Entities_[index];
        target.type=type;target.collisionRadius=type==1 ? 1342 : type==4 ? 928 : type==8 ? 860 : 559;
        target.stateBytes4dTo52={255,1,0,0,static_cast<std::uint8_t>(type==1 || type==4 ? 1 : type==8 ? 2 : 4),0};
        target.xVelocity=0;target.yVelocity=0;target.zVelocity=0;target.forwardVelocity=0;
        target.yawVelocity=0;target.weaponSlotCount=0;target.weaponCooldown=0;
        model.level2ActorPool_[index]=target;model.level2Score_=100;
        addShot(model,64,false,index);
        return model;
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

void scoreCallerChecks() {
    using namespace stellar7;
    const Level2PeriodicOrdinaryProfile periodic{44,{0,1342},{0,928},{0,441},{0,134},{}};
    const auto pages=CockpitPageState::fromRecoveredMenu();
    for(const auto type:{1,4,8,13}) {
        auto model=GameplayModelTestAccess::periodicKill(static_cast<std::int16_t>(type));
        const auto targetIndex=type==8 ? 1U : 0U;
        const auto work=model.recoveredLevel2PeriodicProjectilePrefix(font(),pages,periodic);
        const auto& receipt=work.projectileReceipt->work;
        const auto& prefix=receipt.damage.combat.prefix;
        const auto award=type==1 ? 20U : type==4 ? 30U : type==8 ? 25U : 10U;
        assert(receipt.targetHits.size()==1 && receipt.targetHits[0].work.destroyed);
        assert(receipt.activeEnemies==4 && !prefix.entities[targetIndex].active());
        assert(receipt.score==100+award && receipt.scoreSteps.size()==1);
        const auto& score=receipt.scoreSteps[0];
        assert(score.work.score==100+award && score.operationOffset>0);
        const auto& caller=prefix.operations.at(score.operationOffset-1);
        assert(caller.kind==NormalWorkKind::instructions && caller.count==5);
        const auto& cleanup=prefix.operations.at(score.operationOffset);
        assert(cleanup.kind==NormalWorkKind::instructions && cleanup.count==2);
        assert(prefix.projectiles[0].stateBytes4dTo54[1]==0);
        if(type==1 || type==4) {
            rejects([&] {model.recoveredLevel2ProjectilePrefix(font(),pages);});
        } else {
            const auto strict=model.recoveredLevel2ProjectilePrefix(font(),pages);
            assert(strict.projectileReceipt->work.score==receipt.score);
            assert(strict.projectileReceipt->work.scoreSteps.size()==1);
        }
        model.commitRecoveredLevel2ProjectilePrefix(work);
        assert(model.score()==100+award && !model.level2Entities()[targetIndex].active());
    }
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    _set_error_mode(_OUT_TO_STDERR);
#endif
    try {scoreCallerChecks();return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
