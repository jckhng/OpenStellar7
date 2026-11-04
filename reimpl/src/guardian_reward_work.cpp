#include "stellar7/guardian_reward_work.hpp"
#include "level1_effect_constructor.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace stellar7 {
namespace {
using Operations=std::vector<NormalWorkOperation>;
constexpr std::size_t registryCapacity=64;
constexpr std::uint16_t rewardRadius=897;
constexpr std::size_t maximumDisplacementSteps=65536;
void plain(Operations& out,std::uint64_t count) {out.push_back({NormalWorkKind::instructions,count});}
void append(Operations& out,const Operations& child) {out.insert(out.end(),child.begin(),child.end());}
void validateRefs(const std::vector<Level1ObjectRef>& refs) {
    if(refs.size()>registryCapacity) {throw std::invalid_argument("reward registry exceeds source capacity");}
    for(std::size_t i=0;i<refs.size();++i) {
        const auto ref=refs[i];
        if((ref.kind!=Level1PresentationKind::entity && ref.kind!=Level1PresentationKind::projectile && ref.kind!=Level1PresentationKind::deathEffect)
            || (ref.kind==Level1PresentationKind::projectile && ref.index>=Level1State::projectilePoolSize)
            || (ref.kind==Level1PresentationKind::deathEffect && ref.index>=Level1State::deathEffectPoolSize)
            || std::find(refs.begin(),refs.begin()+i,ref)!=refs.begin()+i) {
            throw std::invalid_argument("reward registry requires unique physical identities");
        }
    }
}
void query(Level1GuardianRewardWork& out,const std::vector<Level1RewardObject>& inherited,
    const Level1Entity& subject,bool& touching) {
    auto& ops=out.operations;plain(ops,9);touching=false;
    if(!inherited.empty()) {plain(ops,2);}
    for(const auto& record:inherited) {
        plain(ops,4);
        if(record.identity!=Level1ObjectRef{Level1PresentationKind::entity,subject.allocationIndex}) {
            plain(ops,2);
            if(record.actor.stateBytes4dTo52[0]!=253) {
                plain(ops,2);
                if(record.actor.stateBytes4dTo52[0]!=252) {
                    plain(ops,3);
                    const auto child=actorOverlapOperations(record.actor,subject);append(ops,child.operations);plain(ops,3);
                    if(child.overlaps) {
                        plain(ops,2);touching=true;out.contactWrites.push_back({record.identity,subject.allocationIndex});
                    }
                }
            }
        }
        plain(ops,5);
    }
    plain(ops,6);
}
std::uint32_t absolute(std::uint32_t delta,Operations& ops) {
    plain(ops,8);
    if((delta&0x80000000U)==0) {return delta;}
    plain(ops,2);
    if((delta&65535U)==0x8000U) {
        plain(ops,2);
        if(delta==0xffff8000U) {plain(ops,3);return 32767;}
    }
    plain(ops,3);return 0U-delta;
}
std::uint32_t distance(FixedPosition first,FixedPosition second,Operations& ops) {
    plain(ops,6);
    auto x=absolute(static_cast<std::uint32_t>(second.x)-static_cast<std::uint32_t>(first.x),ops);
    auto y=absolute(static_cast<std::uint32_t>(second.y)-static_cast<std::uint32_t>(first.y),ops);
    plain(ops,4);
    const auto xHigh=static_cast<std::int16_t>(x>>16),yHigh=static_cast<std::int16_t>(y>>16);
    bool swap=false;
    if(yHigh>=xHigh) {plain(ops,1);if(yHigh>xHigh) {swap=true;} else {plain(ops,2);swap=(y&65535U)>(x&65535U);}}
    if(swap) {plain(ops,10);std::swap(x,y);}
    plain(ops,20);const auto product=y*3U;
    return x+(product>>3)+((product&0x80000000U)?0xe0000000U:0U);
}
void sort(std::vector<Level1RewardObject>& inherited,FixedPosition camera,Operations& ops) {
    plain(ops,8);
    if(inherited.empty()) {plain(ops,1+5);return;}
    std::optional<std::size_t> selected; //SourceBP-0E persists across outer passes.
    for(std::size_t destination=0;destination<inherited.size();++destination) {
        plain(ops,6);std::uint32_t maximum=0;
        plain(ops,3);
        for(std::size_t candidate=destination;candidate<inherited.size();++candidate) {
            plain(ops,9);
            const auto value=distance(camera,inherited[candidate].actor.position,ops);
            if(value>static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
                throw std::invalid_argument("14B0 distance exceeds qualified signed positive domain");
            }
            plain(ops,5+4);
            const auto hi=static_cast<std::int16_t>(value>>16),maxHi=static_cast<std::int16_t>(maximum>>16);
            if(hi>=maxHi) {plain(ops,1);if(hi==maxHi) {plain(ops,2);}}
            if(value>maximum) {plain(ops,5);maximum=value;selected=candidate;}
            plain(ops,5);
        }
        if(!selected) {throw std::invalid_argument("14B0 reached uninitialized selection scratch");}
        plain(ops,3);
        if(*selected!=destination) {plain(ops,9);std::swap(inherited[*selected],inherited[destination]);}
        plain(ops,destination+1==inherited.size()?4:5);
    }
    plain(ops,5);
}
Operations& operations(std::vector<Level1GuardianDeathStep>& steps) {
    if(steps.empty() || !std::holds_alternative<Operations>(steps.back())) {steps.emplace_back(Operations{});}
    return std::get<Operations>(steps.back());
}
}

