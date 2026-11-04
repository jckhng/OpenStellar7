#include "stellar7/recovered_level2_model.hpp"
#include "stellar7/level2_preloop_prefix_work.hpp"
#include "stellar7/retry_work.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void level2Failure(Level1State& state) {
        state.entities_.clear();state.levelIndex_=1;state.loopTick_=583;state.score_=1350;
        state.missionActive_=true;state.successState_=false;state.failureState_=false;
        state.selectedSystem_=-1;state.systemStatusLevels_={3,2,1,3,2,1,3};
        state.systemTimers_={1,2,3,37,5,6,7};state.projectionListOrigin_={91,92,0};
        state.lastDestroyedType_=13;state.consecutiveDestroyedTypeCount_=2;
        std::array<std::uint16_t,56> words{};state.rng_=StellarRng(words,48,110,0,1);
    }
    static FixedPosition origin(const Level1State& state) {return state.projectionListOrigin_;}
    static void index(Level1State& state) {state.levelIndex_=0;}
    static void success(Level1State& state) {state.successState_=true;}
    static void selected(Level1State& state) {state.selectedSystem_=0;}
    static void broken(Level1State& state) {state.systemStatusLevels_[0]=0;}
    static void alias(Level1State& state) {state.entities_.push_back({});}
};
struct GameplayModelTestAccess {
    enum class Defect { tail, consumed, advanced, sourceLoop, control, entered, finished,
        display, debris, campaign, index, armor, completed, success, alias };
    static std::unique_ptr<GameplayModel> failure() {
        auto model=std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
        Level1ReloadTestAccess::level2Failure(model->level1_);
        model->phase_=GameplayPhase::failurePostExplosionBlackout;model->activeCampaignLevel_=1;
        model->failureFromLevel2_=true;model->level2FailureTailFinished_=true;
        model->level2HasAdvanced_=true;model->level2TailConsumed_=true;model->level2Complete_=false;
        model->level2LoopTick_=583;model->failureSourceLoopTick_=628;model->ticks_=737;
        model->level2CRandState_=0x12345678;model->level2VisualRandomState_=0x87654321;model->level2ArmorWarningActive_=true;
        model->remainingContinues_=2;model->level2Score_=1350;model->level2PlayerArmor_=-64;
        model->level2Player_.position={30,40,0};model->level2Player_.weaponCooldown=8;
        model->level2Player_.weaponSlotTimers={31,31,0};
        model->level2PlayerCollisionMomentum_=-1;
        model->weaponIndicatorVisible_=true;model->weaponReadyIndicator_=false;
        model->cockpitIndicatorX_=140;model->cockpitIndicatorVelocity_=4;
        model->lastCockpitInitialization_.levelIndex=1;model->lastCockpitInitialization_.displaySteps=2;
        model->lastCockpitInitialization_.states[0]={132,4};
        model->lastCockpitInitialization_.states[1]={136,4};
        model->lastCockpitInitialization_.states[2]={140,4};
        model->lastCockpitInitialization_.states[3]={140,4};
        model->recoveredFailureControl_=FailureControlState{};
        model->recoveredFailureControl_->entered=true;model->recoveredFailureControl_->finished=true;
        model->recoveredFailureControl_->display=45;
        model->recoveredFailureControl_->lastListCamera={7777,-2222,123};
        model->failureDebris_.clear();model->failureAudioEvents_={{737,6}};
        std::array<std::uint16_t,56> words{};
        for(unsigned i=0;i<words.size();++i) {words[i]=static_cast<std::uint16_t>(i*1193U+83U);}
        model->failureRng_=StellarRng(words,48,110,117,500);model->level2Rng_=model->failureRng_;
        std::vector<Level1Entity> empty;
        model->level2Visibility_.refresh({-12,-34,0},empty,{},{});
        model->level2ActorIncarnations_[0]=19;model->level2AllocationRevision_=100;
        model->level2NonProjectileAllocationRevision_=70;model->level2FixedAllocationRevision_=30;
        for(auto& actor:model->level2ActorPool_) {actor.renderProfile=WorldActorRenderProfile::noHandles;}
        for(auto& actor:model->level2FixedPool_) {actor.renderProfile=WorldActorRenderProfile::noHandles;}
        for(auto& actor:model->level2DeathEffectPool_) {actor.renderProfile=WorldActorRenderProfile::noHandles;}
        for(std::size_t slot=0;slot<model->level2ProjectilePool_.size();++slot) {
            auto& shot=model->level2ProjectilePool_[slot];shot.type=slot==7?23:19;
            shot.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;shot.flags=4;
            shot.stateBytes4dTo52={253,static_cast<std::uint8_t>(slot&1 ? 0 : 42),2,7,1,0};
            shot.position={1234,-5678,0};shot.targetHeading=123;shot.presenterDistance=456;
        }
        model->level2ActorPool_[0].type=8;model->level2ActorPool_[0].stateBytes4dTo52={255,1,0,2,84,0};
        model->level2ActorPool_[1].type=13;model->level2ActorPool_[1].stateBytes4dTo52={255,0,0,0,0,0};
        auto& upper=model->level2FixedPool_[9];upper.type=42;upper.flags=4;upper.position={123,456,789};
        upper.stateBytes4dTo52={254,7,0,0,0,0};upper.renderProfile=WorldActorRenderProfile::heapSelectorNoCallback;
        model->level2FixedPoolKnown_[9]=true;model->level2FixedPoolEverUsed_[9]=true;
        auto& effect=model->level2DeathEffectPool_[0];effect.type=-1;effect.flags=0x8c;
        effect.position={77,88,0};effect.stateBytes4dTo52={252,20,0,0,24,0};
        return model;
    }
    static void defect(GameplayModel& model,Defect defect) {
        switch(defect) {
        case Defect::tail:model.level2FailureTailFinished_=false;break;
        case Defect::consumed:model.level2TailConsumed_=false;break;
        case Defect::advanced:model.level2HasAdvanced_=false;break;
        case Defect::sourceLoop:model.failureSourceLoopTick_.reset();break;
        case Defect::control:model.recoveredFailureControl_.reset();break;
        case Defect::entered:model.recoveredFailureControl_->entered=false;break;
        case Defect::finished:model.recoveredFailureControl_->finished=false;break;
        case Defect::display:model.recoveredFailureControl_->display=44;break;
        case Defect::debris:model.failureDebris_.push_back({});break;
        case Defect::campaign:model.activeCampaignLevel_=2;break;
        case Defect::index:Level1ReloadTestAccess::index(model.level1_);break;
        case Defect::armor:model.level2PlayerArmor_=0;break;
        case Defect::completed:model.level2Complete_=true;break;
        case Defect::success:Level1ReloadTestAccess::success(model.level1_);break;
        case Defect::alias:Level1ReloadTestAccess::alias(model.level1_);break;
        }
    }
    static void exhaust(GameplayModel& model) {model.remainingContinues_=0;}
    static void oneContinue(GameplayModel& model) {model.remainingContinues_=1;}
    static void staleRng(GameplayModel& model) {model.level2Rng_.next();}
    static void poisonSelector(GameplayModel& model) {model.level2ActorPool_[15].renderProfile=WorldActorRenderProfile::unknown;model.level2ActorPool_[15].type=8;}
    static void selected(GameplayModel& model) {Level1ReloadTestAccess::selected(model.level1_);}
    static void broken(GameplayModel& model) {Level1ReloadTestAccess::broken(model.level1_);}
    static void beforeReset(const GameplayModel& model) {
        assert(model.level2ProjectilePool_[0].type==19&&model.level2ProjectilePool_[0].stateBytes4dTo52[1]==42);
        assert(model.level2ProjectilePool_[1].type==19&&model.level2ProjectilePool_[1].stateBytes4dTo52[1]==0);
        assert(model.failureFromLevel2_&&model.level2FailureTailFinished_&&model.recoveredFailureControl_->finished);
        assert(model.level1_.loopTick()==628&&model.level2LoopTick_==628&&model.level1_.rng().calls()==1);
        assert((model.level1_.systemTimers()==std::array<std::int16_t,7>{1,2,3,37,5,6,7}));
        assert(model.lastCockpitInitialization_.displaySteps==0);
    }
    static void reset(const GameplayModel& model) {
        assert(model.level2FixedPool_[9].type==42&&model.level2FixedPool_[9].stateBytes4dTo52[1]==7);
        assert((model.level2FixedPool_[9].position==FixedPosition{123,456,789}));
        assert(model.level2FixedPoolKnown_[9]&&model.level2FixedPoolEverUsed_[9]);
        assert(model.level2FixedPool_[9].renderProfile==WorldActorRenderProfile::heapSelectorNoCallback);
        for(const auto& shot:model.level2ProjectilePool_) {
            assert(shot.type==0&&shot.stateBytes4dTo52[0]==253&&shot.stateBytes4dTo52[1]==0);
            assert(shot.targetHeading==0&&shot.presenterDistance==0&&shot.position==FixedPosition{});
        }
        assert(model.level2ActorIncarnations_[0]==19&&model.level2AllocationRevision_==100);
        assert(model.level2CRandState_==0x12345678&&model.level2VisualRandomState_==0x87654321&&model.level2ArmorWarningActive_);
        assert(!model.level2HasAdvanced_&&!model.level2TailConsumed_&&model.level2ActiveListOrder_.empty());
        assert(model.level2Visibility_.tracking()==VisibilityTracking::unknown);
        assert((*model.level2Visibility_.lastCamera()==FixedPosition{-12,-34,0}));
        assert((Level1ReloadTestAccess::origin(model.level1_)==FixedPosition{7777,-2222,123}));
        assert(model.failureFromLevel2_&&model.level2FailureTailFinished_&&model.failureSourceLoopTick_==628);
    }
    static void preloop(const GameplayModel& model) {
        assert(!model.failureFromLevel2_&&!model.level2FailureTailFinished_&&!model.failureSourceLoopTick_);
        assert(!model.recoveredFailureControl_&&model.failureAudioEvents_.empty());
        assert(!model.level2TailConsumed_&&!model.level2HasAdvanced_&&model.level2ActiveListOrder_.empty());
        assert(model.level2ActorIncarnations_[0]==20&&model.level2AllocationRevision_==107);
        assert(model.level2NonProjectileAllocationRevision_==77&&model.level2FixedAllocationRevision_==32);
        assert(model.level2Visibility_.tracking()==VisibilityTracking::sourceInitialized&&model.level2Visibility_.refreshRequested());
        assert(model.level1_.rng().calls()==1);
        assert(model.level2CRandState_==0x12345678&&model.level2VisualRandomState_==0x87654321&&model.level2ArmorWarningActive_);
    }
};
}

