#include "stellar7/failure_control_work.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace stellar7 {
namespace {
using Kind = NormalWorkKind;
using Operations = std::vector<NormalWorkOperation>;
constexpr std::size_t debrisCount = 30;
constexpr std::uint16_t displayCount = 45;
constexpr std::uint16_t burstDisplayCount = 8;
constexpr std::int32_t debrisFloor = -400;

void instructions(Operations& operations,const std::uint64_t count) {
    operations.push_back({Kind::instructions,count});
}
Operations& operations(FailureControlWork& work) {
    if (work.steps.empty() || work.steps.back().child) { work.steps.push_back({{}, {}}); }
    return work.steps.back().operations;
}
void plain(FailureControlWork& work,const std::uint64_t count) { instructions(operations(work),count); }
void append(FailureControlWork& work,const Operations& value) {
    auto& target=operations(work);target.insert(target.end(),value.begin(),value.end());
}
void repeat(FailureControlWork& work,const std::uint64_t count) { operations(work).push_back({Kind::repeatString,count}); }
void child(FailureControlWork& work,const FailureControlChildKind kind,const std::uint16_t pc,
    const std::size_t ordinal=0,const std::array<std::int32_t,6> arguments={}) {
    work.steps.push_back({{},FailureControlChild{kind,pc,ordinal,arguments}});
}
std::int32_t add(const std::int32_t a,const std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b));
}
std::int32_t subtract(const std::int32_t a,const std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a)-static_cast<std::uint32_t>(b));
}
std::int32_t shift14(const std::int64_t value) {
    return static_cast<std::int32_t>(value>=0 ? value/16384 : -((-value+16383)/16384));
}
std::int16_t half(const std::int16_t value) {
    return static_cast<std::int16_t>(value>=0 ? value/2 : -((-static_cast<int>(value)+1)/2));
}
std::int16_t cosine(const std::uint16_t angle) { return makeFixedMatrixQ14(0,0,angle).values[0]; }
std::int16_t sine(const std::uint16_t angle) { return makeFixedMatrixQ14(0,0,angle).values[1]; }
void cosineWork(Operations& out,const std::uint16_t angle) { instructions(out,(angle & 0x8000U) ? 16 : 14); }
void sineWork(Operations& out,const std::uint16_t angle) {
    instructions(out,(static_cast<std::uint16_t>(angle+0xc000U) & 0x8000U) ? 17 : 15);
}
void multiplyWork(Operations& out,const std::int32_t a,const std::int32_t b) {
    //E16A selects the short multiply iff both input high words are zero.
    instructions(out,((static_cast<std::uint32_t>(a)|static_cast<std::uint32_t>(b)) >> 16) == 0 ? 11 : 19);
}
Level1Entity& entity(FailureControlSnapshot& state,const FailureObjectRef ref) {
    switch (ref.kind) {
    case FailureObjectKind::mission:return state.mission.at(ref.index);
    case FailureObjectKind::player:if(ref.index==0) {return state.player;}break;
    case FailureObjectKind::debris:return state.debris.at(ref.index);
    case FailureObjectKind::effect:return state.effects.at(ref.index).entity;
    case FailureObjectKind::projectile:break;
    }
    throw std::invalid_argument("failure reference is not an entity");
}
FixedPosition& position(FailureControlSnapshot& state,const FailureObjectRef ref) {
    return ref.kind==FailureObjectKind::projectile ? state.projectiles.at(ref.index).position : entity(state,ref).position;
}
std::uint8_t health(FailureControlSnapshot& state,const FailureObjectRef ref) {
    return ref.kind==FailureObjectKind::projectile ? state.projectiles.at(ref.index).stateBytes4dTo54[1]
        : entity(state,ref).stateBytes4dTo52[1];
}
std::uint8_t category(FailureControlSnapshot& state,const FailureObjectRef ref) {
    return ref.kind==FailureObjectKind::projectile ? state.projectiles.at(ref.index).stateBytes4dTo54[0]
        : entity(state,ref).stateBytes4dTo52[0];
}
std::uint8_t& flags(FailureControlSnapshot& state,const FailureObjectRef ref) {
    return ref.kind==FailureObjectKind::projectile ? state.projectiles.at(ref.index).flags : entity(state,ref).flags;
}
std::int16_t type(FailureControlSnapshot& state,const FailureObjectRef ref) {
    return ref.kind==FailureObjectKind::projectile ? state.projectiles.at(ref.index).type : entity(state,ref).type;
}
void validate(FailureControlWork& work) {
    for (const auto& order:{work.control.allocationOrder,work.control.presenterOrder}) {
        for(std::size_t i=0;i<order.size();++i) {
            position(work.expected,order[i]);
            if(std::find(order.begin(),order.begin()+i,order[i])!=order.begin()+i) {
                throw std::invalid_argument("duplicate failure source reference");
            }
        }
    }
    if(work.expected.rng.indexA()>=112 || work.expected.rng.indexB()>=112
        || (work.expected.rng.indexA() & 1U) || (work.expected.rng.indexB() & 1U)) {
        throw std::invalid_argument("unsupported failure RNG indices");
    }
}
void retire(FailureControlWork& work,const FailureObjectRef ref) {
    auto& order=work.control.allocationOrder;
    plain(work,6); //481A..4824.
    if(order.empty()) {plain(work,4);return;}
    plain(work,2); //4826/4829.
    for(std::size_t i=0;i<order.size();++i) {
        plain(work,2);
        if(order[i]!=ref) {plain(work,5);continue;}
        plain(work,3);
        if(i+1!=order.size()) {order[i]=order.back();plain(work,5);}
        order.pop_back();work.control.refreshRequested=true;work.retired.push_back(ref);
        plain(work,5);return;
    }
    plain(work,4); //Absent pointer returns without setting refresh.
}
std::uint16_t random(FailureControlWork& work) {
    auto& rng=work.expected.rng;
    plain(work,20+(rng.indexA()==0 ? 1 : 0)+(rng.indexB()==0 ? 1 : 0));
    return rng.next();
}
void wrapAxis(FailureControlWork& work,std::int32_t& value) {
    const auto high=static_cast<std::int16_t>(static_cast<std::uint32_t>(value)>>16);
    const auto low=static_cast<std::uint16_t>(value);
    plain(work,2);
    if(high>=0) {
        plain(work,1);
        if(high>0) {plain(work,3);value=subtract(value,65536);return;}
        plain(work,2);
        if(low>32768) {plain(work,3);value=subtract(value,65536);return;}
    }
    plain(work,2);
    if(high>-1) {return;}
    plain(work,1);
    if(high==-1) {
        plain(work,2);
        if(low>=32768) {return;}
    }
    plain(work,2);value=add(value,65536);
}
void recenter(FailureControlWork& work) {
    plain(work,5);
    const auto& order=work.control.presenterOrder;
    if(order.empty()) {plain(work,1);}
    else {
        plain(work,1);
        for(std::size_t i=0;i<order.size();++i) {
            const auto ref=order[i];plain(work,3);
            if(ref.kind==FailureObjectKind::player) {plain(work,1);}
            else {
                plain(work,2);
                if(type(work.expected,ref)==7) {plain(work,1);}
                else {
                    auto& at=position(work.expected,ref);
                    at.x=subtract(at.x,work.expected.player.position.x);
                    at.y=subtract(at.y,work.expected.player.position.y);
                    plain(work,12);
                    if((work.expected.sourceLoopTick & 15U)==0) {wrapAxis(work,at.x);wrapAxis(work,at.y);}
                }
            }
            plain(work,i+1==order.size() ? 5 : 6);
        }
    }
    plain(work,3+12); //0D23 ability0 branch and0DBD..0DDB.
    work.expected.player.position.x=0;work.expected.player.position.y=0;
}
void debrisMove(FailureControlWork& work,Level1Entity& item) {
    plain(work,2); //73E5 PUSH/CALL.
    auto moved=failureKinematicsOperations(item);item=std::move(moved.actor);append(work,moved.operations);
    item.zVelocity=static_cast<std::int16_t>(static_cast<std::uint16_t>(item.zVelocity)-5U);
    plain(work,4); //73E9 cleanup, gravity, high compare/JG.
    const auto high=static_cast<std::int16_t>(static_cast<std::uint32_t>(item.position.z)>>16);
    if(high>-1) {return;}
    plain(work,1); //73F6 JL.
    if(high==-1) {
        plain(work,2); //73F8/73FD.
        if(static_cast<std::uint16_t>(item.position.z)>=static_cast<std::uint16_t>(debrisFloor)) {return;}
    }
    plain(work,11); //73FF..741F.
    item.position.z=debrisFloor;item.zVelocity=static_cast<std::int16_t>(-half(item.zVelocity));
    item.xVelocity=half(item.xVelocity);item.yVelocity=half(item.yVelocity);
    item.pitchVelocity=half(item.pitchVelocity);item.rollVelocity=half(item.rollVelocity);item.yawVelocity=half(item.yawVelocity);
}
std::uint32_t distanceAbsolute(const std::uint32_t difference,
    std::vector<NormalWorkOperation>& operations) {
    instructions(operations, 8);
    if ((difference & 0x80000000U) == 0) {
        return difference;
    }
    instructions(operations, 2);
    if ((difference & 0xffffU) == 0x8000U) {
        instructions(operations, 2);
        if (difference == 0xffff8000U) {
            instructions(operations, 3);
            return 32767;
        }
    }
    instructions(operations, 3);
    return 0U - difference;
}

