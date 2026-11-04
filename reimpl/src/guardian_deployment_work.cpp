#include "stellar7/guardian_deployment_work.hpp"
#include "stellar7/level1_type7_work.hpp"
#include "stellar7/failure_control_work.hpp"
#include "level1_ordinary_constructor.hpp"
#include "level1_source_math.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
namespace stellar7 {
namespace {
using Operations=std::vector<NormalWorkOperation>;
void plain(Operations& out,std::uint64_t n) { out.push_back({NormalWorkKind::instructions,n}); }
Operations& operations(std::vector<Level1DeploymentStep>& steps) {
    if(steps.empty() || !std::holds_alternative<Operations>(steps.back())) { steps.emplace_back(Operations{}); }
    return std::get<Operations>(steps.back());
}
void append(Operations& out,const Operations& child) { out.insert(out.end(),child.begin(),child.end()); }
std::int32_t add(std::int32_t a,std::int32_t b) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b)); }
std::int32_t shift14(std::int64_t v) { return static_cast<std::int32_t>(v>=0?v/16384:-((-v+16383)/16384)); }
std::int16_t cosine(std::uint16_t a) { return makeFixedMatrixQ14(0,0,a).values[0]; }
std::int16_t sine(std::uint16_t a) { return makeFixedMatrixQ14(0,0,a).values[1]; }
void multiply(Operations& out,std::int32_t a,std::int32_t b) {
    plain(out,((static_cast<std::uint32_t>(a)|static_cast<std::uint32_t>(b))>>16)==0?11:19);
}
void lateral(Level1Entity& actor,std::int16_t amount,Operations& out) {
    plain(out,8); //AB80..AB8F.
    if(actor.yaw==0) { plain(out,1+9);actor.position.x=add(actor.position.x,amount);return; }
    const auto angle=static_cast<std::uint16_t>(actor.yaw+0x4000U);
    const auto sin=sine(angle),cos=cosine(angle);
    plain(out,11);plain(out,(static_cast<std::uint16_t>(angle+0xc000U)&0x8000U)?17:15);
    plain(out,6);multiply(out,amount,-static_cast<std::int32_t>(sin));
    plain(out,25);plain(out,(angle&0x8000U)?16:14);
    plain(out,5);multiply(out,amount,cos);plain(out,24);
    actor.position.x=add(actor.position.x,shift14(-static_cast<std::int64_t>(sin)*amount));
    actor.position.y=add(actor.position.y,shift14(static_cast<std::int64_t>(cos)*amount));
}
void retire(std::vector<Level1ObjectRef>& order,Level1ObjectRef ref,Operations& out) {
    plain(out,6);
    if(order.empty()) { plain(out,4);return; }
    plain(out,2);
    for(std::size_t i=0;i<order.size();++i) {
        plain(out,2);
        if(order[i]!=ref) { plain(out,5);continue; }
        plain(out,3);
        if(i+1!=order.size()) { order[i]=order.back();plain(out,5); }
        order.pop_back();plain(out,5);return;
    }
    plain(out,4);
}
bool guardianState(const Level1Entity& a) {
    return a.type==44 && a.collisionRadius==2792 && (a.flags&~4U)==0
        && a.stateBytes4dTo52[0]==255 && a.stateBytes4dTo52[1]>0 && a.stateBytes4dTo52[1]<=15
        && a.stateBytes4dTo52[2]>=1 && a.stateBytes4dTo52[2]<=2 && a.stateBytes4dTo52[3]<=2
        && a.stateBytes4dTo52[4]==5 && a.stateBytes4dTo52[5]==0 && a.modelAnimationVariant<8
        && a.pitch==0 && a.roll==0 && a.pitchVelocity==0 && a.rollVelocity==0
        && a.xVelocity==0 && a.yVelocity==0 && a.zVelocity==0 && a.forwardVelocity==0 && a.position.z==0;
}
Level1Entity motionActor(const Level1Projectile& shot) {
    Level1Entity a{};a.position=shot.position;a.pitch=shot.pitch;a.roll=shot.roll;a.yaw=shot.yaw;
    a.pitchVelocity=shot.pitchVelocity;a.rollVelocity=shot.rollVelocity;a.yawVelocity=shot.yawVelocity;
    a.xVelocity=shot.xVelocity;a.yVelocity=shot.yVelocity;a.zVelocity=shot.zVelocity;a.forwardVelocity=shot.forwardVelocity;return a;
}
void applyMotion(Level1Projectile& shot,const Level1Entity& a) {
    shot.position=a.position;shot.pitch=a.pitch;shot.roll=a.roll;shot.yaw=a.yaw;
}
}
Level1GuardianBirthWork level1GuardianConstructorOperations(const Level1Entity& craft,
    const std::optional<std::size_t>& guardian,const Level1PeriodicSpawnInput& pool,
    const StellarRng& rng,const std::vector<Model3d>& models,const GuardianDeploymentDomain domain) {
    const auto level2=domain==GuardianDeploymentDomain::level2;
    if(domain!=GuardianDeploymentDomain::level1 && !level2) {
        throw std::invalid_argument("unsupported guardian deployment domain");
    }
    constexpr std::uint8_t level2EnemyLimit=12;
    constexpr std::int16_t level2CraftSpawned=13;
    constexpr std::uint8_t level2GuardianHealth=20;
    constexpr std::uint16_t level2SelectorBytes=1;
    Level1GuardianBirthWork result{rng,{},{}};auto& first=operations(result.steps);plain(first,7);
    if(guardian) { plain(first,1+5);return result; }
    if(craft.type!=7 || craft.stateBytes4dTo52[0]!=255 || pool.enemyLimit!=(level2 ? level2EnemyLimit : 10)
        || pool.spawnedEnemies<0 || (level2 ? pool.spawnedEnemies!=level2CraftSpawned : pool.spawnedEnemies>=12) || pool.activeEnemies<0
        || pool.activeEnemies>pool.spawnedEnemies || pool.allocationCount>=32) {
        throw std::invalid_argument("unsupported guardian deployment constructor input");
    }
    detail::OrdinaryConstructorProfile profile{true,nullptr,{}};
    if(level2) { profile.domain=detail::OrdinaryConstructorDomain::level2PostWave; }
    auto child=detail::ordinaryConstructorOperations(pool,rng,models,44,&profile);
    if(!child.spawn || child.spawn->actor.allocationIndex>=16) { throw std::invalid_argument("guardian constructor requires available ordinary slot"); }
    plain(first,3); //478C..478F CALL4370.
    if(profile.releaseOperationOffset) {
        const auto split=*profile.releaseOperationOffset;
        first.insert(first.end(),child.beforeProvider.begin(),child.beforeProvider.begin()+split);
        result.steps.emplace_back(Level1DeploymentHeapChild{Level1DeploymentHeapKind::releaseSelector,
            child.spawn->actor.allocationIndex,0,0xdcca,false});
        auto& next=operations(result.steps);next.insert(next.end(),child.beforeProvider.begin()+split,child.beforeProvider.end());
    } else { append(first,child.beforeProvider); }
    auto& before=operations(result.steps);plain(before,20); //4792..47D2.
    before.push_back({NormalWorkKind::repeatString,6});plain(before,5+14+18+40+7);
    auto actor=child.spawn->actor;actor.position=craft.position;actor.position.z=0;actor.yaw=craft.targetHeading;
    actor.stateBytes4dTo52={255,static_cast<std::uint8_t>(level2 ? level2GuardianHealth : 15),2,0,5,0};actor.forwardVelocity=0;actor.modelAnimationVariant=0;
    actor.presentationModelAnimationVariant=0;actor.animatedModelPresenterEnabled=true;
    actor.renderProfile=WorldActorRenderProfile::heapSelectorDeferredAnimation;
    const auto selectorBytes=static_cast<std::uint16_t>(level2 ? level2SelectorBytes : 3);
    result.steps.emplace_back(Level1DeploymentHeapChild{Level1DeploymentHeapKind::allocateSelector,
        actor.allocationIndex,selectorBytes,0xdcc6,false});
    auto& after=operations(result.steps);plain(after,9);after.push_back({NormalWorkKind::repeatString,selectorBytes});plain(after,6+7+10);
    result.rng=child.rng;result.birth=child.spawn;result.birth->actor=actor;return result;
}
bool isLevel1Type7DeploymentState(const Level1Entity& a) noexcept {
    if(isLevel1Type7ApproachState(a)) { return true; }
    const auto state=a.stateBytes4dTo52[5];
    if(a.type!=7 || a.collisionRadius!=13052 || (a.flags&~12U)!=0 || a.stateBytes4dTo52[0]!=255
        || (a.stateBytes4dTo52[1]!=100 && !(state==4 && a.stateBytes4dTo52[1]==0)) || a.stateBytes4dTo52[4]!=5 || a.stateBytes4dTo52[2]>3
        || a.pitchVelocity!=0 || a.rollVelocity!=0 || a.xVelocity!=0 || a.yVelocity!=0 || a.zVelocity!=0) {return false;}
    if(state==3) {return a.position.z==5000 && a.pitch==0 && a.forwardVelocity==0 && a.behaviorTimer>=1
        && a.behaviorTimer<=70 && ((a.flags&8U)!=0)==(a.behaviorTimer>=30);}
    return state==4 && (a.flags&8U)==0 && a.behaviorTimer==0;
}
Level1Type7DeploymentWork level1Type7DeploymentOperations(const Level1Type7DeploymentInput& input,
    const std::vector<Model3d>& models,const GuardianDeploymentDomain domain) {
    if(domain!=GuardianDeploymentDomain::level1 && domain!=GuardianDeploymentDomain::level2) {
        throw std::invalid_argument("unsupported guardian deployment domain");
    }
    if(!isLevel1Type7DeploymentState(input.actor)) { throw std::invalid_argument("unsupported Type7 deployment state"); }
    Level1Type7DeploymentWork result{input.actor,input.rng,{},input.allocationOrder,false,false,{}};
    auto& actor=result.actor;const auto state=actor.stateBytes4dTo52[5];
    if(state<3 && !(state==2 && actor.behaviorTimer==1 && !input.guardianEntityIndex)) {
        auto child=level1Type7Operations(actor,input.guardianEntityIndex);actor=child.actor;result.steps.emplace_back(std::move(child.operations));return result;
    }
    auto& out=operations(result.steps);
    if(state==2) {
        plain(out,47);actor.behaviorTimer=70;actor.stateBytes4dTo52[2]=3;actor.stateBytes4dTo52[5]=3;
        actor.flags|=8;actor.renderProfile=WorldActorRenderProfile::inlineSelectorBeamCallback;return result;
    }
    plain(out,8+10+7); //A418 caller;AF8C dispatch through state3 JMP.
    if(state==3) {
        --actor.behaviorTimer;plain(out,3);
        if(static_cast<std::int8_t>(actor.behaviorTimer)>29) { plain(out,4+6);return result; }
        plain(out,1);
        if(actor.behaviorTimer==29) {
            actor.flags&=static_cast<std::uint8_t>(~8U);actor.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
            plain(out,4);auto born=level1GuardianConstructorOperations(actor,input.guardianEntityIndex,input.pool,result.rng,models,domain);
            result.rng=born.rng;result.guardianBirth=born.birth;
            for(auto& step:born.steps) {result.steps.push_back(std::move(step));}
            plain(operations(result.steps),1+4+6);
            if(born.birth) {result.allocationOrder.push_back({Level1PresentationKind::entity,born.birth->actor.allocationIndex});result.refreshRequested=true;}
            return result;
        }
        actor.stateBytes4dTo52[2]=static_cast<std::uint8_t>(static_cast<std::int8_t>(actor.behaviorTimer)/10);
        plain(out,7);
        if(actor.behaviorTimer==0) {
            actor.stateBytes4dTo52[5]=4;actor.renderProfile=WorldActorRenderProfile::inlineSelectorDeparture;plain(out,1);
            result.steps.emplace_back(Level1DeploymentAudioChild{Level1DeploymentAudioKind::stopEvent17,17,0xb071,0xb078,3});
            plain(operations(result.steps),1+1+4+6);return result;
        }
        plain(out,4+6);return result;
    }
    plain(out,2); //AFAD..AFB0 state4 dispatch.
    actor.pitch=static_cast<std::uint16_t>(actor.pitch+0x90U);actor.forwardVelocity=static_cast<std::int16_t>(actor.forwardVelocity+50);
    plain(out,7);detail::steeringSourceOperations(actor,0x100,0x100,out);plain(out,1+2);
    const auto high=static_cast<std::int16_t>(actor.presenterDistance>>16U);
    if(high>=0) {
        plain(out,1);
        bool far=high>0;
        if(high==0) {plain(out,2);far=(actor.presenterDistance&65535U)>60000;}
        if(far) {
            actor.stateBytes4dTo52[1]=0;plain(out,3);
            retire(result.allocationOrder,{Level1PresentationKind::entity,actor.allocationIndex},out);
            plain(out,2);result.craftRetired=true;result.refreshRequested=true;
        }
    }
    plain(out,4+6);return result;
}
Level2GuardianBehaviorWork level2GuardianBehaviorOperations(const Level2GuardianBehaviorInput& input) {
    const auto& source=input.actor;
    constexpr std::uint8_t maximumHealth=20;
    constexpr std::uint8_t cycleTicks=36;
    constexpr std::uint8_t pulseTick=20;
    constexpr std::int16_t turnLimit=512;
    constexpr std::uint16_t cycleThreshold=300;
    if((source.type!=44 && source.type!=45) || source.collisionRadius!=1147
        || source.stateBytes4dTo52[0]!=255 || source.stateBytes4dTo52[1]>maximumHealth
        || source.stateBytes4dTo52[4]!=5 || source.stateBytes4dTo52[5]>1
        || source.behaviorTimer>cycleTicks || source.pitch!=0 || source.roll!=0
        || source.pitchVelocity!=0 || source.rollVelocity!=0 || source.xVelocity!=0
        || source.yVelocity!=0 || source.zVelocity!=0 || source.position.z!=0
        || (source.stateBytes4dTo52[5]==1 && source.behaviorTimer==0)
        || source.renderProfile!=WorldActorRenderProfile::heapSelectorDeferredAnimation
        || input.rng.indexA()>=StellarRng::wordCount*2 || input.rng.indexB()>=StellarRng::wordCount*2
        || input.rng.indexA()%2!=0 || input.rng.indexB()%2!=0) {
        throw std::invalid_argument("unsupported Level2 A92E guardian state");
    }
    Level2GuardianBehaviorWork result{source,input.rng,{},{},{},{}};
    auto& actor=result.actor;auto& out=result.beforeEmission;
    plain(out,8); //A92E..A93C, including CALL B39E.
    plain(out,20+(result.rng.indexA()==0?1:0)+(result.rng.indexB()==0?1:0));
    const auto choice=result.rng.next();plain(out,6);
    const auto touching=input.retainedContactPresent || actor.collisionContactIsPlayer
        || actor.collisionContactAllocationIndex!=std::numeric_limits<std::size_t>::max();
    if(actor.stateBytes4dTo52[5]==0) {
        actor.forwardVelocity=100;plain(out,7);
        const auto difference=static_cast<std::int16_t>(actor.yaw-actor.targetHeading);
        detail::steeringSourceOperations(actor,difference,turnLimit,out);
        actor.type=44;plain(out,5);
        if(choice<cycleThreshold) {
            actor.stateBytes4dTo52[5]=1;actor.behaviorTimer=cycleTicks;plain(out,3);
        }
        plain(out,2);
        if(!input.craftPresent) {
            plain(out,3);
            if((choice&0x3fU)==0) {
                plain(out,3);
                if(!touching) {
                    plain(out,4);result.emission=Level2GuardianEmission::actor48;
                    plain(result.afterEmissionSuccess,6);result.afterEmissionFailure=result.afterEmissionSuccess;
                    return result;
                }
            }
        }
        actor.collisionContactIsPlayer=false;
        actor.collisionContactAllocationIndex=std::numeric_limits<std::size_t>::max();
        plain(out,7);return result;
    }
    plain(out,2+18); //A94C/4D; A9B6..A9E1.
    const auto selector=static_cast<std::uint8_t>(std::abs(static_cast<int>(actor.behaviorTimer)-pulseTick)/4);
    actor.stateBytes4dTo52[2]=selector;
    actor.modelAnimationVariant=selector;actor.presentationModelAnimationVariant=selector;
    actor.yawVelocity=0;actor.forwardVelocity=0;actor.type=45;--actor.behaviorTimer;
    if(actor.behaviorTimer==pulseTick) {
        plain(out,3);
        if(!touching) {
            plain(out,3);result.emission=Level2GuardianEmission::pulse46;
            plain(result.afterEmissionSuccess,10);
            result.afterEmissionSuccess.push_back({NormalWorkKind::repeatString,6});
            plain(result.afterEmissionSuccess,5);plain(result.afterEmissionFailure,9);
            return result;
        }
    } else {
        plain(out,3);
        if(actor.behaviorTimer==0) {actor.stateBytes4dTo52[5]=0;plain(out,1);}
    }
    plain(out,5);return result;
}

