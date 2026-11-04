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
void equal(const SoundBlasterCallbackState& actual,const SoundBlasterCallbackState& expected) {
    assert(actual.domain==expected.domain && actual.driverEnabled==expected.driverEnabled);
    assert(actual.position==expected.position && actual.wait==expected.wait && actual.fade==expected.fade);
    assert(actual.mode==expected.mode && actual.operation==expected.operation && actual.runningStatus==expected.runningStatus);
    assert(actual.active==expected.active && actual.gate==expected.gate && actual.latch==expected.latch);
    assert(actual.voices.channels==expected.voices.channels && actual.voices.notes==expected.voices.notes);
    assert(actual.voices.tuning==expected.voices.tuning && actual.voices.directTimers==expected.voices.directTimers);
    assert(actual.voices.channelDeferrals==expected.voices.channelDeferrals);
    assert(actual.velocities==expected.velocities && actual.sounding==expected.sounding);
    assert(actual.channelEnabled==expected.channelEnabled && actual.channelVoiceCursors==expected.channelVoiceCursors);
    assert(actual.sequence==expected.sequence && actual.program && expected.program);
    const auto& a=*actual.program;
    const auto& e=*expected.program;
    assert(a.instrumentBank==e.instrumentBank && a.volumeTable==e.volumeTable && a.playerHitVolumeRow==e.playerHitVolumeRow);
    assert(a.channelPrograms==e.channelPrograms && a.operators==e.operators);
    assert(a.enemy20CallerWindow==e.enemy20CallerWindow);
    assert(a.handleStatus==e.handleStatus && a.repeatCount==e.repeatCount && a.handleVolume==e.handleVolume);
    assert(a.waveformMask==e.waveformMask && a.accumulator==e.accumulator && a.fadeDivider==e.fadeDivider && a.loopMarker==e.loopMarker);
    assert(a.amplitudeDepth==e.amplitudeDepth && a.vibratoDepth==e.vibratoDepth && a.noteSelect==e.noteSelect);
    for (std::size_t v=0;v<a.voices.size();++v) {
        const auto& x=a.voices[v];
        const auto& y=e.voices[v];
        assert(x.program==y.program && x.tuningMagnitude==y.tuningMagnitude);
        assert(x.carrierScale==y.carrierScale && x.carrierFallback==y.carrierFallback);
        assert(x.operatorScale==y.operatorScale && x.operatorFallback==y.operatorFallback && x.operatorDisabled==y.operatorDisabled);
    }
}
void equalBlocks(const SoundBlasterCallbackAdvance& a,const SoundBlasterCallbackAdvance& b) {
    assert(a.dispatches==b.dispatches && a.blocks.size()==b.blocks.size());
    for(std::size_t i=0;i<a.blocks.size();++i) {
        const auto& x=a.blocks[i];const auto& y=b.blocks[i];
        assert(x.kind==y.kind && x.dispatches==y.dispatches && x.voice==y.voice
            && x.registerNumber==y.registerNumber && x.value==y.value);
    }
}
SoundBlasterClockPhase selectedLevel2() {
    auto p=level2MusicTiming(level2Fixture({0,0,0}),Level2MusicChild::emptyStop).phase;
    p=bindLevel2MusicResource(p,level2Sequence());
    p=level2MusicTiming(p,Level2MusicChild::activation).phase;
    return isolatedChildPhase(level2MusicTiming(p,Level2MusicChild::directReset).phase);
}
SoundBlasterClockPhase pendingIrq(SoundBlasterClockPhase p) {
    const auto local=5000-p.remainingCycles-p.deferredCycles;
    p.remainingCycles=1;p.deferredCycles=5000-local-1;
    p.devices->irq0Requested=true;p.irqCheckRequested=true;p.timer->callbackCount=1;
    return p;
}
void component() {
    for(const auto timers:{std::array<std::uint16_t,3>{0,0,0},std::array<std::uint16_t,3>{4,2,1},
            std::array<std::uint16_t,3>{1,2,1}}) {
        auto p=selectedLevel2();
        //Declared pending direct-timer component state after actual initial reset.
        p.callback->voices.directTimers=timers;
        const auto input=*p.callback;
        p=beginLevel2RetryMusic(p);
        assert(p.level2RetryMusic->stage==AudioClockLevel2RetryMusicStage::entered);
        rejects([&]{beginLevel2RetryMusic(p);});
        rejects([&]{level2RetryMusicTiming(p,Level2RetryMusicChild::emptyStop);});
        const auto stopped=level2RetryMusicTiming(p,Level2RetryMusicChild::stop);
        assert(stopped.callbacks.empty());
        p=stopped.phase;
        assert(p.level2RetryMusic->stage==AudioClockLevel2RetryMusicStage::stopped);
        assert(p.callback->domain==SoundBlasterCallbackDomain::level2RetryNoHandle);
        assert(p.callback->voices.directTimers==timers && p.callback->program->handleStatus==0xffff);
        auto expected=soundBlasterStopAdvance(input,SoundBlasterStopProfile::retainedDirectTimers).state;
        expected.active=false;expected.program->handleStatus=0xffff;
        expected=soundBlasterLevel2RetryDeselectState(expected);equal(*p.callback,expected);
        rejects([&]{soundBlasterDeselectState(expected);}); //Old no-handle domain remains strict.
        const auto noCurrent=soundBlasterLevel2RetryNoHandleAdvance(*p.callback);
        auto sameSource=*p.callback;sameSource.domain=SoundBlasterCallbackDomain::successNoHandle;
        const auto reference=soundBlasterSuccessNoHandleAdvance(sameSource);
        equalBlocks(noCurrent,reference);
        auto expectedCallback=reference.state;expectedCallback.domain=SoundBlasterCallbackDomain::level2RetryNoHandle;
        equal(noCurrent.state,expectedCallback);
        const auto irq=normalWorkTiming(pendingIrq(p),{{NormalWorkKind::instructions,1}},horizon);
        assert(irq.callbacks.size()==1 && irq.callbacks[0].advance.kind==SoundBlasterCallbackKind::level2RetryNoHandle);
        equal(irq.callbacks[0].advance.state,noCurrent.state);
        rejects([&]{bindLevel2RetryMusicResource(p,level2Sequence());});
        const auto empty=level2RetryMusicTiming(p,Level2RetryMusicChild::emptyStop);
        assert(empty.total.dispatches==4 && empty.callbacks.empty() && empty.soundBlasterEffects.empty());
        p=empty.phase;equal(*p.callback,expected);
        auto other=level2Sequence();++other[1];
        rejects([&]{bindLevel2RetryMusicResource(p,other);});
        rejects([&]{level2MusicTiming(p,Level2MusicChild::emptyStop);});
        p=bindLevel2RetryMusicResource(p,level2Sequence());
        equal(*p.callback,expected);
        assert(!p.sequenceSelection->published && !p.sequenceSelection->level1DirectResetCompleted);
        rejects([&]{bindLevel2RetryMusicResource(p,level2Sequence());});
        const auto pendingActivation=level2RetryMusicTiming(pendingIrq(p),Level2RetryMusicChild::activation);
        assert(pendingActivation.callbacks.size()==1
            && pendingActivation.callbacks[0].advance.kind==SoundBlasterCallbackKind::level2RetryNoHandle);
        const auto active=level2RetryMusicTiming(p,Level2RetryMusicChild::activation);
        assert(active.total.dispatches==542 && active.callbacks.empty());
        assert(active.phase.callback->sequence==level2Sequence() && active.phase.callback->position==36);
        assert(active.phase.callback->voices.directTimers==timers);
        assert(active.phase.callback->program->repeatCount==0xffff && active.phase.callback->program->handleStatus==0);
        const auto pendingReset=level2RetryMusicTiming(pendingIrq(active.phase),Level2RetryMusicChild::directReset);
        assert(pendingReset.callbacks.size()==1);
        const auto reset=level2RetryMusicTiming(active.phase,Level2RetryMusicChild::directReset);
        assert(reset.total.dispatches==87 && reset.callbacks.empty());
        assert(reset.phase.level2RetryMusic->stage==AudioClockLevel2RetryMusicStage::reset);
        assert(reset.phase.sequenceSelection->songOrdinal==level2MusicIdentity && reset.phase.sequenceSelection->published);
        assert(reset.phase.callback->voices.directTimers==timers);
        rejects([&]{level2RetryMusicTiming(reset.phase,Level2RetryMusicChild::directReset);});
        assert(beginLevel2RetryMusic(reset.phase).level2RetryMusic->stage==AudioClockLevel2RetryMusicStage::entered);
        for(unsigned mutation=0;mutation<5;++mutation) {
            auto bad=selectedLevel2();
            if(mutation==0) {bad.sequenceSelection->songOrdinal=2;}
            if(mutation==1) {bad.sequenceSelection->published=false;}
            if(mutation==2) {bad.sequenceSelection->level1DirectResetCompleted=false;}
            if(mutation==3) {++bad.sequenceSelection->prepared.sequence[1];}
            if(mutation==4) {bad.successAudio->stage=SuccessAudioStage::playing;}
            rejects([&]{beginLevel2RetryMusic(bad);});
        }
        auto forged=p;forged.level2RetryMusic.reset();
        rejects([&]{normalWorkTiming(forged,{{NormalWorkKind::instructions,1}},horizon);});
    }
}
struct Replay {
    std::uint64_t cycles=0,samples=0,callbacks=0;
    std::vector<std::int16_t> pcm;
    std::vector<std::pair<std::uint64_t,std::uint16_t>> opl;
    SoundBlasterClockPhase phase;
};
std::optional<Replay> referenceReplay;
std::size_t packetBudget=10000;
auto clockFields(const SoundBlasterClockPhase& p) {
    return std::make_tuple(p.remainingCycles,p.deferredCycles,p.interrupts,p.irqPending,p.irqCheckRequested,
        p.linesDone,p.timer->globalCount,p.timer->callbackCount,p.timer->biosCount,p.devices->picTicks,
        p.devices->irq0Requested,p.devices->irq0InService,p.devices->irq1Requested,p.devices->irq1InService);
}
void level2HitLiveChecks(PresentationClock& clock,SoundBlasterSynth& synth,SoundBlasterClockPhase& phase) {
    Replay replay;
    auto accept=[&](const ClockedPresentationWork& work) {
        const auto origin=synth.driverState().normalClockCycles-work.timing.total.elapsedCycles;
        replay.pcm.insert(replay.pcm.end(),work.audio.samples.begin(),work.audio.samples.end());
        for(const auto& w:work.audio.writes) {
            replay.opl.emplace_back(origin+w.cycleOffset,static_cast<std::uint16_t>((w.write.registerNumber<<8)|w.write.value));
        }
        replay.callbacks+=work.timing.callbacks.size();phase=work.timing.phase;
        equal(synth.callbackWorkState(),*phase.callback);
        assert(work.audio.firstSample+work.audio.samples.size()==clock.elapsedSamples());
    };
    accept(clock.renderWork(phase,{{NormalWorkKind::failureLevel2ExplosionEvent,1,6}},horizon));
    assert(std::any_of(phase.callback->voices.directTimers.begin(),phase.callback->voices.directTimers.end(),
        [](const auto timer){return timer>0;}));
    phase=beginLevel2RetryMusic(phase);
    const auto planned=level2RetryMusicTiming(phase,Level2RetryMusicChild::stop);
    const auto before=synth.callbackWorkState();const auto cycles=synth.driverState().normalClockCycles;
    const auto samples=clock.elapsedSamples();const auto writes=synth.registerWrites().size();
    for(unsigned mutation=0;mutation<3;++mutation) {
        auto bad=planned;
        auto effect=std::find_if(bad.soundBlasterEffects.begin(),bad.soundBlasterEffects.end(),
            [](const auto& item){return item.kind==SoundBlasterForegroundKind::level2RetryCurrentClear;});
        assert(effect!=bad.soundBlasterEffects.end());
        if(mutation==0) {effect->directState.reset();}
        if(mutation==1) {bad.phase.level2RetryMusic.reset();}
        if(mutation==2) {auto changed=std::make_shared<SoundBlasterCallbackState>(*effect->directState);
            ++changed->voices.directTimers[0];effect->directState=changed;}
        rejects([&]{synth.renderClocked(bad,cycles);});
        equal(synth.callbackWorkState(),before);
        assert(synth.driverState().normalClockCycles==cycles && synth.registerWrites().size()==writes && clock.elapsedSamples()==samples);
    }
    accept(renderLevel2RetryMusic(clock,phase,Level2RetryMusicChild::stop));
    assert(!synth.driverState().sequenceSelections[2] && synth.driverState().sequenceAllocations[2]);
    //Same ordinary source instruction count under two packet partitions while no handle is current.
    std::size_t noCurrentCallbacks=0;
    for(std::size_t left=200000;left!=0;) {
        const auto count=std::min(left,packetBudget);
        const auto work=clock.renderWork(phase,{{NormalWorkKind::instructions,count}},horizon);
        for(const auto& cb:work.timing.callbacks) {
            assert(cb.advance.kind==SoundBlasterCallbackKind::level2RetryNoHandle);++noCurrentCallbacks;
        }
        accept(work);left-=count;
    }
    assert(noCurrentCallbacks>0);
    accept(renderLevel2RetryMusic(clock,phase,Level2RetryMusicChild::emptyStop));
    phase=bindLevel2RetryMusicResource(phase,level2Sequence());
    accept(renderLevel2RetryMusic(clock,phase,Level2RetryMusicChild::activation));
    assert(synth.driverState().sequenceSelections[2] && synth.driverState().sequenceAllocations[2]);
    accept(renderLevel2RetryMusic(clock,phase,Level2RetryMusicChild::directReset));
    for(std::size_t left=200000;left!=0;) {
        const auto count=std::min(left,packetBudget);
        accept(clock.renderWork(phase,{{NormalWorkKind::instructions,count}},horizon));left-=count;
    }
    assert(phase.level2RetryMusic->stage==AudioClockLevel2RetryMusicStage::reset);
    assert(phase.sequenceSelection->songOrdinal==level2MusicIdentity && phase.sequenceSelection->level1DirectResetCompleted);
    replay.cycles=synth.driverState().normalClockCycles;replay.samples=clock.elapsedSamples();replay.phase=phase;
    if(referenceReplay) {
        assert(replay.cycles==referenceReplay->cycles && replay.samples==referenceReplay->samples
            && replay.callbacks==referenceReplay->callbacks && replay.pcm==referenceReplay->pcm && replay.opl==referenceReplay->opl);
        assert(clockFields(replay.phase)==clockFields(referenceReplay->phase));
        equal(*replay.phase.callback,*referenceReplay->phase.callback);
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
