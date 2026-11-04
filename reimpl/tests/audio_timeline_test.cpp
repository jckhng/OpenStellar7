#include "stellar7/audio.hpp"
#include "stellar7/normal_clock.hpp"
#include "stellar7/timer_install_work.hpp"
#include "stellar7/timer_registration_work.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {
using namespace stellar7;
constexpr std::uint64_t normalCyclesPerSecond = 5'000'000;

std::vector<std::uint8_t> instrumentResource() {
    constexpr std::size_t headerSize = 19;
    constexpr std::size_t bankHalfSize = 0x540;
    std::vector<std::uint8_t> resource(headerSize + bankHalfSize * 2 + 2, 0);
    resource[0] = 0x89;
    resource[1] = 0x11;
    resource[headerSize + bankHalfSize] = 0xab;
    resource[headerSize + bankHalfSize + 1] = 0xcd;
    auto* patch = resource.data() + headerSize;
    for (const std::size_t op : {0U, 13U}) {
        patch[op + 1] = 1;
        patch[op + 3] = 15;
        patch[op + 4] = 7;
        patch[op + 7] = 7;
    }
    return resource;
}

std::vector<std::uint8_t> music() {
    std::vector<std::uint8_t> sequence(36, 0);
    sequence[0] = 0x84;
    sequence[5] = 1;
    sequence[6] = 0x04;
    sequence.insert(sequence.end(), {0xc1, 0, 0, 0x91, 60, 64, 3, 0xfc});
    return sequence;
}

void start(SoundBlasterSynth& synth) {
    synth.startMusic();
    synth.render(1);
    assert(synth.callbackWorkState().wait == 2);
}

VgaServiceReceipt receipt(const std::int64_t offset, const std::uint32_t line = 0) {
    const auto time = offset / 5000.0;
    return {VgaServiceKind::scanLine, offset, time, time, 0, 0, line, 400};
}

NormalWorkTiming metadataWork(const SoundBlasterSynth& synth, const std::int64_t elapsed,
    const std::vector<std::int64_t>& offsets) {
    NormalWorkTiming work{};
    work.total.elapsedCycles = elapsed;
    work.phase.callback = synth.callbackWorkState();
    for (const auto offset : offsets) {
        work.vgaServices.push_back(receipt(offset, static_cast<std::uint32_t>(work.vgaServices.size())));
    }
    return work;
}

void splitAndJoinedBoundaries() {
    SoundBlasterSynth joined({}, instrumentResource(), music());
    SoundBlasterSynth split({}, instrumentResource(), music());
    start(joined);
    start(split);
    // Synthetic elapsed intervals isolate metadata rounding, not dispatch/IRQ fidelity.
    const auto whole = joined.renderClocked(metadataWork(joined, 50001,
        {0, 1, 2, 49999, 50000, 50001}), 0);
    assert(whole.firstSample == 0 && whole.samples.size() == 442);
    assert((whole.vgaSamplePositions == std::vector<std::uint64_t>{0, 1, 1, 441, 441, 442}));

    const std::array<std::int64_t, 5> intervals{1, 1, 49997, 1, 1};
    const std::array<std::uint64_t, 5> starts{0, 1, 1, 441, 441};
    const std::array<std::uint64_t, 5> ends{1, 1, 441, 441, 442};
    std::uint64_t origin = 0;
    std::vector<std::int16_t> samples;
    for (std::size_t index = 0; index < intervals.size(); ++index) {
        const auto elapsed = intervals[index];
        const auto part = split.renderClocked(metadataWork(split, elapsed, {0, elapsed, elapsed}), origin);
        assert(part.firstSample == starts[index]);
        assert(part.samples.size() == ends[index] - starts[index]);
        assert((part.vgaSamplePositions == std::vector<std::uint64_t>{starts[index], ends[index], ends[index]}));
        assert(part.firstSample == samples.size());
        samples.insert(samples.end(), part.samples.begin(), part.samples.end());
        origin += static_cast<std::uint64_t>(elapsed);
    }
    assert(samples == whole.samples);
    assert(split.driverState().normalClockCycles == joined.driverState().normalClockCycles);
}

auto callbackSnapshot(const SoundBlasterSynth& synth) {
    const auto state = synth.callbackWorkState();
    return std::make_tuple(state.voices.channels, state.voices.notes, state.voices.tuning,
        state.voices.channelDeferrals, state.voices.directTimers, state.velocities, state.sounding,
        state.channelEnabled, state.channelVoiceCursors, state.sequence, state.position, state.wait,
        state.fade, state.mode, state.operation, state.runningStatus, state.active, state.gate, state.latch);
}

