#include "stellar7/gameplay.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace stellar7 {
struct GameplayModelTestAccess {
    static void clearedWave(GameplayModel& model) {
        model.startLevel2();
        //Controlled post-wave entry: retain the level's fixed records and RNG.
        //This establishes model continuity from here, not natural wave ancestry.
        auto& entities=model.level2Entities_;
        entities.erase(std::remove_if(entities.begin(),entities.end(),[](const auto& actor) {
            return actor.stateBytes4dTo52[0]==255;
        }),entities.end());
        for(std::size_t index=0;index<entities.size();++index) {entities[index].allocationIndex=index;}
        model.initializeCampaignPools(1,0);
        for(auto& actor:model.level2ActorPool_) {actor.stateBytes4dTo52[0]=255;}
        for(auto& projectile:model.level2ProjectilePool_) {
            projectile.stateBytes4dTo52[0]=253;
            projectile.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
            projectile.animatedModelPresenterEnabled=false;
        }
        for(auto& effect:model.level2DeathEffectPool_) {effect.stateBytes4dTo52[0]=252;}
        model.level2ActiveEnemyCount_=0;model.level2SpawnedEnemyCount_=12;
        model.level2PlayerArmor_=680;model.level2LoopTick_=0;model.phase_=GameplayPhase::level2;
        model.level2HasAdvanced_=false;model.level2TailConsumed_=false;
        model.snapshotLevel2Presentation();
    }
    static std::int16_t spawned(const GameplayModel& model) {return model.level2SpawnedEnemyCount_;}
    static std::size_t craft(const GameplayModel& model) {return model.level2CompletionObjectIndex_;}
    static std::size_t guardian(const GameplayModel& model) {return model.postWaveObjectIndex();}
    static std::size_t reward(const GameplayModel& model) {return model.level2PickupIndex_;}
    static void pausedIdentity(const GameplayModel& model) {
        const auto& pause=*model.level2SuccessContinuation_;
        std::cerr<<"pause identity: revision="<<pause.allocationRevision<<'/'<<model.level2AllocationRevision_
            <<" order="<<(pause.order==model.level2ActiveListOrder_)
            <<" fixed="<<(pause.fixedSlots==model.level2EntityFixedPoolSlots_)
            <<" types="<<pause.types.size()<<'/'<<model.level2Entities_.size()
            <<" pickup="<<pause.pickupIndex<<'/'<<model.level2PickupIndex_
            <<" loop="<<static_cast<std::uint16_t>(pause.sourceTick+1)<<'/'<<model.level2LoopTick_<<'\n';
    }
    static bool physicalAliases(const GameplayModel& model) {
        for(std::size_t index=0;index<model.level2Entities_.size();++index) {
            const auto& actor=model.level2Entities_[index];
            const auto ordinary=model.level2EntityActorPoolSlots_[index];
            const auto fixed=model.level2EntityFixedPoolSlots_[index];
            const auto projectile=model.level2EntityProjectilePoolSlots_[index];
            const auto effect=model.level2EntityDeathEffectPoolSlots_[index];
            const auto& physical=ordinary<model.level2ActorPoolSize_ ? model.level2ActorPool_[ordinary]
                : fixed<model.level2FixedObjectPoolSize_ ? model.level2FixedPool_[fixed]
                : projectile<model.level2ProjectilePoolSize_ ? model.level2ProjectilePool_[projectile]
                : model.level2DeathEffectPool_.at(effect);
            if(actor.type!=physical.type || !(actor.position==physical.position)
                || actor.stateBytes4dTo52!=physical.stateBytes4dTo52 || actor.behaviorTimer!=physical.behaviorTimer) {return false;}
        }
        return true;
    }
};
} //namespace stellar7

