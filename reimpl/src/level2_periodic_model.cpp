#include "stellar7/gameplay.hpp"
#include "stellar7/level2_periodic_spawn_work.hpp"
#include "stellar7/level2_projectile_prefix_work.hpp"
#include "stellar7/level1_arrival_work.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <tuple>

namespace stellar7 {
namespace {
constexpr std::size_t fixedKeyBase=16, projectileKeyBase=32, effectKeyBase=40;
constexpr std::size_t maximumLiveRams=maximumLevel2PeriodicLiveType13;
constexpr std::size_t sharedModelCount=44;
constexpr std::size_t allocationCapacity=32;
constexpr std::int16_t ordinarySpawnLimit=12;
constexpr std::int16_t postWaveSpawnLimit=14;
constexpr std::uint16_t periodicMask=15;

auto fields(const Level1Entity& a) {
    return std::tie(a.allocationIndex,a.flags,a.type,a.collisionRadius,a.position.x,a.position.y,a.position.z,
        a.pitch,a.roll,a.yaw,a.pitchVelocity,a.rollVelocity,a.yawVelocity,a.xVelocity,a.yVelocity,a.zVelocity,
        a.forwardVelocity,a.stateBytes4dTo52,a.modelAnimationVariant,a.primaryType11AnimationSelector,
        a.behaviorTimer,a.type12MissGate,a.presenterDistance,a.targetHeading,a.weaponSlotCount,a.weaponCooldown,
        a.weaponSlotTimers,a.collisionContactAllocationIndex,a.collisionContactIsPlayer,
        a.presentationModelAnimationVariant,a.animatedModelPresenterEnabled,a.ownerIsPlayer,a.ownerAllocationIndex,a.renderProfile);
}
bool sameRng(StellarRng a,StellarRng b) {
    if(a.calls()!=b.calls() || a.indexA()!=b.indexA() || a.indexB()!=b.indexB() || a.last()!=b.last()) {return false;}
    for(std::size_t index=0;index<2*StellarRng::wordCount;++index) {
        if(a.next()!=b.next()) {return false;}
    }
    return true;
}
bool same(const Level2PeriodicSpawnWork& a,const Level2PeriodicSpawnWork& b) {
    if(!sameRng(a.rng,b.rng) || a.outcome!=b.outcome || a.selectedType!=b.selectedType
        || a.effectiveType!=b.effectiveType || a.allocationOrder!=b.allocationOrder
        || a.spawn.has_value()!=b.spawn.has_value() || a.operations.size()!=b.operations.size()) {return false;}
    for(std::size_t index=0;index<a.operations.size();++index) {
        const auto& x=a.operations[index];const auto& y=b.operations[index];
        if(x.kind!=y.kind || x.count!=y.count || x.value!=y.value) {return false;}
    }
    if(!a.spawn) {return true;}
    const auto& x=*a.spawn;const auto& y=*b.spawn;
    return x.physicalSlot==y.physicalSlot && x.logicalIndex==y.logicalIndex && fields(x.actor)==fields(y.actor)
        && x.appendedAllocationIndex==y.appendedAllocationIndex && x.activeEnemies==y.activeEnemies
        && x.spawnedEnemies==y.spawnedEnemies && x.refreshRequested==y.refreshRequested
        && x.cacheReset.operationOffset==y.cacheReset.operationOffset
        && x.cacheReset.sourceResume==y.cacheReset.sourceResume && x.cacheReset.physicalKey==y.cacheReset.physicalKey;
}
bool sameArrival(const Level1ArrivalWork& a,const Level1ArrivalWork& b) {
    if(!sameRng(a.rng,b.rng) || a.outcome!=b.outcome || a.birth.has_value()!=b.birth.has_value()
        || a.steps.size()!=b.steps.size()) {return false;}
    if(a.birth) {
        const auto& x=*a.birth;const auto& y=*b.birth;
        if(x.poolSlot!=y.poolSlot || fields(x.actor)!=fields(y.actor) || x.appendedAllocationIndex!=y.appendedAllocationIndex
            || x.activeEnemies!=y.activeEnemies || x.spawnedEnemies!=y.spawnedEnemies || x.refreshRequested!=y.refreshRequested) {return false;}
    }
    for(std::size_t index=0;index<a.steps.size();++index) {
        const auto* x=std::get_if<std::vector<NormalWorkOperation>>(&a.steps[index]);
        const auto* y=std::get_if<std::vector<NormalWorkOperation>>(&b.steps[index]);
        if(x || y) {
            if(!x || !y || x->size()!=y->size()) {return false;}
            for(std::size_t n=0;n<x->size();++n) {
                if((*x)[n].kind!=(*y)[n].kind || (*x)[n].count!=(*y)[n].count || (*x)[n].value!=(*y)[n].value) {return false;}
            }
            continue;
        }
        const auto* audioA=std::get_if<Level1ArrivalAudioChild>(&a.steps[index]);
        const auto* audioB=std::get_if<Level1ArrivalAudioChild>(&b.steps[index]);
        if(!audioA || !audioB || audioA->eventId!=audioB->eventId || audioA->callerBegin!=audioB->callerBegin
            || audioA->callerEnd!=audioB->callerEnd || audioA->callerDispatches!=audioB->callerDispatches) {return false;}
    }
    return true;
}
std::int32_t subtract(const std::int32_t a,const std::int32_t b) {
    const auto bits=static_cast<std::uint32_t>(a)-static_cast<std::uint32_t>(b);
    return bits<=0x7fffffffU ? static_cast<std::int32_t>(bits)
        : static_cast<std::int32_t>(static_cast<std::int64_t>(bits)-0x100000000LL);
}
std::int32_t normalize(std::int32_t value) {
    if(value>0x8000) {return subtract(value,0x10000);}
    if(value<-0x8000) {return subtract(value,-0x10000);}
    return value;
}
} //namespace

Level2PeriodicSpawnInput GameplayModel::recoveredLevel2PeriodicInput(const FrameTailState& recentered) const {
    if((level2LoopTick_&periodicMask)!=0) {
        throw std::invalid_argument("Level2 periodic input requires its source scheduler boundary");
    }
    return level2OrdinaryTailInput(recentered);
}

Level2PeriodicSpawnInput GameplayModel::level2OrdinaryTailInput(const FrameTailState& recentered) const {
    validateLevel2CombatTail(recentered,Level1PlayerCombatDomain::expiryAndLoss);
    if(recentered.scheduler!=FrameTailScheduler::none
        || !(recentered.playerPosition==FixedPosition{0,0,level2Player_.position.z})) {
        throw std::invalid_argument("Level2 ordinary input requires its completed0C6A source boundary");
    }
    const auto keys=recoveredLevel2RenderKeys();
    Level2PeriodicSpawnInput result{};
    result.activeEnemies=level2ActiveEnemyCount_;result.spawnedEnemies=level2SpawnedEnemyCount_;
    result.nextEntityIndex=level2Entities_.size();
    for(std::size_t slot=0;slot<level2ActorPoolSize_;++slot) {result.ordinaryPool[slot].actor=level2ActorPool_[slot];}
    for(std::size_t index=0;index<keys.size();++index) {
        if(keys[index]>=fixedKeyBase) {continue;}
        auto& slot=result.ordinaryPool[keys[index]];
        slot.actor=level2Entities_[index];slot.entityIndex=index;
        if(slot.actor.active()!=level2ActorPoolActive_[keys[index]]) {
            throw std::invalid_argument("Level2 periodic physical liveness differs from its canonical row");
        }
    }
    std::vector<std::size_t> visited;
    for(std::size_t ordinal=0;ordinal<level2ActiveListOrder_.size();++ordinal) {
        const auto index=level2ActiveListOrder_[ordinal];
        if(std::find(visited.begin(),visited.end(),index)!=visited.end()) {
            throw std::invalid_argument("Level2 periodic OLD aliases the same physical actor twice");
        }
        visited.push_back(index);
        auto expected=level2Entities_.at(index).position;
        if(level2Entities_.at(index).type!=7) {
            expected.x=subtract(expected.x,level2Player_.position.x);
            expected.y=subtract(expected.y,level2Player_.position.y);
        }
        if((level2LoopTick_&periodicMask)==0 && level2Entities_.at(index).type!=7) {
            expected.x=normalize(expected.x);
            expected.y=normalize(expected.y);
        }
        if(!(expected==recentered.actors[ordinal].position)) {
            throw std::invalid_argument("Level2 periodic input changed source0C6A recenter writes");
        }
        if(keys[index]<fixedKeyBase) {result.ordinaryPool[keys[index]].actor.position=expected;}
    }
    const auto identity=[&](const std::size_t index) {
        const auto key=keys.at(index);
        return key<projectileKeyBase ? Level1ObjectRef{Level1PresentationKind::entity,index}
            : key<effectKeyBase ? Level1ObjectRef{Level1PresentationKind::projectile,key-projectileKeyBase}
                : Level1ObjectRef{Level1PresentationKind::deathEffect,key-effectKeyBase};
    };
    for(const auto index:level2AllocationListOrder_) {result.allocationOrder.push_back(identity(index));}
    if(result.activeEnemies<0 || result.activeEnemies>result.spawnedEnemies
        || result.spawnedEnemies<0 || result.spawnedEnemies>postWaveSpawnLimit || result.allocationOrder.size()>allocationCapacity) {
        throw std::invalid_argument("Level2 ordinary tail requires its bounded periodic roster");
    }
    std::vector<Level1ObjectRef> listed;
    for(const auto ref:result.allocationOrder) {
        if(std::find(listed.begin(),listed.end(),ref)!=listed.end()) {
            throw std::invalid_argument("Level2 ordinary tail repeats a physical allocation");
        }
        listed.push_back(ref);
    }
    for(const auto& slot:result.ordinaryPool) {
        const auto& actor=slot.actor;
        const auto postWave=actor.type==7 || actor.type==44;
        const auto ordinary=actor.type==1 || actor.type==4 || actor.type==8 || actor.type==13 || postWave;
        const auto allocated=slot.entityIndex && std::find(listed.begin(),listed.end(),
            Level1ObjectRef{Level1PresentationKind::entity,*slot.entityIndex})!=listed.end();
        if(actor.stateBytes4dTo52[0]!=255 || (postWave ? actor.renderProfile!=(actor.type==7
                ? WorldActorRenderProfile::inlineSelectorNoCallback : WorldActorRenderProfile::heapSelectorDeferredAnimation)
                    : actor.renderProfile!=WorldActorRenderProfile::noHandles)
            || (!postWave && (actor.animatedModelPresenterEnabled || actor.primaryType11AnimationSelector))
            || (slot.entityIndex ? !ordinary || actor.allocationIndex!=*slot.entityIndex
                : actor.type!=0 || actor.active()) || actor.active()!=allocated) {
            throw std::invalid_argument("Level2 ordinary tail changed its physical FF ancestry");
        }
    }
    const auto live=std::count_if(result.ordinaryPool.begin(),result.ordinaryPool.end(),[](const auto& slot) {
        return slot.actor.active();
    });
    if(live!=level2ActiveEnemyCount_) {
        throw std::invalid_argument("Level2 periodic counter differs from its live physical ordinary pool");
    }
    const auto rams=std::count_if(result.ordinaryPool.begin(),result.ordinaryPool.end(),[](const auto& slot) {
        return slot.actor.type==13 && slot.actor.active();
    });
    if(rams>maximumLiveRams) {
        throw std::invalid_argument("Level2 periodic input exceeds its qualified liveType13 damage bound");
    }
    return result;
}

Level1ArrivalInput GameplayModel::recoveredLevel2ArrivalInput(const FrameTailState& recentered,
    const Level2PeriodicSpawnWork& periodic,const std::uint8_t cachedEnemyFlag) const {
    const auto input=recoveredLevel2PeriodicInput(recentered);
    Level1ArrivalInput result{cachedEnemyFlag,{},{},level2Player_,{input.activeEnemies,input.spawnedEnemies,
        static_cast<std::uint8_t>(ordinarySpawnLimit),static_cast<std::uint16_t>(input.allocationOrder.size()),input.nextEntityIndex,{}}};
    result.player.position=recentered.playerPosition;
    if(level2CompletionObjectIndex_<level2Entities_.size()) {result.craftEntityIndex=level2CompletionObjectIndex_;}
    const auto guardian = postWaveObjectIndex();
    if (guardian < level2Entities_.size() || hasPhysicalPostWaveRole()) {
        result.guardianEntityIndex = guardian;
    }
    for(std::size_t slot=0;slot<input.ordinaryPool.size();++slot) {
        const auto& previous=input.ordinaryPool[slot];
        result.pool.ordinaryPool[slot]={previous.actor.stateBytes4dTo52[1],
            previous.actor.primaryType11AnimationSelector,previous.entityIndex};
    }
    if(periodic.spawn) {
        const auto& birth=*periodic.spawn;
        result.pool.activeEnemies=birth.activeEnemies;result.pool.spawnedEnemies=birth.spawnedEnemies;
        result.pool.allocationCount=static_cast<std::uint16_t>(birth.appendedAllocationIndex+1);
        result.pool.ordinaryPool.at(birth.physicalSlot)={birth.actor.stateBytes4dTo52[1],false,birth.logicalIndex};
        if(birth.logicalIndex==result.pool.nextEntityIndex) {++result.pool.nextEntityIndex;}
    }
    return result;
}

void GameplayModel::commitRecoveredLevel2OrdinaryTail(const Level2LifecycleTailWork& tail) {
    if((level2LoopTick_&periodicMask)==0) {
        throw std::invalid_argument("Level2 ordinary tail cannot omit a periodic source child");
    }
    validateLevel2LifecycleTail(tail);
    level2OrdinaryTailInput(tail.work.state);
    applyLevel2CombatTailState(tail.work.state);
    level2LoopTick_=tail.work.state.loopTick;level2TailConsumed_=true;
    finishLevel2LifecycleTail(tail);
}

void GameplayModel::commitRecoveredLevel2PeriodicTail(const Level2LifecycleTailWork& tail,
    const Level2PeriodicSpawnWork& periodic,const std::vector<Model3d>& ownedShared,
    const Level1ArrivalWork* arrival,const std::uint8_t cachedEnemyFlag) {
    if(ownedShared.size()!=sharedModelCount) {
        throw std::invalid_argument("Level2 periodic tail requires its owned44 shared model bank");
    }
    validateLevel2LifecycleTail(tail,arrival);
    const auto input=recoveredLevel2PeriodicInput(tail.work.state);
    const auto expected=level2PeriodicSpawnOperations(input,level2Rng_,ownedShared);
    if(!same(periodic,expected)) {
        throw std::invalid_argument("Level2 periodic tail differs from its complete source0DDC receipt");
    }
    std::optional<Level1ArrivalWork> expectedArrival;
    if(arrival) {
        expectedArrival=level2ArrivalOperations(recoveredLevel2ArrivalInput(tail.work.state,expected,cachedEnemyFlag),expected.rng,ownedShared);
        if(!sameArrival(*arrival,*expectedArrival)) {throw std::invalid_argument("Level2 arrival changed its source46C8 receipt");}
    } else if(cachedEnemyFlag!=0) {throw std::invalid_argument("Level2 cached radar flag omitted its arrival receipt");}
    const auto keys=recoveredLevel2RenderKeys();
    std::vector<std::size_t> allocations;
    for(const auto ref:expected.allocationOrder) {
        if(ref.kind==Level1PresentationKind::entity) {
            if(ref.index<keys.size() && keys[ref.index]<projectileKeyBase) {allocations.push_back(ref.index);}
            else if(expected.spawn && ref.index==keys.size() && expected.spawn->logicalIndex==ref.index) {allocations.push_back(ref.index);}
            else {throw std::invalid_argument("Level2 periodic result lost its actual ordinary/fixed alias");}
            continue;
        }
        const auto key=ref.kind==Level1PresentationKind::projectile ? projectileKeyBase+ref.index : effectKeyBase+ref.index;
        const auto found=std::find(keys.begin(),keys.end(),key);
        if(found==keys.end()) {throw std::invalid_argument("Level2 periodic result lost a retained FD/FC mapping");}
        allocations.push_back(static_cast<std::size_t>(found-keys.begin()));
    }
    if(expected.spawn) {
        const auto& birth=*expected.spawn;
        const auto& previous=input.ordinaryPool.at(birth.physicalSlot);
        if(birth.appendedAllocationIndex!=level2AllocationListOrder_.size()
            || birth.cacheReset.physicalKey!=birth.physicalSlot || birth.cacheReset.operationOffset>expected.operations.size()
            || (previous.entityIndex ? birth.logicalIndex!=*previous.entityIndex : birth.logicalIndex!=level2Entities_.size())) {
            throw std::invalid_argument("Level2 periodic birth changed its physical/logical reuse boundary");
        }
        const auto rams=std::count_if(input.ordinaryPool.begin(),input.ordinaryPool.end(),[](const auto& slot) {
            return slot.actor.type==13 && slot.actor.active();
        });
        if(birth.actor.type==13 && rams>=maximumLiveRams) {
            throw std::invalid_argument("Level2 periodic birth exceeds its qualified liveType13 damage bound");
        }
    }
    if(expectedArrival && expectedArrival->birth) {
        const auto& birth=*expectedArrival->birth;
        const auto index=birth.actor.allocationIndex;
        const auto& previous=input.ordinaryPool.at(birth.poolSlot);
        const auto next=level2Entities_.size()+(expected.spawn && expected.spawn->logicalIndex==level2Entities_.size() ? 1U : 0U);
        if(birth.appendedAllocationIndex!=allocations.size()
            || (previous.entityIndex ? index!=*previous.entityIndex : index!=next)) {
            throw std::invalid_argument("Level2 arrival changed its physical/logical reuse boundary");
        }
        allocations.push_back(index);
    }
    //All source/result/mapping validation completed. Recenter old physical aliases before replacing a retired FF.
    applyLevel2CombatTailState(tail.work.state);
    level2Rng_=expected.rng;
    if(expected.spawn) {
        const auto& birth=*expected.spawn;
        if(birth.logicalIndex==level2Entities_.size()) {
            level2Entities_.push_back(birth.actor);
            level2EntityActorPoolSlots_.push_back(birth.physicalSlot);
            level2EntityFixedPoolSlots_.push_back(level2FixedObjectPoolSize_);
            level2EntityProjectilePoolSlots_.push_back(level2ProjectilePoolSize_);
            level2EntityDeathEffectPoolSlots_.push_back(level2DeathEffectPoolSize_);
        } else {level2Entities_[birth.logicalIndex]=birth.actor;}
        level2ActorPool_[birth.physicalSlot]=birth.actor;level2ActorPoolActive_[birth.physicalSlot]=true;
        ++level2ActorIncarnations_[birth.physicalSlot];
        level2ActiveEnemyCount_=birth.activeEnemies;level2SpawnedEnemyCount_=birth.spawnedEnemies;
        ++level2AllocationRevision_;++level2NonProjectileAllocationRevision_;
        level2RetiredEffectSemanticOrderRebuilt_=false;
        level2Visibility_.allocationSucceeded();
    }
    if(expectedArrival) {level2Rng_=expectedArrival->rng;}
    if(expectedArrival && expectedArrival->birth) {
        const auto& birth=*expectedArrival->birth;
        const auto index=birth.actor.allocationIndex;
        if(index==level2Entities_.size()) {
            level2Entities_.push_back(birth.actor);level2EntityActorPoolSlots_.push_back(birth.poolSlot);
            level2EntityFixedPoolSlots_.push_back(level2FixedObjectPoolSize_);
            level2EntityProjectilePoolSlots_.push_back(level2ProjectilePoolSize_);
            level2EntityDeathEffectPoolSlots_.push_back(level2DeathEffectPoolSize_);
        } else {level2Entities_.at(index)=birth.actor;}
        level2ActorPool_[birth.poolSlot]=birth.actor;level2ActorPoolActive_[birth.poolSlot]=true;
        ++level2ActorIncarnations_[birth.poolSlot];
        level2CompletionObjectIndex_=index;
        level2ActiveEnemyCount_=birth.activeEnemies;level2SpawnedEnemyCount_=birth.spawnedEnemies;
        ++level2AllocationRevision_;++level2NonProjectileAllocationRevision_;
        level2RetiredEffectSemanticOrderRebuilt_=false;level2Visibility_.allocationSucceeded();
    }
    if(expected.spawn || (expectedArrival && expectedArrival->birth)) {
        level2AllocationListOrder_=std::move(allocations);level2ObjectListOrder_=level2AllocationListOrder_;
    }
    level2LoopTick_=tail.work.state.loopTick;level2TailConsumed_=true;
    finishLevel2LifecycleTail(tail);
}
} //namespace stellar7
