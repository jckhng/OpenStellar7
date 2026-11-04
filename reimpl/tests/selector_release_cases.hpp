#pragma once
#include "stellar7/campaign_loader.hpp"
#include "stellar7/level1_arrival_work.hpp"

namespace {
bool sameSourceWork(const std::vector<NormalWorkOperation>& a,const std::vector<NormalWorkOperation>& b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](const auto& x,const auto& y) {
        return x.kind==y.kind && x.count==y.count && x.value==y.value;
    });
}
struct SelectorFixture {
    std::vector<Level1Entity> entities;
    Level1PeriodicSpawnInput pool{1,3,10,5,6,{}};
    SelectorFixture() {
        Level1State level;level.reloadAfterFailure(StellarRng{});entities=level.entities();
        entities[0]=entities[1];entities[0].allocationIndex=0;entities[0].stateBytes4dTo52[1]=0;
        entities[0].primaryType11AnimationSelector=false;
        entities[0].renderProfile=WorldActorRenderProfile::heapSelectorDeferredAnimation;
        pool.ordinaryPool[0]={0,true,0};pool.ordinaryPool[1]={1,true,1};
        pool.selectorReuse=Level1SelectorReuseDomain::retainedType11;pool.retainedEntities=&entities;
    }
    SelectorFixture(const SelectorFixture&)=delete;
    SelectorFixture& operator=(const SelectorFixture&)=delete;
};
StellarRng selectorRng(const std::uint16_t choice) {
    std::array<std::uint16_t,StellarRng::wordCount> table{};
    table[55]=choice;table[54]=2567;table[53]=43981;return StellarRng(table,48,110,0,0);
}
Level1PeriodicSpawnInput strictSelectorInput(Level1PeriodicSpawnInput input) {
    input.selectorReuse=Level1SelectorReuseDomain::rejectAnimated;input.retainedEntities=nullptr;
    input.ordinaryPool[0].hasAnimation=false;return input;
}
void eraseSelectorSource(std::vector<NormalWorkOperation>& operations,const std::size_t cut) {
    assert(cut>0 && cut<operations.size());
    //43D4/CALL13AC +13AC..13C0;13C3..13CD, actualDCC6(FFFF),13D0..13DC,43D8.
    assert(operations[cut-1].kind==NormalWorkKind::instructions && operations[cut-1].count==2+10);
    assert(operations[cut].kind==NormalWorkKind::instructions && operations[cut].count==5+15+7+1);
    operations.erase(operations.begin()+cut-1,operations.begin()+cut+1);
}
void checkSelectorReleaseFamily() {
    for(const auto choice:{std::uint16_t{261},std::uint16_t{25605},std::uint16_t{43525}}) {
        SelectorFixture fixture;const auto rng=selectorRng(choice);
        rejects([&]{level1PeriodicSpawnOperations(fixture.pool,rng,models());});
        const auto released=level1RetainedPeriodicSpawnOperations(fixture.pool,rng,models());
        const auto strict=strictSelectorInput(fixture.pool);
        const auto fresh=level1PeriodicSpawnOperations(strict,rng,models());
        assert(released.spawn && fresh.spawn && released.selectorRelease);
        assert(released.spawn->actor.allocationIndex==0 && released.spawn->poolSlot==0);
        assert(record(released.spawn->actor)==record(fresh.spawn->actor));
        assert(released.selectorRelease->call.entityIndex==0 && released.selectorRelease->call.sourceEntry==0xdcca
            && !released.selectorRelease->call.guestElapsedQualified);
        auto body=released.beforeProvider;eraseSelectorSource(body,released.selectorRelease->operationOffset);
        assert(sameSourceWork(body,fresh.beforeProvider) && sameSourceWork(released.afterProvider,fresh.afterProvider));
        assert(released.provider.has_value()==(released.spawn->actor.type==11));
        auto a=released.rng,b=fresh.rng;
        for(std::size_t i=0;i<StellarRng::wordCount*2;++i){assert(a.next()==b.next());}
        assert(released.rng.calls()==fresh.rng.calls());
        FrameTailState state{16,{},0,0,{0,0,0},{{11,{}},{29,{}},{29,{}},{29,{}},{29,{}}}};
        FrameTailGuards guard{};guard.armor=600;guard.keyboardHead=guard.keyboardTail=0x1e;guard.missionActive=true;
        guard.periodic=FrameTailPeriodicInput{0,1,3,10,campaignSpawnThreshold(0,1),48,110,0};
        const auto tail=level1PeriodicCombatFrameTailOperations(state,guard,rng,fixture.pool,models());
        const auto oldTail=level1PeriodicCombatFrameTailOperations(state,guard,rng,strict,models());
        assert(tail.tail.selectorReleases.size()==1 && tail.tail.state.loopTick==17);
        const auto& cut=tail.tail.selectorReleases[0];
        assert(cut.call.entityIndex==0 && cut.segment==(released.provider
            ? Level1TailSelectorSegment::beforePeriodicProvider : Level1TailSelectorSegment::afterEntryFade));
        auto before=tail.beforePeriodicProvider,after=tail.tail.afterEntryFade;
        eraseSelectorSource(released.provider ? before : after,cut.operationOffset);
        assert(sameSourceWork(before,oldTail.beforePeriodicProvider) && sameSourceWork(after,oldTail.tail.afterEntryFade));
    }
    SelectorFixture fixture;const auto rng=selectorRng(261);
    auto invalid=fixture.pool;invalid.retainedEntities=nullptr;
    rejects([&]{level1RetainedPeriodicSpawnOperations(invalid,rng,models());});
    invalid=fixture.pool;invalid.ordinaryPool[0].entityIndex=2;
    rejects([&]{level1RetainedPeriodicSpawnOperations(invalid,rng,models());});
    fixture.entities[0].stateBytes4dTo52[1]=1;
    rejects([&]{level1RetainedPeriodicSpawnOperations(fixture.pool,rng,models());});
    fixture.entities[0].stateBytes4dTo52[1]=0;fixture.entities[0].type=44;
    rejects([&]{level1RetainedPeriodicSpawnOperations(fixture.pool,rng,models());});
    fixture.entities[0].type=3;
    rejects([&]{level1RetainedPeriodicSpawnOperations(fixture.pool,rng,models());});
    fixture.entities[0].type=7;fixture.entities[0].renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
    fixture.entities[0].primaryType11AnimationSelector=false;fixture.pool.ordinaryPool[0].hasAnimation=false;
    rejects([&]{level1RetainedPeriodicSpawnOperations(fixture.pool,rng,models());});
}
void checkArrivalSelectorRelease() {
    SelectorFixture fixture;fixture.entities[1].stateBytes4dTo52[1]=0;
    fixture.pool.ordinaryPool[1].health=0;fixture.pool.activeEnemies=0;
    fixture.pool.spawnedEnemies=10;fixture.pool.allocationCount=4;
    auto resources=models();resources[7].flags=1;resources[7].collisionRadius=13052;resources[7].animationLimits={4};
    const Level1ArrivalInput input{1,{},{},{},fixture.pool};const auto rng=selectorRng(2567);
    rejects([&]{level1ArrivalOperations(input,rng,resources);});
    const auto released=level1RetainedArrivalOperations(input,rng,resources);
    auto strict=input;strict.pool=strictSelectorInput(fixture.pool);
    const auto fresh=level1ArrivalOperations(strict,rng,resources);
    assert(released.birth && fresh.birth && released.steps.size()==5 && fresh.steps.size()==3);
    assert(released.birth->actor.type==7 && released.birth->actor.allocationIndex==0);
    assert(record(released.birth->actor)==record(fresh.birth->actor));
    assert(std::get<Level1SelectorReleaseCall>(released.steps[1]).entityIndex==0);
    assert(std::get<Level1ArrivalAudioChild>(released.steps[3]).eventId==17);
    auto before=std::get<std::vector<NormalWorkOperation>>(released.steps[0]);
    const auto& after=std::get<std::vector<NormalWorkOperation>>(released.steps[2]);
    const auto cut=before.size();before.insert(before.end(),after.begin(),after.end());eraseSelectorSource(before,cut);
    assert(sameSourceWork(before,std::get<std::vector<NormalWorkOperation>>(fresh.steps[0])));
    assert(sameSourceWork(std::get<std::vector<NormalWorkOperation>>(released.steps[4]),
        std::get<std::vector<NormalWorkOperation>>(fresh.steps[2])));
    assert(released.rng.calls()==fresh.rng.calls());
    auto a=released.rng,b=fresh.rng;for(std::size_t i=0;i<StellarRng::wordCount*2;++i){assert(a.next()==b.next());}
    FrameTailState state{16,{},0,0,{0,0,0},{{29,{}},{29,{}},{29,{}},{29,{}}}};
    FrameTailGuards guard{};guard.armor=600;guard.keyboardHead=guard.keyboardTail=0x1e;guard.missionActive=true;
    guard.periodic=FrameTailPeriodicInput{0,0,10,10,campaignSpawnThreshold(0,0),48,110,1};
    const Level1CombatEffectMembership membership{4,4,0};
    const auto tail=level1ArrivalFrameTailOperations(state,guard,rng,input,resources,membership);
    const auto oldTail=level1ArrivalFrameTailOperations(state,guard,rng,strict,resources,membership);
    assert(tail.periodic.tail.selectorReleases.size()==1 && tail.arrival && tail.arrival->birth);
    const auto& boundary=tail.periodic.tail.selectorReleases[0];
    assert(boundary.segment==Level1TailSelectorSegment::afterEntryFade && boundary.call.entityIndex==0);
    auto parent=tail.periodic.tail.afterEntryFade;eraseSelectorSource(parent,boundary.operationOffset);
    assert(sameSourceWork(parent,oldTail.periodic.tail.afterEntryFade));
}
}
