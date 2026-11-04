#include "stellar7/gameplay.hpp"
#include "stellar7/weapon_lamp_work.hpp"
#include <cassert>
#include <stdexcept>
#include <iostream>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void sourceGlobals(Level1State& level) {
        level.levelIndex_=1;level.entities_.clear();level.selectedSystem_=-1;level.systemTimers_.fill(0);
        level.systemTimers_[3]=17;
    }
    static void timer(Level1State& level,std::size_t index,std::int16_t value) {
        level.systemTimers_[index]=value;
    }
};
struct GameplayModelTestAccess {
    static void timer(GameplayModel& model,std::size_t index,std::int16_t value) {
        Level1ReloadTestAccess::timer(model.level1_,index,value);
    }
    static GameplayModel lifecycle(std::int16_t armor,std::uint16_t tick) {
        auto model=tail();model.level2PlayerArmor_=armor;model.level2LoopTick_=tick;return model;
    }
    // Synthetic lower-model seam, not a retained route or elapsed-clock fixture.
    static GameplayModel tail() {
        GameplayModel model;
        Level1ReloadTestAccess::sourceGlobals(model.level1_);
        model.phase_=GameplayPhase::level2;model.activeCampaignLevel_=1;
        model.level2HasAdvanced_=true;model.level2TailConsumed_=false;model.level2Complete_=false;
        model.level2LoopTick_=41;model.level2ActiveEnemyCount_=5;model.level2SpawnedEnemyCount_=5;
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
};
}
namespace {
template<class Function> void rejects(Function function) {
    bool rejected=false;try {function();} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    try {
    using namespace stellar7;
    auto model=GameplayModelTestAccess::tail();
    GameplayAssets assets{};assets.models.resize(45);assets.sharedModelRegistry[0]=44;
    assets.models[23].scaleShift=0;assets.models[23].collisionRadius=134;
    rejects([&]{model.recoveredLevel2PrimaryInput(assets);});
    for(const auto index:{0U,1U,2U,4U,5U,6U}) {
        auto active=GameplayModelTestAccess::tail();GameplayModelTestAccess::timer(active,index,1);
        rejects([&]{active.recoveredLevel2CombatPrimaryInput(assets);});
    }
    const auto input=model.recoveredLevel2CombatPrimaryInput(assets);
    assert(input.membership==Level1PrimaryFireMembership::level2Combat);
    const auto fire=level1PrimaryFireOperations(input);
    const auto old=model.level2ActiveListOrder();
    model.commitRecoveredLevel2CombatPrimary(assets,fire);
    GameplayModelTestAccess::allocated(model);
    assert(model.level2ActiveListOrder()==old);
    assert(!model.weaponIndicatorVisible());
    auto pages=CockpitPageState::fromRecoveredMenu();
    const auto lamp=weaponLampOperations(0xffff,WeaponLampState::unavailable,{24,5,std::vector<std::uint8_t>(120,7)},pages);
    model.commitRecoveredLevel2WeaponLamp(lamp);
    assert(model.weaponIndicatorVisible() && !model.weaponReadyIndicator());
    FrameTailState state{model.level2LoopTick(),model.level2Player().position,0,0,{0,0,0},{}};
    for(const auto index:old) {const auto& actor=model.level2Entities()[index];state.actors.push_back({actor.type,actor.position});}
    FrameTailGuards guards{};guards.armor=680;guards.missionActive=true;guards.heldKeys[8]=true;
    const auto tail=level2CombatFrameTailOperations(state,guards,Level1TailFireAction{fire,lamp.operations});
    assert(tail.state.weaponTimers==(std::array<std::uint8_t,3>{8,31,0}));
    model.commitRecoveredLevel2CombatTail(tail.state);
    assert(model.level1State().systemTimers()[3]==17);
    assert(model.level2LoopTick()==42 && model.level2Player().weaponCooldown==8);
    assert(model.level2ActiveListOrder()==old && model.level2AudioEvents().size()==1);
    rejects([&]{model.commitRecoveredLevel2CombatTail(tail.state);});
    for(const auto armor:{199,0,-1,-896}) {
        for(const auto tick:{std::uint16_t{63},std::uint16_t{64}}) {
            auto lifecycle=GameplayModelTestAccess::lifecycle(static_cast<std::int16_t>(armor),tick);
            FrameTailState incoming{tick,lifecycle.level2Player().position,0,0,{0,0,0},{}};
            for(const auto index:lifecycle.level2ActiveListOrder()) {
                const auto& actor=lifecycle.level2Entities()[index];incoming.actors.push_back({actor.type,actor.position});
            }
            FrameTailGuards lifecycleGuards{};lifecycleGuards.armor=static_cast<std::int16_t>(armor);
            lifecycleGuards.missionActive=true;
            if(tick==64) {
                lifecycleGuards.periodic=FrameTailPeriodicInput{1,5,5,12,0,
                    lifecycle.level2Rng().indexA(),lifecycle.level2Rng().indexB(),0};
            }
            rejects([&]{lifecycle.recoveredLevel2CombatPrimaryInput(assets);});
            lifecycle.recoveredLevel2CombatPrimaryInput(assets,Level1PlayerCombatDomain::expiryAndLoss);
            const auto work=level2LifecycleFrameTailOperations(incoming,lifecycleGuards);
            rejects([&]{lifecycle.commitRecoveredLevel2CombatTail(work.work.state);});
            const auto rngCalls=lifecycle.level2Rng().calls();
            auto wrong=work;wrong.failureRequested=!work.failureRequested;
            rejects([&]{lifecycle.commitRecoveredLevel2LifecycleTail(wrong);});
            assert(lifecycle.level2Rng().calls()==rngCalls && lifecycle.level2LoopTick()==tick);
            lifecycle.commitRecoveredLevel2LifecycleTail(work);
            assert(lifecycle.level2LoopTick()==tick+1 && lifecycle.level2Rng().calls()==rngCalls+(tick==64));
            assert(lifecycle.phase()==(armor<0 ? GameplayPhase::failurePreExplosionBlackout : GameplayPhase::level2));
            assert(lifecycle.level2AudioEvents().size()==(tick==64 ? 1U : 0U));
            assert(!lifecycle.pendingLevel2FailureTail());
            rejects([&]{lifecycle.commitRecoveredLevel2LifecycleTail(work);});
        }
    }
    return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
