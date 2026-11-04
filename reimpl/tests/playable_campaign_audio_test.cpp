#include "stellar7/native_session.hpp"
#include <iostream>
namespace {
void level2HitLiveChecks(stellar7::PresentationClock&,stellar7::SoundBlasterSynth&,stellar7::SoundBlasterClockPhase&);
}
#define main included_level2_music_main
#include "level2_music_fixture.inc"
#undef main

namespace {
std::optional<std::vector<std::int16_t>> voiceReference;
std::optional<std::uint64_t> voiceCycles;
void unownedProgramChecks() {
    constexpr std::uint8_t unusedLevel7Program=117;
    auto state=fixture();
    state.voices.channels.fill(0xff);
    state.sounding.fill(0);
    state.sequence.resize(36);
    state.sequence.insert(state.sequence.end(),{0xc1,unusedLevel7Program,0,0x91,0x4e,0x18,1,0xfc});
    const auto result=soundBlasterProgramCallbackAdvance(state);
    assert(result.state.program->channelPrograms[1]==unusedLevel7Program);
    assert(result.state.program->instrumentBank==state.program->instrumentBank);
    assert(std::none_of(result.blocks.begin(),result.blocks.end(),[](const auto& block) {
        return block.kind==SoundBlasterWorkKind::registerWrite;
    }));
    state.voices.channels[0]=1;
    rejects([&]{soundBlasterProgramCallbackAdvance(state);});
}
void level2HitLiveChecks(PresentationClock& clock,SoundBlasterSynth& synth,SoundBlasterClockPhase& phase) {
    const auto firstCycle=synth.driverState().normalClockCycles;
    const auto firstSample=clock.elapsedSamples();
    const auto oldAllocations=synth.driverState().sequenceAllocations;
    phase.playableCampaign=true;
    phase.callback=synth.triggerPlayableEvent(gameplayPrimaryFireEvent);
    assert(phase.callback->voices.directTimers[0]!=0 || phase.callback->voices.directTimers[1]!=0
        || phase.callback->voices.directTimers[2]!=0);
    assert(synth.driverState().normalClockCycles==firstCycle && clock.elapsedSamples()==firstSample);
    const auto deathWrites=synth.registerWrites().size();
    auto wrongDeath=gameplayOrdinaryDeathDirectRecord;
    wrongDeath[1]=0;
    rejects([&]{soundBlasterPlayableDirectAdvance(*phase.callback,wrongDeath,
        SoundBlasterDirectCaller::gameplayOrdinaryDeath);});
    auto fadedDeath=*phase.callback;
    fadedDeath.fade=1;
    rejects([&]{soundBlasterPlayableDirectAdvance(fadedDeath,gameplayOrdinaryDeathDirectRecord,
        SoundBlasterDirectCaller::gameplayOrdinaryDeath);});
    phase.callback=synth.triggerPlayableEvent(gameplayOrdinaryDeathEvent);
    const auto deathTimer=std::find(phase.callback->voices.directTimers.begin(),
        phase.callback->voices.directTimers.end(),80);
    assert(deathTimer!=phase.callback->voices.directTimers.end());
    const auto deathSlot=deathTimer-phase.callback->voices.directTimers.begin();
    assert(phase.callback->voices.notes[6+deathSlot]==1);
    assert(phase.callback->program->channelPrograms[13+deathSlot]==44);
    assert(synth.registerWrites().size()>deathWrites);
    assert(synth.driverState().normalClockCycles==firstCycle && clock.elapsedSamples()==firstSample);
    auto sequence=level2Sequence();
    sequence.resize(36);
    sequence.insert(sequence.end(),{0xc1,0,0,0x91,65,90,1,0xfc});
    phase.callback=synth.selectPlayableSequence(sequence,PlayableSequenceMode::repeat);
    assert(phase.callback->sequence==sequence && phase.callback->program->repeatCount==0xffff);
    assert(synth.driverState().sequenceAllocations==oldAllocations);
    assert(synth.driverState().sequenceSelections[2] && !synth.driverState().sequenceSelections[0]);
    phase.sequenceSelection.reset();phase.successAudio.reset();phase.level2RetryMusic.reset();
    const PageFlipClockState flip{PageFlipSynchronization::enabled,0xa000,0xa800,0x3d4,
        PageFlipLayout::recoveredStartup,PageFlipWaitDomain::retainedProgram};
    bool ended=false,restarted=false,injectedAtEof=false;
    std::uint64_t priorCycle=firstCycle,priorSample=firstSample;
    for(unsigned frame=0;frame<100 && !restarted;++frame) {
        const auto work=clock.renderPageFlip(phase,flip,240000);
        assert(work.work.audio.firstSample==priorSample);
        phase=work.work.timing.phase;
        assert(synth.driverState().normalClockCycles>priorCycle && clock.elapsedSamples()>priorSample);
        priorCycle=synth.driverState().normalClockCycles;priorSample=clock.elapsedSamples();
        for(const auto& callback:work.work.timing.callbacks) {
            if(!callback.advance.state.active) {ended=true;}
            else if(ended && injectedAtEof) {restarted=true;}
        }
        if (!phase.callback->active && !injectedAtEof) {
            rejects([&]{soundBlasterDirectAdvance(*phase.callback,gameplayPrimaryFireDirectRecord,
                SoundBlasterDirectCaller::gameplayPrimaryFire);});
            const auto position=phase.callback->position;
            const auto wait=phase.callback->wait;
            phase.callback=synth.triggerPlayableEvent(gameplayPrimaryFireEvent);
            assert(!phase.callback->active && phase.callback->program->handleStatus==0xffff);
            assert(phase.callback->position==position && phase.callback->wait==wait);
            assert(synth.driverState().normalClockCycles==priorCycle && clock.elapsedSamples()==priorSample);
            injectedAtEof=true;
        }
        assert(callbackFields(synth.callbackWorkState())==callbackFields(*phase.callback));
    }
    assert(ended && injectedAtEof && restarted);
    phase.callback=synth.stopPlayableSequence();
    assert(!phase.callback->active && phase.callback->program->handleStatus==0xffff);
    phase.callback=synth.deselectPlayableSequence();
    assert(phase.callback->domain==SoundBlasterCallbackDomain::level2RetryNoHandle);
    assert(synth.driverState().normalClockCycles==priorCycle && clock.elapsedSamples()==priorSample);
    phase.callback=synth.selectPlayableSequence(sequence,PlayableSequenceMode::once);
    assert(synth.driverState().normalClockCycles==priorCycle && clock.elapsedSamples()==priorSample);
    for(unsigned frame=0;frame<100 && phase.callback->active;++frame) {
        const auto work=clock.renderPageFlip(phase,flip,240000);
        phase=work.work.timing.phase;
    }
    assert(!phase.callback->active && phase.callback->program->repeatCount==1);
    assert(phase.callback->program->handleStatus==0xffff);
    assert(synth.driverState().clockSource==SoundBlasterClockSource::normalWork);
    assert(synth.driverState().normalClockCycles>firstCycle && clock.elapsedSamples()>firstSample);
    const auto beforeStop=synth.driverState().normalClockCycles;
    const auto beforeVoiceSample=clock.elapsedSamples();
    phase.callback=synth.stopPlayableSequence();
    assert(!phase.callback->active && synth.driverState().sequenceSelections[2]);
    phase.callback=synth.deselectPlayableSequence();
    assert(phase.callback->domain==SoundBlasterCallbackDomain::level2RetryNoHandle);
    assert(!synth.driverState().sequenceSelections[2] && !synth.driverState().sequenceAllocations[2]);
    assert(synth.driverState().normalClockCycles==beforeStop && clock.elapsedSamples()==beforeVoiceSample);
    const auto paletteWork=clock.renderPalette(phase,{}, {0,vgaPaletteColors,0,0},240000);
    assert(paletteWork.work.audio.firstSample==beforeVoiceSample && !paletteWork.work.audio.samples.empty());
    phase=paletteWork.work.timing.phase;
    assert(phase.callback->domain==SoundBlasterCallbackDomain::level2RetryNoHandle);
    const auto voiceFirstSample=clock.elapsedSamples();
    std::vector<std::uint8_t> speech(81);
    speech[0]=0x84;speech[2]=2;speech[36]=0xfc;
    speech[51]=0xc0;speech[52]=0x2b;speech[69]=0x20;speech[70]=0x4e;
    for (std::size_t sample=0;sample<20000;++sample) {speech.push_back(sample%2==0 ? 0 : 255);}
    synth.loadPlayableSampledVoice(speech);synth.startSampledVoice();phase.callback=synth.callbackWorkState();
    assert(synth.sampledVoiceObservation().phase==SampledVoicePhase::pending);
    std::vector<std::int16_t> pcm;
    const auto append=[&](ClockedPresentationWork work) {
        assert(work.audio.firstSample==voiceFirstSample+pcm.size());
        phase=work.timing.phase;pcm.insert(pcm.end(),work.audio.samples.begin(),work.audio.samples.end());
    };
    append(clock.renderWork(phase,{{NormalWorkKind::playableIrqWaitStore,1,24}},120000));
    bool returned=false;
    const auto packetBudget=voiceReference ? 257U : 1000U;
    while (!returned) {
        auto work=clock.renderWork(phase,{{NormalWorkKind::playableIrqWaitPoll,packetBudget}},120000);
        returned=work.timing.countdownReturned;append(std::move(work));
    }
    assert(phase.timer->globalCount==0 && synth.sampledVoiceObservation().phase==SampledVoicePhase::playing);
    assert(synth.sampledVoiceObservation().transferred>0 && !pcm.empty());
    if (voiceReference) {assert(*voiceReference==pcm && *voiceCycles==synth.driverState().normalClockCycles);}
    else {voiceReference=pcm;voiceCycles=synth.driverState().normalClockCycles;}
    const auto voiceCycle=synth.driverState().normalClockCycles,voiceSample=clock.elapsedSamples();
    auto strict=phase;strict.playableCampaign=false;
    rejects([&]{clock.renderWork(strict,{{NormalWorkKind::instructions,1}},120000);});
    assert(synth.driverState().normalClockCycles==voiceCycle && clock.elapsedSamples()==voiceSample);
    synth.stopSampledVoice();synth.releaseSampledVoice();
    assert(synth.driverState().clockSource==SoundBlasterClockSource::normalWork);
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    try {unownedProgramChecks();level2LiveReplay();level2LiveReplay();return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