namespace {
using namespace stellar7;
template<class Action>void rejects(Action action) {
    bool rejected=false;try {action();}catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
}
void yes(GameplayModel& model) {
    model.enterRecoveredContinue();model.pollRecoveredContinue({});
    GameplayInput up{};up.forward=true;model.pollRecoveredContinue(up);
    GameplayInput enter{};enter.confirm=true;model.pollRecoveredContinue(enter);
    assert(model.pollRecoveredContinue({})==ContinueDecision::yes);
}
std::vector<Model3d> models() {
    std::vector<Model3d> shared(44);shared[0].flags=1;shared[0].collisionRadius=723;
    shared[8].flags=1;shared[8].collisionRadius=860;shared[13].collisionRadius=559;shared[30].collisionRadius=868;
    return shared;
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    try {
        using namespace stellar7;using Access=GameplayModelTestAccess;
        const auto shared=models();
        for(const auto defect:{Access::Defect::tail,Access::Defect::consumed,Access::Defect::advanced,
            Access::Defect::sourceLoop,Access::Defect::control,Access::Defect::entered,Access::Defect::finished,
            Access::Defect::display,Access::Defect::debris,Access::Defect::campaign,Access::Defect::index,
            Access::Defect::armor,Access::Defect::completed,Access::Defect::success,Access::Defect::alias}) {
            auto bad=Access::failure();Access::defect(*bad,defect);
            rejects([&] {bad->enterRecoveredContinue();});
            assert(bad->ticks()==737&&bad->remainingContinues()==2);
        }
        auto exhausted=Access::failure();Access::exhaust(*exhausted);
        rejects([&] {exhausted->enterRecoveredContinue();});
        auto wrong=Access::failure();rejects([&] {wrong->beginRecoveredLevel2Retry();});
        wrong->enterRecoveredContinue();rejects([&] {wrong->beginRecoveredLevel2Retry();});
        GameplayInput enter{};enter.confirm=true;wrong->pollRecoveredContinue(enter);wrong->pollRecoveredContinue({});
        assert(wrong->recoveredContinueDecision()==ContinueDecision::no);
        rejects([&] {wrong->beginRecoveredLevel2Retry();});
        auto cancelled=Access::failure();cancelled->enterRecoveredContinue();GameplayInput escape{};escape.exit=true;
        assert(cancelled->pollRecoveredContinue(escape)==ContinueDecision::cancelled);
        rejects([&] {cancelled->beginRecoveredLevel2Retry();});
        for(const auto defect:{Access::Defect::finished,Access::Defect::consumed,Access::Defect::display}) {
            auto bad=Access::failure();bad->enterRecoveredContinue();Access::defect(*bad,defect);
            rejects([&] {bad->pollRecoveredContinue({});});
        }
        auto stale=Access::failure();yes(*stale);Access::staleRng(*stale);
        rejects([&] {stale->beginRecoveredLevel2Retry();});assert(stale->score()==1350);
        auto selected=Access::failure();yes(*selected);Access::selected(*selected);
        rejects([&] {selected->beginRecoveredLevel2Retry();});
        auto broken=Access::failure();yes(*broken);Access::broken(*broken);
        rejects([&] {broken->beginRecoveredLevel2Retry();});
        auto noBudget=Access::failure();yes(*noBudget);Access::exhaust(*noBudget);
        rejects([&] {noBudget->beginRecoveredLevel2Retry();});assert(noBudget->score()==1350);
        auto poisoned=Access::failure();yes(*poisoned);poisoned->beginRecoveredLevel2Retry();Access::poisonSelector(*poisoned);
        rejects([&] {poisoned->resetRecoveredLevel2RetryPools(shared);});assert(poisoned->level2Rng().calls()==500);

        auto model=Access::failure();
        rejects([&] {model->resetRecoveredLevel2RetryPools(shared);});
        rejects([&] {model->prepareRecoveredLevel2RetryLoaderPages({82,{}});});
        yes(*model);assert(model->score()==1350&&model->remainingContinues()==2&&model->ticks()==737);
        rejects([&] {model->resumeRecoveredContinueRetry();}); //Old L1 entry stays strict.
        Level1RetryProfile profile{};profile.decision=ContinueOutcome::yes;profile.levelIndex=1;
        profile.score=model->score();profile.remainingContinues=2;profile.systemLevels=model->level1State().systemStatusLevels();
        profile.systemTimers=model->level1State().systemTimers();profile.frontPage=0xa000;profile.backPage=0xa800;profile.cachePage=0xa400;
        HighscoreTable::Entries table{};for(auto& row:table) {row.score=1800;}profile.currentHighscores=HighscoreTable({},table);
        const auto control=level2RankMissRetryOperations(profile);assert(model->score()==1350);
        model->beginRecoveredLevel2Retry();
        assert(model->score()==control.observation.score&&model->playerArmor()==680&&model->remainingContinues()==1);
        assert(model->ticks()==737&&model->campaignLevel()==1&&model->phase()==GameplayPhase::level2LoaderReturn);
        assert(model->cockpitIndicatorX()==140&&model->level2CockpitIndicatorVelocity()==4);
        assert(model->weaponIndicatorVisible()&&!model->weaponReadyIndicator());Access::beforeReset(*model);
        rejects([&] {model->beginRecoveredLevel2Retry();});
        rejects([&] {model->resetRecoveredLevel2Pools(shared);}); //No forged success ancestry.
        rejects([&] {model->constructRecoveredLevel2Actors(shared);});
        rejects([&] {model->prepareRecoveredLevel2RetryLoaderPages({82,{}});});
        rejects([&] {model->completeRecoveredLevel2MusicActivation();});
        auto badShared=shared;badShared.pop_back();rejects([&] {model->resetRecoveredLevel2RetryPools(badShared);});
        badShared=shared;badShared[0].scaleShift=1;rejects([&] {model->resetRecoveredLevel2RetryPools(badShared);});
        const auto reset=model->resetRecoveredLevel2RetryPools(shared);
        assert(reset.reset.selectorReleases.empty()&&reset.projectionResetKeys.size()==40);
        assert(reset.projectionResetKeys[23]==23&&reset.projectionResetKeys[24]==32&&reset.projectionResetKeys.back()==47);
        assert(model->level2Rng().calls()==500&&model->ticks()==737);Access::reset(*model);
        assert((model->level1State().systemTimers()==std::array<std::int16_t,7>{0,0,0,37,0,0,0}));
        assert((model->level1State().systemStatusLevels()==std::array<std::int8_t,7>{3,2,1,3,2,1,3}));
        assert(model->weaponIndicatorVisible()&&!model->weaponReadyIndicator());
        rejects([&] {model->resetRecoveredLevel2RetryPools(shared);});
        const auto constructors=model->constructRecoveredLevel2Actors(shared);
        assert(constructors.actors.size()==7&&model->level2Rng().calls()==512&&model->ticks()==737);
        rejects([&] {model->constructRecoveredLevel2Actors(shared);});
        rejects([&] {model->prepareRecoveredLevel2LoaderPages({82,{}});}); //Explicit retry origin only.
        rejects([&] {model->prepareRecoveredLevel2RetryLoaderPages({50,{}});});
        rejects([&] {model->completeRecoveredLevel2MusicActivation();});
        const auto pages=model->prepareRecoveredLevel2RetryLoaderPages({82,{}});
        assert(pages.cockpit.levelIndex==1&&pages.cockpit.displaySteps==2&&pages.armor==680);
        assert(pages.cockpit.states[0].x==140&&pages.cockpit.states[1].x==144&&pages.cockpit.states[2].x==148);
        assert(model->cockpitIndicatorX()==148&&model->level2LoopTick()==628&&model->ticks()==737);
        rejects([&] {model->prepareRecoveredLevel2RetryLoaderPages({82,{}});});
        model->completeRecoveredLevel2MusicActivation();
        assert(model->level2LoopTick()==0&&model->phase()==GameplayPhase::level2MusicReturn&&model->ticks()==737);
        rejects([&] {model->completeRecoveredLevel2MusicActivation();});
        const auto preloop=model->completeRecoveredLevel2Preloop();
        assert(model->phase()==GameplayPhase::level2PreloopReturn&&model->level2Rng().calls()==524&&model->ticks()==737);
        assert(preloop.firstEntryDisplay&&preloop.firstPrefix.activeOrder.empty()&&preloop.firstPrefix.refreshRequested);
        assert((preloop.firstPrefix.lastListCamera==FixedPosition{7777,-2222,123}));
        assert(preloop.allocationOrder==std::vector<std::size_t>({0,1,2,3,4,5,6}));Access::preloop(*model);
        const auto first=level2ResetPreloopPrefixOperations({preloop.firstPrefix,preloop.allocationOrder});
        assert(first.activeOrder.size()==7&&!first.refreshRequested&&first.lastListCamera==FixedPosition{});
        assert(model->level2Rng().calls()==524&&model->ticks()==737&&model->cockpitIndicatorX()==148);
        assert(model->weaponIndicatorVisible()&&!model->weaponReadyIndicator());
        assert(model->level1State().lastDestroyedType()==13&&model->level1State().consecutiveDestroyedTypeCount()==0);
        rejects([&] {model->completeRecoveredLevel2Preloop();});
        rejects([&] {model->prepareRecoveredLevel2RetryLoaderPages({82,{}});});
        rejects([&] {model->beginRecoveredLevel2Retry();});
        auto last=Access::failure();Access::oneContinue(*last);yes(*last);last->beginRecoveredLevel2Retry();
        assert(last->remainingContinues()==0&&last->campaignLevel()==1&&last->score()==0);
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
