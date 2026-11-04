#include "stellar7/native_startup.hpp"
#include "../src/retained_menu_clock_domain.hpp"
#include "stellar7/native_session.hpp"
#include "stellar7/timer_install_work.hpp"
#include "stellar7/audio_direct_work.hpp"
#include "stellar7/level1_loader_work.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
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
GameplayAssets gameAssets() {
    GameplayAssets assets;
    assets.cockpit = {std::vector<std::uint8_t>(320 * 200), std::vector<Rgb>(256)};
    assets.cockpitDacPalette.resize(256 * 3);
    assets.uiPalette = std::vector<Rgb>(256, {12, 20, 28});
    assets.soundBlasterVolumeTable.resize(16 * 64);
    for (std::size_t i = 0; i < assets.soundBlasterVolumeTable.size(); ++i) {
        assets.soundBlasterVolumeTable[i] = static_cast<std::uint8_t>(i % 64);
    }
    assets.horizonFrames = {{256, 16, std::vector<std::uint8_t>(256 * 16)}};
    assets.artGroups.resize(6, std::vector<IndexedBitmap>(64, {1, 1, {5}}));
    auto& ui = assets.artGroups[0];
    ui[0] = {16, 20, std::vector<std::uint8_t>(16 * 20, 5)};
    ui[11] = {4, 5, std::vector<std::uint8_t>(4 * 5, 9)};
    ui[12] = {40, 40, std::vector<std::uint8_t>(40 * 40, 6)};
    ui[16] = {112, 16, std::vector<std::uint8_t>(112 * 16, 3)};
    ui[17] = ui[18] = {116, 30, std::vector<std::uint8_t>(116 * 30, 8)};
    ui[20] = {264, 2, std::vector<std::uint8_t>(264 * 2, 4)};
    ui[21] = ui[22] = {8, 5, std::vector<std::uint8_t>(8 * 5, 7)};
    assets.titleFont = {32, 6, 0, std::vector<IndexedBitmap>(64, {8, 6, std::vector<std::uint8_t>(48, 1)})};
    auto song = std::vector<std::uint8_t>(36);
    song[0] = 0x84; song[5] = 1; song[6] = 4;
    song.insert(song.end(), {0xc1, 0, 0, 0x91, 60, 64, 3, 0xfc});
    constexpr std::size_t header = 19;
    constexpr std::size_t half = 0x540;
    auto& bank = assets.soundBlasterInstrumentResource;
    bank.resize(header + half * 2 + 2);
    bank[0] = 0x89; bank[1] = 0x11;
    bank[header + half] = 0xab; bank[header + half + 1] = 0xcd;
    for (const auto offset : {header, header + 13}) {
        bank[offset + 1] = 1; bank[offset + 3] = 15;
        bank[offset + 4] = 7; bank[offset + 7] = 7;
    }
    assets.levelMusicSequence = song;
    assets.logoMusicSequence = song;
    assets.presentationMusicSequence = song;
    return assets;
}
NativeStartupAssets startupAssets() {
    NativeStartupAssets result;
    result.logoScreen = {std::vector<std::uint8_t>(320 * 200, 9), std::vector<Rgb>(256)};
    result.menuScreen = result.logoScreen;
    result.logoFrames.resize(8, {1, 1, {7}});
    return result;
}
auto snapshot(const SoundBlasterSynth& synth) {
    const auto state = synth.driverState();
    return std::make_tuple(state.notes, state.velocities, state.directTimers,
        state.channelVoiceCursors, state.clockPhase, state.clockPeriod,
        state.sequencePosition, state.sequenceWait, state.runningStatus);
}

void assertRecoveredContext(const GameplayAssets& game, const NativeStartupAssets& resources) {
    NativeStartupSequence startup(game, resources);
    assert(startup.recoveredContext() == nullptr && startup.elapsedSamples() == 0);
    startup.start(NativeStartupExecution::recoveredContext);
    assert(startup.phase() == NativeStartupPhase::awaitingSettings);
    const auto* run = startup.recoveredContext();
    assert(run != nullptr && startup.logoSteps() == 0 && startup.frame().pixels.empty());
    assert(run->context.state.matrix.mode == 3);
    constexpr std::array<std::int16_t, 9> matrix{-11586, -11587, 1, 8192, -8192, 11585, -8194, 8193, 11584};
    assert(run->context.state.matrix.values == matrix);
    assert((run->context.state.light == FixedPosition{256, -256, 362}));
    assert(std::all_of(run->context.state.allocationFlags.begin(), run->context.state.allocationFlags.end(),
        [](const auto flag) { return flag == 0; }));
    assert(run->installer.timing.total.dispatches == 105 && run->installer.timing.total.delayCycles == 15);
    assert(run->work.timing.total.dispatches == 487 && run->work.timing.total.delayCycles == 0);
    assert(run->work.timing.callbacks.empty() && run->work.audio.writes.empty());
    assert(run->installer.audio.firstSample == 0);
    assert(run->work.audio.firstSample == run->installer.audio.samples.size());
    const auto samples = run->installer.audio.samples.size() + run->work.audio.samples.size();
    assert(startup.elapsedSamples() == samples && samples != 0);
    const auto& before = run->installer.timing.phase;
    const auto& after = run->work.timing.phase;
    assert(after.timerInstall->stage == AudioClockTimerInstallStage::installed);
    assert(after.devices->pitStartMilliseconds == before.devices->pitStartMilliseconds);
    assert(after.timer->biosCount == before.timer->biosCount && after.timer->activeMask == 0);
    assert(after.timer->globalCount == 0 && after.timer->callbackCount == 0);

    SoundBlasterSynth packetSynth({}, {}, {}, {}, {}, {}, {}, {}, SoundBlasterInitialization::recoveredWork);
    PresentationClock packetClock(packetSynth);
    auto initialPhase = nativeTimerInstallPhase();
    initialPhase.soundBlasterInitialization.emplace();
    const auto installed = packetClock.renderWork(initialPhase, timerInstallOperations(), 1000);
    auto retained = installed.timing.phase;
    std::vector<std::int16_t> packetPcm;
    for (const auto& operation : run->context.operations) {
        assert(operation.kind == NormalWorkKind::instructions);
        for (std::uint64_t index = 0; index < operation.count; ++index) {
            const auto next = packetClock.renderWork(retained, {{NormalWorkKind::instructions, 1}}, 1000);
            retained = next.timing.phase;
            packetPcm.insert(packetPcm.end(), next.audio.samples.begin(), next.audio.samples.end());
        }
    }
    assert(packetPcm == run->work.audio.samples && packetClock.elapsedSamples() == startup.elapsedSamples());
    assert(retained.remainingCycles == after.remainingCycles && retained.deferredCycles == after.deferredCycles);
    assert(retained.linesDone == after.linesDone && retained.devices->picTicks == after.devices->picTicks);
    assert(retained.devices->pitStartMilliseconds == after.devices->pitStartMilliseconds);
    assert(retained.devices->pitNowMilliseconds == after.devices->pitNowMilliseconds);
    assert(retained.events.size() == after.events.size());
    for (std::size_t index = 0; index < retained.events.size(); ++index) {
        assert(retained.events[index].kind == after.events[index].kind);
        assert(retained.events[index].value == after.events[index].value);
        assert(retained.events[index].deadlineMilliseconds == after.events[index].deadlineMilliseconds);
    }

    for (const auto action : {0, 1, 2, 3, 4, 5}) {
        bool rejected = false;
        try {
            switch (action) {
            case 0: startup.advance(1); break;
            case 1: startup.start(); break;
            case 2: startup.start(NativeStartupExecution::recoveredContext); break;
            case 3: startup.acceptSkipInput(); break;
            case 4: startup.requestBegin(); break;
            case 5: startup.releaseAudio(); break;
            }
        } catch (const std::logic_error&) {
            rejected = true;
        }
        assert(rejected && startup.recoveredContext() == run && startup.elapsedSamples() == samples);
        assert(startup.phase() == NativeStartupPhase::awaitingSettings);
    }
    NativeStartupSequence unknown(game, resources);
    bool rejected = false;
    try {
        unknown.start(static_cast<NativeStartupExecution>(99));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && unknown.phase() == NativeStartupPhase::idle);
    assert(unknown.elapsedSamples() == 0 && unknown.recoveredContext() == nullptr);
}