namespace {
using namespace stellar7;
constexpr std::size_t noObject=static_cast<std::size_t>(-1);

void require(const bool condition,const char* message) {
    if(!condition) {throw std::runtime_error(message);}
}

std::size_t find(const GameplayModel& model,const std::int16_t first,const std::int16_t last) {
    const auto& entities=model.level2Entities();
    const auto actor=std::find_if(entities.begin(),entities.end(),[first,last](const auto& value) {
        return value.type>=first && value.type<=last;
    });
    return actor==entities.end() ? noObject : static_cast<std::size_t>(actor-entities.begin());
}

GameplayInput approach(const GameplayModel& model,const Level1Entity& target,const bool combat) {
    const auto& player=model.level2Player();
    const auto desired=presenterTargetHeading(player.position,target.position);
    const auto error=static_cast<std::int16_t>(desired-player.yaw);
    const auto brakingError=static_cast<int>(error)-4*player.yawVelocity;
    const auto distance=approximatePlanarDistance(player.position,target.position);
    GameplayInput input{};
    input.left=brakingError>120;input.right=brakingError<-120;
    input.fire=combat && std::abs(static_cast<int>(error))<1500;
    if(combat) {
        input.forward=distance>6500 && std::abs(static_cast<int>(error))<4000;
        input.reverse=distance<3500;
    } else {
        input.forward=std::abs(static_cast<int>(error))<4000;
    }
    return input;
}

void retainedGuardianRoute() {
    auto model=std::make_unique<GameplayModel>();
    GameplayModelTestAccess::clearedWave(*model);
    bool arrival=false,guardian=false,cycle=false,pulse=false,emission=false,reward=false;
    const auto initialRngCalls=model->level2Rng().calls();
    unsigned guardianBirthFrame=0;
    constexpr unsigned maximumFrames=7000;
    for(unsigned frame=0;frame<maximumFrames;++frame) {
        const auto guardianIndex=find(*model,44,45);
        const auto rewardIndex=find(*model,25,25);
        GameplayInput input{};
        if(guardianIndex!=noObject) {
            //Allow real A92E cycles and emissions before starting ordinary fire.
            input=approach(*model,model->level2Entities()[guardianIndex],true);
            input.fire=input.fire && frame>guardianBirthFrame+500;
        } else if(rewardIndex!=noObject) {
            input=approach(*model,model->level2Entities()[rewardIndex],false);
            input.fire=true;
        }
        const auto beforeAdvance=model->level2LoopTick();
        model->advancePlayableCampaign(input);
        require(model->level2LoopTick()==static_cast<std::uint16_t>(beforeAdvance+1),"playable advance did not advance once");
        if(model->phase()==GameplayPhase::level2SuccessPresentation) {
            require(arrival && guardian && cycle && pulse && emission && reward,"missing retained guardian route boundary");
            require(model->score()>=1000,"guardian defeat did not award1000");
            require(model->playerArmor()>=0,"guardian route exhausted player armor");
            const auto& events=model->level2AudioEvents();
            require(std::count_if(events.begin(),events.end(),[](const auto& event) {return event.eventId==17;})==1,
                "arrival event17 was repeated");
            require(std::count_if(events.begin(),events.end(),[](const auto& event) {return event.eventId==12;})==1,
                "guardian defeat event12 missing or repeated");
            const auto ticks=model->ticks();
            const auto sourceTick=model->level2LoopTick();
            const auto random=model->level2Rng();
            const auto ground=model->level2Ground().points();
            const auto liveRemainder=std::count_if(model->level2Entities().begin(),model->level2Entities().end(),[](const auto& actor) {
                return actor.type==47 || actor.type==48 || actor.stateBytes4dTo52[0]==253
                    || actor.stateBytes4dTo52[0]==252;
            });
            model->setSuccessAudioActive(true);model->setSuccessAudioActive(false);
            GameplayModelTestAccess::pausedIdentity(*model);
            model->resumePlayableCampaignSuccess();
            require(model->phase()==GameplayPhase::level2SuccessReturn,"playable success did not return to its retained caller");
            require(model->ticks()==ticks && model->level2LoopTick()==sourceTick,"success return advanced the public frame");
            require(model->level2Player().position==FixedPosition{},"success tail did not recenter the player");
            require(model->level2Rng().calls()==random.calls(),"success return entered later AI/ground random work");
            require(model->level2Ground().points()==ground,"success return repeated main ground work");
            require(GameplayModelTestAccess::physicalAliases(*model),"success return lost physical pool aliases");
            for(const auto eventId:{17,12}) {
                require(std::count_if(model->level2AudioEvents().begin(),model->level2AudioEvents().end(),[eventId](const auto& event) {
                    return event.eventId==eventId;
                })==1,"success return repeated arrival/defeat audio");
            }
            std::cout<<"Retained Level2 guardian route completed in "<<frame+1<<" frames\n";
            std::cout<<"Success caller resumed with "<<liveRemainder<<" live ordinary/projectile/effect records\n";
            return;
        }
        require(model->phase()==GameplayPhase::level2,"retained guardian route left ordinary play before success");
        const auto beforeTail=model->level2LoopTick();
        model->finishPlayableCampaignFrame();
        require(model->level2LoopTick()==beforeTail,"playable tail changed the public frame ordinal");
        if(frame==0) {
            bool rejected=false;
            try {model->finishPlayableCampaignFrame();}
            catch(const std::invalid_argument&) {rejected=true;}
            require(rejected,"playable tail accepted double consumption");
        }
        require(model->level2Rng().calls()>=initialRngCalls,"playable route reset its retained RNG");
        const auto craftIndex=find(*model,7,7);
        if(!arrival && craftIndex!=noObject) {
            require(GameplayModelTestAccess::spawned(*model)==13,"arrival did not increment12->13");
            require(GameplayModelTestAccess::craft(*model)==craftIndex,"arrival lost its canonical pointer");
            arrival=true;
        }
        const auto deployed=find(*model,44,45);
        if(deployed!=noObject) {
            const auto& actor=model->level2Entities()[deployed];
            if(!guardian) {
                require(arrival,"guardian appeared before arrival");
                require(GameplayModelTestAccess::spawned(*model)==14,"guardian did not increment13->14");
                require(actor.stateBytes4dTo52[1]==20 && actor.collisionRadius==1147,"guardian constructor metadata");
                require(GameplayModelTestAccess::guardian(*model)==deployed,"guardian lost its canonical pointer");
                guardianBirthFrame=frame;guardian=true;
            }
            cycle=cycle || actor.type==45;
        }
        pulse=pulse || find(*model,46,46)!=noObject;
        emission=emission || find(*model,47,48)!=noObject;
        if(find(*model,25,25)!=noObject) {
            require(guardian && find(*model,44,45)==noObject,"reward appeared before guardian defeat");
            require(GameplayModelTestAccess::reward(*model)==find(*model,25,25),"reward lost its canonical pointer");
            reward=true;
        }
        if(frame+1==maximumFrames) {
            std::cerr<<"route state: guardian="<<guardian<<" cycle="<<cycle<<" pulse="<<pulse
                <<" emission="<<emission<<" reward="<<reward<<" armor="<<model->playerArmor()
                <<" score="<<model->score()<<'\n';
        }
    }
    throw std::runtime_error("retained Level2 guardian route did not complete");
}
} //namespace

int main() {
    try {retainedGuardianRoute();}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