Level2GuardianPulseWork level2GuardianPulseOperations(const Level2GuardianPulseInput& input,const Model3d& model46) {
    constexpr std::size_t registryCapacity=64;
    constexpr std::uint32_t pulseRadius=2260;
    if((input.source.type!=44 && input.source.type!=45) || input.source.collisionRadius!=1147
        || model46.scaleShift!=0 || model46.flags!=0 || model46.collisionRadius!=pulseRadius
        || !model46.animationLimits.empty() || input.allocationOrder.size()>registryCapacity) {
        throw std::invalid_argument("unsupported Level2 guardian pulse input/model");
    }
    Level2GuardianPulseWork result{{},input.allocationOrder,{}};
    auto& out=result.operations;plain(out,6);
    std::size_t slot=0;
    for(;slot<input.fixedSlots.size();++slot) {
        const auto& fixed=input.fixedSlots[slot];
        if(fixed.provenance==Level1BonusFixedProvenance::unknown) {
            throw std::invalid_argument("guardian pulse reached unknown fixed bytes");
        }
        plain(out,3);
        if(!fixed.entity.active()) {break;}
        plain(out,4);
    }
    if(slot==input.fixedSlots.size()) {plain(out,6);return result;}
    const auto& fixed=input.fixedSlots[slot];
    const auto index=fixed.entityIndex.value_or(input.nextEntityIndex);
    const auto ref=Level1ObjectRef{Level1PresentationKind::entity,index};
    if(input.allocationOrder.size()==registryCapacity || fixed.drawCallback!=0
        || (fixed.entityIndex && index>=input.nextEntityIndex)
        || std::find(input.allocationOrder.begin(),input.allocationOrder.end(),ref)!=input.allocationOrder.end()
        || (fixed.entity.renderProfile!=WorldActorRenderProfile::noHandles
            && fixed.entity.renderProfile!=WorldActorRenderProfile::inlineSelectorNoCallback)) {
        throw std::invalid_argument("unsupported guardian pulse fixed identity/presenter");
    }
    for(std::size_t other=0;other<input.fixedSlots.size();++other) {
        if(other!=slot && input.fixedSlots[other].entityIndex==index) {
            throw std::invalid_argument("guardian pulse fixed identity aliases another slot");
        }
    }
    auto actor=fixed.entity;actor.allocationIndex=index;actor.flags=0;actor.type=46;
    actor.collisionRadius=pulseRadius;actor.position=input.source.position;actor.pitch=0;actor.roll=0;actor.yaw=0;
    actor.stateBytes4dTo52[0]=254;actor.stateBytes4dTo52[1]=100;actor.stateBytes4dTo52[4]=0;
    plain(out,14+6+40+15+14); //4506 partial reset, local D03C/C05C and append.
    result.birth=Level2GuardianFixedBirth{slot,index,result.allocationOrder.size(),!fixed.entityIndex,actor};
    result.allocationOrder.push_back(ref);return result;
}