auto driverSnapshot(const SoundBlasterSynth& synth) {
    const auto state = synth.driverState();
    return std::make_tuple(state.notes, state.velocities, state.channelVoiceCursors, state.directTimers,
        state.directLevelLatch, state.clockSource, state.normalClockCycles, state.clockPhase,
        state.clockPeriod, state.sequencePosition, state.sequenceWait, state.runningStatus);
}

auto writesSnapshot(const SoundBlasterSynth& synth) {
    std::vector<std::pair<std::uint8_t, std::uint8_t>> result;
    for (const auto& write : synth.registerWrites()) {
        result.emplace_back(write.registerNumber, write.value);
    }
    return result;
}

SoundBlasterClockPhase nestedPhase(const SoundBlasterSynth& synth) {
    SoundBlasterClockPhase phase{};
    phase.remainingCycles = 1;
    phase.deferredCycles = 4999;
    phase.interrupts = AudioClockInterrupts::enabled;
    phase.isa8DelayNanoseconds = 1020;
    phase.dmaDacRate = 44100;
    phase.pitPeriodMilliseconds = 55;
    phase.singleLinePeriodMilliseconds = 0.001;
    phase.horizontalPeriodMilliseconds = 0.001;
    phase.linesTotal = 400;
    phase.linesDone = 400;
    phase.minimumLatchDelayMilliseconds = 0.001;
    phase.frameStart = AudioClockFrameStart::activeUnlockedVgaSurface;
    phase.vgaInterrupt = AudioClockVgaInterrupt::fixedVgaNoDemand;
    phase.devices.emplace();
    phase.devices->fixedPitAndMaskedDma = true;
    phase.devices->picTicks = 100;
    phase.devices->framePeriodMilliseconds = 14;
    phase.devices->displayEndMilliseconds = 12;
    phase.devices->retraceStartMilliseconds = 13;
    phase.devices->retraceEndMilliseconds = 13.1;
    phase.devices->frame = AudioClockVgaFramePhase{1000.0 / 14, 100, 0};
    phase.devices->irq0Requested = true;
    phase.interruptDelivery = AudioClockInterruptDelivery::residentTimerIrq0;
    phase.irq0ActivationPossible = true;
    phase.irqDelayMilliseconds = 0.002;
    phase.timer = AudioClockTimerPhase{1, 0, 5, 1, 4};
    phase.callback = synth.callbackWorkState();
    phase.events = {{0.0002, AudioClockEventKind::irqCheck, 0},
        {0.008, AudioClockEventKind::frameStart, 0}};
    return phase;
}

enum class InvalidOffset { negative, pastEnd, descending };

void rejectedMetadataLeavesLiveSynth(const InvalidOffset control) {
    SoundBlasterSynth rejected({}, instrumentResource(), music());
    SoundBlasterSynth untouched({}, instrumentResource(), music());
    start(rejected);
    start(untouched);
    const auto valid = normalWorkTiming(nestedPhase(rejected), {{NormalWorkKind::instructions, 150}}, 1000);
    assert(valid.callbacks.size() == 1 && valid.phase.callback->wait == 1);
    assert(valid.vgaServices.size() >= 2 && valid.total.elapsedCycles >= 2);
    auto invalid = valid;
    switch (control) {
    case InvalidOffset::negative:
        invalid.vgaServices.front().cycleOffset = -1;
        break;
    case InvalidOffset::pastEnd:
        invalid.vgaServices.back().cycleOffset = valid.total.elapsedCycles + 1;
        break;
    case InvalidOffset::descending:
        invalid.vgaServices = {receipt(2), receipt(1, 1)};
        break;
    }
    const auto beforeCallback = callbackSnapshot(rejected);
    const auto beforeDriver = driverSnapshot(rejected);
    const auto beforeWrites = writesSnapshot(rejected);
    bool refused = false;
    try {
        rejected.renderClocked(invalid, 0);
    } catch (const std::invalid_argument& error) {
        refused = std::string(error.what()).find("invalid VGA order") != std::string::npos;
    }
    assert(refused);
    assert(callbackSnapshot(rejected) == beforeCallback);
    assert(driverSnapshot(rejected) == beforeDriver);
    assert(writesSnapshot(rejected) == beforeWrites);

    const auto retry = rejected.renderClocked(valid, 0);
    const auto reference = untouched.renderClocked(valid, 0);
    assert(retry.firstSample == 0);
    assert(rejected.callbackWorkState().wait == 1 && callbackSnapshot(rejected) != beforeCallback);
    assert(retry.samples == reference.samples && retry.firstSample == reference.firstSample);
    assert(retry.vgaSamplePositions == reference.vgaSamplePositions);
    assert(callbackSnapshot(rejected) == callbackSnapshot(untouched));
    assert(driverSnapshot(rejected) == driverSnapshot(untouched));
    assert(writesSnapshot(rejected) == writesSnapshot(untouched));
    assert(retry.vgaSamplePositions.size() == valid.vgaServices.size());
    for (std::size_t index = 0; index < valid.vgaServices.size(); ++index) {
        const auto cycles = static_cast<std::uint64_t>(valid.vgaServices[index].cycleOffset);
        const auto expected = (cycles * SoundBlasterSynth::sampleRate + normalCyclesPerSecond - 1)
            / normalCyclesPerSecond;
        assert(retry.vgaSamplePositions[index] == expected);
    }
}
}