void assertRecoveredSettings(GameplayAssets game, const NativeStartupAssets& resources) {
    const CockpitSettings configuration{81, {1, 0, 1, 1, 0, 255}};
    HighscoreTable table({}, {});
    table.setConfiguration(configuration);
    game.cockpitSettings = configuration;
    game.highscoreTable = table;
    NativeStartupSequence startup(game, resources);
    startup.start(NativeStartupExecution::recoveredContext);
    const auto* context = startup.recoveredContext();
    const auto beforeSamples = startup.elapsedSamples();
    const auto matrix = context->context.state.matrix.values;
    startup.initializeSettings(StartupCapabilityProfile::noMouseOrJoystick);
    assert(startup.phase() == NativeStartupPhase::awaitingResourceInitialization);
    const auto* settings = startup.recoveredSettings();
    assert(settings != nullptr && startup.recoveredContext() == context);
    assert(settings->settings.configuration.viewSize == 81);
    assert(settings->settings.configuration.options[0] == 0);
    assert(settings->settings.loadedTable.header() == table.header());
    assert(settings->settings.viewAccumulator == 252);
    assert((settings->settings.viewDescriptor.words == std::array<std::int16_t, 6>{0, 0, 0, 252, 90, 0}));
    assert(context->context.state.viewDescriptor.words == settings->settings.viewDescriptor.words);
    assert(context->context.state.matrix.values == matrix);
    assert((context->context.state.light == FixedPosition{256, -256, 362}));
    assert(settings->work.timing.total.dispatches == 89 && settings->work.timing.total.delayCycles == 0);
    assert(settings->work.audio.firstSample == beforeSamples);
    assert(startup.elapsedSamples() == beforeSamples + settings->work.audio.samples.size());
    assert(settings->work.timing.callbacks.empty() && settings->work.audio.writes.empty());
    assert(settings->work.timing.phase.devices->pitStartMilliseconds == context->work.timing.phase.devices->pitStartMilliseconds);
    assert(settings->work.timing.phase.timer->activeMask == 0 && settings->work.timing.phase.timer->globalCount == 0);
    std::vector<NormalWorkOperation> individualWork;
    for (unsigned index = 0; index < 89; ++index) {
        individualWork.push_back({NormalWorkKind::instructions, 1});
    }
    const auto packet = normalWorkTiming(context->work.timing.phase, individualWork, 1000);
    const auto& returned = settings->work.timing.phase;
    assert(packet.phase.remainingCycles == returned.remainingCycles && packet.phase.deferredCycles == returned.deferredCycles);
    assert(packet.phase.linesDone == returned.linesDone && packet.phase.events.size() == returned.events.size());
    for (std::size_t index = 0; index < packet.phase.events.size(); ++index) {
        assert(packet.phase.events[index].kind == returned.events[index].kind);
        assert(packet.phase.events[index].value == returned.events[index].value);
        assert(packet.phase.events[index].deadlineMilliseconds == returned.events[index].deadlineMilliseconds);
    }
    const auto completedSamples = startup.elapsedSamples();
    for (const auto action : {0, 1, 2, 3}) {
        bool rejected = false;
        try {
            switch (action) {
            case 0: startup.initializeSettings(StartupCapabilityProfile::noMouseOrJoystick); break;
            case 1: startup.advance(1); break;
            case 2: startup.requestBegin(); break;
            case 3: startup.releaseAudio(); break;
            }
        } catch (const std::logic_error&) {
            rejected = true;
        }
        assert(rejected && startup.elapsedSamples() == completedSamples);
        assert(startup.recoveredSettings() == settings);
    }

    for (const auto rejectedCase : {0, 1, 2, 3, 4, 5, 6}) {
        auto invalid = game;
        auto profile = StartupCapabilityProfile::noMouseOrJoystick;
        switch (rejectedCase) {
        case 0: invalid.cockpitSettings.reset(); break;
        case 1: invalid.highscoreTable.reset(); break;
        case 2: invalid.cockpitSettings->viewSize = 83; break;
        case 3: invalid.cockpitSettings->options[1] = 1; break;
        case 4: invalid.cockpitSettings->options[4] = 7; break;
        case 5: profile = static_cast<StartupCapabilityProfile>(99); break;
        case 6: invalid.cockpitSettings->viewSize = -1; break;
        }
        NativeStartupSequence guarded(invalid, resources);
        guarded.start(NativeStartupExecution::recoveredContext);
        const auto before = guarded.elapsedSamples();
        const auto* retained = guarded.recoveredContext();
        bool rejected = false;
        try {
            guarded.initializeSettings(profile);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected && guarded.elapsedSamples() == before && guarded.recoveredContext() == retained);
        assert(guarded.phase() == NativeStartupPhase::awaitingSettings && guarded.recoveredSettings() == nullptr);
        assert((retained->context.state.viewDescriptor.words == std::array<std::int16_t, 6>{}));
    }

    struct ViewCase { std::int16_t setting, accumulator, width, scale; std::uint64_t dispatches; };
    for (const auto& control : {ViewCase{0, 0, 64, 9, 90}, ViewCase{20, 62, 64, 12, 90},
        ViewCase{21, 65, 65, 17, 89}, ViewCase{81, 252, 252, 90, 89}, ViewCase{82, 256, 255, 128, 90}}) {
        auto input = configuration;
        input.viewSize = control.setting;
        auto loaded = table;
        loaded.setConfiguration(input);
        StartupContextState incoming{};
        incoming.viewDescriptor.words = {3, 5, 7, 0, 0, 11};
        const auto plan = startupSettingsOperations(incoming, input, loaded, StartupCapabilityProfile::noMouseOrJoystick);
        assert(plan.viewAccumulator == control.accumulator);
        assert((plan.viewDescriptor.words == std::array<std::int16_t, 6>{3, 5, 7, control.width, control.scale, 11}));
        std::uint64_t dispatches = 0;
        for (const auto& operation : plan.operations) {
            assert(operation.kind == NormalWorkKind::instructions);
            dispatches += operation.count;
        }
        assert(dispatches == control.dispatches);
        assert(plan.loadedTable.header() == loaded.header() && plan.configuration.options[0] == 0);
    }
    auto disabled = configuration;
    disabled.options[0] = 0;
    auto loaded = table;
    loaded.setConfiguration(disabled);
    const auto noQuery = startupSettingsOperations({}, disabled, loaded, StartupCapabilityProfile::noMouseOrJoystick);
    std::uint64_t dispatches = 0;
    for (const auto& operation : noQuery.operations) {
        dispatches += operation.count;
    }
    assert(dispatches == 85 && noQuery.hostServices.size() == 2);
    assert(noQuery.effects.size() == 5 && noQuery.configuration.options == disabled.options);
}

SoundBlasterSynth stagedSynth(const GameplayAssets& game) {
    return SoundBlasterSynth(game.soundSequences, game.soundBlasterInstrumentResource,
        game.musicForLevel(0), game.presentationMusicSequence, game.musicForLevel(1),
        game.musicForLevel(2), game.logoMusicSequence, game.soundBlasterVolumeTable,
        SoundBlasterInitialization::recoveredWork);
}

void assertRecoveredResources(GameplayAssets game, const NativeStartupAssets& resources) {
    const CockpitSettings configuration{81, {1, 0, 1, 1, 0, 255}};
    HighscoreTable table({}, {});
    table.setConfiguration(configuration);
    game.cockpitSettings = configuration;
    game.highscoreTable = table;
    NativeStartupSequence startup(game, resources);
    startup.start(NativeStartupExecution::recoveredContext);
    startup.initializeSettings(StartupCapabilityProfile::noMouseOrJoystick);
    const auto* context = startup.recoveredContext();
    const auto* settings = startup.recoveredSettings();
    const auto before = startup.elapsedSamples();
    assert(!settings->work.timing.phase.soundBlasterInitialization->driverEnabled);
    assert(settings->work.timing.phase.soundBlasterInitialization->oplWrites == 0);
    startup.initializeResources(NativeStartupDeviceProfile::portableOplNoDspProbe);
    assert(startup.phase() == NativeStartupPhase::awaitingPresentationResources);
    const auto* initialized = startup.recoveredResources();
    assert(initialized != nullptr && startup.recoveredContext() == context && startup.recoveredSettings() == settings);
    const auto& work = initialized->work;
    assert(work.audio.firstSample == before && startup.elapsedSamples() == before + work.audio.samples.size());
    assert(initialized->resources.state.driverEnabled && initialized->resources.state.callbackRegistered);
    assert(initialized->transferBuffer.size() == 32 * 32 && initialized->scratchBuffer.size() == 500);
    assert(initialized->resources.driverInitialization.state.ready);
    assert(initialized->resources.driverInitialization.state.operation2Entry == 0x2d83);
    assert(initialized->resources.driverInitialization.state.bank.size() == 2 * 0x540);
    assert(initialized->resources.driverInitialization.hostServices.size() == 1);
    assert(work.timing.phase.soundBlasterInitialization->driverEnabled);
    assert(work.timing.phase.soundBlasterInitialization->oplWrites == soundBlasterInitializationWriteCount);
    assert(work.timing.phase.timer->activeMask == 1 && work.timing.phase.timer->callbackReload == 4);
    assert(work.timing.phase.timer->globalCount == 0);
    assert(work.timing.phase.callback->domain == SoundBlasterCallbackDomain::noHandle);
    assert(work.timing.phase.callback->sequence.empty() && !work.timing.phase.callback->active);
    assert(std::all_of(work.timing.phase.callback->voices.channels.begin(), work.timing.phase.callback->voices.channels.end(),
        [](const auto channel) { return channel == 0; }));
    SoundBlasterSynth compatibility;
    assert(work.audio.writes.size() == compatibility.registerWrites().size());
    for (std::size_t index = 0; index < work.audio.writes.size(); ++index) {
        const auto& current = work.audio.writes[index];
        const auto& expected = compatibility.registerWrites()[index];
        assert(current.write.registerNumber == expected.registerNumber && current.write.value == expected.value);
        assert(current.cycleOffset > 0 && current.cycleOffset < work.timing.total.elapsedCycles);
        if (index != 0) {
            assert(current.cycleOffset > work.audio.writes[index - 1].cycleOffset);
        }
    }

    auto splitSynth = stagedSynth(game);
    assert(splitSynth.registerWrites().empty() && !splitSynth.driverState().driverEnabled);
    assert(!splitSynth.callbackWorkState().driverEnabled);
    for (const auto action : {0, 1, 2}) {
        bool rejected = false;
        try {
            switch (action) {
            case 0: splitSynth.startMusic(); break;
            case 1: splitSynth.render(1); break;
            case 2: splitSynth.resetDirectEffects(); break;
            }
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected && splitSynth.registerWrites().empty() && !splitSynth.driverState().driverEnabled);
    }
    PresentationClock splitClock(splitSynth);
    auto phase = nativeTimerInstallPhase();
    phase.soundBlasterInitialization.emplace();
    phase = splitClock.renderWork(phase, timerInstallOperations(), 1000).timing.phase;
    phase = splitClock.renderWork(phase, context->context.operations, 1000).timing.phase;
    phase = splitClock.renderWork(phase, settings->settings.operations, 1000).timing.phase;
    assert(splitClock.elapsedSamples() == before);
    std::vector<std::int16_t> pcm;
    std::vector<ClockedOplRegisterWrite> writes;
    std::int64_t elapsed = 0;
    for (const auto& operation : initialized->resources.operations) {
        const auto part = splitClock.renderWork(phase, {operation}, 2000);
        pcm.insert(pcm.end(), part.audio.samples.begin(), part.audio.samples.end());
        for (const auto& write : part.audio.writes) {
            writes.push_back({elapsed + write.cycleOffset, write.write});
        }
        elapsed += part.timing.total.elapsedCycles;
        phase = part.timing.phase;
    }
    assert(pcm == work.audio.samples && elapsed == work.timing.total.elapsedCycles);
    assert(splitClock.elapsedSamples() == startup.elapsedSamples() && writes.size() == work.audio.writes.size());
    for (std::size_t index = 0; index < writes.size(); ++index) {
        assert(writes[index].cycleOffset == work.audio.writes[index].cycleOffset);
        assert(writes[index].write.registerNumber == work.audio.writes[index].write.registerNumber);
        assert(writes[index].write.value == work.audio.writes[index].write.value);
    }
    assert(phase.remainingCycles == work.timing.phase.remainingCycles && phase.deferredCycles == work.timing.phase.deferredCycles);
    assert(phase.timer->biosCount == work.timing.phase.timer->biosCount && phase.timer->callbackCount == work.timing.phase.timer->callbackCount);
    assert(phase.events.size() == work.timing.phase.events.size());
    for (std::size_t index = 0; index < phase.events.size(); ++index) {
        assert(phase.events[index].kind == work.timing.phase.events[index].kind);
        assert(phase.events[index].deadlineMilliseconds == work.timing.phase.events[index].deadlineMilliseconds);
    }
    assert(splitSynth.driverState().driverEnabled && splitSynth.driverState().initializationOplWrites == 158);
    assert(splitSynth.registerWrites().size() == 158);
    for (const auto action : {0, 1, 2, 3}) {
        bool rejected = false;
        try {
            switch (action) {
            case 0: startup.initializeResources(NativeStartupDeviceProfile::portableOplNoDspProbe); break;
            case 1: startup.advance(1); break;
            case 2: startup.requestBegin(); break;
            case 3: startup.releaseAudio(); break;
            }
        } catch (const std::logic_error&) {
            rejected = true;
        }
        assert(rejected && startup.elapsedSamples() == splitClock.elapsedSamples());
    }
    for (const auto control : {0, 1, 2, 3}) {
        auto invalid = game;
        auto profile = NativeStartupDeviceProfile::portableOplNoDspProbe;
        switch (control) {
        case 0: invalid.soundBlasterVolumeTable.clear(); break;
        case 1: invalid.soundBlasterInstrumentResource.pop_back(); break;
        case 2:
            invalid.cockpitSettings->options[2] = 0;
            invalid.highscoreTable->setConfiguration(*invalid.cockpitSettings);
            break;
        case 3: profile = static_cast<NativeStartupDeviceProfile>(99); break;
        }
        NativeStartupSequence guarded(invalid, resources);
        guarded.start(NativeStartupExecution::recoveredContext);
        guarded.initializeSettings(StartupCapabilityProfile::noMouseOrJoystick);
        const auto samples = guarded.elapsedSamples();
        bool rejected = false;
        try {
            guarded.initializeResources(profile);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected && guarded.elapsedSamples() == samples && guarded.recoveredResources() == nullptr);
        assert(guarded.phase() == NativeStartupPhase::awaitingResourceInitialization);
    }
}

