#include "stellar7/native_session.hpp"

#include <iostream>

namespace {
void level2HitLiveChecks(stellar7::PresentationClock&,stellar7::SoundBlasterSynth&,
    stellar7::SoundBlasterClockPhase&);
}
#define main included_level2_music_main
#include "level2_music_fixture.inc"
#undef main

namespace {
struct AbilitySound {
    std::uint16_t event;
    std::array<std::uint8_t,8> record;
    SoundBlasterDirectCaller caller;
};

const std::array<AbilitySound,5> sounds{{
    {gameplaySuperPrimaryFireEvent,gameplaySuperPrimaryFireDirectRecord,SoundBlasterDirectCaller::gameplaySuperPrimaryFire},
    {gameplayTurboActivateEvent,gameplayTurboActivateDirectRecord,SoundBlasterDirectCaller::gameplayTurboActivate},
    {gameplayShieldDefeatEvent,gameplayShieldDefeatDirectRecord,SoundBlasterDirectCaller::gameplayShieldDefeat},
    {gameplayScannerPulseEvent,gameplayScannerPulseDirectRecord,SoundBlasterDirectCaller::gameplayScannerPulse},
    {gameplayDecoyEvent,gameplayDecoyDirectRecord,SoundBlasterDirectCaller::gameplayDecoy},
}};

void componentChecks() {
    for(const auto& sound:sounds) {
        auto incoming=fixture();
        for(std::size_t slot=0;slot<incoming.voices.directTimers.size();++slot) {
            incoming.voices.channels[6+slot]=static_cast<std::uint8_t>(13+slot);
        }
        incoming.voices.directTimers={100,2,60};
        const auto result=soundBlasterPlayableDirectAdvance(incoming,sound.record,sound.caller);
        //216E replaces the smallest signed duration; retained channels/sequence stay owned.
        assert(result.state.voices.directTimers[0]==100 && result.state.voices.directTimers[2]==60);
        assert(result.state.voices.directTimers[1]==(sound.record[2]|(sound.record[3]<<8)));
        assert(result.state.program->channelPrograms[14]==sound.record[0]);
        assert(result.state.voices.notes[7]==sound.record[1]);
        assert(result.state.position==incoming.position && result.state.wait==incoming.wait);
        assert(result.state.program->instrumentBank==incoming.program->instrumentBank);
        assert(result.state.program->volumeTable==incoming.program->volumeTable);
        assert(!result.blocks.empty());
        rejects([&]{soundBlasterDirectAdvance(incoming,sound.record,sound.caller);});
        auto changed=sound.record;
        ++changed[1];
        rejects([&]{soundBlasterPlayableDirectAdvance(incoming,changed,sound.caller);});
        auto faded=incoming;
        faded.fade=1;
        rejects([&]{soundBlasterPlayableDirectAdvance(faded,sound.record,sound.caller);});
        auto ended=incoming;
        ended.active=false;
        ended.program->handleStatus=0xffff;
        const auto betweenCallbacks=soundBlasterPlayableDirectAdvance(ended,sound.record,sound.caller);
        assert(!betweenCallbacks.state.active && betweenCallbacks.state.program->handleStatus==0xffff);
        assert(betweenCallbacks.state.position==ended.position && betweenCallbacks.state.wait==ended.wait);
    }
}

void level2HitLiveChecks(PresentationClock& clock,SoundBlasterSynth& synth,SoundBlasterClockPhase& phase) {
    phase.playableCampaign=true;
    for(const auto& sound:sounds) {
        const auto samples=clock.elapsedSamples();
        const auto cycles=synth.driverState().normalClockCycles;
        const auto writes=synth.registerWrites().size();
        const auto position=phase.callback->position;
        const auto wait=phase.callback->wait;
        const auto allocation=synth.driverState().sequenceAllocations;
        phase.callback=synth.triggerPlayableEvent(sound.event);
        assert(clock.elapsedSamples()==samples && synth.driverState().normalClockCycles==cycles);
        assert(synth.registerWrites().size()>writes);
        assert(synth.driverState().sequenceAllocations==allocation);
        assert(phase.callback->position==position && phase.callback->wait==wait);
        assert(callbackFields(synth.callbackWorkState())==callbackFields(*phase.callback));
        const PageFlipClockState flip{PageFlipSynchronization::enabled,0xa000,0xa800,0x03d4,PageFlipLayout::recoveredStartup,PageFlipWaitDomain::retainedProgram};
        const auto work=clock.renderPageFlip(phase,flip,240000);
        phase=work.work.timing.phase;
        assert(clock.elapsedSamples()>samples && synth.driverState().normalClockCycles>cycles);
        assert(callbackFields(synth.callbackWorkState())==callbackFields(*phase.callback));
    }
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    try {
        componentChecks();
        level2LiveReplay();
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
    std::cout<<"Five exact playable ability direct callers retain the live clock/sequence owner\n";
}
