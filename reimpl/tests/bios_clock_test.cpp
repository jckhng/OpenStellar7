#include "stellar7/audio_clock.hpp"
#include "stellar7/normal_clock.hpp"
#include "stellar7/presentation_clock.hpp"
#include "stellar7/timer_work.hpp"
#include "stellar7/timer_install_work.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {
stellar7::SoundBlasterClockPhase phase() {
    using namespace stellar7;
    SoundBlasterClockPhase p{};
    p.remainingCycles = 1000;
    p.interrupts = AudioClockInterrupts::masked;
    p.isa8DelayNanoseconds = 1020;
    p.dmaDacRate = 44100;
    p.pitPeriodMilliseconds = 55;
    p.irqDelayMilliseconds = 0.002;
    p.singleLinePeriodMilliseconds = 0.03;
    p.horizontalPeriodMilliseconds = 0.03;
    p.linesTotal = 400;
    p.minimumLatchDelayMilliseconds = 0.001;
    p.frameStart = AudioClockFrameStart::unsupported;
    p.interruptDelivery = AudioClockInterruptDelivery::blockedByIrq0;
    p.vgaInterrupt = AudioClockVgaInterrupt::fixedVgaNoDemand;
    p.irqCheckRequested = true;
    p.devices = AudioClockDevicePhase{};
    p.devices->fixedPitAndMaskedDma = true;
    p.devices->framePeriodMilliseconds = 14;
    p.devices->displayEndMilliseconds = 12;
    p.devices->retraceStartMilliseconds = 13;
    p.devices->retraceEndMilliseconds = 13.1;
    p.devices->irq0InService = true;
    p.timer = AudioClockTimerPhase{1, 0, 5, 20, 20, 5};
    p.bios = AudioClockBiosPhase{};
    p.bios->domain = AudioClockBiosDomain::normalVgaDefaultTimer;
    p.bios->timerTicks = 1234;
    p.bios->keyboardState = 0x20;
    p.bios->keyboardLeds = 2;
    p.bios->keyboardLedSync = true;
    return p;
}

bool reject(const stellar7::SoundBlasterClockPhase& p) {
    try { stellar7::biosChainTiming(p); } catch (const std::invalid_argument&) { return true; }
    return false;
}

stellar7::SoundBlasterClockPhase emptyTimerPhase() {
    using namespace stellar7;
    constexpr double pitFrequency = 1193182;
    constexpr double pixelClock = 25175000;
    constexpr double linePeriod = 100 * 8 * 1000 / pixelClock;
    SoundBlasterClockPhase p{};
    // Declared native programming epoch; no captured phase or callback seed.
    p.remainingCycles = 1;
    p.deferredCycles = 4999;
    p.interrupts = AudioClockInterrupts::enabled;
    p.isa8DelayNanoseconds = 1020;
    p.pitPeriodMilliseconds = 5041 * 1000 / pitFrequency;
    p.irqDelayMilliseconds = 0.002;
    p.singleLinePeriodMilliseconds = static_cast<float>(linePeriod);
    p.horizontalPeriodMilliseconds = linePeriod;
    p.linesTotal = 400;
    p.minimumLatchDelayMilliseconds = 0.001;
    p.frameStart = AudioClockFrameStart::activeUnlockedVgaSurface;
    p.interruptDelivery = AudioClockInterruptDelivery::residentTimerIrq0;
    p.vgaInterrupt = AudioClockVgaInterrupt::fixedVgaNoDemand;
    p.irq0ActivationPossible = true;
    p.devices = AudioClockDevicePhase{};
    p.devices->fixedPitAndMaskedDma = true;
    p.devices->irq0Requested = true;
    p.devices->framePeriodMilliseconds = linePeriod * 449;
    p.devices->displayEndMilliseconds = linePeriod * 400;
    p.devices->retraceStartMilliseconds = linePeriod * 412;
    p.devices->retraceEndMilliseconds = linePeriod * 414;
    p.events = {{0.0002, AudioClockEventKind::irqCheck, 0},
        {linePeriod / 4, AudioClockEventKind::scanline, 0},
        {p.pitPeriodMilliseconds, AudioClockEventKind::pit, 0}};
    p.timer = AudioClockTimerPhase{0, 0, 13, 1, 4, 13};
    p.bios = AudioClockBiosPhase{};
    p.bios->domain = AudioClockBiosDomain::normalVgaDefaultTimer;
    return p;
}

