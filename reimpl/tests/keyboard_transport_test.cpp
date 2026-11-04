#include "stellar7/normal_clock.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

#ifdef NDEBUG
#error Assertions must be enabled
#endif

namespace {
stellar7::SoundBlasterClockPhase initial() {
    using namespace stellar7;
    SoundBlasterClockPhase phase{};
    phase.remainingCycles = 1000;
    phase.deferredCycles = 4000;
    phase.interrupts = AudioClockInterrupts::masked;
    phase.isa8DelayNanoseconds = 1020;
    phase.pitPeriodMilliseconds = 55;
    phase.irqDelayMilliseconds = 0.0003;
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
    phase.keyboard = KeyboardTransportState{};
    auto& keyboard = *phase.keyboard;
    keyboard.domain = KeyboardTransportDomain::normalVgaSet1Up;
    keyboard.position = 79;
    keyboard.port60Data = 0xd0;
    keyboard.port61Data = 0x30;
    for (std::size_t i = 0; i < keyboard.buffer.size(); ++i) {
        keyboard.buffer[i] = static_cast<std::uint16_t>(i * 7 + 1);
    }
    return phase;
}

void equal(const stellar7::SoundBlasterClockPhase& left, const stellar7::SoundBlasterClockPhase& right) {
    assert(left.remainingCycles == right.remainingCycles && left.deferredCycles == right.deferredCycles);
    assert(left.devices->picTicks == right.devices->picTicks);
    assert(left.devices->irq1Input == right.devices->irq1Input);
    assert(left.devices->irq1Requested == right.devices->irq1Requested);
    assert(left.irqPending == right.irqPending && left.irqCheckRequested == right.irqCheckRequested);
    assert(left.events.size() == right.events.size());
    for (std::size_t i = 0; i < left.events.size(); ++i) {
        assert(left.events[i].kind == right.events[i].kind);
        assert(left.events[i].deadlineMilliseconds == right.events[i].deadlineMilliseconds);
        assert(left.events[i].value == right.events[i].value);
    }
    const auto& a = *left.keyboard;
    const auto& b = *right.keyboard;
    assert(a.buffer == b.buffer && a.position == b.position && a.used == b.used);
    assert(a.port60Data == b.port60Data && a.port60Pending == b.port60Pending && a.scheduled == b.scheduled);
    assert(a.repeatKey == b.repeatKey && a.repeatWait == b.repeatWait && a.port61Data == b.port61Data);
}

template<class Action> bool rejects(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}
}

