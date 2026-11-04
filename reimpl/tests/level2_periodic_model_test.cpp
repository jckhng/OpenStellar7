#include "stellar7/gameplay.hpp"
#include "stellar7/level2_periodic_tail_work.hpp"
#include "stellar7/campaign_loader.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <cstdio>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void level2(Level1State& state) {state.levelIndex_=1;state.entities_.clear();}
};
struct GameplayModelTestAccess {
    //Controlled model/physical-alias seam. No native clock or campaign ancestry.
    static GameplayModel fixture(const bool oldContainsRetired=true) {
        GameplayModel m;
        Level1ReloadTestAccess::level2(m.level1_);
        m.phase_=GameplayPhase::level2;m.activeCampaignLevel_=1;m.level2LoopTick_=16;
        m.level2HasAdvanced_=true;m.level2TailConsumed_=false;m.level2Complete_=false;m.level2PlayerArmor_=680;
        m.level2PrefixGuards_=GameplayModel::StationaryPrefixGuards{0,false,{false,false}};
        m.level2Player_={};m.level2Player_.position={100,-200,0};
        m.level2Player_.yawVelocity=80;m.level2ActiveEnemyCount_=2;m.level2SpawnedEnemyCount_=7;
        m.level2Entities_.clear();m.level2AllocationListOrder_.clear();m.level2ActiveListOrder_.clear();
        m.level2EntityActorPoolSlots_.clear();m.level2EntityFixedPoolSlots_.clear();
        m.level2EntityProjectilePoolSlots_.clear();m.level2EntityDeathEffectPoolSlots_.clear();
        m.level2ActorPoolActive_.fill(false);m.level2ActorIncarnations_.fill(9);
        for(auto& actor:m.level2ActorPool_) {
            actor={};actor.stateBytes4dTo52[0]=255;
            actor.animatedModelPresenterEnabled=false;actor.renderProfile=WorldActorRenderProfile::noHandles;
        }
        for(std::size_t index=0;index<9;++index) {
            Level1Entity actor{};actor.allocationIndex=index;actor.animatedModelPresenterEnabled=false;
            actor.renderProfile=WorldActorRenderProfile::noHandles;
            actor.position={static_cast<std::int32_t>(40000+index),-40000,0};
            actor.type=index<3?8:index<5?13:index<7?30:index==7?19:-1;
            actor.stateBytes4dTo52={static_cast<std::uint8_t>(index<5?255:index<7?254:index==7?253:252),
                static_cast<std::uint8_t>(index<3?0:1),0,0,0,0};
            actor.presenterDistance=1234;actor.targetHeading=5678;
            actor.ownerAllocationIndex=0;actor.collisionContactAllocationIndex=0;
            m.level2Entities_.push_back(actor);
            m.level2EntityActorPoolSlots_.push_back(index<5?index:m.level2ActorPoolSize_);
            m.level2EntityFixedPoolSlots_.push_back(index>=5 && index<7?index-5:m.level2FixedObjectPoolSize_);
            m.level2EntityProjectilePoolSlots_.push_back(index==7?0:m.level2ProjectilePoolSize_);
            m.level2EntityDeathEffectPoolSlots_.push_back(index==8?0:m.level2DeathEffectPoolSize_);
            if(index<5) {m.level2ActorPool_[index]=actor;m.level2ActorPoolActive_[index]=actor.active();}
            if(index>=3) {m.level2AllocationListOrder_.push_back(index);}
            if(index>=3 || (oldContainsRetired && index==0)) {m.level2ActiveListOrder_.push_back(index);}
        }
        m.level2FixedPool_[0]=m.level2Entities_[5];m.level2FixedPool_[1]=m.level2Entities_[6];
        m.level2ProjectilePool_[0]=m.level2Entities_[7];m.level2ProjectilePoolActive_[0]=true;
        m.level2DeathEffectPool_[0]=m.level2Entities_[8];m.level2DeathEffectPoolActive_[0]=true;
        m.level2ObjectListOrder_=m.level2AllocationListOrder_;
        m.level2AllocationRevision_=11;m.level2NonProjectileAllocationRevision_=7;
        m.level2Visibility_.resetLevel();m.level2AudioEvents_.clear();
        std::array<std::uint16_t,StellarRng::wordCount> table{};
        table[55]=0x0105;table[54]=0x0a07;table[53]=0x0311;table[52]=0x4560;
        m.level2Rng_=StellarRng(table,48,110,0,17);
        return m;
    }
    static FrameTailState state(const GameplayModel& m) {
        FrameTailState result{m.level2LoopTick_,m.level2Player_.position,m.level2Player_.yawVelocity,0,{},{}};
        for(const auto index:m.level2ActiveListOrder_) {result.actors.push_back({m.level2Entities_[index].type,m.level2Entities_[index].position});}
        return result;
    }
    static void reordered(GameplayModel& m) {
        const auto remap=[](std::size_t value) {return value==0?std::size_t{7}:value==7?std::size_t{0}:value;};
        std::swap(m.level2Entities_[0],m.level2Entities_[7]);
        for(auto* mapping:{&m.level2EntityActorPoolSlots_,&m.level2EntityFixedPoolSlots_,
                &m.level2EntityProjectilePoolSlots_,&m.level2EntityDeathEffectPoolSlots_}) {std::swap((*mapping)[0],(*mapping)[7]);}
        for(auto* order:{&m.level2AllocationListOrder_,&m.level2ObjectListOrder_,&m.level2ActiveListOrder_}) {
            for(auto& index:*order) {index=remap(index);}
        }
        for(std::size_t index=0;index<m.level2Entities_.size();++index) {
            auto& actor=m.level2Entities_[index];actor.allocationIndex=index;
            actor.ownerAllocationIndex=remap(actor.ownerAllocationIndex);
            actor.collisionContactAllocationIndex=remap(actor.collisionContactAllocationIndex);
        }
        for(auto* pool:{&m.level2ActorPool_,&m.level2FixedPool_}) {
            for(auto& actor:*pool) {
                actor.allocationIndex=remap(actor.allocationIndex);actor.ownerAllocationIndex=remap(actor.ownerAllocationIndex);
                actor.collisionContactAllocationIndex=remap(actor.collisionContactAllocationIndex);
            }
        }
        for(auto* pool:{&m.level2ProjectilePool_,&m.level2DeathEffectPool_}) {
            for(auto& actor:*pool) {
                actor.allocationIndex=remap(actor.allocationIndex);actor.ownerAllocationIndex=remap(actor.ownerAllocationIndex);
                actor.collisionContactAllocationIndex=remap(actor.collisionContactAllocationIndex);
            }
        }
    }
    static void neverMapped(GameplayModel& m) {
        //A separate controlled physical-projection shape, not the initial-seven constructor route.
        for(std::size_t index=0;index<3;++index) {
            m.level2EntityActorPoolSlots_[index]=m.level2ActorPoolSize_;
            m.level2EntityFixedPoolSlots_[index]=index+2;
            auto& actor=m.level2Entities_[index];actor.type=30;actor.stateBytes4dTo52[0]=254;
            m.level2FixedPool_[index+2]=actor;
            m.level2ActorPool_[index]={};m.level2ActorPool_[index].stateBytes4dTo52[0]=255;
            m.level2ActorPool_[index].animatedModelPresenterEnabled=false;
            m.level2ActorPool_[index].renderProfile=WorldActorRenderProfile::noHandles;
        }
        m.level2Entities_[7].ownerAllocationIndex=3;
        m.level2ProjectilePool_[0]=m.level2Entities_[7];
    }
    static void check(const GameplayModel& m,const Level2PeriodicSpawnReceipt& birth,
        const std::vector<std::size_t>& old,const std::size_t count) {
        assert(m.level2Entities_.size()==count && m.level2ActiveListOrder_==old);
        assert(m.level2ActorPool_[birth.physicalSlot].position==birth.actor.position);
        assert(m.level2Entities_[birth.logicalIndex].position==birth.actor.position);
        assert(m.level2Entities_[birth.logicalIndex].presenterDistance==0 && m.level2Entities_[birth.logicalIndex].targetHeading==0);
        assert(m.level2ActorIncarnations_[birth.physicalSlot]==10);
        assert(m.level2ActiveEnemyCount_==3 && m.level2SpawnedEnemyCount_==8);
        assert(m.level2AllocationRevision_==12 && m.level2NonProjectileAllocationRevision_==8);
        assert(m.level2AllocationListOrder_.back()==birth.logicalIndex && m.level2Visibility_.refreshRequested());
        assert(m.level2LoopTick_==17 && m.level2TailConsumed_);
        const auto projectile=std::find(m.level2EntityProjectilePoolSlots_.begin(),m.level2EntityProjectilePoolSlots_.end(),0);
        assert(projectile!=m.level2EntityProjectilePoolSlots_.end());
        const auto index=static_cast<std::size_t>(projectile-m.level2EntityProjectilePoolSlots_.begin());
        assert(m.level2Entities_[index].type==19 && m.level2Entities_[index].ownerAllocationIndex==birth.logicalIndex);
    }
    static GameplayModel thirdLiftFixture() {
        auto m=fixture(false);m.level2SpawnedEnemyCount_=10;
        for(const auto index:{3U,4U}) {
            auto& actor=m.level2Entities_[index];actor.collisionRadius=559;
            actor.stateBytes4dTo52={255,1,0,0,4,0};
            m.level2ActorPool_[index]=actor;
        }
        std::array<std::uint16_t,StellarRng::wordCount> table{};
        table[55]=0xcd05;table[54]=0x0a07;table[53]=0x0311;table[52]=0x4560;
        m.level2Rng_=StellarRng(table,48,110,0,17);
        return m;
    }
    static void checkThirdLift(const GameplayModel& m,const Level2PeriodicSpawnReceipt& birth,
        const std::vector<std::size_t>& old) {
        assert(birth.physicalSlot==0 && birth.logicalIndex==0);
        assert(m.level2ActiveListOrder_==old && m.level2ActiveEnemyCount_==3 && m.level2SpawnedEnemyCount_==11);
        assert(m.level2ActorIncarnations_[0]==10 && m.level2AllocationRevision_==12);
        assert(m.level2Entities_[0].type==13 && m.level2ActorPool_[0].type==13);
        assert(m.level2Entities_[0].stateBytes4dTo52[1]==1 && m.level2Entities_[0].collisionRadius==559);
        assert(m.level2AllocationListOrder_.back()==0 && m.level2Entities_[7].ownerAllocationIndex==0);
    }
    static void beginThirdLiftAdjacentTail(GameplayModel& m) {
        //Controlled late-3768 seam for the model/tail contract; no renderer or AI claim.
        m.level2ActiveListOrder_=m.level2AllocationListOrder_;m.level2TailConsumed_=false;
    }
    static void nextPeriodicBoundary(GameplayModel& m) {
        m.level2LoopTick_=32;m.level2TailConsumed_=false;m.level2Player_.position={100,-200,0};
    }
    static void spawnedLimit(GameplayModel& m) {m.level2SpawnedEnemyCount_=12;}
    static void spawned(GameplayModel& m,const std::int16_t total) {m.level2SpawnedEnemyCount_=total;}
    static void checkArrival(const GameplayModel& m,const Level1PeriodicSpawnReceipt& birth,
        const std::vector<std::size_t>& old) {
        assert(m.level2CompletionObjectIndex_==birth.actor.allocationIndex);
        assert(m.level2PostWaveObjectIndex_==static_cast<std::size_t>(-1));
        assert(m.level2ActiveListOrder_==old && m.level2AllocationListOrder_.back()==birth.actor.allocationIndex);
        assert(m.level2SpawnedEnemyCount_==13 && m.level2ActiveEnemyCount_==birth.activeEnemies);
        assert(m.level2ActorPool_[birth.poolSlot].type==7 && m.level2ActorPoolActive_[birth.poolSlot]);
        assert(m.level2ActorPool_[birth.poolSlot].position==birth.actor.position);
        assert(m.level2AudioEvents_.size()==1 && m.level2AudioEvents_.front().eventId==17);
    }
    static void sameModel(const GameplayModel& a,const GameplayModel& b) {
        assert(a.phase_==b.phase_ && a.level2LoopTick_==b.level2LoopTick_ && a.level2TailConsumed_==b.level2TailConsumed_);
        assert(a.level2Player_.position==b.level2Player_.position && a.level2Player_.yawVelocity==b.level2Player_.yawVelocity);
        assert(a.level2ActiveEnemyCount_==b.level2ActiveEnemyCount_ && a.level2SpawnedEnemyCount_==b.level2SpawnedEnemyCount_);
        assert(a.level2AllocationListOrder_==b.level2AllocationListOrder_ && a.level2ObjectListOrder_==b.level2ObjectListOrder_);
        assert(a.level2ActiveListOrder_==b.level2ActiveListOrder_ && a.level2ActorIncarnations_==b.level2ActorIncarnations_);
        assert(a.level2AllocationRevision_==b.level2AllocationRevision_ && a.level2NonProjectileAllocationRevision_==b.level2NonProjectileAllocationRevision_);
        assert(a.level2ActorPoolActive_==b.level2ActorPoolActive_ && a.level2AudioEvents_.size()==b.level2AudioEvents_.size());
        assert(a.level2EntityActorPoolSlots_==b.level2EntityActorPoolSlots_ && a.level2EntityFixedPoolSlots_==b.level2EntityFixedPoolSlots_);
        assert(a.level2EntityProjectilePoolSlots_==b.level2EntityProjectilePoolSlots_ && a.level2EntityDeathEffectPoolSlots_==b.level2EntityDeathEffectPoolSlots_);
        const auto actors=[](const auto& x,const auto& y) {
            assert(x.size()==y.size());
            for(std::size_t index=0;index<x.size();++index) {
                assert(x[index].type==y[index].type && x[index].position==y[index].position);
                assert(x[index].stateBytes4dTo52==y[index].stateBytes4dTo52 && x[index].ownerAllocationIndex==y[index].ownerAllocationIndex);
            }
        };
        actors(a.level2Entities_,b.level2Entities_);actors(a.level2ActorPool_,b.level2ActorPool_);
        actors(a.level2FixedPool_,b.level2FixedPool_);actors(a.level2ProjectilePool_,b.level2ProjectilePool_);
        actors(a.level2DeathEffectPool_,b.level2DeathEffectPool_);
    }
    static void mismatchedActiveCount(GameplayModel& m) {--m.level2ActiveEnemyCount_;}
    static void checkOrdinary(const GameplayModel& before,const GameplayModel& after) {
        assert(after.level2LoopTick_==static_cast<std::uint16_t>(before.level2LoopTick_+1) && after.level2TailConsumed_);
        assert(after.level2AllocationListOrder_==before.level2AllocationListOrder_);
        assert(after.level2ActiveListOrder_==before.level2ActiveListOrder_);
        assert(after.level2ActorIncarnations_==before.level2ActorIncarnations_);
        assert(after.level2AllocationRevision_==before.level2AllocationRevision_);
        assert(after.level2NonProjectileAllocationRevision_==before.level2NonProjectileAllocationRevision_);
        assert(after.level2ActiveEnemyCount_==before.level2ActiveEnemyCount_);
        assert(after.level2SpawnedEnemyCount_==before.level2SpawnedEnemyCount_);
        assert(after.level2Entities_.size()==before.level2Entities_.size());
    }
    static void prefixBoundary(GameplayModel& m) {m.level2TailConsumed_=false;}
    static void zeroThreshold(GameplayModel& m) {
        m.level2SpawnedEnemyCount_=5;
        m.level2Entities_[2].stateBytes4dTo52[1]=1;m.level2ActorPool_[2]=m.level2Entities_[2];m.level2ActorPoolActive_[2]=true;
        m.level2AllocationListOrder_.push_back(2);m.level2ObjectListOrder_=m.level2AllocationListOrder_;m.level2ActiveEnemyCount_=3;
    }
};
} //namespace stellar7

namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected=false;try {action();} catch(const std::invalid_argument&) {rejected=true;} assert(rejected);
}
FrameTailGuards guards(const GameplayModel& m,const std::int16_t active=2,const std::int16_t spawned=7) {
    FrameTailGuards result{};result.armor=680;result.missionActive=true;result.keyboardHead=result.keyboardTail=0x1e;
    result.periodic=FrameTailPeriodicInput{1,active,spawned,12,campaignSpawnThreshold(1,static_cast<std::uint8_t>(active)),
        m.level2Rng().indexA(),m.level2Rng().indexB(),0};
    return result;
}
std::vector<Model3d> models() {
    std::vector<Model3d> result(44);
    for(const auto type:{1,4,8,13}) {result[type].scaleShift=0;result[type].collisionRadius=type==1?1342:type==4?928:type==8?860:750;}
    result[7].collisionRadius=13052;result[7].flags=1;result[7].animationLimits={4};
    return result;
}
Level2PeriodicFrameTailWork plan(const GameplayModel& model,const std::vector<Model3d>& bank,const FrameTailGuards& g) {
    return level2PeriodicFrameTailOperations(GameplayModelTestAccess::state(model),g,[&](const FrameTailState& recentered) {
        return level2PeriodicSpawnOperations(model.recoveredLevel2PeriodicInput(recentered),model.level2Rng(),bank);
    });
}
void sameRng(StellarRng a,StellarRng b) {
    assert(a.calls()==b.calls() && a.indexA()==b.indexA() && a.indexB()==b.indexB() && a.last()==b.last());
    for(std::size_t i=0;i<2*StellarRng::wordCount;++i) {assert(a.next()==b.next());}
}
std::vector<NormalWorkOperation> normalized(const std::vector<NormalWorkOperation>& source) {
    std::vector<NormalWorkOperation> result;
    for(const auto operation:source) {
        if(!result.empty() && operation.kind==NormalWorkKind::instructions && result.back().kind==operation.kind) {result.back().count+=operation.count;}
        else {result.push_back(operation);}
    }
    return result;
}
void thirdLiftBirthAndAdjacentTail() {
    auto model=GameplayModelTestAccess::thirdLiftFixture();auto bank=models();bank[13].collisionRadius=559;
    const auto old=model.level2ActiveListOrder();const auto incomingRng=model.level2Rng();
    const auto birth=plan(model,bank,guards(model,2,10));
    assert(birth.periodic && birth.periodic->selectedType==13 && birth.periodic->effectiveType==13 && birth.periodic->spawn);
    assert(birth.periodic->rng.calls()==incomingRng.calls()+4);
    model.commitRecoveredLevel2PeriodicTail(birth.lifecycle,*birth.periodic,bank);
    GameplayModelTestAccess::checkThirdLift(model,*birth.periodic->spawn,old);
    sameRng(model.level2Rng(),birth.periodic->rng);
    GameplayModelTestAccess::beginThirdLiftAdjacentTail(model);
    const auto before=model;std::size_t calls=0;
    const auto adjacent=level2PeriodicFrameTailOperations(GameplayModelTestAccess::state(model),guards(model,3,11),
        [&](const FrameTailState&) {++calls;return *birth.periodic;});
    assert(!adjacent.periodic && calls==0);
    model.commitRecoveredLevel2OrdinaryTail(adjacent.lifecycle);
    GameplayModelTestAccess::checkOrdinary(before,model);sameRng(before.level2Rng(),model.level2Rng());
}
void cachedArrivalUnfinished() {
    const auto bank=models();
    const auto checkExit=[&](GameplayModel model,const std::int16_t active,const std::int16_t total,
        const Level1PeriodicSpawnOutcome outcome) {
        const auto before=model;const auto clearGuards=guards(model,active,total);
        const auto clear=plan(model,bank,clearGuards);
        auto cachedGuards=clearGuards;cachedGuards.periodic->enemyFlag=1;
        const auto unfinished=plan(model,bank,cachedGuards);
        const auto birth=outcome==Level1PeriodicSpawnOutcome::spawned;
        assert(clear.periodic && unfinished.periodic && clear.periodic->spawn.has_value()==birth);
        assert(clear.periodic->outcome==outcome && unfinished.periodic->outcome==outcome);
        assert(unfinished.periodic->spawn.has_value()==birth && unfinished.periodicOperationOffset==clear.periodicOperationOffset);
        auto expected=clear.lifecycle.work.operations;
        const auto exit=*clear.periodicOperationOffset+clear.periodic->operations.size();
        assert(expected.at(exit).kind==NormalWorkKind::instructions && expected[exit].count==1+13);
        expected[exit].count+=20-13;
        const auto& actual=unfinished.lifecycle.work.operations;assert(expected.size()==actual.size());
        for(std::size_t index=0;index<expected.size();++index) {
            assert(expected[index].kind==actual[index].kind && expected[index].count==actual[index].count && expected[index].value==actual[index].value);
        }
        sameRng(clear.periodic->rng,unfinished.periodic->rng);
        assert(unfinished.periodic->rng.calls()==before.level2Rng().calls()+(birth?3:1));
        if(birth) {
            assert(unfinished.periodic->spawn->spawnedEnemies==total+1 && unfinished.periodic->spawn->activeEnemies==active+1);
            assert(unfinished.periodic->spawn->actor.type==1 && unfinished.periodic->spawn->physicalSlot==0);
        }
        auto control=model;control.commitRecoveredLevel2PeriodicTail(clear.lifecycle,*clear.periodic,bank);
        model.commitRecoveredLevel2PeriodicTail(unfinished.lifecycle,*unfinished.periodic,bank);
        GameplayModelTestAccess::sameModel(control,model);sameRng(control.level2Rng(),model.level2Rng());
        assert(model.level2AudioEvents().size()==before.level2AudioEvents().size());
    };
    auto noBirth=GameplayModelTestAccess::fixture(false);GameplayModelTestAccess::zeroThreshold(noBirth);
    checkExit(noBirth,3,5,Level1PeriodicSpawnOutcome::threshold);
    auto defaultGuards=guards(noBirth,3,5);defaultGuards.periodic->enemyFlag=1;
    rejects([&] {level2LifecycleFrameTailOperations(GameplayModelTestAccess::state(noBirth),defaultGuards);});
    auto born=GameplayModelTestAccess::fixture(false);GameplayModelTestAccess::spawned(born,10);
    checkExit(born,2,10,Level1PeriodicSpawnOutcome::spawned);
    for(const auto total:{11,12}) {
        auto boundary=GameplayModelTestAccess::fixture(false);GameplayModelTestAccess::spawned(boundary,static_cast<std::int16_t>(total));
        const auto before=boundary;auto g=guards(boundary,2,static_cast<std::int16_t>(total));
        const auto clear=plan(boundary,bank,g);
        assert(clear.periodic && clear.periodic->spawn.has_value()==(total==11));
        if(clear.periodic->spawn) {assert(clear.periodic->spawn->spawnedEnemies==12);}
        g.periodic->enemyFlag=1;
        rejects([&] {
            const auto result=plan(boundary,bank,g);
            boundary.commitRecoveredLevel2PeriodicTail(result.lifecycle,*result.periodic,bank);
        });
        GameplayModelTestAccess::sameModel(before,boundary);sameRng(before.level2Rng(),boundary.level2Rng());
    }
    for(const auto invalid:{2,-1}) {
        auto g=guards(noBirth,3,5);
        if(invalid<0) {g.periodic->enemyFlag.reset();} else {g.periodic->enemyFlag=static_cast<std::uint8_t>(invalid);}
        rejects([&] {plan(noBirth,bank,g);});
    }
}
void joinedArrival() {
    const auto bank=models();
    for(const auto total:{11,12}) {
        for(const auto reorder:{false,true}) {
            auto model=GameplayModelTestAccess::fixture(true);
            GameplayModelTestAccess::spawned(model,static_cast<std::int16_t>(total));
            if(reorder) {GameplayModelTestAccess::reordered(model);}
            const auto before=model;const auto old=model.level2ActiveListOrder();
            auto g=guards(model,2,static_cast<std::int16_t>(total));g.periodic->enemyFlag=1;
            const auto result=level2PeriodicFrameTailOperations(GameplayModelTestAccess::state(model),g,
                [&](const FrameTailState& state) {
                    return level2PeriodicSpawnOperations(model.recoveredLevel2PeriodicInput(state),model.level2Rng(),bank);
                },{},{},[&](const FrameTailState& state,const Level2PeriodicSpawnWork& periodic,std::uint8_t flag) {
                    return level2ArrivalOperations(model.recoveredLevel2ArrivalInput(state,periodic,flag),periodic.rng,bank);
                });
            assert(result.periodic && result.arrival && result.arrival->birth && result.arrivalCacheReset);
            assert(result.arrival->birth->poolSlot==(total==11?1:0));
            assert(result.arrival->rng.calls()==before.level2Rng().calls()+(total==11?5:2));
            assert(result.lifecycle.audioEvents.size()==1 && result.lifecycle.audioEvents.front().eventId==17);
            assert(result.lifecycle.audioEvents.front().loopTick==16);
            const auto& operations=result.lifecycle.work.operations;
            const auto reset=result.arrivalCacheReset->operationOffset;
            assert(reset>0 && reset<operations.size() && operations[reset-1].kind==NormalWorkKind::instructions
                && operations[reset-1].count==4 && result.arrivalCacheReset->physicalKey==result.arrival->birth->poolSlot);
            assert(std::count_if(operations.begin(),operations.end(),[](const auto& op) {
                return op.kind==NormalWorkKind::gameplayLevel2ArrivalEvent;
            })==1);
            auto invalid=*result.arrival;invalid.rng.next();
            rejects([&] {model.commitRecoveredLevel2PeriodicTail(result.lifecycle,*result.periodic,bank,&invalid,1);});
            GameplayModelTestAccess::sameModel(model,before);sameRng(model.level2Rng(),before.level2Rng());
            auto missing=result.lifecycle;missing.audioEvents.clear();
            rejects([&] {model.commitRecoveredLevel2PeriodicTail(missing,*result.periodic,bank,&*result.arrival,1);});
            GameplayModelTestAccess::sameModel(model,before);
            model.commitRecoveredLevel2PeriodicTail(result.lifecycle,*result.periodic,bank,&*result.arrival,1);
            GameplayModelTestAccess::checkArrival(model,*result.arrival->birth,old);
            sameRng(model.level2Rng(),result.arrival->rng);
            //Retain the craft and its physical alias into the next ordinary tail.
            GameplayModelTestAccess::beginThirdLiftAdjacentTail(model);
            const auto adjacent=level2PeriodicFrameTailOperations(GameplayModelTestAccess::state(model),guards(model,result.arrival->birth->activeEnemies,13),
                [&](const FrameTailState&) {return *result.periodic;});
            model.commitRecoveredLevel2OrdinaryTail(adjacent.lifecycle);
            sameRng(model.level2Rng(),result.arrival->rng);
            GameplayModelTestAccess::nextPeriodicBoundary(model);
            auto retainedGuards=guards(model,result.arrival->birth->activeEnemies,13);retainedGuards.periodic->enemyFlag=1;
            const auto retained=level2PeriodicFrameTailOperations(GameplayModelTestAccess::state(model),retainedGuards,
                [&](const FrameTailState& state) {
                    return level2PeriodicSpawnOperations(model.recoveredLevel2PeriodicInput(state),model.level2Rng(),bank);
                },{},{},[&](const FrameTailState& state,const Level2PeriodicSpawnWork& periodic,std::uint8_t flag) {
                    return level2ArrivalOperations(model.recoveredLevel2ArrivalInput(state,periodic,flag),periodic.rng,bank);
                });
            assert(retained.arrival && retained.arrival->outcome==Level1ArrivalOutcome::craftPresent);
            assert(!retained.arrival->birth && !retained.arrivalCacheReset && retained.lifecycle.audioEvents.empty());
            model.commitRecoveredLevel2PeriodicTail(retained.lifecycle,*retained.periodic,bank,&*retained.arrival,1);
            sameRng(model.level2Rng(),result.arrival->rng);
        }
    }
}
} //namespace

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    try {
    using namespace stellar7;
    thirdLiftBirthAndAdjacentTail();
    cachedArrivalUnfinished();
    joinedArrival();
    const auto bank=models();
    for(const auto oldPresent:{false,true}) {
        for(const auto reorder:{false,true}) {
            auto model=GameplayModelTestAccess::fixture(oldPresent);
            if(reorder) {GameplayModelTestAccess::reordered(model);}
            const auto old=model.level2ActiveListOrder();const auto size=model.level2Entities().size();
            const auto result=plan(model,bank,guards(model));
            assert(result.periodic && result.periodic->spawn && result.periodic->selectedType==1 && result.periodic->effectiveType==4);
            const auto birth=*result.periodic->spawn;
            assert(birth.logicalIndex==(reorder?7U:0U) && birth.physicalSlot==0);
            assert(result.lifecycle.work.state.scheduler==FrameTailScheduler::none);
            auto invalid=*result.periodic;++invalid.operations.front().count;
            rejects([&] {model.commitRecoveredLevel2PeriodicTail(result.lifecycle,invalid,bank);});
            assert(model.level2LoopTick()==16 && model.level2Entities().size()==size && model.level2Rng().calls()==17);
            invalid=*result.periodic;invalid.rng.next();
            rejects([&] {model.commitRecoveredLevel2PeriodicTail(result.lifecycle,invalid,bank);});
            model.commitRecoveredLevel2PeriodicTail(result.lifecycle,*result.periodic,bank);
            GameplayModelTestAccess::check(model,birth,old,size);sameRng(model.level2Rng(),result.periodic->rng);
            rejects([&] {model.commitRecoveredLevel2PeriodicTail(result.lifecycle,*result.periodic,bank);});
            //Controlled next-prefix boundary; actor execution itself remains a native/model-bridge gate.
            GameplayModelTestAccess::prefixBoundary(model);
            auto plannerCalls=0;
            const auto next=level2PeriodicFrameTailOperations(GameplayModelTestAccess::state(model),guards(model,3,8),
                [&](const FrameTailState&) {++plannerCalls;return *result.periodic;});
            assert(!next.periodic && plannerCalls==0);
            rejects([&] {model.recoveredLevel2PeriodicInput(next.lifecycle.work.state);});
            rejects([&] {model.commitRecoveredLevel2PeriodicTail(next.lifecycle,*result.periodic,bank);});
            auto invalidNext=next.lifecycle;invalidNext.work.state.scheduler=FrameTailScheduler::zeroThresholdDraw;
            rejects([&] {model.commitRecoveredLevel2OrdinaryTail(invalidNext);});
            auto wrongCount=model;GameplayModelTestAccess::mismatchedActiveCount(wrongCount);
            rejects([&] {wrongCount.commitRecoveredLevel2OrdinaryTail(next.lifecycle);});
            sameRng(wrongCount.level2Rng(),model.level2Rng());
            const auto beforeNext=model;
            model.commitRecoveredLevel2OrdinaryTail(next.lifecycle);
            GameplayModelTestAccess::checkOrdinary(beforeNext,model);sameRng(model.level2Rng(),beforeNext.level2Rng());
            rejects([&] {model.commitRecoveredLevel2OrdinaryTail(next.lifecycle);});
        }
    }
    auto appended=GameplayModelTestAccess::fixture(false);GameplayModelTestAccess::neverMapped(appended);
    rejects([&] {appended.commitRecoveredLevel2OrdinaryTail(plan(appended,bank,guards(appended)).lifecycle);});
    const auto appendOld=appended.level2ActiveListOrder();const auto appendSize=appended.level2Entities().size();
    const auto appendPlan=plan(appended,bank,guards(appended));assert(appendPlan.periodic && appendPlan.periodic->spawn);
    assert(appendPlan.periodic->spawn->logicalIndex==appendSize);
    appended.commitRecoveredLevel2PeriodicTail(appendPlan.lifecycle,*appendPlan.periodic,bank);
    //Keep the surviving projectile's existing physical owner; appending FF0 does not rebind it.
    assert(appended.level2Entities().size()==appendSize+1 && appended.level2ActiveListOrder()==appendOld);
    assert(appended.recoveredLevel2RenderKeys().back()==0);
    assert(appended.level2Entities()[7].ownerAllocationIndex==3);
    auto limited=GameplayModelTestAccess::fixture(false);GameplayModelTestAccess::spawnedLimit(limited);
    const auto limit=plan(limited,bank,guards(limited,2,12));
    assert(limit.periodic && !limit.periodic->spawn && limit.periodic->outcome==Level1PeriodicSpawnOutcome::limit);
    const auto rng=limited.level2Rng();
    rejects([&] {limited.commitRecoveredLevel2PeriodicTail(limit.lifecycle,*limit.periodic,{});});
    limited.commitRecoveredLevel2PeriodicTail(limit.lifecycle,*limit.periodic,bank);sameRng(limited.level2Rng(),rng);
    auto mismatched=GameplayModelTestAccess::fixture(false);
    const auto mismatchPlan=plan(mismatched,bank,guards(mismatched));
    const auto mismatchRng=mismatched.level2Rng();
    const auto mismatchSize=mismatched.level2Entities().size();
    GameplayModelTestAccess::mismatchedActiveCount(mismatched);
    rejects([&] {mismatched.commitRecoveredLevel2PeriodicTail(mismatchPlan.lifecycle,*mismatchPlan.periodic,bank);});
    assert(mismatched.level2LoopTick()==16 && mismatched.level2Entities().size()==mismatchSize);
    sameRng(mismatched.level2Rng(),mismatchRng);
    auto ordinary=GameplayModelTestAccess::fixture(false);GameplayModelTestAccess::zeroThreshold(ordinary);
    const auto g=guards(ordinary,3,5);const auto state=GameplayModelTestAccess::state(ordinary);
    const auto old=level2LifecycleFrameTailOperations(state,g);const auto complete=plan(ordinary,bank,g);
    assert(complete.periodic && !complete.periodic->spawn);
    const auto a=normalized(old.work.operations),b=normalized(complete.lifecycle.work.operations);
    assert(a.size()==b.size());
    for(std::size_t i=0;i<a.size();++i) {assert(a[i].kind==b[i].kind && a[i].count==b[i].count && a[i].value==b[i].value);}
    auto oldModel=ordinary;oldModel.commitRecoveredLevel2LifecycleTail(old);
    ordinary.commitRecoveredLevel2PeriodicTail(complete.lifecycle,*complete.periodic,bank);
    sameRng(oldModel.level2Rng(),ordinary.level2Rng());
    rejects([&] {level2LifecycleFrameTailOperations(GameplayModelTestAccess::state(GameplayModelTestAccess::fixture()),guards(GameplayModelTestAccess::fixture()));});
    } catch(const std::exception& error) {
        std::fprintf(stderr,"%s\n",error.what());
        return 1;
    }
}
