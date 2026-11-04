#include "native_level2_frame_fixture.hpp"
#include "stellar7/level2_projectile_prefix_receipt.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <utility>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif
using namespace stellar7;
namespace {
template<class Action>void rejects(Action action){bool rejected=false;try{action();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
void same(const IndexedScreen& a,const IndexedScreen& b){
    assert(a.pixels==b.pixels && a.palette.size()==b.palette.size());
    for(std::size_t i=0;i<a.palette.size();++i){assert(a.palette[i].red==b.palette[i].red && a.palette[i].green==b.palette[i].green && a.palette[i].blue==b.palette[i].blue);}
}
std::optional<std::uint8_t> lifetime(const GameplayModel& model,const std::int16_t type=23) {
    for(const auto& actor:model.level2Entities()){if(actor.type==type && actor.active()){return actor.stateBytes4dTo52[1];}}
    return std::nullopt;
}
void sameModel(const GameplayModel& a,const GameplayModel& b) {
    assert(a.phase()==b.phase() && a.score()==b.score() && a.playerArmor()==b.playerArmor());
    assert(a.ticks()==b.ticks() && a.level2LoopTick()==b.level2LoopTick());
    assert(a.level1State().systemTimers()==b.level1State().systemTimers());
    assert(a.level2ActiveListOrder()==b.level2ActiveListOrder());
    const auto fields=[](const Level1Entity& actor) {
        return std::tie(actor.flags,actor.type,actor.position.x,actor.position.y,actor.position.z,
            actor.pitch,actor.roll,actor.yaw,actor.xVelocity,actor.yVelocity,actor.zVelocity,
            actor.pitchVelocity,actor.rollVelocity,actor.yawVelocity,actor.forwardVelocity,
            actor.stateBytes4dTo52,actor.presenterDistance,actor.targetHeading,
            actor.weaponCooldown,actor.weaponSlotTimers,actor.renderProfile,actor.allocationIndex,
            actor.collisionRadius,actor.ownerIsPlayer,actor.ownerAllocationIndex,actor.collisionContactAllocationIndex,
            actor.behaviorTimer,actor.weaponSlotCount,actor.animatedModelPresenterEnabled);
    };
    assert(fields(a.level2Player())==fields(b.level2Player()));
    assert(a.level2Entities().size()==b.level2Entities().size());
    for(std::size_t index=0;index<a.level2Entities().size();++index) {
        assert(fields(a.level2Entities()[index])==fields(b.level2Entities()[index]));
    }
    auto left=a.level2Rng(),right=b.level2Rng();
    assert(left.calls()==right.calls());
    for(std::size_t index=0;index<StellarRng::wordCount;++index) {assert(left.next()==right.next());}
    if(a.phase()!=GameplayPhase::level2) {
        assert(a.failureFromLevel2() && b.failureFromLevel2());
        assert(a.phase()==GameplayPhase::failurePreExplosionBlackout || a.phase()==GameplayPhase::failureExplosion
            || a.phase()==GameplayPhase::failurePostExplosionBlackout);
        assert(fields(a.failurePlayer())==fields(b.failurePlayer()));
        assert(a.failurePlayerVisible()==b.failurePlayerVisible() && a.presentationFrame()==b.presentationFrame());
        for(const auto pair:{std::pair{&a.failureMissionEntities(),&b.failureMissionEntities()},
                std::pair{&a.failureDebris(),&b.failureDebris()}}) {
            assert(pair.first->size()==pair.second->size());
            for(std::size_t i=0;i<pair.first->size();++i) {assert(fields((*pair.first)[i])==fields((*pair.second)[i]));}
        }
        const auto& av=a.failureView();const auto& bv=b.failureView();
        assert(av.origin==bv.origin && av.matrix.mode==bv.matrix.mode && av.matrix.values==bv.matrix.values
            && av.viewport==bv.viewport && av.perspectiveScale==bv.perspectiveScale);
        assert(a.failureBursts().size()==b.failureBursts().size());
        for(std::size_t i=0;i<a.failureBursts().size();++i) {
            assert(a.failureBursts()[i].position==b.failureBursts()[i].position
                && a.failureBursts()[i].spawnFrame==b.failureBursts()[i].spawnFrame);
        }
        assert(a.failureVisibleItems().size()==b.failureVisibleItems().size());
        for(std::size_t i=0;i<a.failureVisibleItems().size();++i) {
            assert(a.failureVisibleItems()[i].kind==b.failureVisibleItems()[i].kind
                && a.failureVisibleItems()[i].index==b.failureVisibleItems()[i].index);
        }
        auto failureA=a.failureRng(),failureB=b.failureRng();
        assert(failureA.calls()==failureB.calls());
        for(std::size_t i=0;i<StellarRng::wordCount;++i) {assert(failureA.next()==failureB.next());}
        return; //Failure snapshots have their own tested physical-pool adapter, not an alive-prefix admission.
    }
    const auto first=a.recoveredLevel2ProjectilePrefixInput(Level1PlayerCombatDomain::expiryAndLoss),
        second=b.recoveredLevel2ProjectilePrefixInput(Level1PlayerCombatDomain::expiryAndLoss);
    assert(first.activeEnemies==second.activeEnemies && first.score==second.score);
    assert(first.prefix.allocationOrder==second.prefix.allocationOrder && first.prefix.activeOrder==second.prefix.activeOrder);
    assert(first.prefix.lastListCamera==second.prefix.lastListCamera && first.prefix.refreshRequested==second.prefix.refreshRequested);
    assert(first.prefix.fadeTicks==second.prefix.fadeTicks && first.prefix.impulse==second.prefix.impulse);
    const auto projectile=[](const Level1Projectile& shot) {
        return std::tie(shot.poolSlot,shot.flags,shot.type,shot.collisionRadius,shot.position.x,shot.position.y,shot.position.z,
            shot.pitch,shot.roll,shot.yaw,shot.pitchVelocity,shot.rollVelocity,shot.yawVelocity,
            shot.xVelocity,shot.yVelocity,shot.zVelocity,shot.forwardVelocity,shot.stateBytes4dTo54,
            shot.ownerIsPlayer,shot.ownerAllocationIndex,shot.presenterDistance,shot.targetHeading);
    };
    for(std::size_t slot=0;slot<first.prefix.projectiles.size();++slot) {
        assert(projectile(first.prefix.projectiles[slot])==projectile(second.prefix.projectiles[slot]));
        assert(first.effects[slot].listed==second.effects[slot].listed);
        assert(fields(first.effects[slot].entity)==fields(second.effects[slot].entity));
    }
}
void sameWork(const std::vector<ClockedPresentationWork>& a,const std::vector<ClockedPresentationWork>& b,
    const std::uint64_t firstCycle,const std::uint64_t lastCycle) {
    const auto normalize=[&](const auto& work) {
        std::vector<std::tuple<std::uint64_t,std::uint8_t,std::uint8_t>> writes;
        auto cycle=firstCycle;
        for(const auto& part:work) {
            for(const auto& write:part.audio.writes) {
                assert(write.cycleOffset>=0);
                writes.emplace_back(cycle+static_cast<std::uint64_t>(write.cycleOffset),
                    write.write.registerNumber,write.write.value);
            }
            //Elapsed totals already include interrupts/nested work. Count each returned piece once.
            cycle+=part.timing.total.elapsedCycles;
        }
        assert(cycle==lastCycle);return writes;
    };
    assert(normalize(a)==normalize(b));
}
void route(bool arrows, const std::filesystem::path& resources) {
    auto a=test::makeRecoveredLevel2PreloopSession(resources),b=test::makeRecoveredLevel2PreloopSession(resources);assert(a && b);
    const auto firstTick=a->model().ticks();
    constexpr unsigned neutralFramesThroughLoss=582;
    const auto end=arrows?18U:neutralFramesThroughLoss;
    for(unsigned raw=0;raw<end;++raw) {
        std::cerr<<"combat source route arrows="<<arrows<<" raw="<<raw<<'\n';
        GameplayInput input{};
        if(arrows && raw>=1 && raw<=7){input.left=input.forward=true;}
        if(arrows && raw>=8 && raw<=15){input.right=input.reverse=true;}
        if(arrows && raw==16){input.left=input.right=input.forward=input.reverse=true;}
        if(arrows && raw==17){input.right=input.forward=true;}
        input.fire=arrows?(raw==3 || raw==12 || raw==16):raw==1;
        GameplayInput unsupported{};unsupported.exit=true;
        const auto samples=a->elapsedSamples(),ticks=a->model().ticks();
        const auto previousFront=a->frame().pixels;
        const auto firstCycle=a->synth().driverState().normalClockCycles;
        rejects([&]{a->advanceRecoveredLevel2Frame(FrameCopyProfile::modeXNoop,10000,unsupported);});
        assert(a->elapsedSamples()==samples && a->model().ticks()==ticks);
        const auto left=a->advanceRecoveredLevel2Frame(FrameCopyProfile::modeXNoop,10000,input);
        const auto right=b->advanceRecoveredLevel2Frame(FrameCopyProfile::modeXNoop,257,input);
        const auto* run=a->recoveredLevel2Frame();assert(run && run->loopTick==raw && run->returnedLoopTick==raw+1);
        sameWork(run->work,b->recoveredLevel2Frame()->work,firstCycle,a->synth().driverState().normalClockCycles);
        assert(left.samples==right.samples && left.firstSample==right.firstSample && a->elapsedSamples()==b->elapsedSamples());
        assert(a->synth().driverState().normalClockCycles==b->synth().driverState().normalClockCycles);
        assert(left.events.size()==right.events.size());
        for(std::size_t i=0;i<left.events.size();++i){assert(left.events[i].loopTick==right.events[i].loopTick && left.events[i].eventId==right.events[i].eventId);}
        same(a->frame(),b->frame());
        sameModel(a->model(),b->model());
        assert(a->model().level2Rng().calls()==b->model().level2Rng().calls());
        assert(a->model().ticks()==firstTick+raw+1 && a->model().level2LoopTick()==raw+1);
        if(!arrows && raw==1){assert(lifetime(a->model())==64 && run->audioEvents.size()==1 && run->audioEvents[0].eventId==14);}
        if(!arrows && raw==2){assert(lifetime(a->model())==64);}
        if(!arrows && raw==3){assert(lifetime(a->model())==63);}
        if(!arrows && raw==65){assert(lifetime(a->model())==1);}
        if(!arrows && raw==66){assert(!lifetime(a->model()));}
        if(!arrows && (raw==155 || raw==229)) {
            assert(a->model().playerArmor()==(raw==155?552:424) && left.events.size()==2);
            assert(left.events[0].eventId==6 && left.events[1].eventId==6);
            assert(run->palettes.size()==1 && left.presentations.size()==2);
            assert(left.presentations.front().indexed.pixels==previousFront);
        }
        if(!arrows && raw>=156 && raw<=163) {
            assert(a->model().playerArmor()==552 && left.events.empty() && run->palettes.size()==1);
            assert(left.presentations.size()==2 && left.presentations.front().indexed.pixels==previousFront);
        }
        if(!arrows && (raw==213 || raw==226 || raw==322 || raw==335)) {
            assert(left.events.size()==1 && left.events[0].eventId==2);
            const auto source=a->model().recoveredLevel2ProjectilePrefixInput();
            const auto born=std::find_if(source.prefix.projectiles.begin(),source.prefix.projectiles.end(),
                [](const auto& shot){return shot.type==19 && shot.stateBytes4dTo54[1]==84;});
            assert(born!=source.prefix.projectiles.end() && !born->ownerIsPlayer && born->collisionRadius==441);
            assert(source.prefix.entities.at(born->ownerAllocationIndex).type==8);
            if(raw==213 || raw==322) {assert(born->poolSlot==0);}
        }
        if(!arrows && (raw==376 || raw==472 || raw==485 || raw==581)) {
            const auto expected=raw==376?184:raw==472?104:raw==485?24:-56;
            assert(a->model().playerArmor()==expected && left.events.size()==1 && left.events[0].eventId==6);
        }
        if(!arrows && (raw==384 || raw==448 || raw==512 || raw==576)) {
            assert(left.events.size()==1 && left.events[0].eventId==3);
        }
        if(!arrows && raw==214) {assert(lifetime(a->model(),19)==83);}
        if(!arrows && raw==297) {assert(!lifetime(a->model(),19));}
        if(!arrows && (raw==267 || raw==363)) {
            assert(a->model().playerArmor()==(raw==267?344:264));
            assert(left.events.size()==1 && left.events[0].eventId==6 && run->palettes.size()==1);
        }
        if(!arrows && ((raw>=230 && raw<=237) || (raw>=268 && raw<=275) || (raw>=364 && raw<=371))) {
            assert(left.events.empty() && run->palettes.size()==1 && left.presentations.size()==2);
            assert(left.presentations.front().indexed.pixels==previousFront);
        }
    }
    if(!arrows) {
        assert(a->model().playerArmor()==-56 && a->model().phase()==GameplayPhase::failurePreExplosionBlackout);
        assert(!a->model().pendingLevel2FailureTail());
        const auto samples=a->elapsedSamples(),cycles=a->synth().driverState().normalClockCycles;
        const auto ticks=a->model().ticks();
        rejects([&]{a->advanceRecoveredLevel2Frame(FrameCopyProfile::modeXNoop,10000,{});});
        assert(a->elapsedSamples()==samples && a->model().ticks()==ticks
            && a->synth().driverState().normalClockCycles==cycles);
        unsigned explosionEvents=0;
        for(unsigned call=0;call<46;++call) {
            const auto firstCycle=a->synth().driverState().normalClockCycles;
            const auto left=a->advanceRecoveredFailure(FrameCopyProfile::modeXNoop,10000);
            const auto right=b->advanceRecoveredFailure(FrameCopyProfile::modeXNoop,257);
            assert(left.firstSample==right.firstSample && left.samples==right.samples && a->elapsedSamples()==b->elapsedSamples());
            assert(a->synth().driverState().normalClockCycles==b->synth().driverState().normalClockCycles);
            assert(left.events.size()==right.events.size() && left.presentations.size()==right.presentations.size());
            for(std::size_t i=0;i<left.events.size();++i) {
                assert(left.events[i].loopTick==right.events[i].loopTick && left.events[i].eventId==right.events[i].eventId);
                assert(left.events[i].eventId==6);++explosionEvents;
            }
            for(std::size_t i=0;i<left.presentations.size();++i) {
                assert(left.presentations[i].sampleEnd==right.presentations[i].sampleEnd);
                same(left.presentations[i].indexed,right.presentations[i].indexed);
            }
            const auto* run=a->recoveredFailureFrame();assert(run && run->entered==(call==0) && run->finished==(call==45));
            assert(run->completedDisplays==(call<45 ? call+1 : 45));
            sameWork(run->work,b->recoveredFailureFrame()->work,firstCycle,a->synth().driverState().normalClockCycles);
            same(a->frame(),b->frame());sameModel(a->model(),b->model());
        }
        assert(explosionEvents==8 && a->model().phase()==GameplayPhase::failurePostExplosionBlackout);
        assert(a->model().level2LoopTick()==neutralFramesThroughLoss);
        std::cerr<<"retained loss582 and failure45+leave complete at "<<a->elapsedSamples()<<'\n';
    }
}
}
int main(int argc, char** argv){
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    if(argc!=2){std::cerr<<"Expected supplied resource directory\n";return 2;}
    const std::filesystem::path resources(argv[1]);
    try{route(false,resources);route(true,resources);}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 17;}
    return 0;
}
