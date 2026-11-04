#include "stellar7/frame_tail_work.hpp"
#include "stellar7/weapon_lamp_work.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif
namespace {
using namespace stellar7;
template<class Action>void rejects(Action action){bool rejected=false;try{action();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
void same(const std::vector<NormalWorkOperation>& a,const std::vector<NormalWorkOperation>& b){
    assert(a.size()==b.size());for(std::size_t i=0;i<a.size();++i){assert(a[i].kind==b[i].kind&&a[i].count==b[i].count&&a[i].value==b[i].value);}}
void sameRng(StellarRng a,StellarRng b){assert(a.calls()==b.calls()&&a.indexA()==b.indexA()&&a.indexB()==b.indexB()&&a.last()==b.last());
    for(unsigned i=0;i<112;++i){assert(a.next()==b.next());}}
Level1CombatEffectMembership membership(std::size_t count,std::size_t entities,std::size_t effects,std::size_t bonuses){
    return {count,entities,effects,Level1CombatEffectProfile::systemBonus,bonuses};}
FrameTailState state(){FrameTailState out{2,{100,-200,0},0,0,{},{}};
    for(const auto type:{1,11,29,29,29,29}){out.actors.push_back({static_cast<std::int16_t>(type),{1000,2000,0}});}return out;}
FrameTailGuards guards(){FrameTailGuards out{};out.armor=600;out.keyboardHead=out.keyboardTail=0x1e;out.missionActive=true;return out;}
Level1PeriodicSpawnInput periodic(std::uint16_t count,std::size_t next=7){Level1PeriodicSpawnInput out{2,2,10,count,next,{}};
    out.ordinaryPool[0]={1,false,0};out.ordinaryPool[1]={1,false,1};return out;}
FrameTailState bonusState(){auto out=state();out.actors.push_back({42,{40000,-40000,100}});return out;}
Level1PrimaryFireInput fireInput(const Level1CombatEffectMembership context){Level1PrimaryFireInput out{};
    out.player.position=state().playerPosition;out.model={256,0,134};out.allocationCount=context.allocationCount;
    out.membership=Level1PrimaryFireMembership::combatEffects;out.combatEffects=context;
    for(std::size_t i=0;i<out.pool.size();++i){out.pool[i].poolSlot=i;out.pool[i].stateBytes4dTo54[0]=253;}return out;}
std::vector<Model3d> models(){std::vector<Model3d> out(44);for(const auto type:{1,3,11}){
    out[type].flags=1;out[type].collisionRadius=type==1?1342:type==3?770:748;}out[11].animationLimits={8};return out;}
StellarRng spawnRng(){std::array<std::uint16_t,56> table{};table[53]=0xabcd;table[54]=0x0a07;table[55]=0x0105;
    return {table,48,110,0,0};} //Existing source-qualified Type1 periodic control input.
void periodicGuards(FrameTailGuards& g,const StellarRng& rng,const std::int16_t spawned){
    g.periodic=FrameTailPeriodicInput{0,2,spawned,10,10,rng.indexA(),rng.indexB(),std::uint8_t{0}};}
void traversal(){
    const auto rng=StellarRng{};const auto g=guards();const auto base=state();
    //Explicit profile with no bonus preserves the old default operation stream.
    const auto ordinary=level1CombatEffectFrameTailOperations(base,g,rng,periodic(6),{}, {6,6,0});
    const auto enabled=level1CombatEffectFrameTailOperations(base,g,rng,periodic(6),{},membership(6,6,0,0));
    same(ordinary.tail.afterEntryFade,enabled.tail.afterEntryFade);sameRng(ordinary.tail.rng,enabled.tail.rng);
    const auto born=bonusState();
    const auto birth=level1CombatEffectFrameTailOperations(born,g,rng,periodic(7),{},membership(7,7,0,1));
    assert(birth.tail.state.actors.size()==7&&birth.tail.state.actors.back().type==42);
    assert((birth.tail.state.actors.back().position==FixedPosition{39900,-39800,100}));
    //0C6A has no type/category branch. Reuse the already qualified terminal-FC path.
    auto fc=born;fc.actors.back().type=-1;
    const auto sourceEquivalent=level1CombatEffectFrameTailOperations(fc,g,rng,periodic(7),{}, {7,6,1});
    same(birth.tail.afterEntryFade,sourceEquivalent.tail.afterEntryFade);sameRng(birth.tail.rng,rng);
    rejects([&]{level1CombatEffectFrameTailOperations(born,g,rng,periodic(7),{}, {7,7,0});});
    rejects([&]{level1PeriodicCombatFrameTailOperations(born,g,rng,periodic(7),{});});

    //A terminal old42 remains independent of live registry membership after pickup.
    const auto terminal=level1CombatEffectFrameTailOperations(born,g,rng,periodic(6),{},membership(6,6,0,0));
    same(birth.tail.afterEntryFade,terminal.tail.afterEntryFade);
    assert(terminal.tail.state.actors.back().type==42&&terminal.tail.state.actors.size()==7);
    auto picked=terminal.tail.state;picked.loopTick=3;picked.actors.pop_back();
    const auto afterPickup=level1CombatEffectFrameTailOperations(picked,g,rng,periodic(6),{},membership(6,6,0,0));
    assert(afterPickup.tail.state.actors.size()==6&&!afterPickup.periodic);sameRng(afterPickup.tail.rng,rng);

    auto oldFC=born;oldFC.actors.push_back({-1,{777,888,0}});
    const auto both=level1CombatEffectFrameTailOperations(oldFC,g,rng,periodic(7),{},membership(7,7,0,1));
    assert(both.tail.state.actors.size()==8&&both.tail.state.actors.back().type==-1);
    assert((both.tail.state.actors.back().position==FixedPosition{677,1088,0}));
    assert(both.tail.audioEvents.empty()); //No main/radar callback or aging is added.

    auto wrapped=born;wrapped.loopTick=16;auto cap=periodic(7);cap.spawnedEnemies=10;
    auto pg=g;periodicGuards(pg,rng,10);
    const auto normalized=level1CombatEffectFrameTailOperations(wrapped,pg,rng,cap,{},membership(7,7,0,1));
    assert((normalized.tail.state.actors.back().position==FixedPosition{-25636,25736,100}));
    assert(normalized.periodic&&normalized.periodic->outcome==Level1PeriodicSpawnOutcome::limit);
    sameRng(normalized.tail.rng,rng);
    auto next=normalized.tail.state;next.loopTick=17;next.playerPosition={50,-75,0};
    const auto adjacent=level1CombatEffectFrameTailOperations(next,g,normalized.tail.rng,periodic(7),{},membership(7,7,0,1));
    assert((adjacent.tail.state.actors.back().position==FixedPosition{-25686,25811,100}));
    sameRng(adjacent.tail.rng,rng);
}
void capacity(){const auto g=guards();const auto rng=StellarRng{};auto maximum=state();maximum.actors.clear();
    for(unsigned i=0;i<10;++i){maximum.actors.push_back({1,{}});}for(unsigned i=0;i<4;++i){maximum.actors.push_back({29,{}});}
    maximum.actors.push_back({42,{}});for(unsigned i=0;i<8;++i){maximum.actors.push_back({23,{}});maximum.actors.push_back({-1,{}});}
    auto input=periodic(31,15);input.activeEnemies=input.spawnedEnemies=10;
    const auto full=level1CombatEffectFrameTailOperations(maximum,g,rng,input,{},membership(31,15,8,1));assert(full.tail.state.actors.size()==31);
    rejects([&]{level1CombatEffectFrameTailOperations(maximum,g,rng,input,{}, {31,15,8});});
    for(const auto unsupported:{1,29,42,23,-1}){auto bad=maximum;bad.actors.push_back({static_cast<std::int16_t>(unsupported),{}});
        rejects([&]{level1CombatEffectFrameTailOperations(bad,g,rng,input,{},membership(31,15,8,1));});}
    auto wrong=membership(31,15,8,2);rejects([&]{level1CombatEffectFrameTailOperations(maximum,g,rng,input,{},wrong);});
    wrong=membership(31,15,8,1);wrong.profile=static_cast<Level1CombatEffectProfile>(255);
    rejects([&]{level1CombatEffectFrameTailOperations(maximum,g,rng,input,{},wrong);});
    input.nextEntityIndex=16;rejects([&]{level1CombatEffectFrameTailOperations(maximum,g,rng,input,{},membership(31,15,8,1));});
}
void fireAndPeriodic(){
    const auto ctx=membership(7,7,0,1);auto in=fireInput(ctx);const auto fired=level1PrimaryFireOperations(in);
    assert(fired.launch&&fired.launch->allocationIndex==7&&fired.allocationCount==8);
    auto old=in;old.membership=Level1PrimaryFireMembership::initialSix;old.combatEffects.reset();
    rejects([&]{level1PrimaryFireOperations(old);});old.allocationCount=6;
    const auto legacy=level1PrimaryFireOperations(old);same(fired.beforeEvent14,legacy.beforeEvent14);
    same(fired.afterEvent14BeforeLamp,legacy.afterEvent14BeforeLamp);same(fired.afterLamp,legacy.afterLamp);
    const Level1TailFireAction action{fired,weaponLampOperations(0,WeaponLampState::unavailable,{},CockpitPageState{}).operations};
    auto g=guards();g.heldKeys[8]=true;const auto s=bonusState();
    const auto result=level1CombatEffectFrameTailOperations(s,g,StellarRng{},periodic(8),{},ctx,action);
    assert(result.tail.state.actors.size()==s.actors.size()); //New44BA shot is absent from old recenter list.
    assert((fired.launch->projectile.position==s.playerPosition));
    assert((result.tail.state.weaponTimers==std::array<std::uint8_t,3>{8,31,0}));
    assert(std::count_if(result.tail.afterEntryFade.begin(),result.tail.afterEntryFade.end(),[](const auto& op){
        return op.kind==NormalWorkKind::gameplayPrimaryFireEvent&&op.count==1&&op.value==14;})==1);
    rejects([&]{level1CombatEffectFrameTailOperations(s,g,StellarRng{},periodic(7),{},ctx,action);});
    auto wrong=ctx;wrong.profile=Level1CombatEffectProfile::ordinary;in.combatEffects=wrong;
    rejects([&]{level1PrimaryFireOperations(in);});
    wrong=membership(15,15,0,1);in=fireInput(wrong);assert(level1PrimaryFireOperations(in).allocationCount==16);
    wrong.bonusAllocations=0;in.combatEffects=wrong;rejects([&]{level1PrimaryFireOperations(in);});
    wrong=membership(7,7,0,1);wrong.profile=static_cast<Level1CombatEffectProfile>(255);in=fireInput(wrong);
    rejects([&]{level1PrimaryFireOperations(in);});

    //Source0332 recenter precedes033C periodic append; neither new birth is oldlisted.
    auto periodicState=s;periodicState.loopTick=16;const auto rng=spawnRng();periodicGuards(g,rng,2);
    auto pi=periodic(8);const auto shared=models();
    const auto expected=level1PeriodicSpawnOperations(pi,rng,shared);
    const auto combined=level1CombatEffectFrameTailOperations(periodicState,g,rng,pi,shared,ctx,action);
    assert(combined.periodic&&combined.periodic->spawn&&expected.spawn);
    assert(combined.periodic->spawn->appendedAllocationIndex==8&&combined.periodic->spawn->actor.allocationIndex==7);
    assert(combined.periodic->spawn->actor.position==expected.spawn->actor.position);
    assert(combined.tail.state.actors.size()==s.actors.size());sameRng(combined.tail.rng,expected.rng);
    auto outOfRange=pi;outOfRange.nextEntityIndex=15;
    rejects([&]{level1CombatEffectFrameTailOperations(periodicState,g,rng,outOfRange,shared,ctx,action);});
    outOfRange.ordinaryPool[2].entityIndex=14;
    const auto reused=level1CombatEffectFrameTailOperations(periodicState,g,rng,outOfRange,shared,ctx,action);
    assert(reused.periodic&&reused.periodic->spawn&&reused.periodic->spawn->actor.allocationIndex==14);
}
}
int main(){
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    try{traversal();capacity();fireAndPeriodic();}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}return 0;
}
