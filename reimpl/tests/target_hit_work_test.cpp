#include "stellar7/bonus_target_hit_work.hpp"
#include <algorithm>
#include <cstdio>
#include <array>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string_view>
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
    {"lethal-first",1,1,255,1,0,0,0,0,{{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0}},0,1,1,1,{{Level1PresentationKind::entity,1},{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::projectile,0}},{{0,122,0},{2,6,0},{0,49,0},{-1,4,6},{0,8,0}},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,14,165,165,17,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
    {"nonlethal",1,2,255,1,0,0,0,0,{{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0}},1,0,0,2,{{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0},{Level1PresentationKind::deathEffect,0}},{{0,55,0},{2,6,0},{0,9,0},{-1,4,5},{0,6,0}},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,1,165,165,12,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
    {"fixed-impact",29,100,254,0,0,11,2,0,{{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0}},99,11,2,2,{{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0},{Level1PresentationKind::deathEffect,0}},{{0,55,0},{2,6,0},{0,9,0},{-1,4,5},{0,6,0}},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,1,165,165,12,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
    {"fixed-lethal",29,1,254,0,0,11,2,0,{{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0}},0,29,2,2,{{Level1PresentationKind::entity,1},{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::projectile,0}},{{0,119,0},{2,6,0},{0,48,0},{-1,4,6},{0,8,0}},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,14,165,165,17,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
    {"lethal-repeat",1,1,255,1,0,1,1,0,{{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0}},0,1,2,1,{{Level1PresentationKind::entity,1},{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::projectile,0}},{{0,123,0},{2,6,0},{0,49,0},{-1,4,6},{0,8,0}},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,14,165,165,17,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
    {"lethal-word-wrap",1,1,255,1,0,1,65535,0,{{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0}},0,1,0,1,{{Level1PresentationKind::entity,1},{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::projectile,0}},{{0,123,0},{2,6,0},{0,49,0},{-1,4,6},{0,8,0}},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,14,165,165,17,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
    {"lethal-slot-seven",1,1,255,1,0,0,0,7,{{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0},{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::deathEffect,1},{Level1PresentationKind::deathEffect,2},{Level1PresentationKind::deathEffect,3},{Level1PresentationKind::deathEffect,4},{Level1PresentationKind::deathEffect,5},{Level1PresentationKind::deathEffect,6}},0,1,1,1,{{Level1PresentationKind::entity,1},{Level1PresentationKind::deathEffect,7},{Level1PresentationKind::projectile,0},{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::deathEffect,1},{Level1PresentationKind::deathEffect,2},{Level1PresentationKind::deathEffect,3},{Level1PresentationKind::deathEffect,4},{Level1PresentationKind::deathEffect,5},{Level1PresentationKind::deathEffect,6}},{{0,171,0},{2,6,0},{0,49,0},{-1,4,6},{0,8,0}},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,14,165,165,17,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
    {"lethal-full-pool-last",1,1,255,1,0,0,0,8,{{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::deathEffect,1},{Level1PresentationKind::deathEffect,2},{Level1PresentationKind::deathEffect,3},{Level1PresentationKind::deathEffect,4},{Level1PresentationKind::deathEffect,5},{Level1PresentationKind::deathEffect,6},{Level1PresentationKind::deathEffect,7},{Level1PresentationKind::entity,1},{Level1PresentationKind::projectile,0},{Level1PresentationKind::entity,0}},0,1,1,1,{{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::deathEffect,1},{Level1PresentationKind::deathEffect,2},{Level1PresentationKind::deathEffect,3},{Level1PresentationKind::deathEffect,4},{Level1PresentationKind::deathEffect,5},{Level1PresentationKind::deathEffect,6},{Level1PresentationKind::deathEffect,7},{Level1PresentationKind::entity,1},{Level1PresentationKind::projectile,0}},{{0,263,0},{-1,4,6},{0,8,0}},{}},
    {"nonlethal-full-pool",1,2,255,1,0,0,0,8,{{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::deathEffect,1},{Level1PresentationKind::deathEffect,2},{Level1PresentationKind::deathEffect,3},{Level1PresentationKind::deathEffect,4},{Level1PresentationKind::deathEffect,5},{Level1PresentationKind::deathEffect,6},{Level1PresentationKind::deathEffect,7},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0}},1,0,0,2,{{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::deathEffect,1},{Level1PresentationKind::deathEffect,2},{Level1PresentationKind::deathEffect,3},{Level1PresentationKind::deathEffect,4},{Level1PresentationKind::deathEffect,5},{Level1PresentationKind::deathEffect,6},{Level1PresentationKind::deathEffect,7},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0}},{{0,98,0},{-1,4,5},{0,6,0}},{}},
    {"lethal-bucket-edge",1,1,255,1,6500,0,0,0,{{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0}},0,1,1,1,{{Level1PresentationKind::entity,1},{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::projectile,0}},{{0,122,0},{2,6,0},{0,49,0},{-1,4,6},{0,8,0}},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,17,165,165,20,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
    {"lethal-bucket-five",1,1,255,1,32500,0,0,0,{{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0}},0,1,1,1,{{Level1PresentationKind::entity,1},{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::projectile,0}},{{0,122,0},{2,6,0},{0,49,0},{-1,4,6},{0,8,0}},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,29,165,165,32,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
    {"lethal-bucket-clamp",1,1,255,1,39000,0,0,0,{{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0}},0,1,1,1,{{Level1PresentationKind::entity,1},{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::projectile,0}},{{0,123,0},{2,6,0},{0,49,0},{-1,4,6},{0,8,0}},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,29,165,165,32,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
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
void survivingFixedStateBytes() {
    const auto fixture=std::find_if(controls.begin(),controls.end(),[](const auto& control) {
        return std::string_view(control.label)=="fixed-impact";
    });
    assert(fixture!=controls.end());
    auto retained=input(*fixture);
    retained.targetIndex=5;
    retained.target.allocationIndex=retained.targetIndex;
    retained.target.position={-4350,-8243,0};
    retained.target.presenterDistance=9874;
    retained.target.stateBytes4dTo52[1]=16;
    retained.target.stateBytes4dTo52[4]=0xf3;
    retained.bonus={11,1};
    retained.activeEnemies=3;
    retained.allocationOrder={{Level1PresentationKind::entity,6},{Level1PresentationKind::entity,1},
        {Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},
        {Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},
        {Level1PresentationKind::projectile,0},{Level1PresentationKind::projectile,4},
        {Level1PresentationKind::projectile,1},{Level1PresentationKind::projectile,2},
        {Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,3}};
    const Model3d unusedBonusModel{};
    const Level1TargetBonusContext context{{},7,44,unusedBonusModel};
    const auto check=[&](const auto& before) {
        const auto originalBytes=before.target.stateBytes4dTo52;
        const auto direct=level1TargetHitOperations(before);
        const auto planned=level1BonusTargetHitOperations(before,context);
        assert(!planned.bonus);
        for(const auto* work:{&direct,&planned.hit}) {
            assert(work->targetHealth==before.target.stateBytes4dTo52[1]-1);
            assert(!work->destroyed&&!work->removal);
            assert(work->activeEnemies==before.activeEnemies);
            assert(work->bonus.lastType==before.bonus.lastType&&work->bonus.consecutive==before.bonus.consecutive);
            assert(work->effect&&work->effect->poolSlot==0&&work->effect->appendedAllocationIndex==before.allocationOrder.size());
            assert(work->effect->position.x==before.target.position.x&&work->effect->position.y==before.target.position.y
                &&work->effect->position.z==before.target.position.z);
            assert(work->effect->age==1&&work->effect->endAge==12);
            auto expected=before.allocationOrder;expected.push_back({Level1PresentationKind::deathEffect,0});
            assert(apply(before,*work)==expected&&work->allocationCount==expected.size()&&work->refreshRequested);
            const auto actual=canonical(*work);assert(actual.size()==fixture->operations.size());
            for(std::size_t i=0;i<actual.size();++i) {
                const auto a=actual[i],b=fixture->operations[i];
                assert(a.kind==b.kind&&a.count==b.count&&a.value==b.value);
            }
        }
        assert(before.target.stateBytes4dTo52==originalBytes);
    };
    //Actual702 FE5/F3 impact:1176 jumps118A->126C before any +51 access.
    check(retained);
    for(const auto health:{2,16,127}) {
        for(const auto byte:{0,1,3,5,0xf3,0xff}) {
            auto independent=retained;
            independent.target.stateBytes4dTo52[1]=static_cast<std::uint8_t>(health);
            independent.target.stateBytes4dTo52[4]=static_cast<std::uint8_t>(byte);
            check(independent);
        }
    }
    for(const auto byte:{1,3,5,0xf3,0xff}) {
        auto lethal=retained;lethal.target.stateBytes4dTo52[1]=1;
        lethal.target.stateBytes4dTo52[4]=static_cast<std::uint8_t>(byte);
        rejects([&]{level1TargetHitOperations(lethal);});
        rejects([&]{level1BonusTargetHitOperations(lethal,context);});
    }
    auto ordinary=retained;ordinary.target.type=1;ordinary.target.stateBytes4dTo52[0]=255;
    rejects([&]{level1TargetHitOperations(ordinary);});
    auto level2=retained;level2.target.type=30;
    rejects([&]{level1TargetHitOperations(level2,Level1TargetHitDomain::level2Constructor);});
}
int main(){
    try { survivingFixedStateBytes(); }
    catch (const std::exception& error) {
        std::fprintf(stderr,"retained FE5 surviving impact: %s\n",error.what());
        return 1;
    }
    for(const auto& c:controls){
        const auto before=input(c);const auto work=level1TargetHitOperations(before);
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
    bad.bonus={1,2};rejects([&]{level1TargetHitOperations(bad);});assert(bad.bonus.consecutive==2&&bad.target.stateBytes4dTo52[1]==1);
    bad=good;bad.playerTarget=true;rejects([&]{level1TargetHitOperations(bad);});
    bad=good;bad.specialTarget=true;rejects([&]{level1TargetHitOperations(bad);});
    bad=good;bad.damage=5;rejects([&]{level1TargetHitOperations(bad);});
    bad=good;bad.target.type=11;rejects([&]{level1TargetHitOperations(bad);});
    bad=good;bad.target.stateBytes4dTo52[4]=5;rejects([&]{level1TargetHitOperations(bad);});
    bad=good;bad.target.stateBytes4dTo52[1]=128;rejects([&]{level1TargetHitOperations(bad);});
    bad=good;bad.target.stateBytes4dTo52[1]=0;rejects([&]{level1TargetHitOperations(bad);});
    bad=good;bad.allocationOrder[1]=bad.allocationOrder[0];rejects([&]{level1TargetHitOperations(bad);});
    bad=good;bad.allocationOrder.push_back({Level1PresentationKind::deathEffect,0});rejects([&]{level1TargetHitOperations(bad);});
    bad=good;bad.activeEnemies=0;rejects([&]{level1TargetHitOperations(bad);});
    bad=good;bad.target.presenterDistance=0x80000000U;rejects([&]{level1TargetHitOperations(bad);});
    //A full pool and surviving impact leave both allocation membership and prior refresh unchanged.
    auto full=input(controls[8]);const auto dropped=level1TargetHitOperations(full);assert(!dropped.effect&&!dropped.removal&&!dropped.refreshRequested);
}
