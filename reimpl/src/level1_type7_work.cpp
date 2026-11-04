#include "stellar7/level1_type7_work.hpp"
#include "level1_source_math.hpp"
#include <algorithm>
#include <stdexcept>
namespace stellar7 {
namespace {
void plain(std::vector<NormalWorkOperation>& out,std::uint64_t n) { out.push_back({NormalWorkKind::instructions,n}); }
void steering(Level1Entity& actor,std::vector<NormalWorkOperation>& out,const std::uint16_t limit=0x500,
    std::int16_t difference=0) {
    plain(out,12); //AC3A..AC51.
    if(difference==static_cast<std::int16_t>(0x8000)) { difference=32767;plain(out,1); }
    const auto magnitude=difference<0 ? -static_cast<int>(difference) : difference;
    plain(out,7); //AC56..AC61.
    auto turn=std::min(magnitude,static_cast<int>(limit));
    if(magnitude<limit) { plain(out,1); }
    plain(out,5); //AC65..AC75, both sign paths.
    if(difference<0) { turn=-turn; }
    actor.yawVelocity=static_cast<std::int16_t>(turn);
    const auto high=static_cast<std::int16_t>(static_cast<std::uint32_t>(actor.position.z)>>16U);
    plain(out,2); //AC78/AC7C.
    bool bank=high>=0;
    if(high>=0) {
        plain(out,1); //AC7E.
        if(high==0) { plain(out,2);bank=(static_cast<std::uint32_t>(actor.position.z)&0xffffU)>=500; }
    }
    if(bank) {
        const auto value=static_cast<std::int16_t>(0U-static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(actor.yawVelocity)<<4U)+actor.roll));
        const auto shifted=value>=0 ? value/16 : -((-static_cast<int>(value)+15)/16);
        actor.roll=static_cast<std::uint16_t>(actor.roll+shifted);
        plain(out,7); //AC87..AC94.
    }
    plain(out,4); //AC97..AC9B.
}
}
bool isLevel1Type7ApproachState(const Level1Entity& incoming) noexcept {
    const auto state=incoming.stateBytes4dTo52[5];
    return !(incoming.type!=7 || incoming.collisionRadius!=13052 || incoming.flags!=0
        || incoming.stateBytes4dTo52[0]!=255 || incoming.stateBytes4dTo52[1]!=100
        || incoming.stateBytes4dTo52[4]!=5 || state>2 || incoming.stateBytes4dTo52[2]>3
        || incoming.position.z!=5000 || incoming.pitch!=0 || incoming.pitchVelocity!=0
        || incoming.rollVelocity!=0 || incoming.xVelocity!=0 || incoming.yVelocity!=0 || incoming.zVelocity!=0
        || (state==0 && (incoming.forwardVelocity!=400 || incoming.behaviorTimer!=0))
        || (state==1 && (incoming.behaviorTimer==0 || incoming.behaviorTimer>80
            || incoming.forwardVelocity!=5*incoming.behaviorTimer))
        || (state==2 && (incoming.forwardVelocity!=0 || incoming.behaviorTimer==0 || incoming.behaviorTimer>29)));
}
Level1Type7Work level1Type7Operations(const Level1Entity& incoming,
    const std::optional<std::size_t>& guardianEntityIndex) {
    if(!isLevel1Type7ApproachState(incoming)) {
        throw std::invalid_argument("unsupported Level1 Type7 approach state");
    }
    const auto state=incoming.stateBytes4dTo52[5];
    Level1Type7Work result{incoming,{}};auto& actor=result.actor;auto& out=result.operations;
    plain(out,8+10); //A418..A426;AF8C..AF9F.
    if(state==0) {
        plain(out,5); //AFB8..AFBE includingAC3A CALL.
        steering(actor,out,0x500,static_cast<std::int16_t>(actor.targetHeading-actor.yaw));plain(out,1); //AFC1 cleanup.
        const auto high=static_cast<std::int16_t>(actor.presenterDistance>>16U);
        plain(out,2); //AFC4/AFC8.
        bool close=high<0;
        if(high>0) { plain(out,1); }
        else {
            plain(out,1); //AFCD.
            if(high==0) {
                plain(out,2);close=(actor.presenterDistance&0xffffU)<1000;
                if(!close) { plain(out,1); }
            }
        }
        if(close) { actor.stateBytes4dTo52[5]=1;actor.behaviorTimer=80;plain(out,2); }
    } else if(state==1) {
        plain(out,2+4); //AFA1/AFA2;AFE6..AFF1.
        actor.forwardVelocity=static_cast<std::int16_t>(actor.forwardVelocity-5);--actor.behaviorTimer;
        if(actor.forwardVelocity==0) { actor.stateBytes4dTo52[5]=2;actor.behaviorTimer=29;plain(out,2); }
        else { plain(out,1); }
    } else {
        plain(out,4+9); //AFA1..AFA5;B004..B019.
        --actor.behaviorTimer;
        actor.stateBytes4dTo52[2]=static_cast<std::uint8_t>(static_cast<std::int8_t>(actor.behaviorTimer)/-10+3);
        if(guardianEntityIndex) { plain(out,1); }
        else {
            plain(out,2); //B01E/B022.
            if(actor.behaviorTimer==0) { throw std::invalid_argument("Type7 beam installation is not source-admitted"); }
            plain(out,1); //B024.
        }
    }
    plain(out,4+6); //AF8C nearRET;A429,A437..A43E.
    return result;
}
} // namespace stellar7

namespace stellar7::detail {
void steeringSourceOperations(Level1Entity& actor,std::int16_t difference,std::uint16_t limit,
    std::vector<NormalWorkOperation>& out) { steering(actor,out,limit,difference); }
}
