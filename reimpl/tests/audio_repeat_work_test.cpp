#include "stellar7/audio_repeat_work.hpp"
#include "stellar7/audio_program_work.hpp"
#include <cassert>
#include <stdexcept>
#include <utility>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace {
using namespace stellar7;
SoundBlasterCallbackState tables() {
    SoundBlasterCallbackState state{};
    state.program.emplace();
    state.program->instrumentBank.resize(96*28);
    for (std::size_t i=0;i<state.program->instrumentBank.size();++i) {
        state.program->instrumentBank[i]=static_cast<std::uint8_t>((i*7+13)%64);
    }
    state.program->volumeTable.resize(16*64);
    for (std::size_t r=0;r<16;++r) {
        for (std::size_t c=0;c<64;++c) {
            state.program->volumeTable[r*64+c]=static_cast<std::uint8_t>((r*3+c*5)%64);
        }
    }
    return state;
}
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
template<class Function> void rejects(Function function) {
    bool rejected=false;
    try { function(); } catch (const std::invalid_argument&) { rejected=true; }
    assert(rejected);
}

void case0() {
    // header-loop: same owner across FC, restart and wait.
    auto state=tables();
    state.sequence={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,145,65,90,5,252};
    state.position = 40;
    state.wait = 0;
    state.fade = 0;
    state.operation = 2;
    state.runningStatus = 0;
    state.active = 1;
    state.program->handleStatus = 0;
    state.program->repeatCount = 65535;
    state.program->handleVolume = 15;
    state.program->waveformMask = 32;
    state.program->accumulator = 127;
    state.program->fadeDivider = 20;
    state.program->loopMarker = 34;
    state.program->amplitudeDepth = 0;
    state.program->vibratoDepth = 0;
    state.program->noteSelect = 0;
    state.voices.channels = {1,255,255,255,255,255,255,255,255};
    state.voices.notes = {60,61,62,63,64,65,66,67,68};
    state.voices.tuning = {0,0,0,0,0,0,0,0,0};
    state.voices.channelDeferrals = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.channelEnabled = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    state.channelVoiceCursors = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.velocities = {0,40,48,0,64,72,0,88,96};
    state.sounding = {1,1,0,1,1,0,1,1,1};
    state.voices.directTimers = {0,0,0};
    state.program->channelPrograms = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.latch = SoundBlasterLevelLatch::Unlatched;
    state.program->voices[0] = {0,0,0,20,1,31,0};
    state.program->voices[1] = {0,0,1,21,2,30,65535};
    state.program->voices[2] = {0,0,2,22,3,29,0};
    state.program->voices[3] = {0,0,3,23,0,28,65535};
    state.program->voices[4] = {0,0,0,24,1,27,0};
    state.program->voices[5] = {0,0,1,25,2,26,65535};
    state.program->voices[6] = {0,0,2,26,3,25,0};
    state.program->voices[7] = {0,0,3,27,0,24,65535};
    state.program->voices[8] = {0,0,0,28,1,23,0};
    state.program->operators[0] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[3] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[4] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[5] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[6] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[7] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[8] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[9] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[11] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[12] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    rejects([&]{ soundBlasterCallbackAdvance(state); });
    auto unsupported=state;
    unsupported.fade=1;
    rejects([&]{ soundBlasterLevel1CallbackAdvance(unsupported); });
    unsupported=state;
    unsupported.program->repeatCount=2;
    rejects([&]{ soundBlasterLevel1CallbackAdvance(unsupported); });
    unsupported=state;
    unsupported.program->loopMarker=0;
    rejects([&]{ soundBlasterLevel1CallbackAdvance(unsupported); });
    unsupported=state;
    unsupported.program->loopMarker=static_cast<std::uint16_t>(state.sequence.size());
    rejects([&]{ soundBlasterLevel1CallbackAdvance(unsupported); });
    unsupported=state;
    unsupported.sequence[unsupported.position]=0xb1;
    unsupported.sequence.insert(unsupported.sequence.end(),{0x4b,1,1});
    rejects([&]{ soundBlasterLevel1CallbackAdvance(unsupported); });
    {
    auto expected=state;
    expected.position = 36;
    expected.wait = 0;
    expected.fade = 0;
    expected.operation = 2;
    expected.runningStatus = 252;
    expected.active = 0;
    expected.program->handleStatus = 65535;
    expected.program->repeatCount = 65535;
    expected.program->handleVolume = 15;
    expected.program->waveformMask = 32;
    expected.program->accumulator = 127;
    expected.program->fadeDivider = 20;
    expected.program->loopMarker = 34;
    expected.program->amplitudeDepth = 0;
    expected.program->vibratoDepth = 0;
    expected.program->noteSelect = 0;
    expected.voices.channels = {1,255,255,255,255,255,255,255,255};
    expected.voices.notes = {60,61,62,63,64,65,66,67,68};
    expected.voices.tuning = {0,0,0,0,0,0,0,0,0};
    expected.voices.channelDeferrals = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.channelEnabled = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    expected.channelVoiceCursors = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.velocities = {0,40,48,0,64,72,0,88,96};
    expected.sounding = {0,0,0,0,0,0,0,0,0};
    expected.voices.directTimers = {0,0,0};
    expected.program->channelPrograms = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.latch = SoundBlasterLevelLatch::Unlatched;
    expected.program->voices[0] = {0,0,0,20,1,31,0};
    expected.program->voices[1] = {0,0,1,21,2,30,65535};
    expected.program->voices[2] = {0,0,2,22,3,29,0};
    expected.program->voices[3] = {0,0,3,23,0,28,65535};
    expected.program->voices[4] = {0,0,0,24,1,27,0};
    expected.program->voices[5] = {0,0,1,25,2,26,65535};
    expected.program->voices[6] = {0,0,2,26,3,25,0};
    expected.program->voices[7] = {0,0,3,27,0,24,65535};
    expected.program->voices[8] = {0,0,0,28,1,23,0};
    expected.program->operators[0] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[3] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[4] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[5] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[6] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[7] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[8] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[9] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[11] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[12] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    const auto actual=soundBlasterLevel1CallbackAdvance(state);
    assert(actual.dispatches==2715);
    assert(actual.kind==SoundBlasterCallbackKind::programControl);
    const std::vector<SoundBlasterWorkBlock> blocks={
        {SoundBlasterWorkKind::plain,137,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,160,87},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,176,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,161,107},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,177,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,162,129},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,178,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,163,152},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,179,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,164,176},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,180,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,165,202},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,181,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,166,229},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,182,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,167,2},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,183,18},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,168,32},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,184,18},
        {SoundBlasterWorkKind::plain,44,-1,0,0},
    };
    assert(actual.blocks.size()==blocks.size());
    for (std::size_t i=0;i<blocks.size();++i) {
        const auto& a=actual.blocks[i];
        const auto& e=blocks[i];
        assert(a.kind==e.kind && a.dispatches==e.dispatches && a.voice==e.voice && a.registerNumber==e.registerNumber && a.value==e.value);
    }
    equal(actual.state,expected);
    state=actual.state;
    }
    {
    auto expected=state;
    expected.position = 40;
    expected.wait = 4;
    expected.fade = 0;
    expected.operation = 2;
    expected.runningStatus = 145;
    expected.active = 1;
    expected.program->handleStatus = 0;
    expected.program->repeatCount = 65535;
    expected.program->handleVolume = 15;
    expected.program->waveformMask = 32;
    expected.program->accumulator = 127;
    expected.program->fadeDivider = 20;
    expected.program->loopMarker = 34;
    expected.program->amplitudeDepth = 0;
    expected.program->vibratoDepth = 0;
    expected.program->noteSelect = 0;
    expected.voices.channels = {1,255,255,255,255,255,255,255,255};
    expected.voices.notes = {65,61,62,63,64,65,66,67,68};
    expected.voices.tuning = {0,0,0,0,0,0,0,0,0};
    expected.voices.channelDeferrals = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.channelEnabled = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    expected.channelVoiceCursors = {0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.velocities = {90,40,48,0,64,72,0,88,96};
    expected.sounding = {1,0,0,0,0,0,0,0,0};
    expected.voices.directTimers = {0,0,0};
    expected.program->channelPrograms = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.latch = SoundBlasterLevelLatch::Unlatched;
    expected.program->voices[0] = {255,0,0,20,1,31,0};
    expected.program->voices[1] = {0,0,1,21,2,30,65535};
    expected.program->voices[2] = {0,0,2,22,3,29,0};
    expected.program->voices[3] = {0,0,3,23,0,28,65535};
    expected.program->voices[4] = {0,0,0,24,1,27,0};
    expected.program->voices[5] = {0,0,1,25,2,26,65535};
    expected.program->voices[6] = {0,0,2,26,3,25,0};
    expected.program->voices[7] = {0,0,3,27,0,24,65535};
    expected.program->voices[8] = {0,0,0,28,1,23,0};
    expected.program->operators[0] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[3] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[4] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[5] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[6] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[7] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[8] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[9] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[11] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[12] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    const auto actual=soundBlasterLevel1CallbackAdvance(state);
    assert(actual.dispatches==624);
    assert(actual.kind==SoundBlasterCallbackKind::programControl);
    const std::vector<SoundBlasterWorkBlock> blocks={
        {SoundBlasterWorkKind::plain,165,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,67,113},
        {SoundBlasterWorkKind::plain,46,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,160,202},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,176,49},
        {SoundBlasterWorkKind::plain,79,-1,0,0},
    };
    assert(actual.blocks.size()==blocks.size());
    for (std::size_t i=0;i<blocks.size();++i) {
        const auto& a=actual.blocks[i];
        const auto& e=blocks[i];
        assert(a.kind==e.kind && a.dispatches==e.dispatches && a.voice==e.voice && a.registerNumber==e.registerNumber && a.value==e.value);
    }
    equal(actual.state,expected);
    state=actual.state;
    }
    {
    auto expected=state;
    expected.position = 40;
    expected.wait = 3;
    expected.fade = 0;
    expected.operation = 2;
    expected.runningStatus = 145;
    expected.active = 1;
    expected.program->handleStatus = 0;
    expected.program->repeatCount = 65535;
    expected.program->handleVolume = 15;
    expected.program->waveformMask = 32;
    expected.program->accumulator = 127;
    expected.program->fadeDivider = 20;
    expected.program->loopMarker = 34;
    expected.program->amplitudeDepth = 0;
    expected.program->vibratoDepth = 0;
    expected.program->noteSelect = 0;
    expected.voices.channels = {1,255,255,255,255,255,255,255,255};
    expected.voices.notes = {65,61,62,63,64,65,66,67,68};
    expected.voices.tuning = {0,0,0,0,0,0,0,0,0};
    expected.voices.channelDeferrals = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.channelEnabled = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    expected.channelVoiceCursors = {0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.velocities = {90,40,48,0,64,72,0,88,96};
    expected.sounding = {1,0,0,0,0,0,0,0,0};
    expected.voices.directTimers = {0,0,0};
    expected.program->channelPrograms = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.latch = SoundBlasterLevelLatch::Unlatched;
    expected.program->voices[0] = {255,0,0,20,1,31,0};
    expected.program->voices[1] = {0,0,1,21,2,30,65535};
    expected.program->voices[2] = {0,0,2,22,3,29,0};
    expected.program->voices[3] = {0,0,3,23,0,28,65535};
    expected.program->voices[4] = {0,0,0,24,1,27,0};
    expected.program->voices[5] = {0,0,1,25,2,26,65535};
    expected.program->voices[6] = {0,0,2,26,3,25,0};
    expected.program->voices[7] = {0,0,3,27,0,24,65535};
    expected.program->voices[8] = {0,0,0,28,1,23,0};
    expected.program->operators[0] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[3] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[4] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[5] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[6] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[7] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[8] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[9] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[11] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[12] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    const auto actual=soundBlasterLevel1CallbackAdvance(state);
    assert(actual.dispatches==73);
    assert(actual.kind==SoundBlasterCallbackKind::programWait);
    const std::vector<SoundBlasterWorkBlock> blocks={
        {SoundBlasterWorkKind::plain,73,-1,0,0},
    };
    assert(actual.blocks.size()==blocks.size());
    for (std::size_t i=0;i<blocks.size();++i) {
        const auto& a=actual.blocks[i];
        const auto& e=blocks[i];
        assert(a.kind==e.kind && a.dispatches==e.dispatches && a.voice==e.voice && a.registerNumber==e.registerNumber && a.value==e.value);
    }
    equal(actual.state,expected);
    state=actual.state;
    }
}

