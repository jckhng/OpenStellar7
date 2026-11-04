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
constexpr NormalWorkOperation primaryFire{NormalWorkKind::gameplayPrimaryFireEvent, 1, gameplayPrimaryFireEvent};
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
        state.velocities[voice] = static_cast<std::uint8_t>(40 + voice);
        state.program->voices[voice].carrierFallback = static_cast<std::uint16_t>(31 - voice);
        state.program->voices[voice].operatorFallback = static_cast<std::uint16_t>(31 - voice);
    }
    state.sequence.resize(36);
    state.sequence.insert(state.sequence.end(), {0xc1, 0, 0, 0x91, 65, 90, 5, 0xfc});
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
        [](const auto& effect) { return effect.kind == SoundBlasterForegroundKind::gameplayDirectState; });
    assert(found != work.soundBlasterEffects.end() && found->directState);
    return *found;
}

void sourceControl() {
    const auto reset = soundBlasterDirectResetState(fixture());
    assert(reset.voices.channels[6] == 13 && reset.voices.channels[7] == 14 && reset.voices.channels[8] == 15);
    const auto history = soundBlasterCallbackAdvance(reset);
    assert(history.dispatches == 3681 && history.state.position == 43 && history.state.wait == 4);
    rejects([&] { soundBlasterDirectAdvance(history.state, gameplayPrimaryFireDirectRecord); });
    rejects([&] { soundBlasterDirectAdvance(history.state, startupLogoDirectRecord,
        SoundBlasterDirectCaller::gameplayPrimaryFire); });
    rejects([&] { soundBlasterDirectAdvance(history.state, gameplayPrimaryFireDirectRecord,
        static_cast<SoundBlasterDirectCaller>(99)); });
    const auto direct = soundBlasterDirectAdvance(history.state, gameplayPrimaryFireDirectRecord,
        SoundBlasterDirectCaller::gameplayPrimaryFire);
    assert(blockHash(direct.blocks) == 0xf5c031e9407ba3dcULL);
    std::uint64_t count = 0;
    std::vector<std::uint16_t> writes;
    for (const auto& block : direct.blocks) {
        count += block.dispatches;
        if (block.kind == SoundBlasterWorkKind::registerWrite) {
            writes.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
        }
    }
    const std::vector<std::uint16_t> expectedWrites{0xbd00, 0x0800, 0x5000, 0xc600, 0x70f0, 0x9077, 0x3001,
        0xf000, 0xbd00, 0x0800, 0x5300, 0x73f0, 0x9377, 0x3301, 0xf300, 0x533f, 0xa657, 0xb621, 0x5340};
    assert(count == 3117 && writes == expectedWrites);
    assert((direct.state.voices.directTimers == std::array<std::uint16_t, 3>{12, 0, 0}));
    assert(direct.state.voices.notes[6] == 0 && direct.state.sounding[6] == 1);
    assert(direct.state.velocities[0] == 0xff && direct.state.program->channelPrograms[13] == 54);
    assert(direct.state.position == history.state.position && direct.state.wait == history.state.wait);
    const auto adjacent = soundBlasterCallbackAdvance(direct.state);
    assert(adjacent.dispatches == 76 && adjacent.state.wait == 3 && adjacent.state.voices.directTimers[0] == 11);

    const auto phase = phaseFor(history.state);
    const auto work = normalWorkTiming(phase, {primaryFire}, horizon);
    assert(work.total.dispatches == 3191 && work.callbacks.empty());
    assert(callbackFields(*directReceipt(work).directState) == callbackFields(direct.state));
    assert(callbackFields(*work.phase.callback) == callbackFields(direct.state));
    assert(work.phase.interrupts == AudioClockInterrupts::enabled);
    assert(work.phase.sequenceSelection->songOrdinal == 2 && work.phase.sequenceSelection->level1DirectResetCompleted);
    assert(work.phase.sequenceSelection->logoEventPlayed == phase.sequenceSelection->logoEventPlayed);
    writes.clear();
    for (const auto& effect : work.soundBlasterEffects) {
        if (effect.kind == SoundBlasterForegroundKind::runtimeOplWrite) { writes.push_back(effect.value); }
    }
    assert(writes == expectedWrites);

    for (const auto maskedBoundary : {false, true}) {
        auto raced = phase;
        const auto local = 5000 - raced.remainingCycles - raced.deferredCycles;
        raced.remainingCycles = maskedBoundary ? 60 : 1;
        raced.deferredCycles = 5000 - local - raced.remainingCycles;
        raced.devices->irq0Requested = true;
        raced.irqCheckRequested = true;
        raced.timer->callbackCount = 1;
        const auto result = normalWorkTiming(raced, {primaryFire}, horizon);
        assert(result.callbacks.size() == 1);
        const auto& receipt = directReceipt(result);
        if (maskedBoundary) {
            assert(receipt.cycleOffset < result.callbacks[0].cycleOffset);
            assert(receipt.directState->voices.directTimers[0] == 12);
            assert(result.phase.callback->voices.directTimers[0] == 11);
        } else {
            assert(result.callbacks[0].cycleOffset < receipt.cycleOffset);
            const auto live = soundBlasterDirectAdvance(result.callbacks[0].advance.state, gameplayPrimaryFireDirectRecord,
                SoundBlasterDirectCaller::gameplayPrimaryFire);
            assert(callbackFields(*receipt.directState) == callbackFields(live.state));
            assert(receipt.directState->wait == 3 && result.phase.callback->voices.directTimers[0] == 12);
        }
    }

    for (unsigned mutation = 0; mutation < 15; ++mutation) {
        auto invalid = phase;
        auto operation = primaryFire;
        if (mutation == 0) { operation.value = 26; }
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
        rejects([&] { normalWorkTiming(invalid, {operation}, horizon); });
    }
    rejects([&] { normalWorkTiming(phase, {{NormalWorkKind::startupLogoEvent, 1, 14}}, horizon); });
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
    const auto work = normalWorkTiming(phase, {primaryFire}, horizon);
    assert(!work.soundBlasterEffects.empty());
    const auto origin = synth.driverState().normalClockCycles;
    const auto before = callbackFields(synth.callbackWorkState());
    const auto writeCount = synth.registerWrites().size();
    for (unsigned mutation = 0; mutation < 5; ++mutation) {
        auto invalid = work;
        auto receipt = std::find_if(invalid.soundBlasterEffects.begin(), invalid.soundBlasterEffects.end(),
            [](const auto& effect) { return effect.kind == SoundBlasterForegroundKind::gameplayDirectState; });
        assert(receipt != invalid.soundBlasterEffects.end());
        if (mutation == 0) { receipt->value = 26; }
        if (mutation == 1) { receipt->directState.reset(); }
        if (mutation == 2) {
            auto state = std::make_shared<SoundBlasterCallbackState>(*receipt->directState);
            ++state->voices.directTimers[0];
            receipt->directState = state;
        }
        if (mutation == 3) { invalid.soundBlasterEffects.front().value ^= 1; }
        if (mutation == 4) { receipt->kind = SoundBlasterForegroundKind::directState; }
        rejects([&] { synth.renderClocked(invalid, origin); });
        assert(callbackFields(synth.callbackWorkState()) == before);
        assert(synth.registerWrites().size() == writeCount && synth.driverState().normalClockCycles == origin);
    }
    const auto audio = synth.renderClocked(work, origin);
    assert(callbackFields(synth.callbackWorkState()) == callbackFields(*work.phase.callback));
    assert(!audio.samples.empty() && audio.writes.size() >= 3);
    for (const auto& effect : work.soundBlasterEffects) {
        if (effect.kind != SoundBlasterForegroundKind::runtimeOplWrite) { continue; }
        const auto write = std::find_if(audio.writes.begin(), audio.writes.end(), [&](const auto& item) {
            return item.cycleOffset == effect.cycleOffset;
        });
        assert(write != audio.writes.end());
        assert(write->write.registerNumber == effect.value >> 8 && write->write.value == (effect.value & 0xff));
    }
    assert(synth.driverState().normalClockCycles == origin + work.total.elapsedCycles);
    const auto continued = normalWorkTiming(work.phase, {{NormalWorkKind::instructions, 100000}}, horizon);
    synth.renderClocked(continued, synth.driverState().normalClockCycles);
    assert(callbackFields(synth.callbackWorkState()) == callbackFields(*continued.phase.callback));
    assert(!continued.callbacks.empty() && continued.phase.callback->voices.directTimers[0] < 12);
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    sourceControl();
    synthReplay();
}