void assertRecoveredPresentation(GameplayAssets game, NativeStartupAssets resources) {
    const CockpitSettings configuration{81, {1, 0, 1, 1, 0, 255}};
    HighscoreTable table({}, {});
    table.setConfiguration(configuration);
    game.cockpitSettings = configuration;
    game.highscoreTable = table;
    resources.logoScreen.palette[1] = {60, 120, 180};
    NativeStartupSequence startup(game, resources);
    startup.start(NativeStartupExecution::recoveredContext);
    startup.initializeSettings(StartupCapabilityProfile::noMouseOrJoystick);
    startup.initializeResources(NativeStartupDeviceProfile::portableOplNoDspProbe);
    const auto* initialization = startup.recoveredResources();
    const auto before = startup.elapsedSamples();
    startup.initializePresentation();
    const auto* run = startup.recoveredPresentation();
    assert(startup.phase() == NativeStartupPhase::awaitingFontResources && run != nullptr);
    assert(startup.recoveredResources() == initialization && run->firstSample == before);
    assert(startup.elapsedSamples() == before + run->samples.size());
    assert(run->songs.size() == 1 && run->loadedHead == 0 && run->selectedHead == 0);
    assert(!run->songs[0].next && run->songs[0].repeat == 0xffff);
    assert(run->songs[0].payload == game.logoMusicSequence);
    assert(run->finalPhase.callback->sequence == game.logoMusicSequence && run->finalPhase.callback->position == 36);
    assert(run->finalPhase.callback->wait == 0 && run->finalPhase.callback->active);
    assert(run->finalPhase.sequenceSelection->published && run->finalPhase.sequenceSelection->countdownWritten);
    assert(startup.countdown() == 240 && run->palettes.size() == 17);
    assert(run->drawPage == StartupPresentationPage::cache);
    assert(run->directReset.effects.size() == 37);
    assert(run->directReset.operations.size() > 1 && run->sequenceInitialization.operations.size() > 1);
    std::uint64_t resetDispatches = 0;
    for (const auto& operation : run->directReset.operations) { resetDispatches += operation.count; }
    assert(resetDispatches == 46);
    std::size_t noHandle = 0;
    std::size_t publications = 0;
    for (const auto& work : run->work) {
        assert(work.audio.writes.empty());
        for (const auto& effect : work.timing.soundBlasterEffects) {
            assert(effect.kind == SoundBlasterForegroundKind::sequencePublish);
            ++publications;
        }
        for (const auto& callback : work.timing.callbacks) {
            assert(callback.advance.kind == SoundBlasterCallbackKind::noHandle);
            ++noHandle;
        }
    }
    for (std::size_t index = 0; index < run->palettes.size(); ++index) {
        const auto& palette = run->palettes[index];
        assert(palette.commits.size() == 256 && palette.work.audio.writes.empty());
        for (const auto& callback : palette.work.timing.callbacks) {
            assert(callback.advance.kind == SoundBlasterCallbackKind::noHandle);
            ++noHandle;
        }
    }
    assert(noHandle > 0 && publications == 1);
    assert(run->palette.dac.colors[3] == (60 >> 2) * 60 / 63);
    assert(run->palette.dac.colors[4] == (120 >> 2) * 60 / 63);
    assert(run->palette.dac.colors[5] == (180 >> 2) * 60 / 63);
    for (const auto page : {NativeMenuPage::front, NativeMenuPage::back, NativeMenuPage::cache}) {
        const auto pixels = startup.menuPagePixels(page);
        const auto expected = 9;
        assert(std::all_of(pixels.begin(), pixels.end(), [&](const auto pixel) { return pixel == expected; }));
    }
    for (const auto action : {0, 1, 2, 3}) {
        bool rejected = false;
        try {
            switch (action) {
            case 0: startup.initializePresentation(); break;
            case 1: startup.advance(1); break;
            case 2: startup.requestBegin(); break;
            case 3: startup.releaseAudio(); break;
            }
        } catch (const std::logic_error&) { rejected = true; }
        assert(rejected && startup.elapsedSamples() == before + run->samples.size());
    }
    for (const auto control : {0, 1, 2}) {
        auto invalid = game;
        if (control == 0) { invalid.logoMusicSequence.clear(); }
        if (control == 1) { invalid.logoMusicSequence[2] = 2; }
        if (control == 2) { invalid.presentationMusicSequence.clear(); }
        NativeStartupSequence guarded(invalid, resources);
        guarded.start(NativeStartupExecution::recoveredContext);
        guarded.initializeSettings(StartupCapabilityProfile::noMouseOrJoystick);
        guarded.initializeResources(NativeStartupDeviceProfile::portableOplNoDspProbe);
        const auto samples = guarded.elapsedSamples();
        bool rejected = false;
        try { guarded.initializePresentation(); } catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected && guarded.elapsedSamples() == samples && guarded.recoveredPresentation() == nullptr);
        assert(guarded.phase() == NativeStartupPhase::awaitingPresentationResources);
    }
}