void case1() {
    // interior-loop: same owner across FC, restart and wait.
    auto state=tables();
    state.sequence={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,193,7,0,207,127,0,145,65,90,5,252};
    state.position = 46;
    state.wait = 0;
    state.fade = 0;
    state.operation = 2;
    state.runningStatus = 0;
    state.active = 1;
    state.program->handleStatus = 0;
    state.program->repeatCount = 65535;
    state.program->handleVolume = 15;
    state.program->waveformMask = 32;
    state.program->accumulator = 127;
    state.program->fadeDivider = 20;
    state.program->loopMarker = 37;
    state.program->amplitudeDepth = 0;
    state.program->vibratoDepth = 0;
    state.program->noteSelect = 0;
    state.voices.channels = {1,255,255,255,255,255,255,255,255};
    state.voices.notes = {60,61,62,63,64,65,66,67,68};
    state.voices.tuning = {0,0,0,0,0,0,0,0,0};
    state.voices.channelDeferrals = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.channelEnabled = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    state.channelVoiceCursors = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.velocities = {0,40,48,0,64,72,0,88,96};
    state.sounding = {1,1,0,1,1,0,1,1,1};
    state.voices.directTimers = {0,0,0};
    state.program->channelPrograms = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.latch = SoundBlasterLevelLatch::Unlatched;
    state.program->voices[0] = {0,0,0,20,1,31,0};
    state.program->voices[1] = {0,0,1,21,2,30,65535};
    state.program->voices[2] = {0,0,2,22,3,29,0};
    state.program->voices[3] = {0,0,3,23,0,28,65535};
    state.program->voices[4] = {0,0,0,24,1,27,0};
    state.program->voices[5] = {0,0,1,25,2,26,65535};
    state.program->voices[6] = {0,0,2,26,3,25,0};
    state.program->voices[7] = {0,0,3,27,0,24,65535};
    state.program->voices[8] = {0,0,0,28,1,23,0};
    state.program->operators[0] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[3] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[4] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[5] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[6] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[7] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[8] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[9] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[11] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[12] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    rejects([&]{ soundBlasterCallbackAdvance(state); });
    {
    auto expected=state;
    expected.position = 39;
    expected.wait = 0;
    expected.fade = 0;
    expected.operation = 2;
    expected.runningStatus = 252;
    expected.active = 0;
    expected.program->handleStatus = 65535;
    expected.program->repeatCount = 65535;
    expected.program->handleVolume = 15;
    expected.program->waveformMask = 32;
    expected.program->accumulator = 127;
    expected.program->fadeDivider = 20;
    expected.program->loopMarker = 37;
    expected.program->amplitudeDepth = 0;
    expected.program->vibratoDepth = 0;
    expected.program->noteSelect = 0;
    expected.voices.channels = {1,255,255,255,255,255,255,255,255};
    expected.voices.notes = {60,61,62,63,64,65,66,67,68};
    expected.voices.tuning = {0,0,0,0,0,0,0,0,0};
    expected.voices.channelDeferrals = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.channelEnabled = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    expected.channelVoiceCursors = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.velocities = {0,40,48,0,64,72,0,88,96};
    expected.sounding = {0,0,0,0,0,0,0,0,0};
    expected.voices.directTimers = {0,0,0};
    expected.program->channelPrograms = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.latch = SoundBlasterLevelLatch::Unlatched;
    expected.program->voices[0] = {0,0,0,20,1,31,0};
    expected.program->voices[1] = {0,0,1,21,2,30,65535};
    expected.program->voices[2] = {0,0,2,22,3,29,0};
    expected.program->voices[3] = {0,0,3,23,0,28,65535};
    expected.program->voices[4] = {0,0,0,24,1,27,0};
    expected.program->voices[5] = {0,0,1,25,2,26,65535};
    expected.program->voices[6] = {0,0,2,26,3,25,0};
    expected.program->voices[7] = {0,0,3,27,0,24,65535};
    expected.program->voices[8] = {0,0,0,28,1,23,0};
    expected.program->operators[0] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[3] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[4] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[5] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[6] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[7] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[8] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[9] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[11] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[12] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    const auto actual=soundBlasterLevel1CallbackAdvance(state);
    assert(actual.dispatches==2715);
    assert(actual.kind==SoundBlasterCallbackKind::programControl);
    const std::vector<SoundBlasterWorkBlock> blocks={
        {SoundBlasterWorkKind::plain,137,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,160,87},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,176,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,161,107},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,177,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,162,129},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,178,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,163,152},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,179,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,164,176},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,180,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,165,202},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,181,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,166,229},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,182,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,167,2},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,183,18},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,168,32},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,184,18},
        {SoundBlasterWorkKind::plain,44,-1,0,0},
    };
    assert(actual.blocks.size()==blocks.size());
    for (std::size_t i=0;i<blocks.size();++i) {
        const auto& a=actual.blocks[i];
        const auto& e=blocks[i];
        assert(a.kind==e.kind && a.dispatches==e.dispatches && a.voice==e.voice && a.registerNumber==e.registerNumber && a.value==e.value);
    }
    equal(actual.state,expected);
    state=actual.state;
    }
    {
    auto expected=state;
    expected.position = 46;
    expected.wait = 4;
    expected.fade = 0;
    expected.operation = 2;
    expected.runningStatus = 145;
    expected.active = 1;
    expected.program->handleStatus = 0;
    expected.program->repeatCount = 65535;
    expected.program->handleVolume = 15;
    expected.program->waveformMask = 32;
    expected.program->accumulator = 127;
    expected.program->fadeDivider = 20;
    expected.program->loopMarker = 37;
    expected.program->amplitudeDepth = 0;
    expected.program->vibratoDepth = 0;
    expected.program->noteSelect = 0;
    expected.voices.channels = {1,255,255,255,255,255,255,255,255};
    expected.voices.notes = {65,61,62,63,64,65,66,67,68};
    expected.voices.tuning = {0,0,0,0,0,0,0,0,0};
    expected.voices.channelDeferrals = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.channelEnabled = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    expected.channelVoiceCursors = {0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.velocities = {90,40,48,0,64,72,0,88,96};
    expected.sounding = {1,0,0,0,0,0,0,0,0};
    expected.voices.directTimers = {0,0,0};
    expected.program->channelPrograms = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.latch = SoundBlasterLevelLatch::Unlatched;
    expected.program->voices[0] = {255,0,0,20,1,31,0};
    expected.program->voices[1] = {0,0,1,21,2,30,65535};
    expected.program->voices[2] = {0,0,2,22,3,29,0};
    expected.program->voices[3] = {0,0,3,23,0,28,65535};
    expected.program->voices[4] = {0,0,0,24,1,27,0};
    expected.program->voices[5] = {0,0,1,25,2,26,65535};
    expected.program->voices[6] = {0,0,2,26,3,25,0};
    expected.program->voices[7] = {0,0,3,27,0,24,65535};
    expected.program->voices[8] = {0,0,0,28,1,23,0};
    expected.program->operators[0] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[3] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[4] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[5] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[6] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[7] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[8] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[9] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[11] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[12] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    const auto actual=soundBlasterLevel1CallbackAdvance(state);
    assert(actual.dispatches==692);
    assert(actual.kind==SoundBlasterCallbackKind::programControl);
    const std::vector<SoundBlasterWorkBlock> blocks={
        {SoundBlasterWorkKind::plain,233,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,67,113},
        {SoundBlasterWorkKind::plain,46,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,160,202},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,176,49},
        {SoundBlasterWorkKind::plain,79,-1,0,0},
    };
    assert(actual.blocks.size()==blocks.size());
    for (std::size_t i=0;i<blocks.size();++i) {
        const auto& a=actual.blocks[i];
        const auto& e=blocks[i];
        assert(a.kind==e.kind && a.dispatches==e.dispatches && a.voice==e.voice && a.registerNumber==e.registerNumber && a.value==e.value);
    }
    equal(actual.state,expected);
    state=actual.state;
    }
    {
    auto expected=state;
    expected.position = 46;
    expected.wait = 3;
    expected.fade = 0;
    expected.operation = 2;
    expected.runningStatus = 145;
    expected.active = 1;
    expected.program->handleStatus = 0;
    expected.program->repeatCount = 65535;
    expected.program->handleVolume = 15;
    expected.program->waveformMask = 32;
    expected.program->accumulator = 127;
    expected.program->fadeDivider = 20;
    expected.program->loopMarker = 37;
    expected.program->amplitudeDepth = 0;
    expected.program->vibratoDepth = 0;
    expected.program->noteSelect = 0;
    expected.voices.channels = {1,255,255,255,255,255,255,255,255};
    expected.voices.notes = {65,61,62,63,64,65,66,67,68};
    expected.voices.tuning = {0,0,0,0,0,0,0,0,0};
    expected.voices.channelDeferrals = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.channelEnabled = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    expected.channelVoiceCursors = {0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.velocities = {90,40,48,0,64,72,0,88,96};
    expected.sounding = {1,0,0,0,0,0,0,0,0};
    expected.voices.directTimers = {0,0,0};
    expected.program->channelPrograms = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.latch = SoundBlasterLevelLatch::Unlatched;
    expected.program->voices[0] = {255,0,0,20,1,31,0};
    expected.program->voices[1] = {0,0,1,21,2,30,65535};
    expected.program->voices[2] = {0,0,2,22,3,29,0};
    expected.program->voices[3] = {0,0,3,23,0,28,65535};
    expected.program->voices[4] = {0,0,0,24,1,27,0};
    expected.program->voices[5] = {0,0,1,25,2,26,65535};
    expected.program->voices[6] = {0,0,2,26,3,25,0};
    expected.program->voices[7] = {0,0,3,27,0,24,65535};
    expected.program->voices[8] = {0,0,0,28,1,23,0};
    expected.program->operators[0] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[3] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[4] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[5] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[6] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[7] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[8] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[9] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[11] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[12] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    const auto actual=soundBlasterLevel1CallbackAdvance(state);
    assert(actual.dispatches==73);
    assert(actual.kind==SoundBlasterCallbackKind::programWait);
    const std::vector<SoundBlasterWorkBlock> blocks={
        {SoundBlasterWorkKind::plain,73,-1,0,0},
    };
    assert(actual.blocks.size()==blocks.size());
    for (std::size_t i=0;i<blocks.size();++i) {
        const auto& a=actual.blocks[i];
        const auto& e=blocks[i];
        assert(a.kind==e.kind && a.dispatches==e.dispatches && a.voice==e.voice && a.registerNumber==e.registerNumber && a.value==e.value);
    }
    equal(actual.state,expected);
    state=actual.state;
    }
}

