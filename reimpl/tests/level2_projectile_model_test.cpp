#include "stellar7/gameplay.hpp"
#include "stellar7/level2_projectile_prefix_receipt.hpp"
#include "level2_control_prefix_fixture.hpp"
#include "score_work_fixture.inc"

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

int main() {
    using namespace stellar7;
    const auto scoreFont=font();const auto pages=CockpitPageState::fromRecoveredMenu();
    for(const auto index:{0U,1U,2U,4U,5U,6U}) {
        auto active=GameplayModelTestAccess::prefix();GameplayModelTestAccess::timer(active,index,1);
        rejects([&] {active.recoveredLevel2ProjectilePrefixInput();});
    }
    auto pending=GameplayModelTestAccess::prefix();GameplayModelTestAccess::addShot(pending,64,true);
    const auto oldOrder=pending.level2ActiveListOrder();
    const auto work=pending.recoveredLevel2ProjectilePrefix(scoreFont,pages);
    assert(pending.level2ActiveListOrder()==oldOrder && work.projectileReceipt);
    assert(pending.recoveredLevel2RenderKeys().back()==32);
    auto forged=work;++forged.operations.front().count;
    rejects([&] {pending.commitRecoveredLevel2ProjectilePrefix(forged);});
    assert(pending.level2ActiveListOrder()==oldOrder);
    pending.commitRecoveredLevel2ProjectilePrefix(work);
    assert(pending.level1State().systemTimers()[3]==17);
    GameplayModelTestAccess::check(pending,true,false);
    assert(pending.level2Entities().back().stateBytes4dTo52[1]==64);
    assert(pending.level2ActiveListOrder().size()==8 && pending.level2LoopTick()==41);
    assert(pending.recoveredLevel2RenderKeys().back()==32);

    auto expiry=GameplayModelTestAccess::prefix();GameplayModelTestAccess::addShot(expiry,1,false);
    expiry.commitRecoveredLevel2ProjectilePrefix(expiry.recoveredLevel2ProjectilePrefix(scoreFont,pages));
    GameplayModelTestAccess::check(expiry,false,false);
    assert(expiry.level2Entities().size()==7 && expiry.level2ActiveListOrder().size()==7);
    assert(expiry.level2AudioEvents().empty() && expiry.level2LoopTick()==41);

    auto hit=GameplayModelTestAccess::prefix();GameplayModelTestAccess::addShot(hit,64,false,1);
    const auto dying=hit.recoveredLevel2ProjectilePrefix(scoreFont,pages);
    auto bad=dying;
    auto receipt=std::make_shared<Level2ProjectilePrefixReceipt>(*dying.projectileReceipt);
    ++receipt->work.score;bad.projectileReceipt=receipt;
    rejects([&] {hit.commitRecoveredLevel2ProjectilePrefix(bad);});
    assert(hit.level2AudioEvents().empty() && hit.level2Entities()[1].active());
    hit.commitRecoveredLevel2ProjectilePrefix(dying);
    GameplayModelTestAccess::check(hit,false,true);
    assert(hit.level2Entities().size()==8 && !hit.level2Entities()[1].active());
    assert(hit.recoveredLevel2RenderKeys().back()==40);
    assert(hit.level2AudioEvents().size()==1 && hit.level2AudioEvents()[0].eventId==6);
    assert(hit.level2AudioEvents()[0].loopTick==41 && hit.level2LoopTick()==41);
    assert(hit.level2Player().weaponCooldown==0);

    auto hostile=GameplayModelTestAccess::prefix();GameplayModelTestAccess::addShot(hostile,3,false);
    GameplayModelTestAccess::hostile(hostile);
    rejects([&] {hostile.recoveredLevel2ProjectilePrefix(scoreFont,pages);});
    const Level2HostileProjectileModel descriptor{44,{0,441}};
    const auto advanced=hostile.recoveredLevel2ProjectilePrefix(scoreFont,pages,&descriptor);
    auto changed=advanced;
    auto hostileReceipt=std::make_shared<Level2ProjectilePrefixReceipt>(*advanced.projectileReceipt);
    hostileReceipt->hostile->model19.baseRadius=134;changed.projectileReceipt=hostileReceipt;
    rejects([&] {hostile.commitRecoveredLevel2ProjectilePrefix(changed);});
    assert(hostile.level2Entities().back().stateBytes4dTo52[1]==3);
    hostile.commitRecoveredLevel2ProjectilePrefix(advanced);
    GameplayModelTestAccess::check(hostile,true,false);
    assert(hostile.level2Entities().back().type==19 && hostile.level2Entities().back().stateBytes4dTo52[1]==2);
    assert(hostile.level2Entities().back().ownerAllocationIndex==1);
}
