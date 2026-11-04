#include "stellar7/timer_install_work.hpp"

#include <algorithm>
#include <cmath>

namespace stellar7 {
namespace {

constexpr double pitFrequency = 1193182;
constexpr double pixelFrequency = 25175000;
constexpr double characterPixels = 8;
constexpr double horizontalCharacters = 100;
constexpr double verticalLines = 449;
constexpr std::uint32_t displayLines = 400;
constexpr double retraceStartLine = 412;
constexpr double retraceEndLine = 414;
constexpr std::uint32_t verticalRetraceEndMask = 0x0f;
constexpr std::uint32_t crtcWriteProtection = 0x80;
constexpr double verticalInterruptDelayMilliseconds = 0.005;
constexpr std::int64_t cyclesPerMillisecond = 5000;
constexpr std::uint64_t pitFrequencyHertz = 1193182;
constexpr std::uint64_t nanosecondsPerSecond = 1000000000;
constexpr double nanosecondsPerMillisecond = 1000000;
constexpr std::uint32_t s3VideoMemoryBytes = 2 * 1024 * 1024;
constexpr std::uint16_t biosPeriod = 13;
constexpr std::uint16_t installedDivisor = 0xffff / biosPeriod;

}  // namespace

SoundBlasterClockPhase nativeTimerInstallPhase() {
    SoundBlasterClockPhase phase{};
    const auto characterFrequency = pixelFrequency / characterPixels;
    const auto linePeriod = horizontalCharacters * 1000 / characterFrequency;
    const auto framesPerSecond = characterFrequency / (horizontalCharacters * verticalLines);
    const auto framePeriod = 1000 / framesPerSecond;
    phase.interrupts = AudioClockInterrupts::enabled;
    phase.isa8DelayNanoseconds = 1020;
    phase.pitPeriodMilliseconds = 65536 * 1000 / pitFrequency;
    phase.irqDelayMilliseconds = (nanosecondsPerSecond / pitFrequencyHertz)
        / nanosecondsPerMillisecond;
    phase.irq0ActivationPossible = true;
    phase.singleLinePeriodMilliseconds = static_cast<float>(linePeriod);
    phase.horizontalPeriodMilliseconds = linePeriod;
    phase.linesTotal = displayLines;
    phase.frameStart = AudioClockFrameStart::activeUnlockedVgaSurface;
    phase.interruptDelivery = AudioClockInterruptDelivery::defaultBiosIrq0;
    phase.vgaInterrupt = AudioClockVgaInterrupt::fixedVgaNoDemand;
    phase.devices.emplace();
    auto& devices = *phase.devices;
    devices.fixedPitAndMaskedDma = true;
    devices.framePeriodMilliseconds = framePeriod;
    devices.displayEndMilliseconds = linePeriod * displayLines;
    devices.retraceStartMilliseconds = linePeriod * retraceStartLine;
    devices.retraceEndMilliseconds = linePeriod * retraceEndLine;
    devices.verticalRetraceEndRegister = (static_cast<std::uint32_t>(retraceEndLine)
        & verticalRetraceEndMask) | crtcWriteProtection;
    devices.memoryMask = s3VideoMemoryBytes - 1;
    devices.frame = AudioClockVgaFramePhase{framesPerSecond, 0, 0};
    phase.timer.emplace();
    phase.timerInstall.emplace();
    phase.bios.emplace();
    phase.bios->domain = AudioClockBiosDomain::normalVgaDefaultTimer;

    // Backend epoch: existing PIT, then vertical timer's frame/latches/IRQ/line.
    phase.events = {
        {phase.pitPeriodMilliseconds, AudioClockEventKind::pit, 0},
        {static_cast<float>(framePeriod), AudioClockEventKind::frameStart, 0},
        {static_cast<float>(devices.retraceStartMilliseconds), AudioClockEventKind::displayStartLatch, 0},
        {static_cast<float>(devices.retraceEndMilliseconds), AudioClockEventKind::panningLatch, 0},
        {static_cast<float>(devices.displayEndMilliseconds + verticalInterruptDelayMilliseconds),
            AudioClockEventKind::pendingVgaInterrupt, 0},
        {static_cast<float>(linePeriod / 4), AudioClockEventKind::scanline, 0}
    };
    phase.minimumLatchDelayMilliseconds = std::min({
        static_cast<float>(devices.retraceStartMilliseconds),
        static_cast<float>(devices.retraceEndMilliseconds),
        static_cast<float>(devices.displayEndMilliseconds + verticalInterruptDelayMilliseconds)
    });
    std::stable_sort(phase.events.begin(), phase.events.end(), [](const auto& left, const auto& right) {
        return left.deadlineMilliseconds < right.deadlineMilliseconds;
    });
    phase.remainingCycles = static_cast<std::int64_t>(std::floor(
        phase.events.front().deadlineMilliseconds * cyclesPerMillisecond));
    phase.deferredCycles = cyclesPerMillisecond - phase.remainingCycles;
    return phase;
}

std::vector<NormalWorkOperation> timerInstallOperations() {
    std::vector<NormalWorkOperation> work;
    const auto instruction = [&]() {
        work.push_back({NormalWorkKind::instructions, 1});
    };
    const auto effect = [&](const NormalWorkKind kind, const std::uint16_t value = 0) {
        work.push_back({kind, 1, value});
    };

    // 0824:41AA,41AD,41AE: period argument and far CALL.
    instruction(); instruction(); instruction();
    // 1764:271E,271F,2721,2723,2727; fresh installed byte is zero.
    instruction(); instruction(); instruction(); instruction(); instruction();
    effect(NormalWorkKind::timerInstallClearMask); // 2729
    instruction(); // 272C far CALL firmware probe

    // 2891,2892,2893,2896,2898,289B,289E,28A0.
    // Native ROM policy F000:FFFE=FC != FF skips the second probe.
    instruction(); instruction(); instruction(); instruction();
    instruction(); instruction(); instruction(); instruction();
    // 28B1,28B4,28B5,28B6,28B7; native [1FEC]=0.
    instruction(); instruction(); instruction(); instruction(); instruction();
    instruction(); // 2731 MOV AX,3508
    effect(NormalWorkKind::timerInstallSaveBiosVector); // 2734 INT21 plus default guest stub
    // 2736,273B: preserve old vector; 2740,2742,2744,2747,274B,
    // 274D,274F,2751: source argument guards and numerator preparation.
    instruction(); instruction(); instruction(); instruction(); instruction();
    instruction(); instruction(); instruction(); instruction(); instruction();
    effect(NormalWorkKind::timerInstallBiosReload, biosPeriod); // 2752
    effect(NormalWorkKind::timerInstallBiosCount, biosPeriod); // 2756
    instruction(); instruction(); // 275A DIV BX; 275C MOV BX,AX
    effect(NormalWorkKind::timerInstallDivisor, installedDivisor); // 275E
    effect(NormalWorkKind::cli); // 2761
    instruction(); // 2762
    effect(NormalWorkKind::timerInstallPitControl, 0x36); // 2764
    instruction(); // 2766
    effect(NormalWorkKind::timerInstallPitLow, installedDivisor & 0xff); // 2768
    instruction(); // 276A
    effect(NormalWorkKind::timerInstallPitHigh, installedDivisor >> 8); // 276C
    instruction(); // 276E
    effect(NormalWorkKind::timerInstallPicMask, 0x28); // 2770
    // 2772,2773,2775,2777,277A: DS:DX=1764:27C0, AX=2508.
    instruction(); instruction(); instruction(); instruction(); instruction();
    effect(NormalWorkKind::timerInstallVector); // 277D INT21 plus default guest stub
    instruction(); // 277F POP DS
    effect(NormalWorkKind::sti); // 2780
    instruction(); // 2781 MOV AX,1; STI shadow ends here
    effect(NormalWorkKind::timerInstallComplete, 1); // 2784
    instruction(); instruction(); // 2787 POP BP; 2788 RETF -> 0824:41B3
    return work;
}

}  // namespace stellar7