Level2GuardianActorEmissionWork level2GuardianActorEmissionOperations(
    const Level2GuardianActorEmissionInput& input,const std::vector<Model3d>& models) {
    const auto& source=input.source;
    if((source.type!=44 && source.type!=45) || source.collisionRadius!=1147
        || input.pool.activeEnemies<0 || input.pool.activeEnemies>input.pool.spawnedEnemies
        || input.pool.spawnedEnemies==std::numeric_limits<std::int16_t>::max()
        || input.rng.indexA()>=StellarRng::wordCount*2 || input.rng.indexB()>=StellarRng::wordCount*2
        || input.rng.indexA()%2!=0 || input.rng.indexB()%2!=0) {
        throw std::invalid_argument("unsupported Level2 guardian AE10 source/pool");
    }
    Level2GuardianActorEmissionWork result{input.rng,input.visualRandomState,{},{}};
    plain(operations(result.steps),8);
    detail::OrdinaryConstructorProfile profile{false,input.pool.retainedEntities,{},detail::OrdinaryConstructorDomain::level2GuardianEmission};
    auto child=detail::ordinaryConstructorOperations(input.pool,input.rng,models,13,&profile);
    auto& before=operations(result.steps);
    if(profile.releaseOperationOffset) {
        const auto split=*profile.releaseOperationOffset;
        before.insert(before.end(),child.beforeProvider.begin(),child.beforeProvider.begin()+split);
        result.steps.emplace_back(Level1DeploymentHeapChild{Level1DeploymentHeapKind::releaseSelector,
            child.spawn->actor.allocationIndex,0,0xdcca,false});
        auto& after=operations(result.steps);
        after.insert(after.end(),child.beforeProvider.begin()+split,child.beforeProvider.end());
    } else {append(before,child.beforeProvider);}
    auto& out=operations(result.steps);
    if(!child.spawn) {plain(out,10);return result;}
    if(models.size()<=48 || models[48].flags!=1 || models[48].scaleShift!=0
        || models[48].collisionRadius!=291 || models[48].animationLimits!=std::vector<std::uint8_t>{14}) {
        throw std::invalid_argument("unsupported Level2 guardian emitted model48");
    }
    plain(out,4+8);out.push_back({NormalWorkKind::repeatString,6});plain(out,10+8);
    auto actor=child.spawn->actor;actor.position=source.position;actor.position.z=add(actor.position.z,500);
    actor.pitch=source.pitch;actor.roll=0;actor.yaw=source.yaw;actor.type=48;actor.yawVelocity=0;
    actor.forwardVelocity=0;actor.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
    actor.animatedModelPresenterEnabled=false;
    constexpr std::uint32_t visualMultiplier=0x343fd;
    constexpr std::uint32_t visualIncrement=0x269ec3;
    result.visualRandomState=result.visualRandomState*visualMultiplier+visualIncrement;
    actor.behaviorTimer=static_cast<std::uint8_t>(result.visualRandomState>>16U);
    plain(out,14+14+19); //AE9E..AEBD; E078 and its whole E16A long multiply.
    result.rng=child.rng;result.birth=child.spawn;result.birth->actor=actor;return result;
}

