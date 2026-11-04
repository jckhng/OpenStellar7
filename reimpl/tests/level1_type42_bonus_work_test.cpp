#include "stellar7/type42_bonus_work.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>
using namespace stellar7;
struct Control { const char* label;std::size_t free;bool retained;std::uint16_t streak;std::int16_t prior;std::int16_t sourceType=1;std::uint16_t capacity=44; };
const std::array<Control,10> controls{{
    {"third-first-fresh",0,false,2,1},{"third-loader-four-fresh",4,false,2,1},
    {"third-retained-slot-two",2,true,2,1},{"third-last-fixed-slot",15,true,2,1},
    {"third-full-fixed-pool",16,false,2,1},{"second-no-emission",4,false,1,1},
    {"word-wrap-no-emission",4,false,65535,1},{"different-type-no-emission",4,false,2,11},
    {"lift-third-loader-four-fresh",4,false,2,11,11},
    {"tank-third-native-registry256",4,false,2,1,1,256}}};
using Raw=std::array<std::uint8_t,99>;
void put(Raw& body,const std::size_t at,const std::uint32_t value,const std::size_t size) {
    for(std::size_t i=0;i<size;++i){body[at+i]=static_cast<std::uint8_t>(value>>(8*i));}
}
std::uint32_t get(const Raw& body,const std::size_t at,const std::size_t size) {
    std::uint32_t value=0;for(std::size_t i=0;i<size;++i){value|=std::uint32_t{body[at+i]}<<(8*i);}return value;
}
Raw rawFixed(const Control& c,const std::size_t slot) {
    Raw body{};if(c.retained){body.fill(0xa5);}
    put(body,2,c.retained||slot<c.free?29:0,2);put(body,0x1a,0,2);
    body[0x4d]=254;body[0x4e]=slot<c.free?1:0;return body;
}
Level1Entity decode(const Raw& body,const std::size_t identity) {
    Level1Entity e{};e.allocationIndex=identity;e.flags=body[0];e.type=static_cast<std::int16_t>(get(body,2,2));
    e.collisionRadius=get(body,4,4);e.position={static_cast<std::int32_t>(get(body,8,4)),static_cast<std::int32_t>(get(body,12,4)),static_cast<std::int32_t>(get(body,16,4))};
    e.pitch=static_cast<std::uint16_t>(get(body,0x14,2));e.roll=static_cast<std::uint16_t>(get(body,0x16,2));e.yaw=static_cast<std::uint16_t>(get(body,0x18,2));
    e.pitchVelocity=static_cast<std::int16_t>(get(body,0x30,2));e.rollVelocity=static_cast<std::int16_t>(get(body,0x32,2));e.yawVelocity=static_cast<std::int16_t>(get(body,0x34,2));
    e.xVelocity=static_cast<std::int16_t>(get(body,0x36,2));e.yVelocity=static_cast<std::int16_t>(get(body,0x38,2));e.zVelocity=static_cast<std::int16_t>(get(body,0x3a,2));e.forwardVelocity=static_cast<std::int16_t>(get(body,0x3c,2));
    for(std::size_t i=0;i<6;++i){e.stateBytes4dTo52[i]=body[0x4d+i];}
    e.behaviorTimer=body[0x54];e.presenterDistance=get(body,0x49,4);e.targetHeading=static_cast<std::uint16_t>(get(body,0x57,2));
    e.weaponSlotCount=body[0x59];e.weaponCooldown=body[0x5a];
    for(std::size_t i=0;i<3;++i){e.weaponSlotTimers[i]=body[0x5b+i];}
    e.collisionContactAllocationIndex=get(body,0x5f,2);e.ownerAllocationIndex=get(body,0x61,2);
    e.renderProfile=WorldActorRenderProfile::noHandles;e.animatedModelPresenterEnabled=false;return e;
}
Raw encode(Raw body,const Level1Type42BonusReceipt& birth) {
    const auto& e=birth.entity;put(body,0,e.flags,1);put(body,2,static_cast<std::uint16_t>(e.type),2);put(body,4,e.collisionRadius,4);
    put(body,8,e.position.x,4);put(body,12,e.position.y,4);put(body,16,e.position.z,4);
    put(body,0x14,e.pitch,2);put(body,0x16,e.roll,2);put(body,0x18,e.yaw,2);
    put(body,0x1a,birth.drawCallback,2);put(body,0x1c,0x8000+birth.poolSlot*0x80+birth.inlineSelectorOffset,2);
    put(body,0x30,static_cast<std::uint16_t>(e.pitchVelocity),2);put(body,0x32,static_cast<std::uint16_t>(e.rollVelocity),2);put(body,0x34,static_cast<std::uint16_t>(e.yawVelocity),2);
    put(body,0x36,static_cast<std::uint16_t>(e.xVelocity),2);put(body,0x38,static_cast<std::uint16_t>(e.yVelocity),2);put(body,0x3a,static_cast<std::uint16_t>(e.zVelocity),2);put(body,0x3c,static_cast<std::uint16_t>(e.forwardVelocity),2);
    for(std::size_t i=0;i<6;++i){body[0x4d+i]=e.stateBytes4dTo52[i];}
    body[0x54]=e.behaviorTimer;put(body,0x49,e.presenterDistance,4);put(body,0x57,e.targetHeading,2);
    body[0x59]=e.weaponSlotCount;body[0x5a]=e.weaponCooldown;for(std::size_t i=0;i<3;++i){body[0x5b+i]=e.weaponSlotTimers[i];}
    put(body,0x5f,static_cast<std::uint16_t>(e.collisionContactAllocationIndex),2);put(body,0x61,static_cast<std::uint16_t>(e.ownerAllocationIndex),2);return body;
}
Level1Type42BonusInput input(const Control& c) {
    Level1Type42BonusInput in{};in.source.type=c.sourceType;in.source.stateBytes4dTo52[0]=255;in.source.stateBytes4dTo52[4]=c.sourceType==11?3:1;in.source.allocationIndex=0;in.source.position={123456,-234567,345678};
    in.history={c.prior,c.streak};in.nextEntityIndex=19;
    in.allocationOrder={{Level1PresentationKind::entity,0},{Level1PresentationKind::entity,1}};
    for(std::size_t i=0;i<16;++i){auto& slot=in.fixedSlots[i];slot.provenance=c.retained?Level1BonusFixedProvenance::retained:Level1BonusFixedProvenance::reset;
        if(i<c.free||c.retained){slot.entityIndex=3+i;}
        slot.entity=decode(rawFixed(c,i),slot.entityIndex.value_or(0));
        if(i<c.free){in.allocationOrder.push_back({Level1PresentationKind::entity,3+i});}
    }
    in.allocationOrder.push_back({Level1PresentationKind::projectile,0});return in;
}
Model3d model42(){Model3d model{};model.collisionRadius=330;return model;}
std::vector<NormalWorkOperation> canonical(const std::vector<NormalWorkOperation>& operations) {
    std::vector<NormalWorkOperation> out;for(const auto op:operations){
        if(op.kind==NormalWorkKind::instructions&&!out.empty()&&out.back().kind==op.kind){out.back().count+=op.count;}
        else{out.push_back(op);}}
    return out;
}

