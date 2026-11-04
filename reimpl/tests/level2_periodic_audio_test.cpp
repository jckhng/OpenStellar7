#include "stellar7/native_session.hpp"
#include "stellar7/bitmap_cache.hpp"
namespace {void level2HitLiveChecks(stellar7::PresentationClock&,stellar7::SoundBlasterSynth&,stellar7::SoundBlasterClockPhase&);}
#define main included_level2_music_main
#include "level2_periodic_music_fixture.inc"
#undef main
#include <iostream>
namespace stellar7 {
struct Enemy20McbTestAccess {
    static std::optional<SoundBlasterEnemy20McbByte> fromCache(const BitmapCache& cache) {
        return SoundBlasterEnemy20McbByte::fromCache(cache);
    }
};
}
namespace {
constexpr NormalWorkOperation event15{NormalWorkKind::gameplayLevel2EnemyType20Event,1,15};
constexpr NormalWorkOperation event9{NormalWorkKind::gameplayLevel2SolidContactEvent,1,9};
SoundBlasterEnemy20McbByte testMcbToken() {
    std::vector<IndexedBitmap> frames{{120,56,std::vector<std::uint8_t>(120*56)},
        {56,6,std::vector<std::uint8_t>(56*6)},{120,18,std::vector<std::uint8_t>(120*18)}};
    frames[2].pixels[2042]=156;
    const auto byte=Enemy20McbTestAccess::fromCache(cacheBitmapGroup(frames,0));
    assert(byte);return *byte;
}
void bindStartupMcb(SoundBlasterSynth& synth,SoundBlasterClockPhase& phase) {
    phase=bindLevel1Enemy20McbAudio(synth,phase,testMcbToken());
}
enum class ArrivalAudioChild { arrival, stop, defeat };
struct ArrivalAudioCase {
    ArrivalAudioChild child;
    NormalWorkOperation operation;
    NormalWorkKind level1Kind;
    SoundBlasterForegroundKind receipt;
    SoundBlasterForegroundKind wrongReceipt;
    std::array<std::uint8_t,8> record;
    SoundBlasterDirectCaller caller;
};
const std::array<ArrivalAudioCase,3> arrivalCases{{
    {ArrivalAudioChild::arrival,{NormalWorkKind::gameplayLevel2ArrivalEvent,1,17},NormalWorkKind::gameplayArrivalEvent,
        SoundBlasterForegroundKind::gameplayLevel2ArrivalDirectState,SoundBlasterForegroundKind::gameplayArrivalDirectState,
        gameplayArrivalDirectRecord,SoundBlasterDirectCaller::gameplayArrival},
    {ArrivalAudioChild::stop,{NormalWorkKind::gameplayLevel2ArrivalStopEvent,1,17},NormalWorkKind::gameplayArrivalStopEvent,
        SoundBlasterForegroundKind::gameplayLevel2ArrivalStopState,SoundBlasterForegroundKind::directState,
        {},SoundBlasterDirectCaller::unownedChannel},
    {ArrivalAudioChild::defeat,{NormalWorkKind::gameplayLevel2GuardianDefeatEvent,1,12},NormalWorkKind::gameplayGuardianDefeatEvent,
        SoundBlasterForegroundKind::gameplayLevel2GuardianDefeatDirectState,SoundBlasterForegroundKind::gameplayGuardianDefeatDirectState,
        gameplayGuardianDefeatDirectRecord,SoundBlasterDirectCaller::gameplayGuardianDefeat}}};
SoundBlasterClockPhase selected() {
    auto p=level2MusicTiming(level2Fixture({0,0,0}),Level2MusicChild::emptyStop).phase;
    p=bindLevel2MusicResource(p,level2Sequence());p=level2MusicTiming(p,Level2MusicChild::activation).phase;
    p=isolatedChildPhase(level2MusicTiming(p,Level2MusicChild::directReset).phase);
    auto payload=std::vector<std::uint8_t>(36);payload[0]=0x84;
    p.callback->program->enemy20CallerWindow=level1Enemy20CallerWindow(payload);return p;
}
void arrivalComponent() {
    for(const auto& item:arrivalCases) {
        for(const auto timers:{std::array<std::uint16_t,3>{0,0,0},std::array<std::uint16_t,3>{4,2,1}}) {
            auto p=selected();p.callback->voices.directTimers=timers;p.callback->program->enemy20McbByte=testMcbToken();
            const auto token=p.callback->program->enemy20McbByte;
            const auto work=normalWorkTiming(p,{item.operation},horizon);
            assert(work.callbacks.empty());
            auto oldPhase=p;oldPhase.sequenceSelection->songOrdinal=2;
            auto oldOperation=item.operation;oldOperation.kind=item.level1Kind;
            const auto old=normalWorkTiming(oldPhase,{oldOperation},horizon);
            assert(work.total.dispatches==old.total.dispatches && work.total.elapsedCycles==old.total.elapsedCycles);
            assert(callbackFields(*work.phase.callback)==callbackFields(*old.phase.callback));
            assert(work.phase.callback->program->enemy20McbByte==token);
            const auto receipt=std::find_if(work.soundBlasterEffects.begin(),work.soundBlasterEffects.end(),
                [&](const auto& effect){return effect.kind==item.receipt;});
            assert(receipt!=work.soundBlasterEffects.end() && receipt->directState && receipt->value==item.operation.value);
            assert(receipt->directState->program->enemy20McbByte==token);
            if(item.child==ArrivalAudioChild::stop) {
                assert(work.total.dispatches==50 && work.soundBlasterEffects.size()==1 && receipt->cycleOffset==0);
                assert(callbackFields(*work.phase.callback)==callbackFields(*p.callback));
                assert(work.phase.interrupts==AudioClockInterrupts::enabled && old.soundBlasterEffects.empty());
            } else {
                const auto direct=soundBlasterDirectAdvance(*p.callback,item.record,item.caller);
                std::uint64_t kernel=0;std::vector<std::uint16_t> expected,actual;
                for(const auto& block:direct.blocks) {
                    kernel+=block.dispatches;
                    if(block.kind==SoundBlasterWorkKind::registerWrite) {
                        expected.push_back(static_cast<std::uint16_t>((block.registerNumber<<8)|block.value));
                    }
                }
                for(const auto& effect:work.soundBlasterEffects) {
                    if(effect.kind==SoundBlasterForegroundKind::runtimeOplWrite) {actual.push_back(effect.value);}
                }
                assert(work.total.dispatches==kernel+74 && !expected.empty() && actual==expected);
                assert(callbackFields(*receipt->directState)==callbackFields(direct.state));
            }
            rejects([&]{normalWorkTiming(p,{oldOperation},horizon);});
            rejects([&]{normalWorkTiming(oldPhase,{item.operation},horizon);});
        }
        auto p=selected();p.callback->program->enemy20McbByte=testMcbToken();
        for(unsigned mutation=0;mutation<13;++mutation) {
            auto wrong=p;auto op=item.operation;
            if(mutation==0) {op.count=2;}
            if(mutation==1) {op.value=6;}
            if(mutation==2) {wrong.sequenceSelection->songOrdinal=2;}
            if(mutation==3) {wrong.sequenceSelection->published=false;}
            if(mutation==4) {wrong.sequenceSelection->level1DirectResetCompleted=false;}
            if(mutation==5) {wrong.successAudio->stage=SuccessAudioStage::playing;}
            if(mutation==6) {++wrong.sequenceSelection->prepared.sequence[1];}
            if(mutation==7) {wrong.callback.reset();}
            if(mutation==8) {wrong.callback->program.reset();}
            if(mutation==9) {wrong.callback->active=false;}
            if(mutation==10) {wrong.callback->driverEnabled=false;}
            if(mutation==11) {wrong.callback->program->handleStatus=0xffff;}
            if(mutation==12) {wrong.interrupts=AudioClockInterrupts::masked;}
            rejects([&]{normalWorkTiming(wrong,{op},horizon);});
        }
        for(const auto until:{1,40}) {
            auto irq=p;const auto local=5000-irq.remainingCycles-irq.deferredCycles;
            irq.remainingCycles=until;irq.deferredCycles=5000-local-until;
            irq.devices->irq0Requested=true;irq.irqCheckRequested=true;irq.timer->callbackCount=1;
            const auto work=normalWorkTiming(irq,{item.operation},horizon);
            assert(work.callbacks.size()==1 && work.phase.callback->program->enemy20McbByte==p.callback->program->enemy20McbByte);
        }
    }
}
void arrivalLiveChecks(PresentationClock& clock,SoundBlasterSynth& synth,SoundBlasterClockPhase& phase) {
    const auto token=phase.callback->program->enemy20McbByte;
    assert(token && synth.callbackWorkState().program->enemy20McbByte==token);
    for(const auto& item:arrivalCases) {
        const auto planned=normalWorkTiming(phase,{item.operation},horizon);
        const auto cycle=synth.driverState().normalClockCycles,samples=clock.elapsedSamples();
        const auto before=callbackFields(synth.callbackWorkState());const auto count=synth.registerWrites().size();
        for(unsigned mutation=0;mutation<10;++mutation) {
            auto bad=planned;
            auto receipt=std::find_if(bad.soundBlasterEffects.begin(),bad.soundBlasterEffects.end(),
                [&](const auto& effect){return effect.kind==item.receipt;});
            assert(receipt!=bad.soundBlasterEffects.end() && receipt->directState);
            if(mutation==0) {receipt->directState.reset();}
            if(mutation==1) {receipt->kind=item.wrongReceipt;}
            if(mutation==2) {receipt->value=6;}
            if(mutation==3) {bad.phase.sequenceSelection->songOrdinal=2;}
            if(mutation==4) {++bad.phase.sequenceSelection->prepared.sequence[1];}
            if(mutation==5) {bad.phase.successAudio->stage=SuccessAudioStage::playing;}
            if(mutation>=6 && mutation<=8) {
                auto changed=std::make_shared<SoundBlasterCallbackState>(*receipt->directState);
                if(mutation==6) {changed->program->enemy20McbByte.reset();}
                if(mutation==7) {changed->program->enemy20McbByte=testMcbToken();}
                if(mutation==8) {changed->voices.directTimers[0]^=1;}
                receipt->directState=changed;
            }
            if(mutation==9) {
                const auto write=std::find_if(bad.soundBlasterEffects.begin(),bad.soundBlasterEffects.end(),
                    [](const auto& effect){return effect.kind==SoundBlasterForegroundKind::runtimeOplWrite;});
                if(item.child==ArrivalAudioChild::stop) {
                    bad.soundBlasterEffects.insert(bad.soundBlasterEffects.begin(),
                        {SoundBlasterForegroundKind::runtimeOplWrite,receipt->cycleOffset,0xb000,{}});
                } else {assert(write!=bad.soundBlasterEffects.end());write->value^=1;}
            }
            rejects([&]{synth.renderClocked(bad,cycle);});
            assert(callbackFields(synth.callbackWorkState())==before && synth.registerWrites().size()==count
                && synth.callbackWorkState().program->enemy20McbByte==token
                && synth.driverState().normalClockCycles==cycle && clock.elapsedSamples()==samples);
        }
        const auto rendered=clock.renderWork(phase,{item.operation},horizon);phase=rendered.timing.phase;
        assert(rendered.audio.firstSample==samples && synth.callbackWorkState().program->enemy20McbByte==token);
        assert(callbackFields(synth.callbackWorkState())==callbackFields(*phase.callback));
        auto next=rendered.audio.writes.begin();
        for(const auto& effect:planned.soundBlasterEffects) {
            if(effect.kind!=SoundBlasterForegroundKind::runtimeOplWrite) {continue;}
            const auto write=std::find_if(next,rendered.audio.writes.end(),[&](const auto& value){return value.cycleOffset==effect.cycleOffset;});
            assert(write!=rendered.audio.writes.end() && write->write.registerNumber==effect.value>>8 && write->write.value==(effect.value&255));
            next=write+1;
        }
        phase=clock.renderWork(phase,{{NormalWorkKind::instructions,100000}},horizon).timing.phase;
        assert(callbackFields(synth.callbackWorkState())==callbackFields(*phase.callback)
            && phase.callback->program->enemy20McbByte==token);
    }
}
void component() {
    for(const auto op:{event15,event9}) {
        const auto p=selected();
        const auto work=normalWorkTiming(p,{op},horizon);
        const auto direct=soundBlasterDirectAdvance(*p.callback,op.value==15?gameplayEnemyType20DirectRecord:gameplaySolidContactDirectRecord,
            op.value==15?SoundBlasterDirectCaller::gameplayEnemyType20:SoundBlasterDirectCaller::gameplaySolidContact);
        assert(callbackFields(*work.phase.callback)==callbackFields(direct.state));
        const auto receipt=std::find_if(work.soundBlasterEffects.begin(),work.soundBlasterEffects.end(),[&](const auto& e) {
            return e.kind==(op.value==15?SoundBlasterForegroundKind::gameplayLevel2EnemyType20DirectState
                :SoundBlasterForegroundKind::gameplayLevel2SolidContactDirectState);
        });assert(receipt!=work.soundBlasterEffects.end() && receipt->value==op.value);
        auto wrong=p;wrong.sequenceSelection->songOrdinal=2;rejects([&]{normalWorkTiming(wrong,{op},horizon);});
        wrong=p;wrong.successAudio->stage=SuccessAudioStage::playing;rejects([&]{normalWorkTiming(wrong,{op},horizon);});
        wrong=p;++wrong.sequenceSelection->prepared.sequence[1];rejects([&]{normalWorkTiming(wrong,{op},horizon);});
        auto old=op;old.kind=op.value==15?NormalWorkKind::gameplayEnemyFireEvent:NormalWorkKind::gameplaySolidContactEvent;
        rejects([&]{normalWorkTiming(p,{old},horizon);});
        for(const auto until:{1,60}) {
            auto irq=p;const auto local=5000-irq.remainingCycles-irq.deferredCycles;
            irq.remainingCycles=until;irq.deferredCycles=5000-local-until;
            irq.devices->irq0Requested=true;irq.irqCheckRequested=true;irq.timer->callbackCount=1;
            assert(normalWorkTiming(irq,{op},horizon).callbacks.size()==1);
        }
    }
    auto p=selected();p.callback->program->enemy20CallerWindow.reset();
    rejects([&]{normalWorkTiming(p,{event15},horizon);});
    p=selected();p.callback->voices.directTimers={5,4,0};
    rejects([&]{normalWorkTiming(p,{event15},horizon);}); //Lookup35 remains unsupported.
}
void level2HitLiveChecks(PresentationClock& clock,SoundBlasterSynth& synth,SoundBlasterClockPhase& phase) {
    assert(phase.callback->program->enemy20CallerWindow==synth.callbackWorkState().program->enemy20CallerWindow);
    for(const auto op:{event15,event9}) {
        const auto planned=normalWorkTiming(phase,{op},horizon);
        const auto cycle=synth.driverState().normalClockCycles,samples=clock.elapsedSamples();
        const auto before=callbackFields(synth.callbackWorkState());const auto count=synth.registerWrites().size();
        for(unsigned mutation=0;mutation<4;++mutation) {
            auto bad=planned;
            auto receipt=std::find_if(bad.soundBlasterEffects.begin(),bad.soundBlasterEffects.end(),[&](const auto& e) {
                return e.kind==(op.value==15?SoundBlasterForegroundKind::gameplayLevel2EnemyType20DirectState
                    :SoundBlasterForegroundKind::gameplayLevel2SolidContactDirectState);
            });assert(receipt!=bad.soundBlasterEffects.end());
            if(mutation==0) {receipt->directState.reset();}
            if(mutation==1) {bad.phase.sequenceSelection->songOrdinal=2;}
            if(mutation==2) {bad.phase.successAudio->stage=SuccessAudioStage::playing;}
            if(mutation==3) {receipt->kind=op.value==15?SoundBlasterForegroundKind::gameplayEnemyType20DirectState
                :SoundBlasterForegroundKind::gameplaySolidContactDirectState;}
            rejects([&]{synth.renderClocked(bad,cycle);});
            assert(callbackFields(synth.callbackWorkState())==before && synth.registerWrites().size()==count
                && synth.driverState().normalClockCycles==cycle && clock.elapsedSamples()==samples);
        }
        const auto rendered=clock.renderWork(phase,{op},horizon);phase=rendered.timing.phase;
        assert(callbackFields(synth.callbackWorkState())==callbackFields(*phase.callback));
        auto next=rendered.audio.writes.begin();
        for(const auto& effect:planned.soundBlasterEffects) {
            if(effect.kind!=SoundBlasterForegroundKind::runtimeOplWrite) {continue;}
            const auto w=std::find_if(next,rendered.audio.writes.end(),[&](const auto& item) {return item.cycleOffset==effect.cycleOffset;});
            assert(w!=rendered.audio.writes.end() && w->write.registerNumber==effect.value>>8 && w->write.value==(effect.value&255));
            next=w+1;
        }
    }
    phase=clock.renderWork(phase,{{NormalWorkKind::instructions,100000}},horizon).timing.phase;
    assert(callbackFields(synth.callbackWorkState())==callbackFields(*phase.callback));
    arrivalLiveChecks(clock,synth,phase);
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    try {component();arrivalComponent();level2LiveReplay(bindStartupMcb);return 0;}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