void assertRecoveredLogoWork(GameplayAssets game, const NativeStartupAssets& resources) {
    const CockpitSettings configuration{81, {1, 0, 1, 1, 0, 255}};
    HighscoreTable table({}, {});
    table.setConfiguration(configuration);
    game.cockpitSettings = configuration;
    game.highscoreTable = table;
    game.scoreFont = game.titleFont;
    game.scoreFont.height = 8;
    game.scoreFont.glyphs.assign(96, {3, 8, std::vector<std::uint8_t>(24, 1)});
    // Keep this bounded menu interval before repeat EOF, which remains rejected.
    game.presentationMusicSequence.at(game.presentationMusicSequence.size() - 2) = 127;
    game.logoMusicSequence.resize(36);
    game.logoMusicSequence[5] = 7; // Leave source direct channel13 unowned, as on the actual logo route.
    game.logoMusicSequence.insert(game.logoMusicSequence.end(), {0xc1, 0, 127, 0xfc});
    game.soundBlasterInstrumentResource[19 + 12] = 1; // FM patch, same admitted operator branch as actual logo.
    auto animationResources = resources;
    animationResources.logoFrames.assign(8, {4, 1, {1, 0, 2, 3}});
    const auto initialize = [&](NativeStartupSequence& startup) {
        startup.start(NativeStartupExecution::recoveredContext);
        startup.initializeSettings(StartupCapabilityProfile::noMouseOrJoystick);
        startup.initializeResources(NativeStartupDeviceProfile::portableOplNoDspProbe);
        startup.initializePresentation();
        startup.initializeFonts();
    };
    NativeStartupSequence joined(game, animationResources);
    NativeStartupSequence split(game, animationResources);
    initialize(joined);
    initialize(split);
    assert(joined.phase() == NativeStartupPhase::logoCountdownWork);
    assert(joined.menuPageSegment(NativeMenuPage::front) == 0xa000);
    assert(joined.menuPageSegment(NativeMenuPage::back) == 0xa800);
    assert(joined.menuPageSegment(NativeMenuPage::cache) == 0xa400);
    assert(joined.recoveredFonts()->firstFont.glyphs.size() == game.titleFont.glyphs.size());
    assert(joined.recoveredFonts()->secondFont.glyphs.size() == game.scoreFont.glyphs.size());
    assert(joined.recoveredFonts()->firstFont.height == 6 && joined.recoveredFonts()->secondFont.height == 8);
    assert(joined.recoveredFonts()->memoryProbeBytes == 0x2e400 && joined.recoveredFonts()->probeReleased);
    std::uint64_t fontDispatches = 0;
    for (const auto& work : joined.recoveredFonts()->work) { fontDispatches += work.timing.total.dispatches; }
    assert(fontDispatches == 45);
    const auto before = joined.elapsedSamples();
    std::vector<std::int16_t> joinedPcm, splitPcm;
    std::vector<std::tuple<std::int64_t, std::uint8_t, std::uint8_t>> joinedWrites, splitWrites;
    std::int64_t joinedCycles = 0, splitCycles = 0;
    SoundBlasterClockPhase joinedPhase{}, splitPhase{};
    const auto append = [](const auto& work, auto& pcm, auto& writes, auto& cycles) {
        pcm.insert(pcm.end(), work.audio.samples.begin(), work.audio.samples.end());
        for (const auto& write : work.audio.writes) {
            writes.emplace_back(cycles + write.cycleOffset, write.write.registerNumber, write.write.value);
        }
        cycles += work.timing.total.elapsedCycles;
    };
    for (unsigned packet = 0; packet < 2; ++packet) {
        const auto work = joined.pollLogoCountdown(10000);
        assert(work.timing.countdownPolls == 10000 && !work.timing.countdownReturned);
        append(work, joinedPcm, joinedWrites, joinedCycles);
        joinedPhase = work.timing.phase;
    }
    for (unsigned packet = 0; packet < 20; ++packet) {
        const auto work = split.pollLogoCountdown(1000);
        append(work, splitPcm, splitWrites, splitCycles);
        splitPhase = work.timing.phase;
    }
    assert(joinedPcm == splitPcm && joinedWrites == splitWrites && joinedCycles == splitCycles);
    assert(!joinedWrites.empty() && joined.elapsedSamples() == split.elapsedSamples());
    assert(joined.elapsedSamples() == before + joinedPcm.size());
    assert(joinedPhase.remainingCycles == splitPhase.remainingCycles && joinedPhase.deferredCycles == splitPhase.deferredCycles);
    assert(joinedPhase.timer->globalCount == splitPhase.timer->globalCount);
    assert(joinedPhase.timer->callbackCount == splitPhase.timer->callbackCount);
    assert(joinedPhase.devices->irq0Requested == splitPhase.devices->irq0Requested);
    assert(joinedPhase.devices->irq0InService == splitPhase.devices->irq0InService);
    assert(joinedPhase.interrupts == splitPhase.interrupts && joinedPhase.irqPending == splitPhase.irqPending);
    assert(joinedPhase.irqCheckRequested == splitPhase.irqCheckRequested);
    assert(joinedPhase.irq0ActivationPossible == splitPhase.irq0ActivationPossible);
    assert(joinedPhase.linesDone == splitPhase.linesDone && joinedPhase.linesTotal == splitPhase.linesTotal);
    assert(joinedPhase.devices->pitStartMilliseconds == splitPhase.devices->pitStartMilliseconds);
    assert(joinedPhase.devices->pitNowMilliseconds == splitPhase.devices->pitNowMilliseconds);
    assert(joinedPhase.devices->pitCycleBase == splitPhase.devices->pitCycleBase);
    assert(joinedPhase.devices->frameStartMilliseconds == splitPhase.devices->frameStartMilliseconds);
    assert(joinedPhase.devices->realStart == splitPhase.devices->realStart);
    assert(joinedPhase.timer->biosCount == splitPhase.timer->biosCount);
    assert(joinedPhase.events.size() == splitPhase.events.size());
    for (std::size_t index = 0; index < joinedPhase.events.size(); ++index) {
        assert(joinedPhase.events[index].kind == splitPhase.events[index].kind);
        assert(joinedPhase.events[index].deadlineMilliseconds == splitPhase.events[index].deadlineMilliseconds);
        assert(joinedPhase.events[index].value == splitPhase.events[index].value);
    }
    assert(joinedPhase.callback->position == 39 && joinedPhase.callback->runningStatus == 0xc1);
    assert(joinedPhase.callback->program->channelPrograms[1] == 0);
    assert(joinedPhase.callback->program->operators == splitPhase.callback->program->operators);
    assert(joinedPhase.callback->wait == splitPhase.callback->wait);
    assert(joinedPhase.callback->voices.tuning == splitPhase.callback->voices.tuning);
    assert(joinedPhase.callback->channelEnabled == splitPhase.callback->channelEnabled);
    assert(joinedPhase.callback->program->channelPrograms == splitPhase.callback->program->channelPrograms);
    auto directWait = *joinedPhase.callback;
    directWait.wait = 2;
    directWait.voices.directTimers = {1, 0, 0};
    directWait.voices.notes[6] = 80;
    directWait.sounding[6] = 1;
    directWait.velocities[0] = 0xff;
    directWait.latch = SoundBlasterLevelLatch::Latched;
    const auto expired = soundBlasterCallbackAdvance(directWait);
    assert(expired.kind == SoundBlasterCallbackKind::programWait);
    assert(expired.state.wait == 1 && expired.state.voices.directTimers[0] == 0);
    assert(expired.state.sounding[6] == 0 && expired.state.voices.notes[6] == 80);
    assert(expired.state.position == directWait.position);
    assert(expired.state.velocities[0] == 0xff && expired.state.latch == SoundBlasterLevelLatch::Latched);
    std::vector<std::pair<std::uint8_t, std::uint8_t>> expiredWrites;
    for (const auto& block : expired.blocks) {
        if (block.kind == SoundBlasterWorkKind::registerWrite) {
            expiredWrites.emplace_back(block.registerNumber, block.value);
        }
    }
    assert(expiredWrites.size() == 3);
    assert(expiredWrites[0].first == 0xa6 && expiredWrites[1].first == 0xb6);
    assert((expiredWrites[1].second & 0x20) == 0);
    assert((expiredWrites[2] == std::make_pair<std::uint8_t, std::uint8_t>(0x53, 0x40)));
    auto callbackPhase = joinedPhase;
    callbackPhase.interrupts = AudioClockInterrupts::masked;
    callbackPhase.interruptDelivery = AudioClockInterruptDelivery::blockedByIrq0;
    const auto expirationTiming = soundBlasterCallbackTiming(callbackPhase, expired.blocks, expired.kind);
    assert(expirationTiming.total.dispatches == expired.dispatches);
    assert(expirationTiming.oplEffects.size() == 3 && expirationTiming.interruptBoundaries.empty());
    const auto quietWait = soundBlasterCallbackAdvance(expired.state);
    assert(quietWait.kind == SoundBlasterCallbackKind::programWait && quietWait.dispatches == 73);
    assert(quietWait.state.wait == 0 && quietWait.blocks.size() == 1);
    assert(quietWait.state.sounding[6] == 0);
    auto inconsistentLatch = directWait;
    inconsistentLatch.latch = SoundBlasterLevelLatch::Unlatched;
    bool rejectedLatch = false;
    try { soundBlasterCallbackAdvance(inconsistentLatch); }
    catch (const std::invalid_argument&) { rejectedLatch = true; }
    assert(rejectedLatch);
    auto note = *joinedPhase.callback;
    note.wait = 0;
    note.voices.tuning.fill(0);
    note.channelEnabled[1] = 1;
    note.sequence.resize(note.position);
    note.sequence.insert(note.sequence.end(), {0x91, 60, 64, 1});
    const auto admitted = soundBlasterCallbackAdvance(note);
    assert(admitted.kind == SoundBlasterCallbackKind::programControl);
    assert(admitted.state.voices.notes[0] == 60 && admitted.state.sounding[0] == 1);
    assert(admitted.state.velocities[0] == 64 && admitted.state.program->voices[0].program == 0xff);
    for (const std::size_t messages : {37U,64U,65U}) {
        auto burst = note;
        burst.sequence.resize(burst.position);
        for (std::size_t index = 0; index < messages; ++index) {
            burst.sequence.insert(burst.sequence.end(), {0xb1,7,0,
                static_cast<std::uint8_t>(index + 1 == messages ? 1 : 0)});
        }
        bool rejected = false;
        try {
            const auto group = soundBlasterCallbackAdvance(burst);
            assert(group.state.position == burst.position + messages * 4 && group.state.wait == 0);
            assert(group.state.runningStatus == 0xb1 && group.state.velocities == burst.velocities);
            assert(messages <= 64);
        } catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected == (messages > 64));
    }
    auto disabled = note;
    disabled.channelEnabled[1] = 0;
    const auto disabledAdvance = soundBlasterCallbackAdvance(disabled);
    assert(disabledAdvance.state.velocities[0] == 0 && disabledAdvance.state.sounding[0] == 1);
    assert(admitted.dispatches - disabledAdvance.dispatches == 156);
    auto muted = note;
    muted.program->handleVolume = 0;
    const auto mutedAdvance = soundBlasterCallbackAdvance(muted);
    assert(mutedAdvance.state.voices.notes[0] == 60 && mutedAdvance.state.sounding[0] == 1);
    assert(std::count_if(mutedAdvance.blocks.begin(), mutedAdvance.blocks.end(), [](const auto& block) {
        return block.kind == SoundBlasterWorkKind::registerWrite;
    }) == 1);
    auto unsupported = admitted.state;
    unsupported.sequence.insert(unsupported.sequence.end(), {0xb1, 0x4b, 0, 1});
    bool rejectedController = false;
    try { soundBlasterCallbackAdvance(unsupported); }
    catch (const std::invalid_argument&) { rejectedController = true; }
    assert(rejectedController);
    for (const auto polls : {0, 10001}) {
        bool rejected = false;
        try { joined.pollLogoCountdown(polls); } catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected && joined.elapsedSamples() == before + joinedPcm.size());
    }
    while (joined.phase() == NativeStartupPhase::logoCountdownWork) { joined.pollLogoCountdown(10000); }
    const auto front = joined.menuPagePixels(NativeMenuPage::front);
    const auto back = joined.menuPagePixels(NativeMenuPage::back);
    const auto cache = joined.menuPagePixels(NativeMenuPage::cache);
    const auto firstEventSample = joined.elapsedSamples();
    const auto event = joined.playLogoEvent();
    assert(joined.phase() == NativeStartupPhase::awaitingLogoAnimation);
    assert(event.audio.firstSample == firstEventSample);
    assert(joined.elapsedSamples() == firstEventSample + event.audio.samples.size());
    // Synthetic voice6 is untuned; actual logo's FF tuning path adds13 source instructions.
    assert(event.timing.total.dispatches == 587 && event.timing.callbacks.empty());
    assert(event.timing.phase.sequenceSelection->logoEventPlayed);
    const auto& eventState = *event.timing.phase.callback;
    assert(eventState.voices.directTimers[0] == 30 && eventState.voices.notes[6] == 80);
    assert(eventState.sounding[6] == 1 && eventState.velocities[0] == 0xff);
    assert(eventState.latch == SoundBlasterLevelLatch::Latched && event.audio.writes.size() == 3);
    assert(event.audio.writes[0].write.registerNumber == 0xa6 && event.audio.writes[0].write.value == 0x20);
    assert(event.audio.writes[1].write.registerNumber == 0xb6 && event.audio.writes[1].write.value == 0x36);
    assert(event.audio.writes[2].write.registerNumber == 0x53 && event.audio.writes[2].write.value == 0x40);
    assert(joined.menuPagePixels(NativeMenuPage::front) == front);
    assert(joined.menuPagePixels(NativeMenuPage::back) == back);
    assert(joined.menuPagePixels(NativeMenuPage::cache) == cache);
    bool repeated = false;
    try { joined.playLogoEvent(); } catch (const std::logic_error&) { repeated = true; }
    assert(repeated && joined.elapsedSamples() == firstEventSample + event.audio.samples.size());
    for (const auto packet : {0, 10001}) {
        bool refused = false;
        try { joined.animateLogo(packet); } catch (const std::invalid_argument&) { refused = true; }
        assert(refused && joined.recoveredLogo() == nullptr);
    }
    joined.animateLogo();
    const auto& logo = *joined.recoveredLogo();
    assert(joined.phase() == NativeStartupPhase::awaitingLogoSoundEnd && joined.logoSteps() == 17);
    assert(logo.frames.size() == 17 && logo.finalPhase.sequenceSelection->logoFramesCompleted == 17);
    assert(!logo.finalPhase.sequenceSelection->logoFrameActive && logo.finalPhase.timer->globalCount == 0);
    assert(logo.frames[0].index == 0 && logo.frames[1].index == 1 && logo.frames[16].index == -1);
    assert(logo.frames[0].pages[0] == logo.frames[16].pages[0]);
    assert(logo.frames[1].pages[0][63 * 320 + 236] == 1);
    assert(logo.frames[1].pages[0][63 * 320 + 237] == 9);
    assert(logo.frames[1].pages[0][63 * 320 + 238] == 2);
    for (std::size_t index = 0; index < logo.frames.size(); ++index) {
        assert(logo.frames[index].pages[1] == back);
        if (index != 0) { assert(logo.frames[index].sampleEnd > logo.frames[index - 1].sampleEnd); }
    }
    assert(logo.finalPhase.callback->voices.directTimers[0] == 0);
    assert(joined.elapsedSamples() == logo.firstSample + logo.samples.size());
    std::vector<std::int16_t> logoEndStream;
    auto logoEndSample = joined.elapsedSamples();
    joined.finishLogoSound(10000,[&](const NativeSessionDelivery& delivery) {
        assert(delivery.firstSample == logoEndSample && delivery.presentations.empty());
        logoEndSample += delivery.samples.size();
        logoEndStream.insert(logoEndStream.end(),delivery.samples.begin(),delivery.samples.end());
    });
    const auto& end = *joined.recoveredLogoEnd();
    assert(logoEndStream == end.samples && logoEndSample == joined.elapsedSamples());
    assert(joined.phase() == NativeStartupPhase::awaitingLogoRelease);
    assert(end.finalPhase.sequenceSelection->logoSoundReturned);
    assert(end.finalPhase.callback->program->repeatCount == 1 && end.finalPhase.callback->program->handleStatus == 0xffff);
    assert(!end.finalPhase.callback->active && end.finalPhase.callback->wait == 0);
    assert(std::all_of(end.finalPhase.callback->sounding.begin(), end.finalPhase.callback->sounding.end(),
        [](const auto value) { return value == 0; }));
    assert(joined.menuPagePixels(NativeMenuPage::front) == logo.frames.back().pages[0]);
    assert(joined.elapsedSamples() == end.firstSample + end.samples.size());
    const auto ended = soundBlasterCallbackAdvance(*end.finalPhase.callback);
    assert(ended.kind == SoundBlasterCallbackKind::programWait && ended.dispatches == 24);
    assert(ended.blocks.size() == 1 && ended.state.position == end.finalPhase.callback->position);
    auto repeatingEof = *end.finalPhase.callback;
    repeatingEof.active = true;
    repeatingEof.program->handleStatus = 0;
    repeatingEof.program->repeatCount = 0xffff;
    repeatingEof.position = repeatingEof.sequence.size() - 1;
    bool rejectedRestart = false;
    try { soundBlasterCallbackAdvance(repeatingEof); } catch (const std::invalid_argument&) { rejectedRestart = true; }
    assert(rejectedRestart);
    auto sampledEnd = end.finalPhase;
    sampledEnd.sequenceSelection->logoSoundReturned = false;
    sampledEnd.interrupts = AudioClockInterrupts::masked;
    const auto observedEnd = normalWorkTiming(sampledEnd, {{NormalWorkKind::startupLogoEndPoll, 1}}, 1000);
    assert(observedEnd.sequenceEndPolls == 1 && observedEnd.sequenceEndLastRead == 0xffff);
    assert(observedEnd.sequenceEndReturned && observedEnd.total.dispatches == 7);
    bool repeatedEnd = false;
    try { joined.finishLogoSound(); } catch (const std::logic_error&) { repeatedEnd = true; }
    assert(repeatedEnd && joined.elapsedSamples() == end.firstSample + end.samples.size());
    for (int control = 0; control < 4; ++control) {
        auto invalid = end.finalPhase;
        switch (control) {
        case 0: invalid.sequenceSelection->logoSoundReturned = false; break;
        case 1: invalid.callback->voices.directTimers[0] = 1; break;
        case 2: invalid.callback->program->repeatCount = 0xffff; break;
        default: invalid.callback->latch = SoundBlasterLevelLatch::Unlatched; break;
        }
        bool refused = false;
        try { normalWorkTiming(invalid, {{NormalWorkKind::startupLogoStop, 1}}, 120000); }
        catch (const std::invalid_argument&) { refused = true; }
        assert(refused);
    }
    joined.releaseLogo();
    const auto& release = *joined.recoveredLogoRelease();
    assert(joined.phase() == NativeStartupPhase::awaitingIntroInput && release.metadataReleased);
    assert(release.firstSample == end.firstSample + end.samples.size());
    assert(joined.elapsedSamples() == release.firstSample + release.samples.size());
    assert(release.finalPhase.sequenceSelection->logoCurrentCleared);
    const auto& noHandle = *release.finalPhase.callback;
    assert(noHandle.domain == SoundBlasterCallbackDomain::noHandle && !noHandle.program);
    assert(noHandle.velocities == end.finalPhase.callback->velocities);
    assert(noHandle.voices.notes == end.finalPhase.callback->voices.notes);
    assert(noHandle.latch == SoundBlasterLevelLatch::Latched && noHandle.velocities[0] == 0xff);
    assert(soundBlasterCallbackAdvance(noHandle).dispatches == 58);
    assert(joined.menuPagePixels(NativeMenuPage::front) == logo.frames.back().pages[0]);
    assert(joined.menuPagePixels(NativeMenuPage::back) == back);
    assert(joined.menuPagePixels(NativeMenuPage::cache) == logo.frames.back().pages[2]);
    std::size_t stopWrites = 0;
    for (const auto& work : release.work) { stopWrites += work.audio.writes.size(); }
    assert(stopWrites == 27);
    auto next = release.finalPhase;
    const auto callback = normalWorkTiming(next, {{NormalWorkKind::instructions, 100000}}, 120000);
    assert(!callback.callbacks.empty());
    assert(std::all_of(callback.callbacks.begin(), callback.callbacks.end(), [](const auto& receipt) {
        return receipt.advance.kind == SoundBlasterCallbackKind::noHandle && receipt.advance.dispatches == 58;
    }));
    bool repeatedRelease = false;
    try { joined.releaseLogo(); } catch (const std::logic_error&) { repeatedRelease = true; }
    assert(repeatedRelease && joined.elapsedSamples() == release.firstSample + release.samples.size());
    bool noKey = false;
    try { joined.skipIntro(NativeIntroKey::none); } catch (const std::invalid_argument&) { noKey = true; }
    assert(noKey && joined.elapsedSamples() == release.firstSample + release.samples.size());
    joined.skipIntro(NativeIntroKey::escape);
    const auto& intro = *joined.recoveredIntro();
    assert(joined.phase() == NativeStartupPhase::awaitingMenuMusic);
    assert(intro.firstSample == release.firstSample + release.samples.size());
    assert(joined.elapsedSamples() == intro.firstSample + intro.samples.size());
    assert(intro.finalPhase.callback->domain == SoundBlasterCallbackDomain::noHandle);
    assert(intro.finalPhase.callback->velocities == release.finalPhase.callback->velocities);
    assert(intro.blackout.state.palette.source == joined.recoveredPresentation()->palette.source);
    assert(intro.palette.dac.colors == intro.palette.source && intro.palette.output == intro.palette.source);
    for (std::size_t index = 0; index < resources.menuScreen.palette.size(); ++index) {
        assert(intro.palette.source[index * 3] == resources.menuScreen.palette[index].red >> 2);
        assert(intro.palette.source[index * 3 + 1] == resources.menuScreen.palette[index].green >> 2);
        assert(intro.palette.source[index * 3 + 2] == resources.menuScreen.palette[index].blue >> 2);
    }
    assert(joined.menuPagePixels(NativeMenuPage::front) == std::vector<std::uint8_t>(64000));
    assert(joined.menuPagePixels(NativeMenuPage::back) == back);
    assert(joined.menuPagePixels(NativeMenuPage::cache) == logo.frames.back().pages[2]);
    std::size_t noHandleCallbacks = 0;
    for (const auto& step : intro.blackout.steps) {
        for (const auto* work : {&step.before, &step.palette.work}) {
            assert(work->audio.writes.empty());
            noHandleCallbacks += work->timing.callbacks.size();
            for (const auto& receipt : work->timing.callbacks) {
                assert(receipt.advance.kind == SoundBlasterCallbackKind::noHandle && receipt.advance.dispatches == 58);
            }
        }
    }
    assert(noHandleCallbacks != 0);
    bool repeatedInput = false;
    try { joined.skipIntro(NativeIntroKey::escape); } catch (const std::logic_error&) { repeatedInput = true; }
    assert(repeatedInput && joined.elapsedSamples() == intro.firstSample + intro.samples.size());
    joined.beginMenuSong();
    const auto& song = *joined.recoveredMenuSong();
    assert(joined.phase() == NativeStartupPhase::awaitingMenuResources);
    assert(song.firstSample == intro.firstSample + intro.samples.size());
    assert(joined.elapsedSamples() == song.firstSample + song.samples.size());
    assert(song.after.sequenceAllocations[0] && song.after.sequenceAllocations[1]);
    assert(!song.after.sequenceSelections[0] && song.after.sequenceSelections[1]);
    assert(!song.after.sequenceActivity[0] && song.after.sequenceActivity[1]);
    assert(song.before.sequencePositions[0] == song.after.sequencePositions[0]);
    assert(song.finalPhase.sequenceSelection->songOrdinal == 1 && song.finalPhase.sequenceSelection->published);
    assert(song.finalPhase.callback->sequence == game.presentationMusicSequence);
    assert(song.finalPhase.callback->velocities[0] == 0 && song.finalPhase.callback->latch == SoundBlasterLevelLatch::Unlatched);
    assert(song.finalPhase.callback->runningStatus == 0xfc); // Canonical driver A2/A3 survives17E5.
    assert(joined.menuPagePixels(NativeMenuPage::back) == back);
    assert(joined.menuPagePixels(NativeMenuPage::cache) == logo.frames.back().pages[2]);
    bool repeatedSong = false;
    try { joined.beginMenuSong(); } catch (const std::logic_error&) { repeatedSong = true; }
    assert(repeatedSong && joined.elapsedSamples() == song.firstSample + song.samples.size());
    bool missingArrays = false;
    try { joined.initializeMenuResources(); } catch (const std::invalid_argument&) { missingArrays = true; }
    assert(missingArrays && joined.elapsedSamples() == song.firstSample + song.samples.size());
    game.modelColorTables.resize(2);
    game.artGroups[3].assign(5, {4, 1, {1, 2, 3, 4}});
    joined.initializeMenuResources();
    const auto& menuResources = *joined.recoveredMenuResources();
    assert(joined.phase() == NativeStartupPhase::awaitingMenuConstruction);
    assert(menuResources.firstSample == song.firstSample + song.samples.size());
    assert(joined.elapsedSamples() == menuResources.firstSample + menuResources.samples.size());
    assert((menuResources.randomSeed == std::array<std::uint16_t, 2>{0x30, 0x6e}));
    assert(menuResources.streamsClosed && menuResources.configuration.options[0] == 0);
    assert(menuResources.configuration.options[1] == 0 && menuResources.configuration.viewSize == configuration.viewSize);
    assert(menuResources.driverAfter.sequenceAllocations[0] && menuResources.driverAfter.sequenceSelections[1]);
    assert(!menuResources.driverAfter.sequenceSelections[0]);
    assert(menuResources.driverBefore.sequencePositions[0] == menuResources.driverAfter.sequencePositions[0]);
    assert(menuResources.palette.dac.colors == menuResources.palette.source);
    assert(menuResources.palette.output == menuResources.palette.source);
    assert(menuResources.cache.frames.size() == 5 && menuResources.cache.initialCursor == 0);
    assert(menuResources.cache.nextCursor == 5);
    std::uint64_t prefixDispatches = 0;
    std::size_t menuCallbacks = 0;
    for (const auto& slice : menuResources.before) { prefixDispatches += slice.timing.total.dispatches; }
    for (const auto& step : menuResources.blackout.steps) {
        menuCallbacks += step.before.timing.callbacks.size() + step.palette.work.timing.callbacks.size();
    }
    assert(prefixDispatches == 106 && menuCallbacks != 0);
    assert(joined.menuPagePixels(NativeMenuPage::front) == std::vector<std::uint8_t>(64000));
    assert(joined.menuPagePixels(NativeMenuPage::back) == back);
    assert(joined.menuPagePixels(NativeMenuPage::cache) == logo.frames.back().pages[2]);
    bool repeatedResources = false;
    try { joined.initializeMenuResources(); } catch (const std::logic_error&) { repeatedResources = true; }
    assert(repeatedResources && joined.elapsedSamples() == menuResources.firstSample + menuResources.samples.size());
    const auto rejectsCredits = [&](const auto& operations) {
        bool rejected = false;
        try { normalWorkTiming(menuResources.finalPhase,operations,120000); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    };
    rejectsCredits(std::vector<NormalWorkOperation>{{NormalWorkKind::startupCreditsCountdown,1,240}});
    rejectsCredits(std::vector<NormalWorkOperation>{{NormalWorkKind::startupCreditsFirstInput,1,0x011b}});
    rejectsCredits(std::vector<NormalWorkOperation>{{NormalWorkKind::startupCreditsCountdown,1,7200},
        {NormalWorkKind::startupCreditsCountdown,1,7200}});
    rejectsCredits(std::vector<NormalWorkOperation>{{NormalWorkKind::startupCreditsCountdown,1,7200},
        {NormalWorkKind::startupCreditsFirstInput,1,0}});
    const auto creditsInput = normalWorkTiming(menuResources.finalPhase,
        {{NormalWorkKind::startupCreditsCountdown,1,7200},{NormalWorkKind::startupCreditsFirstInput,1,0x011b}},120000);
    assert(creditsInput.creditsCountdownRead && *creditsInput.creditsCountdownRead > 0);
    assert(creditsInput.phase.sequenceSelection->creditsCountdownWritten
        && creditsInput.phase.sequenceSelection->creditsInputReturned);
    rejectsCredits(std::vector<NormalWorkOperation>{{NormalWorkKind::startupMenuEntryCountdown,1,30000}});
    const auto menuEntry = normalWorkTiming(creditsInput.phase,
        {{NormalWorkKind::startupMenuEntryCountdown,1,30000}},120000);
    assert(menuEntry.phase.sequenceSelection->menuEntryCountdownWritten);
    assert(menuEntry.phase.timer->globalCount == 30000);
    bool repeatedMenuCountdown = false;
    try {
        normalWorkTiming(menuEntry.phase,{{NormalWorkKind::startupMenuEntryCountdown,1,30000}},120000);
    } catch (const std::invalid_argument&) { repeatedMenuCountdown = true; }
    assert(repeatedMenuCountdown);
    bool wrongMenuCountdown = false;
    try {
        normalWorkTiming(creditsInput.phase,{{NormalWorkKind::startupMenuEntryCountdown,1,7200}},120000);
    } catch (const std::invalid_argument&) { wrongMenuCountdown = true; }
    assert(wrongMenuCountdown);
    auto beginDecision = menuEntry.phase;
    const auto rejectsBegin = [&](const auto& phase, const NormalWorkOperation operation) {
        bool rejected = false;
        try { normalWorkTiming(phase,{operation},120000); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    };
    rejectsBegin(beginDecision,{NormalWorkKind::startupMenuDecision,1,1});
    rejectsBegin(beginDecision,{NormalWorkKind::startupBeginFadeRequest,1,1});
    for (const std::uint16_t choice : {0,0,1}) {
        const auto decision = normalWorkTiming(beginDecision,{{NormalWorkKind::startupMenuDecision,1,choice}},120000);
        assert(decision.menuDecisionCountdownRead && *decision.menuDecisionCountdownRead > 0);
        beginDecision = decision.phase;
    }
    rejectsBegin(beginDecision,{NormalWorkKind::startupMenuDecision,1,1});
    auto expiredDecision = menuEntry.phase;
    expiredDecision.timer->globalCount = 0;
    rejectsBegin(expiredDecision,{NormalWorkKind::startupMenuDecision,1,0});
    const auto reentry=normalWorkTiming(expiredDecision,
        {{NormalWorkKind::startupMenuReentryCountdown,1,30000}},120000);
    assert(reentry.phase.timer->globalCount==30000);
    assert(reentry.phase.sequenceSelection->menuEntryCountdownWritten);
    rejectsBegin(expiredDecision,{NormalWorkKind::startupMenuReentryCountdown,1,7200});
{
    auto selectedEof=beginDecision;
    auto& callback=*selectedEof.callback;
    //This test's startup resources are synthetic; install a source-valid loop body
    //only in this controlled phase copy. Runtime continuity is checked separately.
    callback.sequence.resize(36);
    callback.sequence.insert(callback.sequence.end(),{0xcf,0x7f,0,0xc1,0,1,0xfc});
    selectedEof.sequenceSelection->prepared.sequence=callback.sequence;
    callback.program->loopMarker=34;
    callback.position=callback.sequence.size()-1;
    callback.wait=0;callback.fade=0;
    callback=soundBlasterMenuCallbackAdvance(callback).state; //Actual1D08 kernel.
    assert(retainedMenuProgram(selectedEof)&&retainedMenuEof(callback));
    const auto before=callback;
    const auto faded=normalWorkTiming(selectedEof,{{NormalWorkKind::startupBeginFadeRequest,1,1}},120000);
    assert(faded.phase.sequenceSelection->beginFadeRequests==1);
    assert(std::count_if(faded.soundBlasterEffects.begin(),faded.soundBlasterEffects.end(),[](const auto& effect) {
        return effect.kind==SoundBlasterForegroundKind::fadeRequest&&effect.value==0x0c;
    })==1);
    if(faded.callbacks.empty()) {
        //2D60 stores AC only; no early reactivation, status/cursor or AA write.
        assert(faded.phase.callback->fade==before.program->handleVolume);
        assert(!faded.phase.callback->active&&faded.phase.callback->program->handleStatus==0xffff);
        assert(faded.phase.callback->position==before.position&&faded.phase.callback->wait==before.wait);
        assert(faded.phase.callback->program->fadeDivider==before.program->fadeDivider);
        assert(faded.phase.callback->voices.directTimers==before.voices.directTimers);
    }
    auto wrong=selectedEof;wrong.callback->program->repeatCount=1;
    rejectsBegin(wrong,{NormalWorkKind::startupBeginFadeRequest,1,1});
    wrong=selectedEof;wrong.callback->program->repeatCount=0;
    rejectsBegin(wrong,{NormalWorkKind::startupBeginFadeRequest,1,1});
    wrong=selectedEof;wrong.sequenceSelection->prepared.sequence.push_back(0);
    rejectsBegin(wrong,{NormalWorkKind::startupBeginFadeRequest,1,1});
    wrong=selectedEof;wrong.callback->runningStatus=0;
    rejectsBegin(wrong,{NormalWorkKind::startupBeginFadeRequest,1,1});
    wrong=selectedEof;wrong.sequenceSelection->beginCurrentCleared=true;
    rejectsBegin(wrong,{NormalWorkKind::startupBeginFadeRequest,1,1});
    wrong=selectedEof;wrong.callback->program->handleVolume=0;
    rejectsBegin(wrong,{NormalWorkKind::startupBeginFadeRequest,1,1});
}

    auto beginFade = normalWorkTiming(beginDecision,{{NormalWorkKind::startupBeginFadeRequest,1,1}},120000).phase;
    assert(beginFade.callback->fade == beginFade.callback->program->handleVolume);
    rejectsBegin(beginFade,{NormalWorkKind::startupBeginFadeRequest,1,1});
    beginFade = normalWorkTiming(beginFade,{{NormalWorkKind::startupBeginFadeRequest,1,2}},120000).phase;
    rejectsBegin(beginFade,{NormalWorkKind::startupBeginCountdown,1,240});
    const auto beginCountdown = normalWorkTiming(beginFade,{{NormalWorkKind::startupBeginCountdown,1,480}},120000);
    assert(beginCountdown.phase.timer->globalCount == 480);
    rejectsBegin(beginCountdown.phase,{NormalWorkKind::startupBeginCountdown,1,480});
    rejectsBegin(beginCountdown.phase,{NormalWorkKind::startupBeginStop,1});
    auto completedBegin = beginCountdown.phase;
    completedBegin.timer->globalCount = 0;
    const auto beginReturn = normalWorkTiming(completedBegin,{{NormalWorkKind::startupBeginWait,1}},120000);
    assert(beginReturn.countdownReturned && beginReturn.countdownPolls == 1);
    assert(beginReturn.phase.sequenceSelection->beginCountdownReturned);
    rejectsBegin(beginReturn.phase,{NormalWorkKind::startupBeginWait,1});
    SoundBlasterCallbackState deselected{};
    deselected.domain = SoundBlasterCallbackDomain::noHandle;
    deselected.active = false;
    deselected.operation = 0x1e;
    deselected.fade = 15;
    const auto ignoredFade = soundBlasterCallbackAdvance(deselected);
    assert(ignoredFade.state.fade == 15 && ignoredFade.kind == SoundBlasterCallbackKind::noHandle);
    deselected.fade = 16;
    bool unsupportedNoHandleFade = false;
    try { soundBlasterCallbackAdvance(deselected); }
    catch (const std::invalid_argument&) { unsupportedNoHandleFade = true; }
    assert(unsupportedNoHandleFade);
    bool noCreditsKey = false;
    try { joined.skipCredits(NativeIntroKey::none); }
    catch (const std::invalid_argument&) { noCreditsKey = true; }
    assert(noCreditsKey && joined.elapsedSamples() == menuResources.firstSample + menuResources.samples.size());
    bool sampleFallback = false;
    try { joined.advance(1); } catch (const std::logic_error&) { sampleFallback = true; }
    assert(sampleFallback);
}

void assertRecoveredLevel1Music() {
    auto game = gameAssets();
    auto resources = startupAssets();
    const CockpitSettings configuration{81,{1,0,1,1,0,255}};
    HighscoreTable table({},{});
    table.setConfiguration(configuration);
    game.cockpitSettings = configuration;
    game.highscoreTable = table;
    game.scoreFont = {32,8,0,std::vector<IndexedBitmap>(96,{4,8,std::vector<std::uint8_t>(32,1)})};
    for (auto& bitmap : game.artGroups[0]) {
        if (bitmap.width == 1) { bitmap = {4,1,std::vector<std::uint8_t>(4,5)}; }
    }
    game.artGroups[0][14] = {24,22,std::vector<std::uint8_t>(24 * 22,5)};
    game.artGroups[0][16] = {120,43,std::vector<std::uint8_t>(120 * 43,3)};
    game.artGroups[0][17] = {120,33,std::vector<std::uint8_t>(120 * 33,8)};
    game.artGroups[0][18] = {120,25,std::vector<std::uint8_t>(120 * 25,8)};
    game.modelColorTables.resize(2);
    game.artGroups[3].assign(5,{4,1,{1,2,3,4}});
    game.logoMusicSequence.resize(36);
    game.logoMusicSequence[5] = 7;
    game.logoMusicSequence.insert(game.logoMusicSequence.end(),{0xc1,0,127,0xfc});
    game.soundBlasterInstrumentResource[19 + 12] = 1;
    game.presentationMusicSequence.resize(36);
    for (std::size_t group = 0; group < 8; ++group) {
        game.presentationMusicSequence.insert(game.presentationMusicSequence.end(),{0xc1,0,127});
    }
    game.presentationMusicSequence.push_back(0xfc);
    resources.logoFrames.assign(8,{4,1,{1,0,2,3}});
    auto stopped = SoundBlasterCallbackState{};
    stopped.domain = SoundBlasterCallbackDomain::noHandle;
    stopped.active = false;
    stopped.fade = 9;
    const auto initialized = applyDriverSequenceCallbackState(stopped,
        soundBlasterSequenceOperations(game.levelMusicSequence,DriverSequenceAdmission::inheritedChannels));
    assert(initialized.fade == 0); //197D clears AC before0325 publishes current.
    NativeStartupSequence startup(game,resources);
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
    std::vector<std::int16_t> streamed;
    std::size_t streamedPresentations = 0;
    auto nextSample = startup.elapsedSamples();
    const auto collect = [&](const NativeSessionDelivery& delivery) {
        assert(delivery.firstSample == nextSample);
        nextSample += delivery.samples.size();
        streamed.insert(streamed.end(),delivery.samples.begin(),delivery.samples.end());
        for (const auto& frame : delivery.presentations) {
            assert(frame.sampleEnd == nextSample);
            ++streamedPresentations;
        }
    };
    bool mixedInputRejected = false;
    try { startup.pollRecoveredMenu({true,false,false},10000,collect); }
    catch (const std::invalid_argument&) { mixedInputRejected = true; }
    assert(mixedInputRejected && streamed.empty() && nextSample == startup.elapsedSamples());
    assert(startup.pollRecoveredMenu({false,false,true},10000,collect) == NativeMenuAction::none);
    assert(streamed.empty());
    nextSample = startup.elapsedSamples();
    assert(startup.pollRecoveredMenu({},10000,collect) == NativeMenuAction::begin);
    assert(nextSample == startup.elapsedSamples());
    assert(streamed == startup.recoveredMenuInput()->samples);
    assert(streamed.size() > 44100);
    assert(streamedPresentations == blackoutPaletteSteps+2);
    auto session = std::make_unique<NativeGameplaySession>(game,startup.releaseGameplay());
    auto& synth = NativeSessionTestAccess::synth(*session);
    auto& clock = NativeSessionTestAccess::clock(*session);
    const auto phase = *session->recoveredClockPhase();
    const auto before = synth.driverState();
    const auto firstSample = clock.elapsedSamples();
    const auto writes = synth.registerWrites().size();
    const auto retainedFront = session->retainedPagePixels(NativePageRole::front);
    const auto retainedBack = session->retainedPagePixels(NativePageRole::back);
    const auto retainedCache = session->retainedPagePixels(NativePageRole::cache);
    auto invalid = phase;
    invalid.sequenceSelection->beginCurrentCleared = false;
    bool earlyRejected = false;
    try { activateLevel1Music(clock,synth,invalid); }
    catch (const std::invalid_argument&) { earlyRejected = true; }
    assert(earlyRejected && clock.elapsedSamples() == firstSample && synth.registerWrites().size() == writes);

    // A controlled5098 call on the retained owner; preceding loader work is not claimed here.
    const auto expected = synth.previewRecoveredLevel1();
    const auto run = activateLevel1Music(clock,synth,phase);
    const auto after = synth.driverState();
    assert(run.firstSample == firstSample && clock.elapsedSamples() == firstSample + run.samples.size());
    assert(after.clockSource == SoundBlasterClockSource::normalWork);
    assert(synth.startupPhase() == SoundBlasterStartupPhase::gameplay);
    assert(after.sequenceAllocations[0] && after.sequenceAllocations[1] && after.sequenceAllocations[2]);
    assert(!after.sequenceSelections[0] && !after.sequenceSelections[1] && after.sequenceSelections[2]);
    for (std::size_t slot = 0; slot < 2; ++slot) {
        assert(after.sequencePositions[slot] == before.sequencePositions[slot]);
        assert(after.sequenceActivity[slot] == before.sequenceActivity[slot]);
    }
    assert(run.phase.sequenceSelection->songOrdinal == 2 && run.phase.sequenceSelection->published);
    assert(run.phase.callback->sequence == game.levelMusicSequence);
    assert(run.phase.callback->program->repeatCount == 0xffff);
    assert(run.phase.callback->runningStatus == expected.runningStatus);
    assert(session->model().ticks() == 0 && session->model().level1State().rng().calls() == 16);
    assert(session->retainedPagePixels(NativePageRole::front) == retainedFront);
    assert(session->retainedPagePixels(NativePageRole::back) == retainedBack);
    assert(session->retainedPagePixels(NativePageRole::cache) == retainedCache);
    assert(run.work.size() == 4);
    assert(run.work.back().timing.phase.sequenceSelection->published);
    const auto sampleEnd = clock.elapsedSamples();
    bool repeated = false;
    try { activateLevel1Music(clock,synth,run.phase); }
    catch (const std::invalid_argument&) { repeated = true; }
    assert(repeated && clock.elapsedSamples() == sampleEnd);
    repeated = false;
    try { synth.prepareRecoveredLevel1(); }
    catch (const std::invalid_argument&) { repeated = true; }
    assert(repeated && clock.elapsedSamples() == sampleEnd);
    const auto reset = resetLevel1DirectEffects(clock,synth,run.phase);
    assert(reset.phase.sequenceSelection->level1DirectResetCompleted);
    std::uint64_t dispatches = 0;
    for (const auto& work : reset.work) { dispatches += work.timing.total.dispatches; }
    assert(dispatches == 87);
    assert(reset.firstSample == sampleEnd);
    assert(clock.elapsedSamples() == sampleEnd + reset.samples.size());
    const auto resetEnd = clock.elapsedSamples();
    repeated = false;
    try { resetLevel1DirectEffects(clock,synth,reset.phase); }
    catch (const std::invalid_argument&) { repeated = true; }
    assert(repeated && clock.elapsedSamples() == resetEnd);
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
    using namespace stellar7;
    const auto game = gameAssets();
    const auto resources = startupAssets();
    assertRecoveredContext(game, resources);
    assertRecoveredSettings(game, resources);
    assertRecoveredResources(game, resources);
    assertRecoveredPresentation(game, resources);
    assertRecoveredLogoWork(game, resources);
    assertRecoveredLevel1Music();
    NativeStartupSequence joined(game, resources);
    NativeStartupSequence split(game, resources);
    joined.start(); split.start();
    joined.acceptSkipInput(); split.acceptSkipInput();
    bool rejected = false;
    try { joined.releaseAudio(); } catch (const std::logic_error&) { rejected = true; }
    assert(rejected);
    std::vector<std::int16_t> whole, parts;
    while (joined.phase() != NativeStartupPhase::menu) {
        const auto chunk = joined.advance(421); whole.insert(whole.end(), chunk.begin(), chunk.end());
        assert(whole.size() < SoundBlasterSynth::sampleRate * 20);
    }
    while (split.phase() != NativeStartupPhase::menu) {
        const auto chunk = split.advance(137); parts.insert(parts.end(), chunk.begin(), chunk.end());
    }
    assert(whole == parts && joined.elapsedSamples() == split.elapsedSamples());
    assert(joined.logoSteps() == 17 && split.logoSteps() == 17);
    assert(joined.frame().pixels[0] == 4);
    assert(joined.frame().pixels[58 * 320 + 102] == 8);
    assert(joined.frame().pixels[69 * 320 + 163] == 5);
    assert(joined.frame().palette[0].red == 12);
    assert(joined.frame().palette[0].green == 20);
    assert(joined.frame().palette[0].blue == 28);
    assert(joined.menuPageSegment(NativeMenuPage::front) == 0xa000);
    assert(joined.menuPageSegment(NativeMenuPage::back) == 0xa400);
    assert(joined.menuPageSegment(NativeMenuPage::cache) == 0xa800);
    assert(joined.countdown() == 30000);
    const auto menuBack = joined.menuPagePixels(NativeMenuPage::back);
    const auto menuCache = joined.menuPagePixels(NativeMenuPage::cache);
    assert(menuBack[69 * 320 + 163] != joined.frame().pixels[69 * 320 + 163]);
    assert(joined.pollMenu({true, false, false}) == NativeMenuAction::none);
    assert(joined.menu().selection() == 4);
    assert(joined.pollMenu({true, false, false}) == NativeMenuAction::none);
    assert(joined.menu().selection() == 4);
    joined.pollMenu({});
    joined.pollMenu({false, true, false});
    assert(joined.menu().selection() == 0);
    joined.pollMenu({});
    assert(joined.menuPagePixels(NativeMenuPage::back) == menuBack);
    assert(joined.menuPagePixels(NativeMenuPage::cache) == menuCache);
    assert(std::any_of(whole.begin(), whole.end(), [](auto c) { return c != 0; }));
    const auto beforeBegin = joined.elapsedSamples();
    assert(joined.pollMenu({false, false, true}) == NativeMenuAction::none);
    assert(joined.phase() == NativeStartupPhase::menu);
    assert(joined.menuPagePixels(NativeMenuPage::back) != menuBack);
    assert(joined.pollMenu({}) == NativeMenuAction::begin);
    assert(joined.elapsedSamples() == beforeBegin);
    assert(joined.menuPagePixels(NativeMenuPage::back) == menuBack);
    assert(joined.menuPagePixels(NativeMenuPage::cache) == menuCache);
    assert(std::all_of(joined.frame().pixels.begin(), joined.frame().pixels.end(), [](auto c) { return c == 0; }));
    assert(joined.countdown() == 480);
    assert((joined.difficultyParameters() == std::array<std::int8_t, 7>{3, 3, 3, 3, 3, 3, 3}));
    while (joined.phase() != NativeStartupPhase::ready) { joined.advance(257); }
    assert(joined.elapsedSamples() > beforeBegin && joined.countdown() == 0);
    split.requestBegin();
    while (split.phase() != NativeStartupPhase::ready) { split.advance(97); }
    auto splitOwner = split.releaseAudio();
    auto owner = joined.releaseAudio();
    assert(snapshot(owner->synth()) == snapshot(splitOwner->synth()));
    const auto& a = owner->synth().registerWrites();
    const auto& b = splitOwner->synth().registerWrites();
    assert(a.size() == b.size());
    for (std::size_t index = 0; index < a.size(); ++index) {
        assert(a[index].registerNumber == b[index].registerNumber && a[index].value == b[index].value);
    }
    const auto firstSample = owner->elapsedSamples();
    const auto state = snapshot(owner->synth());
    const auto writes = owner->synth().registerWrites().size();
    NativeGameplaySession session(game, GameplayModel{}, NativeSessionStart::level1, std::move(owner));
    assert(snapshot(session.synth()) == state && session.synth().registerWrites().size() == writes);
    const auto entry = session.initialize();
    assert(entry.firstSample == firstSample && session.elapsedSamples() > firstSample);
    const auto next = session.advance({});
    assert(next.firstSample >= firstSample && !next.samples.empty());
    rejected = false;
    try { joined.releaseAudio(); } catch (const std::logic_error&) { rejected = true; }
    assert(rejected);
    rejected = false;
    try {
        NativeGameplaySession invalid(game, GameplayModel{}, NativeSessionStart::level1,
            std::make_unique<NativeAudioOwner>(game));
    } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    NativeStartupSequence unownedScene(game, resources);
    unownedScene.start();
    assert(unownedScene.countdown() == 240);
    unownedScene.advance(1);
    assert(unownedScene.countdown() == 239);
    unownedScene.advance(100);
    assert(unownedScene.countdown() == 239);
    rejected = false;
    try {
        while (unownedScene.phase() != NativeStartupPhase::menu) { unownedScene.advance(1024); }
    } catch (const std::logic_error&) { rejected = true; }
    assert(rejected && unownedScene.phase() == NativeStartupPhase::rejected);
    NativeStartupSequence timed(game, resources, LogoCompletion::countdown);
    timed.start(); timed.acceptSkipInput();
    while (timed.phase() != NativeStartupPhase::menu) { timed.advance(1024); }
    assert(timed.logoSteps() == 17 && timed.elapsedSamples() > beforeBegin);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
