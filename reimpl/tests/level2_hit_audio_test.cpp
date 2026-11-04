#include "stellar7/native_session.hpp"
#include "stellar7/palette_clock.hpp"
#include <iostream>
namespace {
void level2HitLiveChecks(stellar7::PresentationClock&,stellar7::SoundBlasterSynth&,stellar7::SoundBlasterClockPhase&);
}
#define main included_level2_music_main
#include "level2_music_fixture.inc"
#undef main

namespace {
NormalWorkOperation hit(const std::uint16_t event) {return {event==gameplayEnemyType17Event
    ? NormalWorkKind::gameplayLevel2EnemyFireEvent : event==gameplayPlayerCombatEvent
    ? NormalWorkKind::gameplayLevel2PlayerCombatEvent : NormalWorkKind::gameplayLevel2TargetHitEvent,1,event};}
NormalWorkOperation oldHit(const std::uint16_t event) {return {event==gameplayEnemyType17Event
    ? NormalWorkKind::gameplayEnemyFireEvent : event==gameplayPlayerCombatEvent
    ? NormalWorkKind::gameplayPlayerCombatEvent : NormalWorkKind::gameplayTargetHitEvent,1,event};}
SoundBlasterForegroundKind receiptKind(const std::uint16_t event) {
    return event==gameplayEnemyType17Event ? SoundBlasterForegroundKind::gameplayLevel2EnemyDirectState
        : event==gameplayPlayerCombatEvent ? SoundBlasterForegroundKind::gameplayLevel2PlayerCombatDirectState
        : SoundBlasterForegroundKind::gameplayLevel2TargetHitDirectState;
}

SoundBlasterClockPhase readyLevel2(const std::array<std::uint16_t,3>& timers) {
    auto phase=level2MusicTiming(level2Fixture(timers),Level2MusicChild::emptyStop).phase;
    phase=bindLevel2MusicResource(phase,level2Sequence());
    phase=level2MusicTiming(phase,Level2MusicChild::activation).phase;
    phase=isolatedChildPhase(level2MusicTiming(phase,Level2MusicChild::directReset).phase);
    phase.callback->program->playerHitVolumeRow=syntheticRow();
    return phase;
}

void componentChecks() {
    const auto palettePhase=readyLevel2({0,0,0});PaletteWorkState palette{};palette.source.fill(63);
    const PaletteRequest shortPalette{17,1,0,3};
    const auto shortBound=paletteClockTiming(palettePhase,palette,shortPalette,horizon);
    const auto longBound=paletteClockTiming(palettePhase,palette,shortPalette,2*horizon);
    assert(shortBound.timing.total.elapsedCycles==longBound.timing.total.elapsedCycles);
    assert(shortBound.timing.total.dispatches==longBound.timing.total.dispatches);
    assert(callbackFields(*shortBound.timing.phase.callback)==callbackFields(*longBound.timing.phase.callback));
    assert(shortBound.writes.size()==longBound.writes.size());
    for(std::size_t i=0;i<shortBound.writes.size();++i) {
        const auto& a=shortBound.writes[i];const auto& b=longBound.writes[i];
        assert(std::tie(a.cycleOffset,a.port,a.value)==std::tie(b.cycleOffset,b.port,b.value));
    }
    paletteClockTiming(palettePhase,palette,{0,256,238,59},2*horizon);
    rejects([&]{paletteClockTiming(palettePhase,palette,shortPalette,2*horizon+1);});
    for(unsigned mutation=0;mutation<4;++mutation) {
        auto invalid=palettePhase;
        if(mutation==0) {invalid.sequenceSelection->published=false;}
        if(mutation==1) {invalid.sequenceSelection->songOrdinal=3;}
        if(mutation==2) {invalid.sequenceSelection->level1DirectResetCompleted=false;}
        if(mutation==3) {invalid.successAudio->stage=SuccessAudioStage::playing;}
        rejects([&]{paletteClockTiming(invalid,palette,shortPalette,2*horizon);});
    }
    for(const auto event:{gameplayTargetImpactEvent,gameplayPlayerHitEvent,gameplayEnemyType17Event,gameplayPlayerCombatEvent}) {
        for(const auto timers:{std::array<std::uint16_t,3>{0,0,0},
                std::array<std::uint16_t,3>{4,0,1},std::array<std::uint16_t,3>{4,2,1}}) {
            const auto phase=readyLevel2(timers);
            rejects([&]{normalWorkTiming(phase,{oldHit(event)},horizon);});
            const auto work=normalWorkTiming(phase,{hit(event)},horizon);
            auto oldDomain=phase;oldDomain.sequenceSelection->songOrdinal=2;
            const auto old=normalWorkTiming(oldDomain,{oldHit(event)},horizon);
            assert(work.total.elapsedCycles==old.total.elapsedCycles&&work.total.dispatches==old.total.dispatches);
            assert(callbackFields(*work.phase.callback)==callbackFields(*old.phase.callback));
            assert(work.phase.sequenceSelection->songOrdinal==level2MusicIdentity);
            assert(work.soundBlasterEffects.size()==old.soundBlasterEffects.size());
            for(std::size_t i=0;i<work.soundBlasterEffects.size();++i) {
                const auto& actual=work.soundBlasterEffects[i];const auto& expected=old.soundBlasterEffects[i];
                assert(actual.cycleOffset==expected.cycleOffset&&actual.value==expected.value);
                const auto receipt=expected.kind==SoundBlasterForegroundKind::gameplayTargetImpactDirectState
                    ||expected.kind==SoundBlasterForegroundKind::gameplayPlayerHitDirectState
                    ||expected.kind==SoundBlasterForegroundKind::gameplayEnemyDirectState
                    ||expected.kind==SoundBlasterForegroundKind::gameplayPlayerCombatDirectState;
                assert(actual.kind==(receipt?receiptKind(event):expected.kind));
            }
            const auto entry=normalWorkTiming(phase,{{NormalWorkKind::instructions,event==gameplayEnemyType17Event?46U:47U},{NormalWorkKind::cli,1}},horizon);
            const auto direct=soundBlasterDirectAdvance(*entry.phase.callback,
                event==gameplayEnemyType17Event?gameplayEnemyType17DirectRecord:event==gameplayPlayerHitEvent?gameplayPlayerHitDirectRecord
                    :event==gameplayPlayerCombatEvent?gameplayPlayerCombatDirectRecord:gameplayTargetImpactDirectRecord,
                event==gameplayEnemyType17Event?SoundBlasterDirectCaller::gameplayEnemyFire:event==gameplayPlayerHitEvent?SoundBlasterDirectCaller::gameplayPlayerHit
                    :event==gameplayPlayerCombatEvent?SoundBlasterDirectCaller::gameplayPlayerCombat:SoundBlasterDirectCaller::gameplayTargetImpact);
            const auto receipt=std::find_if(work.soundBlasterEffects.begin(),work.soundBlasterEffects.end(),
                [&](const auto& effect){return effect.kind==receiptKind(event);});
            assert(receipt!=work.soundBlasterEffects.end()&&receipt->directState);
            assert(callbackFields(*receipt->directState)==callbackFields(direct.state));
        }
        const auto valid=readyLevel2({0,0,0});
        for(unsigned mutation=0;mutation<11;++mutation) {
            auto phase=valid;auto operation=hit(event);
            if(mutation==0) {operation.count=2;}
            if(mutation==1) {operation.value=15;}
            if(mutation==2) {phase.sequenceSelection->songOrdinal=2;}
            if(mutation==3) {phase.sequenceSelection->published=false;}
            if(mutation==4) {phase.sequenceSelection->level1DirectResetCompleted=false;}
            if(mutation==5) {phase.successAudio->stage=SuccessAudioStage::playing;}
            if(mutation==6) {phase.sequenceSelection->prepared.sequence.clear();}
            if(mutation==7) {phase.callback->program->repeatCount=1;}
            if(mutation==8) {phase.callback->active=false;}
            if(mutation==9) {phase.callback->fade=1;}
            if(mutation==10) {phase.interrupts=AudioClockInterrupts::masked;}
            bool rejected=false;
            try {normalWorkTiming(phase,{operation},horizon);} catch(const std::invalid_argument&) {rejected=true;}
            if(!rejected) {std::cerr<<"accepted phase mutation event="<<event<<" mutation="<<mutation<<'\n';}
            assert(rejected);
        }
        for(const auto until:{1,80}) {
            auto phase=valid;const auto local=5000-phase.remainingCycles-phase.deferredCycles;
            phase.remainingCycles=until;phase.deferredCycles=5000-local-until;
            phase.devices->irq0Requested=true;phase.irqCheckRequested=true;phase.timer->callbackCount=1;
            const auto work=normalWorkTiming(phase,{hit(event)},horizon);
            assert(work.callbacks.size()==1&&work.phase.sequenceSelection->songOrdinal==level2MusicIdentity);
        }
    }
    const auto failurePhase=readyLevel2({0,0,0});
    const auto ordinaryHit=normalWorkTiming(failurePhase,{hit(gameplayPlayerHitEvent)},horizon);
    const auto failureHit=normalWorkTiming(failurePhase,{{NormalWorkKind::failureLevel2ExplosionEvent,1,6}},horizon);
    assert(ordinaryHit.total.elapsedCycles==failureHit.total.elapsedCycles);
    assert(callbackFields(*ordinaryHit.phase.callback)==callbackFields(*failureHit.phase.callback));
    assert(std::any_of(failureHit.soundBlasterEffects.begin(),failureHit.soundBlasterEffects.end(),[](const auto& effect) {
        return effect.kind==SoundBlasterForegroundKind::failureLevel2PlayerHitDirectState;
    }));
    rejects([&]{normalWorkTiming(failurePhase,{{NormalWorkKind::failureLevel2ExplosionEvent,1,gameplayTargetImpactEvent}},horizon);});
    auto noRow=readyLevel2({0,0,0});noRow.callback->program->playerHitVolumeRow.reset();
    rejects([&]{normalWorkTiming(noRow,{hit(gameplayPlayerHitEvent)},horizon);});
    normalWorkTiming(noRow,{hit(gameplayTargetImpactEvent)},horizon);
}

void level2HitLiveChecks(PresentationClock& clock,SoundBlasterSynth& synth,SoundBlasterClockPhase& phase) {
    for(const auto event:{gameplayTargetImpactEvent,gameplayPlayerHitEvent,gameplayEnemyType17Event,gameplayPlayerCombatEvent}) {
        rejects([&]{clock.renderWork(phase,{oldHit(event)},horizon);});
        const auto work=normalWorkTiming(phase,{hit(event)},horizon);
        const auto cycle=synth.driverState().normalClockCycles;
        const auto samples=clock.elapsedSamples();
        const auto state=callbackFields(synth.callbackWorkState());
        const auto writes=synth.registerWrites().size();
        for(unsigned mutation=0;mutation<9;++mutation) {
            auto bad=work;
            auto receipt=std::find_if(bad.soundBlasterEffects.begin(),bad.soundBlasterEffects.end(),
                [&](const auto& effect){return effect.kind==receiptKind(event);});
            assert(receipt!=bad.soundBlasterEffects.end());
            if(mutation==0) {receipt->value=15;}
            if(mutation==1) {receipt->directState.reset();}
            if(mutation==2) {
                auto changed=std::make_shared<SoundBlasterCallbackState>(*receipt->directState);
                ++changed->voices.directTimers[0];receipt->directState=changed;
            }
            if(mutation==3) {receipt->kind=event==gameplayEnemyType17Event?SoundBlasterForegroundKind::gameplayEnemyDirectState
                :event==gameplayPlayerHitEvent?SoundBlasterForegroundKind::gameplayPlayerHitDirectState
                :event==gameplayPlayerCombatEvent?SoundBlasterForegroundKind::gameplayPlayerCombatDirectState:SoundBlasterForegroundKind::gameplayTargetImpactDirectState;}
            if(mutation==4) {bad.phase.sequenceSelection->songOrdinal=2;}
            if(mutation==5) {bad.phase.sequenceSelection->prepared.sequence.clear();}
            if(mutation==6) {bad.phase.successAudio->stage=SuccessAudioStage::playing;}
            if(mutation==7) {bad.phase.sequenceSelection->level1DirectResetCompleted=false;}
            if(mutation==8) {bad.soundBlasterEffects.front().value^=1;}
            bool rejected=false;
            try {synth.renderClocked(bad,cycle);} catch(const std::invalid_argument&) {rejected=true;}
            if(!rejected) {std::cerr<<"accepted audio mutation event="<<event<<" mutation="<<mutation<<'\n';}
            assert(rejected);
            assert(callbackFields(synth.callbackWorkState())==state);
            assert(synth.driverState().normalClockCycles==cycle&&clock.elapsedSamples()==samples);
            assert(synth.registerWrites().size()==writes);
        }
        const auto rendered=clock.renderWork(phase,{hit(event)},horizon);phase=rendered.timing.phase;
        assert(callbackFields(synth.callbackWorkState())==callbackFields(*phase.callback));
        assert(synth.driverState().normalClockCycles==cycle+work.total.elapsedCycles);
        assert(phase.sequenceSelection->songOrdinal==level2MusicIdentity);
        auto nextWrite=rendered.audio.writes.begin();
        for(const auto& effect:work.soundBlasterEffects) {
            if(effect.kind!=SoundBlasterForegroundKind::runtimeOplWrite) {continue;}
            const auto write=std::find_if(nextWrite,rendered.audio.writes.end(),
                [&](const auto& item){return item.cycleOffset==effect.cycleOffset;});
            assert(write!=rendered.audio.writes.end());
            assert(write->write.registerNumber==effect.value>>8&&write->write.value==(effect.value&0xff));
            nextWrite=write+1;
        }
    }
    const auto failure=clock.renderWork(phase,{{NormalWorkKind::failureLevel2ExplosionEvent,1,6}},horizon);
    phase=failure.timing.phase;
    assert(callbackFields(synth.callbackWorkState())==callbackFields(*phase.callback));
    const auto continued=clock.renderWork(phase,{{NormalWorkKind::instructions,100000}},horizon);
    assert(!continued.timing.callbacks.empty());phase=continued.timing.phase;
    assert(callbackFields(synth.callbackWorkState())==callbackFields(*phase.callback));
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    componentChecks();level2LiveReplay();return 0;
}