int checkTransport() {
    using namespace stellar7;
    const auto original = initial();
    const auto queued = queueUpKey(original, KeyboardEdge::press);
    assert(queued.remainingCycles == original.remainingCycles && queued.deferredCycles == original.deferredCycles);
    assert(queued.keyboard->used == 2 && queued.keyboard->position == 79 && queued.keyboard->scheduled);
    assert(queued.keyboard->buffer[79] == 0xe0 && queued.keyboard->buffer[80] == 0x48);
    assert(queued.keyboard->buffer[78] == original.keyboard->buffer[78]);
    assert(queued.keyboard->port60Data == 0xd0 && queued.keyboard->repeatWait == 500);
    assert(queued.events[0].deadlineMilliseconds == static_cast<double>(0.300f));
    assert(original.keyboard->used == 0);

    const auto first = normalWorkTiming(queued, {{NormalWorkKind::instructions,1600}}, 8000);
    assert(first.phase.keyboard->position == 80 && first.phase.keyboard->used == 1);
    assert(first.phase.keyboard->port60Data == 0xe0 && first.phase.keyboard->port60Pending);
    assert(first.phase.devices->irq1Input && first.phase.devices->irq1Requested);
    const auto read = normalWorkTiming(first.phase, {{NormalWorkKind::keyboardDataRead,1}}, 8000);
    assert(read.total.dispatches == 1 && read.total.delayCycles == 5);
    assert(read.keyboardReads.size() == 1 && read.keyboardReads[0].value == 0xe0);
    assert(read.phase.keyboard->scheduled && !read.phase.keyboard->port60Pending);
    assert(!read.phase.devices->irq1Input && !read.phase.devices->irq1Requested);
    const auto second = normalWorkTiming(read.phase, {{NormalWorkKind::instructions,1600}}, 8000);
    assert(second.phase.keyboard->position == 81 && second.phase.keyboard->used == 0);
    assert(second.phase.keyboard->port60Data == 0x48);
    const auto drained = normalWorkTiming(second.phase, {{NormalWorkKind::keyboardDataRead,1},
        {NormalWorkKind::keyboardControlRead,1}, {NormalWorkKind::keyboardControlWrite,1,0xa0},
        {NormalWorkKind::keyboardControlWrite,1,0x20}}, 8000);
    assert(drained.total.delayCycles == 16 && drained.keyboardReads[0].value == 0x48);
    assert(!drained.phase.keyboard->port60Pending && !drained.phase.keyboard->scheduled);
    assert(drained.phase.keyboard->port61Data == 0x20);
    const auto joined = normalWorkTiming(queued, {{NormalWorkKind::instructions,1600},
        {NormalWorkKind::keyboardDataRead,1}, {NormalWorkKind::instructions,1600},
        {NormalWorkKind::keyboardDataRead,1}, {NormalWorkKind::keyboardControlRead,1},
        {NormalWorkKind::keyboardControlWrite,1,0xa0}, {NormalWorkKind::keyboardControlWrite,1,0x20}}, 8000);
    equal(drained.phase, joined.phase);
    assert(joined.total.dispatches == 3205 && joined.total.delayCycles == 21);
    assert(joined.total.elapsedCycles == first.total.elapsedCycles + read.total.elapsedCycles
        + second.total.elapsedCycles + drained.total.elapsedCycles);
    assert(joined.total.terminalCharges == first.total.terminalCharges + read.total.terminalCharges
        + second.total.terminalCharges + drained.total.terminalCharges);

    const auto waited = normalWorkTiming(drained.phase, {{NormalWorkKind::instructions,6000}}, 10000);
    assert(waited.phase.keyboard->repeatWait == 499);
    auto released = queueUpKey(waited.phase, KeyboardEdge::release);
    assert(released.keyboard->repeatWait == 0 && released.keyboard->repeatKey == KeyboardTransportKey::none);
    const auto breakFirst = normalWorkTiming(released, {{NormalWorkKind::instructions,1600},
        {NormalWorkKind::keyboardDataRead,1}}, 8000);
    assert(breakFirst.keyboardReads[0].value == 0xe0);
    const auto breakSecond = normalWorkTiming(breakFirst.phase, {{NormalWorkKind::instructions,1600},
        {NormalWorkKind::keyboardDataRead,1}}, 8000);
    assert(breakSecond.keyboardReads[0].value == 0xc8);
    assert(breakSecond.phase.keyboard->used == 0 && !breakSecond.phase.keyboard->port60Pending);

    auto capped = original;
    capped.remainingCycles = 5000;
    capped.deferredCycles = 0;
    const auto cappedQueue = queueUpKey(capped, KeyboardEdge::press);
    assert(cappedQueue.remainingCycles == 0 && cappedQueue.deferredCycles == 5000);
    const auto cappedWork = normalWorkTiming(cappedQueue, {{NormalWorkKind::instructions,1600}}, 8000);
    assert(cappedWork.phase.keyboard->port60Data == 0xe0);
    auto enabled = queued;
    enabled.interrupts = AudioClockInterrupts::enabled;
    assert(rejects([&] { normalWorkTiming(enabled, {{NormalWorkKind::instructions,1600}}, 8000); }));
    auto expiry = original;
    expiry.keyboard->repeatKey = KeyboardTransportKey::up;
    expiry.keyboard->repeatWait = 1;
    const auto repeated = normalWorkTiming(expiry, {{NormalWorkKind::instructions,6000}}, 8000);
    assert(repeated.phase.keyboard->repeatKey == KeyboardTransportKey::up);
    assert(repeated.phase.keyboard->repeatWait == keyboardRepeatRateMilliseconds);
    assert(repeated.phase.keyboard->used == 2 && repeated.phase.keyboard->scheduled);
    assert(!repeated.phase.keyboard->port60Pending && !repeated.phase.devices->irq1Input);
    assert(repeated.phase.keyboard->buffer[79] == 0xe0 && repeated.phase.keyboard->buffer[80] == 0x48);
    assert(repeated.phase.events.size() == 1);
    assert(repeated.phase.events[0].kind == AudioClockEventKind::keyboardTransfer);
    assert(expiry.keyboard->repeatWait == 1);
    constexpr std::int64_t tickOvershootCycles = 4;
    constexpr std::int64_t fixedCyclesPerMillisecond = 5000;
    auto overshoot = expiry;
    overshoot.remainingCycles = 1;
    overshoot.deferredCycles = 0;
    const auto shifted = normalWorkTiming(overshoot, {{NormalWorkKind::port8Write,1},
        {NormalWorkKind::instructions,1}}, 100);
    const auto repeatDeadline = static_cast<double>(keyboardTransferDelayMilliseconds)
        + static_cast<double>(tickOvershootCycles) / fixedCyclesPerMillisecond;
    assert(shifted.phase.events.size() == 1);
    assert(shifted.phase.events[0].deadlineMilliseconds == repeatDeadline);
    assert(shifted.phase.keyboard->used == 2 && shifted.phase.keyboard->scheduled);
    auto overflow = *expiry.keyboard;
    overflow.used = keyboardBufferWords - 1;
    overflow.port60Pending = true;
    overflow.buffer.fill(0xe0);
    assert(rejects([&] { keyboardTick(overflow); }));
    auto wrapExpiry = *expiry.keyboard;
    wrapExpiry.position = keyboardBufferWords - 1;
    const auto wrappedRepeat = keyboardTick(wrapExpiry);
    assert(wrappedRepeat.state.buffer[95] == 0xe0 && wrappedRepeat.state.buffer[0] == 0x48);
    assert(wrappedRepeat.intent.scheduleTransfer && wrappedRepeat.state.repeatWait == keyboardRepeatRateMilliseconds);
    auto absent = original;
    absent.keyboard.reset();
    assert(rejects([&] { queueUpKey(absent, KeyboardEdge::press); }));
    assert(rejects([&] { normalWorkTiming(original, {{NormalWorkKind::keyboardControlWrite,1,1}}, 100); }));
    auto brokenEvent = queued;
    brokenEvent.events.clear();
    assert(rejects([&] { normalWorkTiming(brokenEvent, {{NormalWorkKind::instructions,1}}, 100); }));
    auto missingOwner = original;
    missingOwner.events.push_back({0.3,AudioClockEventKind::keyboardTransfer,0});
    assert(rejects([&] { normalWorkTiming(missingOwner, {{NormalWorkKind::instructions,1}}, 100); }));
    assert(keyboardRead61(*original.keyboard, 0.014) == 0x20);
    assert(keyboardRead61(*original.keyboard, 0.016) == 0x30);
    auto wrap = *original.keyboard;
    wrap.position = 95;
    const auto wrapped = keyboardUpEdge(wrap, KeyboardEdge::press);
    assert(wrapped.state.buffer[95] == 0xe0 && wrapped.state.buffer[0] == 0x48);
    assert(keyboardTransfer(wrapped.state).state.position == 0);
    return 0;
}

int main() {
    try {
        return checkTransport();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
