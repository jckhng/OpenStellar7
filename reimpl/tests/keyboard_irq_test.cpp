#include "keyboard_clock_access.hpp"
#include "stellar7/presentation_clock.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <type_traits>

#ifdef NDEBUG
#error Assertions must be enabled
#endif

static_assert(!std::is_copy_constructible_v<stellar7::PresentationClock>);
static_assert(!std::is_move_constructible_v<stellar7::PresentationClock>);
static_assert(!std::is_copy_constructible_v<stellar7::FrameController>);
static_assert(!std::is_move_constructible_v<stellar7::FrameController>);
static_assert(!std::is_copy_constructible_v<stellar7::KeyboardClockAccess>);
static_assert(!std::is_move_constructible_v<stellar7::KeyboardClockAccess>);

namespace {
constexpr std::uint16_t ringBegin = 0x1e;
constexpr std::uint16_t lastRingOffset = 0x3c;
constexpr std::uint16_t ringEntryBytes = 2;
constexpr std::size_t ringEntries = 16;
constexpr std::size_t upperRepeatEntries = 2;
constexpr std::size_t transportEntries = 96;

stellar7::SoundBlasterClockPhase initial() {
    using namespace stellar7;
    SoundBlasterClockPhase phase{};
    phase.remainingCycles = 1000;
    phase.deferredCycles = 4000;
    phase.interrupts = AudioClockInterrupts::enabled;
    phase.interruptDelivery = AudioClockInterruptDelivery::residentTimerKeyboard;
    phase.isa8DelayNanoseconds = 1020;
    phase.pitPeriodMilliseconds = 55;
    phase.irqDelayMilliseconds = 0.0003;
    phase.irq0ActivationPossible = true;
    phase.singleLinePeriodMilliseconds = 0.03;
    phase.horizontalPeriodMilliseconds = 0.03;
    phase.linesTotal = 400;
    phase.minimumLatchDelayMilliseconds = 0.001;
    phase.vgaInterrupt = AudioClockVgaInterrupt::fixedVgaNoDemand;
    phase.devices = AudioClockDevicePhase{};
    phase.devices->fixedPitAndMaskedDma = true;
    phase.devices->picTicks = 211000;
    phase.devices->framePeriodMilliseconds = 14;
    phase.devices->displayEndMilliseconds = 12;
    phase.devices->retraceStartMilliseconds = 13;
    phase.devices->retraceEndMilliseconds = 13.1;
    phase.timer = AudioClockTimerPhase{1,100,10,100,1,4};
    phase.bios = AudioClockBiosPhase{};
    phase.bios->domain = AudioClockBiosDomain::normalVgaDefaultTimer;
    phase.keyboard = KeyboardTransportState{};
    phase.keyboard->domain = KeyboardTransportDomain::normalVgaSet1Up;
    phase.keyboard->position = 79;
    phase.keyboard->port60Data = 0xd0;
    phase.keyboard->port61Data = 0x30;
    phase.instructionKeyboardEdge = KeyboardEdge::press;
    return phase;
}

int check() {
    using namespace stellar7;
    {
        constexpr std::int64_t cyclesPerMillisecond = 5000;
        constexpr std::int64_t prefixDispatches = 43;
        constexpr std::int64_t prefixIsaDelayCycles = 19;
        constexpr std::int64_t initialSpentSliceCharge = 1;
        const auto restoreBoundary = static_cast<double>(prefixDispatches
            + prefixIsaDelayCycles + initialSpentSliceCharge) / cyclesPerMillisecond;

        FrameController sliceOwner;
        FrameControllerInitial sliceController{};
        sliceController.origin = FrameControllerOrigin::restoredDiagnosticInitial;
        sliceController.loopTick = 4;
        sliceController.keyboardHead = sliceController.keyboardTail = 0x36;
        sliceOwner.initialize(sliceController);
        KeyboardClockAccess sliceAccess(sliceOwner);

        auto slicePhase = initial();
        slicePhase.remainingCycles = 0;
        slicePhase.deferredCycles = cyclesPerMillisecond;
        slicePhase.instructionKeyboardEdge.reset();
        slicePhase.irqCheckRequested = true;
        slicePhase.keyboard->port60Data = 0xe0;
        slicePhase.keyboard->port60Pending = true;
        slicePhase.keyboard->used = 0;
        slicePhase.keyboard->scheduled = false;
        slicePhase.devices->irq1Requested = true;
        slicePhase.devices->irq1Input = true;
        slicePhase.events = {{restoreBoundary, AudioClockEventKind::scanline, 0}};

        const auto boundary = normalWorkTimingBound(slicePhase,
            {{NormalWorkKind::instructions, 1}}, 1000, &sliceAccess);
        assert(boundary.keyboardIrqs.size() == 1);
        const auto& irq = boundary.keyboardIrqs.front();
        assert(irq.scan == 0xe0);
        assert(irq.preIret.dispatches - irq.entry.dispatches == prefixDispatches);
        assert(irq.preIret.terminalCharges == 2);
        assert(irq.completed.terminalCharges >= irq.preIret.terminalCharges);
        assert(boundary.total.terminalCharges >= irq.completed.terminalCharges);
        assert(boundary.total.elapsedCycles == boundary.total.dispatches
            + boundary.total.delayCycles + boundary.total.terminalCharges);
    }
    FrameController owner;
    FrameControllerInitial controller{};
    controller.origin = FrameControllerOrigin::restoredDiagnosticInitial;
    controller.loopTick = 4;
    controller.keyboardHead = controller.keyboardTail = 0x36;
    for (std::size_t i = 0; i < controller.keyboardSlots.size(); ++i) {
        controller.keyboardSlots[i] = i % 2 == 0 ? 0x4b00 : 0x4d00;
    }
    owner.initialize(controller);
    KeyboardClockAccess access(owner);
    const auto make = normalWorkTimingBound(initial(), {{NormalWorkKind::instructions,4000}}, 10000, &access);
    assert(make.keyboardIrqs.size() == 2);
    assert(make.total.dispatches == 4144 && make.total.delayCycles == 38);
    const std::uint8_t scans[] = {0xe0,0x48};
    const std::uint64_t counts[] = {43,99};
    for (std::size_t i = 0; i < make.keyboardIrqs.size(); ++i) {
        const auto& irq = make.keyboardIrqs[i];
        assert(irq.scan == scans[i]);
        assert(irq.preIret.dispatches - irq.entry.dispatches == counts[i]);
        assert(irq.preIret.delayCycles - irq.entry.delayCycles == 19);
        assert(irq.completed.dispatches - irq.preIret.dispatches == 1);
    }
    auto state = owner.keyboardState();
    assert(state.metadata48 == 0x71 && state.axisRoles[0] == 7 && state.lastKey == 0x4800);
    assert(state.head == 0x36 && state.tail == 0x38 && state.slots[12] == 0x4800);
    assert(state.slots[13] == controller.keyboardSlots[13]);
    assert(make.phase.interrupts == AudioClockInterrupts::enabled && !make.phase.devices->irq1InService);
    const auto consume = normalWorkTimingBound(make.phase, {{NormalWorkKind::keyboardConsumeHead,1},
        {NormalWorkKind::keyboardStoreScan,1,0x48}}, 100, &access);
    FrameTailWork tail{};
    tail.state.loopTick = 5;
    tail.keyboardHead = 0x38;
    tail.consumedScan = 0x48;
    owner.completeTail(4,tail);
    auto phase = consume.phase;
    try {
        access.validateEdge(KeyboardEdge::press);
        assert(false);
    } catch (const std::invalid_argument&) {
    }
    phase.keyboard->repeatWait = 1;
    const auto repeat = normalWorkTimingBound(phase, {{NormalWorkKind::instructions,8000}}, 14000, &access);
    assert(repeat.keyboardIrqs.size() == 2);
    assert(repeat.keyboardIrqs[0].scan == 0xe0 && repeat.keyboardIrqs[1].scan == 0x48);
    for (std::size_t i = 0; i < repeat.keyboardIrqs.size(); ++i) {
        const auto& irq = repeat.keyboardIrqs[i];
        assert(irq.preIret.dispatches - irq.entry.dispatches == counts[i]);
        assert(irq.preIret.delayCycles - irq.entry.delayCycles == 19);
    }
    state = owner.keyboardState();
    assert(state.metadata48 == 0x71 && state.axisRoles[0] == 7 && state.lastKey == 0x4800);
    assert(state.head == 0x38 && state.tail == 0x3a && state.slots[13] == 0x4800);
    const auto repeatedConsume = normalWorkTimingBound(repeat.phase, {{NormalWorkKind::keyboardConsumeHead,1},
        {NormalWorkKind::keyboardStoreScan,1,0x48}}, 100, &access);
    tail.state.loopTick = 6;
    tail.keyboardHead = 0x3a;
    owner.completeTail(5,tail);
    phase = repeatedConsume.phase;
    for (std::size_t index = 0; index < upperRepeatEntries + ringEntries; ++index) {
        const auto head = static_cast<std::uint16_t>(index < upperRepeatEntries
            ? 0x3a + index * ringEntryBytes : ringBegin + (index - upperRepeatEntries) * ringEntryBytes);
        try {
            access.validateEdge(KeyboardEdge::press);
            assert(false);
        } catch (const std::invalid_argument&) {
        }
        phase.keyboard->repeatWait = 1;
        const auto nextRepeat = normalWorkTimingBound(phase, {{NormalWorkKind::instructions,8000}}, 14000, &access);
        assert(nextRepeat.keyboardIrqs.size() == 2);
        assert(nextRepeat.keyboardIrqs[0].scan == 0xe0 && nextRepeat.keyboardIrqs[1].scan == 0x48);
        assert(nextRepeat.keyboardIrqs[0].preIret.dispatches - nextRepeat.keyboardIrqs[0].entry.dispatches == 43);
        assert(nextRepeat.keyboardIrqs[1].preIret.dispatches - nextRepeat.keyboardIrqs[1].entry.dispatches == (head == 0x3c ? 97 : 99));
        for (const auto& irq : nextRepeat.keyboardIrqs) {
            assert(irq.preIret.delayCycles - irq.entry.delayCycles == 19);
            assert(irq.completed.dispatches - irq.preIret.dispatches == 1);
        }
        const auto nextHead = static_cast<std::uint16_t>(head == lastRingOffset ? ringBegin : head + ringEntryBytes);
        state = owner.keyboardState();
        assert(state.metadata48 == 0x71 && state.axisRoles[0] == 7 && state.lastKey == 0x4800);
        assert(state.head == head && state.tail == nextHead && state.slots[(head - 0x1e) / 2] == 0x4800);
        try {
            access.validateScan(0x48);
            assert(false);
        } catch (const std::invalid_argument&) {
        }
        const auto guards = owner.augmentGuards({});
        assert(guards.keyboardQueue == FrameTailKeyboardQueue::forwardMake);
        const auto consumed = normalWorkTimingBound(nextRepeat.phase, {{NormalWorkKind::keyboardConsumeHead,1},
            {NormalWorkKind::keyboardStoreScan,1,0x48}}, 100, &access);
        const auto tick = owner.expectedLoopTick();
        tail.state.loopTick = static_cast<std::uint16_t>(tick + 1);
        tail.keyboardHead = nextHead;
        owner.completeTail(tick,tail);
        phase = consumed.phase;
    }
    access.validateScan(0x48);
    for (const auto scan : {std::uint8_t{0x4b}, std::uint8_t{0xcb}, std::uint8_t{0}}) {
        try {
            access.validateScan(scan);
            assert(false);
        } catch (const std::invalid_argument&) {
        }
    }
    phase.instructionKeyboardEdge = KeyboardEdge::release;
    const auto release = normalWorkTimingBound(phase, {{NormalWorkKind::instructions,4000}}, 10000, &access);
    assert(release.keyboardIrqs.size() == 2 && release.total.dispatches == 4118);
    assert(release.keyboardIrqs[0].scan == 0xe0 && release.keyboardIrqs[1].scan == 0xc8);
    assert(release.keyboardIrqs[1].preIret.dispatches - release.keyboardIrqs[1].entry.dispatches == 73);
    state = owner.keyboardState();
    assert(state.metadata48 == 0x70 && state.axisRoles[0] == 0 && state.lastKey == 0);
    assert(state.head == 0x1e && state.tail == 0x1e && state.slots[12] == 0x4800);
    assert(state.consumedScan == 0x48);
    assert(release.phase.keyboard->used == 0 && !release.phase.keyboard->port60Pending);
    assert(release.phase.keyboard->repeatWait == 0 && release.phase.keyboard->position == (89 + ringEntries * 2) % transportEntries);
    return 0;
}
}

int main() {
    try {
        return check();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
