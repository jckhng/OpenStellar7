#include "stellar7/gameplay.hpp"
#include "stellar7/level2_compact_bridge.hpp"
#include "stellar7/level2_projectile_prefix_receipt.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace stellar7 {
namespace {
constexpr std::size_t constructorCount = 7;
constexpr std::size_t noIndex = std::numeric_limits<std::size_t>::max();
constexpr std::size_t fixedKeyBase = 16;
constexpr std::size_t projectileKeyBase = 32;
constexpr std::size_t effectKeyBase = 40;
constexpr std::array<std::size_t,6> abilityTimerIndices{0,1,2,4,5,6};

auto fields(const Level1Entity& actor) {
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
auto fields(const Level1Projectile& actor) {
    return std::tie(actor.poolSlot,actor.flags,actor.type,actor.collisionRadius,
        actor.position.x,actor.position.y,actor.position.z,actor.pitch,actor.roll,actor.yaw,
        actor.pitchVelocity,actor.rollVelocity,actor.yawVelocity,actor.xVelocity,actor.yVelocity,
        actor.zVelocity,actor.forwardVelocity,actor.stateBytes4dTo54,actor.ownerIsPlayer,
        actor.ownerAllocationIndex,actor.presenterDistance,actor.targetHeading);
}
Level1Projectile projectile(const Level1Entity& actor,const std::size_t slot) {
    Level1Projectile result{};
    result.poolSlot=slot;result.flags=actor.flags;result.type=actor.type;result.collisionRadius=actor.collisionRadius;
    result.position=actor.position;result.pitch=actor.pitch;result.roll=actor.roll;result.yaw=actor.yaw;
    result.pitchVelocity=actor.pitchVelocity;result.rollVelocity=actor.rollVelocity;result.yawVelocity=actor.yawVelocity;
    result.xVelocity=actor.xVelocity;result.yVelocity=actor.yVelocity;result.zVelocity=actor.zVelocity;
    result.forwardVelocity=actor.forwardVelocity;
    std::copy(actor.stateBytes4dTo52.begin(),actor.stateBytes4dTo52.end(),result.stateBytes4dTo54.begin());
    result.ownerIsPlayer=actor.ownerIsPlayer;result.ownerAllocationIndex=actor.ownerAllocationIndex;
    result.presenterDistance=actor.presenterDistance;result.targetHeading=actor.targetHeading;
    return result;
}
Level1Entity actor(const Level1Projectile& source,Level1Entity result) {
    result.flags=source.flags;result.type=source.type;result.collisionRadius=source.collisionRadius;
    result.position=source.position;result.pitch=source.pitch;result.roll=source.roll;result.yaw=source.yaw;
    result.pitchVelocity=source.pitchVelocity;result.rollVelocity=source.rollVelocity;result.yawVelocity=source.yawVelocity;
    result.xVelocity=source.xVelocity;result.yVelocity=source.yVelocity;result.zVelocity=source.zVelocity;
    result.forwardVelocity=source.forwardVelocity;
    std::copy_n(source.stateBytes4dTo54.begin(),result.stateBytes4dTo52.size(),result.stateBytes4dTo52.begin());
    result.ownerIsPlayer=source.ownerIsPlayer;result.ownerAllocationIndex=source.ownerAllocationIndex;
    result.presenterDistance=source.presenterDistance;result.targetHeading=source.targetHeading;
    return result;
}
bool same(const std::vector<NormalWorkOperation>& a,const std::vector<NormalWorkOperation>& b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](const auto& x,const auto& y) {
        return x.kind==y.kind && x.count==y.count && x.value==y.value;
    });
}
bool same(const std::vector<Level1Entity>& a,const std::vector<Level1Entity>& b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](const auto& x,const auto& y) {
        return fields(x)==fields(y);
    });
}
bool same(StellarRng a,StellarRng b) {
    if(a.indexA()!=b.indexA() || a.indexB()!=b.indexB() || a.last()!=b.last() || a.calls()!=b.calls()) {return false;}
    //Read-only copies validate the private table through its existing API.
    for(std::size_t i=0;i<2*StellarRng::wordCount;++i) {if(a.next()!=b.next()) {return false;}}
    return true;
}
bool same(const CockpitPageState& a,const CockpitPageState& b) {
    return a.frontPage()==b.frontPage() && a.backPage()==b.backPage() && a.cachePage()==b.cachePage()
        && a.drawPage()==b.drawPage() && a.copyPage()==b.copyPage();
}
bool same(const IndexedPageEffect& a,const IndexedPageEffect& b) {
    if(a.index()!=b.index()) {return false;}
    if(const auto* x=std::get_if<RadarPixelWrite>(&a)) {
        const auto& y=std::get<RadarPixelWrite>(b);
        return x->point.x==y.point.x && x->point.y==y.point.y && x->color==y.color && x->page==y.page;
    }
    const auto& x=std::get<RadarPageCopy>(a);const auto& y=std::get<RadarPageCopy>(b);
    return x.sourcePage==y.sourcePage && x.destinationPage==y.destinationPage
        && x.position.x==y.position.x && x.position.y==y.position.y
        && x.coveredWidth==y.coveredWidth && x.height==y.height;
}
bool same(const ScoreWork& a,const ScoreWork& b) {
    if(a.score!=b.score || !same(a.pages,b.pages) || a.drawPage!=b.drawPage || a.copyPage!=b.copyPage
        || a.textColor!=b.textColor || a.text!=b.text || !same(a.operations,b.operations)
        || a.effects.size()!=b.effects.size() || a.fontSelections.size()!=b.fontSelections.size()) {return false;}
    for(std::size_t i=0;i<a.effects.size();++i) {if(!same(a.effects[i],b.effects[i])) {return false;}}
    for(std::size_t i=0;i<a.fontSelections.size();++i) {
        const auto& x=a.fontSelections[i];const auto& y=b.fontSelections[i];
        if(x.operationOffset!=y.operationOffset || x.selection!=y.selection
            || x.guestElapsedQualified!=y.guestElapsedQualified) {return false;}
    }
    return true;
}
enum class PrefixComparison { strict, periodic };
bool same(const Level1TargetPrefixWork& a,const Level1TargetPrefixWork& b,
    const PrefixComparison comparison=PrefixComparison::strict) {
    const auto& x=a.damage.combat.prefix;const auto& y=b.damage.combat.prefix;
    if((comparison==PrefixComparison::periodic && x.impulse!=y.impulse) || fields(x.player)!=fields(y.player) || !same(x.entities,y.entities)
        || x.allocationOrder!=y.allocationOrder || x.activeOrder!=y.activeOrder
        || !(x.lastListCamera==y.lastListCamera) || x.refreshRequested!=y.refreshRequested
        || !same(x.rng,y.rng)
        || !same(x.operations,y.operations) || a.damage.armor!=b.damage.armor || a.damage.fadeTicks!=b.damage.fadeTicks
        || a.damage.paletteSteps.size()!=b.damage.paletteSteps.size()
        || a.bonus.lastType!=b.bonus.lastType || a.bonus.consecutive!=b.bonus.consecutive
        || a.activeEnemies!=b.activeEnemies || a.score!=b.score || a.scoreSteps.size()!=b.scoreSteps.size()
        || a.damage.combat.audioEvents.size()!=b.damage.combat.audioEvents.size()
        || a.targetHits.size()!=b.targetHits.size()) {return false;}
    for(std::size_t i=0;i<x.projectiles.size();++i) {
        if(fields(x.projectiles[i])!=fields(y.projectiles[i])) {return false;}
    }
    for(std::size_t i=0;i<a.effects.size();++i) {
        if(fields(a.effects[i].entity)!=fields(b.effects[i].entity) || a.effects[i].listed!=b.effects[i].listed
            || a.effects[i].projected.x!=b.effects[i].projected.x || a.effects[i].projected.y!=b.effects[i].projected.y) {return false;}
    }
    for(std::size_t i=0;i<a.damage.combat.audioEvents.size();++i) {
        const auto& left=a.damage.combat.audioEvents[i];const auto& right=b.damage.combat.audioEvents[i];
        if(left.loopTick!=right.loopTick || left.eventId!=right.eventId) {return false;}
    }
    for(std::size_t i=0;i<a.scoreSteps.size();++i) {
        if(a.scoreSteps[i].operationOffset!=b.scoreSteps[i].operationOffset
            || !same(a.scoreSteps[i].work,b.scoreSteps[i].work)) {return false;}
    }
    for(std::size_t i=0;i<a.damage.paletteSteps.size();++i) {
        const auto& left=a.damage.paletteSteps[i];const auto& right=b.damage.paletteSteps[i];
        if(std::tie(left.operationOffset,left.request.start,left.request.count,left.request.targetColor,left.request.intensity)
            !=std::tie(right.operationOffset,right.request.start,right.request.count,right.request.targetColor,right.request.intensity)) {return false;}
    }
    for(std::size_t i=0;i<a.targetHits.size();++i) {
        const auto& left=a.targetHits[i];const auto& right=b.targetHits[i];
        if(left.projectileSlot!=right.projectileSlot || left.targetIndex!=right.targetIndex
            || left.operationOffset!=right.operationOffset) {return false;}
    }
    return true;
}
struct View {
    std::vector<Level1Entity> actors;
    std::vector<Level1ObjectRef> identities;
    std::vector<std::size_t> allocations;
    std::vector<std::size_t> active;
};
View view(const Level1TargetPrefixWork& work,const std::array<Level1Entity,Level1State::projectilePoolSize>& projectilePool,
    const Level2CompactBridge* bridge=nullptr) {
    const auto& source=work.damage.combat.prefix;
    View result{source.entities,{},{},{}};
    for(std::size_t index=0;index<source.entities.size();++index) {
        result.identities.push_back({Level1PresentationKind::entity,index});
    }
    const auto append=[&](Level1Entity value,const Level1ObjectRef ref) {
        value.allocationIndex=result.actors.size();result.actors.push_back(value);result.identities.push_back(ref);
    };
    for(std::size_t slot=0;slot<source.projectiles.size();++slot) {
        if(source.projectiles[slot].active()) {
            auto value=projectilePool[slot];
            if(bridge) {bridge->project(value);}
            append(actor(source.projectiles[slot],value),{Level1PresentationKind::projectile,slot});
        }
    }
    for(std::size_t slot=0;slot<work.effects.size();++slot) {
        const Level1ObjectRef ref{Level1PresentationKind::deathEffect,slot};
        if(work.effects[slot].listed || std::find(source.activeOrder.begin(),source.activeOrder.end(),ref)!=source.activeOrder.end()) {
            auto effect=work.effects[slot].entity;
            //0BC8's callback write projects through the existing source profile.
            effect.animatedModelPresenterEnabled=false;
            effect.renderProfile=work.effects[slot].listed ? WorldActorRenderProfile::bitmapEffectCallback
                : WorldActorRenderProfile::noHandles;
            append(effect,ref);
        }
    }
    const auto convert=[&](const auto& from,auto& into) {
        for(const auto ref:from) {
            const auto found=std::find(result.identities.begin(),result.identities.end(),ref);
            if(found==result.identities.end()) {throw std::logic_error("Level2 prefix lost a returned physical identity");}
            into.push_back(static_cast<std::size_t>(found-result.identities.begin()));
        }
    };
    convert(source.allocationOrder,result.allocations);convert(source.activeOrder,result.active);
    return result;
}
} // namespace


