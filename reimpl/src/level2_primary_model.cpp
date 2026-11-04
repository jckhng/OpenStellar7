#include "stellar7/gameplay.hpp"
#include "stellar7/level2_projectile_prefix_work.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace stellar7 {
namespace {
constexpr std::size_t constructorCount = 7;
constexpr std::size_t projectileType = 23;
constexpr std::uint16_t sharedModelCount = 44;
constexpr std::uint16_t primaryRadius = 134;
constexpr std::size_t noEntity = std::numeric_limits<std::size_t>::max();
constexpr std::array<std::size_t,6> abilityTimerIndices{0,1,2,4,5,6};

Level1Projectile projection(const Level1Entity& actor, const std::size_t slot) {
    Level1Projectile shot{};
    shot.poolSlot=slot;shot.flags=actor.flags;shot.type=actor.type;shot.collisionRadius=actor.collisionRadius;
    shot.position=actor.position;shot.pitch=actor.pitch;shot.roll=actor.roll;shot.yaw=actor.yaw;
    shot.pitchVelocity=actor.pitchVelocity;shot.rollVelocity=actor.rollVelocity;shot.yawVelocity=actor.yawVelocity;
    shot.xVelocity=actor.xVelocity;shot.yVelocity=actor.yVelocity;shot.zVelocity=actor.zVelocity;
    shot.forwardVelocity=actor.forwardVelocity;
    std::copy(actor.stateBytes4dTo52.begin(),actor.stateBytes4dTo52.end(),shot.stateBytes4dTo54.begin());
    shot.ownerIsPlayer=actor.ownerIsPlayer;shot.ownerAllocationIndex=actor.ownerAllocationIndex;
    shot.presenterDistance=actor.presenterDistance;shot.targetHeading=actor.targetHeading;
    return shot;
}

auto fields(const Level1Projectile& shot) {
    return std::tie(shot.poolSlot,shot.flags,shot.type,shot.collisionRadius,
        shot.position.x,shot.position.y,shot.position.z,shot.pitch,shot.roll,shot.yaw,
        shot.pitchVelocity,shot.rollVelocity,shot.yawVelocity,shot.xVelocity,shot.yVelocity,shot.zVelocity,
        shot.forwardVelocity,shot.stateBytes4dTo54,shot.ownerIsPlayer,shot.ownerAllocationIndex,
        shot.presenterDistance,shot.targetHeading);
}

bool same(const std::vector<NormalWorkOperation>& first, const std::vector<NormalWorkOperation>& second) {
    return first.size()==second.size() && std::equal(first.begin(),first.end(),second.begin(),
        [](const auto& a,const auto& b) {return a.kind==b.kind && a.count==b.count && a.value==b.value;});
}

bool same(const Level1PrimaryFireWork& a, const Level1PrimaryFireWork& b) {
    if(a.outcome!=b.outcome || a.launch.has_value()!=b.launch.has_value()
        || a.mainCooldown!=b.mainCooldown || a.weaponTimers!=b.weaponTimers
        || a.allocationCount!=b.allocationCount || a.refreshRequested!=b.refreshRequested
        || !(a.shotOrigin==b.shotOrigin) || !same(a.beforeEvent14,b.beforeEvent14)
        || !same(a.afterEvent14BeforeLamp,b.afterEvent14BeforeLamp) || !same(a.afterLamp,b.afterLamp)) {
        return false;
    }
    return !a.launch || (a.launch->weaponSlot==b.launch->weaponSlot && a.launch->poolSlot==b.launch->poolSlot
        && a.launch->allocationIndex==b.launch->allocationIndex && fields(a.launch->projectile)==fields(b.launch->projectile));
}

void reindex(Level1Entity& actor, const std::size_t insertion, const std::size_t oldSize) {
    if(!actor.collisionContactIsPlayer && actor.collisionContactAllocationIndex>=insertion
        && actor.collisionContactAllocationIndex<oldSize) {++actor.collisionContactAllocationIndex;}
    if(!actor.ownerIsPlayer && actor.ownerAllocationIndex>=insertion
        && actor.ownerAllocationIndex<oldSize) {++actor.ownerAllocationIndex;}
}
}

Level1PrimaryFireInput GameplayModel::recoveredLevel2PrimaryInput(const GameplayAssets& assets) const {
    return level2PrimaryInput(assets,Level1PrimaryFireMembership::level2Constructor);
}

