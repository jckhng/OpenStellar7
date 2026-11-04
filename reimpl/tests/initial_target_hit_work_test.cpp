#include "stellar7/target_hit_work.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>
using namespace stellar7;
struct Op { int kind;std::uint64_t count;std::uint32_t value; };
struct Control {
    const char* label;std::int16_t type;std::uint8_t health,category,callbackClass;
    std::uint32_t distance;std::int16_t priorType;std::uint16_t streak;std::uint8_t slot;
    std::vector<Level1ObjectRef> allocations;
    std::uint8_t healthAfter;std::int16_t priorAfter;std::uint16_t streakAfter;
    std::int16_t activeAfter;std::vector<Level1ObjectRef> allocationsAfter;
    std::vector<Op> operations;std::vector<std::uint8_t> effectBytes;
};
const std::vector<Control> controls{
    {"lift-lethal",11,1,255,3,0,0,0,0,{{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0}},0,11,1,1,{{Level1PresentationKind::entity,1},{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::projectile,0}},{{0,122,0},{2,6,0},{0,49,0},{-1,4,6},{0,8,0}},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,14,165,165,17,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
    {"lift-nonlethal",11,2,255,3,0,0,0,0,{{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0}},1,0,0,2,{{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0},{Level1PresentationKind::deathEffect,0}},{{0,55,0},{2,6,0},{0,9,0},{-1,4,5},{0,6,0}},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,1,165,165,12,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
    {"lift-repeat",11,1,255,3,0,11,1,0,{{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0}},0,11,2,1,{{Level1PresentationKind::entity,1},{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::projectile,0}},{{0,123,0},{2,6,0},{0,49,0},{-1,4,6},{0,8,0}},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,14,165,165,17,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
    {"lift-word-wrap",11,1,255,3,0,11,65535,0,{{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0}},0,11,0,1,{{Level1PresentationKind::entity,1},{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::projectile,0}},{{0,123,0},{2,6,0},{0,49,0},{-1,4,6},{0,8,0}},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,14,165,165,17,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
    {"lift-full-pool",11,1,255,3,0,0,0,8,{{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::deathEffect,1},{Level1PresentationKind::deathEffect,2},{Level1PresentationKind::deathEffect,3},{Level1PresentationKind::deathEffect,4},{Level1PresentationKind::deathEffect,5},{Level1PresentationKind::deathEffect,6},{Level1PresentationKind::deathEffect,7},{Level1PresentationKind::entity,1},{Level1PresentationKind::projectile,0},{Level1PresentationKind::entity,0}},0,11,1,1,{{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::deathEffect,1},{Level1PresentationKind::deathEffect,2},{Level1PresentationKind::deathEffect,3},{Level1PresentationKind::deathEffect,4},{Level1PresentationKind::deathEffect,5},{Level1PresentationKind::deathEffect,6},{Level1PresentationKind::deathEffect,7},{Level1PresentationKind::entity,1},{Level1PresentationKind::projectile,0}},{{0,263,0},{-1,4,6},{0,8,0}},{}},
};
Level1TargetHitInput input(const Control& c){
    Level1TargetHitInput out{};out.targetIndex=0;out.target.type=c.type;
    out.target.position={321,-654,0};out.target.presenterDistance=c.distance;
    out.target.stateBytes4dTo52[0]=c.category;out.target.stateBytes4dTo52[1]=c.health;
    out.target.stateBytes4dTo52[4]=c.callbackClass;out.bonus={c.priorType,c.streak};
    out.activeEnemies=2;out.allocationOrder=c.allocations;
    for(std::size_t i=0;i<out.effectAges.size();++i){out.effectAges[i]=i<c.slot?1:0;}
    return out;
}
std::vector<Op> canonical(const Level1TargetHitWork& work){
    std::vector<Op> out;
    for(const auto& step:work.steps){
        for(const auto& op:step.operations){
            const auto kind=static_cast<int>(op.kind);
            if(op.kind==NormalWorkKind::instructions&&!out.empty()&&out.back().kind==kind){out.back().count+=op.count;}
            else{out.push_back({kind,op.count,op.value});}
        }
        if(step.audio){out.push_back({-1,step.audio->callerDispatches,static_cast<std::uint32_t>(step.audio->sound)});}
    }
    return out;
}
std::vector<std::uint8_t> apply(const Level1TargetHitEffect& effect){
    std::vector<std::uint8_t> out(99,0xa5);out[0x4e]=0;
    const auto word=[&](std::size_t at,std::uint16_t value){out[at]=static_cast<std::uint8_t>(value);out[at+1]=static_cast<std::uint8_t>(value>>8U);};
    const auto dword=[&](std::size_t at,std::uint32_t value){word(at,static_cast<std::uint16_t>(value));word(at+2,static_cast<std::uint16_t>(value>>16U));};
    word(2,static_cast<std::uint16_t>(effect.type));out[0x4d]=effect.category;out[0x4e]=effect.age;
    dword(8,static_cast<std::uint32_t>(effect.position.x));dword(12,static_cast<std::uint32_t>(effect.position.y));
    dword(16,static_cast<std::uint32_t>(effect.position.z));out[0x51]=effect.endAge;word(0x1a,effect.callback);
    return out;
}
std::vector<Level1ObjectRef> apply(const Level1TargetHitInput& before,const Level1TargetHitWork& work){
    auto out=before.allocationOrder;
    if(work.effect){assert(work.effect->appendedAllocationIndex==out.size());out.push_back({Level1PresentationKind::deathEffect,work.effect->poolSlot});}
    if(work.removal){const auto& r=*work.removal;assert(r.lastIndex+1==out.size()&&out.back()==r.replacement);
        out.at(r.removedIndex)=out.back();out.pop_back();}
    return out;
}

template<class Action>void rejects(Action action){bool rejected=false;try{action();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
int main(){
    for(const auto& c:controls){
        const auto before=input(c);const auto work=level1TargetHitOperations(before,Level1TargetHitDomain::initialOrdinary);
        assert(work.targetHealth==c.healthAfter&&work.bonus.lastType==c.priorAfter&&work.bonus.consecutive==c.streakAfter);
        assert(work.activeEnemies==c.activeAfter&&work.destroyed==(c.healthAfter==0));
        assert(work.refreshRequested==(work.destroyed||c.slot<8)&&work.allocationCount==c.allocationsAfter.size());
        assert(apply(before,work)==c.allocationsAfter);
        assert(work.effect.has_value()==!c.effectBytes.empty());
        if(work.effect){assert(work.effect->poolSlot==c.slot&&apply(*work.effect)==c.effectBytes);}
        assert(work.removal.has_value()==work.destroyed);
        const auto actual=canonical(work);assert(actual.size()==c.operations.size());
        for(std::size_t i=0;i<actual.size();++i){const auto a=actual[i],b=c.operations[i];assert(a.kind==b.kind&&a.count==b.count&&a.value==b.value);}
        std::size_t audioCount=0;
        for(const auto& step:work.steps){if(!step.audio){continue;}++audioCount;const auto& request=*step.audio;
            assert(step.operations.empty()&&request.callerDispatches==4);
            assert(request.callerBegin==(work.destroyed?0x1238:0x127a));
            assert(request.callerEnd==(work.destroyed?0x1242:0x1284));}
        assert(audioCount==1&&before.target.stateBytes4dTo52[1]==c.health&&before.bonus.consecutive==c.streak);
    }
    auto good=input(controls[0]);auto bad=good;
    rejects([&]{level1TargetHitOperations(good);});
    bad.bonus={11,2};rejects([&]{level1TargetHitOperations(bad,Level1TargetHitDomain::initialOrdinary);});
    assert(bad.bonus.consecutive==2&&bad.target.stateBytes4dTo52[1]==1);
    bad=good;bad.target.type=12;rejects([&]{level1TargetHitOperations(bad,Level1TargetHitDomain::initialOrdinary);});
    bad=good;bad.target.stateBytes4dTo52[4]=1;rejects([&]{level1TargetHitOperations(bad,Level1TargetHitDomain::initialOrdinary);});
    rejects([&]{level1TargetHitOperations(good,static_cast<Level1TargetHitDomain>(99));});
}