void foregroundOutPrecedesPendingIrq() {
    // Synthetic delayed-PIC control retains the native installer's device epoch.
    auto phase = normalWorkTiming(nativeTimerInstallPhase(), timerInstallOperations(), 1000).phase;
    phase = normalWorkTiming(phase, {{NormalWorkKind::instructions, 100}}, 1000).phase;
    const auto localPosition = 5000 - phase.remainingCycles - phase.deferredCycles;
    constexpr std::int64_t dataOutCycles = 23 + 6 * (5 + 2) + 2 * 3;
    for (const auto& event : phase.events) {
        assert(event.deadlineMilliseconds * 5000 > localPosition + dataOutCycles);
    }
    phase.soundBlasterInitialization.emplace();
    phase.remainingCycles = dataOutCycles;
    phase.deferredCycles = 5000 - localPosition - dataOutCycles;
    phase.devices->irq0Requested = true;
    phase.irqCheckRequested = true;
    const auto work = normalWorkTiming(phase, {{NormalWorkKind::soundBlasterOplWrite, 1, 0x0100}}, 1000);
    assert(work.soundBlasterEffects.size() == 1 && work.soundBlasterEffects.front().cycleOffset == dataOutCycles);
    assert(work.total.dispatches > soundBlasterOplWork().dispatches && !work.phase.devices->irq0Requested);
    SoundBlasterSynth synth({}, {}, {}, {}, {}, {}, {}, {}, SoundBlasterInitialization::recoveredWork);
    const auto audio = synth.renderClocked(work, 0);
    assert(audio.writes.size() == 1 && audio.writes.front().cycleOffset == dataOutCycles);
    assert(synth.driverState().initializationOplWrites == 1 && !synth.driverState().driverEnabled);
    phase.interrupts = AudioClockInterrupts::masked;
    const auto masked = normalWorkTiming(phase, {{NormalWorkKind::soundBlasterOplWrite, 1, 0x0100}}, 1000);
    assert(masked.soundBlasterEffects.front().cycleOffset == dataOutCycles);
    assert(masked.total.dispatches == soundBlasterOplWork().dispatches && masked.phase.devices->irq0Requested);
}