void assertEmptyTimerIrq() {
    using namespace stellar7;
    constexpr std::uint64_t foregroundDispatches = 100;
    const std::vector<NormalWorkOperation> work{{NormalWorkKind::instructions, foregroundDispatches}};
    auto p = emptyTimerPhase();
    const auto local = normalWorkTiming(p, work, 1000);
    assert(local.total.dispatches == foregroundDispatches
        + timerIrqWrapperDispatches(0, 0, TimerIrqExit::localIret));
    assert(local.total.dispatches == 138 && local.total.delayCycles == 3);
    assert(local.phase.timer->globalCount == 0 && local.phase.timer->biosCount == 12);
    assert(local.phase.timer->callbackCount == 1 && local.phase.timer->callbackReload == 4);
    assert(!local.phase.callback && local.callbacks.empty());
    assert(!local.phase.devices->irq0Requested && !local.phase.devices->irq0InService);
    assert(local.phase.interrupts == AudioClockInterrupts::enabled);
    assert(p.timer->biosCount == 13 && p.devices->irq0Requested);
    for (const std::uint16_t inactiveCount : {0, 0xffff}) {
        auto inactive = p;
        inactive.timer->callbackCount = inactiveCount;
        const auto untouched = normalWorkTiming(inactive, work, 1000);
        assert(untouched.phase.timer->callbackCount == inactiveCount && untouched.callbacks.empty());
    }

    p.timer->biosCount = 1;
    p.timer->globalCount = 2;
    p.callback = SoundBlasterCallbackState{};
    p.callback->wait = 7;
    const auto chained = normalWorkTiming(p, work, 1000);
    assert(chained.total.dispatches == foregroundDispatches
        + timerIrqWrapperDispatches(0, 0, TimerIrqExit::biosChain) + 15);
    assert(chained.total.dispatches == 152 && chained.total.delayCycles == 3);
    assert(chained.phase.bios->timerTicks == 1 && chained.phase.timer->biosCount == 13);
    assert(chained.phase.timer->globalCount == 1);
    assert(chained.phase.timer->callbackCount == 1 && chained.phase.timer->callbackReload == 4);
    assert(chained.callbacks.empty() && chained.phase.callback->wait == 7);
    assert(!chained.phase.devices->irq0Requested && !chained.phase.devices->irq0InService);
    assert(chained.phase.interrupts == AudioClockInterrupts::enabled);

    const auto rejects = [&](const SoundBlasterClockPhase& invalid) {
        try {
            normalWorkTiming(invalid, work, 1000);
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    auto invalid = p;
    invalid.bios.reset();
    assert(rejects(invalid));
    invalid = p;
    invalid.bios->domain = AudioClockBiosDomain::unsupported;
    assert(rejects(invalid));
    invalid = p;
    invalid.timer->activeMask = 2;
    assert(rejects(invalid));
    invalid = p;
    invalid.timer->biosCount = 0;
    assert(rejects(invalid));
    invalid = p;
    invalid.timer->biosCount = 14;
    assert(rejects(invalid));
    invalid = p;
    invalid.timer->biosReload = 0;
    assert(rejects(invalid));
}

void assertRetainedEmptyTimerClock() {
    using namespace stellar7;
    auto initial = emptyTimerPhase();
    auto& devices = *initial.devices;
    devices.frame = AudioClockVgaFramePhase{1000 / devices.framePeriodMilliseconds, 0, 0};
    initial.events.push_back({static_cast<float>(devices.displayEndMilliseconds + 0.005),
        AudioClockEventKind::pendingVgaInterrupt, 0});
    initial.events.push_back({static_cast<float>(devices.retraceStartMilliseconds),
        AudioClockEventKind::displayStartLatch, 0});
    initial.events.push_back({static_cast<float>(devices.retraceEndMilliseconds),
        AudioClockEventKind::panningLatch, 0});
    initial.events.push_back({static_cast<float>(devices.framePeriodMilliseconds),
        AudioClockEventKind::frameStart, 0});

    SoundBlasterSynth firstSynth;
    SoundBlasterSynth secondSynth;
    PresentationClock firstClock(firstSynth);
    PresentationClock secondClock(secondSynth);
    auto first = initial;
    auto second = initial;
    std::vector<std::int16_t> firstPcm;
    std::vector<std::int16_t> secondPcm;
    SoundBlasterClockStep firstTotal{};
    SoundBlasterClockStep secondTotal{};
    const auto advance = [](PresentationClock& clock, SoundBlasterClockPhase& retained,
        std::vector<std::int16_t>& pcm, SoundBlasterClockStep& total, const std::uint64_t count) {
        auto work = clock.renderWork(retained, {{NormalWorkKind::instructions, count}}, 120000);
        assert(work.timing.callbacks.empty() && work.audio.writes.empty());
        retained = work.timing.phase;
        pcm.insert(pcm.end(), work.audio.samples.begin(), work.audio.samples.end());
        total.dispatches += work.timing.total.dispatches;
        total.delayCycles += work.timing.total.delayCycles;
        total.elapsedCycles += work.timing.total.elapsedCycles;
        total.terminalCharges += work.timing.total.terminalCharges;
    };
    // Same source work in different packets crosses BIOS expiry and VGA frames.
    for (int index = 0; index < 3; ++index) {
        advance(firstClock, first, firstPcm, firstTotal, 100000);
        advance(secondClock, second, secondPcm, secondTotal, 50000);
        advance(secondClock, second, secondPcm, secondTotal, 50000);
    }
    assert(firstTotal.dispatches == 300000 + 14 * 38 + 52);
    assert(firstTotal.delayCycles == 15 * 3);
    assert(firstTotal.dispatches == secondTotal.dispatches);
    assert(firstTotal.delayCycles == secondTotal.delayCycles);
    assert(firstTotal.elapsedCycles == secondTotal.elapsedCycles);
    assert(firstTotal.terminalCharges == secondTotal.terminalCharges);
    assert(firstPcm == secondPcm && firstClock.elapsedSamples() == secondClock.elapsedSamples());
    assert(first.timer->biosCount == 11 && first.bios->timerTicks == 1);
    assert(first.timer->callbackCount == 1 && first.timer->callbackReload == 4);
    assert(first.timer->globalCount == 0 && first.timer->activeMask == 0);
    assert(first.remainingCycles == second.remainingCycles && first.deferredCycles == second.deferredCycles);
    assert(first.linesDone == second.linesDone && first.devices->picTicks == second.devices->picTicks);
    assert(first.devices->frame->framesSinceTimeBase == 4);
    assert(first.devices->frame->framesSinceTimeBase == second.devices->frame->framesSinceTimeBase);
    assert(first.devices->pitStartMilliseconds == second.devices->pitStartMilliseconds);
    assert(first.devices->pitNowMilliseconds == second.devices->pitNowMilliseconds);
    assert(first.devices->pitCycleBase == second.devices->pitCycleBase);
    assert(first.events.size() == second.events.size());
    for (std::size_t index = 0; index < first.events.size(); ++index) {
        assert(first.events[index].kind == second.events[index].kind);
        assert(first.events[index].deadlineMilliseconds == second.events[index].deadlineMilliseconds);
        assert(first.events[index].value == second.events[index].value);
    }
    bool rejected = false;
    try {
        firstClock.renderSamples(1);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    assert(rejected);

    SoundBlasterSynth inactive;
    PresentationClock inactiveClock(inactive);
    auto stale = initial;
    stale.callback = SoundBlasterCallbackState{};
    stale.callback->wait = 7;
    auto work = inactiveClock.renderWork(stale, {{NormalWorkKind::instructions, 100}}, 1000);
    assert(work.timing.phase.callback->wait == 7 && work.timing.callbacks.empty());
    const auto flip = inactiveClock.renderPageFlip(work.timing.phase,
        {PageFlipSynchronization::disabled, 0xa400, 0xa000, 0x3d4}, 1000);
    assert(flip.state.frontPage == 0xa400 && flip.state.backPage == 0xa000);
    assert(flip.work.timing.phase.callback->wait == 7 && flip.work.timing.callbacks.empty());
    assert(flip.work.timing.phase.timer->biosCount == 12);

    work.timing.callbacks.emplace_back();
    SoundBlasterSynth guarded;
    const auto before = guarded.driverState();
    rejected = false;
    try {
        guarded.renderClocked(work.timing, 0);
    } catch (const std::invalid_argument& error) {
        rejected = std::string(error.what()) == "inactive timer cannot deliver Sound Blaster callbacks";
    }
    assert(rejected && guarded.driverState().normalClockCycles == before.normalClockCycles);

    for (const bool registered : {false, true}) {
        auto unsupported = initial;
        if (registered) {
            unsupported.timer->activeMask = 1;
        } else {
            unsupported.timer.reset();
        }
        SoundBlasterSynth uninitialized;
        PresentationClock clock(uninitialized);
        rejected = false;
        try {
            clock.renderWork(unsupported, {{NormalWorkKind::instructions, 100}}, 1000);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected && clock.elapsedSamples() == 0);
    }
}

void assertTimerInstallation() {
    using namespace stellar7;
    using Stage = AudioClockTimerInstallStage;
    const auto initial = nativeTimerInstallPhase();
    const auto operations = timerInstallOperations();
    assert(operations.size() == 61);
    assert(initial.remainingCycles == 39 && initial.deferredCycles == 4961);
    assert(initial.irqDelayMilliseconds == 0.000838);
    assert(initial.timerInstall->stage == Stage::fresh && !initial.devices->irq0Requested);
    SoundBlasterSynth wholeSynth;
    PresentationClock wholeClock(wholeSynth);
    const auto whole = wholeClock.renderWork(initial, operations, 1000);
    assert(whole.timing.phase.timerInstall->stage == Stage::installed);
    assert(whole.timing.total.dispatches == 67 + 38 && whole.timing.total.delayCycles == 4 * 3 + 3);
    assert(whole.timing.phase.timer->biosCount == 12 && whole.timing.phase.timer->biosReload == 13);
    assert(whole.timing.phase.timer->globalCount == 0 && whole.timing.phase.timer->activeMask == 0);
    assert(whole.timing.phase.timer->callbackCount == 0 && whole.timing.phase.timer->callbackReload == 0);
    assert(whole.timing.phase.interrupts == AudioClockInterrupts::enabled);
    assert(!whole.timing.phase.devices->irq0InService && !whole.timing.phase.devices->irq0Requested);
    assert(whole.timing.phase.timerInstall->irq0InputHigh && whole.timing.phase.timerInstall->masterPicMask == 0x28);
    assert(whole.timing.callbacks.empty() && whole.audio.writes.empty());

    SoundBlasterSynth splitSynth;
    PresentationClock splitClock(splitSynth);
    auto retained = initial;
    std::vector<std::int16_t> pcm;
    SoundBlasterClockStep total{};
    double programmedEpoch = 0;
    for (const auto& operation : operations) {
        const auto previousEpoch = retained.devices->pitStartMilliseconds;
        const auto previousBiosCount = retained.timer->biosCount;
        auto next = splitClock.renderWork(retained, {operation}, 1000);
        retained = next.timing.phase;
        pcm.insert(pcm.end(), next.audio.samples.begin(), next.audio.samples.end());
        total.dispatches += next.timing.total.dispatches;
        total.delayCycles += next.timing.total.delayCycles;
        total.elapsedCycles += next.timing.total.elapsedCycles;
        total.terminalCharges += next.timing.total.terminalCharges;
        if (operation.kind == NormalWorkKind::timerInstallPitControl) {
            assert(retained.devices->irq0Requested && retained.timerInstall->irq0InputHigh);
            assert(retained.interrupts == AudioClockInterrupts::masked);
            assert(retained.interruptDelivery == AudioClockInterruptDelivery::defaultBiosIrq0);
            assert(std::none_of(retained.events.begin(), retained.events.end(), [](const auto& event) {
                return event.kind == AudioClockEventKind::pit;
            }));
        }
        if (operation.kind == NormalWorkKind::timerInstallPitControl
            || operation.kind == NormalWorkKind::timerInstallPitLow) {
            assert(retained.devices->pitStartMilliseconds == previousEpoch);
        }
        if (operation.kind == NormalWorkKind::timerInstallPitHigh) {
            programmedEpoch = retained.devices->pitStartMilliseconds;
            assert(programmedEpoch > previousEpoch);
            assert(retained.devices->pitNowMilliseconds == programmedEpoch && retained.devices->pitCycleBase == 0);
            assert(retained.devices->irq0Requested && retained.timer->biosCount == previousBiosCount);
            assert(std::count_if(retained.events.begin(), retained.events.end(), [](const auto& event) {
                return event.kind == AudioClockEventKind::pit;
            }) == 1);
            const auto pit = std::find_if(retained.events.begin(), retained.events.end(), [](const auto& event) {
                return event.kind == AudioClockEventKind::pit;
            });
            assert(pit->deadlineMilliseconds == programmedEpoch + retained.pitPeriodMilliseconds);
        }
        if (operation.kind == NormalWorkKind::timerInstallVector) {
            assert(retained.timerInstall->stage == Stage::vectorInstalled);
            assert(!retained.devices->irq0Requested && retained.timer->biosCount == 12);
            assert(retained.interrupts == AudioClockInterrupts::masked);
        }
        if (operation.kind == NormalWorkKind::timerInstallComplete) {
            assert(previousBiosCount == 12 && retained.timer->biosCount == 12);
        }
    }
    assert(pcm == whole.audio.samples && splitClock.elapsedSamples() == wholeClock.elapsedSamples());
    assert(total.dispatches == whole.timing.total.dispatches && total.delayCycles == whole.timing.total.delayCycles);
    assert(total.elapsedCycles == whole.timing.total.elapsedCycles);
    assert(total.terminalCharges == whole.timing.total.terminalCharges);
    assert(retained.remainingCycles == whole.timing.phase.remainingCycles);
    assert(retained.deferredCycles == whole.timing.phase.deferredCycles);
    assert(retained.devices->pitStartMilliseconds == whole.timing.phase.devices->pitStartMilliseconds);
    assert(retained.events.size() == whole.timing.phase.events.size());
    for (std::size_t index = 0; index < retained.events.size(); ++index) {
        assert(retained.events[index].kind == whole.timing.phase.events[index].kind);
        assert(retained.events[index].deadlineMilliseconds == whole.timing.phase.events[index].deadlineMilliseconds);
    }
    const auto samplesBeforeRetry = wholeClock.elapsedSamples();
    bool rejected = false;
    try {
        wholeClock.renderWork(whole.timing.phase, operations, 1000);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && wholeClock.elapsedSamples() == samplesBeforeRetry);
}

void assertTimerInstallGuards() {
    using namespace stellar7;
    const auto initial = nativeTimerInstallPhase();
    const auto original = timerInstallOperations();
    const auto rejects = [](const SoundBlasterClockPhase& phase, const std::vector<NormalWorkOperation>& operations) {
        SoundBlasterSynth synth;
        PresentationClock clock(synth);
        bool rejected = false;
        try {
            clock.renderWork(phase, operations, 1000);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected && clock.elapsedSamples() == 0 && synth.driverState().normalClockCycles == 0);
    };
    for (const auto missing : {NormalWorkKind::cli, NormalWorkKind::timerInstallPitHigh,
        NormalWorkKind::timerInstallVector}) {
        auto invalid = original;
        const auto operation = std::find_if(invalid.begin(), invalid.end(), [missing](const auto& item) {
            return item.kind == missing;
        });
        assert(operation != invalid.end());
        operation->kind = missing == NormalWorkKind::timerInstallPitHigh
            ? NormalWorkKind::port8Write : NormalWorkKind::instructions;
        rejects(initial, invalid);
    }
    auto invalid = original;
    const auto low = std::find_if(invalid.begin(), invalid.end(), [](const auto& item) {
        return item.kind == NormalWorkKind::timerInstallPitLow;
    });
    low->value ^= 1;
    rejects(initial, invalid);
    auto invalidPhase = initial;
    invalidPhase.bios->domain = AudioClockBiosDomain::unsupported;
    rejects(invalidPhase, original);
    invalidPhase = initial;
    invalidPhase.timerInstall.reset();
    rejects(invalidPhase, original);
    invalidPhase = initial;
    invalidPhase.devices->pitCycleBase = 1;
    rejects(invalidPhase, original);
    invalidPhase = initial;
    invalidPhase.timerInstall->masterPicMask |= 1;
    rejects(invalidPhase, original);
    invalidPhase = initial;
    invalidPhase.interrupts = AudioClockInterrupts::masked;
    rejects(invalidPhase, original);
    invalidPhase = initial;
    invalidPhase.pitPeriodMilliseconds /= 2;
    rejects(invalidPhase, original);
    invalidPhase = initial;
    invalidPhase.events.erase(std::find_if(invalidPhase.events.begin(), invalidPhase.events.end(), [](const auto& event) {
        return event.kind == AudioClockEventKind::pit;
    }));
    rejects(invalidPhase, original);
    invalidPhase = initial;
    invalidPhase.events.push_back({initial.pitPeriodMilliseconds + 1, AudioClockEventKind::pit, 0});
    rejects(invalidPhase, original);
    assert(initial.timerInstall->stage == AudioClockTimerInstallStage::fresh && initial.timer->biosCount == 0);
}

void assertTimerInstallContinuity() {
    using namespace stellar7;
    SoundBlasterSynth synth;
    PresentationClock clock(synth);
    auto retained = nativeTimerInstallPhase();
    // Declared work control crosses the old BIOS timer edge without installing early.
    for (int index = 0; index < 3; ++index) {
        auto work = clock.renderWork(retained, {{NormalWorkKind::instructions, 100000}}, 120000);
        retained = work.timing.phase;
        assert(work.timing.callbacks.empty() && work.audio.writes.empty());
    }
    assert(retained.bios->timerTicks == 1 && retained.timer->biosCount == 0);
    assert(retained.timerInstall->stage == AudioClockTimerInstallStage::fresh);
    assert(retained.interruptDelivery == AudioClockInterruptDelivery::defaultBiosIrq0);
    assert(retained.timerInstall->irq0InputHigh && !retained.devices->irq0Requested);
    retained.timer->globalCount = 7; // Controlled prior source storage; installer must preserve it.
    retained.timer->callbackCount = 0xffff;
    retained.timer->callbackReload = 7;
    const auto installed = clock.renderWork(retained, timerInstallOperations(), 1000);
    assert(installed.timing.total.dispatches == 67 && installed.timing.total.delayCycles == 12);
    assert(installed.timing.phase.bios->timerTicks == 1 && installed.timing.phase.timer->biosCount == 13);
    assert(installed.timing.phase.timer->globalCount == 7);
    assert(installed.timing.phase.timer->callbackCount == 0xffff && installed.timing.phase.timer->callbackReload == 7);
    assert(installed.timing.phase.timerInstall->stage == AudioClockTimerInstallStage::installed);
    assert(installed.timing.phase.interruptDelivery == AudioClockInterruptDelivery::residentTimerIrq0);
    // The newly programmed counter's own next edge now uses the resident owner.
    const auto adjacent = clock.renderWork(installed.timing.phase, {{NormalWorkKind::instructions, 23000}}, 30000);
    assert(adjacent.timing.total.dispatches == 23000 + 38 && adjacent.timing.total.delayCycles == 3);
    assert(adjacent.timing.phase.timer->biosCount == 12 && adjacent.timing.phase.bios->timerTicks == 1);
    assert(adjacent.timing.phase.timer->globalCount == 6);
    assert(adjacent.timing.phase.timer->callbackCount == 0xffff && adjacent.timing.phase.timer->callbackReload == 7);
    assert(adjacent.audio.firstSample == installed.audio.firstSample + installed.audio.samples.size());
    assert(adjacent.timing.phase.timerInstall->stage == AudioClockTimerInstallStage::installed);
    assert(!adjacent.timing.phase.devices->irq0InService && !adjacent.timing.phase.devices->irq0Requested);
}
}

int main() {
    using namespace stellar7;
    try {
        assertEmptyTimerIrq();
        assertRetainedEmptyTimerClock();
        assertTimerInstallation();
        assertTimerInstallGuards();
        assertTimerInstallContinuity();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Empty timer IRQ: %s\n", error.what());
        return 1;
    }
    auto p = phase();
    // A BIOS tail is now supported only with the explicit retained-state proof.
    auto irq = p;
    irq.interrupts = AudioClockInterrupts::enabled;
    irq.interruptDelivery = AudioClockInterruptDelivery::residentTimerIrq0;
    irq.devices->irq0InService = false;
    irq.devices->irq0Requested = true;
    irq.irq0ActivationPossible = true;
    irq.remainingCycles = 1;
    irq.deferredCycles = 4999;
    irq.timer->biosCount = 1;
    const auto joined = normalWorkTiming(irq, {{NormalWorkKind::instructions, 100}}, 1000);
    assert(joined.phase.bios->timerTicks == 1235);
    assert(joined.phase.timer->biosCount == 5);
    assert(joined.total.dispatches == 160 && joined.total.delayCycles == 3);

    const auto result = biosChainTiming(p);
    assert(result.checkpoints.size() == 7);
    assert(result.checkpoints.front().boundary == BiosClockBoundary::preInt8);
    assert(result.checkpoints.front().phase.remainingCycles == 1);
    assert(result.checkpoints.front().phase.deferredCycles == 998);
    assert(result.checkpoints[1].phase.bios->timerTicks == 1235);
    assert(result.checkpoints[1].timing.terminalCharges == 0);
    assert(result.checkpoints[3].phase.interrupts == AudioClockInterrupts::masked);
    assert(result.checkpoints[4].phase.interrupts == AudioClockInterrupts::enabled);
    assert(result.checkpoints.back().boundary == BiosClockBoundary::preIret);
    assert(result.checkpoints.back().timing.dispatches == 14);
    assert(result.checkpoints.back().phase.interrupts == AudioClockInterrupts::masked);
    assert(!result.checkpoints.back().phase.devices->irq0InService);
    assert(result.total.dispatches == 15 && result.total.delayCycles == 3);
    assert(result.total.elapsedCycles == 18 && result.total.terminalCharges == 0);
    assert(result.phase.interrupts == AudioClockInterrupts::enabled);
    assert(result.phase.interruptDelivery == AudioClockInterruptDelivery::residentTimerIrq0);
    assert(p.bios->timerTicks == 1234 && p.devices->irq0InService);

    // Source final IRET returns to PIC_RunQueue when an IRQ check is pending.
    auto recheck = phase();
    recheck.irqCheckRequested = false;
    recheck.events = {{0.803, AudioClockEventKind::irqCheck, 0}};
    recheck.irqPending = true;
    const auto checked = biosChainTiming(recheck);
    assert(!checked.phase.irqCheckRequested && !checked.phase.irqPending);
    assert(checked.total.dispatches == 15);
    assert(checked.checkpoints.back().phase.irqCheckRequested);
    assert(checked.total.terminalCharges == checked.checkpoints.back().timing.terminalCharges);
    assert(checked.phase.interrupts == AudioClockInterrupts::enabled);

    auto missing = phase(); missing.bios.reset(); assert(reject(missing));
    missing = phase(); missing.bios->domain = AudioClockBiosDomain::unsupported; assert(reject(missing));
    missing = phase(); missing.bios->keyboardLeds = 0; assert(reject(missing));
    missing = phase(); missing.bios->syncTime = true; missing.bios->cmosSyncPending = true; assert(reject(missing));
    missing = phase(); missing.bios->floppyTimeout = 1; assert(reject(missing));
    missing = phase(); missing.bios->motorBits = 1; assert(reject(missing));
    missing = phase(); missing.bios->timerTicks = 0x1800af; assert(reject(missing));
    missing = irq; missing.bios.reset();
    bool rejected = false;
    try { normalWorkTiming(missing, {{NormalWorkKind::instructions, 100}}, 1000); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    missing = irq; missing.timer->biosReload = 0; rejected = false;
    try { normalWorkTiming(missing, {{NormalWorkKind::instructions, 100}}, 1000); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);

    auto callback = irq;
    callback.timer->callbackCount = 1;
    callback.callback = SoundBlasterCallbackState{};
    callback.callback->wait = 2;
    const auto composed = normalWorkTiming(callback, {{NormalWorkKind::instructions, 100}}, 1000);
    assert(composed.phase.bios->timerTicks == 1235);
    assert(composed.callbacks.size() == 1 && composed.phase.callback->wait == 1);
    assert(composed.phase.timer->callbackCount == 20 && composed.phase.timer->biosCount == 5);
    assert(composed.total.dispatches == 100 + 47 + 73 + 15 && composed.total.delayCycles == 3);
}