void case2() {
    // interior-timers-latch: same owner across FC, restart and wait.
    auto state=tables();
    state.sequence={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,193,7,0,207,127,0,145,65,90,5,252};
    state.position = 46;
    state.wait = 0;
    state.fade = 0;
    state.operation = 2;
    state.runningStatus = 0;
    state.active = 1;
    state.program->handleStatus = 0;
    state.program->repeatCount = 65535;
    state.program->handleVolume = 15;
    state.program->waveformMask = 32;
    state.program->accumulator = 127;
    state.program->fadeDivider = 20;
    state.program->loopMarker = 37;
    state.program->amplitudeDepth = 0;
    state.program->vibratoDepth = 0;
    state.program->noteSelect = 0;
    state.voices.channels = {1,255,255,255,255,255,255,255,255};
    state.voices.notes = {60,61,62,63,64,65,66,67,68};
    state.voices.tuning = {0,0,0,0,0,0,0,0,0};
    state.voices.channelDeferrals = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.channelEnabled = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    state.channelVoiceCursors = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.velocities = {255,40,48,0,64,72,0,88,96};
    state.sounding = {1,1,0,1,1,0,1,1,1};
    state.voices.directTimers = {1,2,0};
    state.program->channelPrograms = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.latch = SoundBlasterLevelLatch::Latched;
    state.program->voices[0] = {0,0,0,20,1,31,0};
    state.program->voices[1] = {0,0,1,21,2,30,65535};
    state.program->voices[2] = {0,0,2,22,3,29,0};
    state.program->voices[3] = {0,0,3,23,0,28,65535};
    state.program->voices[4] = {0,0,0,24,1,27,0};
    state.program->voices[5] = {0,0,1,25,2,26,65535};
    state.program->voices[6] = {0,0,2,26,3,25,0};
    state.program->voices[7] = {0,0,3,27,0,24,65535};
    state.program->voices[8] = {0,0,0,28,1,23,0};
    state.program->operators[0] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[3] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[4] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[5] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[6] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[7] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[8] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[9] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[11] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[12] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    rejects([&]{ soundBlasterCallbackAdvance(state); });
    {
    auto expected=state;
    expected.position = 39;
    expected.wait = 0;
    expected.fade = 0;
    expected.operation = 2;
    expected.runningStatus = 252;
    expected.active = 0;
    expected.program->handleStatus = 65535;
    expected.program->repeatCount = 65535;
    expected.program->handleVolume = 15;
    expected.program->waveformMask = 32;
    expected.program->accumulator = 127;
    expected.program->fadeDivider = 20;
    expected.program->loopMarker = 37;
    expected.program->amplitudeDepth = 0;
    expected.program->vibratoDepth = 0;
    expected.program->noteSelect = 0;
    expected.voices.channels = {1,255,255,255,255,255,255,255,255};
    expected.voices.notes = {60,61,62,63,64,65,66,67,68};
    expected.voices.tuning = {0,0,0,0,0,0,0,0,0};
    expected.voices.channelDeferrals = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.channelEnabled = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    expected.channelVoiceCursors = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.velocities = {255,40,48,0,64,72,0,88,96};
    expected.sounding = {0,0,0,0,0,0,0,0,0};
    expected.voices.directTimers = {0,1,0};
    expected.program->channelPrograms = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.latch = SoundBlasterLevelLatch::Latched;
    expected.program->voices[0] = {0,0,0,20,1,31,0};
    expected.program->voices[1] = {0,0,1,21,2,30,65535};
    expected.program->voices[2] = {0,0,2,22,3,29,0};
    expected.program->voices[3] = {0,0,3,23,0,28,65535};
    expected.program->voices[4] = {0,0,0,24,1,27,0};
    expected.program->voices[5] = {0,0,1,25,2,26,65535};
    expected.program->voices[6] = {0,0,2,26,3,25,0};
    expected.program->voices[7] = {0,0,3,27,0,24,65535};
    expected.program->voices[8] = {0,0,0,28,1,23,0};
    expected.program->operators[0] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[3] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[4] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[5] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[6] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[7] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[8] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[9] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[11] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[12] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    const auto actual=soundBlasterLevel1CallbackAdvance(state);
    assert(actual.dispatches==4226);
    assert(actual.kind==SoundBlasterCallbackKind::programControl);
    const std::vector<SoundBlasterWorkBlock> blocks={
        {SoundBlasterWorkKind::plain,81,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,166,229},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,182,17},
        {SoundBlasterWorkKind::plain,18,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,83,64},
        {SoundBlasterWorkKind::plain,117,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,160,87},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,176,17},
        {SoundBlasterWorkKind::plain,18,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,67,64},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,161,107},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,177,17},
        {SoundBlasterWorkKind::plain,18,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,68,64},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,162,129},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,178,17},
        {SoundBlasterWorkKind::plain,18,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,69,64},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,163,152},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,179,17},
        {SoundBlasterWorkKind::plain,18,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,75,64},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,164,176},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,180,17},
        {SoundBlasterWorkKind::plain,18,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,76,64},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,165,202},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,181,17},
        {SoundBlasterWorkKind::plain,18,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,77,64},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,166,229},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,182,17},
        {SoundBlasterWorkKind::plain,18,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,83,64},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,167,2},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,183,18},
        {SoundBlasterWorkKind::plain,18,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,84,64},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,168,32},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,184,18},
        {SoundBlasterWorkKind::plain,18,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,85,64},
        {SoundBlasterWorkKind::plain,44,-1,0,0},
    };
    assert(actual.blocks.size()==blocks.size());
    for (std::size_t i=0;i<blocks.size();++i) {
        const auto& a=actual.blocks[i];
        const auto& e=blocks[i];
        assert(a.kind==e.kind && a.dispatches==e.dispatches && a.voice==e.voice && a.registerNumber==e.registerNumber && a.value==e.value);
    }
    equal(actual.state,expected);
    state=actual.state;
    }
    {
    auto expected=state;
    expected.position = 46;
    expected.wait = 4;
    expected.fade = 0;
    expected.operation = 2;
    expected.runningStatus = 145;
    expected.active = 1;
    expected.program->handleStatus = 0;
    expected.program->repeatCount = 65535;
    expected.program->handleVolume = 15;
    expected.program->waveformMask = 32;
    expected.program->accumulator = 127;
    expected.program->fadeDivider = 20;
    expected.program->loopMarker = 37;
    expected.program->amplitudeDepth = 0;
    expected.program->vibratoDepth = 0;
    expected.program->noteSelect = 0;
    expected.voices.channels = {1,255,255,255,255,255,255,255,255};
    expected.voices.notes = {65,61,62,63,64,65,66,67,68};
    expected.voices.tuning = {0,0,0,0,0,0,0,0,0};
    expected.voices.channelDeferrals = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.channelEnabled = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    expected.channelVoiceCursors = {0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.velocities = {90,40,48,0,64,72,0,88,96};
    expected.sounding = {1,0,0,0,0,0,0,0,0};
    expected.voices.directTimers = {0,0,0};
    expected.program->channelPrograms = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.latch = SoundBlasterLevelLatch::Unlatched;
    expected.program->voices[0] = {255,0,0,20,1,31,0};
    expected.program->voices[1] = {0,0,1,21,2,30,65535};
    expected.program->voices[2] = {0,0,2,22,3,29,0};
    expected.program->voices[3] = {0,0,3,23,0,28,65535};
    expected.program->voices[4] = {0,0,0,24,1,27,0};
    expected.program->voices[5] = {0,0,1,25,2,26,65535};
    expected.program->voices[6] = {0,0,2,26,3,25,0};
    expected.program->voices[7] = {0,0,3,27,0,24,65535};
    expected.program->voices[8] = {0,0,0,28,1,23,0};
    expected.program->operators[0] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[3] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[4] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[5] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[6] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[7] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[8] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[9] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[11] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[12] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    const auto actual=soundBlasterLevel1CallbackAdvance(state);
    assert(actual.dispatches==1102);
    assert(actual.kind==SoundBlasterCallbackKind::programControl);
    const std::vector<SoundBlasterWorkBlock> blocks={
        {SoundBlasterWorkKind::plain,90,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,167,2},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,183,18},
        {SoundBlasterWorkKind::plain,18,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,84,64},
        {SoundBlasterWorkKind::plain,201,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,67,113},
        {SoundBlasterWorkKind::plain,46,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,160,202},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,176,49},
        {SoundBlasterWorkKind::plain,79,-1,0,0},
    };
    assert(actual.blocks.size()==blocks.size());
    for (std::size_t i=0;i<blocks.size();++i) {
        const auto& a=actual.blocks[i];
        const auto& e=blocks[i];
        assert(a.kind==e.kind && a.dispatches==e.dispatches && a.voice==e.voice && a.registerNumber==e.registerNumber && a.value==e.value);
    }
    equal(actual.state,expected);
    state=actual.state;
    }
    {
    auto expected=state;
    expected.position = 46;
    expected.wait = 3;
    expected.fade = 0;
    expected.operation = 2;
    expected.runningStatus = 145;
    expected.active = 1;
    expected.program->handleStatus = 0;
    expected.program->repeatCount = 65535;
    expected.program->handleVolume = 15;
    expected.program->waveformMask = 32;
    expected.program->accumulator = 127;
    expected.program->fadeDivider = 20;
    expected.program->loopMarker = 37;
    expected.program->amplitudeDepth = 0;
    expected.program->vibratoDepth = 0;
    expected.program->noteSelect = 0;
    expected.voices.channels = {1,255,255,255,255,255,255,255,255};
    expected.voices.notes = {65,61,62,63,64,65,66,67,68};
    expected.voices.tuning = {0,0,0,0,0,0,0,0,0};
    expected.voices.channelDeferrals = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.channelEnabled = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    expected.channelVoiceCursors = {0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.velocities = {90,40,48,0,64,72,0,88,96};
    expected.sounding = {1,0,0,0,0,0,0,0,0};
    expected.voices.directTimers = {0,0,0};
    expected.program->channelPrograms = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.latch = SoundBlasterLevelLatch::Unlatched;
    expected.program->voices[0] = {255,0,0,20,1,31,0};
    expected.program->voices[1] = {0,0,1,21,2,30,65535};
    expected.program->voices[2] = {0,0,2,22,3,29,0};
    expected.program->voices[3] = {0,0,3,23,0,28,65535};
    expected.program->voices[4] = {0,0,0,24,1,27,0};
    expected.program->voices[5] = {0,0,1,25,2,26,65535};
    expected.program->voices[6] = {0,0,2,26,3,25,0};
    expected.program->voices[7] = {0,0,3,27,0,24,65535};
    expected.program->voices[8] = {0,0,0,28,1,23,0};
    expected.program->operators[0] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[3] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[4] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[5] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[6] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[7] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[8] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[9] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[11] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[12] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    const auto actual=soundBlasterLevel1CallbackAdvance(state);
    assert(actual.dispatches==73);
    assert(actual.kind==SoundBlasterCallbackKind::programWait);
    const std::vector<SoundBlasterWorkBlock> blocks={
        {SoundBlasterWorkKind::plain,73,-1,0,0},
    };
    assert(actual.blocks.size()==blocks.size());
    for (std::size_t i=0;i<blocks.size();++i) {
        const auto& a=actual.blocks[i];
        const auto& e=blocks[i];
        assert(a.kind==e.kind && a.dispatches==e.dispatches && a.voice==e.voice && a.registerNumber==e.registerNumber && a.value==e.value);
    }
    equal(actual.state,expected);
    state=actual.state;
    }
}

