#include "stellar7/native_session.hpp"
#include "stellar7/audio_repeat_work.hpp"
#include <iostream>
namespace {
void level2HitLiveChecks(stellar7::PresentationClock&,stellar7::SoundBlasterSynth&,stellar7::SoundBlasterClockPhase&);
}
#define main included_level2_music_main
#include "level2_music_fixture.inc"
#undef main

namespace {
SoundBlasterClockPhase repeatPhase() {
    auto phase=level2MusicTiming(level2Fixture({0,0,0}),Level2MusicChild::emptyStop).phase;
    phase=bindLevel2MusicResource(phase,level2Sequence());
    phase=level2MusicTiming(phase,Level2MusicChild::activation).phase;
    phase=isolatedChildPhase(level2MusicTiming(phase,Level2MusicChild::directReset).phase);
    //Declared controlled FC predecessor, independent of the real failure history.
    phase.callback->position=phase.callback->sequence.size()-1;
    phase.callback->wait=0;phase.callback->runningStatus=0;
    phase.callback->voices.directTimers={1,2,1};
    return phase;
}
SoundBlasterClockPhase pendingIrq(SoundBlasterClockPhase phase) {
    const auto local=5000-phase.remainingCycles-phase.deferredCycles;
    phase.remainingCycles=1;phase.deferredCycles=5000-local-1;
    phase.devices->irq0Requested=true;phase.irqCheckRequested=true;phase.timer->callbackCount=1;
    return phase;
}
void component() {
    const auto phase=pendingIrq(repeatPhase());
    rejects([&] {soundBlasterCallbackAdvance(*phase.callback);});
    const auto reference=soundBlasterLevel1CallbackAdvance(*phase.callback); //Same qualified source body.
    const auto work=normalWorkTiming(phase,{{NormalWorkKind::instructions,1}},horizon);
    assert(work.callbacks.size()==1);
    const auto& eof=work.callbacks.front().advance;
    assert(eof.kind==reference.kind && eof.blocks.size()==reference.blocks.size());
    for(std::size_t i=0;i<eof.blocks.size();++i) {
        const auto& a=eof.blocks[i];const auto& b=reference.blocks[i];
        assert(a.kind==b.kind && a.dispatches==b.dispatches && a.voice==b.voice
            && a.registerNumber==b.registerNumber && a.value==b.value);
    }
    assert(callbackFields(eof.state)==callbackFields(reference.state));
    assert(!eof.state.active && eof.state.program->handleStatus==0xffff && eof.state.program->repeatCount==0xffff);
    assert(eof.state.position==eof.state.program->loopMarker+2 && eof.state.wait==0);
    const auto restarted=normalWorkTiming(pendingIrq(work.phase),{{NormalWorkKind::instructions,1}},horizon);
    assert(restarted.callbacks.size()==1 && restarted.phase.callback->active);
    assert(restarted.phase.callback->program->handleStatus==0 && restarted.phase.callback->program->repeatCount==0xffff);
    const auto successor=normalWorkTiming(pendingIrq(restarted.phase),{{NormalWorkKind::instructions,1}},horizon);
    assert(successor.callbacks.size()==1 && successor.phase.callback->active);
    for(unsigned mutation=0;mutation<4;++mutation) {
        auto bad=phase;
        if(mutation==0) {bad.sequenceSelection->songOrdinal=1;}
        if(mutation==1) {bad.sequenceSelection->level1DirectResetCompleted=false;}
        if(mutation==2) {++bad.sequenceSelection->prepared.sequence[1];}
        if(mutation==3) {bad.successAudio->stage=SuccessAudioStage::playing;}
        rejects([&] {normalWorkTiming(bad,{{NormalWorkKind::instructions,1}},horizon);});
    }
}
struct RepeatReplay {
    std::uint64_t instructions=0,cycles=0,sampleEnd=0;
    std::vector<std::int16_t> pcm;
    std::vector<std::pair<std::uint64_t,std::uint16_t>> opl;
    SoundBlasterClockPhase phase;
};
std::optional<RepeatReplay> referenceReplay;
std::size_t packetBudget=10000;
auto clockFields(const SoundBlasterClockPhase& p) {
    return std::make_tuple(p.remainingCycles,p.deferredCycles,p.interrupts,p.irqPending,p.irqCheckRequested,
        p.linesDone,p.timer->globalCount,p.timer->callbackCount,p.timer->biosCount,p.devices->picTicks,
        p.devices->irq0Requested,p.devices->irq0InService,p.devices->irq1Requested,p.devices->irq1InService);
}
void level2HitLiveChecks(PresentationClock& clock,SoundBlasterSynth& synth,SoundBlasterClockPhase& phase) {
    RepeatReplay replay;
    bool eof=false,restarted=false,successor=false,historyRejected=false;
    std::size_t callbackCount=0;
    auto preceding=*phase.callback;
    //Safety bound on this declared short synthetic sequence, not a fitted callback schedule.
    constexpr std::uint64_t discoveryPacketLimit=2048;
    const auto packetLimit=referenceReplay ? (referenceReplay->instructions+packetBudget-1)/packetBudget : discoveryPacketLimit;
    for(std::uint64_t packet=0;packet<packetLimit;++packet) {
        if(referenceReplay ? replay.instructions==referenceReplay->instructions : successor) {break;}
        const auto count=referenceReplay ? std::min<std::uint64_t>(packetBudget,referenceReplay->instructions-replay.instructions) : packetBudget;
        const auto origin=synth.driverState().normalClockCycles;
        const auto planned=normalWorkTiming(phase,{{NormalWorkKind::instructions,count}},horizon);
        if(!historyRejected && std::any_of(planned.callbacks.begin(),planned.callbacks.end(),[](const auto& callback) {
            return !callback.advance.state.active && callback.advance.state.program->repeatCount==0xffff;
        })) {
            const auto before=callbackFields(synth.callbackWorkState());
            const auto writes=synth.registerWrites().size();const auto samples=clock.elapsedSamples();
            for(unsigned mutation=0;mutation<2;++mutation) {
                auto bad=planned;
                if(mutation==0) {bad.phase.successAudio->stage=SuccessAudioStage::playing;}
                else {bad.phase.successAudio.reset();}
                rejects([&] {synth.renderClocked(bad,origin);});
                assert(callbackFields(synth.callbackWorkState())==before && synth.registerWrites().size()==writes
                    && synth.driverState().normalClockCycles==origin && clock.elapsedSamples()==samples);
            }
            historyRejected=true;
        }
        const auto work=clock.renderWork(phase,{{NormalWorkKind::instructions,count}},horizon);
        replay.instructions+=count;
        replay.pcm.insert(replay.pcm.end(),work.audio.samples.begin(),work.audio.samples.end());
        phase=work.timing.phase;
        std::int64_t prior=-1;
        for(const auto& write:work.audio.writes) {
            assert(write.cycleOffset>=prior && write.cycleOffset<=work.timing.total.elapsedCycles);prior=write.cycleOffset;
            replay.opl.emplace_back(origin+write.cycleOffset,static_cast<std::uint16_t>((write.write.registerNumber<<8)|write.write.value));
        }
        assert(work.audio.firstSample+work.audio.samples.size()==clock.elapsedSamples());
        assert(synth.driverState().normalClockCycles==origin+work.timing.total.elapsedCycles);
        for(const auto& callback:work.timing.callbacks) {
            const auto& after=callback.advance.state;++callbackCount;
            if(preceding.active && !after.active && after.program->repeatCount==0xffff) {
                eof=true;
                assert(after.program->handleStatus==0xffff && after.wait==0 && after.position==after.program->loopMarker+2);
            } else if(eof && !preceding.active && after.active) {
                restarted=true;assert(after.program->handleStatus==0);
            } else if(restarted && after.active) {successor=true;}
            preceding=after;
        }
        assert(callbackFields(synth.callbackWorkState())==callbackFields(*phase.callback));
        assert(synth.driverState().sequenceSelections[2] && synth.driverState().sequenceAllocations[2]);
    }
    if(!eof || !restarted || !successor || !historyRejected || callbackCount<3) {
        std::cerr<<"repeat budget="<<packetBudget<<" instructions="<<replay.instructions
            <<" eof="<<eof<<" restarted="<<restarted<<" successor="<<successor
            <<" historyRejected="<<historyRejected<<" callbacks="<<callbackCount<<'\n';
    }
    assert(eof && restarted && successor && historyRejected && callbackCount>=3);
    replay.cycles=synth.driverState().normalClockCycles;replay.sampleEnd=clock.elapsedSamples();replay.phase=phase;
    if(referenceReplay) {
        assert(replay.instructions==referenceReplay->instructions && replay.cycles==referenceReplay->cycles
            && replay.sampleEnd==referenceReplay->sampleEnd && replay.pcm==referenceReplay->pcm && replay.opl==referenceReplay->opl);
        assert(clockFields(replay.phase)==clockFields(referenceReplay->phase));
        assert(callbackFields(*replay.phase.callback)==callbackFields(*referenceReplay->phase.callback));
    } else {referenceReplay=std::move(replay);}
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    try {component();level2LiveReplay();packetBudget=257;level2LiveReplay();return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