Level1PrimaryFireInput GameplayModel::recoveredLevel2CombatPrimaryInput(const GameplayAssets& assets,
    const Level1PlayerCombatDomain domain) const {
    return level2PrimaryInput(assets,Level1PrimaryFireMembership::level2Combat,domain);
}

Level1PrimaryFireInput GameplayModel::level2PrimaryInput(const GameplayAssets& assets,
    const Level1PrimaryFireMembership membership, const Level1PlayerCombatDomain domain) const {
    const auto periodic=membership==Level1PrimaryFireMembership::level2Periodic;
    const auto combat=membership==Level1PrimaryFireMembership::level2Combat || periodic;
    const auto lifecycle=combat && domain==Level1PlayerCombatDomain::expiryAndLoss;
    if((domain!=Level1PlayerCombatDomain::survivingOnly && !lifecycle)) {
        throw std::invalid_argument("unknown Level2 primary arithmetic domain");
    }
    const auto minimumArmor=lifecycle ? -level2MaximumFrameDamage : 200;
    if(!combat && membership!=Level1PrimaryFireMembership::level2Constructor) {
        throw std::invalid_argument("unknown Level2 primary membership domain");
    }
    if(phase_!=GameplayPhase::level2 || activeCampaignLevel_!=1 || level1_.levelIndex_!=1
        || !level1_.entities_.empty() || !level2HasAdvanced_ || level2TailConsumed_ || !level2PrefixGuards_
        || level2Complete_ || level2PlayerArmor_<minimumArmor || level2PlayerArmor_>680 || playerCannonSlotCount_!=2
        || level1_.selectedSystem_!=-1
        || (combat ? std::any_of(abilityTimerIndices.begin(),abilityTimerIndices.end(),
                [&](const auto index){return level1_.systemTimers_[index]!=0;})
            : std::any_of(level1_.systemTimers_.begin(),level1_.systemTimers_.end(),[](auto timer) {return timer!=0;}))
        || level2Player_.weaponSlotTimers[2]!=0 || (!periodic && level2PlayerCollisionMomentum_!=0)
        || level2Visibility_.tracking()!=VisibilityTracking::sourceInitialized || !level2Visibility_.lastCamera()
        || (combat ? level2Entities_.size()<constructorCount
                || level2Entities_.size()>(periodic ? level2ActorPoolSize_+level2FixedObjectPoolSize_ : constructorCount)+level2ProjectilePoolSize_+level2DeathEffectPoolSize_
            : level2Entities_.size()!=constructorCount+std::count(level2ProjectilePoolActive_.begin(),level2ProjectilePoolActive_.end(),true)
                || level2AllocationListOrder_.size()!=level2Entities_.size())
        || level2EntityActorPoolSlots_.size()!=level2Entities_.size()
        || level2EntityFixedPoolSlots_.size()!=level2Entities_.size()
        || level2EntityProjectilePoolSlots_.size()!=level2Entities_.size()
        || level2EntityDeathEffectPoolSlots_.size()!=level2Entities_.size()
        || !level2PendingTerminalEffectPresentations_.empty() || !level2RetiredEffectSemanticEntities_.empty()
        || assets.models.size()<=sharedModelCount || assets.models[projectileType].scaleShift!=0
        || assets.models[projectileType].collisionRadius!=primaryRadius) {
        throw std::invalid_argument("Level2 primary projection requires its retained constructor/pool owners");
    }
    if(assets.campaignLevels.empty() ? assets.sharedModelRegistry[0]!=sharedModelCount
        : assets.campaignLevels.front().models.empty()
            || assets.campaignLevels.front().models.size()>assets.models.size()
            || assets.models.size()-assets.campaignLevels.front().models.size()!=sharedModelCount) {
        throw std::invalid_argument("Level2 primary projection lacks its decoded shared model bank");
    }
    std::vector<bool> seen(level2Entities_.size());
    std::vector<std::size_t> allocated;
    std::array<bool,level2ProjectilePoolSize_> selected{};
    std::array<bool,level2DeathEffectPoolSize_> selectedEffects{};
    std::size_t effectCount=0, ordinaryCount=0;
    std::array<std::size_t,3> constructors{};
    for(const auto index:level2AllocationListOrder_) {
        if(index>=seen.size() || seen[index]) {throw std::invalid_argument("Level2 primary has invalid49F8 membership");}
        seen[index]=true;
        const auto& actor=level2Entities_[index];
        const auto slot=level2EntityProjectilePoolSlots_[index];
        if(actor.allocationIndex!=index || !actor.active()) {throw std::invalid_argument("Level2 primary lost allocation identity");}
        if(slot<level2ProjectilePoolSize_) {
            const auto primary=actor.type==projectileType && actor.ownerIsPlayer && actor.ownerAllocationIndex==noEntity;
            const auto owner=actor.ownerAllocationIndex;
            const auto physicalOwner=owner<level2Entities_.size()
                && level2EntityActorPoolSlots_[owner]<level2ActorPoolSize_;
            const auto hostile=combat && (actor.type==19 || (periodic && (actor.type==17 || actor.type==20)))
                && !actor.ownerIsPlayer
                && (periodic ? physicalOwner : owner<constructorCount && level2Entities_[owner].type==8
                    && level2Entities_[owner].stateBytes4dTo52[0]==255 && level2Entities_[owner].stateBytes4dTo52[4]==2)
                && actor.collisionRadius==(actor.type==20 ? 134 : 441)
                && actor.stateBytes4dTo52[1]<=(actor.type==17 ? 54 : actor.type==20 ? 60 : 84)
                && actor.stateBytes4dTo52[2]==0 && actor.stateBytes4dTo52[4]==0
                && actor.position.z==0 && actor.pitch==0 && actor.roll==0 && actor.forwardVelocity==500;
            if(selected[slot] || !level2ProjectilePoolActive_[slot] || (!primary && !hostile)
                || actor.stateBytes4dTo52[0]!=253
                || actor.renderProfile!=WorldActorRenderProfile::inlineSelectorNoCallback) {
                throw std::invalid_argument("Level2 primary lost its physical FD identity");
            }
            selected[slot]=true;allocated.push_back(slot);
        } else if(combat && level2EntityDeathEffectPoolSlots_[index]<level2DeathEffectPoolSize_) {
            const auto effect=level2EntityDeathEffectPoolSlots_[index];
            if(selectedEffects[effect] || !level2DeathEffectPoolActive_[effect] || actor.type!=-1
                || actor.stateBytes4dTo52[0]!=252 || actor.renderProfile!=WorldActorRenderProfile::bitmapEffectCallback) {
                throw std::invalid_argument("Level2 primary lost its actual FC identity");
            }
            selectedEffects[effect]=true;++effectCount;
        } else {
            const auto type=actor.type;
            if(periodic && (type==1 || type==4)) {++ordinaryCount;continue;}
            if(type!=8 && type!=13 && type!=30) {throw std::invalid_argument("Level2 primary excludes nonconstructor actors");}
            ++constructors[type==8?0:type==13?1:2];
        }
    }
    if((combat ? (periodic ? ordinaryCount+constructors[0]+constructors[1]>12 : constructors[0]>3) || constructors[1]>2 || constructors[2]>2
                || selectedEffects!=level2DeathEffectPoolActive_
            : constructors!=std::array<std::size_t,3>{3,2,2})
        || selected!=level2ProjectilePoolActive_) {
        throw std::invalid_argument("Level2 primary requires actual seven-actor membership");
    }
    std::array<Level1Projectile,level2ProjectilePoolSize_> pool{};
    for(std::size_t slot=0;slot<pool.size();++slot) {pool[slot]=projection(level2ProjectilePool_[slot],slot);}
    const auto& model=assets.models[projectileType];
    return {level2Player_,pool,level2Player_.weaponCooldown,
        {level2Player_.weaponSlotTimers[0],level2Player_.weaponSlotTimers[1]},0,
        level2AllocationListOrder_.size(),allocated,level2Visibility_.refreshRequested(),level1ShotOrigin_,
        {sharedModelCount,model.scaleShift,model.collisionRadius},Level1ProjectileAllocationDomain::retained,
        membership,combat ? std::optional<Level1CombatEffectMembership>{{level2AllocationListOrder_.size(),
            ordinaryCount+constructors[0]+constructors[1]+constructors[2],effectCount}} : std::nullopt};
}