Level1GuardianRewardWork level1GuardianRewardOperations(const Level1GuardianRewardInput& input,const Model3d& model25) {
    Level1GuardianRewardWork result{input.rng,{},input.allocationOrder,{},{}};
    auto& ops=result.operations;plain(ops,12);
    if(input.existingReward) {plain(ops,1+5);return result;}
    validateRefs(input.allocationOrder);
    std::vector<Level1ObjectRef> inheritedRefs;
    for(const auto& record:input.inherited) {inheritedRefs.push_back(record.identity);}
    validateRefs(inheritedRefs);
    const auto rngDispatches=20+(input.rng.indexA()==0?1:0)+(input.rng.indexB()==0?1:0);
    const auto choice=result.rng.next();plain(ops,1+rngDispatches+4); //45E8 CALL; wholeB39E;45EB..45F2.
    plain(ops,6);
    std::size_t slot=0;
    for(;slot<input.fixedSlots.size();++slot) {
        const auto& fixed=input.fixedSlots[slot];
        if(fixed.provenance==Level1BonusFixedProvenance::unknown) {throw std::invalid_argument("reward reached unknown fixed bytes");}
        plain(ops,3);
        if(!fixed.entity.active()) {break;}
        plain(ops,4);
    }
    if(slot==input.fixedSlots.size()) {throw std::invalid_argument("reward full-pool fallback is outside qualified allocator");}
    const auto& fixed=input.fixedSlots[slot];const auto index=fixed.entityIndex.value_or(input.nextEntityIndex);
    if(input.firstBlockCapacity!=44 || model25.scaleShift!=0 || model25.collisionRadius!=rewardRadius
        || fixed.drawCallback!=0 || result.allocationOrder.size()==registryCapacity
        || (fixed.entityIndex && index>=input.nextEntityIndex)
        || std::find(result.allocationOrder.begin(),result.allocationOrder.end(),Level1ObjectRef{Level1PresentationKind::entity,index})!=result.allocationOrder.end()) {
        throw std::invalid_argument("unsupported reward selected fixed record/model/registry");
    }
    for(std::size_t i=0;i<input.fixedSlots.size();++i) {
        if(i!=slot && input.fixedSlots[i].entityIndex==index) {throw std::invalid_argument("reward fixed identity aliases another slot");}
    }
    plain(ops,14+6+27+15+14); //4506 partial reset and residentD03C/C05C.
    plain(ops,4+8); //45F5..45FD;462C..463D.
    ops.push_back({NormalWorkKind::repeatString,6});
    auto actor=fixed.entity;actor.allocationIndex=index;actor.flags=0;actor.type=25;actor.collisionRadius=rewardRadius;
    actor.position={input.source.position.x,input.source.position.y,0};actor.pitch=actor.roll=0;
    actor.yaw=static_cast<std::uint16_t>(input.source.targetHeading+0x8000U);
    actor.pitchVelocity=actor.rollVelocity=actor.yawVelocity=0;actor.forwardVelocity=100;
    actor.stateBytes4dTo52[0]=254;actor.stateBytes4dTo52[1]=100;actor.stateBytes4dTo52[2]=2;
    actor.stateBytes4dTo52[4]=0;actor.stateBytes4dTo52[5]=1;actor.modelAnimationVariant=2;
    actor.animatedModelPresenterEnabled=false;actor.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
    plain(ops,19); //4642..4677 inclusiveCALL0C0A.
    bool touching=false;query(result,input.inherited,actor,touching);plain(ops,3);
    if(!touching) {plain(ops,2);} else {
        plain(ops,1);
        std::size_t iterations=0;
        do {
            if(++iterations>maximumDisplacementSteps) {throw std::invalid_argument("reward displacement did not terminate within native bound");}
            plain(ops,2);const auto child=flatKinematicsOperations(actor);actor=child.actor;append(ops,child.operations);
            plain(ops,3);query(result,input.inherited,actor,touching);plain(ops,3);
        } while(touching);
    }
    actor.forwardVelocity=0;
    actor.yawVelocity=static_cast<std::int16_t>((10+((choice>>8)&3U))<<8);
    plain(ops,7);
    if((choice&0x100U)!=0) {actor.yawVelocity=static_cast<std::int16_t>(-actor.yawVelocity);plain(ops,1);}
    plain(ops,2+2+5);
    result.birth=Level1Type42BonusReceipt{slot,index,result.allocationOrder.size(),!fixed.entityIndex,actor,0};
    result.allocationOrder.push_back({Level1PresentationKind::entity,index});return result;
}

