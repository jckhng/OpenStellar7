#include "stellar7/audio_direct_work.hpp"
#include "stellar7/level1_loader_work.hpp"
#include "stellar7/native_session.hpp"
#include "stellar7/native_startup.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <memory>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace stellar7 {
struct NativeSessionTestAccess {
    static SoundBlasterSynth& synth(NativeGameplaySession& session) { return session.synth_; }
    static PresentationClock& clock(NativeGameplaySession& session) { return session.clock_; }
};
}

namespace {
using namespace stellar7;
constexpr std::int64_t horizon = 120000;
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

GameplayAssets assets() {
    //The qualified native_startup_test synthetic resource profile; Level1 wait extended for this leaf test.
    GameplayAssets result;
    const CockpitSettings configuration{81,{1,0,1,1,0,255}};
    HighscoreTable table({},{});
    table.setConfiguration(configuration);
    result.cockpitSettings = configuration;
    result.highscoreTable = table;
    result.cockpit = {std::vector<std::uint8_t>(320 * 200), std::vector<Rgb>(256)};
    result.cockpitDacPalette.resize(256 * 3);
    result.uiPalette.assign(256, {12, 20, 28});
    result.soundBlasterVolumeTable.resize(16 * 64);
    for (std::size_t index = 0; index < result.soundBlasterVolumeTable.size(); ++index) {
        result.soundBlasterVolumeTable[index] = static_cast<std::uint8_t>(index % 64);
    }
    result.horizonFrames = {{256, 16, std::vector<std::uint8_t>(256 * 16)}};
    result.artGroups.resize(6, std::vector<IndexedBitmap>(64, {4, 1, {5,5,5,5}}));
    auto& ui = result.artGroups[0];
    ui[0] = {16, 20, std::vector<std::uint8_t>(320, 5)};
    ui[11] = {4, 5, std::vector<std::uint8_t>(20, 9)};
    ui[12] = {40, 40, std::vector<std::uint8_t>(1600, 6)};
    ui[14] = {24,22,std::vector<std::uint8_t>(24 * 22,5)};
    ui[16] = {120,43,std::vector<std::uint8_t>(120 * 43,3)};
    ui[17] = {120,33,std::vector<std::uint8_t>(120 * 33,8)};
    ui[18] = {120,25,std::vector<std::uint8_t>(120 * 25,8)};
    ui[20] = {264, 2, std::vector<std::uint8_t>(528, 4)};
    ui[21] = ui[22] = {8, 5, std::vector<std::uint8_t>(40, 7)};
    result.titleFont = {32, 6, 0, std::vector<IndexedBitmap>(64, {8, 6, std::vector<std::uint8_t>(48, 1)})};
    result.scoreFont = {32,8,0,std::vector<IndexedBitmap>(96,{4,8,std::vector<std::uint8_t>(32,1)})};
    result.modelColorTables.resize(2);
    result.artGroups[3].assign(5,{4,1,{1,2,3,4}});
    auto song = std::vector<std::uint8_t>(36);
    song[0] = 0x84; song[5] = 1; song[6] = 4;
    song.insert(song.end(), {0xc1, 0, 0, 0x91, 60, 64, 3, 0xfc});
    result.logoMusicSequence = result.presentationMusicSequence = result.levelMusicSequence = song;
    result.levelMusicSequence[result.levelMusicSequence.size() - 2] = 127;
    result.logoMusicSequence.resize(36);
    result.logoMusicSequence[5] = 7;
    result.logoMusicSequence.insert(result.logoMusicSequence.end(),{0xc1,0,127,0xfc});
    result.presentationMusicSequence.resize(36);
    for (std::size_t group = 0; group < 8; ++group) {
        result.presentationMusicSequence.insert(result.presentationMusicSequence.end(),{0xc1,0,127});
    }
    result.presentationMusicSequence.push_back(0xfc);
    constexpr std::size_t header = 19;
    constexpr std::size_t half = 0x540;
    auto& bank = result.soundBlasterInstrumentResource;
    bank.resize(header + half * 2 + 2);
    bank[0] = 0x89; bank[1] = 0x11;
    bank[header + half] = 0xab; bank[header + half + 1] = 0xcd;
    for (const auto offset : {header, header + 13}) {
        bank[offset + 1] = 1; bank[offset + 3] = 15;
        bank[offset + 4] = 7; bank[offset + 7] = 7;
    }
    bank[header + 12] = 1;
    return result;
}

SoundBlasterCallbackState stopFixture(const bool latched) {
    SoundBlasterCallbackState state{};
    state.program.emplace();
    state.program->instrumentBank.resize(96 * 28);
    state.program->volumeTable.resize(16 * 64);
    state.program->fadeDivider = 7;
    state.sequence.resize(36);
    state.sequence.insert(state.sequence.end(), {0x91, 65, 90, 5, 0xfc});
    state.position = 36; state.wait = 9;
    state.voices.channels.fill(0xff); state.voices.channels[0] = 1;
    for (std::size_t voice = 0; voice < 9; ++voice) {
        state.voices.notes[voice] = static_cast<std::uint8_t>(60 + voice);
        state.velocities[voice] = static_cast<std::uint8_t>(voice % 3 == 0 ? 0 : 32 + 8 * voice);
        state.sounding[voice] = voice == 2 || voice == 5 ? 0 : 1;
    }
    state.voices.directTimers = {1, 2, 0};
    if (latched) { state.velocities[0] = 0xff; state.latch = SoundBlasterLevelLatch::Latched; }
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

bool sameCallbackState(const SoundBlasterCallbackState& first, const SoundBlasterCallbackState& second) {
    const auto sameProgram = [](const auto& left, const auto& right) {
        if (left.has_value() != right.has_value()) { return false; }
        if (!left) { return true; }
        if (left->instrumentBank != right->instrumentBank || left->volumeTable != right->volumeTable
            || left->playerHitVolumeRow != right->playerHitVolumeRow
            || left->channelPrograms != right->channelPrograms || left->operators != right->operators
            || left->handleStatus != right->handleStatus || left->repeatCount != right->repeatCount
            || left->handleVolume != right->handleVolume || left->waveformMask != right->waveformMask
            || left->accumulator != right->accumulator || left->fadeDivider != right->fadeDivider
            || left->amplitudeDepth != right->amplitudeDepth || left->vibratoDepth != right->vibratoDepth
            || left->noteSelect != right->noteSelect || left->loopMarker != right->loopMarker) { return false; }
        for (std::size_t voice = 0; voice < left->voices.size(); ++voice) {
            const auto& a = left->voices[voice];
            const auto& b = right->voices[voice];
            if (a.program != b.program || a.tuningMagnitude != b.tuningMagnitude
                || a.operatorScale != b.operatorScale || a.operatorFallback != b.operatorFallback
                || a.carrierScale != b.carrierScale || a.carrierFallback != b.carrierFallback
                || a.operatorDisabled != b.operatorDisabled) { return false; }
        }
        return true;
    };
    return first.voices.channels == second.voices.channels && first.voices.notes == second.voices.notes
        && first.voices.tuning == second.voices.tuning && first.voices.channelDeferrals == second.voices.channelDeferrals
        && first.voices.directTimers == second.voices.directTimers && first.velocities == second.velocities
        && first.sounding == second.sounding && first.channelEnabled == second.channelEnabled
        && first.channelVoiceCursors == second.channelVoiceCursors && first.sequence == second.sequence
        && first.position == second.position && first.wait == second.wait && first.fade == second.fade
        && first.mode == second.mode && first.operation == second.operation && first.runningStatus == second.runningStatus
        && first.active == second.active && first.gate == second.gate && first.latch == second.latch
        && first.domain == second.domain && first.driverEnabled == second.driverEnabled
        && sameProgram(first.program, second.program);
}

void retainedStopLeaf() {
    for (const bool latched : {false, true}) {
        const auto input = stopFixture(latched);
        rejects([&] { soundBlasterStopAdvance(input); });
        rejects([&] { soundBlasterStopAdvance(input,static_cast<SoundBlasterStopProfile>(99)); });
        const auto stopped = soundBlasterStopAdvance(input,SoundBlasterStopProfile::retainedDirectTimers);
        assert(blockHash(stopped.blocks) == (latched ? 0x91259d296eb9518bULL : 0x3d2084129baa2443ULL));
        const auto& output = stopped.state;
        assert(output.voices.directTimers == input.voices.directTimers);
        assert(output.voices.notes == input.voices.notes && output.velocities == input.velocities);
        assert(output.position == input.position && output.wait == input.wait && output.fade == input.fade);
        assert(output.program->repeatCount == input.program->repeatCount);
        assert(output.program->handleStatus == input.program->handleStatus && output.active == input.active);
        assert(std::all_of(output.sounding.begin(),output.sounding.end(),[](const auto value) { return value == 0; }));
    }
}

void retainedRetry() {
    const auto game = assets();
    NativeStartupAssets resources;
    resources.logoScreen = {std::vector<std::uint8_t>(320 * 200,9),std::vector<Rgb>(256)};
    resources.menuScreen = resources.logoScreen;
    resources.logoFrames.assign(8,{4,1,{1,0,2,3}});
    NativeStartupSequence startup(game,resources);
    startup.start(NativeStartupExecution::recoveredContext);
    startup.initializeSettings(StartupCapabilityProfile::noMouseOrJoystick);
    startup.initializeResources(NativeStartupDeviceProfile::portableOplNoDspProbe);
    startup.initializePresentation(); startup.initializeFonts();
    while (startup.phase() == NativeStartupPhase::logoCountdownWork) { startup.pollLogoCountdown(10000); }
    startup.playLogoEvent(); startup.animateLogo(); startup.finishLogoSound(); startup.releaseLogo();
    startup.skipIntro(NativeIntroKey::escape); startup.beginMenuSong(); startup.initializeMenuResources();
    startup.skipCredits(NativeIntroKey::escape); startup.enterMenu(NativeMenuEntryProfile::keyboardOnly);
    startup.beginGame(NativeMenuBeginProfile::enterPressRelease);
    auto session = std::make_unique<NativeGameplaySession>(game,startup.releaseGameplay());
    auto& synth = NativeSessionTestAccess::synth(*session);
    auto& clock = NativeSessionTestAccess::clock(*session);
    auto phase = activateLevel1Music(clock,synth,*session->recoveredClockPhase()).phase;
    phase = resetLevel1DirectEffects(clock,synth,phase).phase;
    SoundBlasterVolumeRow row{gameplayPlayerHitCallerVolume,{}};
    synth.configurePlayerHitVolumeRow(row);
    phase.callback->program->playerHitVolumeRow = row;
    phase = clock.renderWork(phase,{{NormalWorkKind::instructions,100000}},horizon).timing.phase;
    assert(phase.callback->voices.notes[0] == 60);
    //A controlled positive countdown distinguishes retained retry from Begin's completed480 wait.
    phase.timer->globalCount = 17;
    const auto initialPit = phase.devices->pitStartMilliseconds;
    const auto pitPeriod = phase.pitPeriodMilliseconds;
    const auto pitDivisor = phase.timerInstall->pitDivisor;
    const auto initialPicTicks = phase.devices->picTicks;
    const auto biosReload = phase.timer->biosReload;
    const auto beforePages = session->retainedPagePixels(NativePageRole::front);
    for (unsigned retry = 0; retry < 2; ++retry) {
        rejects([&] { synth.previewRecoveredLevel1Retry(); });
        rejects([&] { level1RetryMusicActivationPhase(phase,*phase.callback); });
        auto positiveTimers = phase; positiveTimers.callback->voices.directTimers[0] = 1;
        rejects([&] { level1RetryMusicStopPhase(positiveTimers); });
        const auto firstSample = clock.elapsedSamples();
        const auto liveBefore = synth.callbackWorkState();
        const auto writesBefore = synth.registerWrites().size();
        const auto copiedStop = level1RetryMusicStopPhase(phase);
        assert(copiedStop.sequenceSelection->level1RetryStoppedHandle);
        const auto copiedSecondStop = level1RetryMusicStopPhase(copiedStop);
        const auto copiedPrepared = synth.previewRecoveredLevel1Retry(*copiedSecondStop.callback,
            *copiedSecondStop.sequenceSelection->level1RetryStoppedHandle);
        const auto copiedActivation = level1RetryMusicActivationPhase(copiedSecondStop,copiedPrepared);
        assert(copiedActivation.sequenceSelection->published);
        assert(synth.driverState().sequenceSelections[2] && clock.elapsedSamples() == firstSample);
        assert(synth.registerWrites().size() == writesBefore && sameCallbackState(synth.callbackWorkState(),liveBefore));
        auto wrongCurrent = *copiedStop.callback; wrongCurrent.voices.notes[0] ^= 1;
        rejects([&] { synth.previewRecoveredLevel1Retry(wrongCurrent,*copiedStop.sequenceSelection->level1RetryStoppedHandle); });
        auto wrongResource = *copiedStop.sequenceSelection->level1RetryStoppedHandle;
        wrongResource.program->instrumentBank[0] ^= 1;
        rejects([&] { synth.previewRecoveredLevel1Retry(*copiedStop.callback,wrongResource); });
        auto stopped = stopLevel1RetryMusic(clock,synth,phase);
        assert(stopped.firstSample == firstSample && clock.elapsedSamples() == firstSample + stopped.samples.size());
        phase = stopped.phase;
        assert(phase.sequenceSelection->level1RetryCurrentCleared);
        assert(phase.callback->domain == SoundBlasterCallbackDomain::noHandle);
        assert(!synth.driverState().sequenceSelections[2] && synth.driverState().sequenceAllocations[2]);
        const auto writes = synth.registerWrites().size();
        auto second = stopLevel1RetryMusic(clock,synth,phase);
        phase = second.phase;
        assert(second.work.size() == 1 && second.work[0].timing.total.dispatches >= 4);
        assert(synth.registerWrites().size() == writes);
        const auto prepared = synth.previewRecoveredLevel1Retry();
        assert(sameCallbackState(copiedPrepared,prepared));
        rejects([&] { level1MusicActivationPhase(phase,prepared); });
        const auto activation = activateLevel1RetryMusic(clock,synth,phase);
        phase = activation.phase;
        assert(activation.work.size() == 4 && phase.sequenceSelection->published);
        assert(!phase.sequenceSelection->level1RetryStoppedHandle);
        assert(sameCallbackState(*phase.callback,*copiedActivation.callback));
        assert(phase.sequenceSelection->songOrdinal == 2 && !phase.sequenceSelection->level1RetryCurrentCleared);
        assert(!phase.sequenceSelection->level1DirectResetCompleted);
        assert(phase.callback->sequence == game.levelMusicSequence && phase.sequenceSelection->prepared.position == 36);
        assert(phase.callback->program->playerHitVolumeRow == row);
        rejects([&] { activateLevel1RetryMusic(clock,synth,phase); });
        const auto reset = resetLevel1DirectEffects(clock,synth,phase);
        phase = reset.phase;
        assert(phase.sequenceSelection->level1DirectResetCompleted);
        assert(phase.callback->program->playerHitVolumeRow == row);
        assert(phase.devices->pitStartMilliseconds >= initialPit && phase.devices->picTicks >= initialPicTicks);
        assert(phase.pitPeriodMilliseconds == pitPeriod && phase.timerInstall->pitDivisor == pitDivisor);
        assert(phase.timer->biosReload == biosReload);
        assert(phase.timer->globalCount > 0 && phase.timer->globalCount <= 17);
        assert(synth.driverState().sequenceSelections[2]);
        assert(session->model().ticks() == 0 && session->retainedPagePixels(NativePageRole::front) == beforePages);
    }
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    try {
        retainedStopLeaf();
        retainedRetry();
    } catch (const std::exception& error) {
        std::fprintf(stderr,"%s\n",error.what());
        return 1;
    }
    return 0;
}
