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
constexpr NormalWorkOperation combatEvent{NormalWorkKind::gameplayArrivalEvent,1,gameplayArrivalEvent};
constexpr std::int64_t horizon = 120000;
constexpr std::uint16_t arrivalDuration = 360;
constexpr std::uint8_t arrivalProgram = 40;

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
        state.program->channelPrograms, state.program->operators,state.program->playerHitVolumeRow);
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
        [](const auto& effect) { return effect.kind == SoundBlasterForegroundKind::gameplayArrivalDirectState; });
    assert(found != work.soundBlasterEffects.end() && found->directState);
    return *found;
}

SoundBlasterVolumeRow syntheticRow() {
    SoundBlasterVolumeRow row{gameplayPlayerHitCallerVolume,{}};
    for (std::size_t index = 0; index < row.values.size(); ++index) {
        row.values[index] = static_cast<std::uint8_t>(29*index+1);
    }
    return row;
}

SoundBlasterCallbackState history() {
    auto state = soundBlasterDirectResetState(fixture());
    state = soundBlasterCallbackAdvance(state).state;
    state = soundBlasterDirectAdvance(state,gameplayPrimaryFireDirectRecord,SoundBlasterDirectCaller::gameplayPrimaryFire).state;
    for (unsigned callback = 0; callback < 12; ++callback) { state = soundBlasterCallbackAdvance(state).state; }
    state = soundBlasterDirectAdvance(state,gameplayEnemyType17DirectRecord,SoundBlasterDirectCaller::gameplayEnemyFire).state;
    for (unsigned callback = 0; callback < 10; ++callback) { state = soundBlasterCallbackAdvance(state).state; }
    state = soundBlasterDirectAdvance(state,gameplayPrimaryFireDirectRecord,SoundBlasterDirectCaller::gameplayPrimaryFire).state;
    assert((state.voices.directTimers == std::array<std::uint16_t,3>{50,12,0}));
    return state;
}

