#include "stellar7/audio_direct_work.hpp"
#include "stellar7/audio_program_work.hpp"
#include "stellar7/level1_loader_work.hpp"
#include "stellar7/native_session.hpp"
#include "stellar7/native_startup.hpp"
#include "stellar7/timer_install_work.hpp"
#include "stellar7/timer_registration_work.hpp"

#include <algorithm>
#include <cassert>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#include <stdexcept>
#include <tuple>

namespace stellar7 {
struct NativeSessionTestAccess {
    static SoundBlasterSynth& synth(NativeGameplaySession& session) { return session.synth_; }
    static PresentationClock& clock(NativeGameplaySession& session) { return session.clock_; }
};
}

namespace {
using namespace stellar7;
constexpr NormalWorkOperation enemyFire{NormalWorkKind::gameplayEnemyFireEvent, 1, gameplayEnemyType17Event};
constexpr std::int64_t horizon = 120000;

template<class Function>
void rejects(Function function) {
    bool rejected = false;
    try { function(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
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

SoundBlasterCallbackState fixture() {
    SoundBlasterCallbackState state{};
    state.program.emplace();
    state.program->instrumentBank = bank();
    state.program->volumeTable.resize(16 * 64);
    state.program->accumulator = 0;
    for (std::size_t row = 0; row < 16; ++row) {
        for (std::size_t column = 0; column < 64; ++column) {
            state.program->volumeTable[row * 64 + column] = static_cast<std::uint8_t>((row * 3 + column * 5) % 64);
        }
    }
    state.channelEnabled.fill(1);
    state.voices.channels.fill(0xff);
    state.voices.channels[0] = 1;
    state.sounding[0] = 1;
    for (std::size_t voice = 0; voice < 9; ++voice) {
        state.voices.notes[voice] = static_cast<std::uint8_t>(60 + voice);
        state.velocities[voice] = static_cast<std::uint8_t>(voice < 6 ? 40 + voice : std::array<unsigned,3>{99,104,71}[voice-6]);
        state.program->voices[voice].carrierFallback = static_cast<std::uint16_t>(31 - voice);
        state.program->voices[voice].operatorFallback = static_cast<std::uint16_t>(31 - voice);
    }
    state.sequence.resize(36);
    state.sequence.insert(state.sequence.end(), {0xc1, 0, 0, 0x91, 65, 90, 127, 0xfc});
    state.position = 36;
    return state;
}

std::uint64_t blockHash(const std::vector<SoundBlasterWorkBlock>& blocks) {
    auto hash = 14695981039346656037ULL;
    for (const auto& block : blocks) {
        for (const auto value : {static_cast<std::uint64_t>(block.kind), block.dispatches,
                static_cast<std::uint64_t>(block.registerNumber), static_cast<std::uint64_t>(block.value)}) {
            hash = (hash ^ value) * 1099511628211ULL;
        }
    }
    return hash;
}

auto callbackFields(const SoundBlasterCallbackState& state) {
    return std::make_tuple(state.voices.channels, state.voices.notes, state.voices.tuning,
        state.voices.directTimers, state.velocities, state.sounding, state.channelVoiceCursors,
        state.sequence, state.position, state.wait, state.runningStatus, state.active, state.latch,
        state.program->channelPrograms, state.program->operators);
}

SoundBlasterClockPhase phaseFor(const SoundBlasterCallbackState& state) {
    auto phase = normalWorkTiming(nativeTimerInstallPhase(), timerInstallOperations(), 1000).phase;
    SoundBlasterCallbackState noHandle{};
    noHandle.domain = SoundBlasterCallbackDomain::noHandle;
    noHandle.operation = 0x1e;
    noHandle.active = false;
    phase.callback = noHandle;
    phase.soundBlasterInitialization = AudioClockSoundBlasterInitializationPhase{158, true};
    phase = normalWorkTiming(phase, timerRegistrationOperations(), 1000).phase;
    phase.callback = state;
    phase.sequenceSelection = AudioClockSequenceSelectionPhase{state};
    phase.sequenceSelection->published = true;
    phase.sequenceSelection->songOrdinal = 2; //Third retained handle, Level1 SNG0.
    phase.sequenceSelection->level1DirectResetCompleted = true;
    return phase;
}

const SoundBlasterForegroundEffect& directReceipt(const NormalWorkTiming& work) {
    const auto found = std::find_if(work.soundBlasterEffects.begin(), work.soundBlasterEffects.end(),
        [](const auto& effect) { return effect.kind == SoundBlasterForegroundKind::gameplayEnemyDirectState; });
    assert(found != work.soundBlasterEffects.end() && found->directState);
    return *found;
}

SoundBlasterCallbackState history() {
    auto state = soundBlasterDirectResetState(fixture());
    state = soundBlasterCallbackAdvance(state).state;
    state = soundBlasterDirectAdvance(state,gameplayPrimaryFireDirectRecord,
        SoundBlasterDirectCaller::gameplayPrimaryFire).state;
    for (unsigned callback = 0; callback < 12; ++callback) {
        state = soundBlasterCallbackAdvance(state).state;
    }
    assert((state.voices.directTimers == std::array<std::uint16_t,3>{0,0,0}));
    assert(state.velocities[6] == 99 && state.velocities[7] == 104 && state.velocities[8] == 71);
    return state;
}

struct Control {
    const char* name;
    std::uint16_t event;
    std::array<std::uint16_t,3> timers;
    std::size_t slot;
    std::uint64_t kernelDispatches;
    std::uint64_t kernelHash;
    std::uint64_t adjacentDispatches;
};
constexpr std::array<Control,3> controls{{
    {"free2",gameplayEnemyType17Event,{0,0,0},0,3117,0xf5c031e9407ba3dcULL,76},
    {"busy2",gameplayEnemyType17Event,{5,3,7},1,3541,0x95a2beb20ca90c29ULL,82},
    {"tie2",gameplayEnemyType17Event,{7,7,7},2,3542,0xfe29badc5cec2082ULL,82},
}};

std::vector<std::uint16_t> expectedWrites(const std::size_t slot) {
    const std::array<std::vector<std::uint16_t>,3> writes{{
        {0xbd00,0x0800,0x5000,0xc600,0x70f0,0x9077,0x3001,0xf000,0xbd00,0x0800,
            0x5300,0x73f0,0x9377,0x3301,0xf300,0x533f,0xa657,0xb621,0x5340},
        {0xa757,0xb701,0x5440,0xbd00,0x0800,0x5100,0xc700,0x71f0,0x9177,0x3101,0xf100,
            0xbd00,0x0800,0x5400,0x74f0,0x9477,0x3401,0xf400,0x543f,0xa757,0xb721,0x5440},
        {0xa857,0xb801,0x5540,0xbd00,0x0800,0x5200,0xc800,0x72f0,0x9277,0x3201,0xf200,
            0xbd00,0x0800,0x5500,0x75f0,0x9577,0x3501,0xf500,0x553f,0xa857,0xb821,0x5540}
    }};
    return writes[slot];
}

void sourceControl() {
    const auto incoming = history();
    for (const auto& control : controls) {
        auto state = incoming;
        state.voices.directTimers = control.timers;
        const auto& record = control.event == gameplayEnemyType17Event
            ? gameplayEnemyType17DirectRecord : gameplayEnemyType20DirectRecord;
        rejects([&] { soundBlasterDirectAdvance(state,record); });
        rejects([&] { soundBlasterDirectAdvance(state,record,SoundBlasterDirectCaller::gameplayPrimaryFire); });
        const auto direct = soundBlasterDirectAdvance(state,record,SoundBlasterDirectCaller::gameplayEnemyFire);
        assert(blockHash(direct.blocks) == control.kernelHash);
        std::uint64_t count = 0;
        std::vector<std::uint16_t> writes;
        for (const auto& block : direct.blocks) {
            count += block.dispatches;
            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                writes.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
            }
        }
        assert(count == control.kernelDispatches && writes == expectedWrites(control.slot));
        auto timers = control.timers;
        timers[control.slot] = control.event == gameplayEnemyType17Event ? 60 : 40;
        assert(direct.state.voices.directTimers == timers && direct.state.voices.channels == state.voices.channels);
        assert(direct.state.voices.notes[control.slot+6] == 0 && direct.state.sounding[control.slot+6] == 1);
        assert(direct.state.velocities[0] == 0xff && direct.state.velocities[6] == 99
            && direct.state.velocities[7] == 104 && direct.state.velocities[8] == 71);
        assert(direct.state.program->channelPrograms[control.slot+13] == record[0]);
        assert(direct.state.position == state.position && direct.state.wait == state.wait);
        const auto adjacent = soundBlasterCallbackAdvance(direct.state);
        assert(adjacent.dispatches == control.adjacentDispatches);
        assert(adjacent.state.voices.directTimers[control.slot] == timers[control.slot]-1);
        const auto phase = phaseFor(state);
        const auto work = normalWorkTiming(phase,{{NormalWorkKind::gameplayEnemyFireEvent,1,control.event}},horizon);
        assert(work.total.dispatches == control.kernelDispatches+73 && work.callbacks.empty());
        assert(callbackFields(*directReceipt(work).directState) == callbackFields(direct.state));
        assert(callbackFields(*work.phase.callback) == callbackFields(direct.state));
        assert(work.phase.interrupts == AudioClockInterrupts::enabled);
        writes.clear();
        for (const auto& effect : work.soundBlasterEffects) {
            if (effect.kind == SoundBlasterForegroundKind::runtimeOplWrite) { writes.push_back(effect.value); }
        }
        assert(writes == expectedWrites(control.slot));
    }
    rejects([&] { soundBlasterDirectAdvance(incoming,gameplayPrimaryFireDirectRecord,SoundBlasterDirectCaller::gameplayEnemyFire); });
    rejects([&] { soundBlasterDirectAdvance(incoming,startupLogoDirectRecord,SoundBlasterDirectCaller::gameplayEnemyFire); });
    rejects([&] { soundBlasterDirectAdvance(incoming,gameplayEnemyType20DirectRecord,SoundBlasterDirectCaller::gameplayEnemyFire); });
    auto unknown = gameplayEnemyType17DirectRecord; ++unknown[2];
    rejects([&] { soundBlasterDirectAdvance(incoming,unknown,SoundBlasterDirectCaller::gameplayEnemyFire); });
    const auto phase = phaseFor(incoming);
    for (const auto maskedBoundary : {false,true}) {
        auto raced = phase;
        const auto local = 5000-raced.remainingCycles-raced.deferredCycles;
        raced.remainingCycles = maskedBoundary ? 60 : 1;
        raced.deferredCycles = 5000-local-raced.remainingCycles;
        raced.devices->irq0Requested = true;
        raced.irqCheckRequested = true;
        raced.timer->callbackCount = 1;
        const auto work = normalWorkTiming(raced,{enemyFire},horizon);
        assert(work.callbacks.size() == 1);
        const auto& receipt = directReceipt(work);
        if (maskedBoundary) {
            assert(receipt.cycleOffset < work.callbacks[0].cycleOffset);
            assert(receipt.directState->voices.directTimers[0] == 60 && work.phase.callback->voices.directTimers[0] == 59);
        } else {
            assert(work.callbacks[0].cycleOffset < receipt.cycleOffset);
            const auto live = soundBlasterDirectAdvance(work.callbacks[0].advance.state,gameplayEnemyType17DirectRecord,
                SoundBlasterDirectCaller::gameplayEnemyFire);
            assert(callbackFields(*receipt.directState) == callbackFields(live.state));
        }
    }
    for (unsigned mutation = 0; mutation < 16; ++mutation) {
        auto invalid = phase;
        auto operation = enemyFire;
        if (mutation == 0) { operation.value = gameplayPrimaryFireEvent; }
        if (mutation == 1) { operation.count = 2; }
        if (mutation == 2) { invalid.sequenceSelection->published = false; }
        if (mutation == 3) { invalid.sequenceSelection->songOrdinal = 1; }
        if (mutation == 4) { invalid.sequenceSelection->level1DirectResetCompleted = false; }
        if (mutation == 5) { invalid.callback->active = false; }
        if (mutation == 6) { invalid.callback->program->handleStatus = 0xffff; }
        if (mutation == 7) { invalid.callback->program.reset(); }
        if (mutation == 8) { invalid.interrupts = AudioClockInterrupts::masked; }
        if (mutation == 9) { invalid.callback->gate = SoundBlasterFrequencyGate::Suppressed; }
        if (mutation == 10) { invalid.callback->program->waveformMask = 0; }
        if (mutation == 11) { invalid.callback->mode = 1; }
        if (mutation == 12) { invalid.callback->driverEnabled = false; }
        if (mutation == 13) { invalid.soundBlasterInitialization->driverEnabled = false; }
        if (mutation == 14) { invalid.sequenceSelection.reset(); }
        if (mutation == 15) { operation.value = 26; }
        rejects([&] { normalWorkTiming(invalid,{operation},horizon); });
    }
    rejects([&] { normalWorkTiming(phase,{{NormalWorkKind::gameplayPrimaryFireEvent,1,gameplayEnemyType17Event}},horizon); });
    rejects([&] { normalWorkTiming(phase,{{NormalWorkKind::startupLogoEvent,1,gameplayEnemyType17Event}},horizon); });
    rejects([&] { normalWorkTiming(phase,{{NormalWorkKind::gameplayEnemyFireEvent,1,gameplayEnemyType20Event}},horizon); });
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

void synthReplay() {
    auto game = assets();
    game.soundSequences={std::vector<std::uint8_t>(36)};
    game.soundSequences[0][0]=0x84;
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
    phase = clock.renderWork(phase, {{NormalWorkKind::instructions, 100000}}, horizon).timing.phase;
    assert(phase.callback->position == 47 && phase.callback->wait > 0);
    assert(phase.callback->voices.notes[0] == 65 && phase.callback->velocities[0] == 90);
    // Retain an earlier primary effect and its stopped direct voice history.
    phase = clock.renderWork(phase,{{NormalWorkKind::gameplayPrimaryFireEvent,1,gameplayPrimaryFireEvent}},horizon).timing.phase;
    for (unsigned slice = 0; phase.callback->voices.directTimers[0] != 0 && slice < 32; ++slice) {
        phase = clock.renderWork(phase,{{NormalWorkKind::instructions,100000}},horizon).timing.phase;
    }
    assert(phase.callback->voices.directTimers[0] == 0 && phase.callback->sounding[6] == 0);
    enemy20LiveChecks(clock,synth,phase);
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    sourceControl();
    synthReplay();
    return 0;
}