Level1TargetPrefixInput GameplayModel::recoveredLevel2ProjectilePrefixInput(const Level1PlayerCombatDomain domain) const {
    return level2ProjectilePrefixInput(domain,nullptr);
}

Level1TargetPrefixInput GameplayModel::recoveredLevel2PeriodicProjectilePrefixInput(const Level1PlayerCombatDomain domain) const {
    Level2PeriodicOrdinaryProfile periodic{};
    return level2ProjectilePrefixInput(domain,&periodic);
}

Level1TargetPrefixInput GameplayModel::level2ProjectilePrefixInput(const Level1PlayerCombatDomain domain,
    const Level2PeriodicOrdinaryProfile* periodic) const {
    if(domain!=Level1PlayerCombatDomain::survivingOnly && domain!=Level1PlayerCombatDomain::expiryAndLoss) {
        throw std::invalid_argument("unknown Level2 player-combat arithmetic domain");
    }
    const auto minimumArmor=domain==Level1PlayerCombatDomain::expiryAndLoss ? 0 : 200;
    if(phase_!=GameplayPhase::level2 || activeCampaignLevel_!=1 || level1_.levelIndex_!=1
        || !level1_.entities_.empty() || !level2HasAdvanced_ || !level2TailConsumed_ || !level2PrefixGuards_
        || level2Complete_ || level2PlayerArmor_<minimumArmor || level2PlayerArmor_>680 || (!periodic && level2PlayerCollisionMomentum_!=0)
        || level1_.selectedSystem_!=-1
        || std::any_of(abilityTimerIndices.begin(),abilityTimerIndices.end(),
            [&](const auto index){return level1_.systemTimers_[index]!=0;})
        || level2Visibility_.tracking()!=VisibilityTracking::sourceInitialized || !level2Visibility_.lastCamera()
        || !level2PendingTerminalEffectPresentations_.empty() || !level2RetiredEffectSemanticEntities_.empty()
        || !level2RetiredEffectSemanticActiveOrder_.empty() || !level2RetiredEffectSemanticActivePositions_.empty()
        || !level2EffectSemanticActivePositionOverrides_.empty()) {
        throw std::invalid_argument("Level2 projectile prefix requires its source session owners");
    }
    const auto keys=recoveredLevel2RenderKeys();
    Level1TargetPrefixInput result{};auto& prefix=result.prefix;
    const Level2CompactBridge bridge(keys);
    prefix.player=level2Player_;
    if(periodic) {
        bridge.project(prefix.player);
        for(const auto semantic:bridge.compactToSemantic) {
            auto value=level2Entities_[semantic];value.allocationIndex=prefix.entities.size();
            bridge.project(value);prefix.entities.push_back(value);
        }
    } else {
        prefix.entities.assign(level2Entities_.begin(),level2Entities_.begin()+constructorCount);
    }
    prefix.lastListCamera=*level2Visibility_.lastCamera();prefix.refreshRequested=level2Visibility_.refreshRequested();
    prefix.rng=level2Rng_;prefix.fadeTicks=level2PrefixGuards_->fadeTicks;
    prefix.specialActorPresent=level2PrefixGuards_->specialActorPresent;prefix.impulse=level2PlayerCollisionMomentum_;
    prefix.active=true;prefix.projectileHandlePresent=level2PrefixGuards_->effectHandles;
    result.bonus={level1_.lastDestroyedType_,level1_.consecutiveDestroyedTypeCount_};
    result.activeEnemies=level2ActiveEnemyCount_;result.score=level2Score_;
    for(std::size_t slot=0;slot<prefix.projectiles.size();++slot) {
        prefix.projectiles[slot]=projectile(level2ProjectilePool_[slot],slot);
        result.effects[slot].entity=level2DeathEffectPool_[slot];result.effects[slot].listed=level2DeathEffectPoolActive_[slot];
    }
    std::vector<Level1ObjectRef> identities;std::array<bool,8> projectiles{},effects{};
    for(std::size_t index=0;index<keys.size();++index) {
        const auto key=keys[index];const auto& value=level2Entities_[index];
        if(value.allocationIndex!=index) {throw std::invalid_argument("Level2 prefix view index mismatch");}
        if((periodic && key<projectileKeyBase) || (!periodic && index<constructorCount)) {
            if(key>=projectileKeyBase || (value.type==30 ? key<fixedKeyBase : key>=fixedKeyBase)) {
                throw std::invalid_argument("Level2 constructor lost its physical pool category");
            }
            identities.push_back({Level1PresentationKind::entity,periodic ? bridge.compact(index) : index});
        } else if(key>=projectileKeyBase && key<effectKeyBase) {
            const auto slot=key-projectileKeyBase;projectiles[slot]=true;
            prefix.projectiles[slot]=projectile(value,slot);
            identities.push_back({Level1PresentationKind::projectile,slot});
        } else if(key>=effectKeyBase) {
            const auto slot=key-effectKeyBase;effects[slot]=true;result.effects[slot].entity=value;
            identities.push_back({Level1PresentationKind::deathEffect,slot});
        } else {throw std::invalid_argument("Level2 prefix excludes a nonconstructor ordinary birth");}
    }
    for(std::size_t slot=0;slot<projectiles.size();++slot) {
        if(level2ProjectilePoolActive_[slot]!=prefix.projectiles[slot].active()
            || (level2ProjectilePoolActive_[slot] && !projectiles[slot])
            || (level2DeathEffectPoolActive_[slot] && !effects[slot])) {
            throw std::invalid_argument("Level2 source pool liveness lacks its physical view");
        }
    }
    if(periodic) {
        for(auto& shot:prefix.projectiles) {
            if(!shot.ownerIsPlayer) {shot.ownerAllocationIndex=bridge.compact(shot.ownerAllocationIndex);}
        }
        for(auto& effect:result.effects) {bridge.project(effect.entity);}
    }
    const auto convert=[&](const auto& from,auto& into) {
        for(const auto index:from) {
            if(index>=identities.size()) {throw std::invalid_argument("Level2 source list exceeds physical view");}
            into.push_back(identities[index]);
        }
    };
    convert(level2AllocationListOrder_,prefix.allocationOrder);convert(level2ActiveListOrder_,prefix.activeOrder);
    return result;
}

