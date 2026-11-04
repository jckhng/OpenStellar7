#include "stellar7/gameplay.hpp"
#include <cassert>
#include <stdexcept>

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void sourceGlobals(Level1State& level) {
        level.levelIndex_=1;level.entities_.clear();level.selectedSystem_=-1;level.systemTimers_.fill(0);
    }
};
struct GameplayModelTestAccess {
    // Synthetic lower-model seam, not a retained route or elapsed-clock fixture.
    static GameplayModel tail() {
        GameplayModel model;
        Level1ReloadTestAccess::sourceGlobals(model.level1_);
        model.phase_=GameplayPhase::level2;model.activeCampaignLevel_=1;
        model.level2HasAdvanced_=true;model.level2TailConsumed_=false;model.level2Complete_=false;
        model.level2PlayerArmor_=680;model.level2Player_={};model.level2Player_.type=31;
        model.level2PrefixGuards_=GameplayModel::StationaryPrefixGuards{0,false,{false,false}};
        model.level2Entities_.clear();model.level2AllocationListOrder_.clear();model.level2ActiveListOrder_.clear();
        model.level2EntityActorPoolSlots_.clear();model.level2EntityFixedPoolSlots_.clear();
        model.level2EntityProjectilePoolSlots_.clear();model.level2EntityDeathEffectPoolSlots_.clear();
        for(std::size_t i=0;i<7;++i) {
            Level1Entity actor{};actor.allocationIndex=i;actor.type=i<3?8:i<5?13:30;
            actor.stateBytes4dTo52={static_cast<std::uint8_t>(i<5?255:254),1,0,0,0,0};
            actor.renderProfile=WorldActorRenderProfile::noHandles;actor.position={static_cast<int>(4096+i*1024),8192,0};
            model.level2Entities_.push_back(actor);model.level2AllocationListOrder_.push_back(i);
            model.level2EntityActorPoolSlots_.push_back(i<5?i:model.level2ActorPoolSize_);
            model.level2EntityFixedPoolSlots_.push_back(i<5?model.level2FixedObjectPoolSize_:i-5);
            model.level2EntityProjectilePoolSlots_.push_back(model.level2ProjectilePoolSize_);
            model.level2EntityDeathEffectPoolSlots_.push_back(model.level2DeathEffectPoolSize_);
        }
        model.level2ProjectilePoolActive_.fill(false);
        for(auto& shot:model.level2ProjectilePool_) {
            shot={};shot.stateBytes4dTo52[0]=253;shot.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
        }
        auto& carried=model.level2ProjectilePool_[0];carried.flags=4;carried.type=22;
        carried.pitchVelocity=17;carried.yVelocity=41;carried.presenterDistance=9347;
        carried.collisionContactAllocationIndex=2;
        model.level2Visibility_.resetLevel();
        model.level2ActiveListOrder_=model.level2Visibility_.refresh({},model.level2Entities_,
            model.level2AllocationListOrder_,{}).activeOrder;
        model.level2ObjectListOrder_=model.level2AllocationListOrder_;
        model.level2AudioEvents_.clear();model.level2AllocationRevision_=0;
        return model;
    }
    static void allocated(const GameplayModel& model) {
        assert(model.level2AllocationListOrder_.size()==8 && model.level2AllocationListOrder_.back()==7);
        assert(model.level2ActiveListOrder_.size()==7 && model.level2Visibility_.refreshRequested());
        assert(model.level2ProjectilePoolActive_[0] && model.level2EntityProjectilePoolSlots_[7]==0);
        const auto& shot=model.level2ProjectilePool_[0];
        assert(shot.flags==4 && shot.pitchVelocity==17 && shot.yVelocity==41 && shot.presenterDistance==9347);
        assert(shot.collisionContactAllocationIndex==2 && shot.ownerIsPlayer);
        assert(shot.stateBytes4dTo52[1]==64 && shot.type==23 && shot.collisionRadius==134);
    }
    static void hostile(GameplayModel& model) {
        Level1Entity shot{};shot.allocationIndex=model.level2Entities_.size();
        shot.type=19;shot.collisionRadius=441;shot.position={0,10000,0};shot.forwardVelocity=500;
        shot.stateBytes4dTo52={253,84,0,0,0,0};shot.ownerIsPlayer=false;shot.ownerAllocationIndex=1;
        shot.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
        model.level2Entities_[1].stateBytes4dTo52[4]=2;
        model.level2ProjectilePool_[0]=shot;model.level2ProjectilePoolActive_[0]=true;
        model.level2Entities_.push_back(shot);model.level2AllocationListOrder_.push_back(shot.allocationIndex);
        model.level2ObjectListOrder_=model.level2AllocationListOrder_;
        model.level2EntityActorPoolSlots_.push_back(model.level2ActorPoolSize_);
        model.level2EntityFixedPoolSlots_.push_back(model.level2FixedObjectPoolSize_);
        model.level2EntityProjectilePoolSlots_.push_back(0);
        model.level2EntityDeathEffectPoolSlots_.push_back(model.level2DeathEffectPoolSize_);
        model.level2Visibility_.allocationSucceeded();
    }
    static void wrongOwner(GameplayModel& model) {model.level2Entities_.back().ownerAllocationIndex=3;}
    static void retiredHostile(GameplayModel& model) {
        auto& shot=model.level2ProjectilePool_[0];shot.type=19;shot.pitchVelocity=0;
        shot.ownerIsPlayer=false;shot.ownerAllocationIndex=1;
    }
};
}
namespace {
template<class Function> void rejects(Function function) {
    bool rejected=false;try {function();} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
}
}
int main() {
    using namespace stellar7;
    auto model=GameplayModelTestAccess::tail();
    GameplayAssets assets{};assets.models.resize(45);assets.sharedModelRegistry[0]=44;
    assets.models[23].scaleShift=0;assets.models[23].collisionRadius=134;
    const auto input=model.recoveredLevel2PrimaryInput(assets);
    assert(input.membership==Level1PrimaryFireMembership::level2Constructor && input.allocationCount==7);
    const auto work=level1PrimaryFireOperations(input);
    auto invalid=work;++invalid.beforeEvent14.front().count;
    rejects([&] {model.commitRecoveredLevel2Primary(assets,invalid);});
    assert(model.level2Entities().size()==7 && model.level2AudioEvents().empty());
    model.commitRecoveredLevel2Primary(assets,work);
    GameplayModelTestAccess::allocated(model);
    assert(model.level2Player().weaponCooldown==9 && model.level2Player().weaponSlotTimers[0]==32);
    assert(model.level2AudioEvents().size()==1 && model.level2AudioEvents().front().eventId==14);
    rejects([&] {model.commitRecoveredLevel2Primary(assets,work);});
    const auto cooldown=level1PrimaryFireOperations(model.recoveredLevel2PrimaryInput(assets));
    assert(!cooldown.launch);model.commitRecoveredLevel2Primary(assets,cooldown);
    assert(model.level2AudioEvents().size()==1 && model.level2Entities().size()==8);
    auto mixed=GameplayModelTestAccess::tail();GameplayModelTestAccess::hostile(mixed);
    rejects([&] {mixed.recoveredLevel2PrimaryInput(assets);});
    const auto shared=mixed.recoveredLevel2CombatPrimaryInput(assets);
    assert(shared.pool[0].type==19 && shared.pool[0].ownerAllocationIndex==1);
    const auto primary=level1PrimaryFireOperations(shared);
    assert(primary.launch && primary.launch->poolSlot==1 && primary.launch->projectile.type==23);
    mixed.commitRecoveredLevel2CombatPrimary(assets,primary);
    assert(mixed.level2Entities()[7].type==19 && mixed.level2Entities()[7].ownerAllocationIndex==1);
    assert(mixed.level2Entities()[8].type==23 && mixed.level2Entities()[8].ownerIsPlayer);
    auto invalidOwner=GameplayModelTestAccess::tail();GameplayModelTestAccess::hostile(invalidOwner);
    GameplayModelTestAccess::wrongOwner(invalidOwner);
    rejects([&] {invalidOwner.recoveredLevel2CombatPrimaryInput(assets);});
    auto reused=GameplayModelTestAccess::tail();GameplayModelTestAccess::retiredHostile(reused);
    const auto reuse=level1PrimaryFireOperations(reused.recoveredLevel2CombatPrimaryInput(assets));
    assert(reuse.launch && reuse.launch->poolSlot==0 && reuse.launch->projectile.type==23);
    reused.commitRecoveredLevel2CombatPrimary(assets,reuse);
    const auto& replacement=reused.level2Entities().back();
    assert(replacement.type==23 && replacement.ownerIsPlayer && replacement.stateBytes4dTo52[1]==64);
    assert(replacement.flags==4 && replacement.yVelocity==41 && replacement.presenterDistance==9347);
}