std::uint32_t distanceOperations(const FixedPosition first,
    const FixedPosition second, std::vector<NormalWorkOperation>& operations) {
    instructions(operations, 6);
    auto x = distanceAbsolute(static_cast<std::uint32_t>(second.x)
        - static_cast<std::uint32_t>(first.x), operations);
    auto y = distanceAbsolute(static_cast<std::uint32_t>(second.y)
        - static_cast<std::uint32_t>(first.y), operations);
    instructions(operations, 4);
    const auto xHigh = static_cast<std::int16_t>(x >> 16);
    const auto yHigh = static_cast<std::int16_t>(y >> 16);
    auto swap = false;
    if (yHigh >= xHigh) {
        instructions(operations, 1);
        if (yHigh > xHigh) {
            swap = true;
        } else {
            instructions(operations, 2);
            swap = (y & 0xffffU) > (x & 0xffffU);
        }
    }
    if (swap) {
        instructions(operations, 10);
        std::swap(x, y);
    }
    instructions(operations, 20);
    const auto product = y * 3U;
    const auto shifted = (product >> 3)
        | ((product & 0x80000000U) != 0 ? 0xe0000000U : 0U);
    return x + shifted;
}

std::uint16_t headingOperations(const FixedPosition actor,
    const FixedPosition player, std::vector<NormalWorkOperation>& operations) {
    const auto signedX = static_cast<std::int16_t>(
        static_cast<std::uint32_t>(player.x) - static_cast<std::uint32_t>(actor.x));
    const auto signedY = static_cast<std::int16_t>(
        static_cast<std::uint32_t>(player.y) - static_cast<std::uint32_t>(actor.y));
    const auto x = static_cast<std::uint32_t>(signedX < 0 ? -static_cast<int>(signedX) : signedX);
    const auto y = static_cast<std::uint32_t>(signedY < 0 ? -static_cast<int>(signedY) : signedY);
    instructions(operations, 4);
    instructions(operations, signedX < 0 ? 10 : 6);
    instructions(operations, signedY < 0 ? 10 : 6);
    instructions(operations, 3);
    if (x == 0) {
        instructions(operations, 2);
    } else {
        instructions(operations, 3);
        if (y == 0) {
            instructions(operations, 2);
        } else {
            instructions(operations, 4);
            if (x == y) {
                instructions(operations, 4);
            } else {
                if ((x & 0xffffU) == (y & 0xffffU)) {
                    instructions(operations, 2);
                }
                instructions(operations, 5);
                instructions(operations, y > x ? 11 : 13);
                instructions(operations, 30); // E0D0 positive16-bit denominator.
                instructions(operations, 2); // PUSH ratio and CALLF atan.
                instructions(operations, 5); // 16e6:34a1 table accessor through RETF.
                instructions(operations, y > x ? 4 : 2);
            }
        }
    }
    instructions(operations, signedX < 0 ? 5 : 2);
    instructions(operations, signedY < 0 ? 5 : 2);
    instructions(operations, 7);
    return fixedHeadingBetween(actor, player);
}