void compositionControl() {
    const auto historyState = history();
    for (const auto replace : {false,true}) {
        auto state = historyState;
        if (replace) {
            state = soundBlasterDirectAdvance(state,gameplayArrivalDirectRecord,
                SoundBlasterDirectCaller::gameplayArrival).state;
        }
        const auto direct = soundBlasterDirectAdvance(state,gameplayArrivalDirectRecord,
            SoundBlasterDirectCaller::gameplayArrival);
        const auto expected = replace ? std::array<std::uint16_t,3>{50,arrivalDuration,arrivalDuration}
            : std::array<std::uint16_t,3>{50,12,arrivalDuration};
        assert(direct.state.voices.directTimers == expected);
        assert(direct.state.velocities == state.velocities && direct.state.voices.channels == state.voices.channels);
        assert(direct.state.program->channelPrograms[replace ? 14 : 15] == arrivalProgram);
        assert(direct.state.voices.notes[replace ? 7 : 8] == 0);
        const auto adjacent = soundBlasterCallbackAdvance(direct.state);
        assert(adjacent.dispatches == 82);
        for (std::size_t index = 0; index < expected.size(); ++index) {
            assert(adjacent.state.voices.directTimers[index] == expected[index]-1);
        }
        std::uint64_t kernelDispatches = 0;
        for (const auto& block : direct.blocks) { kernelDispatches += block.dispatches; }
        const auto work = normalWorkTiming(phaseFor(state),{combatEvent},horizon);
        assert(work.total.dispatches == kernelDispatches+74 && work.callbacks.empty());
        assert(callbackFields(*work.phase.callback) == callbackFields(direct.state));
        assert(callbackFields(*directReceipt(work).directState) == callbackFields(direct.state));
        std::vector<std::uint16_t> expectedWrites,actualWrites;
        for (const auto& block : direct.blocks) {
            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                expectedWrites.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
            }
        }
        for (const auto& effect : work.soundBlasterEffects) {
            if (effect.kind == SoundBlasterForegroundKind::runtimeOplWrite) { actualWrites.push_back(effect.value); }
        }
        assert(!expectedWrites.empty() && actualWrites == expectedWrites);

        auto changedTable = state;
        for (auto& value : changedTable.program->volumeTable) { value ^= 63; }
        const auto tableIndependent = soundBlasterDirectAdvance(changedTable,gameplayArrivalDirectRecord,
            SoundBlasterDirectCaller::gameplayArrival);
        assert(blockHash(tableIndependent.blocks) == blockHash(direct.blocks));
        auto changedAlias = state;
        constexpr std::size_t aliasOffset = 0x13c3-0x0d28;
        for (std::size_t index = 0; index < 64; ++index) {
            changedAlias.program->instrumentBank[aliasOffset+index] ^= 63;
        }
        const auto bankDependent = soundBlasterDirectAdvance(changedAlias,gameplayArrivalDirectRecord,
            SoundBlasterDirectCaller::gameplayArrival);
        assert(blockHash(bankDependent.blocks) != blockHash(direct.blocks));
    }
    auto invalid = historyState;
    invalid.fade = 1;
    rejects([&] { soundBlasterDirectAdvance(invalid,gameplayArrivalDirectRecord,SoundBlasterDirectCaller::gameplayArrival); });
    rejects([&] { normalWorkTiming(phaseFor(invalid),{combatEvent},horizon); });
    rejects([&] { soundBlasterDirectAdvance(historyState,gameplayPrimaryFireDirectRecord,SoundBlasterDirectCaller::gameplayArrival); });
    rejects([&] { soundBlasterDirectAdvance(historyState,gameplayEnemyType20DirectRecord,SoundBlasterDirectCaller::gameplayArrival); });
    rejects([&] { soundBlasterDirectAdvance(historyState,gameplayArrivalDirectRecord); });
    rejects([&] { normalWorkTiming(phaseFor(historyState),{{NormalWorkKind::gameplayArrivalEvent,1,14}},horizon); });
    rejects([&] { normalWorkTiming(phaseFor(historyState),{{NormalWorkKind::gameplayArrivalEvent,2,3}},horizon); });
    auto phase = phaseFor(historyState);
    phase.sequenceSelection->level1DirectResetCompleted = false;
    rejects([&] { normalWorkTiming(phase,{combatEvent},horizon); });
    phase = phaseFor(historyState); phase.sequenceSelection->songOrdinal = 1;
    rejects([&] { normalWorkTiming(phase,{combatEvent},horizon); });
    phase = phaseFor(historyState); phase.interrupts = AudioClockInterrupts::masked;
    rejects([&] { normalWorkTiming(phase,{combatEvent},horizon); });
    for (const auto maskedBoundary : {false,true}) {
        phase = phaseFor(historyState);
        const auto local = 5000-phase.remainingCycles-phase.deferredCycles;
        phase.remainingCycles = maskedBoundary ? 60 : 1;
        phase.deferredCycles = 5000-local-phase.remainingCycles;
        phase.devices->irq0Requested = true; phase.irqCheckRequested = true; phase.timer->callbackCount = 1;
        const auto work = normalWorkTiming(phase,{combatEvent},horizon);
        assert(work.callbacks.size() == 1);
        const auto& receipt = directReceipt(work);
        if (maskedBoundary) {
            assert(receipt.cycleOffset < work.callbacks[0].cycleOffset);
            assert(receipt.directState->voices.directTimers[2] == arrivalDuration && work.phase.callback->voices.directTimers[2] == arrivalDuration-1);
        } else {
            assert(work.callbacks[0].cycleOffset < receipt.cycleOffset);
            const auto direct = soundBlasterDirectAdvance(work.callbacks[0].advance.state,gameplayArrivalDirectRecord,
                SoundBlasterDirectCaller::gameplayArrival);
            assert(callbackFields(*receipt.directState) == callbackFields(direct.state));
        }
    }
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
    auto row = syntheticRow();
    assert(!phase.callback->program->playerHitVolumeRow);
    auto wrongRow = row; wrongRow.volume = 0;
    rejects([&] { synth.configurePlayerHitVolumeRow(wrongRow); });
    synth.configurePlayerHitVolumeRow(row);
    auto expectedBinding = *phase.callback;
    expectedBinding.program->playerHitVolumeRow = row;
    assert(callbackFields(synth.callbackWorkState()) == callbackFields(expectedBinding));
    phase.callback->program->playerHitVolumeRow = synth.callbackWorkState().program->playerHitVolumeRow;
    rejects([&] { synth.configurePlayerHitVolumeRow(row); });

    phase = clock.renderWork(phase, {{NormalWorkKind::instructions, 100000}}, horizon).timing.phase;
    assert(phase.callback->position == 47 && phase.callback->wait > 0);
    assert(phase.callback->voices.notes[0] == 65 && phase.callback->velocities[0] == 90);
    // Retain an earlier primary effect and its stopped direct voice history.
    phase = clock.renderWork(phase,{{NormalWorkKind::gameplayPrimaryFireEvent,1,gameplayPrimaryFireEvent}},horizon).timing.phase;
    for (unsigned slice = 0; phase.callback->voices.directTimers[0] != 0 && slice < 32; ++slice) {
        phase = clock.renderWork(phase,{{NormalWorkKind::instructions,100000}},horizon).timing.phase;
    }
    assert(phase.callback->voices.directTimers[0] == 0 && phase.callback->sounding[6] == 0);
    const auto work = normalWorkTiming(phase, {combatEvent}, horizon);
    assert(!work.soundBlasterEffects.empty());
    const auto origin = synth.driverState().normalClockCycles;
    const auto before = callbackFields(synth.callbackWorkState());
    const auto writeCount = synth.registerWrites().size();
    for (unsigned mutation = 0; mutation < 6; ++mutation) {
        auto invalid = work;
        auto receipt = std::find_if(invalid.soundBlasterEffects.begin(), invalid.soundBlasterEffects.end(),
            [](const auto& effect) { return effect.kind == SoundBlasterForegroundKind::gameplayArrivalDirectState; });
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
        if (mutation == 5) {
            auto state = std::make_shared<SoundBlasterCallbackState>(*receipt->directState);
            state->program->playerHitVolumeRow->values[63] ^= 1;
            receipt->directState = state;
        }
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
    assert(!continued.callbacks.empty() && continued.phase.callback->voices.directTimers[0] < arrivalDuration);
    rejects([&] { normalWorkTiming(continued.phase,
        {{NormalWorkKind::gameplayEnemyFireEvent,1,gameplayEnemyType20Event}},horizon); });
    auto wrongRecord = normalWorkTiming(continued.phase,{combatEvent},horizon);
    auto receipt = std::find_if(wrongRecord.soundBlasterEffects.begin(),wrongRecord.soundBlasterEffects.end(),
        [](const auto& effect) { return effect.kind == SoundBlasterForegroundKind::gameplayArrivalDirectState; });
    assert(receipt != wrongRecord.soundBlasterEffects.end());
    receipt->value = gameplayEnemyType20Event;
    rejects([&] { synth.renderClocked(wrongRecord,synth.driverState().normalClockCycles); });
    assert(callbackFields(synth.callbackWorkState()) == callbackFields(*continued.phase.callback));
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    compositionControl();
    synthReplay();
    return 0;
}