Level1GuardianDeathWork level1GuardianDeathOperations(const Level1GuardianDeathInput& input,const Model3d& model25,
    const BitmapFont& font,const CockpitPageState& pages) {
    const auto& hit=input.hit;const auto& target=hit.target;
    if(target.type!=44 || target.stateBytes4dTo52[0]!=255 || target.stateBytes4dTo52[4]!=5
        || (hit.damage!=1 && hit.damage!=5) || target.stateBytes4dTo52[1]==0 || target.stateBytes4dTo52[1]>hit.damage
        || target.allocationIndex!=hit.targetIndex || hit.activeEnemies<1 || hit.activeEnemies>12 || hit.playerTarget || hit.specialTarget
        || target.presenterDistance>static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
        || hit.allocationOrder!=input.reward.allocationOrder || input.reward.source.allocationIndex!=hit.targetIndex) {
        throw std::invalid_argument("unsupported lethal guardian1176 input");
    }
    validateRefs(hit.allocationOrder);
    auto order=hit.allocationOrder;const Level1ObjectRef ref{Level1PresentationKind::entity,hit.targetIndex};
    const auto found=std::find(order.begin(),order.end(),ref);
    if(found==order.end()) {throw std::invalid_argument("lethal guardian is absent from live registry");}
    const auto removed=static_cast<std::size_t>(found-order.begin());
    Level1GuardianDeathWork result{{0,hit.bonus,static_cast<std::int16_t>(hit.activeEnemies-1),{},{},order.size(),true,true,{}},
        {input.reward.rng,{},order,{},{}},input.reward.inherited,input.score,{}};
    auto& first=operations(result.steps);plain(first,9+4+2+14+1+8+30);
    result.hit.bonus.lastType=44;const auto raw=target.presenterDistance/0x1964U;const auto bucket=std::min(raw,5U);
    plain(first,3+(raw>5?1:0)+9);
    const auto effect=detail::effectConstructorOperations(hit.effectAges,target.position,static_cast<std::uint8_t>(3*bucket+13),
        static_cast<std::uint8_t>(3*bucket+17),order.size());append(first,effect.operations);result.hit.effect=effect.effect;
    if(effect.effect) {order.push_back({Level1PresentationKind::deathEffect,effect.effect->poolSlot});}
    plain(first,1+3);const auto last=order.size()-1;
    result.hit.removal=Level1TargetHitRemoval{removed,last,order.back()};plain(first,18+7*removed+(removed==last?0:5));
    order[removed]=order.back();order.pop_back();plain(first,2+1+2);
    result.steps.emplace_back(Level1GuardianDeathAudio{});
    auto score=scoreOperations(input.score,1000,input.difficulty,font,pages,ScoreAwardDomain::guardianDefeat);
    //11FB..1202 caller belongs here;7970 child contains only its own complete body.
    plain(operations(result.steps),3);result.score=score.score;result.steps.emplace_back(std::move(score));
    auto& tail=operations(result.steps);plain(tail,1+1);sort(result.inherited,input.camera,tail);plain(tail,2+4);
    auto rewardInput=input.reward;rewardInput.source=target;rewardInput.source.stateBytes4dTo52[1]=0;
    rewardInput.allocationOrder=order;rewardInput.inherited=result.inherited;
    result.reward=level1GuardianRewardOperations(rewardInput,model25);append(tail,result.reward.operations);
    plain(tail,1+2+2+6);result.hit.allocationCount=result.reward.allocationOrder.size();return result;
}