void refresh(FailureControlWork& work) {
    const auto camera=work.expected.camera.position;
    plain(work,10);
    bool rebuild=work.control.refreshRequested;
    if(rebuild) {plain(work,2);}
    else {
        const auto x=subtract(camera.x,work.control.lastListCamera.x);
        const auto y=subtract(camera.y,work.control.lastListCamera.y);
        if(x<=-32768 || x>=32768 || y<=-32768 || y>=32768) {
            throw std::invalid_argument("failure list displacement exceeds admitted camera bound");
        }
        plain(work,14+(x<0 ? 5 : 0)+7);
        rebuild=x<=-1024 || x>=1024;
        if(!rebuild) {
            plain(work,(y<0 ? 9 : 5)+7);
            rebuild=y<=-1024 || y>=1024;
            if(!rebuild) {plain(work,7);return;}
        }
    }
    plain(work,6);repeat(work,6);plain(work,4);
    work.control.presenterOrder.clear();
    if(work.control.allocationOrder.empty()) {plain(work,1+6);}
    else {
        plain(work,4);
        const auto absolute=[&](const std::int32_t value) {
            if(value>=0) {plain(work,3+2);return static_cast<std::uint32_t>(value);}
            plain(work,2);
            if(static_cast<std::uint16_t>(value)==0x8000U) {
                plain(work,2);
                if(value==-32768) {plain(work,3);return std::uint32_t{32767};}
            }
            plain(work,5+2);return 0U-static_cast<std::uint32_t>(value);
        };
        const auto visible=[&](const std::uint32_t value) {
            const auto high=static_cast<std::int16_t>(value>>16);plain(work,2);
            if(high>0) {return false;}
            plain(work,1);if(high<0) {return true;}
            plain(work,2);return static_cast<std::uint16_t>(value)<65000;
        };
        for(std::size_t i=0;i<work.control.allocationOrder.size();++i) {
            const auto ref=work.control.allocationOrder[i];
            if(health(work.expected,ref)==0) {throw std::invalid_argument("inactive failure allocation requires additional raw-list ancestry");}
            plain(work,4+13+2);
            const auto at=position(work.expected,ref);
            const auto x=absolute(subtract(camera.x,at.x));
            bool accepted=visible(x);
            if(accepted) {
                plain(work,2);const auto y=absolute(subtract(camera.y,at.y));accepted=visible(y);
            }
            if(accepted) {plain(work,5);work.control.presenterOrder.push_back(ref);}
            else {flags(work.expected,ref)&=0xfbU;plain(work,2);}
            plain(work,i+1==work.control.allocationOrder.size() ? 4 : 5);
        }
        plain(work,6);
    }
    work.control.lastListCamera=camera;work.control.refreshRequested=false;
}
void sort(FailureControlWork& work) {
    auto& order=work.control.presenterOrder;plain(work,8);
    if(order.empty()) {plain(work,1+5);return;}
    std::size_t selected=0;
    for(std::size_t destination=0;destination<order.size();++destination) {
        plain(work,9);std::uint32_t maximum=0;
        for(std::size_t candidate=destination;candidate<order.size();++candidate) {
            plain(work,9);
            const auto distance=distanceOperations(work.expected.camera.position,
                position(work.expected,order[candidate]),operations(work));
            plain(work,4+1+4); //Raw campaign index0 skips Type46 override.
            const auto dh=static_cast<std::int16_t>(distance>>16);
            const auto mh=static_cast<std::int16_t>(maximum>>16);
            if(dh>=mh) {plain(work,1);if(dh==mh) {plain(work,2);}}
            if(distance>maximum) {plain(work,5);maximum=distance;selected=candidate;}
            plain(work,5);
        }
        plain(work,3);
        if(selected!=destination) {std::swap(order[selected],order[destination]);plain(work,8);}
        plain(work,destination+1==order.size() ? 4 : 5);
    }
    plain(work,5);
}
void constructDebris(FailureControlWork& work,const std::size_t index,const std::array<std::uint32_t,5>& radii) {
    plain(work,6);const auto draw=static_cast<std::int16_t>(random(work));
    const auto remainder=draw%5;const auto kind=static_cast<std::int16_t>(36+(remainder<0 ? -remainder : remainder));
    plain(work,10);child(work,FailureControlChildKind::heapCreate,0x39ee,index,{kind,99,0,0,0,0});
    plain(work,4+8);repeat(work,6); //Approved native provider returns successful zero99 storage.
    Level1Entity debris{};debris.allocationIndex=index;debris.type=kind;debris.collisionRadius=radii[kind-36];
    debris.position=work.expected.player.position;debris.stateBytes4dTo52={0,1,0,0,0,0};
    plain(work,3);debris.pitch=static_cast<std::uint16_t>(std::abs(static_cast<int>(static_cast<std::int16_t>(random(work)))));
    plain(work,6);debris.roll=random(work);plain(work,3);debris.yaw=random(work);
    plain(work,3);debris.forwardVelocity=static_cast<std::int16_t>((random(work)&63U)+20);
    plain(work,6);auto moved=failureKinematicsOperations(debris);debris=std::move(moved.actor);append(work,moved.operations);
    plain(work,2);debris.pitchVelocity=static_cast<std::int16_t>(random(work));
    plain(work,3);debris.rollVelocity=static_cast<std::int16_t>(random(work));
    plain(work,3);debris.yawVelocity=static_cast<std::int16_t>(random(work));
    debris.forwardVelocity=0;
    debris.xVelocity=static_cast<std::int16_t>(subtract(debris.position.x,work.expected.player.position.x));
    debris.yVelocity=static_cast<std::int16_t>(subtract(debris.position.y,work.expected.player.position.y));
    debris.zVelocity=static_cast<std::int16_t>(subtract(debris.position.z,work.expected.player.position.z));
    plain(work,20);work.expected.debris.push_back(debris);
}
void burst(FailureControlWork& work) {
    plain(work,2); //7429/742D.
    if(work.control.display>=burstDisplayCount) {return;}
    plain(work,6);repeat(work,6);
    auto at=work.expected.player.position;
    const auto bounded=[&](const unsigned mask) {
        plain(work,3+5); //Outer argument/CALL7210;7210 prefix through CALLB39E.
        const auto value=random(work);plain(work,8+2); //721A..7226, outer cleanup/CWD.
        return static_cast<std::int32_t>(value&mask)-static_cast<std::int32_t>(mask>>1);
    };
    at.x=add(at.x,bounded(1023));plain(work,2);
    at.y=add(at.y,bounded(1023));plain(work,2);
    at.z=add(at.z,bounded(511));plain(work,2);
    plain(work,7); //747C..7489 CALL0BC8.
    plain(work,6); //0BC8..0BD0 CALL4580.
    plain(work,6); //4580..458A.
    auto slot=work.expected.effects.size();
    for(std::size_t i=0;i<work.expected.effects.size();++i) {
        plain(work,3);
        if(!work.expected.effects[i].entity.active()) {slot=i;break;}
        plain(work,4);
    }
    if(slot==work.expected.effects.size()) {plain(work,1+5+2+5);}
    else {
        plain(work,11); //45A0..45BD.
        auto& effect=work.expected.effects[slot];effect.entity.type=-1;effect.entity.stateBytes4dTo52[0]=252;
        effect.entity.stateBytes4dTo52[1]=14;effect.entity.stateBytes4dTo52[4]=17;effect.entity.position=at;
        effect.listed=true;work.control.refreshRequested=true;
        work.control.allocationOrder.push_back({FailureObjectKind::effect,slot});
        plain(work,2+11);repeat(work,6);plain(work,8); //0BD3..0C08, copy and suffix.
    }
    plain(work,1); //748C cleanup.
    operations(work).push_back({Kind::failureExplosionEvent,1,6});
}
} //namespace