StationaryLevel2PrefixWork GameplayModel::recoveredLevel2ProjectilePrefix(const BitmapFont& scoreFont,
    const CockpitPageState& pages, const Level2HostileProjectileModel* hostile, const Level1PlayerCombatDomain domain) const {
    const auto descriptor = hostile ? std::optional<Level2HostileProjectileModel>{*hostile} : std::nullopt;
    auto work=level2ProjectilePrefixOperations(recoveredLevel2ProjectilePrefixInput(domain),
        {level2LoopTick_,level2PlayerArmor_,0,descriptor,domain},scoreFont,pages);
    auto projected=view(work,level2ProjectilePool_);
    StationaryLevel2PrefixWork result{work.damage.combat.prefix.player,std::move(projected.actors),
        std::move(projected.active),work.damage.combat.prefix.operations};
    result.projectileReceipt=std::make_shared<const Level2ProjectilePrefixReceipt>(
        Level2ProjectilePrefixReceipt{std::move(work),scoreFont,pages,descriptor,domain});
    return result;
}

StationaryLevel2PrefixWork GameplayModel::recoveredLevel2PeriodicProjectilePrefix(const BitmapFont& scoreFont,
    const CockpitPageState& pages, const Level2PeriodicOrdinaryProfile& periodic,
    const Level2HostileProjectileModel* hostile, const Level1PlayerCombatDomain domain) const {
    auto descriptor=periodic;
    descriptor.physicalKeys=Level2CompactBridge(recoveredLevel2RenderKeys()).physicalKeys;
    const auto hostileDescriptor=hostile ? std::optional<Level2HostileProjectileModel>{*hostile} : std::nullopt;
    auto work=level2ProjectilePrefixOperations(level2ProjectilePrefixInput(domain,&descriptor),
        {level2LoopTick_,level2PlayerArmor_,0,hostileDescriptor,domain,descriptor},scoreFont,pages);
    const Level2CompactBridge bridge(recoveredLevel2RenderKeys());
    auto projected=view(work,level2ProjectilePool_,&bridge);
    StationaryLevel2PrefixWork result{work.damage.combat.prefix.player,std::move(projected.actors),
        std::move(projected.active),work.damage.combat.prefix.operations};
    result.projectileReceipt=std::make_shared<const Level2ProjectilePrefixReceipt>(
        Level2ProjectilePrefixReceipt{std::move(work),scoreFont,pages,hostileDescriptor,domain,descriptor});
    return result;
}

