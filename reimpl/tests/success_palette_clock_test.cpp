#include "stellar7/success_palette_clock.hpp"
#include "stellar7/level1_loader_work.hpp"
#include "stellar7/native_startup.hpp"
#include "stellar7/native_session.hpp"
#include <algorithm>
#include <cassert>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace stellar7 {
struct NativeSessionTestAccess {
    static SoundBlasterSynth& synth(NativeGameplaySession& s) { return s.synth_; }
    static PresentationClock& clock(NativeGameplaySession& s) { return s.clock_; }
};
}
namespace {
using namespace stellar7;
constexpr std::int64_t horizon = 120000;
stellar7::SoundBlasterClockPhase phase() {
    using namespace stellar7;
    SoundBlasterClockPhase p{};
    p.remainingCycles = 5000;
    p.interrupts = AudioClockInterrupts::enabled;
    p.isa8DelayNanoseconds = 1020;
    p.dmaDacRate = 44100;
    p.pitPeriodMilliseconds = 55;
    p.irqDelayMilliseconds = 0.002;
    p.singleLinePeriodMilliseconds = 0.03;
    p.horizontalPeriodMilliseconds = 0.03;
    p.linesTotal = 400;
    p.minimumLatchDelayMilliseconds = 0.001;
    p.frameStart = AudioClockFrameStart::activeUnlockedVgaSurface;
    p.interruptDelivery = AudioClockInterruptDelivery::residentTimerIrq0;
    p.vgaInterrupt = AudioClockVgaInterrupt::fixedVgaNoDemand;
    p.devices = AudioClockDevicePhase{};
    p.devices->fixedPitAndMaskedDma = true;
    p.devices->framePeriodMilliseconds = 14;
    p.devices->displayEndMilliseconds = 12;
    p.devices->retraceStartMilliseconds = 0.001;
    p.devices->retraceEndMilliseconds = 10;
    p.devices->frame = AudioClockVgaFramePhase{1000.0 / 14, 0, 0};
    p.timer = AudioClockTimerPhase{1, 0, 5, 20, 20, 5};
    return p;
}

struct Control {
    const char* name;
    bool initial = false;
    bool blue = false;
    PaletteDacMode mode = PaletteDacMode::write;
    std::uint8_t pel = 0;
    bool sound = true;
    bool finished = false;
    std::uint16_t ticks = 1000;
    bool mixed = false;
    std::uint64_t dispatches = 0;
};
std::vector<Control> controls() {
    return {{"initial-green",true,false,PaletteDacMode::write,0,true,false,1000,false,469},
        {"initial-blue-read",true,true,PaletteDacMode::read,0,true,false,1000,false,467},
        {"advance-green",false,false,PaletteDacMode::write,0,true,false,1000,false,1249},
        {"advance-blue-read",false,true,PaletteDacMode::read,0,true,false,1000,false,1247},
        {"advance-mixed-green",false,false,PaletteDacMode::write,0,true,false,1000,true,1264},
        {"advance-mixed-blue",false,true,PaletteDacMode::write,0,true,false,1000,true,1264},
        {"advance-write-pel2",false,false,PaletteDacMode::write,2,true,false,1000,false,1249},
        {"advance-write-pel1",false,true,PaletteDacMode::write,1,true,false,1000,false,1249},
        {"sound-complete",false,false,PaletteDacMode::write,0,true,true,1000,false,8},
        {"timer-active",false,false,PaletteDacMode::write,0,false,false,1,false,1250},
        {"timer-complete",false,false,PaletteDacMode::write,0,false,false,0,false,7}};
}
PaletteWorkState paletteFor(const Control& c) {
    PaletteWorkState p{};
    for (std::size_t i=0;i<p.dac.colors.size();++i) {
        p.dac.colors[i] = static_cast<std::uint8_t>((i*7+3)%64);
        p.source[i] = static_cast<std::uint8_t>((i*11+5)%64);
        p.output[i] = static_cast<std::uint8_t>((i*13+9)%64);
    }
    p.dac.mode=c.mode;p.dac.writeIndex=5;p.dac.readIndex=4;p.dac.pelIndex=c.pel;
    p.dac.latch=std::array<std::uint8_t,3>{11,22,33};return p;
}
SuccessPaletteState oscillatorFor(const Control& c) {
    auto state=initialSuccessPalette(c.blue?SuccessPaletteChannel::blue:SuccessPaletteChannel::green);
    if (c.mixed) {
        for (std::size_t i=0;i<state.values.size();++i) {
            state.values[i]=std::array<std::uint8_t,3>{16,63,32}[i%3];
            state.directions[i]=std::array<std::int8_t,3>{-1,1,-1}[i%3];
        }
    }
    return state;
}
SoundBlasterClockPhase phaseFor(const Control& c) {
    auto p=phase();p.timer->globalCount=c.ticks;
    if (c.sound) {
        p.callback.emplace();p.callback->program.emplace();
        p.callback->program->handleStatus=c.finished?0xffff:0;
    }
    return p;
}
SuccessPaletteClockTiming execute(const Control& c) {
    return successPaletteClockTiming(phaseFor(c),oscillatorFor(c),paletteFor(c),
        c.initial?SuccessPaletteExecution::initial:SuccessPaletteExecution::advance,
        c.sound?SuccessPaletteCompletionSource::soundHandle:SuccessPaletteCompletionSource::timer,horizon);
}
template<class Function> void rejects(Function f) {
    bool rejected=false;try {f();} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
}
void sourceControls() {
    for (const auto& c:controls()) {
        const auto work=execute(c);const auto& upload=work.palette;
        assert(upload.timing.total.dispatches==c.dispatches && upload.timing.callbacks.empty());
        assert(upload.state.source==paletteFor(c).source && upload.state.output==paletteFor(c).output);
        const bool complete=!c.initial&&((c.sound&&c.finished)||(!c.sound&&c.ticks==0));
        assert(work.boundary==(complete?SuccessPaletteBoundary::presentationComplete:
            c.initial?SuccessPaletteBoundary::initialUploadComplete:SuccessPaletteBoundary::repeat));
        if (complete) {
            assert(upload.writes.empty() && upload.commits.empty() && upload.highReads==0);
            assert(work.state.values==oscillatorFor(c).values && work.state.directions==oscillatorFor(c).directions);
            continue;
        }
        assert(upload.highReads==1 && upload.writes.size()==(c.mode==PaletteDacMode::write?146:145));
        assert(upload.commits.size()==(c.mode==PaletteDacMode::write&&c.pel==2?49:48));
        assert(upload.state.dac.writeIndex==64 && upload.state.dac.readIndex==63 && upload.state.dac.pelIndex==0);
        assert(upload.state.dac.mode==PaletteDacMode::write && upload.timing.phase.interrupts==AudioClockInterrupts::enabled);
        assert(upload.timing.stringIterations==144);
        if (c.pel==2) {
            assert(upload.commits.front().index==5);
            assert((upload.commits.front().color==std::array<std::uint8_t,3>{11,22,0}));
        }
        if (!c.initial) {
            if (c.mixed) {assert(work.state.values[0]==17&&work.state.values[1]==62&&work.state.values[2]==31);}
            else {assert(work.state.values[0]==62&&work.state.values[1]==63&&work.state.values[47]==17);}
        }
        for (std::size_t i=0;i<48;++i) {
            const auto& color=upload.commits[upload.commits.size()-48+i];
            assert(color.index==16+i && color.color[c.blue?2:1]==work.state.values[i]);
            assert(color.color[0]==0 && color.color[c.blue?1:2]==0);
        }
    }
    auto c=controls()[2];auto s=oscillatorFor(c);auto pal=paletteFor(c);auto p=phaseFor(c);
    s.values[0]=64;rejects([&]{successPaletteClockTiming(p,s,pal,SuccessPaletteExecution::advance,SuccessPaletteCompletionSource::soundHandle,horizon);});
    s=oscillatorFor(c);s.directions[0]=0;rejects([&]{successPaletteClockTiming(p,s,pal,SuccessPaletteExecution::advance,SuccessPaletteCompletionSource::soundHandle,horizon);});
    s=oscillatorFor(c);s.values[0]=62;rejects([&]{successPaletteClockTiming(p,s,pal,SuccessPaletteExecution::initial,SuccessPaletteCompletionSource::soundHandle,horizon);});
    s=oscillatorFor(c);pal.dac.pelIndex=2;pal.dac.latch.reset();rejects([&]{successPaletteClockTiming(p,s,pal,SuccessPaletteExecution::advance,SuccessPaletteCompletionSource::soundHandle,horizon);});
    pal=paletteFor(c);p.callback.reset();rejects([&]{successPaletteClockTiming(p,s,pal,SuccessPaletteExecution::advance,SuccessPaletteCompletionSource::soundHandle,horizon);});
    p=phaseFor(c);rejects([&]{successPaletteClockTiming(p,s,pal,SuccessPaletteExecution::advance,SuccessPaletteCompletionSource::soundHandle,120001);});
    p.devices->retraceStartMilliseconds=0.4;
    const auto waited=successPaletteClockTiming(p,s,pal,SuccessPaletteExecution::advance,SuccessPaletteCompletionSource::soundHandle,horizon);
    assert(waited.palette.highReads>1);
    // A timer IRQ before7093 must affect this poll's source read.
    auto live=phase();live.timer->globalCount=1;live.timer->callbackCount=20;
    live.remainingCycles=1;live.deferredCycles=4999;
    live.devices->irq0Requested=true;live.irqCheckRequested=true;
    const auto expired=successPaletteClockTiming(live,s,pal,SuccessPaletteExecution::advance,
        SuccessPaletteCompletionSource::timer,horizon);
    assert(expired.boundary==SuccessPaletteBoundary::presentationComplete);
    assert(expired.palette.timing.phase.timer->globalCount==0 && expired.palette.writes.empty());

    // Source516B masks every RGB triplet and permits the pending callback only
    // after51A2 STI and51A3 POPSI's interrupt shadow.
    auto masked=phase();masked.timer->globalCount=10;
    masked.remainingCycles=1250;masked.deferredCycles=3750;
    masked.events={{0.25,AudioClockEventKind::irqCheck,0}};
    masked.irqPending=true;masked.devices->irq0Requested=true;masked.irq0ActivationPossible=true;
    masked.timer->callbackCount=1;masked.callback=SoundBlasterCallbackState{};masked.callback->wait=2;
    const auto interrupted=successPaletteClockTiming(masked,s,pal,SuccessPaletteExecution::advance,
        SuccessPaletteCompletionSource::timer,horizon);
    assert(interrupted.palette.timing.callbacks.size()==1);
    assert(interrupted.palette.timing.callbacks[0].cycleOffset>=interrupted.palette.uploadEndCycles+2);
    assert(interrupted.palette.timing.phase.callback->wait==1);
    assert(interrupted.palette.timing.phase.interrupts==AudioClockInterrupts::enabled);
    for (const auto channel:{SuccessPaletteChannel::green,SuccessPaletteChannel::blue}) {
        auto value=initialSuccessPalette(channel);const auto initial=value;
        for (unsigned i=0;i<94;++i) {
            value=successPaletteClockTiming(phaseFor(c),value,paletteFor(c),SuccessPaletteExecution::advance,
                SuccessPaletteCompletionSource::soundHandle,horizon).state;
        }
        assert(value.values==initial.values);
    }
}
std::vector<std::uint8_t> bank() {
    std::vector<std::uint8_t> result(96 * 28);
    for (std::size_t program = 0; program < 96; ++program) {
        const auto offset = program * 28;
        for (const auto op : {0U, 13U}) {
            result[offset + op + 1] = 1;
            result[offset + op + 3] = 15;
            result[offset + op + 4] = 7;
            result[offset + op + 7] = 7;
        }
        result[offset + 12] = 1;
    }
    return result;
}

GameplayAssets assets() {
    GameplayAssets game;
    game.cockpit = {std::vector<std::uint8_t>(320 * 200), std::vector<Rgb>(256)};
    game.cockpitDacPalette.resize(256 * 3);
    game.uiPalette.assign(256, {12, 20, 28});
    game.horizonFrames = {{256, 16, std::vector<std::uint8_t>(256 * 16)}};
    game.soundBlasterVolumeTable.resize(16 * 64);
    for (std::size_t i = 0; i < game.soundBlasterVolumeTable.size(); ++i) {
        game.soundBlasterVolumeTable[i] = static_cast<std::uint8_t>(i % 64);
    }
    game.artGroups.resize(6, std::vector<IndexedBitmap>(64, {4, 1, {1, 2, 3, 4}}));
    auto& ui = game.artGroups[0];
    ui[0] = {16, 20, std::vector<std::uint8_t>(320, 5)};
    ui[11] = {4, 5, std::vector<std::uint8_t>(20, 9)};
    ui[12] = {40, 40, std::vector<std::uint8_t>(1600, 6)};
    ui[14] = {24, 22, std::vector<std::uint8_t>(528, 5)};
    ui[16] = {120, 43, std::vector<std::uint8_t>(5160, 3)};
    ui[17] = {120, 33, std::vector<std::uint8_t>(3960, 8)};
    ui[18] = {120, 25, std::vector<std::uint8_t>(3000, 8)};
    ui[20] = {264, 2, std::vector<std::uint8_t>(528, 4)};
    ui[21] = ui[22] = {8, 5, std::vector<std::uint8_t>(40, 7)};
    game.titleFont = {32, 6, 0, std::vector<IndexedBitmap>(64, {8, 6, std::vector<std::uint8_t>(48, 1)})};
    game.scoreFont = {32, 8, 0, std::vector<IndexedBitmap>(96, {4, 8, std::vector<std::uint8_t>(32, 1)})};
    game.modelColorTables.resize(2);
    auto song = std::vector<std::uint8_t>(36);
    song[0] = 0x84; song[5] = 1; song[6] = 4;
    game.levelMusicSequence = song;
    // Enable the synthetic song channel through its recovered controller message.
    game.levelMusicSequence.insert(game.levelMusicSequence.end(), {0xb1, 0x4e, 1, 0, 0xc1, 0, 0, 0x91, 65, 90, 127, 0xfc});
    game.logoMusicSequence = song;
    game.logoMusicSequence[5] = 7;
    game.logoMusicSequence.insert(game.logoMusicSequence.end(), {0xc1, 0, 127, 0xfc});
    game.presentationMusicSequence = song;
    for (unsigned group = 0; group < 8; ++group) {
        game.presentationMusicSequence.insert(game.presentationMusicSequence.end(), {0xc1, 0, 127});
    }
    game.presentationMusicSequence.push_back(0xfc);
    game.soundBlasterInstrumentResource.resize(19 + 2 * 0x540 + 2);
    game.soundBlasterInstrumentResource[0] = 0x89;
    game.soundBlasterInstrumentResource[1] = 0x11;
    const auto patches = bank();
    std::copy(patches.begin(), patches.begin() + 0x540, game.soundBlasterInstrumentResource.begin() + 19);
    game.soundBlasterInstrumentResource[19 + 0x540] = 0xab;
    game.soundBlasterInstrumentResource[19 + 0x540 + 1] = 0xcd;
    std::copy(patches.begin() + 0x540, patches.end(), game.soundBlasterInstrumentResource.begin() + 19 + 0x540 + 2);
    const CockpitSettings configuration{81, {1, 0, 1, 1, 0, 255}};
    HighscoreTable scores({}, {});
    scores.setConfiguration(configuration);
    game.cockpitSettings = configuration;
    game.highscoreTable = scores;
    return game;
}

void retainedClock() {
    auto game = assets();
    NativeStartupAssets resources;
    resources.logoScreen = {std::vector<std::uint8_t>(320 * 200, 9), std::vector<Rgb>(256)};
    resources.menuScreen = resources.logoScreen;
    resources.logoFrames.assign(8, {4, 1, {1, 0, 2, 3}});
    NativeStartupSequence startup(game, resources);
    startup.start(NativeStartupExecution::recoveredContext);
    startup.initializeSettings(StartupCapabilityProfile::noMouseOrJoystick);
    startup.initializeResources(NativeStartupDeviceProfile::portableOplNoDspProbe);
    startup.initializePresentation();
    startup.initializeFonts();
    while (startup.phase() == NativeStartupPhase::logoCountdownWork) { startup.pollLogoCountdown(10000); }
    startup.playLogoEvent();
    startup.animateLogo();
    startup.finishLogoSound();
    startup.releaseLogo();
    startup.skipIntro(NativeIntroKey::escape);
    startup.beginMenuSong();
    startup.initializeMenuResources();
    startup.skipCredits(NativeIntroKey::escape);
    startup.enterMenu(NativeMenuEntryProfile::keyboardOnly);
    startup.beginGame(NativeMenuBeginProfile::enterPressRelease);
    auto session = std::make_unique<NativeGameplaySession>(game, startup.releaseGameplay());
    auto& synth = NativeSessionTestAccess::synth(*session);
    auto& clock = NativeSessionTestAccess::clock(*session);
    const auto music = activateLevel1Music(clock, synth, *session->recoveredClockPhase());
    auto phase = resetLevel1DirectEffects(clock, synth, music.phase).phase;
    const auto state=initialSuccessPalette(SuccessPaletteChannel::green);
    const auto palette=paletteFor(controls()[0]);
    const auto origin=synth.driverState().normalClockCycles;
    const auto expected=successPaletteClockTiming(phase,state,palette,SuccessPaletteExecution::initial,
        SuccessPaletteCompletionSource::soundHandle,horizon);
    const auto first=clock.renderSuccessPalette(phase,state,palette,SuccessPaletteExecution::initial,
        SuccessPaletteCompletionSource::soundHandle,horizon);
    assert(first.palette.work.timing.total.elapsedCycles==expected.palette.timing.total.elapsedCycles);
    assert(first.palette.state.dac.colors==expected.palette.state.dac.colors);
    assert(synth.driverState().normalClockCycles==origin+first.palette.work.timing.total.elapsedCycles);
    assert(!first.palette.work.audio.samples.empty());
    const auto previous=clock.elapsedSamples();
    const auto adjacent=clock.renderSuccessPalette(first.palette.work.timing.phase,first.state,first.palette.state,
        SuccessPaletteExecution::advance,SuccessPaletteCompletionSource::soundHandle,horizon);
    assert(adjacent.boundary==SuccessPaletteBoundary::repeat);
    assert(clock.elapsedSamples()==previous+adjacent.palette.work.audio.samples.size());
    assert(adjacent.palette.state.source==palette.source&&adjacent.palette.state.output==palette.output);
    const auto stoppedAt=clock.elapsedSamples();
    rejects([&]{clock.renderSuccessPalette(first.state,SuccessPaletteExecution::advance);});
    assert(clock.elapsedSamples()==stoppedAt);
    auto invalid=adjacent.state;invalid.directions[0]=0;
    const auto cycles=synth.driverState().normalClockCycles;
    rejects([&]{clock.renderSuccessPalette(adjacent.palette.work.timing.phase,invalid,adjacent.palette.state,
        SuccessPaletteExecution::advance,SuccessPaletteCompletionSource::soundHandle,horizon);});
    assert(clock.elapsedSamples()==stoppedAt&&synth.driverState().normalClockCycles==cycles);
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    sourceControls();retainedClock();return 0;
}
