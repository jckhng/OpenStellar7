#include "stellar7/presentation_clock.hpp"
#include "stellar7/timer_install_work.hpp"
#include "stellar7/timer_registration_work.hpp"
#include "stellar7/timer_work.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <tuple>

namespace {
using namespace stellar7;

auto phaseState(const SoundBlasterClockPhase& phase) {
    return std::make_tuple(phase.remainingCycles, phase.deferredCycles, phase.interrupts,
        phase.linesDone, phase.devices->picTicks, phase.devices->pitStartMilliseconds,
        phase.devices->pitNowMilliseconds, phase.devices->frameStartMilliseconds,
        phase.timer->globalCount, phase.timer->biosCount, phase.timer->callbackCount,
        phase.timer->callbackReload, phase.timer->activeMask, phase.bios->timerTicks,
        phase.timerInstall->registration, phase.timerInstall->callbackOffset,
        phase.timerInstall->callbackSegment, phase.devices->irq0Requested, phase.devices->irq0InService);
}

void assertSamePhase(const SoundBlasterClockPhase& first, const SoundBlasterClockPhase& second) {
    assert(phaseState(first) == phaseState(second));
    assert(first.events.size() == second.events.size());
    for (std::size_t index = 0; index < first.events.size(); ++index) {
        const auto& a = first.events[index];
        const auto& b = second.events[index];
        assert(a.kind == b.kind && a.value == b.value && a.deadlineMilliseconds == b.deadlineMilliseconds);
    }
}

void retainedNoHandleCallbacks() {
    SoundBlasterSynth joined;
    SoundBlasterSynth split;
    PresentationClock joinedClock(joined);
    PresentationClock splitClock(split);
    const auto initial = nativeTimerInstallPhase();
    const auto installed = joinedClock.renderWork(initial, timerInstallOperations(), 1000);
    auto retained = splitClock.renderWork(initial, timerInstallOperations(), 1000).timing.phase;
    const auto program = timerRegistrationOperations();
    const auto registered = joinedClock.renderWork(installed.timing.phase, program, 1000);
    assert(registered.timing.total.dispatches == 29 && registered.timing.total.delayCycles == 0);
    assert(registered.timing.phase.timer->activeMask == 1 && registered.timing.phase.timer->callbackCount == 4);
    assert(registered.timing.phase.timer->globalCount == installed.timing.phase.timer->globalCount);
    assert(registered.timing.phase.timer->biosCount == installed.timing.phase.timer->biosCount);
    assert(registered.timing.phase.timerInstall->callbackOffset == 0x0494);
    assert(registered.timing.phase.timerInstall->callbackSegment == 0x1ebc);
    assert(registered.timing.phase.callback->domain == SoundBlasterCallbackDomain::noHandle);
    assert(registered.timing.phase.callback->sequence.empty() && !registered.timing.phase.callback->active);
    std::vector<std::int16_t> packetPcm;
    for (const auto& operation : program) {
        for (std::uint64_t index = 0; index < operation.count; ++index) {
            const auto part = splitClock.renderWork(retained, {{operation.kind, 1, operation.value}}, 1000);
            packetPcm.insert(packetPcm.end(), part.audio.samples.begin(), part.audio.samples.end());
            retained = part.timing.phase;
        }
    }
    assert(packetPcm == registered.audio.samples);
    assertSamePhase(retained, registered.timing.phase);

    constexpr std::uint64_t foregroundDispatches = 90000;
    const auto whole = joinedClock.renderWork(registered.timing.phase,
        {{NormalWorkKind::instructions, foregroundDispatches}}, 100000);
    assert(whole.timing.callbacks.size() == 1);
    const auto& callback = whole.timing.callbacks.front();
    assert(callback.advance.kind == SoundBlasterCallbackKind::noHandle && callback.advance.dispatches == 58);
    assert(callback.timing.opl.empty() && callback.timing.interruptBoundaries.empty());
    assert(whole.audio.writes.empty() && whole.timing.phase.callback->sequence.empty());
    assert(whole.timing.phase.timer->callbackCount == 4);
    assert(whole.timing.phase.timer->biosCount == registered.timing.phase.timer->biosCount - 4);
    const auto expectedDispatches = foregroundDispatches + 58
        + 3 * timerIrqWrapperDispatches(1, 0, TimerIrqExit::localIret)
        + timerIrqWrapperDispatches(1, 1, TimerIrqExit::localIret);
    assert(whole.timing.total.dispatches == expectedDispatches && whole.timing.total.delayCycles == 12);
    packetPcm.clear();
    std::vector<std::int64_t> callbackPositions;
    std::int64_t elapsed = 0;
    for (std::uint64_t index = 0; index < foregroundDispatches / 1000; ++index) {
        const auto part = splitClock.renderWork(retained, {{NormalWorkKind::instructions, 1000}}, 2000);
        packetPcm.insert(packetPcm.end(), part.audio.samples.begin(), part.audio.samples.end());
        for (const auto& delivered : part.timing.callbacks) {
            callbackPositions.push_back(elapsed + delivered.cycleOffset);
        }
        elapsed += part.timing.total.elapsedCycles;
        retained = part.timing.phase;
    }
    assert(packetPcm == whole.audio.samples && splitClock.elapsedSamples() == joinedClock.elapsedSamples());
    assert(elapsed == whole.timing.total.elapsedCycles && callbackPositions.size() == 1);
    assert(callbackPositions.front() == callback.cycleOffset);
    assertSamePhase(retained, whole.timing.phase);
    assert(split.driverState().normalClockCycles == joined.driverState().normalClockCycles);
    assert(joined.registerWrites().size() == 158 && split.registerWrites().size() == 158);
}

void pendingRegistrationIrq() {
    // Controlled pending IRQ at registration, not an original system-phase claim.
    const auto installed = normalWorkTiming(nativeTimerInstallPhase(), timerInstallOperations(), 1000);
    const auto program = timerRegistrationOperations();
    auto phase = installed.phase;
    SoundBlasterSynth synth;
    phase.callback = synth.callbackWorkState();
    std::vector<NormalWorkOperation> prefix;
    for (const auto& operation : program) {
        if (operation.kind == NormalWorkKind::timerCallbackActivate) {
            break;
        }
        prefix.push_back(operation);
    }
    phase = normalWorkTiming(phase, prefix, 1000).phase;
    assert(phase.interrupts == AudioClockInterrupts::masked && phase.timer->activeMask == 0);
    phase.devices->irq0Requested = true;
    phase.irqCheckRequested = true;
    const auto activated = normalWorkTiming(phase, {{NormalWorkKind::timerCallbackActivate, 1, 1}}, 1000);
    assert(activated.phase.timer->callbackCount == 4 && activated.phase.devices->irq0Requested);
    const auto sti = normalWorkTiming(activated.phase, {{NormalWorkKind::sti, 1}}, 1000);
    assert(sti.phase.timer->callbackCount == 4 && sti.phase.devices->irq0Requested);
    const auto shadow = normalWorkTiming(sti.phase, {{NormalWorkKind::instructions, 1}}, 1000);
    assert(shadow.phase.timer->callbackCount == 3 && !shadow.phase.devices->irq0Requested);
    assert(shadow.total.dispatches == 1 + timerIrqWrapperDispatches(1, 0, TimerIrqExit::localIret));
    assert(shadow.callbacks.empty());
}

void registrationGuards() {
    const auto installed = normalWorkTiming(nativeTimerInstallPhase(), timerInstallOperations(), 1000);
    SoundBlasterSynth synth;
    auto phase = installed.phase;
    phase.callback = synth.callbackWorkState();
    const auto program = timerRegistrationOperations();
    const auto rejects = [&](const SoundBlasterClockPhase& incoming, const std::vector<NormalWorkOperation>& operations) {
        try {
            normalWorkTiming(incoming, operations, 1000);
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    assert(rejects(nativeTimerInstallPhase(), program));
    assert(rejects(installed.phase, program));
    auto missingOwner = phase;
    missingOwner.timerInstall.reset();
    assert(rejects(missingOwner, {{NormalWorkKind::instructions, 1}}));
    missingOwner = phase;
    missingOwner.timer.reset();
    assert(rejects(missingOwner, {{NormalWorkKind::instructions, 1}}));
    assert(rejects(phase, {{NormalWorkKind::timerCallbackActivate, 1, 1}}));
    for (const auto kind : {NormalWorkKind::timerCallbackReload, NormalWorkKind::timerCallbackCount,
        NormalWorkKind::timerCallbackOffset, NormalWorkKind::timerCallbackSegment, NormalWorkKind::timerCallbackActivate}) {
        auto wrong = program;
        for (auto& operation : wrong) {
            if (operation.kind == kind) {
                operation.value ^= 1;
            }
        }
        assert(rejects(phase, wrong));
    }
    const auto registered = normalWorkTiming(phase, program, 1000);
    assert(rejects(registered.phase, program));
    auto forged = registered.phase;
    forged.timerInstall->callbackOffset = 0;
    assert(rejects(forged, {{NormalWorkKind::instructions, 1}}));
    forged = registered.phase;
    forged.timerInstall->registration = AudioClockCallbackRegistrationStage::fresh;
    assert(rejects(forged, {{NormalWorkKind::instructions, 1}}));
}
}

int main() {
    retainedNoHandleCallbacks();
    pendingRegistrationIrq();
    registrationGuards();
}
