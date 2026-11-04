#include "arrival_work_fixtures.hpp"
namespace {
using namespace arrival_fixture;
constexpr std::uint8_t level2Limit=12;

Level1ArrivalInput level2Input(const ArrivalCase& control) {
    auto result=input(control);
    result.pool.spawnedEnemies+=level2Limit-result.pool.enemyLimit;
    result.pool.enemyLimit=level2Limit;
    return result;
}

void sameRng(StellarRng left,StellarRng right) {
    assert(left.calls()==right.calls() && left.indexA()==right.indexA()
        && left.indexB()==right.indexB() && left.last()==right.last());
    for(std::size_t index=0;index<2*StellarRng::wordCount;++index) {assert(left.next()==right.next());}
}

void unchanged(const Level1ArrivalInput& before,const Level1ArrivalInput& after) {
    assert(before.enemyFlag==after.enemyFlag && before.craftEntityIndex==after.craftEntityIndex
        && before.guardianEntityIndex==after.guardianEntityIndex);
    assert(record(before.player,0x7000)==record(after.player,0x7000));
    const auto& left=before.pool;const auto& right=after.pool;
    assert(left.activeEnemies==right.activeEnemies && left.spawnedEnemies==right.spawnedEnemies
        && left.enemyLimit==right.enemyLimit && left.allocationCount==right.allocationCount
        && left.nextEntityIndex==right.nextEntityIndex && left.selectorReuse==right.selectorReuse
        && left.retainedEntities==right.retainedEntities);
    for(std::size_t index=0;index<left.ordinaryPool.size();++index) {
        const auto& a=left.ordinaryPool[index];const auto& b=right.ordinaryPool[index];
        assert(a.health==b.health && a.hasAnimation==b.hasAnimation && a.entityIndex==b.entityIndex);
    }
}

void level2Controls() {
    for(const auto& control:arrivals) {
        auto incoming=level2Input(control);const auto before=incoming;
        const StellarRng rng(control.table,control.a,control.b,0,0);const auto originalRng=rng;
        const auto work=level2ArrivalOperations(incoming,rng,models());
        const auto expectedBirth=!control.after.empty();
        assert(work.birth.has_value()==expectedBirth && work.steps.size()==(expectedBirth ? 3U : 1U));
        auto expected=control.before;
        if(expectedBirth) {expected.front().count+=4;}
        assert(same(std::get<Ops>(work.steps.front()),expected));
        if(expectedBirth) {
            assert(work.outcome==Level1ArrivalOutcome::born);
            assert(same(std::get<Ops>(work.steps.back()),control.after));
            const auto& audio=std::get<Level1ArrivalAudioChild>(work.steps[1]);
            assert(audio.eventId==17 && audio.callerBegin==0x4705
                && audio.callerEnd==0x470f && audio.callerDispatches==4);
            assert(record(work.birth->actor,0x7000+control.slot*0x80)==control.actor);
            assert(work.birth->poolSlot==control.slot && work.birth->actor.allocationIndex==control.slot);
            assert(work.birth->appendedAllocationIndex==incoming.pool.allocationCount && work.birth->refreshRequested);
            assert(work.birth->activeEnemies==incoming.pool.activeEnemies+1
                && work.birth->spawnedEnemies==level2Limit+1 && work.rng.calls()==rng.calls()+2);
        } else {
            sameRng(work.rng,rng);
        }
        auto future=work.rng;
        for(const auto value:control.future) {assert(future.next()==value);}
        unchanged(before,incoming);sameRng(originalRng,rng);
        rejects([&]{level1ArrivalOperations(incoming,rng,models());});
    }
}

void level2GuardExits() {
    const auto expect=[](const Level1ArrivalInput& incoming,const Level1ArrivalOutcome outcome,
        const std::uint64_t instructions) {
        const auto before=incoming;const StellarRng rng;
        const auto work=level2ArrivalOperations(incoming,rng,{});
        assert(work.outcome==outcome && !work.birth && work.steps.size()==1);
        assert(same(std::get<Ops>(work.steps.front()),{{NormalWorkKind::instructions,instructions}}));
        sameRng(work.rng,rng);unchanged(before,incoming);
    };
    auto incoming=level2Input(arrivals[4]);incoming.enemyFlag=0;
    expect(incoming,Level1ArrivalOutcome::enemies,13);
    incoming.enemyFlag=1;incoming.craftEntityIndex=0;incoming.pool.spawnedEnemies=level2Limit+1;
    expect(incoming,Level1ArrivalOutcome::craftPresent,15);
    incoming.craftEntityIndex.reset();incoming.guardianEntityIndex=1;incoming.pool.spawnedEnemies=level2Limit+2;
    expect(incoming,Level1ArrivalOutcome::guardianPresent,17);
    incoming.guardianEntityIndex.reset();incoming.pool.spawnedEnemies=level2Limit-1;
    expect(incoming,Level1ArrivalOutcome::unfinished,20);
}

void level2PhysicalSlots() {
    const auto& control=arrivals[4];const StellarRng rng(control.table,control.a,control.b,0,0);
    auto fresh=level2Input(control);fresh.pool.ordinaryPool[0].entityIndex.reset();
    const auto born=level2ArrivalOperations(fresh,rng,models());
    assert(born.birth && born.birth->poolSlot==0 && born.birth->actor.allocationIndex==fresh.pool.nextEntityIndex);
    auto reused=level2Input(arrivals[5]);
    reused.pool.ordinaryPool[2].entityIndex=7;reused.pool.ordinaryPool[7].entityIndex.reset();
    const auto original=reused;
    const auto carried=level2ArrivalOperations(reused,rng,models());
    assert(carried.birth && carried.birth->poolSlot==2 && carried.birth->actor.allocationIndex==7);
    assert(carried.birth->activeEnemies==3 && carried.birth->spawnedEnemies==level2Limit+1);
    assert(carried.birth->actor.type==7 && carried.birth->actor.collisionRadius==13052
        && carried.birth->actor.renderProfile==WorldActorRenderProfile::inlineSelectorNoCallback);
    assert(carried.rng.calls()==rng.calls()+2);unchanged(original,reused);
}

void level2Rejections() {
    auto incoming=level2Input(arrivals[4]);const StellarRng rng;const auto resource=models();
    incoming.pool.enemyLimit=10;rejects([&]{level2ArrivalOperations(incoming,rng,resource);});
    incoming=level2Input(arrivals[4]);
    for(const auto total:{level2Limit+1,level2Limit+2,level2Limit+3}) {
        incoming.pool.spawnedEnemies=static_cast<std::int16_t>(total);
        rejects([&]{level2ArrivalOperations(incoming,rng,resource);});
    }
    incoming=level2Input(arrivals[4]);incoming.pool.ordinaryPool[0].hasAnimation=true;
    rejects([&]{level2ArrivalOperations(incoming,rng,resource);});
    incoming=level2Input(arrivals[4]);incoming.pool.selectorReuse=Level1SelectorReuseDomain::retainedType11;
    rejects([&]{level2ArrivalOperations(incoming,rng,resource);});
    std::vector<Level1Entity> retained;
    incoming=level2Input(arrivals[4]);incoming.pool.retainedEntities=&retained;
    rejects([&]{level2ArrivalOperations(incoming,rng,resource);});
    incoming=level2Input(arrivals[4]);
    for(auto& slot:incoming.pool.ordinaryPool) {slot.health=1;}
    rejects([&]{level2ArrivalOperations(incoming,rng,resource);});
    incoming=level2Input(arrivals[4]);incoming.pool.allocationCount=32;
    rejects([&]{level2ArrivalOperations(incoming,rng,resource);});
    incoming=level2Input(arrivals[4]);
    auto bad=resource;bad[7].collisionRadius=1147;
    rejects([&]{level2ArrivalOperations(incoming,rng,bad);});
    bad=resource;bad[7].flags=3;rejects([&]{level2ArrivalOperations(incoming,rng,bad);});
    bad=resource;bad[7].scaleShift=1;rejects([&]{level2ArrivalOperations(incoming,rng,bad);});
    bad=resource;bad[7].animationLimits={6};rejects([&]{level2ArrivalOperations(incoming,rng,bad);});
    rejects([&]{level1PostWavePeriodicOperations(incoming.pool,rng,resource);});
}
} //namespace