KinematicsWork failureKinematicsOperations(const Level1Entity& actor) {
    const auto pitch=static_cast<std::uint16_t>(actor.pitch+actor.pitchVelocity);
    const auto roll=static_cast<std::uint16_t>(actor.roll+actor.rollVelocity);
    if(pitch==0 && roll==0) {return flatKinematicsOperations(actor);}
    KinematicsWork result{actor,{}};
    result.actor.position={};advanceEntityKinematics(result.actor);
    result.actor.position={add(actor.position.x,result.actor.position.x),add(actor.position.y,result.actor.position.y),add(actor.position.z,result.actor.position.z)};
    auto& out=result.operations;
    const auto yaw=result.actor.yaw;
    instructions(out,14+2);cosineWork(out,pitch); //3B3A..3B64.
    instructions(out,24);sineWork(out,yaw); //3B69..3B98.
    const auto negative=shift14(-static_cast<std::int64_t>(cosine(pitch))*actor.forwardVelocity);
    const auto positive=shift14(static_cast<std::int64_t>(cosine(pitch))*actor.forwardVelocity);
    instructions(out,5);multiplyWork(out,negative,sine(yaw));
    instructions(out,21);cosineWork(out,pitch);
    instructions(out,23);cosineWork(out,yaw);
    instructions(out,5);multiplyWork(out,positive,cosine(yaw));
    instructions(out,21);sineWork(out,pitch);
    instructions(out,22+16); //3C41..3C6F,3CEC..3D0E.
    return result;
}