#include <cassert>
#include <iostream>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif
namespace {
template<class Action>void rejects(Action action){bool rejected=false;try{action();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
void verify(const Control& c){
    const auto before=input(c);const auto work=level1Type42BonusOperations(before,c.capacity,model42());
    const auto payload=c.sourceType==11?3:7;
    const auto emitted=c.prior==c.sourceType&&c.streak==2;
    const auto born=emitted&&c.free<16;
    assert(work.history.lastType==c.sourceType&&work.history.consecutive==(emitted?0:c.prior==c.sourceType?static_cast<std::uint16_t>(c.streak+1U):1));
    assert(static_cast<bool>(work.birth)==born&&work.allocationOrder.size()==before.allocationOrder.size()+born);
    assert(work.refreshRequested==born);
    assert(std::equal(before.allocationOrder.begin(),before.allocationOrder.end(),work.allocationOrder.begin()));
    const auto ops=canonical(work.operations);
    if(born){
        assert(ops.size()==3&&ops[0].kind==NormalWorkKind::instructions&&ops[0].count==129+7*c.free);
        assert(ops[1].kind==NormalWorkKind::repeatString&&ops[1].count==6&&ops[2].count==22);
        const auto& receipt=*work.birth;const auto& entity=receipt.entity;const auto& prior=before.fixedSlots[c.free].entity;
        assert(receipt.poolSlot==c.free&&receipt.entityIndex==(c.retained?3+c.free:19));
        assert(receipt.appendedAllocationIndex==before.allocationOrder.size()&&receipt.materializeEntity==!c.retained);
        assert((entity.position==FixedPosition{123456,-234567,100}));
        assert(entity.type==42&&entity.flags==0&&entity.collisionRadius==330&&entity.pitch==0&&entity.roll==0&&entity.yaw==0);
        assert(entity.stateBytes4dTo52[0]==254&&entity.stateBytes4dTo52[1]==1&&entity.stateBytes4dTo52[2]==payload&&entity.stateBytes4dTo52[4]==0&&entity.stateBytes4dTo52[5]==payload);
        assert(entity.stateBytes4dTo52[3]==prior.stateBytes4dTo52[3]);
        assert(entity.pitchVelocity==prior.pitchVelocity&&entity.rollVelocity==0x800&&entity.yawVelocity==0x800);
        assert(entity.xVelocity==prior.xVelocity&&entity.yVelocity==prior.yVelocity&&entity.zVelocity==prior.zVelocity&&entity.forwardVelocity==prior.forwardVelocity);
        assert(entity.presenterDistance==prior.presenterDistance&&entity.targetHeading==prior.targetHeading&&entity.behaviorTimer==prior.behaviorTimer);
        assert(entity.weaponSlotCount==prior.weaponSlotCount&&entity.weaponCooldown==prior.weaponCooldown&&entity.weaponSlotTimers==prior.weaponSlotTimers);
        assert(entity.collisionContactAllocationIndex==prior.collisionContactAllocationIndex&&entity.ownerAllocationIndex==prior.ownerAllocationIndex);
        assert(entity.modelAnimationVariant==payload&&!entity.animatedModelPresenterEnabled&&entity.renderProfile==WorldActorRenderProfile::inlineSelectorNoCallback);
    }else{assert(ops.size()==1&&ops[0].count==(emitted?171:c.prior==c.sourceType?20:19));}
}
}
int main(){
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    try{
        for(const auto& c:controls){verify(c);}
        verify({"lift-second-no-emission",4,false,1,11,11});
        auto in=input(controls[1]);in.fixedSlots[4].provenance=Level1BonusFixedProvenance::unknown;
        rejects([&]{level1Type42BonusOperations(in,44,model42());});
        in=input(controls[1]);in.fixedSlots[8].provenance=Level1BonusFixedProvenance::unknown;
        assert(level1Type42BonusOperations(in,44,model42()).birth); //Unknown unvisited record is immaterial.
        in=input(controls[2]);in.fixedSlots[2].drawCallback=0x130c;
        rejects([&]{level1Type42BonusOperations(in,44,model42());});
        in=input(controls[2]);in.allocationOrder.push_back({Level1PresentationKind::entity,5});
        rejects([&]{level1Type42BonusOperations(in,44,model42());});
        in=input(controls[2]);in.fixedSlots[3].entityIndex=5;
        rejects([&]{level1Type42BonusOperations(in,44,model42());});
        in=input(controls[0]);rejects([&]{level1Type42BonusOperations(in,43,model42());});
        auto wrong=model42();wrong.scaleShift=1;rejects([&]{level1Type42BonusOperations(in,44,wrong);});
        wrong=model42();wrong.collisionRadius=329;rejects([&]{level1Type42BonusOperations(in,44,wrong);});
        in.source.type=11; //Type11 cannot use the Type1 class1 provider.
        rejects([&]{level1Type42BonusOperations(in,44,model42());});
        in=input(controls[8]);in.source.stateBytes4dTo52[4]=1;
        rejects([&]{level1Type42BonusOperations(in,44,model42());});
        in=input(controls.front());in.source.stateBytes4dTo52[4]=3;
        rejects([&]{level1Type42BonusOperations(in,44,model42());});
        in=input(controls[8]);in.source.stateBytes4dTo52[0]=254;
        rejects([&]{level1Type42BonusOperations(in,44,model42());});
        in=input(controls.front());in.source.type=5;
        rejects([&]{level1Type42BonusOperations(in,44,model42());});
        in=input(controls[8]);in.source.stateBytes4dTo52[4]=0;
        rejects([&]{level1Type42BonusOperations(in,44,model42());});
        in=input(controls.front());
        rejects([&]{level1Type42BonusOperations(in,257,model42());});
        Level1Type42BonusInput empty{};
        rejects([&]{level1Type42BonusOperations(empty,44,model42());});
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;
}