void GameplayModel::commitRecoveredLevel2ProjectilePrefix(const StationaryLevel2PrefixWork& prefix) {
    if(!prefix.projectileReceipt) {throw std::invalid_argument("Level2 prefix has no explicit projectile receipt");}
    const auto& receipt=*prefix.projectileReceipt;
    const auto incoming=level2ProjectilePrefixInput(receipt.combatDomain,
        receipt.periodic ? &*receipt.periodic : nullptr);
    const auto expected=receipt.periodic ? recoveredLevel2PeriodicProjectilePrefix(receipt.scoreFont,receipt.pages,
        *receipt.periodic,receipt.hostile ? &*receipt.hostile : nullptr,receipt.combatDomain)
        : recoveredLevel2ProjectilePrefix(receipt.scoreFont,receipt.pages,
        receipt.hostile ? &*receipt.hostile : nullptr,receipt.combatDomain);
    if(fields(prefix.player)!=fields(expected.player) || !same(prefix.actors,expected.actors)
        || prefix.activeOrder!=expected.activeOrder || !same(prefix.operations,expected.operations)
        || !same(receipt.work,expected.projectileReceipt->work,
            receipt.periodic ? PrefixComparison::periodic : PrefixComparison::strict)) {
        throw std::invalid_argument("Level2 projectile prefix differs from its canonical source receipt");
    }
    const auto& work=receipt.work;const auto& source=work.damage.combat.prefix;
    const Level2CompactBridge bridge(recoveredLevel2RenderKeys());
    const auto projected=view(work,level2ProjectilePool_,receipt.periodic ? &bridge : nullptr);
    auto visibility=level2Visibility_;
    const auto changed=source.allocationOrder!=incoming.prefix.allocationOrder;
    if(changed) {visibility.allocationSucceeded();}
    auto actors=projected.actors;
    const auto visible=visibility.refresh(source.player.position,actors,projected.allocations,
        visibility.refreshRequested() ? std::vector<std::size_t>{} : projected.active,VisibilityDomain::nativeGameplay);
    if(visible.activeOrder!=projected.active || visibility.refreshRequested()!=source.refreshRequested
        || !visibility.lastCamera() || !(*visibility.lastCamera()==source.lastListCamera)
        || !same(actors,projected.actors)) {
        throw std::invalid_argument("Level2 projectile visibility adoption differs from completed3768");
    }
    std::vector<std::size_t> ordinary,fixed,projectiles,effects;
    for(const auto ref:projected.identities) {
        ordinary.push_back(ref.kind==Level1PresentationKind::entity ? level2EntityActorPoolSlots_[receipt.periodic ? bridge.compactToSemantic.at(ref.index) : ref.index] : level2ActorPoolSize_);
        fixed.push_back(ref.kind==Level1PresentationKind::entity ? level2EntityFixedPoolSlots_[receipt.periodic ? bridge.compactToSemantic.at(ref.index) : ref.index] : level2FixedObjectPoolSize_);
        projectiles.push_back(ref.kind==Level1PresentationKind::projectile ? ref.index : level2ProjectilePoolSize_);
        effects.push_back(ref.kind==Level1PresentationKind::deathEffect ? ref.index : level2DeathEffectPoolSize_);
    }
    auto nextProjectiles=level2ProjectilePool_;
    auto nextProjectileActive=level2ProjectilePoolActive_;
    auto nextEffects=level2DeathEffectPool_;
    auto nextEffectActive=level2DeathEffectPoolActive_;
    for(std::size_t slot=0;slot<level2ProjectilePoolSize_;++slot) {
        auto retained=nextProjectiles[slot];
        if(receipt.periodic) {bridge.project(retained);}
        nextProjectiles[slot]=actor(source.projectiles[slot],retained);
        nextProjectileActive[slot]=source.projectiles[slot].active();
        nextEffects[slot]=work.effects[slot].entity;
        nextEffectActive[slot]=work.effects[slot].listed;
    }
    for(std::size_t index=0;index<projected.actors.size();++index) {
        if(projectiles[index]<level2ProjectilePoolSize_) {
            nextProjectiles[projectiles[index]]=projected.actors[index];
        } else if(effects[index]<level2DeathEffectPoolSize_) {
            nextEffects[effects[index]]=projected.actors[index];
        }
    }
    //All projections validated before any canonical pool or global write.
    level2ProjectilePool_=std::move(nextProjectiles);level2ProjectilePoolActive_=nextProjectileActive;
    level2DeathEffectPool_=std::move(nextEffects);level2DeathEffectPoolActive_=nextEffectActive;
    for(std::size_t index=0;index<projected.actors.size();++index) {
        const auto& value=projected.actors[index];
        if(ordinary[index]<level2ActorPoolSize_) {
            level2ActorPool_[ordinary[index]]=value;level2ActorPoolActive_[ordinary[index]]=value.active();
        } else if(fixed[index]<level2FixedObjectPoolSize_) {level2FixedPool_[fixed[index]]=value;}

    }
    if(receipt.periodic) {level2PlayerCollisionMomentum_=source.impulse;}
    level2Player_=source.player;level2Entities_=projected.actors;
    level2EntityActorPoolSlots_=std::move(ordinary);level2EntityFixedPoolSlots_=std::move(fixed);
    level2EntityProjectilePoolSlots_=std::move(projectiles);level2EntityDeathEffectPoolSlots_=std::move(effects);
    level2AllocationListOrder_=projected.allocations;level2ObjectListOrder_=projected.allocations;
    level2ActiveListOrder_=projected.active;level2Visibility_=std::move(visibility);level2Rng_=source.rng;
    level2ActiveEnemyCount_=work.activeEnemies;level2Score_=work.score;
    level2PlayerArmor_=work.damage.armor;level2PrefixGuards_->fadeTicks=work.damage.fadeTicks;
    level1_.lastDestroyedType_=work.bonus.lastType;level1_.consecutiveDestroyedTypeCount_=work.bonus.consecutive;
    level2AudioEvents_.insert(level2AudioEvents_.end(),work.damage.combat.audioEvents.begin(),work.damage.combat.audioEvents.end());
    if(changed) {++level2AllocationRevision_;level2RetiredEffectSemanticOrderRebuilt_=false;}
}
} // namespace stellar7
