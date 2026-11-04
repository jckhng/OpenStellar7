#include "stellar7/page_flip_clock.hpp"

#include <cassert>
#include <stdexcept>

namespace {
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
    p.devices->retraceStartMilliseconds = 0.010;
    p.devices->retraceEndMilliseconds = 0.020;
    p.devices->frame = AudioClockVgaFramePhase{1000.0 / 14, 0, 0};
    p.timer = AudioClockTimerPhase{1, 0, 5, 20, 20, 5};
    return p;
}

bool reject(const stellar7::SoundBlasterClockPhase& p, const stellar7::PageFlipClockState& state) {
    try {
        stellar7::pageFlipClockTiming(p, state, 1000);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}
}

int main() {
    using namespace stellar7;
    const PageFlipClockState state{PageFlipSynchronization::enabled, 0xa400, 0xa000, 0x3d4};
    auto noSync = state;
    noSync.synchronization = PageFlipSynchronization::disabled;
    auto p = phase();
    p.devices->displayStart = 0x120034;
    const auto fast = pageFlipClockTiming(p, noSync, 1000);
    assert(fast.timing.total.dispatches == 31 && fast.timing.total.delayCycles == 2);
    assert(fast.timing.total.elapsedCycles == 33);
    assert(fast.lowReads == 0 && fast.highReads == 0);
    assert(fast.state.backPage == 0xa000 && fast.state.frontPage == 0xa400);
    assert(fast.timing.phase.devices->displayStart == 0x124034);
    assert(p.devices->displayStart == 0x120034);

    auto recovered = noSync;
    recovered.layout = PageFlipLayout::recoveredStartup;
    recovered.backPage = 0xa800;
    const auto recoveredFirst = pageFlipClockTiming(phase(), recovered, 1000);
    assert(recoveredFirst.state.frontPage == 0xa800 && recoveredFirst.state.backPage == 0xa000);
    const auto recoveredSecond = pageFlipClockTiming(recoveredFirst.timing.phase, recoveredFirst.state, 1000);
    assert(recoveredSecond.state.frontPage == 0xa000 && recoveredSecond.state.backPage == 0xa800);
    assert(recoveredFirst.timing.total.dispatches == fast.timing.total.dispatches);
    recovered.layout = PageFlipLayout::compatibility;
    assert(reject(phase(),recovered));
    recovered.layout = static_cast<PageFlipLayout>(2);
    assert(reject(phase(),recovered));
    recovered = noSync;
    recovered.layout = PageFlipLayout::recoveredStartup;
    assert(reject(phase(),recovered));

    const auto low = pageFlipClockTiming(phase(), state, 1000);
    assert(low.lowReads == 1 && low.highReads == 2);
    assert(low.timing.total.dispatches == 42 && low.timing.total.delayCycles == 17);
    assert(low.timing.total.elapsedCycles == 59);

    // The first read samples AFTER its five-cycle delay, at the inclusive edge.
    p = phase();
    p.devices->retraceStartMilliseconds = 37.0 / 5000;
    p.devices->retraceEndMilliseconds = 45.0 / 5000;
    p.events = {{0.1, AudioClockEventKind::frameStart, 0}};
    p.linesDone = p.linesTotal;
    p.remainingCycles = 500;
    p.deferredCycles = 4500;
    const auto edge = pageFlipClockTiming(p, state, 1000);
    assert(edge.lowReads == 3 && edge.highReads > 1);

    p = phase();
    p.devices->retraceStartMilliseconds = 30.0 / 5000;
    p.devices->retraceEndMilliseconds = 37.0 / 5000;
    p.events = {{0.1, AudioClockEventKind::frameStart, 0}};
    p.linesDone = p.linesTotal;
    p.remainingCycles = 500;
    p.deferredCycles = 4500;
    const auto high = pageFlipClockTiming(p, state, 1000);
    assert(high.lowReads == 2 && high.highReads > 1);

    // Continued prior work supplies phase; no reset to an independent video clock.
    p = phase();
    const auto prefix = normalWorkTiming(p, {{NormalWorkKind::instructions, 10}}, 1000);
    const auto continued = pageFlipClockTiming(prefix.phase, state, 1000);
    p.remainingCycles -= 10;
    const auto direct = pageFlipClockTiming(p, state, 1000);
    assert(continued.timing.total.elapsedCycles == direct.timing.total.elapsedCycles);
    assert(continued.timing.phase.remainingCycles == direct.timing.phase.remainingCycles);

    // IRQ entry belongs to the shared engine, once, during a polling refill.
    p = phase();
    p.remainingCycles = 1;
    p.deferredCycles = 4999;
    p.irqCheckRequested = true;
    p.devices->irq0Requested = true;
    p.irq0ActivationPossible = true;
    const auto irq = pageFlipClockTiming(p, noSync, 1000);
    assert(irq.timing.total.dispatches == 31 + 46);
    assert(irq.timing.total.delayCycles == 2 + 3);
    assert(irq.timing.phase.timer->biosCount == 4);
    assert(!irq.timing.phase.devices->irq0InService);

    p = phase();
    p.remainingCycles = 40;
    p.deferredCycles = 4960;
    p.events = {{0.008, AudioClockEventKind::irqCheck, 0}};
    p.irqPending = true;
    p.devices->irq0Requested = true;
    p.irq0ActivationPossible = true;
    p.devices->retraceStartMilliseconds = 0.025;
    p.devices->retraceEndMilliseconds = 0.080;
    p.timer->biosCount = 1;
    p.timer->callbackCount = 1;
    p.callback = SoundBlasterCallbackState{};
    p.callback->wait = 2;
    p.bios = AudioClockBiosPhase{};
    p.bios->domain = AudioClockBiosDomain::normalVgaDefaultTimer;
    const auto nested = pageFlipClockTiming(p, state, 1000);
    assert(nested.timing.callbacks.size() == 1);
    assert(nested.timing.phase.bios->timerTicks == 1);
    assert(nested.timing.phase.callback->wait == 1);
    const auto reads = nested.lowReads + nested.highReads;
    assert(nested.timing.total.dispatches == 33 + 3 * reads + 47 + 73 + 15);
    assert(nested.timing.total.delayCycles == 2 + 5 * reads + 3);

    p = phase();
    p.interrupts = AudioClockInterrupts::masked;
    const auto masked = pageFlipClockTiming(p, state, 1000);
    assert(masked.timing.phase.interrupts == AudioClockInterrupts::masked);
    assert(masked.timing.total.dispatches == low.timing.total.dispatches);

    auto bad = state;
    bad.crtcPort = 0x3b4;
    assert(reject(phase(), bad));
    bad = state;
    bad.backPage = bad.frontPage;
    assert(reject(phase(), bad));
    p = phase();
    p.devices.reset();
    assert(reject(p, state));
    p = phase();
    p.devices->retraceEndMilliseconds = p.devices->retraceStartMilliseconds;
    assert(reject(p, state));
    p = phase();
    p.devices->retraceStartMilliseconds = 1;
    p.devices->retraceEndMilliseconds = 2;
    assert(reject(p, state)); // Source polling cannot finish inside this horizon.
    bad = state;
    bad.synchronization = static_cast<PageFlipSynchronization>(2);
    assert(reject(phase(), bad));
    bad = state;
    bad.waitDomain = static_cast<PageFlipWaitDomain>(2);
    assert(reject(phase(), bad));
    bad.layout = PageFlipLayout::recoveredStartup;
    bad.backPage = 0xa800;
    bad.waitDomain = PageFlipWaitDomain::retainedProgram;
    assert(reject(phase(), bad)); // Requires its retained resource-backed callback.
    bool oversized = false;
    try { pageFlipClockTiming(phase(), noSync, 120001); }
    catch (const std::invalid_argument&) { oversized = true; }
    assert(oversized); // Default callers retain the original bound.
}
