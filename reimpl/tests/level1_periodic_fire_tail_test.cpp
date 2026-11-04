#include "stellar7/frame_tail_work.hpp"
#include "stellar7/weapon_lamp_work.hpp"
#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <utility>
using namespace stellar7;
template<class Action>void rejects(Action action){bool rejected=false;try{action();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}

FrameTailState state(unsigned ordinary, unsigned shots) {
    FrameTailState out{2,{},0,0,{},{{1,{}},{11,{}}}};
    for(unsigned i=2;i<ordinary;++i){out.actors.push_back({1,{}});}
    for(unsigned i=0;i<4;++i){out.actors.push_back({29,{}});}
    for(unsigned i=0;i<shots;++i){out.actors.push_back({23,{}});}
    return out;
}
Level1TailFireAction fireAction(unsigned occupied,std::size_t allocationCount) {
    Level1PrimaryFireInput input{};input.model={256,0,134};input.allocationCount=6+occupied;
    for(std::size_t i=0;i<input.pool.size();++i){
        auto& shot=input.pool[i];shot.poolSlot=i;shot.stateBytes4dTo54[0]=253;
        if(i<occupied){shot.stateBytes4dTo54[1]=64;input.allocatedPoolSlots.push_back(i);}
    }
    auto work=level1PrimaryFireOperations(input);
    assert(work.launch);
    //The source44BA append work is independent of the existing registry count.
    //Rebind only the controlled registry identity; retain every source-written field.
    work.allocationCount=allocationCount;work.launch->allocationIndex=allocationCount-1;
    return {std::move(work),weaponLampOperations(0,WeaponLampState::unavailable,{},CockpitPageState{}).operations};
}
Level1PeriodicSpawnInput periodic(unsigned ordinary,std::size_t count) {
    Level1PeriodicSpawnInput out{static_cast<std::int16_t>(ordinary),static_cast<std::int16_t>(ordinary),10,
        static_cast<std::uint16_t>(count),16,{}};
    for(std::size_t i=0;i<out.ordinaryPool.size();++i){out.ordinaryPool[i].health=i<ordinary?1:0;out.ordinaryPool[i].entityIndex=i;}
    return out;
}
FrameTailGuards guards(){FrameTailGuards out{};out.armor=600;out.keyboardHead=out.keyboardTail=0x1e;
    out.missionActive=true;out.heldKeys[8]=true;return out;}
int main(){
    const auto rng=StellarRng{};const auto g=guards();
    for(const auto ordinary:{3U,10U}){
        const auto s=state(ordinary,7);const auto count=s.actors.size()+1;
        const auto fire=fireAction(7,count);const auto input=periodic(ordinary,count);
        const auto work=level1PeriodicCombatFrameTailOperations(s,g,rng,input,{},fire);
        assert(!work.periodic&&work.tail.state.actors.size()==s.actors.size());
        assert(work.tail.state.weaponTimers==(std::array<std::uint8_t,3>{8,31,0}));
        assert(work.tail.rng.calls()==rng.calls());
        const auto markers=std::count_if(work.tail.afterEntryFade.begin(),work.tail.afterEntryFade.end(),[](const auto& op){
            return op.kind==NormalWorkKind::gameplayPrimaryFireEvent&&op.count==1&&op.value==14;});
        assert(markers==1);
        auto bad=input;--bad.allocationCount;
        rejects([&]{level1PeriodicCombatFrameTailOperations(s,g,rng,bad,{},fire);});
        auto forged=fire;++forged.work.allocationCount;++forged.work.launch->allocationIndex;
        rejects([&]{level1PeriodicCombatFrameTailOperations(s,g,rng,input,{},forged);});
        rejects([&]{level1CombatFrameTailOperations(s,g,rng,fire);});
    }
    auto s=state(3,0);s.weaponTimers={2,0,0};auto input=periodic(3,7);
    Level1TailFireAction cooldown{{Level1PrimaryFireOutcome::cooldown,{},2,{0,0},7,false,{},
        {{NormalWorkKind::instructions,14}},{},{}},{}};
    const auto result=level1PeriodicCombatFrameTailOperations(s,g,rng,input,{},cooldown);
    assert(result.tail.state.weaponTimers[0]==1);
    cooldown.work.allocationCount=6;
    rejects([&]{level1PeriodicCombatFrameTailOperations(s,g,rng,input,{},cooldown);});
}