Level1GuardianBehaviorWork level1GuardianBehaviorOperations(const Level1GuardianBehaviorInput& input,
    const Model3d& model22) {
    auto admitted=input.actor;
    if(input.domain==Level1GuardianBehaviorDomain::inheritedRetired && admitted.stateBytes4dTo52[1]==0) {admitted.stateBytes4dTo52[1]=1;}
    if(!guardianState(admitted) || (input.domain!=Level1GuardianBehaviorDomain::live && input.domain!=Level1GuardianBehaviorDomain::inheritedRetired) || model22.scaleShift!=0 || model22.flags!=1
        || model22.collisionRadius!=134 || model22.animationLimits!=std::vector<std::uint8_t>{2}) {
        throw std::invalid_argument("unsupported Level1 A440 guardian/source model");
    }
    Level1GuardianBehaviorWork result{input.actor,input.projectiles,input.allocationOrder,{},false,{}};
    auto& actor=result.actor;auto& out=operations(result.steps);
    const auto difference=static_cast<std::int16_t>(actor.targetHeading-actor.yaw);
    actor.animatedModelPresenterEnabled=false;actor.renderProfile=WorldActorRenderProfile::heapSelectorNoCallback;plain(out,15);
    const auto high=static_cast<std::int16_t>(actor.presenterDistance>>16U);
    bool far=false;
    if(high>=0) {plain(out,1);far=high>0;if(high==0) {plain(out,2);far=(actor.presenterDistance&65535U)>0x7000;}}
    if(far) {actor.yaw=actor.targetHeading;plain(out,2+5);return result;}
    const auto magnitude=static_cast<std::int16_t>(difference<0?-static_cast<int>(difference):difference);
    plain(out,6);
    if(magnitude<0x300) {
        plain(out,2);
        if((input.loopTick&15U)!=0) {plain(out,5);return result;}
        if(result.allocationOrder.size()>=32) {throw std::invalid_argument("guardian projectile registry full");}
        auto free=std::find_if(result.projectiles.begin(),result.projectiles.end(),[](const auto& p){return !p.active();});
        if(free==result.projectiles.end()) {throw std::invalid_argument("A440 null projectile write is unsupported");}
        const auto slot=static_cast<std::size_t>(free-result.projectiles.begin());
        if(free->poolSlot!=slot || free->stateBytes4dTo54[0]!=253) {throw std::invalid_argument("guardian launch requires owned FD pool ancestry");}
        plain(out,2+6); //A48C/CALL;ACEC..CALL44BA.
        plain(out,21+7*slot); //44BA complete successful partial-reset/append.
        auto& shot=*free;shot.stateBytes4dTo54[4]=0;shot.zVelocity=0;shot.stateBytes4dTo54[2]=1;
        plain(out,13); //ACF7..AD15.
        out.push_back({NormalWorkKind::repeatString,6});shot.position=actor.position;shot.position.z=add(shot.position.z,2000);
        shot.yaw=static_cast<std::uint16_t>(actor.yaw-0x4000U);plain(out,14);
        auto motion=failureKinematicsOperations(motionActor(shot));applyMotion(shot,motion.actor);append(out,motion.operations);
        shot.forwardVelocity=500;plain(out,19);
        const auto dx=static_cast<std::int64_t>(input.player.position.x)-shot.position.x;
        const auto dy=static_cast<std::int64_t>(input.player.position.y)-shot.position.y;
        if(dx<-32767 || dx>32767 || dy<-32767 || dy>32767 || shot.position.z<-32767 || shot.position.z>32767
            || actor.presenterDistance>32767) {throw std::invalid_argument("guardian aim exceeds qualified heading arithmetic");}
        shot.yaw=detail::headingSourceOperations(shot.position,input.player.position,out);plain(out,8);
        shot.pitch=detail::headingSourceOperations({}, {shot.position.z,static_cast<std::int32_t>(actor.presenterDistance),0},out);
        shot.type=22;shot.collisionRadius=134;plain(out,7+21+27+6);
        shot.ownerIsPlayer=false;shot.ownerAllocationIndex=actor.allocationIndex;shot.stateBytes4dTo54[1]=80;
        result.steps.emplace_back(Level1DeploymentAudioChild{Level1DeploymentAudioKind::directEvent6,6,0xadb1,0xadbb,4});
        plain(operations(result.steps),6+3+5);shot.stateBytes4dTo54[1]=64;
        result.launchedSlot=slot;result.refreshRequested=true;result.allocationOrder.push_back({Level1PresentationKind::projectile,slot});return result;
    }
    plain(out,2);
    bool distant=false;
    if(high>=0) {plain(out,1);distant=high>0;if(high==0) {plain(out,2);distant=(actor.presenterDistance&65535U)>0x1000;}}
    bool strafe=false;
    if(distant) {plain(out,6);strafe=magnitude<0x3000;}
    actor.animatedModelPresenterEnabled=true;actor.renderProfile=WorldActorRenderProfile::heapSelectorDeferredAnimation;
    if(strafe) {
        actor.yawVelocity=0;plain(out,4);std::int16_t amount=difference>0?120:-120;plain(out,difference>0?2:1);
        plain(out,2);
        if(actor.collisionContactIsPlayer || actor.collisionContactAllocationIndex!=std::numeric_limits<std::size_t>::max()) {
            actor.collisionContactIsPlayer=false;actor.collisionContactAllocationIndex=std::numeric_limits<std::size_t>::max();amount=-amount;plain(out,4);
        } else {plain(out,2);}
        plain(out,2);lateral(actor,amount,out);plain(out,1+5);return result;
    }
    actor.stateBytes4dTo52[2]=1;plain(out,7);detail::steeringSourceOperations(actor,difference,160,out);plain(out,1+5);return result;
}
} //namespace stellar7
