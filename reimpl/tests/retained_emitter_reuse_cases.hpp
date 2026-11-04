#pragma once
#include "stellar7/combat_effect_work.hpp"
#include "stellar7/level1_periodic_spawn_work.hpp"
#include <limits>

namespace {
using namespace stellar7;
constexpr auto retainedRoster=Level1ProjectileAllocationDomain::retainedRoster;

struct EmitterReuseState {
    Level1ProjectilePrefixInput prefix;
    std::array<Level1DeathEffect,Level1State::deathEffectPoolSize> effects{};
    Level1TargetBonusHistory bonus{};
    std::int16_t activeEnemies=3;
    std::uint32_t score=0;
};

Level1TargetPrefixProfile emitterProfile(const EmitterReuseState& state) {
    auto profile=targetProfile(2);
    profile.damage.enemy.allocationDomain=retainedRoster;
    const auto has3=std::any_of(state.prefix.entities.begin(),state.prefix.entities.end(),[](const auto& actor){return actor.type==3;});
    profile.damage.enemy.ordinaryFamily=has3 ? Level1OrdinaryFamily::type1And3 : Level1OrdinaryFamily::type1;
    profile.targetDomain=has3 ? Level1TargetHitDomain::periodicClass1 : Level1TargetHitDomain::initialOrdinary;
    return profile;
}

Level1TargetPrefixWork emitterStep(EmitterReuseState& state) {
    auto work=level1TargetPrefixOperations({state.prefix,state.effects,state.bonus,state.activeEnemies,state.score},
        emitterProfile(state),font(),CockpitPageState::fromRecoveredMenu());
    state.prefix=type20test::carry(work.damage.combat.prefix);state.effects=work.effects;
    state.bonus=work.bonus;state.activeEnemies=work.activeEnemies;state.score=work.score;
    return work;
}

std::vector<Model3d> emitterModels() {
    std::vector<Model3d> models(44);
    for(const auto type:{1,3,11}) {
        models[type].flags=1;models[type].collisionRadius=type==1 ? 1342 : type==3 ? 770 : 748;
    }
    models[11].animationLimits={8};return models;
}

void quietOtherEmitters(EmitterReuseState& state,const std::size_t owner) {
    for(const auto index:{std::size_t{0},std::size_t{1}}) {
        if(index==owner){continue;}
        auto& actor=state.prefix.entities[index];actor.position={index==0 ? -20000 : 20000,-20000,index==0 ? 0 : 1000};
        actor.yaw=0;actor.yawVelocity=0;actor.roll=0;actor.weaponCooldown=255;actor.weaponSlotTimers={255,255,255};
        actor.stateBytes4dTo52[5]=index==0 ? 1 : 4;actor.behaviorTimer=index==0 ? 127 : 63;
        actor.forwardVelocity=index==0 ? 0 : 270;
    }
}

void mainEffect(EmitterReuseState& state,const std::size_t slot) {
    auto& effect=state.effects[slot];
    const Level1ObjectRef identity{Level1PresentationKind::deathEffect,slot};
    const auto found=std::find(state.prefix.allocationOrder.begin(),state.prefix.allocationOrder.end(),identity);
    assert(found!=state.prefix.allocationOrder.end());
    const auto index=static_cast<std::size_t>(found-state.prefix.allocationOrder.begin());
    const auto& bytes=effect.entity.stateBytes4dTo52;
    const std::vector<IndexedBitmap> frames(32,{8,5,std::vector<std::uint8_t>(40,7)});
    const auto work=level1CombatEffectOperations({effect.entity.position,0,
        {effect.entity.flags,effect.projected,0,bytes[1],bytes[4],0x0ac8},
        {24,294,24,145},{159,84},0xa000,state.prefix.allocationOrder.size(),index},frames);
    effect.entity.flags=work.state.flags;effect.entity.stateBytes4dTo52[1]=work.state.age;
    effect.entity.stateBytes4dTo52[4]=work.state.endAge;effect.projected=work.state.projected;
    if(work.removal) {
        assert(work.removal->removedIndex==index);
        state.prefix.allocationOrder[index]=state.prefix.allocationOrder.back();state.prefix.allocationOrder.pop_back();
        state.prefix.refreshRequested=true;effect.listed=false;
    }
}

void killEmitter(EmitterReuseState& state,const std::size_t owner) {
    //Controlled existing-primary input; its whole prefix owns1176 and481A.
    auto& primary=state.prefix.projectiles[1];primary={};primary.poolSlot=1;
    primary.type=23;primary.collisionRadius=134;primary.forwardVelocity=500;
    const auto position=state.prefix.entities[owner].position;
    primary.position={position.x,position.y-500,0};primary.yaw=0;
    primary.stateBytes4dTo54={253,64,0,0,0,0,0,0};primary.ownerIsPlayer=true;
    primary.ownerAllocationIndex=std::numeric_limits<std::size_t>::max();
    state.prefix.allocationOrder.push_back({Level1PresentationKind::projectile,1});
    state.prefix.activeOrder.insert(state.prefix.activeOrder.begin(),{Level1PresentationKind::projectile,1});
    const auto killed=emitterStep(state);
    assert(killed.targetHits.size()==1 && killed.targetHits[0].targetIndex==owner);
    assert(!state.prefix.entities[owner].active() && state.prefix.projectiles[0].active());
    assert(state.effects[0].listed);
    //Carry existing0AC8/481A children until the FC retires; no invented list rewrite.
    while(state.effects[0].listed) {
        mainEffect(state,0);
        if(state.effects[0].listed){emitterStep(state);}
    }
}

Level1PeriodicSpawnWork reuseEmitter(EmitterReuseState& state,const std::size_t owner,const std::int16_t nextType,
    const bool postRelease) {
    Level1PeriodicSpawnInput input{state.activeEnemies,3,10,
        static_cast<std::uint16_t>(state.prefix.allocationOrder.size()),state.prefix.entities.size(),{}};
    std::size_t slot=0;
    for(const auto& actor:state.prefix.entities) {
        if(actor.stateBytes4dTo52[0]!=255){continue;}
        input.ordinaryPool[slot++]={actor.stateBytes4dTo52[1],actor.type==11,actor.allocationIndex};
    }
    std::array<std::uint16_t,StellarRng::wordCount> table{};
    table[55]=nextType==1 ? 261 : nextType==3 ? 25605 : 43525;
    table[54]=nextType==3 ? 2817 : 2567;table[53]=43981;
    const StellarRng sourceRng(table,48,110,0,0);
    if(postRelease) {
        rejects([&]{level1PeriodicSpawnOperations(input,sourceRng,emitterModels());});
        input.selectorReuse=Level1SelectorReuseDomain::retainedType11;
        input.retainedEntities=&state.prefix.entities;
    }
    auto work=postRelease ? level1RetainedPeriodicSpawnOperations(input,sourceRng,emitterModels())
        : level1PeriodicSpawnOperations(input,sourceRng,emitterModels());
    assert(work.selectorRelease.has_value()==postRelease);
    if(postRelease) {
        assert(work.selectorRelease->call.entityIndex==owner && !work.selectorRelease->call.guestElapsedQualified);
    }
    assert(work.spawn && work.spawn->actor.type==nextType && work.spawn->actor.allocationIndex==owner);
    state.prefix.entities[owner]=work.spawn->actor;state.prefix.rng=work.rng;state.activeEnemies=work.spawn->activeEnemies;
    state.prefix.allocationOrder.push_back({Level1PresentationKind::entity,owner});state.prefix.refreshRequested=true;
    assert(std::find(state.prefix.activeOrder.begin(),state.prefix.activeOrder.end(),
        Level1ObjectRef{Level1PresentationKind::entity,owner})==state.prefix.activeOrder.end());
    return work;
}

void retainEmitterChain(const unsigned kind) {
    //0: complete3→1/20;1: complete1→3/17;2: controlled post-release11→3/banked20.
    auto input=kind==2 ? type20test::cachedInput(3) : type3test::input(1);
    auto profile=kind==2 ? type20test::profile() : type3test::profile();
    const std::size_t owner=kind==2 ? 1 : 6;
    if(kind==1) {
        auto& actor=input.entities[owner];actor.type=1;actor.collisionRadius=1342;actor.weaponSlotCount=2;
        actor.forwardVelocity=0;actor.stateBytes4dTo52[4]=1;actor.weaponCooldown=0;
        profile.ordinaryFamily=Level1OrdinaryFamily::type1;
    }
    if(kind==2) {
        input.entities[owner].roll=22;input.entities[owner].position.z=150;
        //The cached AI fixture omits +1C; supply its existing137E heap projection.
        input.entities[owner].renderProfile=WorldActorRenderProfile::heapSelectorDeferredAnimation;
    }
    const auto birth=level1EnemyPrefixOperations(input,profile);
    assert(birth.prefix.projectiles[0].active() && birth.prefix.projectiles[0].ownerAllocationIndex==owner);
    assert(birth.prefix.projectiles[0].type==(kind==1 ? 17 : 20));
    EmitterReuseState state;state.prefix=type20test::carry(birth.prefix);state.activeEnemies=kind==2 ? 2 : 3;
    for(auto& effect:state.effects){effect.entity.flags=0x88;effect.entity.type=-1;effect.entity.stateBytes4dTo52[0]=252;}
    quietOtherEmitters(state,owner);killEmitter(state,owner);
    const auto beforeReuse=state.prefix.projectiles[0];
    reuseEmitter(state,owner,kind==0 ? 1 : 3,kind==2);
    assert(state.prefix.projectiles[0].roll==beforeReuse.roll && state.prefix.projectiles[0].position==beforeReuse.position);
    auto old=emitterProfile(state);old.damage.enemy.allocationDomain=Level1ProjectileAllocationDomain::retainedAirborne;
    rejects([&]{level1TargetPrefixOperations({state.prefix,state.effects,state.bonus,state.activeEnemies,state.score},old,font(),CockpitPageState::fromRecoveredMenu());});
    emitterStep(state);
    emitterStep(state);
    assert(state.prefix.projectiles[0].active() && state.prefix.projectiles[0].ownerAllocationIndex==owner);
    assert(state.prefix.projectiles[0].roll==beforeReuse.roll);
    const auto frame=state;
    auto invalidOwner=frame;invalidOwner.prefix.projectiles[0].ownerAllocationIndex=2;
    rejects([&]{emitterStep(invalidOwner);});
    invalidOwner=frame;invalidOwner.prefix.projectiles[0].ownerAllocationIndex=frame.prefix.entities.size();
    rejects([&]{emitterStep(invalidOwner);});
    auto overlap=frame;
    auto& shot=overlap.prefix.projectiles[0];
    Level1Entity motion{};motion.yaw=shot.yaw;motion.roll=shot.roll;motion.forwardVelocity=shot.forwardVelocity;
    advanceEntityKinematics(motion);
    const auto emitter=overlap.prefix.entities[owner].position;
    shot.position={emitter.x-motion.position.x,emitter.y-motion.position.y,shot.position.z};
    const Level1ObjectRef identity{Level1PresentationKind::projectile,0};
    auto found=std::find(overlap.prefix.activeOrder.begin(),overlap.prefix.activeOrder.end(),identity);
    std::rotate(overlap.prefix.activeOrder.begin(),found,found+1);
    const auto ignored=emitterStep(overlap);
    assert(overlap.prefix.projectiles[0].active() && ignored.targetHits.empty());
    auto contact=frame;
    auto& hitting=contact.prefix.projectiles[0];const auto target=contact.prefix.entities[2].position;
    hitting.position={target.x-motion.position.x,target.y-motion.position.y,hitting.position.z};
    found=std::find(contact.prefix.activeOrder.begin(),contact.prefix.activeOrder.end(),identity);
    std::rotate(contact.prefix.activeOrder.begin(),found,found+1);
    const auto retired=emitterStep(contact);
    assert(!contact.prefix.projectiles[0].active() && retired.targetHits.empty() && retired.scoreSteps.empty());
    auto expired=frame;expired.prefix.projectiles[0].stateBytes4dTo54[1]=1;
    emitterStep(expired);
    assert(!expired.prefix.projectiles[0].active());
    const auto retained=expired.prefix.projectiles[0];
    Level1PrimaryFireInput fire{expired.prefix.player,expired.prefix.projectiles,0,{},0,
        expired.prefix.allocationOrder.size(),{},expired.prefix.refreshRequested,{}, {44,0,134},retainedRoster,
        Level1PrimaryFireMembership::combatEffects,
        Level1CombatEffectMembership{expired.prefix.allocationOrder.size(),static_cast<std::size_t>(state.activeEnemies+4),0},
        &expired.prefix.entities};
    const auto reused=level1PrimaryFireOperations(fire);
    assert(reused.launch && reused.launch->poolSlot==0 && reused.launch->projectile.type==23);
    assert(reused.launch->projectile.presenterDistance==retained.presenterDistance && reused.launch->projectile.targetHeading==retained.targetHeading);
    assert(reused.launch->projectile.ownerIsPlayer && reused.launch->projectile.ownerAllocationIndex==std::numeric_limits<std::size_t>::max());
    if(kind==1) {
        auto legacy=fire;legacy.allocationDomain=Level1ProjectileAllocationDomain::retainedAirborne;
        legacy.retainedEmitterStorage=nullptr;rejects([&]{level1PrimaryFireOperations(legacy);});
    }
    fire.retainedEmitterStorage=nullptr;rejects([&]{level1PrimaryFireOperations(fire);});
    fire.retainedEmitterStorage=&expired.prefix.entities;
    fire.pool[0].ownerAllocationIndex=2;rejects([&]{level1PrimaryFireOperations(fire);});
    fire.pool[0]=retained;fire.pool[0].ownerAllocationIndex=expired.prefix.entities.size();
    rejects([&]{level1PrimaryFireOperations(fire);});
}

void retainedEmitterReuseCases() {
    for(unsigned kind=0;kind<3;++kind){retainEmitterChain(kind);}
}
}