FailureControlState applyFailureSceneRetirements(FailureControlState state,
    const std::vector<FailureObjectRef>& retired) {
    for(const auto ref:retired) {
        const auto found=std::find(state.allocationOrder.begin(),state.allocationOrder.end(),ref);
        if(found==state.allocationOrder.end()) {continue;}
        *found=state.allocationOrder.back();state.allocationOrder.pop_back();state.refreshRequested=true;
    }
    return state;
}

FailureControlWork failureEnterOperations(const FailureControlInput& input) {
    FailureControlWork work{input.control,input.snapshot,{},{},{}};
    if(work.control.entered || work.control.finished || work.control.display!=0 || !work.expected.debris.empty()) {
        throw std::invalid_argument("failure entry requires an unentered source controller");
    }
    validate(work);
    plain(work,5+8+11); //03EB..03FB,3546..3552,resident1B38 prefix.
    child(work,FailureControlChildKind::loadedPalette,0x077a,0,{0,256,0,63,0,0});
    plain(work,3+2+1+7+5); //Resident return,3546return,03FE CALL,72EC through77CE CALL0C6A.
    recenter(work);
    plain(work,3); //77D6..77DA CALL78F6.
    plain(work,13); //78F6 true branch.
    plain(work,2); //77DD cleanup/77E0 CALL9D4C.
    child(work,FailureControlChildKind::blackout,0x9d4c);
    plain(work,6);child(work,FailureControlChildKind::pageClear,0x6622,0); //Front.
    plain(work,3);child(work,FailureControlChildKind::pageClear,0x6622,1); //Back.
    plain(work,9);child(work,FailureControlChildKind::modelViewport,0x4034,0,{0,0,319,199,0,0});
    plain(work,8);repeat(work,49);operations(work).push_back({Kind::string,1});
    work.expected.camera=work.expected.player; //7822 copies99 bytes before player partial writes.
    const FailureObjectRef player{FailureObjectKind::player,0};
    if(std::find(work.control.allocationOrder.begin(),work.control.allocationOrder.end(),player)!=work.control.allocationOrder.end()) {
        throw std::invalid_argument("failure entry player is already allocated");
    }
    work.control.allocationOrder.push_back(player);
    auto& p=work.expected.player;p.position={};p.forwardVelocity=0;p.yawVelocity=0;p.stateBytes4dTo52[1]=1;
    auto& camera=work.expected.camera;camera.yawVelocity=0;
    plain(work,26+5);repeat(work,6);
    camera.position=p.position;camera.yaw=static_cast<std::uint16_t>(p.yaw+0x4000U);camera.forwardVelocity=-1800;
    plain(work,9);
    auto motion=failureKinematicsOperations(camera);camera=std::move(motion.actor);append(work,motion.operations);
    camera.forwardVelocity=0;plain(work,4); //78AA..78B6 CALL78C8.
    plain(work,5); //78C8..78D0.
    if(!work.control.presenterOrder.empty()) {
        plain(work,1);
        for(std::size_t i=0;i<work.control.presenterOrder.size();++i) {
            const auto ref=work.control.presenterOrder[i];plain(work,4);
            if(category(work.expected,ref)==253) {
                if(ref.kind!=FailureObjectKind::projectile) {throw std::invalid_argument("unmapped failure FD record");}
                work.expected.projectiles.at(ref.index).stateBytes4dTo54[1]=0;
                plain(work,3);retire(work,ref);plain(work,1);
            }
            plain(work,5);
        }
    }
    plain(work,3); //78F3 return.
    work.control.refreshRequested=true;plain(work,2); //78B9/78BE CALL5BFE.
    child(work,FailureControlChildKind::hardwareViewport,0x5bfe);
    plain(work,5+4); //77CE return;72FC..7304 initializes stack locals.
    work.control.entered=true;
    return work;
}