void publicationPrecedesPendingIrq() {
    auto phase = normalWorkTiming(nativeTimerInstallPhase(), timerInstallOperations(), 1000).phase;
    SoundBlasterCallbackState noHandle{};
    noHandle.domain = SoundBlasterCallbackDomain::noHandle;
    noHandle.operation = 0x1e;
    noHandle.active = false;
    phase.callback = noHandle;
    phase.soundBlasterInitialization = AudioClockSoundBlasterInitializationPhase{158, true};
    phase = normalWorkTiming(phase, timerRegistrationOperations(), 1000).phase;
    phase = normalWorkTiming(phase, {{NormalWorkKind::instructions, 100}}, 1000).phase;
    auto selected = noHandle;
    selected.domain = SoundBlasterCallbackDomain::sequence;
    selected.active = true;
    selected.operation = 2;
    selected.sequence.resize(36);
    selected.wait = 2; // Synthetic wait isolates the publication/IRQ ordering.
    phase.sequenceSelection = AudioClockSequenceSelectionPhase{selected};
    const auto spendAtNextStore = [](auto& incoming) {
        const auto local = 5000 - incoming.remainingCycles - incoming.deferredCycles;
        for (const auto& event : incoming.events) {
            assert(event.deadlineMilliseconds * 5000 > local + 1);
        }
        incoming.remainingCycles = 1;
        incoming.deferredCycles = 5000 - local - 1;
        incoming.devices->irq0Requested = true;
        incoming.irqCheckRequested = true;
    };
    phase.timer->callbackCount = 1;
    spendAtNextStore(phase);
    const auto published = normalWorkTiming(phase, {{NormalWorkKind::soundBlasterSequencePublish, 1}}, 1000);
    assert(published.soundBlasterEffects.size() == 1 && published.soundBlasterEffects[0].cycleOffset == 1);
    assert(published.callbacks.size() == 1 && published.callbacks[0].advance.kind == SoundBlasterCallbackKind::wait);
    assert(published.phase.callback->wait == 1 && published.phase.sequenceSelection->published);
    auto countdown = published.phase;
    countdown.timer->callbackCount = 4;
    spendAtNextStore(countdown);
    const auto counted = normalWorkTiming(countdown, {{NormalWorkKind::timerGlobalCount, 1, 240}}, 1000);
    assert(counted.phase.timer->globalCount == 239 && counted.phase.sequenceSelection->countdownWritten);
    auto readRace = counted.phase;
    readRace.timer->globalCount = 1;
    readRace.timer->callbackCount = 4;
    const auto local = 5000 - readRace.remainingCycles - readRace.deferredCycles;
    readRace.remainingCycles = 2; // CALL and captured MOV finish before the pending IRQ.
    readRace.deferredCycles = 5000 - local - 2;
    readRace.devices->irq0Requested = true;
    readRace.irqCheckRequested = true;
    const auto observedOne = normalWorkTiming(readRace, {{NormalWorkKind::startupCountdownPoll, 1}}, 1000);
    assert(observedOne.countdownLastRead == 1 && observedOne.phase.timer->globalCount == 0);
    assert(!observedOne.countdownReturned);
    const auto observedZero = normalWorkTiming(observedOne.phase, {{NormalWorkKind::startupCountdownPoll, 1}}, 1000);
    assert(observedZero.countdownLastRead == 0 && observedZero.countdownReturned);
    assert(observedZero.total.dispatches == 8); // Five loop instructions and the three-instruction return.
    for (const auto operation : {NormalWorkOperation{NormalWorkKind::soundBlasterSequencePublish, 1},
        NormalWorkOperation{NormalWorkKind::timerGlobalCount, 1, 240}}) {
        bool rejected = false;
        try { normalWorkTiming(counted.phase, {operation}, 1000); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    phase.interrupts = AudioClockInterrupts::masked;
    const auto masked = normalWorkTiming(phase, {{NormalWorkKind::soundBlasterSequencePublish, 1}}, 1000);
    assert(masked.callbacks.empty() && masked.phase.callback->wait == 2);
}

int main() {
    publicationPrecedesPendingIrq();
    foregroundOutPrecedesPendingIrq();
    {
        SoundBlasterSynth noHandle;
        SoundBlasterCallbackState incoming;
        try {
            incoming = noHandle.callbackWorkState();
        } catch (const std::invalid_argument& error) {
            std::fprintf(stderr, "No-handle callback rejected: %s\n", error.what());
            return 1;
        }
        const auto next = soundBlasterCallbackAdvance(incoming);
        assert(next.dispatches == 58 && next.blocks.size() == 1);
        assert(next.blocks.front().kind == SoundBlasterWorkKind::plain);
        assert(next.state.sequence.empty() && next.state.voices.directTimers == incoming.voices.directTimers);
        assert(incoming.domain == SoundBlasterCallbackDomain::noHandle && !incoming.active);
        auto latched = incoming;
        latched.velocities[0] = 0xff;
        latched.latch = SoundBlasterLevelLatch::Latched;
        const auto retained = soundBlasterCallbackAdvance(latched);
        assert(retained.dispatches == 58 && retained.state.velocities[0] == 0xff);
        assert(retained.state.latch == SoundBlasterLevelLatch::Latched);
        latched.latch = SoundBlasterLevelLatch::Unlatched;
        bool invalidAlias = false;
        try { soundBlasterCallbackAdvance(latched); } catch (const std::invalid_argument&) { invalidAlias = true; }
        assert(invalidAlias);
        for (int control = 0; control < 7; ++control) {
            auto invalid = incoming;
            switch (control) {
            case 0: invalid.driverEnabled = false; break;
            case 1: invalid.active = true; break;
            case 2: invalid.sequence = {0xfc}; break;
            case 3: invalid.wait = 1; break;
            default: invalid.voices.directTimers[control - 4] = 1; break;
            }
            bool refused = false;
            try {
                soundBlasterCallbackAdvance(invalid);
            } catch (const std::invalid_argument&) {
                refused = true;
            }
            assert(refused);
        }
    }
    splitAndJoinedBoundaries();
    for (const auto control : {InvalidOffset::negative, InvalidOffset::pastEnd, InvalidOffset::descending}) {
        rejectedMetadataLeavesLiveSynth(control);
    }
}
