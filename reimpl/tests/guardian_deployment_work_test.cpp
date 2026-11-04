#include "deployment_fixtures.hpp"
#include <iostream>
#include "arrival_prefix_fixtures.hpp"
namespace {
std::vector<stellar7::Model3d> level2Models() {
    auto bank=deployment_fixture::models();
    bank[44].flags=1;
    bank[44].collisionRadius=1147;
    bank[44].animationLimits={6};
    return bank;
}
stellar7::Level1PeriodicSpawnInput level2Pool(const unsigned slot,const bool selector) {
    auto source=deployment_fixture::pool(slot,selector);
    source.enemyLimit=12;
    source.spawnedEnemies=13;
    return source;
}
stellar7::Level1Entity level2Craft() {
    stellar7::Level1Entity craft{};
    craft.allocationIndex=15;
    craft.type=7;
    craft.collisionRadius=13052;
    craft.stateBytes4dTo52={255,100,0,0,5,3};
    craft.position={100,200,5000};
    craft.targetHeading=0x1234;
    craft.flags=8;
    craft.behaviorTimer=30;
    craft.renderProfile=stellar7::WorldActorRenderProfile::inlineSelectorBeamCallback;
    return craft;
}
template<class F> bool rejectsLevel2(F f) {
    try { f(); } catch(const std::invalid_argument&) { return true; }
    return false;
}
bool level2ConstructorProfile() {
    using namespace deployment_fixture;
    const auto bank=level2Models();
    const auto craft=level2Craft();
    auto source=level2Pool(0,false);
    source.ordinaryPool[15].health=100;
    const auto fresh=level1GuardianConstructorOperations(craft,{},source,rng(),bank,GuardianDeploymentDomain::level2);
    const Steps expected{
        Ops{{NormalWorkKind::instructions,43},{NormalWorkKind::repeatString,99},
            {NormalWorkKind::instructions,236},{NormalWorkKind::repeatString,6},{NormalWorkKind::instructions,84}},
        Level1DeploymentHeapChild{Level1DeploymentHeapKind::allocateSelector,0,1,0xdcc6,false},
        Ops{{NormalWorkKind::instructions,9},{NormalWorkKind::repeatString,1},{NormalWorkKind::instructions,23}}};
    if(!fresh.birth || !same(fresh.steps,expected)) { std::cerr<<"Level2 fresh constructor work\n";return false; }
    const auto& actor=fresh.birth->actor;
    auto expectedRng=rng();expectedRng.next();expectedRng.next();
    const auto& heap=std::get<Level1DeploymentHeapChild>(fresh.steps[1]);
    if(actor.allocationIndex!=0 || actor.type!=44 || actor.collisionRadius!=1147
        || actor.position.x!=100 || actor.position.y!=200 || actor.position.z!=0 || actor.yaw!=0x1234
        || actor.stateBytes4dTo52!=std::array<std::uint8_t,6>{255,20,2,0,5,0}
        || actor.forwardVelocity!=0 || actor.modelAnimationVariant!=0 || actor.presentationModelAnimationVariant!=0
        || !actor.animatedModelPresenterEnabled || actor.renderProfile!=WorldActorRenderProfile::heapSelectorDeferredAnimation
        || fresh.birth->poolSlot!=0 || fresh.birth->appendedAllocationIndex!=source.allocationCount
        || fresh.birth->activeEnemies!=2 || fresh.birth->spawnedEnemies!=14 || !fresh.birth->refreshRequested
        || heap.entityIndex!=0 || fresh.rng.calls()!=2 || fresh.rng.last()!=expectedRng.last()
        || fresh.rng.indexA()!=expectedRng.indexA() || fresh.rng.indexB()!=expectedRng.indexB()) {
        std::cerr<<"Level2 fresh constructor state\n";return false;
    }
    const auto present=level1GuardianConstructorOperations(craft,0,source,rng(),bank,GuardianDeploymentDomain::level2);
    if(present.birth || present.rng.calls()!=0 || !same(present.steps,Steps{Ops{{NormalWorkKind::instructions,13}}})) {
        std::cerr<<"Level2 existing guardian work\n";return false;
    }
    auto badCount=source;badCount.spawnedEnemies=14;
    auto badLimit=source;badLimit.enemyLimit=10;
    auto full=source;for(auto& slot:full.ordinaryPool) { slot.health=1; }
    auto fullRegistry=source;fullRegistry.allocationCount=32;
    auto badModel=bank;badModel[44].animationLimits={5};
    const auto invalid=rejectsLevel2([&]{level1GuardianConstructorOperations(craft,{},source,rng(),bank);})
        && rejectsLevel2([&]{level1GuardianConstructorOperations(craft,{},pool(),rng(),models(),GuardianDeploymentDomain::level2);})
        && rejectsLevel2([&]{level1GuardianConstructorOperations(craft,{},badCount,rng(),bank,GuardianDeploymentDomain::level2);})
        && rejectsLevel2([&]{level1GuardianConstructorOperations(craft,{},badLimit,rng(),bank,GuardianDeploymentDomain::level2);})
        && rejectsLevel2([&]{level1GuardianConstructorOperations(craft,{},full,rng(),bank,GuardianDeploymentDomain::level2);})
        && rejectsLevel2([&]{level1GuardianConstructorOperations(craft,{},fullRegistry,rng(),bank,GuardianDeploymentDomain::level2);})
        && rejectsLevel2([&]{level1GuardianConstructorOperations(craft,{},source,rng(),badModel,GuardianDeploymentDomain::level2);});
    if(!invalid) { std::cerr<<"Level2 constructor domain guards\n"; }
    return invalid;
}
bool level2DeploymentProfile() {
    using namespace deployment_fixture;
    auto craft=level2Craft();craft.allocationIndex=0;
    auto source=level2Pool(1,true);source.ordinaryPool[0].health=100;
    const auto work=level1Type7DeploymentOperations({craft,{},source,rng(),registry()},level2Models(),GuardianDeploymentDomain::level2);
    std::vector<Level1DeploymentHeapChild> heaps;
    for(const auto& step:work.steps) {
        if(const auto* heap=std::get_if<Level1DeploymentHeapChild>(&step)) { heaps.push_back(*heap); }
    }
    const Steps expected{
        Ops{{NormalWorkKind::instructions,85}},
        Level1DeploymentHeapChild{Level1DeploymentHeapKind::releaseSelector,1,0,0xdcca,false},
        Ops{{NormalWorkKind::instructions,38},{NormalWorkKind::repeatString,99},
            {NormalWorkKind::instructions,236},{NormalWorkKind::repeatString,6},{NormalWorkKind::instructions,84}},
        Level1DeploymentHeapChild{Level1DeploymentHeapKind::allocateSelector,1,1,0xdcc6,false},
        Ops{{NormalWorkKind::instructions,9},{NormalWorkKind::repeatString,1},{NormalWorkKind::instructions,34}}};
    const auto valid=work.guardianBirth && same(work.steps,expected) && work.rng.calls()==2
        && work.guardianBirth->actor.allocationIndex==1 && work.guardianBirth->actor.stateBytes4dTo52[1]==20
        && work.guardianBirth->activeEnemies==2 && work.guardianBirth->spawnedEnemies==14
        && heaps.size()==2 && heaps[0].entityIndex==1 && heaps[1].entityIndex==1
        && work.actor.behaviorTimer==29 && (work.actor.flags&8U)==0
        && work.actor.renderProfile==WorldActorRenderProfile::inlineSelectorNoCallback
        && work.refreshRequested && !work.craftRetired
        && work.allocationOrder.back()==Level1ObjectRef{Level1PresentationKind::entity,1}
        && work.allocationOrder.size()==registry().size()+1;
    if(!valid) { std::cerr<<"Level2 Type7 deployment/reused selector order\n"; }
    return valid;
}
bool retainedCallbackProfile() {
    using namespace deployment_fixture;
    Level1Entity craft{};
    craft.type=7;craft.stateBytes4dTo52[0]=255;
    const auto born=level1GuardianConstructorOperations(craft,{},pool(),rng(),models());
    auto actor=born.birth->actor;
    actor.presenterDistance=2000;
    actor.targetHeading=actor.yaw;
    std::array<Level1Projectile,Level1State::projectilePoolSize> shots{};
    const auto disabled=level1GuardianBehaviorOperations({actor,{},1,shots,registry()},models()[22]);
    actor=disabled.actor;
    actor.targetHeading=static_cast<std::uint16_t>(actor.yaw+1000);
    const auto enabled=level1GuardianBehaviorOperations({actor,{},1,shots,registry()},models()[22]);
    const auto valid=!disabled.actor.animatedModelPresenterEnabled
        && disabled.actor.renderProfile==WorldActorRenderProfile::heapSelectorNoCallback
        && enabled.actor.animatedModelPresenterEnabled
        && enabled.actor.renderProfile==WorldActorRenderProfile::heapSelectorDeferredAnimation;
    if(!valid) { std::cerr<<"retained callback/profile projection\n"; }
    return valid;
}
bool retainedSelectorRelease() {
    using namespace deployment_fixture;
    auto in=arrival_prefix_fixture::input();
    auto& craft=in.prefix.entities[0];
    craft.stateBytes4dTo52={255,100,3,0,5,3};
    craft.behaviorTimer=30;craft.forwardVelocity=0;craft.flags=12;
    craft.renderProfile=WorldActorRenderProfile::inlineSelectorBeamCallback;
    in.prefix.activeOrder.push_back({Level1PresentationKind::entity,0});
    auto sourcePool=Level1PeriodicSpawnInput{1,11,10,
        static_cast<std::uint16_t>(in.prefix.allocationOrder.size()),in.prefix.entities.size(),{}};
    sourcePool.ordinaryPool[0]={100,false,0};
    sourcePool.ordinaryPool[1]={0,true,1};
    std::array<std::int8_t,7> systems{};systems.fill(3);
    std::array<std::int16_t,7> timers{};const std::vector<IndexedBitmap> art;
    const auto modelBank=models();
    const auto work=level1DeploymentPrefixOperations(in,arrival_prefix_fixture::profile(),
        arrival_prefix_fixture::font(),CockpitPageState::fromRecoveredMenu(),256,
        arrival_prefix_fixture::bonusModel(),{systems,timers,-1,art},
        {0,{},sourcePool,modelBank});
    std::vector<Level1DeploymentHeapKind> providers;
    for(const auto& child:work.children) {
        if(const auto* heap=std::get_if<Level1DeploymentHeapChild>(&child.child)) {
            providers.push_back(heap->kind);
        }
    }
    const auto valid=providers==std::vector<Level1DeploymentHeapKind>{
        Level1DeploymentHeapKind::releaseSelector,Level1DeploymentHeapKind::allocateSelector};
    if(!valid) { std::cerr<<"retained Type11 selector release projection\n"; }
    return valid;
}
bool level2GuardianCycle() {
    auto actor=level2Craft();actor.type=44;actor.collisionRadius=1147;actor.position.z=0;
    actor.stateBytes4dTo52={255,20,2,0,5,0};actor.flags=0;actor.behaviorTimer=0;
    actor.renderProfile=WorldActorRenderProfile::heapSelectorDeferredAnimation;
    actor.animatedModelPresenterEnabled=true;actor.targetHeading=0;actor.yaw=0x8000;
    auto random=[](const std::uint16_t choice) {
        std::array<std::uint16_t,StellarRng::wordCount> words{};words[55]=choice;
        return StellarRng(words,48,110,0,0);
    };
    const auto start=level2GuardianBehaviorOperations({actor,random(64),false});
    if(start.rng.calls()!=1 || start.rng.last()!=64 || start.actor.type!=44
        || start.actor.yawVelocity!=512 || start.actor.forwardVelocity!=100
        || start.actor.behaviorTimer!=36 || start.actor.stateBytes4dTo52[5]!=1
        || start.emission!=Level2GuardianEmission::actor48) {
        std::cerr<<"Level2 A92E entry/emission/half-turn\n";return false;
    }
    auto current=start.actor;unsigned pulses=0;
    for(unsigned elapsed=1;elapsed<=36;++elapsed) {
        const auto previousTimer=current.behaviorTimer;
        const auto next=level2GuardianBehaviorOperations({current,random(0),false});
        current=next.actor;
        const auto selector=static_cast<std::uint8_t>(std::abs(static_cast<int>(previousTimer)-20)/4);
        if(next.rng.calls()!=1 || current.type!=45 || current.collisionRadius!=1147
            || current.yawVelocity!=0 || current.forwardVelocity!=0
            || current.stateBytes4dTo52[2]!=selector
            || current.modelAnimationVariant!=selector || current.presentationModelAnimationVariant!=selector
            || !current.animatedModelPresenterEnabled
            || current.renderProfile!=WorldActorRenderProfile::heapSelectorDeferredAnimation) {
            std::cerr<<"Level2 A92E retained cycle\n";return false;
        }
        if(next.emission) {
            if(*next.emission!=Level2GuardianEmission::pulse46 || current.behaviorTimer!=20) {
                std::cerr<<"Level2 A92E pulse boundary\n";return false;
            }
            ++pulses;
        }
    }
    if(pulses!=1 || current.behaviorTimer!=0 || current.stateBytes4dTo52[5]!=0) {
        std::cerr<<"Level2 A92E cycle return\n";return false;
    }
    const auto resumed=level2GuardianBehaviorOperations({current,random(300),false});
    if(resumed.actor.type!=44 || resumed.actor.stateBytes4dTo52[5]!=0 || resumed.emission) {
        std::cerr<<"Level2 A92E cycle reentry\n";return false;
    }
    actor.collisionContactIsPlayer=true;actor.collisionContactAllocationIndex=12;
    const auto blocked=level2GuardianBehaviorOperations({actor,random(0),false});
    auto craftGate=actor;craftGate.collisionContactIsPlayer=false;
    craftGate.collisionContactAllocationIndex=static_cast<std::size_t>(-1);
    const auto craftBlocked=level2GuardianBehaviorOperations({craftGate,random(0),true});
    if(blocked.emission || blocked.actor.collisionContactIsPlayer
        || blocked.actor.collisionContactAllocationIndex!=static_cast<std::size_t>(-1)
        || craftBlocked.emission) {
        std::cerr<<"Level2 A92E contact/craft gate\n";return false;
    }
    actor=start.actor;actor.behaviorTimer=21;actor.collisionContactIsPlayer=true;
    const auto pulseBlocked=level2GuardianBehaviorOperations({actor,random(0),false});
    actor.stateBytes4dTo52[1]=0;actor.collisionContactIsPlayer=false;
    const auto retired=level2GuardianBehaviorOperations({actor,random(0),false});
    if(pulseBlocked.emission || !pulseBlocked.actor.collisionContactIsPlayer
        || retired.emission!=Level2GuardianEmission::pulse46 || retired.actor.active()) {
        std::cerr<<"Level2 A92E retired OLD/contact gate\n";return false;
    }
    actor.collisionRadius=2792;
    return rejectsLevel2([&]{level2GuardianBehaviorOperations({actor,random(0),false});});
}
bool level2GuardianEmissions() {
    auto source=level2Craft();source.type=45;source.collisionRadius=1147;source.position.z=0;
    source.stateBytes4dTo52={255,20,2,0,5,1};source.yaw=0x1234;
    auto bank=level2Models();bank.resize(49);bank[13].flags=1;bank[13].collisionRadius=559;
    bank[48].flags=1;bank[48].collisionRadius=291;bank[48].animationLimits={14};
    bank[46].collisionRadius=2260;
    auto pool=level2Pool(1,false);pool.spawnedEnemies=14;
    const auto random=deployment_fixture::rng();
    const auto emitted=level2GuardianActorEmissionOperations({source,pool,random,1},bank);
    auto expectedRandom=random;expectedRandom.next();expectedRandom.next();expectedRandom.next();
    if(!emitted.birth || emitted.birth->actor.type!=48 || emitted.birth->poolSlot!=1
        || emitted.birth->activeEnemies!=pool.activeEnemies+1 || emitted.birth->spawnedEnemies!=15
        || emitted.rng.calls()!=3 || emitted.rng.last()!=expectedRandom.last()
        || emitted.visualRandomState!=0x29e2c0 || emitted.birth->actor.behaviorTimer!=41
        || emitted.birth->actor.position.x!=source.position.x || emitted.birth->actor.position.y!=source.position.y
        || emitted.birth->actor.position.z!=500 || emitted.birth->actor.yaw!=source.yaw
        || emitted.birth->actor.collisionRadius!=559 || emitted.birth->actor.yawVelocity!=0
        || emitted.birth->actor.forwardVelocity!=0
        || emitted.birth->actor.renderProfile!=WorldActorRenderProfile::inlineSelectorNoCallback) {
        std::cerr<<"Level2 AE10 provisional constructor/timer\n";return false;
    }
    auto full=pool;for(auto& slot:full.ordinaryPool) {slot.health=1;}
    const auto missed=level2GuardianActorEmissionOperations({source,full,random,1},bank);
    if(missed.birth || missed.rng.calls()!=0 || missed.visualRandomState!=1) {
        std::cerr<<"Level2 AE10 full-pool no-op\n";return false;
    }
    std::vector<Level1Entity> retained(16);retained[1].type=44;
    retained[1].renderProfile=WorldActorRenderProfile::heapSelectorDeferredAnimation;
    pool.ordinaryPool[1].hasAnimation=true;pool.retainedEntities=&retained;
    const auto reused=level2GuardianActorEmissionOperations({source,pool,random,1},bank);
    if(!reused.birth || reused.steps.size()!=3
        || std::get<Level1DeploymentHeapChild>(reused.steps[1]).kind!=Level1DeploymentHeapKind::releaseSelector) {
        std::cerr<<"Level2 AE10 retained selector release\n";return false;
    }
    Level2GuardianPulseInput pulse{source,{},16,{{Level1PresentationKind::entity,0}}};
    for(auto& fixed:pulse.fixedSlots) {
        fixed.provenance=Level1BonusFixedProvenance::reset;
        fixed.entity.renderProfile=WorldActorRenderProfile::noHandles;
    }
    auto& retainedFixed=pulse.fixedSlots[0];retainedFixed.provenance=Level1BonusFixedProvenance::retained;
    retainedFixed.entityIndex=7;retainedFixed.entity.stateBytes4dTo52={254,0,91,92,0,93};
    retainedFixed.entity.behaviorTimer=94;retainedFixed.entity.forwardVelocity=95;
    const auto born=level2GuardianPulseOperations(pulse,bank[46]);
    if(!born.birth || born.birth->entityIndex!=7 || born.birth->materializeEntity
        || born.birth->actor.type!=46 || born.birth->actor.collisionRadius!=2260
        || born.birth->actor.stateBytes4dTo52!=std::array<std::uint8_t,6>{254,100,91,92,0,93}
        || born.birth->actor.forwardVelocity!=95 || born.birth->actor.behaviorTimer!=94
        || born.birth->actor.position.x!=source.position.x || born.birth->actor.position.y!=source.position.y
        || born.allocationOrder.back()!=Level1ObjectRef{Level1PresentationKind::entity,7}) {
        std::cerr<<"Level2 pulse partial fixed reset\n";return false;
    }
    for(auto& fixed:pulse.fixedSlots) {fixed.entity.stateBytes4dTo52[1]=100;}
    const auto noPulse=level2GuardianPulseOperations(pulse,bank[46]);
    if(noPulse.birth || noPulse.allocationOrder!=pulse.allocationOrder) {
        std::cerr<<"Level2 pulse full-pool no-op\n";return false;
    }
    pulse.fixedSlots[1].provenance=Level1BonusFixedProvenance::unknown;
    return rejectsLevel2([&]{level2GuardianPulseOperations(pulse,bank[46]);});
}
}
int main(){const auto callback=retainedCallbackProfile();const auto release=retainedSelectorRelease();const auto local=level2ConstructorProfile();const auto deployment=level2DeploymentProfile();const auto cycle=level2GuardianCycle();const auto emissions=level2GuardianEmissions();if(!callback||!release||!local||!deployment||!cycle||!emissions){return 1;}using namespace deployment_fixture;for(const auto& c:cases){const auto w=run(c);if(!same(w.steps,c.expected)){std::cerr<<"work "<<c.label<<'\n';return 1;}if(normalize(w.actor)!=normalize(c.actor)){std::cerr<<"actor "<<c.label<<'\n';return 1;}if(normalize(w.shot)!=normalize(c.shot)){std::cerr<<"shot "<<c.label<<'\n';return 1;}}}