FailureControlWork failureDisplayOperations(const FailureControlInput& input) {
    FailureControlWork work{input.control,input.snapshot,{},{},{}};
    if(!work.control.entered || work.control.finished || work.control.display>=displayCount
        || work.expected.debris.size()!=work.control.debrisCreationOrder.size()) {
        throw std::invalid_argument("invalid failure display state");
    }
    validate(work);
    ++work.expected.sourceLoopTick;plain(work,13); //7307..732A CALL3768.
    refresh(work);plain(work,3); //732D cleanup,7330 count,7333 CALL14B0.
    sort(work);
    plain(work,16); //7336..7358 CALL3ED6.
    const auto camera=work.expected.camera.position;
    if(camera.x<=-32768 || camera.x>=32768 || camera.y<=-32768 || camera.y>=32768) {
        throw std::invalid_argument("failure heading exceeds admitted camera arithmetic");
    }
    headingOperations(camera,{},operations(work));
    plain(work,12);child(work,FailureControlChildKind::horizon,0x6ba6,0,{100,work.expected.camera.yaw/64,0,0,0,0});
    plain(work,1+2); //737D cleanup,7380/7384 display test.
    if(work.control.display==0) {
        plain(work,2);work.control.refreshRequested=true;
        if(!work.expected.debris.empty()) {throw std::invalid_argument("failure display0 already has debris");}
        for(std::size_t i=0;i<debrisCount;++i) {
            plain(work,2);constructDebris(work,i,input.debrisRadii);plain(work,4+6);
            work.control.debrisCreationOrder.push_back(i);work.control.allocationOrder.push_back({FailureObjectKind::debris,i});
            plain(work,5);
        }
        plain(work,1); //73BA JMP.
    } else {
        plain(work,2); //73BC/73C0.
        if(work.control.display==2) {plain(work,2);retire(work,{FailureObjectKind::player,0});plain(work,1);}
        plain(work,4); //73CC..73D6.
        if(!work.control.debrisCreationOrder.empty()) {
            plain(work,4); //73D8..73E1.
            for(const auto index:work.control.debrisCreationOrder) {
                plain(work,1);debrisMove(work,work.expected.debris.at(index));plain(work,4);
            }
        }
    }
    burst(work);
    work.drawOrder=work.control.presenterOrder;
    plain(work,5);child(work,FailureControlChildKind::scene,0xb996);
    plain(work,2); //74A8 cleanup,74AB style1.
    child(work,FailureControlChildKind::pageFlip,0x22b1);
    child(work,FailureControlChildKind::frameClear,0x22b5);
    plain(work,2); //74D1/74D5.
    if(work.control.display==0) {plain(work,1);child(work,FailureControlChildKind::entryFade,0x9d9e);}
    ++work.control.display;plain(work,3); //74DA/74DD/74E1.
    if(work.control.display<displayCount) {plain(work,1);} //74E3.
    return work;
}

FailureControlWork failureLeaveOperations(const FailureControlInput& input) {
    FailureControlWork work{input.control,input.snapshot,{},{},{}};
    if(!work.control.entered || work.control.finished || work.control.display!=displayCount) {
        throw std::invalid_argument("failure leave requires45 completed displays");
    }
    validate(work);
    plain(work,4); //74E6..74F0.
    if(!work.control.debrisCreationOrder.empty()) {
        plain(work,4);
        for(std::size_t ordinal=0;ordinal<work.control.debrisCreationOrder.size();++ordinal) {
            plain(work,2);child(work,FailureControlChildKind::heapRelease,0x398a,work.control.debrisCreationOrder[ordinal]);plain(work,5);
        }
    }
    plain(work,1);child(work,FailureControlChildKind::blackout,0x9d4c);
    plain(work,5);work.expected.debris.clear();work.control.finished=true;
    return work;
}

} //namespace stellar7
