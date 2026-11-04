#include "stellar7/recovered_level2_model.hpp"
#include "stellar7/level2_preloop_prefix_work.hpp"
#include <cassert>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#include <iostream>
#include <stdexcept>

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void successReturn(Level1State& state) {
        state.levelIndex_=0;state.loopTick_=17;state.player_.position={100,-250,0};state.player_.yawVelocity=20;state.player_.forwardVelocity=30;
        state.playerArmor_=600;state.score_=3400;state.systemTimers_={0,0,0,17,0,0,0};state.selectedSystem_=-1;
        state.entities_[0].stateBytes4dTo52[1]=0;state.entities_[1].stateBytes4dTo52[1]=0;
        Level1Entity guardian{};guardian.allocationIndex=state.entities_.size();guardian.type=44;guardian.collisionRadius=2792;
        guardian.stateBytes4dTo52={255,0,2,0,5,0};guardian.renderProfile=WorldActorRenderProfile::heapSelectorNoCallback;guardian.animatedModelPresenterEnabled=false;
        state.postWaveObjectAllocationIndex_=guardian.allocationIndex;state.entities_.push_back(guardian);
        Level1Entity reward{};reward.allocationIndex=state.entities_.size();reward.type=25;reward.collisionRadius=897;reward.position={12000,4000,0};
        reward.stateBytes4dTo52={254,100,2,0,0,1};reward.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;reward.animatedModelPresenterEnabled=false;
        state.entities_.push_back(reward);state.completionObjectAllocationIndex_=static_cast<std::size_t>(-1);
        state.activeEnemyCount_=0;state.spawnedEnemyCount_=12;state.missionActive_=false;state.successState_=true;
        state.failureState_=false;state.failurePending_=false;state.exitState_=false;state.successContactPending_=false;
        state.ordinaryTailPending_=false;state.ordinaryTailConsumed_=true;state.hasAdvanced_=true;state.playerCollisionMomentum_=0;
        state.playerWeaponCooldown_=0;state.playerProjectileTimers_={};state.projectionListOrigin_={13,29,0};
        state.allocationList_.clear();state.objectListOrder_.clear();state.activeListOrder_.clear();state.semanticActiveListOrder_.clear();
        for(const std::size_t index:{2,3,4,5,7}) {
            state.allocationList_.push_back({Level1State::ObjectKind::entity,index});state.objectListOrder_.push_back({Level1State::ObjectKind::entity,index});
            state.activeListOrder_.push_back(index);state.semanticActiveListOrder_.push_back(index);
        }
        std::array<std::uint16_t,56> table{};for(unsigned i=0;i<table.size();++i) {table[i]=static_cast<std::uint16_t>(i*1193+83);}
        state.rng_=StellarRng(table,48,110,0,100);state.lastDestroyedType_=44;state.consecutiveDestroyedTypeCount_=1;
        //Source FC ground/impact allocation writes no selector handle. Its old
        //position/type survive retirement; the loader must release null handles.
        auto& effect=state.deathEffects_[0].entity;
        effect.type=-1;effect.position={1234,-5678,0};
        effect.stateBytes4dTo52={252,0,0,0,0,0};

    }
    static void materializeUpperFixed(Level1State& state) {
        for(unsigned i=0;i<4;++i) {
            Level1Entity actor{};actor.allocationIndex=state.entities_.size();actor.stateBytes4dTo52[0]=254;
            actor.renderProfile=WorldActorRenderProfile::noHandles;state.entities_.push_back(actor);
        }
    }
};
struct GameplayModelTestAccess {
    static GameplayModel returned() {
        auto model=GameplayModel::fromNativeStartup();Level1ReloadTestAccess::successReturn(model.level1_);
        model.phase_=GameplayPhase::successPresentation;model.activeCampaignLevel_=0;model.level2PickupIndex_=7;
        model.recoveredSuccessPause_.reset();model.recoveredSuccessReturned_=false;model.ticks_=17;model.recoveredLevel1TickBase_=0;
        model.remainingContinues_=1;model.level2CRandState_=0x12345678;model.level2VisualRandomState_=0x87654321;
        model.level2ActorIncarnations_[0]=9;model.level2AllocationRevision_=100;model.level2NonProjectileAllocationRevision_=50;model.level2FixedAllocationRevision_=30;
        auto& upper=model.level2FixedPool_[9];upper.type=42;upper.position={123,456,789};upper.stateBytes4dTo52={254,7,0,0,0,0};
        upper.renderProfile=WorldActorRenderProfile::heapSelectorNoCallback;model.level2FixedPoolKnown_[9]=true;model.level2FixedPoolEverUsed_[9]=true;
        std::vector<Level1Entity> empty;model.level2Visibility_.refresh({777,888,0},empty,{},{});
        return model;
    }
    static void cleared(const GameplayModel& model) {
        assert(model.level2FixedPool_[9].type==42&&model.level2FixedPool_[9].stateBytes4dTo52[1]==7);
        assert((model.level2FixedPool_[9].position==FixedPosition{123,456,789}));assert(model.level2FixedPoolKnown_[9]&&model.level2FixedPoolEverUsed_[9]);
        assert(model.level2FixedPool_[9].renderProfile==WorldActorRenderProfile::heapSelectorNoCallback);
        assert(model.level2ActorIncarnations_[0]==9&&model.level2AllocationRevision_==100);
        assert(model.level2Visibility_.tracking()==VisibilityTracking::unknown);
        assert(model.level2Visibility_.lastCamera()&&(*model.level2Visibility_.lastCamera()==FixedPosition{777,888,0}));
        assert(model.level1_.entities().empty()&&model.level2PickupIndex_==static_cast<std::size_t>(-1));
        assert(model.level2CRandState_==0x12345678&&model.level2VisualRandomState_==0x87654321);
        assert(!model.level2TailConsumed_&&!model.level2HasAdvanced_&&model.level2ActiveListOrder_.empty());
    }
    static void loaded(const GameplayModel& model) {
        assert(model.level2ActorIncarnations_[0]==10&&model.level2AllocationRevision_==107&&model.level2NonProjectileAllocationRevision_==57&&model.level2FixedAllocationRevision_==32);
        assert(model.level2EntityActorPoolSlots_==std::vector<std::size_t>({0,1,2,3,4,16,16}));
        assert(model.level2EntityFixedPoolSlots_==std::vector<std::size_t>({16,16,16,16,16,0,1}));
        assert(model.level2ActiveListOrder_.empty()&&model.level2ObjectListOrder_.empty());
        assert(model.level2AllocationListOrder_==std::vector<std::size_t>({0,1,2,3,4,5,6}));
        assert(model.level2Visibility_.tracking()==VisibilityTracking::sourceInitialized&&model.level2Visibility_.refreshRequested());
        assert((*model.level2Visibility_.lastCamera()==FixedPosition{777,888,0}));
        assert(!model.level2HasAdvanced_&&!model.level2TailConsumed_&&model.level2CRandState_==0x12345678&&model.level2VisualRandomState_==0x87654321);
    }
    static void warning(GameplayModel& model) {model.level2ArmorWarningActive_=true;}
    static bool warning(const GameplayModel& model) {return model.level2ArmorWarningActive_;}
    static void poisonReset(GameplayModel& model) {model.level2ActorPool_[15].flags=1;}
    static void materializeUpperFixed(GameplayModel& model) {Level1ReloadTestAccess::materializeUpperFixed(model.level1_);}
};
}
using namespace stellar7;
template<class Action>void rejects(Action action) {bool rejected=false;try {action();}catch(const std::invalid_argument&) {rejected=true;}assert(rejected);}
int main() {
#ifdef _MSC_VER
_CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
try {
    auto model=GameplayModelTestAccess::returned();GameplayAssets assets{};assets.models.resize(45);
    assets.sharedModelRegistry[0]=44;assets.models[23].collisionRadius=134;
    std::vector<Model3d> shared(44);shared[0].flags=1;shared[0].collisionRadius=723;shared[8].flags=1;shared[8].collisionRadius=860;
    shared[13].collisionRadius=559;shared[30].collisionRadius=868;
    const auto pages=CockpitPageState::fromRecoveredMenu();FrameTailGuards guards{600,{},{},0x1e,0x1e,false,false,false,false,true,false,{}};
    const auto tail=model.advanceRecoveredLevel1SuccessTail(assets,pages,guards,0);
    assert(model.phase()==GameplayPhase::level1SuccessTailReturn&&model.ticks()==18&&model.level1State().loopTick()==18);
    assert(!tail.tail.periodic.tail.missionActive&&!tail.lamp&&model.level1State().rng().calls()==100);
    assert((model.level1State().player().position==FixedPosition{}));assert(model.level1State().player().forwardVelocity==20);
    rejects([&] {model.advanceRecoveredLevel1SuccessTail(assets,pages,guards,0);});
    const auto envelope=model.recoveredLevel2TransitionOperations(pages,0);assert(envelope.observation.levelIndex==1&&envelope.observation.armor==680);
    rejects([&] {model.recoveredLevel2TransitionOperations(pages,1);});
    model.beginRecoveredLevel2Load();assert(model.phase()==GameplayPhase::level2LoaderReturn&&model.campaignLevel()==1&&model.level2LoopTick()==18);
    assert(model.ticks()==18&&model.remainingContinues()==1);rejects([&] {model.beginRecoveredLevel2Load();});
    GameplayModelTestAccess::warning(model);const auto reset=model.resetRecoveredLevel2Pools(shared,{});
    assert(reset.reset.selectorReleases.size()==2);assert(reset.reset.selectorReleases[0].pool==Level2ResetPool::ordinary&&reset.reset.selectorReleases[0].slot==1);
    assert(reset.reset.selectorReleases[1].pool==Level2ResetPool::ordinary&&reset.reset.selectorReleases[1].slot==2);
    assert(reset.projectionResetKeys.size()==40&&reset.projectionResetKeys[0]==0&&reset.projectionResetKeys[23]==23&&reset.projectionResetKeys[24]==32&&reset.projectionResetKeys.back()==47);
    assert(model.playerArmor()==680&&model.score()==3400&&model.level1State().systemTimers()[3]==17&&model.level1State().lastDestroyedType()==44);
    assert(model.level1State().consecutiveDestroyedTypeCount()==0&&model.level2Rng().calls()==100&&GameplayModelTestAccess::warning(static_cast<const GameplayModel&>(model)));
    GameplayModelTestAccess::cleared(model);rejects([&] {model.resetRecoveredLevel2Pools(shared,{});});
    const auto constructor=model.constructRecoveredLevel2Actors(shared,{});assert(constructor.actors.size()==7&&model.level2Rng().calls()==112);
    GameplayModelTestAccess::loaded(model);rejects([&] {model.constructRecoveredLevel2Actors(shared,{});});
    const auto pageInput=model.prepareRecoveredLevel2LoaderPages({82,{}});assert(pageInput.cockpit.levelIndex==1&&pageInput.cockpit.displaySteps==2);
    assert(pageInput.player.stateBytes4dTo52[1]==1&&pageInput.armor==680&&model.level2LoopTick()==18&&model.ticks()==18);
    assert(model.cockpitIndicatorX()==pageInput.cockpit.states[2].x);rejects([&] {model.prepareRecoveredLevel2LoaderPages({82,{}});});
    model.completeRecoveredLevel2MusicActivation();assert(model.level2LoopTick()==0&&model.phase()==GameplayPhase::level2MusicReturn&&model.ticks()==18);
    rejects([&] {model.completeRecoveredLevel2MusicActivation();});const auto preloop=model.completeRecoveredLevel2Preloop();
    assert(model.phase()==GameplayPhase::level2PreloopReturn&&model.level2Rng().calls()==124&&model.ticks()==18&&model.remainingContinues()==1);
    assert(preloop.firstPrefix.activeOrder.empty()&&preloop.firstPrefix.refreshRequested&&preloop.firstEntryDisplay);
    assert((preloop.firstPrefix.lastListCamera==FixedPosition{13,29,0}));assert(preloop.allocationOrder==std::vector<std::size_t>({0,1,2,3,4,5,6}));
    assert(preloop.firstPrefix.actors.size()==7&&preloop.firstPrefix.player.collisionRadius==723&&preloop.firstPrefix.player.forwardVelocity==0);
    const Level2PreloopPrefixInput resetPrefix{preloop.firstPrefix,preloop.allocationOrder};
    const auto first=level2ResetPreloopPrefixOperations(resetPrefix);
    assert(first.player.type==0&&first.activeOrder.size()==7&&!first.refreshRequested);
    rejects([&] {level2PreloopPrefixOperations(resetPrefix);});
    auto captured=resetPrefix;captured.prefix.player.type=31;
    const auto controlled=level2PreloopPrefixOperations(captured);
    assert(controlled.player.type==31&&controlled.operations.size()==first.operations.size());
    for(std::size_t index=0;index<first.operations.size();++index) {
        assert(first.operations[index].kind==controlled.operations[index].kind
            &&first.operations[index].count==controlled.operations[index].count
            &&first.operations[index].value==controlled.operations[index].value);
    }
    rejects([&] {level2ResetPreloopPrefixOperations(captured);});
    GameplayModelTestAccess::loaded(model);rejects([&] {model.completeRecoveredLevel2Preloop();});
    auto firing=GameplayModelTestAccess::returned();auto fireGuards=guards;fireGuards.heldKeys[8]=true;
    assets.artGroups.resize(5);assets.artGroups[4].resize(41);assets.artGroups[4][39]={24,5,std::vector<std::uint8_t>(120)};
    assets.artGroups[4][40]=assets.artGroups[4][39];
    const auto fireTail=firing.advanceRecoveredLevel1SuccessTail(assets,pages,fireGuards,0);
    assert(fireTail.lamp&&firing.level1State().projectiles()[0].active()&&firing.level1State().playerWeaponCooldown()==8);
    assert(firing.level1State().playerProjectileTimers()[0]==31&&firing.level1State().audioEvents().back().eventId==14);
    assert((firing.level1State().projectiles()[0].position==FixedPosition{100,-250,0})); //New49F8 append is absent from old45D2 recenter.
    auto invalid=GameplayModelTestAccess::returned();invalid.advanceRecoveredLevel1SuccessTail(assets,pages,guards,0);invalid.beginRecoveredLevel2Load();
    GameplayModelTestAccess::poisonReset(invalid);rejects([&] {invalid.resetRecoveredLevel2Pools(shared,{});});
    assert(invalid.phase()==GameplayPhase::level2LoaderReturn&&invalid.level1State().entities().size()==8&&invalid.level2Rng().calls()==100);
    auto upper=GameplayModelTestAccess::returned();upper.advanceRecoveredLevel1SuccessTail(assets,pages,guards,0);upper.beginRecoveredLevel2Load();
    GameplayModelTestAccess::materializeUpperFixed(upper);rejects([&] {upper.resetRecoveredLevel2Pools(shared,{});});
    assert(upper.level1State().entities().size()==12&&upper.level2Rng().calls()==100);
    std::cout<<"retained Level1-to-Level2 model ownership checks passed\n";
}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
