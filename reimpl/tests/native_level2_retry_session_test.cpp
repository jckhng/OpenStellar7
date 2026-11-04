#include "native_level2_frame_fixture.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <tuple>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif
using namespace stellar7;
namespace {
constexpr unsigned ordinaryLimit=2048;
constexpr unsigned destructionCalls=46;
void sameFrame(const NativeGameplaySession& a,const NativeGameplaySession& b) {
    assert(a.model().phase()==b.model().phase() && a.model().ticks()==b.model().ticks());
    assert(a.model().score()==b.model().score() && a.model().playerArmor()==b.model().playerArmor());
    assert(a.model().remainingContinues()==b.model().remainingContinues());
    assert(a.model().level2LoopTick()==b.model().level2LoopTick());
    assert(a.frame().pixels==b.frame().pixels && a.frame().palette.size()==b.frame().palette.size());
    for(std::size_t i=0;i<a.frame().palette.size();++i) {
        const auto x=a.frame().palette[i],y=b.frame().palette[i];
        assert(x.red==y.red && x.green==y.green && x.blue==y.blue);
    }
    const auto x=a.pageObservation(),y=b.pageObservation();
    assert(x.projection==y.projection && x.cache==y.cache && x.back==y.back && x.front==y.front
        && x.copy==y.copy && x.draw==y.draw);
    assert(a.model().level2ActiveListOrder()==b.model().level2ActiveListOrder());
    auto rngA=a.model().level2Rng(),rngB=b.model().level2Rng();
    assert(rngA.calls()==rngB.calls() && rngA.indexA()==rngB.indexA() && rngA.indexB()==rngB.indexB());
    for(std::size_t i=0;i<StellarRng::wordCount;++i) {assert(rngA.next()==rngB.next());}
    assert(a.elapsedSamples()==b.elapsedSamples());
    assert(a.synth().driverState().normalClockCycles==b.synth().driverState().normalClockCycles);
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
void sameDelivery(const NativeSessionDelivery& a,const NativeSessionDelivery& b) {
    assert(a.firstSample==b.firstSample && a.samples==b.samples && a.events.size()==b.events.size());
    for(std::size_t i=0;i<a.events.size();++i) {
        assert(a.events[i].loopTick==b.events[i].loopTick && a.events[i].eventId==b.events[i].eventId);
    }
    assert(a.presentations.size()==b.presentations.size());
    for(std::size_t i=0;i<a.presentations.size();++i) {
        const auto& first=a.presentations[i];const auto& second=b.presentations[i];
        assert(first.sampleEnd==second.sampleEnd && first.indexed.pixels==second.indexed.pixels);
        assert(first.indexed.palette.size()==second.indexed.palette.size());
        for(std::size_t color=0;color<first.indexed.palette.size();++color) {
            const auto x=first.indexed.palette[color],y=second.indexed.palette[color];
            assert(x.red==y.red && x.green==y.green && x.blue==y.blue);
        }
    }
}
template<class Action>void rejectsUnchanged(NativeGameplaySession& session,Action action) {
    const auto samples=session.elapsedSamples(),ticks=session.model().ticks();
    const auto cycle=session.synth().driverState().normalClockCycles;
    bool rejected=false;try {action();}catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected && session.elapsedSamples()==samples && session.model().ticks()==ticks);
    assert(session.synth().driverState().normalClockCycles==cycle);
}
}
int main(int argc,char** argv) {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    if(argc!=2) {return 2;}
    try {
        //Controlled success/table setup; every loss/failure/Continue/retry call is owned thereafter.
        auto a=test::makeRecoveredLevel2PreloopSession(argv[1]);
        auto b=test::makeRecoveredLevel2PreloopSession(argv[1]);
        assert(a && b);sameFrame(*a,*b);
        //The old factory's CURRENT all-zero table really ranks3400; never clear score to bypass330A.
        Level1RetryProfile rankHit{};rankHit.levelIndex=1;rankHit.decision=ContinueOutcome::yes;
        rankHit.score=3400;rankHit.remainingContinues=1;rankHit.currentHighscores=HighscoreTable({},{});
        rankHit.systemLevels.fill(1);rankHit.frontPage=0xa000;rankHit.backPage=0xa800;rankHit.cachePage=0xa400;
        rejectsUnchanged(*a,[&]{level2RankMissRetryOperations(rankHit);});
        const auto* originalSuccess=a->recoveredLevel2Transition();assert(originalSuccess);
        const auto successSamples=originalSuccess->samples;
        const auto successRng=originalSuccess->preloop.ground.rng.calls();
        rejectsUnchanged(*a,[&]{a->resumeRecoveredLevel2Retry();});
        unsigned ordinary=0;
        while(a->model().phase()==GameplayPhase::level2PreloopReturn || a->model().phase()==GameplayPhase::level2) {
            assert(ordinary<ordinaryLimit);
            GameplayInput input{};input.fire=ordinary==1;
            sameDelivery(a->advanceRecoveredLevel2Frame(FrameCopyProfile::modeXNoop,10000,input),
                b->advanceRecoveredLevel2Frame(FrameCopyProfile::modeXNoop,257,input));
            sameFrame(*a,*b);++ordinary;
        }
        assert(a->model().phase()==GameplayPhase::failurePreExplosionBlackout && a->model().playerArmor()<0);
        unsigned failure=0;
        while(a->model().phase()==GameplayPhase::failurePreExplosionBlackout || a->model().phase()==GameplayPhase::failureExplosion) {
            assert(failure<destructionCalls);
            sameDelivery(a->advanceRecoveredFailure(FrameCopyProfile::modeXNoop,10000),
                b->advanceRecoveredFailure(FrameCopyProfile::modeXNoop,257));
            sameFrame(*a,*b);++failure;
        }
        assert(failure==destructionCalls && a->model().phase()==GameplayPhase::failurePostExplosionBlackout);
        const auto scoreBefore=a->model().score();
        const auto budget=a->model().remainingContinues();
        const auto ticks=a->model().ticks();
        const auto continueStep=[&](ContinueKeyboardPacket input) {
            const auto firstCycle=a->synth().driverState().normalClockCycles;
            sameDelivery(a->advanceRecoveredContinue(input,10000),b->advanceRecoveredContinue(input,257));
            sameFrame(*a,*b);assert(a->model().score()==scoreBefore && a->model().ticks()==ticks);
            sameWork(a->recoveredContinue()->work,b->recoveredContinue()->work,firstCycle,a->synth().driverState().normalClockCycles);
        };
        continueStep({});continueStep({});
        ContinueKeyboardPacket up{};up.directions[4]=true;continueStep(up);
        ContinueKeyboardPacket pressed{};pressed.enter=true;continueStep(pressed);continueStep({});
        assert(a->model().recoveredContinueDecision()==ContinueDecision::yes);
        rejectsUnchanged(*a,[&]{a->resumeRecoveredLevel1Retry();});
        const auto rngBefore=a->model().level2Rng().calls();
        const auto retryCycles=a->synth().driverState().normalClockCycles;
        sameDelivery(a->resumeRecoveredLevel2Retry(10000),b->resumeRecoveredLevel2Retry(257));
        sameFrame(*a,*b);
        sameWork(a->recoveredLevel2Retry()->work,b->recoveredLevel2Retry()->work,retryCycles,a->synth().driverState().normalClockCycles);
        for(const auto role:{NativePageRole::front,NativePageRole::back,NativePageRole::cache}) {
            assert(a->retainedPagePixels(role)==b->retainedPagePixels(role));
        }
        assert(a->model().phase()==GameplayPhase::level2PreloopReturn && a->model().campaignLevel()==1);
        assert(a->model().score()==0 && a->model().playerArmor()==680 && a->model().remainingContinues()==budget-1);
        assert(a->model().ticks()==ticks && a->model().level2LoopTick()==0);
        assert(a->model().level2Rng().calls()==rngBefore+24); //12 constructors,12 ground; host replans add none.
        assert(a->recoveredLevel2Transition()==originalSuccess && originalSuccess->samples==successSamples);
        assert(originalSuccess->preloop.ground.rng.calls()==successRng);
        const auto* retry=a->recoveredLevel2Retry();assert(retry && retry->sourceGlobalTick==ticks && retry->destinationGlobalTick==ticks);
        assert(retry->scoreFontProviders.empty());
        unsigned scoreProviders=0;
        for(const auto& provider:retry->providers) {
            if(provider.kind==RetryWorkKind::scoreHudProvider) {
                assert(!provider.guestElapsedQualified && provider.sourceEntry==0x7970);++scoreProviders;
            }
        }
        assert(scoreProviders==1);
        std::uint64_t sourceCycles=0;
        for(const auto& work:retry->work) {sourceCycles+=static_cast<std::uint64_t>(work.timing.total.elapsedCycles);}
        assert(retryCycles+sourceCycles==a->synth().driverState().normalClockCycles);
        assert(retry->preloop.firstPrefix.activeOrder.empty() && retry->preloop.firstPrefix.refreshRequested);
        assert(a->model().level2ActiveListOrder().empty() && !a->recoveredLevel2Frame());
        rejectsUnchanged(*a,[&]{a->resumeRecoveredLevel2Retry();});
        for(unsigned adjacent=0;adjacent<2;++adjacent) {
            const auto firstCycle=a->synth().driverState().normalClockCycles;
            sameDelivery(a->advanceRecoveredLevel2Frame(FrameCopyProfile::modeXNoop,10000),
                b->advanceRecoveredLevel2Frame(FrameCopyProfile::modeXNoop,257));
            sameFrame(*a,*b);
            assert(a->model().ticks()==ticks+adjacent+1 && a->model().level2LoopTick()==adjacent+1);
            assert(a->recoveredLevel2Frame()->firstFrame==(adjacent==0));
            sameWork(a->recoveredLevel2Frame()->work,b->recoveredLevel2Frame()->work,firstCycle,a->synth().driverState().normalClockCycles);
        }
        std::cout<<"controlled same-owner Level2 loss/failure/Yes/retry+2 exact; ordinary="<<ordinary<<'\n';
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 17;}
}