Level1PrimaryFireInput GameplayModel::recoveredLevel2PeriodicPrimaryInput(const GameplayAssets& assets,
    const Level1PlayerCombatDomain domain) const {
    return level2PrimaryInput(assets,Level1PrimaryFireMembership::level2Periodic,domain);
}

void GameplayModel::commitRecoveredLevel2PeriodicPrimary(const GameplayAssets& assets,const Level1PrimaryFireWork& work,
    const Level1PlayerCombatDomain domain) {
    commitLevel2Primary(assets,work,Level1PrimaryFireMembership::level2Periodic,domain);
}

void GameplayModel::commitRecoveredLevel2Primary(const GameplayAssets& assets, const Level1PrimaryFireWork& work) {
    commitLevel2Primary(assets,work,Level1PrimaryFireMembership::level2Constructor);
}

void GameplayModel::commitRecoveredLevel2CombatPrimary(const GameplayAssets& assets,const Level1PrimaryFireWork& work,
    const Level1PlayerCombatDomain domain) {
    commitLevel2Primary(assets,work,Level1PrimaryFireMembership::level2Combat,domain);
}

void GameplayModel::commitLevel2Primary(const GameplayAssets& assets,const Level1PrimaryFireWork& work,
    const Level1PrimaryFireMembership membership, const Level1PlayerCombatDomain domain) {
    const auto expected=level1PrimaryFireOperations(level2PrimaryInput(assets,membership,domain));
    if(!same(work,expected)) {throw std::invalid_argument("Level2 primary commit differs from its source05D4 receipt");}
    if(!work.launch) {return;}
    const auto& launch=*work.launch;
    auto shot=level2ProjectilePool_[launch.poolSlot];
    const auto& source=launch.projectile;
    shot.type=source.type;shot.collisionRadius=source.collisionRadius;shot.position=source.position;
    shot.pitch=source.pitch;shot.roll=source.roll;shot.yaw=source.yaw;shot.zVelocity=source.zVelocity;
    shot.forwardVelocity=source.forwardVelocity;
    std::copy_n(source.stateBytes4dTo54.begin(),shot.stateBytes4dTo52.size(),shot.stateBytes4dTo52.begin());
    shot.ownerIsPlayer=source.ownerIsPlayer;shot.ownerAllocationIndex=source.ownerAllocationIndex;
    auto insertion=level2Entities_.size();
    for(std::size_t i=0;i<level2Entities_.size();++i) {
        const auto slot=level2EntityProjectilePoolSlots_[i];
        if(slot<level2ProjectilePoolSize_ && slot>launch.poolSlot) {insertion=i;break;}
    }
    for(auto* order:{&level2AllocationListOrder_,&level2ObjectListOrder_,&level2ActiveListOrder_}) {
        for(auto& index:*order) {if(index>=insertion) {++index;}}
    }
    const auto oldSize=level2Entities_.size();
    reindex(shot,insertion,oldSize);reindex(level2Player_,insertion,oldSize);
    for(auto& actor:level2Entities_) {reindex(actor,insertion,oldSize);}
    for(auto& actor:level2ActorPool_) {reindex(actor,insertion,oldSize);}
    for(auto& actor:level2FixedPool_) {reindex(actor,insertion,oldSize);}
    for(auto& actor:level2ProjectilePool_) {reindex(actor,insertion,oldSize);}
    for(auto& actor:level2DeathEffectPool_) {reindex(actor,insertion,oldSize);}
    shot.allocationIndex=insertion;
    level2Entities_.insert(level2Entities_.begin()+insertion,shot);
    level2EntityActorPoolSlots_.insert(level2EntityActorPoolSlots_.begin()+insertion,level2ActorPoolSize_);
    level2EntityFixedPoolSlots_.insert(level2EntityFixedPoolSlots_.begin()+insertion,level2FixedObjectPoolSize_);
    level2EntityProjectilePoolSlots_.insert(level2EntityProjectilePoolSlots_.begin()+insertion,launch.poolSlot);
    level2EntityDeathEffectPoolSlots_.insert(level2EntityDeathEffectPoolSlots_.begin()+insertion,level2DeathEffectPoolSize_);
    for(std::size_t i=insertion+1;i<level2Entities_.size();++i) {level2Entities_[i].allocationIndex=i;}
    level2ProjectilePool_[launch.poolSlot]=shot;level2ProjectilePoolActive_[launch.poolSlot]=true;
    level2AllocationListOrder_.push_back(insertion);level2ObjectListOrder_=level2AllocationListOrder_;
    ++level2AllocationRevision_;level2RetiredEffectSemanticOrderRebuilt_=false;
    level2Visibility_.allocationSucceeded();level1ShotOrigin_=work.shotOrigin;
    level2Player_.weaponCooldown=work.mainCooldown;
    level2Player_.weaponSlotTimers[0]=work.weaponTimers[0];level2Player_.weaponSlotTimers[1]=work.weaponTimers[1];
    level2AudioEvents_.push_back({level2LoopTick_,14});
    if(membership==Level1PrimaryFireMembership::level2Constructor) {
        weaponIndicatorVisible_=true;weaponReadyIndicator_=false;
    }
}
} // namespace stellar7
