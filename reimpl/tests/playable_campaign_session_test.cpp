#include "native_level2_frame_fixture.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <tuple>

namespace {
using namespace stellar7;
constexpr std::size_t noObject=static_cast<std::size_t>(-1);
enum class CampaignEntry { clearedWave, constructors };

std::size_t find(const GameplayModel& model,const std::int16_t first,const std::int16_t last) {
    const auto& entities=model.level2Entities();
    const auto actor=std::find_if(entities.begin(),entities.end(),[first,last](const auto& value) {
        return value.active() && value.type>=first && value.type<=last;
    });
    return actor==entities.end() ? noObject : static_cast<std::size_t>(actor-entities.begin());
}

GameplayInput approach(const GameplayModel& model,const Level1Entity& target,const bool combat) {
    const auto& player=model.level2Player();
    const auto error=static_cast<std::int16_t>(presenterTargetHeading(player.position,target.position)-player.yaw);
    const auto braking=static_cast<int>(error)-4*player.yawVelocity;
    const auto distance=approximatePlanarDistance(player.position,target.position);
    GameplayInput input{};
    input.left=braking>120;input.right=braking<-120;
    input.fire=combat && std::abs(static_cast<int>(error))<1500;
    input.forward=(combat ? distance>6500 : true) && std::abs(static_cast<int>(error))<4000;
    input.reverse=combat && distance<3500;
    return input;
}

void compare(const NativeGameplaySession& a,const NativeGameplaySession& b,
    const NativeSessionDelivery& left,const NativeSessionDelivery& right) {
    assert(left.firstSample==right.firstSample && left.samples==right.samples);
    assert(left.events.size()==right.events.size() && left.presentations.size()==right.presentations.size());
    for(std::size_t index=0;index<left.events.size();++index) {
        assert(left.events[index].loopTick==right.events[index].loopTick && left.events[index].eventId==right.events[index].eventId);
    }
    const auto sameFrame=[](const IndexedScreen& l,const IndexedScreen& r) {
        assert(l.pixels==r.pixels && l.palette.size()==r.palette.size());
        for(std::size_t index=0;index<l.palette.size();++index) {
            assert(l.palette[index].red==r.palette[index].red && l.palette[index].green==r.palette[index].green
                && l.palette[index].blue==r.palette[index].blue);
        }
    };
    sameFrame(a.frame(),b.frame());
    for(std::size_t index=0;index<left.presentations.size();++index) {
        assert(left.presentations[index].sampleEnd==right.presentations[index].sampleEnd);
        sameFrame(left.presentations[index].indexed,right.presentations[index].indexed);
    }
    for(const auto role:{NativePageRole::cache,NativePageRole::back,NativePageRole::front}) {
        assert(a.retainedPagePixels(role)==b.retainedPagePixels(role));
    }
    assert(a.model().phase()==b.model().phase() && a.model().ticks()==b.model().ticks());
    assert(a.model().campaignLevel()==b.model().campaignLevel() && a.model().score()==b.model().score());
    assert(a.model().playerArmor()==b.model().playerArmor() && a.model().level2LoopTick()==b.model().level2LoopTick());
    assert(a.model().level2ActiveListOrder()==b.model().level2ActiveListOrder());
    const auto actor=[](const Level1Entity& value) {
        return std::tie(value.allocationIndex,value.flags,value.type,value.position.x,value.position.y,value.position.z,
            value.pitch,value.roll,value.yaw,value.pitchVelocity,value.rollVelocity,value.yawVelocity,
            value.xVelocity,value.yVelocity,value.zVelocity,value.forwardVelocity,value.stateBytes4dTo52,
            value.modelAnimationVariant,value.primaryType11AnimationSelector,value.behaviorTimer,value.type12MissGate,
            value.type6CloakTimer,value.weaponSlotCount,value.weaponCooldown,value.weaponSlotTimers,
            value.collisionRadius,value.presenterDistance,value.targetHeading,value.collisionContactAllocationIndex,
            value.collisionContactIsPlayer,value.presentationModelAnimationVariant,value.animatedModelPresenterEnabled,
            value.ownerIsPlayer,value.ownerAllocationIndex,value.renderProfile);
    };
    assert(actor(a.model().level2Player())==actor(b.model().level2Player()));
    assert(a.model().level2Entities().size()==b.model().level2Entities().size());
    for(std::size_t index=0;index<a.model().level2Entities().size();++index) {
        assert(actor(a.model().level2Entities()[index])==actor(b.model().level2Entities()[index]));
    }
    auto rngA=a.model().level2Rng(),rngB=b.model().level2Rng();
    assert(rngA.calls()==rngB.calls());
    for(std::size_t index=0;index<StellarRng::wordCount;++index) {assert(rngA.next()==rngB.next());}
    assert(a.elapsedSamples()==b.elapsedSamples());
    assert(a.synth().driverState().normalClockCycles==b.synth().driverState().normalClockCycles);
    const auto& oplA=a.synth().registerWrites();const auto& oplB=b.synth().registerWrites();
    assert(oplA.size()==oplB.size());
    for(std::size_t index=0;index<oplA.size();++index) {
        assert(oplA[index].registerNumber==oplB[index].registerNumber && oplA[index].value==oplB[index].value);
    }
}

void route(const std::filesystem::path& resources,const CampaignEntry entry) {
    const auto make=entry==CampaignEntry::constructors ? test::makePlayableLevel2Session
        : test::makePlayableLevel2ClearedWaveSession;
    auto a=make(resources);
    auto b=make(resources);
    bool arrived=false,deployed=false,rewarded=false,success=false,loaded=false;
    unsigned level3Frames=0;
    double maximumAdvanceMilliseconds=0,maximumFrameMilliseconds=0;
    std::optional<FixedPosition> firstPrimaryPosition;
    bool primaryMoved=false;
    for(unsigned step=0;step<8000;++step) {
        GameplayInput input{};
        if(a->model().phase()==GameplayPhase::level2) {
            const auto guardian=find(a->model(),44,45),reward=find(a->model(),25,25);
            if(reward!=noObject) {
                rewarded=true;input=approach(a->model(),a->model().level2Entities()[reward],false);input.fire=true;
            } else if(guardian!=noObject) {
                deployed=true;
                input=approach(a->model(),a->model().level2Entities()[guardian],true);
            } else if(entry==CampaignEntry::constructors) {
                const Level1Entity* nearest=nullptr;
                std::uint32_t distance=0xffffffffU;
                for(const auto& actor:a->model().level2Entities()) {
                    if(!actor.active() || actor.stateBytes4dTo52[0]!=255 || actor.type<1 || actor.type>16 || actor.type==7) {continue;}
                    const auto candidate=approximatePlanarDistance(a->model().level2Player().position,actor.position);
                    if(candidate<distance) {distance=candidate;nearest=&actor;}
                }
                if(nearest) {
                    input=approach(a->model(),*nearest,true);
                    const auto error=static_cast<std::int16_t>(presenterTargetHeading(a->model().level2Player().position,
                        nearest->position)-a->model().level2Player().yaw);
                    const auto moving=std::abs(static_cast<int>(error))<10000;
                    input.forward=moving && distance>=3500;
                    input.reverse=moving && distance<3500;
                }
            }
        } else if(a->model().phase()==GameplayPhase::level3Native) {
            input.forward=input.left=input.fire=true;
        }
        const auto before=a->model().phase();
        NativeSessionDelivery left,right;
        try {
            const auto advance=[&](NativeGameplaySession& session,const std::size_t budget) {
                const auto started=std::chrono::steady_clock::now();
                auto delivery=session.advancePlayableCampaign(input,budget);
                const auto milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
                maximumAdvanceMilliseconds=std::max(maximumAdvanceMilliseconds,milliseconds);
                if (before==GameplayPhase::level2 || before==GameplayPhase::level3Native) {
                    maximumFrameMilliseconds=std::max(maximumFrameMilliseconds,milliseconds);
                }
                return delivery;
            };
            left=advance(*a,10000);right=advance(*b,257);
        } catch(const std::exception& error) {
            std::cerr<<"playable session step="<<step<<" phase="<<static_cast<int>(before)<<" level="<<a->model().campaignLevel()<<'\n';
            std::cerr<<"selected="<<a->model().level1State().selectedSystem()<<" armor="<<a->model().playerArmor()<<" timers=";
            for(const auto timer:a->model().level1State().systemTimers()) {std::cerr<<timer<<',';}
            std::cerr<<'\n';
            std::cerr<<test::playableSuccessDiagnostic(a->model())<<'\n';
            for(const auto& actor:a->model().level2PresentationEntities()) {
                std::cerr<<"actor="<<actor.type<<" radius="<<actor.collisionRadius<<" category="
                    <<static_cast<int>(static_cast<std::int8_t>(actor.stateBytes4dTo52[0]))
                    <<" profile="<<static_cast<int>(actor.renderProfile)<<" hp="<<static_cast<unsigned>(actor.stateBytes4dTo52[1])
                    <<" z="<<actor.position.z<<'\n';
            }
            throw;
        }
        compare(*a,*b,left,right);
        if(before==GameplayPhase::level2 && a->model().phase()==GameplayPhase::level2SuccessPresentation) {
            std::cerr<<"contact step="<<step<<' '<<test::playableSuccessDiagnostic(a->model())<<'\n';
        }
        arrived=arrived || find(a->model(),7,7)!=noObject;
        success=success || a->model().phase()==GameplayPhase::level2SuccessPresentation;
        loaded=loaded || a->model().phase()==GameplayPhase::level3LoaderReturn;
        if(a->model().phase()==GameplayPhase::level3Native) {
            ++level3Frames;
            for (const auto& actor:a->model().level2Entities()) {
                if (actor.type!=23 || !actor.ownerIsPlayer) {continue;}
                if (!firstPrimaryPosition) {firstPrimaryPosition=actor.position;}
                else {primaryMoved=primaryMoved || !(actor.position==*firstPrimaryPosition);}
                break;
            }
            if(level3Frames==3) {
                assert(arrived && deployed && rewarded && success && loaded);
                assert(a->model().campaignLevel()==2 && a->model().playerArmor()>=0);
                assert(a->model().level2Player().forwardVelocity>0 && a->model().level2Player().yawVelocity!=0);
                assert(a->playableCampaign() && a->recoveredClockPhase()->playableCampaign);
                assert(firstPrimaryPosition && primaryMoved);
                std::cout<<(entry==CampaignEntry::constructors ? "Constructor" : "Controlled wave-clear")
                    <<" Level2 guardian/success -> Level3 +3 in "<<step+1<<" calls"
                    <<" max_advance_ms="<<maximumAdvanceMilliseconds<<" max_frame_ms="<<maximumFrameMilliseconds
                    <<" samples="<<a->elapsedSamples()<<" normal_cycles="<<a->synth().driverState().normalClockCycles
                    <<" rng_calls="<<a->model().level2Rng().calls()<<" opl_writes="<<a->synth().registerWrites().size()<<'\n';
                return;
            }
        }
        if(step%500==0) {std::cerr<<"playable session step="<<step<<" armor="<<a->model().playerArmor()<<'\n';}
    }
    throw std::runtime_error("playable session did not reach retained Level3 frames");
}
}
int main(int argc,char** argv) {
    if(argc!=2 && argc!=3) {std::cerr<<"expected original resource directory [--constructors]\n";return 2;}
    if(argc==3 && std::string(argv[2])!="--constructors") {return 2;}
    try {route(argv[1],argc==3 ? CampaignEntry::constructors : CampaignEntry::clearedWave);return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