void case3() {
    // owned-level1-body: same owner across FC, restart and wait.
    auto state=tables();
    state.sequence={132,0,0,0,0,1,5,0,0,0,0,0,0,0,0,0,0,1,4,1,4,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0,177,78,127,0,7,85,0,193,30,16,207,127,0,145,36,110,0,185,7,85,0,153,42,104,0,199,71,0,183,78,127,0,151,42,104,0,184,78,127,0,200,24,5,153,42,0,0,151,42,0,2,145,36,0,1,41,107,0,153,42,94,0,151,42,94,4,153,42,0,0,151,42,0,1,145,41,0,2,43,111,0,153,42,61,0,151,42,61,4,153,42,0,0,151,42,0,2,145,43,0,2,41,109,0,153,42,44,0,151,42,44,4,153,42,0,0,151,42,0,2,145,41,0,1,36,110,0,153,39,109,0,42,66,0,151,42,66,0,152,39,97,3,153,42,0,0,151,42,0,3,145,36,0,2,48,111,0,153,42,62,0,151,42,62,5,153,39,0,0,152,39,0,0,153,42,0,0,151,42,0,2,145,36,109,0,153,42,115,0,151,42,115,1,145,48,0,4,36,0,1,153,42,0,0,151,42,0,2,145,43,110,0,153,42,85,0,151,42,85,5,153,42,0,0,151,42,0,2,145,36,111,0,153,42,78,0,151,42,78,1,145,43,0,4,153,42,0,0,151,42,0,2,145,36,0,1,41,111,0,153,42,68,0,151,42,68,4,153,42,0,0,151,42,0,3,145,36,110,0,153,42,88,0,151,42,88,1,145,41,0,2,153,42,0,0,151,42,0,3,145,36,0,2,43,109,0,153,42,71,0,151,42,71,4,153,42,0,0,151,42,0,3,145,36,110,0,153,39,124,0,42,124,0,151,42,124,0,152,39,97,2,145,43,0,3,153,42,0,0,151,42,0,2,145,36,0,1,41,110,0,153,42,74,0,151,42,74,5,153,42,0,0,151,42,0,1,153,39,0,0,152,39,0,1,145,36,110,0,153,42,90,0,151,42,90,3,145,41,0,1,153,42,0,0,151,42,0,4,145,48,111,0,153,42,101,0,151,42,101,3,145,48,0,0,36,0,2,153,42,0,0,151,42,0,2,145,48,111,0,153,42,124,0,151,42,124,5,153,42,0,0,151,42,0,3,145,36,112,0,153,42,90,0,151,42,90,1,145,48,0,4,153,42,0,0,151,42,0,1,145,36,0,1,41,109,0,153,42,74,0,151,42,74,3,153,42,0,0,151,42,0,5,145,36,112,0,153,42,79,0,151,42,79,0,145,41,0,4,153,42,0,0,151,42,0,3,145,43,111,0,153,39,124,0,42,76,0,151,42,76,0,152,39,97,1,145,36,0,2,153,42,0,0,151,42,0,5,145,36,110,0,153,42,83,0,151,42,83,0,145,43,0,5,153,42,0,0,151,42,0,2,145,41,111,0,153,42,127,0,151,42,127,1,145,36,0,2,153,39,0,0,152,39,0,2,153,42,0,0,151,42,0,3,145,36,110,0,153,42,90,0,151,42,90,1,145,41,0,3,153,42,0,0,151,42,0,3,145,36,0,0,43,110,0,153,42,83,0,151,42,83,4,153,42,0,0,151,42,0,4,145,36,112,0,153,42,90,0,151,42,90,0,145,43,0,5,36,0,0,153,42,0,0,151,42,0,2,145,41,110,0,153,42,124,0,151,42,124,6,153,42,0,0,151,42,0,2,145,36,112,0,153,42,97,0,151,42,97,1,145,41,0,3,153,42,0,0,151,42,0,3,145,36,0,0,48,112,0,153,39,104,0,42,115,0,151,42,115,0,152,39,97,4,145,48,0,2,153,39,0,0,152,39,0,2,145,48,112,4,153,42,0,0,151,42,0,3,145,36,111,0,153,42,88,0,151,42,88,0,145,48,0,3,153,42,0,0,151,42,0,2,145,36,0,3,43,110,0,153,42,90,0,151,42,90,4,153,42,0,0,151,42,0,3,145,36,112,0,153,42,101,0,151,42,101,0,145,43,0,5,153,42,0,0,151,42,0,1,145,36,0,2,41,112,0,153,42,94,0,151,42,94,4,153,42,0,0,151,42,0,3,145,36,111,0,153,42,71,0,151,42,71,1,145,41,0,2,153,42,0,0,151,42,0,5,145,43,109,0,153,42,85,0,151,42,85,1,145,36,0,2,43,0,2,153,42,0,0,151,42,0,2,145,43,112,0,153,39,124,0,42,124,0,151,42,124,0,152,39,97,5,153,42,0,0,151,42,0,2,145,43,0,1,36,112,0,153,42,94,0,151,42,94,2,153,42,0,0,151,42,0,2,145,36,0,3,41,112,0,153,42,90,0,151,42,90,3,153,39,0,0,152,39,0,0,153,42,0,0,151,42,0,5,145,36,111,0,153,42,101,0,151,42,101,0,145,41,0,3,153,42,0,0,151,42,0,3,145,36,0,1,43,110,0,153,42,82,0,151,42,82,3,153,42,0,0,151,42,0,5,145,36,112,0,153,42,76,0,151,42,76,0,145,43,0,4,153,42,0,0,151,42,0,3,145,36,0,0,43,111,0,153,42,97,0,151,42,97,4,153,42,0,0,151,42,0,4,145,36,111,0,153,42,115,0,151,42,115,0,145,43,0,4,153,42,0,0,151,42,0,2,145,36,0,1,41,111,0,153,39,115,0,42,73,0,151,42,73,0,152,39,97,3,153,42,0,0,151,42,0,5,145,36,112,0,153,42,109,0,151,42,109,0,145,41,0,4,153,42,0,0,151,42,0,3,145,43,110,0,153,42,115,0,151,42,115,2,145,36,0,1,153,39,0,0,152,39,0,1,145,43,0,1,153,42,0,0,151,42,0,3,145,48,112,0,153,42,104,0,151,42,104,4,153,42,0,0,151,42,0,3,145,48,0,0,36,112,0,153,42,73,0,151,42,73,3,153,42,0,0,151,42,0,4,145,36,0,1,43,112,0,153,42,101,0,151,42,101,3,153,42,0,0,151,42,0,4,145,43,0,0,36,109,0,153,42,124,0,151,42,124,5,153,42,0,0,151,42,0,2,145,36,0,1,41,111,0,153,42,115,0,151,42,115,3,153,42,0,0,151,42,0,4,145,36,112,0,153,39,124,0,42,85,0,151,42,85,0,152,39,97,1,145,41,0,2,153,42,0,0,151,42,0,5,145,43,108,0,153,42,94,0,151,42,94,1,145,36,0,2,153,42,0,0,151,42,0,1,145,43,0,3,43,112,0,153,42,124,0,151,42,124,5,153,42,0,0,151,42,0,2,145,43,0,1,36,111,0,153,42,124,0,151,42,124,3,153,42,0,0,151,42,0,4,145,36,0,0,41,111,0,153,42,79,0,151,42,79,3,153,42,0,0,151,42,0,1,153,39,0,0,152,39,0,4,145,36,109,0,153,42,82,0,151,42,82,0,145,41,0,3,153,42,0,0,151,42,0,3,145,36,0,1,43,111,0,153,39,124,0,42,74,0,151,42,74,0,152,39,97,3,153,42,0,0,151,42,0,4,145,43,0,1,36,111,0,153,42,88,0,151,42,88,3,153,42,0,0,151,42,0,4,145,36,0,0,41,112,0,153,42,109,0,151,42,109,2,153,39,0,0,152,39,0,3,153,42,0,0,151,42,0,3,145,36,112,0,153,39,127,0,42,101,0,151,42,101,0,152,39,97,1,145,41,0,3,153,42,0,0,151,42,0,3,145,43,112,0,153,42,124,0,151,42,124,2,145,36,0,2,43,0,1,153,42,0,0,151,42,0,3,145,48,111,0,153,42,115,0,151,42,115,4,153,42,0,0,151,42,0,2,145,48,0,1,43,112,0,153,42,97,0,151,42,97,4,153,42,0,0,151,42,0,0,145,43,0,4,41,112,0,153,42,82,0,151,42,82,3,153,42,0,0,151,42,0,2,145,41,0,2,36,112,0,153,42,82,0,151,42,82,2,153,39,0,0,152,39,0,1,153,42,0,0,151,42,0,3,145,36,0,2,43,111,0,153,42,101,0,151,42,101,4,153,42,0,0,151,42,0,3,145,43,0,0,36,112,0,153,39,127,0,42,124,0,151,42,124,0,152,39,97,5,153,42,0,0,151,42,0,2,145,36,0,1,43,112,0,153,42,101,0,151,42,101,4,153,42,0,0,151,42,0,3,145,36,112,0,153,42,78,0,151,42,78,1,145,43,0,2,153,42,0,0,151,42,0,5,145,41,110,0,153,42,88,0,151,42,88,0,145,36,0,4,153,42,0,0,151,42,0,1,145,41,0,2,43,112,0,153,42,127,0,151,42,127,6,153,42,0,0,151,42,0,1,145,43,0,1,36,112,0,153,42,104,0,151,42,104,5,153,42,0,0,151,42,0,1,145,36,0,1,43,112,0,153,42,83,0,151,42,83,3,153,42,0,0,151,42,0,4,145,43,0,0,153,39,0,0,152,39,0,1,145,36,111,0,153,42,94,0,151,42,94,5,153,42,0,0,151,42,0,1,145,36,0,1,41,112,0,153,39,124,0,42,97,0,151,42,97,0,152,39,97,3,153,42,0,0,151,42,0,5,145,36,112,0,153,42,94,0,151,42,94,0,145,41,0,5,153,42,0,0,151,42,0,2,145,43,112,0,153,42,127,0,151,42,127,1,145,36,0,3,43,0,2,153,42,0,0,151,42,0,2,145,48,112,0,153,42,115,0,151,42,115,5,153,42,0,0,151,42,0,2,145,48,0,0,36,111,0,153,42,90,0,42,104,0,151,42,90,0,42,104,4,153,42,0,0,42,0,0,151,42,0,0,153,42,0,0,42,0,3,145,36,0,1,43,112,0,153,42,94,0,151,42,94,4,153,42,0,0,151,42,0,3,145,43,0,0,36,110,0,153,42,61,0,151,42,61,4,153,42,0,0,151,42,0,1,153,39,0,0,152,39,0,2,145,36,0,1,41,112,0,153,42,44,0,151,42,44,4,153,42,0,0,151,42,0,3,145,41,0,0,36,111,0,153,39,124,0,42,66,0,151,42,66,0,152,39,97,3,153,42,0,0,151,42,0,3,145,36,0,2,43,111,0,153,42,62,0,151,42,62,5,153,42,0,0,151,42,0,2,145,36,111,0,153,42,115,0,151,42,115,1,145,43,0,4,36,0,1,153,42,0,0,151,42,0,2,145,41,111,0,153,42,85,0,151,42,85,5,153,42,0,0,151,42,0,1,145,41,0,1,36,111,0,153,42,78,0,151,42,78,5,153,42,0,0,151,42,0,2,145,36,0,1,41,112,0,153,42,68,0,151,42,68,4,153,42,0,0,151,42,0,3,145,41,0,0,36,111,0,153,42,88,0,151,42,88,3,153,39,0,0,152,39,0,0,153,42,0,0,151,42,0,5,145,43,109,0,153,42,71,0,151,42,71,0,145,36,0,3,43,0,1,153,42,0,0,151,42,0,3,145,43,112,0,153,39,127,0,42,124,0,151,42,124,0,152,39,97,5,153,39,0,0,42,0,0,151,42,0,0,152,39,0,2,145,43,0,1,36,112,0,153,42,74,0,151,42,74,5,153,42,0,0,151,42,0,2,145,36,0,0,41,112,0,153,39,127,0,42,90,0,151,42,90,0,152,39,97,4,153,42,0,0,151,42,0,4,145,36,112,0,153,42,101,0,151,42,101,0,145,41,0,5,153,42,0,0,151,42,0,1,145,36,0,1,153,39,0,0,152,39,0,0,252};
    state.position = 2523;
    state.wait = 0;
    state.fade = 0;
    state.operation = 2;
    state.runningStatus = 0;
    state.active = 1;
    state.program->handleStatus = 0;
    state.program->repeatCount = 65535;
    state.program->handleVolume = 15;
    state.program->waveformMask = 32;
    state.program->accumulator = 127;
    state.program->fadeDivider = 20;
    state.program->loopMarker = 44;
    state.program->amplitudeDepth = 0;
    state.program->vibratoDepth = 0;
    state.program->noteSelect = 0;
    state.voices.channels = {1,255,255,255,255,255,255,255,255};
    state.voices.notes = {60,61,62,63,64,65,66,67,68};
    state.voices.tuning = {0,0,0,0,0,0,0,0,0};
    state.voices.channelDeferrals = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.channelEnabled = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    state.channelVoiceCursors = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.velocities = {0,40,48,0,64,72,0,88,96};
    state.sounding = {1,1,0,1,1,0,1,1,1};
    state.voices.directTimers = {0,0,0};
    state.program->channelPrograms = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.latch = SoundBlasterLevelLatch::Unlatched;
    state.program->voices[0] = {0,0,0,20,1,31,0};
    state.program->voices[1] = {0,0,1,21,2,30,65535};
    state.program->voices[2] = {0,0,2,22,3,29,0};
    state.program->voices[3] = {0,0,3,23,0,28,65535};
    state.program->voices[4] = {0,0,0,24,1,27,0};
    state.program->voices[5] = {0,0,1,25,2,26,65535};
    state.program->voices[6] = {0,0,2,26,3,25,0};
    state.program->voices[7] = {0,0,3,27,0,24,65535};
    state.program->voices[8] = {0,0,0,28,1,23,0};
    state.program->operators[0] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[3] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[4] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[5] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[6] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[7] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[8] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[9] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[11] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[12] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    state.program->operators[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    rejects([&]{ soundBlasterCallbackAdvance(state); });
    {
    auto expected=state;
    expected.position = 46;
    expected.wait = 0;
    expected.fade = 0;
    expected.operation = 2;
    expected.runningStatus = 252;
    expected.active = 0;
    expected.program->handleStatus = 65535;
    expected.program->repeatCount = 65535;
    expected.program->handleVolume = 15;
    expected.program->waveformMask = 32;
    expected.program->accumulator = 127;
    expected.program->fadeDivider = 20;
    expected.program->loopMarker = 44;
    expected.program->amplitudeDepth = 0;
    expected.program->vibratoDepth = 0;
    expected.program->noteSelect = 0;
    expected.voices.channels = {1,255,255,255,255,255,255,255,255};
    expected.voices.notes = {60,61,62,63,64,65,66,67,68};
    expected.voices.tuning = {0,0,0,0,0,0,0,0,0};
    expected.voices.channelDeferrals = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.channelEnabled = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    expected.channelVoiceCursors = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.velocities = {0,40,48,0,64,72,0,88,96};
    expected.sounding = {0,0,0,0,0,0,0,0,0};
    expected.voices.directTimers = {0,0,0};
    expected.program->channelPrograms = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.latch = SoundBlasterLevelLatch::Unlatched;
    expected.program->voices[0] = {0,0,0,20,1,31,0};
    expected.program->voices[1] = {0,0,1,21,2,30,65535};
    expected.program->voices[2] = {0,0,2,22,3,29,0};
    expected.program->voices[3] = {0,0,3,23,0,28,65535};
    expected.program->voices[4] = {0,0,0,24,1,27,0};
    expected.program->voices[5] = {0,0,1,25,2,26,65535};
    expected.program->voices[6] = {0,0,2,26,3,25,0};
    expected.program->voices[7] = {0,0,3,27,0,24,65535};
    expected.program->voices[8] = {0,0,0,28,1,23,0};
    expected.program->operators[0] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[3] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[4] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[5] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[6] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[7] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[8] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[9] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[11] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[12] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    const auto actual=soundBlasterLevel1CallbackAdvance(state);
    assert(actual.dispatches==2715);
    assert(actual.kind==SoundBlasterCallbackKind::programControl);
    const std::vector<SoundBlasterWorkBlock> blocks={
        {SoundBlasterWorkKind::plain,137,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,160,87},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,176,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,161,107},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,177,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,162,129},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,178,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,163,152},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,179,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,164,176},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,180,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,165,202},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,181,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,166,229},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,182,17},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,167,2},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,183,18},
        {SoundBlasterWorkKind::plain,58,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,168,32},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,184,18},
        {SoundBlasterWorkKind::plain,44,-1,0,0},
    };
    assert(actual.blocks.size()==blocks.size());
    for (std::size_t i=0;i<blocks.size();++i) {
        const auto& a=actual.blocks[i];
        const auto& e=blocks[i];
        assert(a.kind==e.kind && a.dispatches==e.dispatches && a.voice==e.voice && a.registerNumber==e.registerNumber && a.value==e.value);
    }
    equal(actual.state,expected);
    state=actual.state;
    }
    {
    auto expected=state;
    expected.position = 79;
    expected.wait = 4;
    expected.fade = 0;
    expected.operation = 2;
    expected.runningStatus = 200;
    expected.active = 1;
    expected.program->handleStatus = 0;
    expected.program->repeatCount = 65535;
    expected.program->handleVolume = 15;
    expected.program->waveformMask = 32;
    expected.program->accumulator = 127;
    expected.program->fadeDivider = 20;
    expected.program->loopMarker = 44;
    expected.program->amplitudeDepth = 0;
    expected.program->vibratoDepth = 0;
    expected.program->noteSelect = 0;
    expected.voices.channels = {1,255,255,255,255,255,255,255,255};
    expected.voices.notes = {36,61,62,63,64,65,66,67,68};
    expected.voices.tuning = {0,0,0,0,0,0,0,0,0};
    expected.voices.channelDeferrals = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.channelEnabled = {1,1,1,1,1,1,1,127,127,1,1,1,1,1,1,1};
    expected.channelVoiceCursors = {0,1,0,0,0,0,0,2,0,2,0,0,0,0,0,0};
    expected.velocities = {110,40,48,0,64,72,0,88,96};
    expected.sounding = {1,0,0,0,0,0,0,0,0};
    expected.voices.directTimers = {0,0,0};
    expected.program->channelPrograms = {0,0,0,0,0,0,0,71,24,0,0,0,0,0,0,0};
    expected.latch = SoundBlasterLevelLatch::Unlatched;
    expected.program->voices[0] = {255,0,0,20,1,31,0};
    expected.program->voices[1] = {0,0,1,21,2,30,65535};
    expected.program->voices[2] = {0,0,2,22,3,29,0};
    expected.program->voices[3] = {0,0,3,23,0,28,65535};
    expected.program->voices[4] = {0,0,0,24,1,27,0};
    expected.program->voices[5] = {0,0,1,25,2,26,65535};
    expected.program->voices[6] = {0,0,2,26,3,25,0};
    expected.program->voices[7] = {0,0,3,27,0,24,65535};
    expected.program->voices[8] = {0,0,0,28,1,23,0};
    expected.program->operators[0] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[3] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[4] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[5] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[6] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[7] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[8] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[9] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[11] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[12] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    const auto actual=soundBlasterLevel1CallbackAdvance(state);
    assert(actual.dispatches==1509);
    assert(actual.kind==SoundBlasterCallbackKind::programControl);
    const std::vector<SoundBlasterWorkBlock> blocks={
        {SoundBlasterWorkKind::plain,233,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,67,127},
        {SoundBlasterWorkKind::plain,46,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,160,87},
        {SoundBlasterWorkKind::plain,22,-1,0,0},
        {SoundBlasterWorkKind::registerWrite,104,-1,176,41},
        {SoundBlasterWorkKind::plain,896,-1,0,0},
    };
    assert(actual.blocks.size()==blocks.size());
    for (std::size_t i=0;i<blocks.size();++i) {
        const auto& a=actual.blocks[i];
        const auto& e=blocks[i];
        assert(a.kind==e.kind && a.dispatches==e.dispatches && a.voice==e.voice && a.registerNumber==e.registerNumber && a.value==e.value);
    }
    equal(actual.state,expected);
    state=actual.state;
    }
    {
    auto expected=state;
    expected.position = 79;
    expected.wait = 3;
    expected.fade = 0;
    expected.operation = 2;
    expected.runningStatus = 200;
    expected.active = 1;
    expected.program->handleStatus = 0;
    expected.program->repeatCount = 65535;
    expected.program->handleVolume = 15;
    expected.program->waveformMask = 32;
    expected.program->accumulator = 127;
    expected.program->fadeDivider = 20;
    expected.program->loopMarker = 44;
    expected.program->amplitudeDepth = 0;
    expected.program->vibratoDepth = 0;
    expected.program->noteSelect = 0;
    expected.voices.channels = {1,255,255,255,255,255,255,255,255};
    expected.voices.notes = {36,61,62,63,64,65,66,67,68};
    expected.voices.tuning = {0,0,0,0,0,0,0,0,0};
    expected.voices.channelDeferrals = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.channelEnabled = {1,1,1,1,1,1,1,127,127,1,1,1,1,1,1,1};
    expected.channelVoiceCursors = {0,1,0,0,0,0,0,2,0,2,0,0,0,0,0,0};
    expected.velocities = {110,40,48,0,64,72,0,88,96};
    expected.sounding = {1,0,0,0,0,0,0,0,0};
    expected.voices.directTimers = {0,0,0};
    expected.program->channelPrograms = {0,0,0,0,0,0,0,71,24,0,0,0,0,0,0,0};
    expected.latch = SoundBlasterLevelLatch::Unlatched;
    expected.program->voices[0] = {255,0,0,20,1,31,0};
    expected.program->voices[1] = {0,0,1,21,2,30,65535};
    expected.program->voices[2] = {0,0,2,22,3,29,0};
    expected.program->voices[3] = {0,0,3,23,0,28,65535};
    expected.program->voices[4] = {0,0,0,24,1,27,0};
    expected.program->voices[5] = {0,0,1,25,2,26,65535};
    expected.program->voices[6] = {0,0,2,26,3,25,0};
    expected.program->voices[7] = {0,0,3,27,0,24,65535};
    expected.program->voices[8] = {0,0,0,28,1,23,0};
    expected.program->operators[0] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[2] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[3] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[4] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[5] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[6] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[7] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[8] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[9] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[11] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[12] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[15] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    expected.program->operators[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    const auto actual=soundBlasterLevel1CallbackAdvance(state);
    assert(actual.dispatches==73);
    assert(actual.kind==SoundBlasterCallbackKind::programWait);
    const std::vector<SoundBlasterWorkBlock> blocks={
        {SoundBlasterWorkKind::plain,73,-1,0,0},
    };
    assert(actual.blocks.size()==blocks.size());
    for (std::size_t i=0;i<blocks.size();++i) {
        const auto& a=actual.blocks[i];
        const auto& e=blocks[i];
        assert(a.kind==e.kind && a.dispatches==e.dispatches && a.voice==e.voice && a.registerNumber==e.registerNumber && a.value==e.value);
    }
    equal(actual.state,expected);
    state=actual.state;
    }
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    case0();
    case1();
    case2();
    case3();
    return 0;
}