Level1RewardOscillatorWork level1RewardOscillatorOperations(const std::optional<Level1Entity>& actor) {
    Level1RewardOscillatorWork result{actor,{}};auto& ops=result.operations;plain(ops,6);
    if(!result.actor) {plain(ops,4);return result;}
    auto& a=*result.actor;auto& selector=a.stateBytes4dTo52[2];auto& direction=a.stateBytes4dTo52[5];plain(ops,2);
    if(static_cast<std::int8_t>(selector)<=0) {direction=1;plain(ops,2);} else {
        plain(ops,2);if(static_cast<std::int8_t>(selector)>2) {direction=255;plain(ops,1);}
    }
    selector=static_cast<std::uint8_t>(selector+direction);a.modelAnimationVariant=selector;plain(ops,2+4);return result;
}
Level1RewardContactWork level1RewardContactOperations(const Level1Entity& selected,const std::uint16_t campaignIndex) {
    if(selected.type!=25 || selected.stateBytes4dTo52[0]!=254 || selected.stateBytes4dTo52[4]!=0
        || selected.stateBytes4dTo52[1]==0 || selected.stateBytes4dTo52[1]>100 || campaignIndex>4) {
        throw std::invalid_argument("unsupported selected Type25 completion contact");
    }
    Level1RewardContactWork result{campaignIndex,false,{},{},{}};
    plain(result.beforeSuccess,6);plain(result.afterSuccess,4);return result;
}
Level1TargetHitWork level1RewardHitOperations(const Level1TargetHitInput& input) {
    const auto& target=input.target;
    if(target.type!=25 || target.stateBytes4dTo52[0]!=254 || target.stateBytes4dTo52[4]!=0
        || target.stateBytes4dTo52[1]==0 || target.stateBytes4dTo52[1]>100 || target.allocationIndex!=input.targetIndex
        || (input.damage!=1 && input.damage!=5) || input.playerTarget || input.specialTarget
        || std::find(input.allocationOrder.begin(),input.allocationOrder.end(),
            Level1ObjectRef{Level1PresentationKind::entity,input.targetIndex})==input.allocationOrder.end()) {
        throw std::invalid_argument("unsupported Type25 impact1176 profile");
    }
    validateRefs(input.allocationOrder);
    const auto reset=target.stateBytes4dTo52[1]<=input.damage;
    Level1TargetHitWork result{reset?std::uint8_t{100}:static_cast<std::uint8_t>(target.stateBytes4dTo52[1]-input.damage),
        input.bonus,input.activeEnemies,{},{},input.allocationOrder.size(),input.refreshRequested,false,{}};
    Operations before;plain(before,9+(reset?6:1)+6);
    const auto child=detail::effectConstructorOperations(input.effectAges,target.position,0,12,input.allocationOrder.size());
    append(before,child.operations);plain(before,1);result.steps.push_back({std::move(before),{}});
    result.steps.push_back({{},Level1TargetHitAudio{Level1TargetHitSound::impact,0x127a,0x1284,4}});
    result.steps.push_back({{{NormalWorkKind::instructions,6}}, {}});result.effect=child.effect;
    if(child.effect) {++result.allocationCount;result.refreshRequested=true;}
    return result;
}
} //namespace stellar7