int main(){using namespace arrival_fixture;
 level2Controls();level2GuardExits();level2PhysicalSlots();level2Rejections();
 for(const auto& c:arrivals){const auto out=level1ArrivalOperations(input(c),StellarRng(c.table,c.a,c.b,0,0),models());
  assert(out.steps.size()==(out.birth?3U:1U));assert(same(std::get<Ops>(out.steps[0]),c.before));
  if(out.birth){assert(std::get<Level1ArrivalAudioChild>(out.steps[1]).eventId==17);assert(same(std::get<Ops>(out.steps[2]),c.after));assert(record(out.birth->actor,0x7000+c.slot*0x80)==c.actor);assert(out.birth->poolSlot==c.slot&&out.birth->actor.allocationIndex==c.slot);assert(out.birth->activeEnemies==c.slot+1&&out.birth->spawnedEnemies==c.spawned+1);}
  auto rng=out.rng;for(auto value:c.future){assert(rng.next()==value);}assert(out.rng.calls()==(out.birth?2U:0U));
 }
 for(const auto& c:type7){if(c.frontier){rejects([&]{level1Type7Operations(c.actor);});continue;}const auto out=level1Type7Operations(c.actor,c.guardian?std::optional<std::size_t>{1}:std::nullopt);assert(same(out.operations,c.operations));auto raw=record(out.actor,0x7000);raw[0x1c]=raw[0x1d]=0;assert(raw==c.expected);}
 auto c=input(arrivals[4]);auto rng=StellarRng{};auto resource=models();c.pool.ordinaryPool[0].hasAnimation=true;rejects([&]{level1ArrivalOperations(c,rng,resource);});c=input(arrivals[4]);for(auto& slot:c.pool.ordinaryPool){slot.health=1;}rejects([&]{level1ArrivalOperations(c,rng,resource);});c=input(arrivals[4]);resource[7].collisionRadius=1147;rejects([&]{level1ArrivalOperations(c,rng,resource);});
 c=input(arrivals[4]);for(const auto spawned:{10,11,12}){c.pool.spawnedEnemies=spawned;const auto out=level1PostWavePeriodicOperations(c.pool,rng,models());assert(out.rng.calls()==rng.calls()&&same(out.beforeProvider,{{NormalWorkKind::instructions,13}}));}
 c.pool.spawnedEnemies=11;rejects([&]{level1PeriodicSpawnOperations(c.pool,rng,models());});
 auto bad=type7.front().actor;bad.stateBytes4dTo52[5]=3;rejects([&]{level1Type7Operations(bad);});
}
