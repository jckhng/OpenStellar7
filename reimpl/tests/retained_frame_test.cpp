#include "stellar7/retained_frame.hpp"

#include <array>
#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <vector>

// Synthetic API fixture, not a source-state or pixel-fidelity fixture.
static_assert(!std::is_copy_constructible_v<stellar7::RetainedFrameExecutor>);
static_assert(!std::is_copy_assignable_v<stellar7::RetainedFrameExecutor>);
static_assert(!std::is_move_constructible_v<stellar7::RetainedFrameExecutor>);
static_assert(!std::is_move_assignable_v<stellar7::RetainedFrameExecutor>);

namespace {
using namespace stellar7;
constexpr std::int64_t horizonCycles = 120000;
constexpr std::uint64_t syntheticRenderInstructions = 61;
constexpr std::size_t musicHeaderSize = 36;

std::vector<std::uint8_t> instrumentBank() {
    constexpr std::size_t headerSize = 19;
    constexpr std::size_t bankHalfSize = 0x540;
    std::vector<std::uint8_t> bank(headerSize + bankHalfSize * 2 + 2, 0);
    bank[0] = 0x89;
    bank[1] = 0x11;
    bank[headerSize + bankHalfSize] = 0xab;
    bank[headerSize + bankHalfSize + 1] = 0xcd;
    for (const std::size_t patch : {headerSize, headerSize + 13}) {
        bank[patch + 1] = 1;
        bank[patch + 3] = 15;
        bank[patch + 4] = 7;
        bank[patch + 7] = 7;
    }
    return bank;
}

std::vector<std::uint8_t> music() {
    auto sequence = std::vector<std::uint8_t>(musicHeaderSize, 0);
    sequence[0] = 0x84;
    sequence[5] = 1;
    sequence[6] = 0x24;
    const std::vector<std::uint8_t> notes{0xc1, 0, 0, 0x91, 60, 64, 1, 0x81, 60, 64, 0xfc};
    sequence.insert(sequence.end(), notes.begin(), notes.end());
    return sequence;
}

SoundBlasterClockPhase phase() {
    SoundBlasterClockPhase value{};
    value.remainingCycles = 5000;
    value.interrupts = AudioClockInterrupts::enabled;
    value.isa8DelayNanoseconds = 1020;
    value.dmaDacRate = 44100;
    value.pitPeriodMilliseconds = 55;
    value.irqDelayMilliseconds = 0.002;
    value.singleLinePeriodMilliseconds = 0.03;
    value.horizontalPeriodMilliseconds = 0.03;
    value.linesDone = 400;
    value.linesTotal = 400;
    value.minimumLatchDelayMilliseconds = 0.001;
    value.frameStart = AudioClockFrameStart::activeUnlockedVgaSurface;
    value.interruptDelivery = AudioClockInterruptDelivery::residentTimerIrq0;
    value.vgaInterrupt = AudioClockVgaInterrupt::fixedVgaNoDemand;
    value.devices = AudioClockDevicePhase{};
    value.devices->fixedPitAndMaskedDma = true;
    value.devices->framePeriodMilliseconds = 14;
    value.devices->displayEndMilliseconds = 12;
    value.devices->retraceStartMilliseconds = 13;
    value.devices->retraceEndMilliseconds = 13.1;
    value.devices->memoryMask = 0x1fffff;
    value.devices->frame = AudioClockVgaFramePhase{1000.0 / 14, 0, 0};
    value.timer = AudioClockTimerPhase{1, 0, 5, 20, 20, 5};
    value.bios = AudioClockBiosPhase{};
    value.bios->domain = AudioClockBiosDomain::normalVgaDefaultTimer;
    // No PIT/IRQ event is admitted here; the real live synth stays unchanged.
    value.events = {{14, AudioClockEventKind::frameStart, 0}};
    return value;
}

StationaryLevel2PrefixInput prefixInput() {
    StationaryLevel2PrefixInput input{};
    input.active = true;
    input.player.type = 31;
    input.player.collisionRadius = 740;
    const std::array<std::int16_t, 7> types{13, 8, 8, 13, 30, 30, 8};
    const std::array<FixedPosition, 7> positions{{
        {-22896, 26992, 0}, {12288, 24576, 0}, {24576, -8192, 0},
        {10608, -22896, 0}, {-8192, 20480, 0}, {8192, -20480, 0}, {-20480, 4096, 0}}};
    const std::array<std::uint16_t, 7> yaws{33536, 2145, 51297, 34560, 0, 0, 18529};
    for (std::size_t i = 0; i < types.size(); ++i) {
        Level1Entity actor{};
        actor.type = types[i];
        actor.position = positions[i];
        actor.yaw = yaws[i];
        actor.collisionRadius = actor.type == 13 ? 559 : actor.type == 8 ? 860 : 868;
        actor.stateBytes4dTo52[0] = actor.type == 30 ? 254 : 255;
        actor.stateBytes4dTo52[1] = 1;
        actor.stateBytes4dTo52[4] = actor.type == 13 ? 4 : actor.type == 8 ? 2 : 0;
        if (actor.type == 8) {
            actor.yawVelocity = 143;
            actor.weaponSlotCount = 2;
        }
        input.actors.push_back(actor);
        input.activeOrder.push_back(i);
    }
    input.actors[0].yawVelocity = 2048;
    input.actors[0].xVelocity = 112;
    input.actors[0].yVelocity = -112;
    input.actors[3].yawVelocity = -2048;
    input.actors[3].xVelocity = -112;
    input.actors[3].yVelocity = 112;
    return input;
}

Level2FramePlan syntheticPlan() {
    const std::vector<IndexedBitmap> panels{{120, 56, std::vector<std::uint8_t>(120 * 56, 7)}};
    // Only operations are consumed by this API test: no source frame is planned.
    return {{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, CockpitPageState{}, RadarState(panels),
        IndexedPages{}, {{NormalWorkKind::instructions, syntheticRenderInstructions}}};
}

FrameTailState tailInput(const std::uint16_t loopTick, const StationaryLevel2PrefixInput& prefix) {
    FrameTailState result{loopTick, prefix.player.position, 0, 0, {}, {}};
    for (const auto index : prefix.activeOrder) {
        const auto& actor = prefix.actors.at(index);
        result.actors.push_back({actor.type, actor.position});
    }
    return result;
}

FrameTailGuards tailGuards() {
    FrameTailGuards guards{};
    guards.armor = 680;
    guards.keyboardHead = 0x1e;
    guards.keyboardTail = 0x1e;
    guards.missionActive = true;
    return guards;
}

template<class Function>
void rejectWithoutAdvance(RetainedFrameExecutor& executor, PresentationClock& clock,
    SoundBlasterSynth& synth, Function function) {
    const auto before = executor.phase();
    const auto samples = clock.elapsedSamples();
    const auto driverClock = synth.driverState().clockPhase;
    const auto writes = synth.registerWrites().size();
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    const auto& after = executor.phase();
    assert(before.remainingCycles == after.remainingCycles && before.deferredCycles == after.deferredCycles);
    assert(before.linesDone == after.linesDone && before.interrupts == after.interrupts);
    assert(before.devices->picTicks == after.devices->picTicks);
    assert(before.devices->displayStart == after.devices->displayStart && before.devices->realStart == after.devices->realStart);
    assert(before.events.size() == after.events.size());
    for (std::size_t i = 0; i < before.events.size(); ++i) {
        assert(before.events[i].kind == after.events[i].kind && before.events[i].value == after.events[i].value);
        assert(before.events[i].deadlineMilliseconds == after.events[i].deadlineMilliseconds);
    }
    assert(clock.elapsedSamples() == samples && synth.driverState().clockPhase == driverClock);
    assert(synth.registerWrites().size() == writes);
}
}

int main() {
    using namespace stellar7;
    SoundBlasterSynth synth({}, instrumentBank(), music());
    synth.startMusic();
    PresentationClock clock(synth);
    auto initialPhase = phase();
    RetainedFrameExecutor executor(clock, initialPhase, horizonCycles);
    initialPhase.remainingCycles = 1;
    initialPhase.devices->picTicks = 99;
    initialPhase.events.clear();
    assert(executor.phase().remainingCycles == 5000);
    assert(executor.phase().devices->picTicks == 0 && !executor.phase().events.empty());
    CockpitPageState pages;
    auto prefix = prefixInput();
    auto tail = tailInput(4, prefix);
    const auto guards = tailGuards();
    const auto plan = syntheticPlan();

    rejectWithoutAdvance(executor, clock, synth, [&]() { executor.render(plan); });
    rejectWithoutAdvance(executor, clock, synth, [&]() { executor.prepare(prefix); });
    rejectWithoutAdvance(executor, clock, synth, [&]() { executor.finish(tail, guards); });
    rejectWithoutAdvance(executor, clock, synth, [&]() { executor.postcopy(FrameCopyProfile::modeXNoop); });

    std::size_t emittedSamples = 0;
    for (unsigned cycle = 0; cycle < 2; ++cycle) {
        const auto beforePic = executor.phase().devices->picTicks;
        const auto display = executor.display(pages);
        emittedSamples += display.work.audio.samples.size();
        rejectWithoutAdvance(executor, clock, synth, [&]() { executor.display(pages); });
        if (cycle == 0) {
            rejectWithoutAdvance(executor, clock, synth, [&]() { executor.postcopy(FrameCopyProfile::unknown); });
        }
        const auto copy = executor.postcopy(FrameCopyProfile::modeXNoop);
        emittedSamples += copy.audio.samples.size();
        pages.flip();
        rejectWithoutAdvance(executor, clock, synth, [&]() { executor.postcopy(FrameCopyProfile::modeXNoop); });
        const auto finished = executor.finish(tail, guards);
        emittedSamples += finished.work.audio.samples.size();
        tail = finished.state.state;
        for (std::size_t i = 0; i < prefix.activeOrder.size(); ++i) {
            prefix.actors[prefix.activeOrder[i]].position = tail.actors[i].position;
        }
        const auto prepared = executor.prepare(prefix);
        emittedSamples += prepared.work.audio.samples.size();
        prefix.player = prepared.state.player;
        prefix.actors = prepared.state.actors;
        prefix.activeOrder = prepared.state.activeOrder;
        prefix.lastListCamera = prefix.player.position;
        rejectWithoutAdvance(executor, clock, synth, [&]() { executor.prepare(prefix); });
        const auto rendered = executor.render(plan);
        emittedSamples += rendered.audio.samples.size();
        assert(executor.phase().devices->picTicks > beforePic);
        assert(clock.elapsedSamples() == emittedSamples);
        assert(tail.loopTick == 5 + cycle);
        tail = tailInput(tail.loopTick, prefix);
    }

    // Reach the next render stage using the same live owners, then fail its work.
    executor.display(pages);
    executor.postcopy(FrameCopyProfile::modeXNoop);
    pages.flip();
    executor.finish(tail, guards);
    executor.prepare(prefix);
    auto invalidPlan = plan;
    invalidPlan.operations = {{NormalWorkKind::instructions, horizonCycles + 1}};
    rejectWithoutAdvance(executor, clock, synth, [&]() { executor.render(invalidPlan); });
    // A valid render would succeed if rejection had left the stage reusable.
    rejectWithoutAdvance(executor, clock, synth, [&]() { executor.render(plan); });
    rejectWithoutAdvance(executor, clock, synth, [&]() { executor.display(pages); });
    // Synthetic forward-tail integration: no keyboard IRQ or moving-frame qualification.
    SoundBlasterSynth forwardSynth({}, instrumentBank(), music());
    forwardSynth.startMusic();
    PresentationClock forwardClock(forwardSynth);
    RetainedFrameExecutor forwardExecutor(forwardClock, phase(), horizonCycles);
    CockpitPageState forwardPages;
    const auto forwardDisplay = forwardExecutor.display(forwardPages);
    const auto forwardCopy = forwardExecutor.postcopy(FrameCopyProfile::modeXNoop);
    forwardPages.flip();
    auto forwardInput = prefixInput();
    const auto stationaryTail = tailInput(5, forwardInput);
    auto heldForward = tailGuards();
    constexpr std::size_t forwardKey = 4;
    constexpr std::size_t leftKey = 6;
    heldForward.heldKeys[forwardKey] = true;
    auto mixedInput = heldForward;
    mixedInput.heldKeys[leftKey] = true;
    rejectWithoutAdvance(forwardExecutor, forwardClock, forwardSynth,
        [&]() { forwardExecutor.finish(stationaryTail, mixedInput); });
    const auto beforeForward = forwardClock.elapsedSamples();
    const auto finishedForward = forwardExecutor.finish(stationaryTail, heldForward);
    assert(finishedForward.state.state.forwardVelocity == 10);
    assert(finishedForward.state.state.loopTick == 6);
    assert(beforeForward == forwardDisplay.work.audio.samples.size() + forwardCopy.audio.samples.size());
    assert(forwardClock.elapsedSamples() == beforeForward + finishedForward.work.audio.samples.size());
    const auto completedCycles = forwardDisplay.work.timing.total.elapsedCycles
        + forwardCopy.timing.total.elapsedCycles + finishedForward.work.timing.total.elapsedCycles;
    assert(forwardSynth.driverState().normalClockCycles == static_cast<std::uint64_t>(completedCycles));
    rejectWithoutAdvance(forwardExecutor, forwardClock, forwardSynth,
        [&]() { forwardExecutor.finish(stationaryTail, heldForward); });
    forwardInput.player.forwardVelocity = finishedForward.state.state.forwardVelocity;
    auto rebuildingInput = forwardInput;
    rebuildingInput.lastListCamera.y = -1014;
    rejectWithoutAdvance(forwardExecutor, forwardClock, forwardSynth,
        [&]() { forwardExecutor.prepare(rebuildingInput); });
    const auto preparedForward = forwardExecutor.prepare(forwardInput);
    assert((preparedForward.state.player.position == FixedPosition{0, 10, 0}));
    forwardInput.player = preparedForward.state.player;
    forwardInput.actors = preparedForward.state.actors;
    forwardInput.activeOrder = preparedForward.state.activeOrder;
    assert((forwardInput.lastListCamera == FixedPosition{}));
    rejectWithoutAdvance(forwardExecutor, forwardClock, forwardSynth,
        [&]() { forwardExecutor.prepare(forwardInput); });
    const auto forwardRender = forwardExecutor.render(plan);
    const auto secondDisplay = forwardExecutor.display(forwardPages);
    const auto secondCopy = forwardExecutor.postcopy(FrameCopyProfile::modeXNoop);
    forwardPages.flip();
    auto secondTail = tailInput(finishedForward.state.state.loopTick, forwardInput);
    secondTail.forwardVelocity = forwardInput.player.forwardVelocity;
    const auto secondFinish = forwardExecutor.finish(secondTail, heldForward);
    assert(secondFinish.state.state.forwardVelocity == 20);
    forwardInput.player.forwardVelocity = secondFinish.state.state.forwardVelocity;
    const auto secondPrepare = forwardExecutor.prepare(forwardInput);
    assert((secondPrepare.state.player.position == FixedPosition{0, 30, 0}));
    assert(forwardClock.elapsedSamples() == beforeForward
        + finishedForward.work.audio.samples.size() + preparedForward.work.audio.samples.size()
        + forwardRender.audio.samples.size() + secondDisplay.work.audio.samples.size()
        + secondCopy.audio.samples.size() + secondFinish.work.audio.samples.size()
        + secondPrepare.work.audio.samples.size());
    const auto allCycles = completedCycles + preparedForward.work.timing.total.elapsedCycles
        + forwardRender.timing.total.elapsedCycles + secondDisplay.work.timing.total.elapsedCycles
        + secondCopy.timing.total.elapsedCycles + secondFinish.work.timing.total.elapsedCycles
        + secondPrepare.work.timing.total.elapsedCycles;
    assert(forwardSynth.driverState().normalClockCycles == static_cast<std::uint64_t>(allCycles));

}
