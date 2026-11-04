#include "stellar7/level2_music_work.hpp"
#include "stellar7/audio_repeat_work.hpp"
#include "stellar7/audio_clock.hpp"
#include "stellar7/normal_clock.hpp"
#include "stellar7/page_flip_clock.hpp"
#include "stellar7/palette_clock.hpp"
#include "playable_decoy.hpp"
#include "stellar7/success_palette_clock.hpp"
#include "stellar7/success_palette_step.hpp"
#include "stellar7/audio_direct_work.hpp"
#include "stellar7/audio_program_work.hpp"
#include "keyboard_clock_access.hpp"
#include "retained_level1_clock_domain.hpp"
#include "retained_menu_clock_domain.hpp"
#include "playable_startup_clock_domain.hpp"
#include "stellar7/success_audio_work.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace stellar7 {
namespace {

constexpr std::int64_t cyclesPerMillisecond = 5000;
constexpr std::uint16_t maximumMusicVolume = 15;
constexpr std::int64_t writerHorizonCycles = 600;
constexpr std::int64_t biosHorizonCycles = 600;
constexpr std::uint32_t biosMidnightTicks = 0x1800b0;
constexpr std::int64_t callbackHorizonCycles = 2000;
constexpr std::uint64_t maximumNoteOnEvents = 32;
constexpr std::uint64_t maximumNoteOnPlainDispatchesPerEvent = 440;
constexpr std::uint64_t noteOnFixedPlainDispatches = 94;
constexpr std::uint64_t maximumKeyOffEvents = 32;
constexpr std::uint64_t maximumKeyOffPlainDispatchesPerEvent = 252;
constexpr std::uint64_t keyOffFixedPlainDispatches = 88;
constexpr std::size_t maximumCallbackBlocks = 128;
constexpr std::size_t maximumProgramMessages = soundBlasterProgramEventLimit;
constexpr std::size_t maximumProgramWrites = maximumProgramMessages * 9 * 17;
constexpr std::size_t maximumProgramCallbackBlocks = maximumProgramWrites * 2 + 1;
// Per event: nine patch/level paths below1024 each, plus parser/scan/tail below256.
constexpr std::uint64_t maximumProgramPlainDispatches = maximumProgramMessages * (9 * 1024 + 256);
constexpr int soundBlasterVoiceCount = 9;
constexpr std::int64_t isa8ReadDelayCycles = 5;
constexpr std::int64_t isa8WriteDelayCycles = 3;
constexpr std::int64_t isa16WriteDelayCycles = 2;
constexpr std::uint64_t maximumRepTransfers = 4;
constexpr std::size_t maximumWorkOperations = 32768;
constexpr std::int64_t maximumNormalWorkHorizonCycles = 120000;
constexpr std::int64_t maximumProgramPageFlipHorizonCycles = 2 * maximumNormalWorkHorizonCycles;
// A music callback can carry the palette past one retrace window. This is only
// an execution guard; every dispatch, port delay and retrace read remains live.
constexpr std::int64_t maximumProgramPaletteHorizonCycles = 2 * maximumNormalWorkHorizonCycles;
constexpr std::int64_t adlibReadDelayCycles = cyclesPerMillisecond / 2048;
constexpr std::uint32_t isa8DelayNanoseconds = 1020;
constexpr std::size_t maximumQueuedEvents = 8192;
constexpr std::size_t maximumServicesPerSlice = 8192;
constexpr std::size_t timerIrqPrefixDispatches = 43;
constexpr std::size_t timerIrqCallbackEntryDispatches = 26;
constexpr std::size_t timerIrqCallbackExitDispatches = 19;
// 287d reloads the BIOS counter, restores nine registers and tail-jumps.
constexpr std::size_t timerIrqBiosPrefixDispatches = 45;
constexpr std::size_t timerIrqBiosCallbackExitDispatches = 21;
constexpr std::uint16_t soundBlasterCallbackMask = 1;
constexpr std::uint16_t nativeCallbackPeriod = 4;
constexpr std::uint16_t nativeCallbackOffset = 0x0494;
constexpr std::uint16_t nativeCallbackSegment = 0x1ebc;
constexpr std::uint16_t installedBiosReload = 13;
// Empty mask jumps from 27e5 to 2866 without accessing callback slots.
constexpr std::size_t emptyTimerIrqPrefixDispatches = 35;
constexpr std::size_t emptyTimerIrqBiosPrefixDispatches = 37;
constexpr std::uint16_t installedPitDivisor = 0xffff / installedBiosReload;
constexpr double pitInputFrequency = 1193182;
constexpr std::uint32_t biosPitDivisor = 65536;
constexpr std::uint8_t nativeInitialPicMask = 0xf8;
constexpr std::uint8_t installedPicMask = 0x28;
constexpr double frameDriftFraction = 0.25;
constexpr double maximumFrameDriftMilliseconds = 0.1;
constexpr std::uint32_t maximumFrameCount = 0x7fffffff;
constexpr std::int64_t legacyDeviceHorizonCycles = 15000;
constexpr double rtcSecondMilliseconds = 1000;
constexpr double rtcTickToleranceMilliseconds = 0.001;
constexpr double rtcMinimumDelayMilliseconds = 0.01;
constexpr std::uint8_t rtcInterruptFlags = 0x70;

void validateRtc(const SoundBlasterClockPhase& phase) {
    const auto& rtc = *phase.rtc;
    const auto& date = rtc.calendar;
    if (rtc.domain != AudioClockRtcDomain::inactiveNoAccess || !phase.devices || !phase.bios
        || phase.bios->domain != AudioClockBiosDomain::normalVgaDefaultTimer
        || phase.bios->syncTime || phase.bios->cmosSyncPending
        || (rtc.registerB & rtcInterruptFlags) != 0 || (rtc.registerC & rtcInterruptFlags) != 0
        || !std::isfinite(rtc.endedMilliseconds) || rtc.endedMilliseconds < 0 || !std::isfinite(rtc.delayMilliseconds)
        || rtc.delayMilliseconds <= 0 || rtc.divider > 15 || rtc.selected > 127
        || date[0] >= 60 || date[1] >= 60 || date[2] >= 24 || date[3] < 1 || date[3] > 7
        || date[4] < 1 || date[4] > 31 || date[5] < 1 || date[5] > 12) {
        throw std::invalid_argument("unsupported inactive RTC phase");
    }
    if (std::count_if(phase.events.begin(), phase.events.end(), [](const auto& event) {
        return event.kind == AudioClockEventKind::pendingCmos;
    }) != 1) {
        throw std::invalid_argument("inactive RTC requires one retained event owner");
    }
}

void tickRtc(AudioClockRtcPhase& rtc) {
    if (rtc.clockTime == std::numeric_limits<std::int64_t>::max()) {
        throw std::invalid_argument("RTC calendar time overflow");
    }
    ++rtc.clockTime;
    auto& date = rtc.calendar;
    if (++date[0] < 60) { return; }
    date[0] = 0;
    if (++date[1] < 60) { return; }
    date[1] = 0;
    if (++date[2] < 24) { return; }
    date[2] = 0;
    if (++date[3] > 7) { date[3] = 1; }
    constexpr std::array<std::uint16_t,13> monthDays{0,31,28,31,30,31,30,31,31,30,31,30,31};
    auto days = monthDays[date[5]];
    if (date[5] == 2 && date[6] % 4 == 0 && (date[6] % 100 != 0 || date[6] % 400 == 0)) {
        ++days;
    }
    // Preserve the pinned source's strict carry comparisons.
    if (++date[4] < days) { return; }
    date[4] = 1;
    if (++date[5] < 12) { return; }
    date[5] = 1;
    date[6] = static_cast<std::uint16_t>(date[6] + 1);
}

enum class Operation { plain, read, statusRead, write, wordWrite };
enum class EntryPolicy { masked, retained };
constexpr std::size_t keyOffEnableTailDispatches = 18;
constexpr std::size_t keyOffEnableWindowDispatches = 8;
constexpr std::size_t keyOffRestoreTailDispatches = 8;

bool programCallback(const SoundBlasterCallbackKind kind) {
    return kind == SoundBlasterCallbackKind::programControl || kind == SoundBlasterCallbackKind::programWait
        || kind == SoundBlasterCallbackKind::successNoHandle
        || kind == SoundBlasterCallbackKind::level2RetryNoHandle;
}

bool callbackEnableTail(const SoundBlasterCallbackKind kind) {
    return kind == SoundBlasterCallbackKind::explicitKeyOff || kind == SoundBlasterCallbackKind::explicitNoteOn
        || kind == SoundBlasterCallbackKind::programControl;
}

std::int64_t callbackWorkHorizon(const std::vector<SoundBlasterWorkBlock>& blocks,
    const SoundBlasterCallbackKind kind) {
    if (kind == SoundBlasterCallbackKind::noHandle) {
        if (blocks.size() != 1 || blocks.front().kind != SoundBlasterWorkKind::plain
            || blocks.front().dispatches != soundBlasterNoHandleDispatches || blocks.front().voice != -1) {
            throw std::invalid_argument("invalid zero-timer no-handle callback work");
        }
        return callbackHorizonCycles;
    }
    if (kind == SoundBlasterCallbackKind::wait) {
        return callbackHorizonCycles;
    }
    if (programCallback(kind)) {
        if (blocks.empty() || blocks.size() > maximumProgramCallbackBlocks || blocks.size() % 2 == 0) {
            throw std::invalid_argument("invalid bounded program/control callback work");
        }
        std::uint64_t plain = 0;
        std::uint64_t writers = 0;
        for (std::size_t index = 0; index < blocks.size(); ++index) {
            const auto& block = blocks[index];
            if (index % 2 == 0) {
                if (block.kind != SoundBlasterWorkKind::plain || block.dispatches > maximumProgramPlainDispatches - plain) {
                    throw std::invalid_argument("program/control plain work exceeds source family");
                }
                plain += block.dispatches;
            } else if (block.kind != SoundBlasterWorkKind::registerWrite || ++writers > maximumProgramWrites) {
                throw std::invalid_argument("program/control writer work exceeds source family");
            }
        }
        const auto opl = soundBlasterOplWork();
        const auto delay = opl.portReads * (isa8ReadDelayCycles + adlibReadDelayCycles)
            + opl.portWrites * isa8WriteDelayCycles;
        return static_cast<std::int64_t>(2 * (plain + writers * opl.dispatches) + writers * delay + 1);
    }
    if (kind != SoundBlasterCallbackKind::explicitNoteOn && kind != SoundBlasterCallbackKind::explicitKeyOff) {
        throw std::invalid_argument("unsupported bounded callback work family");
    }
    if (blocks.empty() || blocks.size() > maximumCallbackBlocks || blocks.size() % 2 == 0) {
        throw std::invalid_argument("invalid ordered note callback work blocks");
    }
    // Key-off includes at most eight unmatched scans before the ninth voice.
    const auto maximumPlain = kind == SoundBlasterCallbackKind::explicitNoteOn
        ? noteOnFixedPlainDispatches + maximumNoteOnEvents * maximumNoteOnPlainDispatchesPerEvent
        : keyOffFixedPlainDispatches + maximumKeyOffEvents * maximumKeyOffPlainDispatchesPerEvent;
    std::uint64_t plain = 0;
    std::uint64_t writers = 0;
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        const auto& block = blocks[index];
        if ((block.kind == SoundBlasterWorkKind::plain) != (index % 2 == 0)) {
            throw std::invalid_argument("note callback work must alternate plain and writer blocks");
        }
        if (block.kind == SoundBlasterWorkKind::plain) {
            if (block.dispatches > maximumPlain - plain) {
                throw std::invalid_argument("note callback plain work exceeds source family");
            }
            plain += block.dispatches;
        } else {
            ++writers;
        }
    }
    const auto opl = soundBlasterOplWork();
    const auto dispatches = plain + writers * opl.dispatches;
    const auto writerDelay = opl.portReads * (isa8ReadDelayCycles + adlibReadDelayCycles)
        + opl.portWrites * isa8WriteDelayCycles;
    // At most one terminal per dispatch, plus an unsettled incoming slice.
    return static_cast<std::int64_t>(2 * dispatches + writers * writerDelay + 1);
}

void validateBiosHost(const SoundBlasterClockPhase& phase) {
    if (!phase.bios || phase.bios->domain != AudioClockBiosDomain::normalVgaDefaultTimer) {
        throw std::invalid_argument("missing source-owned BIOS timer phase");
    }
    const auto& bios = *phase.bios;
    if (bios.timerTicks >= biosMidnightTicks - 1 || (bios.syncTime && bios.cmosSyncPending)
        || (bios.keyboardLedSync && ((bios.keyboardState >> 4) & 7) != (bios.keyboardLeds & 7))
        || bios.floppyTimeout != 0 || (bios.motorBits & 0x0f) != 0) {
        throw std::invalid_argument("unsupported BIOS timer host branch");
    }
}

void validateTimerInstall(const SoundBlasterClockPhase& phase) {
    if (!phase.timerInstall) {
        if (phase.interruptDelivery == AudioClockInterruptDelivery::defaultBiosIrq0) {
            throw std::invalid_argument("default BIOS delivery requires native installer ownership");
        }
        return;
    }
    using Stage = AudioClockTimerInstallStage;
    using Registration = AudioClockCallbackRegistrationStage;
    const auto& install = *phase.timerInstall;
    const bool registered = install.registration == Registration::active;
    const auto expectedPeriod = (install.stage < Stage::highByteWritten ? biosPitDivisor : installedPitDivisor)
        * 1000.0 / pitInputFrequency;
    const auto expectedPitEvents = install.stage == Stage::controlWritten || install.stage == Stage::lowByteWritten ? 0 : 1;
    if (!phase.devices || !phase.timer || phase.timer->activeMask != (registered ? soundBlasterCallbackMask : 0) || phase.keyboard
        || phase.devices->irq1Requested || phase.devices->irq1InService
        || install.stage < Stage::fresh || install.stage > Stage::installed
        || phase.pitPeriodMilliseconds != expectedPeriod
        || std::count_if(phase.events.begin(), phase.events.end(), [](const auto& event) {
            return event.kind == AudioClockEventKind::pit;
        }) != expectedPitEvents
        || std::any_of(phase.events.begin(), phase.events.end(), [](const auto& event) {
            return event.kind == AudioClockEventKind::pit && event.value != 0;
        })
        || install.masterPicMask != (install.stage < Stage::picMaskWritten ? nativeInitialPicMask : installedPicMask)
        || (install.stage >= Stage::divisorWritten && install.pitDivisor != installedPitDivisor)
        || (install.stage >= Stage::lowByteWritten && install.pitLowByte != (installedPitDivisor & 0xff))
        || (install.stage >= Stage::biosReloadWritten && phase.timer->biosReload != installedBiosReload)
        || (install.stage >= Stage::biosCountWritten
            && (phase.timer->biosCount == 0 || phase.timer->biosCount > installedBiosReload))) {
        throw std::invalid_argument("unsupported native timer installation phase");
    }
    if (install.registration < Registration::fresh || install.registration > Registration::active
        || (install.registration != Registration::fresh && install.stage != Stage::installed)
        || (install.registration >= Registration::reloadWritten && phase.timer->callbackReload != nativeCallbackPeriod)
        || (install.registration >= Registration::countWritten
            && (phase.timer->callbackCount == 0 || phase.timer->callbackCount > nativeCallbackPeriod))
        || (install.registration >= Registration::offsetWritten && install.callbackOffset != nativeCallbackOffset)
        || (install.registration >= Registration::segmentWritten && install.callbackSegment != nativeCallbackSegment)
        || (registered && !phase.callback)) {
        throw std::invalid_argument("unsupported native callback registration phase");
    }
    const auto owner = install.stage < Stage::vectorInstalled
        ? AudioClockInterruptDelivery::defaultBiosIrq0 : AudioClockInterruptDelivery::residentTimerIrq0;
    if (phase.interruptDelivery != owner && phase.interruptDelivery != AudioClockInterruptDelivery::blockedByIrq0) {
        throw std::invalid_argument("native timer vector ownership differs from installation stage");
    }
    validateBiosHost(phase);
}

class OplWorkClock;
enum class OplDataBoundary { settled, afterOut };
void oplAddress(OplWorkClock& clock, SoundBlasterClockStep& step);
SoundBlasterClockStep oplData(OplWorkClock& clock, SoundBlasterClockStep& step, std::int64_t start,
    OplDataBoundary boundary = OplDataBoundary::settled);
void executeCallbackWork(OplWorkClock& clock, const std::vector<SoundBlasterWorkBlock>& blocks,
    SoundBlasterCallbackKind kind, SoundBlasterCallbackClock& result, std::int64_t start);

class OplWorkClock {
public:
    explicit OplWorkClock(const SoundBlasterClockPhase& phase, const std::int64_t horizonCycles,
        const EntryPolicy entryPolicy = EntryPolicy::masked, KeyboardClockAccess* keyboardAccess = nullptr)
        : phase_(phase), horizonCycles_(horizonCycles), keyboardAccess_(keyboardAccess) {
        if (entryPolicy == EntryPolicy::masked && phase_.interrupts != AudioClockInterrupts::masked) {
            throw std::invalid_argument("OPL clock requires interrupts masked");
        }
        if ((phase_.interrupts != AudioClockInterrupts::masked && phase_.interrupts != AudioClockInterrupts::enabled)
            || (phase_.interrupts == AudioClockInterrupts::enabled && phase_.irqCheckRequested
                && phase_.interruptDelivery != AudioClockInterruptDelivery::blockedByIrq0
                && phase_.interruptDelivery != AudioClockInterruptDelivery::residentTimerIrq0
                && phase_.interruptDelivery != AudioClockInterruptDelivery::residentTimerKeyboard
                && phase_.interruptDelivery != AudioClockInterruptDelivery::defaultBiosIrq0)) {
            throw std::invalid_argument("unsupported normal clock entry interrupt domain");
        }
        const bool queuedEntry = phase_.keyboard && phase_.remainingCycles == 0 && phase_.deferredCycles > 0;
        if ((!queuedEntry && phase_.remainingCycles <= 0) || phase_.remainingCycles > cyclesPerMillisecond
            || phase_.deferredCycles < 0 || phase_.deferredCycles > cyclesPerMillisecond - phase_.remainingCycles
            || phase_.isa8DelayNanoseconds != isa8DelayNanoseconds) {
            throw std::invalid_argument("OPL clock outside calibrated normal5000/ISA8 budget");
        }
        if (!positive(phase_.singleLinePeriodMilliseconds) || !positive(phase_.horizontalPeriodMilliseconds)
            || !positive(phase_.pitPeriodMilliseconds) || !positive(phase_.minimumLatchDelayMilliseconds)
            || !std::isfinite(phase_.irqDelayMilliseconds) || phase_.irqDelayMilliseconds < 0
            || phase_.linesTotal == 0 || phase_.linesDone > phase_.linesTotal
            || phase_.events.size() > maximumQueuedEvents) {
            throw std::invalid_argument("invalid OPL clock device phase");
        }
        double previous = -1.0;
        for (const auto& event : phase_.events) {
            if (!std::isfinite(event.deadlineMilliseconds) || event.deadlineMilliseconds < 0
                || event.deadlineMilliseconds < previous
                || static_cast<int>(event.kind) < 0
                || static_cast<int>(event.kind) > static_cast<int>(AudioClockEventKind::keyboardTransfer)) {
                throw std::invalid_argument("invalid ordered OPL clock events");
            }
            previous = event.deadlineMilliseconds;
        }
        origin_ = position();
        if (phase_.devices) {
            const auto& devices = *phase_.devices;
            if (!devices.fixedPitAndMaskedDma || devices.pitCycleBase > 1
                || !std::isfinite(devices.pitStartMilliseconds) || !std::isfinite(devices.pitNowMilliseconds)
                || !positive(devices.framePeriodMilliseconds) || !positive(devices.displayEndMilliseconds)
                || !positive(devices.retraceStartMilliseconds) || !positive(devices.retraceEndMilliseconds)
                || !std::isfinite(devices.frameStartMilliseconds)
                || phase_.vgaInterrupt != AudioClockVgaInterrupt::fixedVgaNoDemand) {
                throw std::invalid_argument("unsupported retained clock device domain");
            }
            if (devices.frame && (!positive(devices.frame->framesPerSecond)
                || !std::isfinite(devices.frame->timeBaseMilliseconds)
                || devices.frame->framesSinceTimeBase > maximumFrameCount)) {
                throw std::invalid_argument("unsupported retained VGA frame phase");
            }
            if (!devices.frame && horizonCycles_ > legacyDeviceHorizonCycles) {
                throw std::invalid_argument("longer retained work requires VGA frame recurrence phase");
            }
        }
        if ((phase_.interruptDelivery == AudioClockInterruptDelivery::residentTimerIrq0
                || phase_.interruptDelivery == AudioClockInterruptDelivery::residentTimerKeyboard)
            && (!phase_.devices || !phase_.timer
                || (phase_.timer->activeMask != 0 && phase_.timer->activeMask != soundBlasterCallbackMask)
                || phase_.devices->irq0InService
                || (phase_.timer->activeMask == 0 && (phase_.timer->biosReload != installedBiosReload
                    || phase_.timer->biosCount == 0 || phase_.timer->biosCount > installedBiosReload)))) {
            throw std::invalid_argument("unsupported resident timer IRQ phase");
        }
        validateTimerInstall(phase_);
        if (phase_.soundBlasterInitialization) {
            const auto& initialization = *phase_.soundBlasterInitialization;
            if (!phase_.timerInstall || !phase_.timer || initialization.oplWrites > soundBlasterInitializationWriteCount
                || (initialization.driverEnabled && initialization.oplWrites != soundBlasterInitializationWriteCount)
                || (!initialization.driverEnabled && phase_.timer->activeMask != 0)
                || (phase_.callback && phase_.callback->driverEnabled != initialization.driverEnabled)) {
                throw std::invalid_argument("unsupported source Sound Blaster initialization phase");
            }
        }
        if (!phase_.playableCampaign && phase_.callback && phase_.callback->domain == SoundBlasterCallbackDomain::successNoHandle
            && !phase_.successAudio) {
            throw std::invalid_argument("retained no-current callback requires explicit success ownership");
        }
        if (!phase_.playableCampaign && !playableStartupProgram(phase_)
            && phase_.callback && phase_.callback->domain == SoundBlasterCallbackDomain::level2RetryNoHandle
            && (!phase_.level2RetryMusic || !phase_.callback->program || !phase_.sequenceSelection
                || phase_.sequenceSelection->songOrdinal != level2MusicIdentity
                || (phase_.level2RetryMusic->stage != AudioClockLevel2RetryMusicStage::stopped
                    && phase_.level2RetryMusic->stage != AudioClockLevel2RetryMusicStage::emptyStopped
                    && phase_.level2RetryMusic->stage != AudioClockLevel2RetryMusicStage::resourceBound))) {
            throw std::invalid_argument("Level2 retry no-current callback requires its stopped source owner");
        }
        if (phase_.callback && (phase_.callback->domain == SoundBlasterCallbackDomain::noHandle
                || phase_.callback->domain == SoundBlasterCallbackDomain::successNoHandle
                || phase_.callback->domain == SoundBlasterCallbackDomain::level2RetryNoHandle)
            && (!phase_.timerInstall || phase_.timerInstall->stage != AudioClockTimerInstallStage::installed
                || !phase_.timer)) {
            throw std::invalid_argument("no-handle callback requires native installed timer ownership");
        }
        const bool attractBorrow=phase_.attractBorrow==AudioClockAttractBorrow::menuPresentation;
        if(phase_.attractBorrow!=AudioClockAttractBorrow::inactive && (!attractBorrow
            || !phase_.playableAutomaticAttract || !phase_.playableCampaign
            || !phase_.timerInstall || phase_.timerInstall->stage!=AudioClockTimerInstallStage::installed
            || !phase_.devices || !phase_.devices->fixedPitAndMaskedDma
            || !phase_.timer || phase_.timer->activeMask!=1
            || !phase_.soundBlasterInitialization || !phase_.soundBlasterInitialization->driverEnabled
            || !phase_.callback || !phase_.callback->driverEnabled)) {
            throw std::invalid_argument("active attract borrow requires its explicit retained native owner");
        }
        if (phase_.sequenceSelection && (!phase_.soundBlasterInitialization
            || !phase_.soundBlasterInitialization->driverEnabled || !phase_.timer || phase_.timer->activeMask != 1
            || !phase_.callback || phase_.sequenceSelection->prepared.domain != SoundBlasterCallbackDomain::sequence
            || !phase_.sequenceSelection->prepared.active
            || phase_.sequenceSelection->prepared.sequence.empty()
            || (!phase_.sequenceSelection->published && !playableStartupProgram(phase_) && !attractBorrow
                && phase_.callback->domain != SoundBlasterCallbackDomain::noHandle
                && !(phase_.sequenceSelection->songOrdinal == level2MusicIdentity && phase_.successAudio
                    && phase_.successAudio->stage == SuccessAudioStage::returned
                    && phase_.callback->domain == SoundBlasterCallbackDomain::successNoHandle)
                && !(phase_.sequenceSelection->songOrdinal == level2MusicIdentity && phase_.level2RetryMusic
                    && phase_.level2RetryMusic->stage == AudioClockLevel2RetryMusicStage::resourceBound
                    && phase_.callback->domain == SoundBlasterCallbackDomain::level2RetryNoHandle))
            || (phase_.sequenceSelection->countdownWritten && !phase_.sequenceSelection->published
                && !playableStartupProgram(phase_) && !attractBorrow))) {
            throw std::invalid_argument("unsupported native sequence selection ownership");
        }
        if (phase_.rtc) {
            validateRtc(phase_);
        }
        validateKeyboard();
        if (phase_.interruptDelivery == AudioClockInterruptDelivery::residentTimerKeyboard) {
            if (!phase_.keyboard || !keyboardAccess_) {
                throw std::invalid_argument("resident keyboard delivery requires controller binding");
            }
            keyboardAccess_->validate();
        }
        if (queuedEntry) {
            // AddEntry stopped the assigned slice outside normal-core execution.
            refillReady();
        }
    }

    void dispatch(const Operation operation, SoundBlasterClockStep& step) {
        settle(step);
        --phase_.remainingCycles;
        ++step.dispatches;
        if (phase_.instructionKeyboardEdge) {
            const auto edge = *phase_.instructionKeyboardEdge;
            phase_.instructionKeyboardEdge.reset();
            queueUp(edge);
        }
        if (operation == Operation::write) {
            phase_.remainingCycles -= isa8WriteDelayCycles;
            step.delayCycles += isa8WriteDelayCycles;
        } else if (operation == Operation::wordWrite) {
            phase_.remainingCycles -= isa16WriteDelayCycles;
            step.delayCycles += isa16WriteDelayCycles;
        } else if (operation == Operation::statusRead) {
            phase_.remainingCycles -= isa8ReadDelayCycles;
            step.delayCycles += isa8ReadDelayCycles;
        } else if (operation == Operation::read) {
            phase_.remainingCycles -= isa8ReadDelayCycles;
            // The original signed clamp can credit a negative remaining budget.
            const auto extra = std::min(adlibReadDelayCycles, phase_.remainingCycles);
            phase_.remainingCycles -= extra;
            step.delayCycles += isa8ReadDelayCycles + extra;
        }
        checkHorizon();
    }

    void plain(const std::size_t count, SoundBlasterClockStep& step) {
        for (std::size_t index = 0; index < count; ++index) {
            dispatch(Operation::plain, step);
        }
    }

    void soundBlasterEffect(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (!phase_.soundBlasterInitialization || !phase_.timerInstall || !phase_.timer
            || phase_.timerInstall->stage != AudioClockTimerInstallStage::installed
            || phase_.timer->activeMask != 0 || operation.count != 1
            || phase_.soundBlasterInitialization->driverEnabled) {
            throw std::invalid_argument("Sound Blaster initialization requires an inactive native source owner");
        }
        auto& initialization = *phase_.soundBlasterInitialization;
        if (operation.kind == NormalWorkKind::soundBlasterOplWrite) {
            if (initialization.oplWrites >= soundBlasterInitializationWriteCount) {
                throw std::invalid_argument("source Sound Blaster initialization repeats OPL setup");
            }
            oplAddress(*this, result.total);
            const auto effect = oplData(*this, result.total, origin_, OplDataBoundary::afterOut);
            plain(1, result.total); //26E5 writer RET; caller CALL is separate work.
            ++initialization.oplWrites;
            result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::oplWrite, effect.elapsedCycles, operation.value});
            return;
        }
        if (operation.kind != NormalWorkKind::soundBlasterDriverEnable || operation.value != 1
            || initialization.oplWrites != soundBlasterInitializationWriteCount) {
            throw std::invalid_argument("driver enable requires complete source OPL initialization");
        }
        dispatch(Operation::plain, result.total); //1EBC:0078 actual B98 enable writer.
        initialization.driverEnabled = true;
        if (phase_.callback) {
            phase_.callback->driverEnabled = true;
        }
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::driverEnable, position() - origin_, 1});
    }

    void sequenceEffect(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (!phase_.sequenceSelection || operation.count != 1) {
            throw std::invalid_argument("sequence publication requires the prepared native owner");
        }
        auto& selection = *phase_.sequenceSelection;
        if (operation.kind == NormalWorkKind::startupMenuCountdown) {
            if (!selection.logoCurrentCleared || selection.menuCountdownWritten || operation.value != 2000
                || !phase_.timer || !phase_.callback || phase_.callback->domain != SoundBlasterCallbackDomain::noHandle) {
                throw std::invalid_argument("menu countdown requires the retained deselected logo");
            }
            dispatch(Operation::plain, result.total); //637F MOV global countdown,2000.
            phase_.timer->globalCount = operation.value;
            selection.menuCountdownWritten = true;
            return;
        }
        if (operation.kind == NormalWorkKind::soundBlasterSequencePublish) {
            if (selection.published || operation.value != selection.songOrdinal || operation.value > 2) {
                throw std::invalid_argument("source current handle may be published only once");
            }
            dispatch(Operation::plain, result.total); //1EBC:0325 current handle store.
            phase_.callback = selection.prepared;
            selection.published = true;
            result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::sequencePublish,
                position() - origin_, operation.value});
            return;
        }
        if (operation.kind != NormalWorkKind::timerGlobalCount || !selection.published
            || selection.countdownWritten || operation.value != 240) {
            throw std::invalid_argument("startup countdown requires the actual post-selection source store");
        }
        dispatch(Operation::plain, result.total); //0824:61BB, distinct from sequence activation.
        phase_.timer->globalCount = operation.value;
        selection.countdownWritten = true;
    }

    void playableStartupMusicOperation(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if ((operation.kind != NormalWorkKind::playableStartupCreditsPoll && operation.count != 1) || !playableStartupProgram(phase_)
            || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("playable startup music operation lost its retained owner");
        }
        auto& selection = *phase_.sequenceSelection;
        const auto empty = phase_.playableMusicOff
            && phase_.callback->domain == SoundBlasterCallbackDomain::level2RetryNoHandle;
        if (operation.kind == NormalWorkKind::playableStartupLogoCountdown) {
            if (!empty || selection.songOrdinal != 0 || selection.published
                || selection.countdownWritten || operation.value != 240) {
                throw std::invalid_argument("cold logo countdown requires its unpublished allocation");
            }
            dispatch(Operation::plain,result.total); //61BB actual store, no0325 receipt.
            phase_.timer->globalCount = 240;
            selection.countdownWritten = true;
            return;
        }
        if (operation.kind == NormalWorkKind::playableStartupLogoWaitStore) {
            if (!empty || selection.songOrdinal != 0 || selection.logoFramesCompleted != 17
                || selection.logoFrameActive || selection.logoTimerFallback || operation.value != 480) {
                throw std::invalid_argument("cold logo fallback requires all seventeen actual frames");
            }
            //6241 CMP and6246 JZ select the B96==0 branch.
            plain(2,result.total);
            dispatch(Operation::plain,result.total);
            phase_.timer->globalCount = 480;
            selection.logoTimerFallback = true;
            selection.countdownReturned = false;
            plain(3,result.total); //625A CALL6638 and its two-instruction prologue.
            return;
        }
        if (operation.kind == NormalWorkKind::playableStartupLogoEmptyReturn) {
            if (!empty || selection.songOrdinal != 0 || !selection.logoTimerFallback
                || !selection.countdownReturned || phase_.timer->globalCount != 0
                || selection.logoEmptyCurrentReturned || operation.value != 0) {
                throw std::invalid_argument("cold logo empty return requires its completed480 IRQ wait");
            }
            //625D CALL51A6 is caller work.51A6 CMP/JZ;51BD writes zero;51C3 RETF.
            plain(4,result.total);
            selection.logoEmptyCurrentReturned = true;
            //No repeatOnce,op0E,keyoff,sequenceEnd,sequenceClear or logoCurrentCleared.
            return;
        }
        if (operation.kind == NormalWorkKind::playableStartupMenuCountdown) {
            if (!empty || selection.songOrdinal != 0 || !selection.logoEmptyCurrentReturned
                || selection.menuCountdownWritten || operation.value != 2000) {
                throw std::invalid_argument("cold menu countdown requires its returned unpublished logo");
            }
            dispatch(Operation::plain,result.total); //637F, before independent0086 menu allocation.
            phase_.timer->globalCount = 2000;
            selection.menuCountdownWritten = true;
            return;
        }
        if (operation.kind == NormalWorkKind::playableStartupCreditsCountdown) {
            if (selection.songOrdinal != 1 || selection.creditsCountdownWritten || selection.creditsSkipReturned || operation.value != 7200) {
                throw std::invalid_argument("native credits countdown requires its loaded menu program");
            }
            dispatch(Operation::plain,result.total); //B29E.
            phase_.timer->globalCount = 7200;
            selection.creditsCountdownWritten = true;
            return;
        }
        if (operation.kind == NormalWorkKind::playableStartupCreditsSkip) {
            if (!phase_.playableAutomaticAttract || selection.songOrdinal != 1 || selection.creditsCountdownWritten
                || selection.creditsInputReturned || selection.creditsSkipReturned || operation.value != 1) {
                throw std::invalid_argument("cold credits skip requires its retained nonzero19CA caller");
            }
            plain(17,result.total); //Three caller instructions; B12A..B13C skips to B2C7..B2CC.
            selection.creditsSkipReturned=true;
            return;
        }
        if (operation.kind == NormalWorkKind::playableStartupCreditsPoll) {
            if (!phase_.playableAutomaticAttract || selection.songOrdinal != 1 || !selection.creditsCountdownWritten
                || selection.creditsInputReturned || operation.count == 0 || operation.count > 10000) {
                throw std::invalid_argument("cold credits poll requires its entered native presentation");
            }
            for (std::uint64_t index=0;index<operation.count;++index) {
                plain(1,result.total);
                dispatch(Operation::plain,result.total); //B2A4 CALL87CA/read.
                const auto read=phase_.timer->globalCount;
                result.creditsCountdownRead=read;result.countdownLastRead=read;++result.countdownPolls;
                plain(3,result.total); //87CD/B2A7/B2A9 use the captured count.
                if (read==0) { selection.creditsInputReturned=true;result.countdownReturned=true;return; }
                plain(3,result.total); //B2AB native03EE;B2B0/B2B3.
                if (index==0 && (operation.value>>8)!=0) {
                    selection.creditsInputReturned=true;result.countdownReturned=true;return;
                }
            }
            return;
        }
        if (operation.kind == NormalWorkKind::playableStartupCreditsInput) {
            if (selection.songOrdinal != 1 || !selection.creditsCountdownWritten
                || selection.creditsInputReturned || operation.value != 0x011b) {
                throw std::invalid_argument("native credits require first-dequeue Escape");
            }
            plain(1,result.total);
            dispatch(Operation::plain,result.total); //87CA captured countdown read.
            result.creditsCountdownRead = phase_.timer->globalCount;
            plain(3,result.total);
            if (result.creditsCountdownRead == 0) {
                throw std::invalid_argument("native credits reached unowned expiration");
            }
            plain(3,result.total); //Untimed03EE returns explicit Escape.
            selection.creditsInputReturned = true;
            return;
        }
        if (operation.kind == NormalWorkKind::playableStartupMenuEntryCountdown) {
            if (selection.songOrdinal != 1 || (!selection.creditsInputReturned && !selection.creditsSkipReturned)
                || selection.menuEntryCountdownWritten || operation.value != 30000) {
                throw std::invalid_argument("native menu entry requires exactly one actual1804 store");
            }
            dispatch(Operation::plain,result.total);
            phase_.timer->globalCount = 30000;
            selection.menuEntryCountdownWritten = true;
            return;
        }
        if (operation.kind == NormalWorkKind::playableStartupBeginFadeRequest) {
            if (!playableStartupMenuProgram(phase_)
                || (selection.recoveredMenuInput ? (!selection.recoveredMenuActivated
                    || selection.recoveredMenuResult != 1 || selection.beginDecisions != 0)
                    : selection.beginDecisions != 3)
                || selection.beginFadeRequests >= 2 || operation.value != selection.beginFadeRequests + 1
                || phase_.callback->program->handleVolume == 0
                || phase_.callback->program->handleVolume > maximumMusicVolume) {
                throw std::invalid_argument("native Begin requires two actual loaded-handle fade calls");
            }
            //Both165E/0152 pass485A loaded head, even when B94 is empty.
            plain(29,result.total);
            dispatch(Operation::plain,result.total); //2D6A global AC store; no-current IRQ never services fade.
            phase_.callback->fade = phase_.callback->program->handleVolume;
            ++selection.beginFadeRequests;
            result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::fadeRequest,
                position()-origin_,0x0c});
            plain(12,result.total);
            foregroundFlags(NormalWorkKind::popFlagsEnabled,result.total);
            plain(6,result.total);
            return;
        }
        if (operation.kind == NormalWorkKind::playableStartupBeginEmptyReturn) {
            if (!empty || selection.songOrdinal != 1 || !selection.beginCountdownReturned
                || phase_.timer->globalCount != 0 || selection.beginEmptyCurrentReturned
                || operation.value != 0) {
                throw std::invalid_argument("native Begin empty return requires actual480 IRQ completion");
            }
            plain(4,result.total); //51A6 CMP/JZ,51BD zero store,51C3 RETF. No op0E.
            selection.beginEmptyCurrentReturned = true;
            return;
        }
        throw std::invalid_argument("unsupported native startup music operation");
    }

    void playableSuccessCountdownStore(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        const auto* callback=phase_.callback ? &*phase_.callback : nullptr;
        if (!phase_.playableCampaign || !phase_.playableMusicOff || phase_.playableSuccessCountdownWritten
            || operation.count!=1 || operation.value!=successRawCountdown || !phase_.timer || phase_.timer->activeMask!=1
            || phase_.interrupts!=AudioClockInterrupts::enabled || !callback || !callback->driverEnabled
            || callback->domain!=SoundBlasterCallbackDomain::level2RetryNoHandle || !callback->program
            || callback->program->repeatCount!=1 || callback->program->handleStatus!=0) {
            throw std::invalid_argument("playable success requires its unpublished once descriptor and one7072 store");
        }
        dispatch(Operation::plain,result.total); //7072 MOV[1DC1],03E0.
        phase_.timer->globalCount=operation.value;
        phase_.playableSuccessCountdownWritten=true;
    }

    void playableIrqWait(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (!phase_.playableCampaign || !phase_.timer || phase_.timer->activeMask==0) {
            throw std::invalid_argument("playable scene wait requires its retained PIT owner");
        }
        if (operation.kind==NormalWorkKind::playableIrqWaitStore) {
            if (operation.count!=1 || operation.value==0 || operation.value%4!=0) {
                throw std::invalid_argument("playable IRQ wait requires its source raw countdown");
            }
            plain(7,result.total); //10A8..10B4; the decoded page already contains the two shifts.
            dispatch(Operation::plain,result.total); //10B6 stores1DC1.
            phase_.timer->globalCount=operation.value;
            return;
        }
        if (operation.count==0 || operation.count>10000 || operation.value!=0) {
            throw std::invalid_argument("invalid playable SCENED wait packet");
        }
        for (std::uint64_t index=0;index<operation.count;++index) {
            //10BA..10CB neutral-input provider;03EE child elapsed is unmeasured.
            plain(8,result.total);
            dispatch(Operation::plain,result.total); //10A4 reads1DC1.
            const auto read=phase_.timer->globalCount;
            result.countdownLastRead=read;++result.countdownPolls;
            plain(3,result.total); //10A7/10CE/10D0.
            if (read!=0) {continue;}
            plain(3,result.total); //10D2..10D5.
            result.countdownReturned=true;
            return;
        }
    }

    void playableAttract(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if(!phase_.playableAutomaticAttract || !phase_.timer || phase_.timer->activeMask==0
            || phase_.interrupts!=AudioClockInterrupts::enabled || operation.count==0 || operation.count>10000) {
            throw std::invalid_argument("automatic attract requires its explicit retained native PIT owner");
        }
        if(operation.kind==NormalWorkKind::playableAttractMenuDecision) {
            if(operation.count!=1 || operation.value!=0) {throw std::invalid_argument("invalid attract menu decision");}
            plain(1,result.total); //160F captures the actual menu action before the timer read.
            dispatch(Operation::plain,result.total); //1611: IRQ may cross incoming1 to captured0.
            const auto read=phase_.timer->globalCount;result.menuDecisionCountdownRead=read;
            plain(1,result.total); //1616 uses the captured read.
            if(read==0) {
                dispatch(Operation::plain,result.total);phase_.timer->globalCount=30000; //1618.
            }
            return; //Native resource-open provider and SI override belong to the caller.
        }
        if(operation.kind==NormalWorkKind::playableAttractCountdownStore) {
            if(operation.count!=1) {throw std::invalid_argument("invalid attract countdown store");}
            dispatch(Operation::plain,result.total);phase_.timer->globalCount=operation.value;return;
        }
        if(operation.kind==NormalWorkKind::playableAttractLogoOnce) {
            if(operation.count!=1 || operation.value!=1 || !phase_.callback || !phase_.callback->program
                || phase_.callback->domain!=SoundBlasterCallbackDomain::sequence
                || phase_.callback->program->repeatCount!=0xffff) {throw std::invalid_argument("attract once requires its actual repeating logo handle");}
            dispatch(Operation::plain,result.total);phase_.callback->program->repeatCount=1; //664C.
            result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::repeatOnce,position()-origin_,1});return;
        }
        if(operation.kind==NormalWorkKind::playableAttractCreditsPoll) {
            for(std::uint64_t index=0;index<operation.count;++index) {
                plain(1,result.total);dispatch(Operation::plain,result.total); //B2A4 CALL87CA/read.
                const auto read=phase_.timer->globalCount;result.creditsCountdownRead=read;
                result.countdownLastRead=read;++result.countdownPolls;
                plain(3,result.total); //87CD/B2A7/B2A9, captured countdown.
                if(read==0) {result.countdownReturned=true;return;}
                plain(3,result.total); //B2AB native03EE, B2B0 TEST/B2B3 JZ.
                if(index==0 && (operation.value>>8)!=0) {result.countdownReturned=true;return;}
            }
            return;
        }
        if(operation.value!=0) {throw std::invalid_argument("invalid attract countdown/handle poll");}
        for(std::uint64_t index=0;index<operation.count;++index) {
            if(operation.kind==NormalWorkKind::playableAttractLogoEndPoll) {
                if(!phase_.callback || !phase_.callback->program || phase_.callback->program->repeatCount!=1
                    || phase_.callback->domain!=SoundBlasterCallbackDomain::sequence) {throw std::invalid_argument("attract logo poll lost its selected once handle");}
                dispatch(Operation::plain,result.total); //6651 captured status.
                const auto read=phase_.callback->program->handleStatus;result.sequenceEndLastRead=read;++result.sequenceEndPolls;
                plain(1,result.total);
                if(read==0xffff) {plain(5,result.total);result.sequenceEndReturned=true;return;}
                continue;
            }
            plain(1,result.total);dispatch(Operation::plain,result.total); //CALL87CA;87CA read.
            const auto read=phase_.timer->globalCount;result.countdownLastRead=read;++result.countdownPolls;
            plain(3,result.total); //87CD RET; caller OR/Jcc, using captured read.
            if(operation.kind==NormalWorkKind::playableAttractCountdownRead) {return;}
            if(operation.kind!=NormalWorkKind::playableAttractWaitPoll) {throw std::invalid_argument("unsupported attract poll");}
            if(read==0) {plain(3,result.total);result.countdownReturned=true;return;} //6642..6645.
        }
    }

    void playableJoystickBriefingRefresh(const NormalWorkOperation& operation,NormalWorkTiming& result) {
        constexpr std::uint16_t briefingRecognizedCountdown=15000;
        if(!phase_.playableJoystickControls||!phase_.timer||phase_.timer->activeMask==0
            ||operation.count!=1||operation.value!=briefingRecognizedCountdown) {
            throw std::invalid_argument("joystick Briefing callback requires its native retained PIT owner");
        }
        dispatch(Operation::plain,result.total);phase_.timer->globalCount=briefingRecognizedCountdown; //1FC5.
        plain(1,result.total); //1FCB clears2C2; retained manual dialog has no automatic-tour owner.
    }

    void playableJoystickDebounce(const NormalWorkOperation& operation,NormalWorkTiming& result) {
        if(!phase_.playableJoystickControls||!phase_.timer||phase_.timer->activeMask==0
            ||operation.count==0||operation.count>10000) {
            throw std::invalid_argument("joystick debounce requires its native retained PIT owner");
        }
        if(operation.kind==NormalWorkKind::playableJoystickDebounceStore) {
            if(operation.count!=1||operation.value!=10) {throw std::invalid_argument("joystick debounce requires source raw10");}
            dispatch(Operation::plain,result.total);phase_.timer->globalCount=10; //88DB.
            plain(3,result.total);return; //88E1 CALL6638;6638 prologue.
        }
        if(operation.value!=0) {throw std::invalid_argument("invalid joystick countdown poll");}
        for(std::uint64_t i=0;i<operation.count;++i) {
            plain(1,result.total);dispatch(Operation::plain,result.total); //663B CALL87CA/read.
            const auto read=phase_.timer->globalCount;result.countdownLastRead=read;++result.countdownPolls;
            plain(3,result.total);
            if(read==0) {plain(3,result.total);result.countdownReturned=true;return;}
        }
    }

    void playableStoryWait(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (!phase_.playableCampaign || !phase_.timer || phase_.timer->activeMask==0) {
            throw std::invalid_argument("Story wait requires its retained native PIT owner");
        }
        if (operation.kind==NormalWorkKind::playableStoryWaitStore) {
            if (operation.count!=1) {throw std::invalid_argument("invalid Story raw countdown store");}
            dispatch(Operation::plain,result.total); //1110:122A or1285 writes the actual raw IRQ count.
            phase_.timer->globalCount=operation.value;
            return;
        }
        if (operation.count==0 || operation.count>10000 || operation.value!=0) {
            throw std::invalid_argument("invalid native Story wait poll");
        }
        for (std::uint64_t index=0;index<operation.count;++index) {
            plain(2,result.total); //1352 PUSH CS/CALL10A4.
            dispatch(Operation::plain,result.total); //10A4 MOV AX,[1DC1].
            const auto count=phase_.timer->globalCount;
            result.countdownLastRead=count;++result.countdownPolls;
            plain(3,result.total); //10A7 RETF;1356 OR;1358 JZ.
            if (count==0) {result.countdownReturned=true;return;}
            //135A CALL B5B8; no active voice completion descriptor in this
            //counter-only domain. Speech completion is serviced by its owner.
            plain(7,result.total);
        }
    }

    void startupCountdownPoll(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (!phase_.sequenceSelection || !phase_.sequenceSelection->countdownWritten
            || phase_.sequenceSelection->countdownReturned || operation.count == 0
            || operation.count > 10000 || operation.value != 0) {
            throw std::invalid_argument("unsupported retained startup countdown poll");
        }
        for (std::uint64_t index = 0; index < operation.count; ++index) {
            plain(1, result.total); //663B CALL87CA.
            dispatch(Operation::plain, result.total); //87CA MOV AX,[1DC1].
            const auto read = phase_.timer->globalCount;
            result.countdownLastRead = read;
            ++result.countdownPolls;
            plain(3, result.total); //87CD RET;663E OR;6640 JNZ.
            if (read != 0) { continue; }
            plain(3, result.total); //6642 MOV SP,BP;6644 POP BP;6645 RET.
            result.countdownReturned = true;
            phase_.sequenceSelection->countdownReturned = true;
            return;
        }
    }

    void startupCreditsInput(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        constexpr std::uint16_t creditsCountdown = 7200;
        constexpr std::uint16_t nativeEscape = 0x011b;
        if (operation.count != 1 || !phase_.timer || !phase_.sequenceSelection
            || !phase_.sequenceSelection->published || phase_.sequenceSelection->songOrdinal != 1
            || !phase_.callback || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || !phase_.callback->program) {
            throw std::invalid_argument("credits input requires the retained selected presentation song");
        }
        auto& selection = *phase_.sequenceSelection;
        if (operation.kind == NormalWorkKind::startupCreditsCountdown) {
            if (selection.creditsCountdownWritten || operation.value != creditsCountdown) {
                throw std::invalid_argument("credits countdown must be stored once at the source boundary");
            }
            dispatch(Operation::plain, result.total); //B29E MOV [1DC1],7200.
            phase_.timer->globalCount = creditsCountdown;
            selection.creditsCountdownWritten = true;
            return;
        }
        if (!selection.creditsCountdownWritten || selection.creditsInputReturned || operation.value != nativeEscape) {
            throw std::invalid_argument("credits admit only an explicit first-dequeue native Escape");
        }
        plain(1, result.total); //B2A4 CALL87CA.
        dispatch(Operation::plain, result.total); //87CA MOV AX,[1DC1].
        const auto read = phase_.timer->globalCount;
        result.creditsCountdownRead = read;
        plain(3, result.total); //87CD RET;B2A7 OR;B2A9 JZ.
        if (read == 0) {
            throw std::invalid_argument("credits first-input path reached the unsupported expiry branch");
        }
        //Whole03EE is an untimed native provider returning the explicit Escape.
        plain(3, result.total); //B2AB CALL03EE;B2B0 TEST AH;B2B3 JZ.
        selection.creditsInputReturned = true;
    }

    void startupMenuEntryCountdown(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        constexpr std::uint16_t menuCountdown = 30000;
        if (operation.count != 1 || operation.value != menuCountdown || !phase_.timer
            || !phase_.sequenceSelection || !phase_.sequenceSelection->published
            || phase_.sequenceSelection->songOrdinal != 1
            || !phase_.sequenceSelection->creditsInputReturned
            || phase_.sequenceSelection->menuEntryCountdownWritten) {
            throw std::invalid_argument("menu entry requires a single retained countdown store");
        }
        dispatch(Operation::plain, result.total); //1804 MOV [1DC1],30000.
        phase_.timer->globalCount = menuCountdown;
        phase_.sequenceSelection->menuEntryCountdownWritten = true;
    }

    void startupMenuReentryCountdown(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        const auto* callback=phase_.callback ? &*phase_.callback : nullptr;
        const bool portable=phase_.playableCampaign && callback
            && callback->program && (callback->domain==SoundBlasterCallbackDomain::sequence
                || (phase_.playableMusicOff && callback->domain==SoundBlasterCallbackDomain::level2RetryNoHandle));
        const bool sourceMenu=retainedMenuProgram(phase_) || playableStartupMenuProgram(phase_);
        if(operation.count!=1 || operation.value!=menuCountdownRawIrqs || !phase_.timer
            || phase_.timer->activeMask==0 || !callback || !callback->driverEnabled
            || (!sourceMenu && !portable)
            || (sourceMenu && (phase_.sequenceSelection->beginFadeRequests!=0
                || phase_.sequenceSelection->beginDecisions!=0
                || phase_.sequenceSelection->recoveredMenuActivated))) {
            throw std::invalid_argument("menu reentry requires its retained modal-return owner");
        }
        dispatch(Operation::plain,result.total); //1804, after menu composition/pointer/palette.
        phase_.timer->globalCount=menuCountdownRawIrqs;
    }

    void startupLogoEventSuppressed(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (operation.count != 1 || operation.value != 26 || !phase_.sequenceSelection
            || !phase_.sequenceSelection->countdownReturned || phase_.sequenceSelection->logoEventPlayed
            || !phase_.callback || !phase_.soundBlasterInitialization
            || !phase_.soundBlasterInitialization->driverEnabled
            || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("suppressed logo event requires its completed startup countdown");
        }
        //6201 caller3, B3E6..B3F0 early return11,6208 cleanup1. No driver call.
        plain(3 + 11 + 1, result.total);
        phase_.sequenceSelection->logoEventPlayed = true; //Caller completed, enabling its animation.
        phase_.sequenceSelection->logoEventSuppressed = true;
    }

    void startupLogoEvent(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (operation.count != 1 || operation.value != 26 || !phase_.sequenceSelection
            || !phase_.sequenceSelection->countdownReturned || phase_.sequenceSelection->logoEventPlayed
            || !phase_.callback || !phase_.soundBlasterInitialization
            || !phase_.soundBlasterInitialization->driverEnabled
            || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("logo event requires the retained completed startup countdown");
        }
        //6201 caller3, B3E6 selected-sound4 path18,0478/dispatcher23,216A prefix3.
        constexpr std::size_t prefixDispatches = 3 + 18 + 23 + 3;
        //216A return4, dispatcher9,0478 return5, B448..B4506,6208 cleanup1.
        constexpr std::size_t suffixDispatches = 4 + 9 + 5 + 6 + 1;
        plain(prefixDispatches, result.total);
        foregroundFlags(NormalWorkKind::cli, result.total); //216D.
        auto direct = soundBlasterDirectAdvance(*phase_.callback, startupLogoDirectRecord);
        foregroundOplBlocks(direct.blocks, result);
        phase_.callback = std::move(direct.state);
        phase_.sequenceSelection->logoEventPlayed = true;
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::directState, position() - origin_, 26,
            std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        foregroundFlags(NormalWorkKind::sti, result.total); //21D5; publication precedes enabled IRQ delivery.
        plain(suffixDispatches, result.total);
    }

    void gameplayPrimaryFire(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        //Ordinal2 is the third retained handle (Level1 SNG0), not campaign Level2.
        if (operation.count != 1 || operation.value != gameplayPrimaryFireEvent
            || !phase_.sequenceSelection || !phase_.sequenceSelection->published
            || phase_.sequenceSelection->songOrdinal != 2 || !phase_.sequenceSelection->level1DirectResetCompleted
            || !phase_.callback || !phase_.callback->program || !phase_.callback->active
            || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || !phase_.callback->driverEnabled || phase_.callback->program->handleStatus != 0
            || !phase_.soundBlasterInitialization || !phase_.soundBlasterInitialization->driverEnabled
            || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("primary fire audio requires the retained running Level1 owner");
        }
        plain(3 + 18 + 23 + 3, result.total); //0710 caller,B3E6,0478/dispatcher,216A prefix.
        foregroundFlags(NormalWorkKind::cli, result.total); //216D, after any prefix IRQ.
        auto direct = soundBlasterDirectAdvance(*phase_.callback, gameplayPrimaryFireDirectRecord,
            SoundBlasterDirectCaller::gameplayPrimaryFire);
        foregroundOplBlocks(direct.blocks, result);
        phase_.callback = std::move(direct.state);
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::gameplayDirectState,
            position() - origin_, gameplayPrimaryFireEvent,
            std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        foregroundFlags(NormalWorkKind::sti, result.total); //21D5; state precedes enabled IRQ delivery.
        plain(4 + 9 + 5 + 6 + 1, result.total); //216A return,dispatcher,0478,B448..B450,0717.
    }

    void gameplayLevel2Fire(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        const auto enemy=operation.kind==NormalWorkKind::gameplayLevel2EnemyFireEvent;
        const auto event=enemy ? gameplayEnemyType17Event : gameplayPrimaryFireEvent;
        if (operation.count != 1 || operation.value != event
            || !phase_.sequenceSelection || !phase_.sequenceSelection->published
            || phase_.sequenceSelection->songOrdinal != level2MusicIdentity || !phase_.sequenceSelection->level1DirectResetCompleted
            || !phase_.successAudio || phase_.successAudio->stage != SuccessAudioStage::returned
            || !phase_.callback || !phase_.callback->program || !phase_.callback->active
            || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || !phase_.callback->driverEnabled || phase_.callback->program->handleStatus != 0
            || (enemy && phase_.callback->fade != 0)
            || phase_.callback->program->repeatCount != 0xffff
            || phase_.callback->sequence != phase_.sequenceSelection->prepared.sequence
            || !phase_.soundBlasterInitialization || !phase_.soundBlasterInitialization->driverEnabled
            || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("fire audio requires the retained running Level2 owner");
        }
        plain((enemy ? 2 : 3) + 18 + 23 + 3, result.total); //9A1B/9A1C or0710 caller, then shared dispatcher.
        foregroundFlags(NormalWorkKind::cli, result.total); //216D, after any prefix IRQ.
        auto direct = soundBlasterDirectAdvance(*phase_.callback,
            enemy ? gameplayEnemyType17DirectRecord : gameplayPrimaryFireDirectRecord,
            enemy ? SoundBlasterDirectCaller::gameplayEnemyFire : SoundBlasterDirectCaller::gameplayPrimaryFire);
        foregroundOplBlocks(direct.blocks, result);
        phase_.callback = std::move(direct.state);
        result.soundBlasterEffects.push_back({enemy ? SoundBlasterForegroundKind::gameplayLevel2EnemyDirectState
                : SoundBlasterForegroundKind::gameplayLevel2DirectState,
            position() - origin_, event,
            std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        foregroundFlags(NormalWorkKind::sti, result.total); //21D5; state precedes enabled IRQ delivery.
        plain(4 + 9 + 5 + 6 + 1, result.total); //216A return,dispatcher,0478,B448..B450,0717.
    }

    void gameplayLevel2TargetHit(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        const auto failure=operation.kind==NormalWorkKind::failureLevel2ExplosionEvent;
        if (operation.count != 1 || (operation.value != gameplayTargetImpactEvent && operation.value != gameplayPlayerHitEvent)
            || (failure && operation.value!=gameplayPlayerHitEvent)
            || !phase_.sequenceSelection || !phase_.sequenceSelection->published
            || phase_.sequenceSelection->songOrdinal != level2MusicIdentity || !phase_.sequenceSelection->level1DirectResetCompleted
            || !phase_.successAudio || phase_.successAudio->stage != SuccessAudioStage::returned
            || !phase_.callback || !phase_.callback->program || !phase_.callback->active
            || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || !phase_.callback->driverEnabled || phase_.callback->program->handleStatus != 0
            || phase_.callback->fade != 0
            || (operation.value == gameplayPlayerHitEvent && (!phase_.callback->program->playerHitVolumeRow
                || phase_.callback->program->playerHitVolumeRow->volume != gameplayPlayerHitCallerVolume))
            || phase_.callback->program->repeatCount != 0xffff
            || phase_.callback->sequence != phase_.sequenceSelection->prepared.sequence
            || !phase_.soundBlasterInitialization || !phase_.soundBlasterInitialization->driverEnabled
            || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("target hit audio requires the retained running Level2 owner");
        }
        plain(3 + 18 + 23 + 3, result.total); //1238..123C or127A..127E; B3E6/dispatcher/216A prefix.
        foregroundFlags(NormalWorkKind::cli, result.total); //216D, after any prefix IRQ.
        const bool playerHit = operation.value == gameplayPlayerHitEvent;
        auto direct = soundBlasterDirectAdvance(*phase_.callback,
            playerHit ? gameplayPlayerHitDirectRecord : gameplayTargetImpactDirectRecord,
            playerHit ? SoundBlasterDirectCaller::gameplayPlayerHit : SoundBlasterDirectCaller::gameplayTargetImpact);
        foregroundOplBlocks(direct.blocks, result);
        phase_.callback = std::move(direct.state);
        result.soundBlasterEffects.push_back({failure ? SoundBlasterForegroundKind::failureLevel2PlayerHitDirectState
                : SoundBlasterForegroundKind::gameplayLevel2TargetHitDirectState,
            position() - origin_, operation.value,
            std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        foregroundFlags(NormalWorkKind::sti, result.total); //21D5; state precedes enabled IRQ delivery.
        plain(4 + 9 + 5 + 6 + 1, result.total); //Driver/wrapper return and123F or1281 cleanup.
    }

    enum class DirectCampaignDomain { level1, level2 };
    void gameplayEnemyType20(const NormalWorkOperation& operation, NormalWorkTiming& result,
        const DirectCampaignDomain domain = DirectCampaignDomain::level1) {
        const auto level2=domain==DirectCampaignDomain::level2;
        if (operation.count != 1 || operation.value != gameplayEnemyType20Event
            || !phase_.sequenceSelection || !phase_.sequenceSelection->published
            || phase_.sequenceSelection->songOrdinal != (level2 ? level2MusicIdentity : 2) || !phase_.sequenceSelection->level1DirectResetCompleted
            || (level2 && (!retainedLevel2Program(phase_)
                || phase_.sequenceSelection->prepared.sequence!=phase_.callback->sequence))
            || !phase_.callback || !phase_.callback->program || !phase_.callback->active
            || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || !phase_.callback->driverEnabled || phase_.callback->program->handleStatus != 0
            || phase_.callback->fade != 0 || !phase_.callback->program->enemy20CallerWindow
            || !phase_.soundBlasterInitialization || !phase_.soundBlasterInitialization->driverEnabled
            || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("enemy fire audio requires the retained running Level1 owner");
        }
        plain(2 + 18 + 23 + 3, result.total); //9A1B/9A1C,B3E6,0478/dispatcher,216A prefix.
        foregroundFlags(NormalWorkKind::cli, result.total);
        auto direct = soundBlasterDirectAdvance(*phase_.callback, gameplayEnemyType20DirectRecord,
            SoundBlasterDirectCaller::gameplayEnemyType20);
        foregroundOplBlocks(direct.blocks, result);
        phase_.callback = std::move(direct.state);
        result.soundBlasterEffects.push_back({level2 ? SoundBlasterForegroundKind::gameplayLevel2EnemyType20DirectState
                : SoundBlasterForegroundKind::gameplayEnemyType20DirectState,
            position() - origin_, operation.value,
            std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        foregroundFlags(NormalWorkKind::sti, result.total);
        plain(4 + 9 + 5 + 6 + 1, result.total); //Return wrappers and9A1F cleanup.
    }

    void gameplayEnemyFire(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (operation.value == gameplayEnemyType20Event) {
            gameplayEnemyType20(operation, result);
            return;
        }
        if (operation.count != 1 || operation.value != gameplayEnemyType17Event
            || !phase_.sequenceSelection || !phase_.sequenceSelection->published
            || phase_.sequenceSelection->songOrdinal != 2 || !phase_.sequenceSelection->level1DirectResetCompleted
            || !phase_.callback || !phase_.callback->program || !phase_.callback->active
            || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || !phase_.callback->driverEnabled || phase_.callback->program->handleStatus != 0
            || !phase_.soundBlasterInitialization || !phase_.soundBlasterInitialization->driverEnabled
            || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("enemy fire audio requires the retained running Level1 owner");
        }
        plain(2 + 18 + 23 + 3, result.total); //9A1B/9A1C,B3E6,0478/dispatcher,216A prefix.
        foregroundFlags(NormalWorkKind::cli, result.total);
        auto direct = soundBlasterDirectAdvance(*phase_.callback, gameplayEnemyType17DirectRecord,
            SoundBlasterDirectCaller::gameplayEnemyFire);
        foregroundOplBlocks(direct.blocks, result);
        phase_.callback = std::move(direct.state);
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::gameplayEnemyDirectState,
            position() - origin_, operation.value,
            std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        foregroundFlags(NormalWorkKind::sti, result.total);
        plain(4 + 9 + 5 + 6 + 1, result.total); //Return wrappers and9A1F cleanup.
    }

    void gameplayPlayerHit(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (operation.count != 1 || operation.value != gameplayPlayerHitEvent
            || !phase_.sequenceSelection || !phase_.sequenceSelection->published
            || phase_.sequenceSelection->songOrdinal != 2 || !phase_.sequenceSelection->level1DirectResetCompleted
            || !phase_.callback || !phase_.callback->program || !phase_.callback->active
            || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || !phase_.callback->driverEnabled || phase_.callback->program->handleStatus != 0
            || !phase_.callback->program->playerHitVolumeRow
            || phase_.callback->program->playerHitVolumeRow->volume != gameplayPlayerHitCallerVolume
            || phase_.callback->fade != 0 || !phase_.soundBlasterInitialization
            || !phase_.soundBlasterInitialization->driverEnabled || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("player hit audio requires the retained Level1 owner and immutable volume row");
        }
        plain(3 + 18 + 23 + 3, result.total); //0E7F..0E83 or748F..7493; same B3E6/dispatcher prefix.
        foregroundFlags(NormalWorkKind::cli, result.total);
        auto direct = soundBlasterDirectAdvance(*phase_.callback, gameplayPlayerHitDirectRecord,
            SoundBlasterDirectCaller::gameplayPlayerHit);
        foregroundOplBlocks(direct.blocks, result);
        phase_.callback = std::move(direct.state);
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::gameplayPlayerHitDirectState,
            position() - origin_, operation.value,
            std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        foregroundFlags(NormalWorkKind::sti, result.total);
        plain(4 + 9 + 5 + 6 + 1, result.total); //Return wrappers and0E86 or7496 cleanup.
    }

    void gameplayTargetHit(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (operation.value == gameplayPlayerHitEvent) {
            //1238/123B/123C/123F: same four-dispatch wrapper and event6 kernel.
            gameplayPlayerHit(operation, result);
            return;
        }
        if (operation.count != 1 || operation.value != gameplayTargetImpactEvent
            || !phase_.sequenceSelection || !phase_.sequenceSelection->published
            || phase_.sequenceSelection->songOrdinal != 2 || !phase_.sequenceSelection->level1DirectResetCompleted
            || !phase_.callback || !phase_.callback->program || !phase_.callback->active
            || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || !phase_.callback->driverEnabled || phase_.callback->program->handleStatus != 0
            || phase_.callback->fade != 0 || !phase_.soundBlasterInitialization
            || !phase_.soundBlasterInitialization->driverEnabled || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("target impact audio requires the retained running Level1 owner");
        }
        plain(3 + 18 + 23 + 3, result.total); //127A..127E,B3E6,0478/dispatcher,216A prefix.
        foregroundFlags(NormalWorkKind::cli, result.total);
        auto direct = soundBlasterDirectAdvance(*phase_.callback, gameplayTargetImpactDirectRecord,
            SoundBlasterDirectCaller::gameplayTargetImpact);
        foregroundOplBlocks(direct.blocks, result);
        phase_.callback = std::move(direct.state);
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::gameplayTargetImpactDirectState,
            position() - origin_, operation.value,
            std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        foregroundFlags(NormalWorkKind::sti, result.total);
        plain(4 + 9 + 5 + 6 + 1, result.total); //Return wrappers and1281 cleanup.
    }

    void gameplayBonusPickup(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (operation.count != 1 || operation.value != gameplaySystemBonusEvent
            || !phase_.sequenceSelection || !phase_.sequenceSelection->published
            || phase_.sequenceSelection->songOrdinal != 2 || !phase_.sequenceSelection->level1DirectResetCompleted
            || !phase_.callback || !phase_.callback->program || !phase_.callback->active
            || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || !phase_.callback->driverEnabled || phase_.callback->program->handleStatus != 0
            || phase_.callback->fade != 0 || !phase_.soundBlasterInitialization
            || !phase_.soundBlasterInitialization->driverEnabled || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("system bonus audio requires the retained running Level1 owner");
        }
        plain(3 + 18 + 23 + 3, result.total); //9103..9107,B3E6,0478/dispatcher,216A prefix.
        foregroundFlags(NormalWorkKind::cli, result.total);
        auto direct = soundBlasterDirectAdvance(*phase_.callback, gameplaySystemBonusDirectRecord,
            SoundBlasterDirectCaller::gameplaySystemBonus);
        foregroundOplBlocks(direct.blocks, result);
        phase_.callback = std::move(direct.state);
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::gameplaySystemBonusDirectState,
            position() - origin_, operation.value,
            std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        foregroundFlags(NormalWorkKind::sti, result.total);
        plain(4 + 9 + 5 + 6 + 1, result.total); //Return wrappers and910A cleanup.
    }

    void gameplayArrivalStop(const NormalWorkOperation& operation, NormalWorkTiming& result,
        const DirectCampaignDomain domain = DirectCampaignDomain::level1) {
        const auto level2=domain==DirectCampaignDomain::level2;
        if (operation.count != 1 || operation.value != gameplayArrivalEvent
            || !phase_.sequenceSelection || !phase_.sequenceSelection->published
            || phase_.sequenceSelection->songOrdinal != (level2 ? level2MusicIdentity : 2) || !phase_.sequenceSelection->level1DirectResetCompleted
            || (level2 && (!retainedLevel2Program(phase_)
                || phase_.sequenceSelection->prepared.sequence!=phase_.callback->sequence))
            || !phase_.callback || !phase_.callback->program || !phase_.callback->active
            || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || !phase_.callback->driverEnabled || !phase_.soundBlasterInitialization
            || !phase_.soundBlasterInitialization->driverEnabled || phase_.callback->program->handleStatus != 0
            || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("arrival stop requires its retained campaign source owner");
        }
        if(level2) {
            //Entry ownership witness only: driver2ECE RET has no state/OPL mutation.
            result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::gameplayLevel2ArrivalStopState,
                position()-origin_,operation.value,std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        }
        plain(3 + 6 + 11 + 12 + 1 + 7,result.total); //B071..B075,B452,0478,dispatcher,2ECE RET,pops.
        foregroundFlags(NormalWorkKind::popFlagsEnabled,result.total); //17E3 POPF.
        plain(1 + 5 + 3,result.total); //17E4,0478 return,B452 epilogue;B078 excluded.
    }

    void gameplayArrival(const NormalWorkOperation& operation, NormalWorkTiming& result,
        const DirectCampaignDomain domain = DirectCampaignDomain::level1) {
        const auto level2=domain==DirectCampaignDomain::level2;
        if (operation.count != 1 || operation.value != gameplayArrivalEvent
            || !phase_.sequenceSelection || !phase_.sequenceSelection->published
            || phase_.sequenceSelection->songOrdinal != (level2 ? level2MusicIdentity : 2) || !phase_.sequenceSelection->level1DirectResetCompleted
            || (level2 && (!retainedLevel2Program(phase_)
                || phase_.sequenceSelection->prepared.sequence!=phase_.callback->sequence))
            || !phase_.callback || !phase_.callback->program || !phase_.callback->active
            || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || !phase_.callback->driverEnabled || phase_.callback->program->handleStatus != 0
            || phase_.callback->fade != 0 || !phase_.soundBlasterInitialization
            || !phase_.soundBlasterInitialization->driverEnabled || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("arrival audio requires its retained running campaign owner");
        }
        plain(3 + 18 + 23 + 3, result.total); //4705..4709,B3E6,0478/dispatcher,216A prefix.
        foregroundFlags(NormalWorkKind::cli, result.total);
        auto direct = soundBlasterDirectAdvance(*phase_.callback, gameplayArrivalDirectRecord,
            SoundBlasterDirectCaller::gameplayArrival);
        foregroundOplBlocks(direct.blocks, result);
        phase_.callback = std::move(direct.state);
        result.soundBlasterEffects.push_back({level2 ? SoundBlasterForegroundKind::gameplayLevel2ArrivalDirectState
                : SoundBlasterForegroundKind::gameplayArrivalDirectState,
            position() - origin_, operation.value,
            std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        foregroundFlags(NormalWorkKind::sti, result.total);
        plain(4 + 9 + 5 + 6 + 1, result.total); //Return wrappers and470C cleanup.
    }

    void gameplayGuardianDefeat(const NormalWorkOperation& operation, NormalWorkTiming& result,
        const DirectCampaignDomain domain = DirectCampaignDomain::level1) {
        const auto level2=domain==DirectCampaignDomain::level2;
        if (operation.count != 1 || operation.value != gameplayGuardianDefeatEvent
            || !phase_.sequenceSelection || !phase_.sequenceSelection->published
            || phase_.sequenceSelection->songOrdinal != (level2 ? level2MusicIdentity : 2) || !phase_.sequenceSelection->level1DirectResetCompleted
            || (level2 && (!retainedLevel2Program(phase_)
                || phase_.sequenceSelection->prepared.sequence!=phase_.callback->sequence))
            || !phase_.callback || !phase_.callback->program || !phase_.callback->active
            || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || !phase_.callback->driverEnabled || phase_.callback->program->handleStatus != 0
            || phase_.callback->fade != 0 || !phase_.soundBlasterInitialization
            || !phase_.soundBlasterInitialization->driverEnabled || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("guardian defeat audio requires its retained running campaign owner");
        }
        plain(3 + 18 + 23 + 3, result.total); //11F1..11F5,B3E6,0478/dispatcher,216A prefix.
        foregroundFlags(NormalWorkKind::cli, result.total);
        auto direct = soundBlasterDirectAdvance(*phase_.callback, gameplayGuardianDefeatDirectRecord,
            SoundBlasterDirectCaller::gameplayGuardianDefeat);
        foregroundOplBlocks(direct.blocks, result);
        phase_.callback = std::move(direct.state);
        result.soundBlasterEffects.push_back({level2 ? SoundBlasterForegroundKind::gameplayLevel2GuardianDefeatDirectState
                : SoundBlasterForegroundKind::gameplayGuardianDefeatDirectState,
            position() - origin_, operation.value,
            std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        foregroundFlags(NormalWorkKind::sti, result.total);
        plain(4 + 9 + 5 + 6 + 1, result.total); //Return wrappers and11F8 cleanup.
    }

    void gameplaySolidContact(const NormalWorkOperation& operation, NormalWorkTiming& result,
        const DirectCampaignDomain domain = DirectCampaignDomain::level1) {
        const auto level2=domain==DirectCampaignDomain::level2;
        if (operation.count != 1 || operation.value != gameplaySolidContactEvent
            || !phase_.sequenceSelection || !phase_.sequenceSelection->published
            || phase_.sequenceSelection->songOrdinal != (level2 ? level2MusicIdentity : 2) || !phase_.sequenceSelection->level1DirectResetCompleted
            || (level2 && (!retainedLevel2Program(phase_)
                || phase_.sequenceSelection->prepared.sequence!=phase_.callback->sequence))
            || !phase_.callback || !phase_.callback->program || !phase_.callback->active
            || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || !phase_.callback->driverEnabled || phase_.callback->program->handleStatus != 0
            || phase_.callback->fade != 0 || !phase_.soundBlasterInitialization
            || !phase_.soundBlasterInitialization->driverEnabled || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("solid contact audio requires the retained running Level1 owner");
        }
        plain(3 + 18 + 23 + 3, result.total); //0905/0908/0909,B3E6,0478/dispatcher,216A prefix.
        foregroundFlags(NormalWorkKind::cli, result.total);
        auto direct = soundBlasterDirectAdvance(*phase_.callback, gameplaySolidContactDirectRecord,
            SoundBlasterDirectCaller::gameplaySolidContact);
        foregroundOplBlocks(direct.blocks, result);
        phase_.callback = std::move(direct.state);
        result.soundBlasterEffects.push_back({level2 ? SoundBlasterForegroundKind::gameplayLevel2SolidContactDirectState
                : SoundBlasterForegroundKind::gameplaySolidContactDirectState,
            position() - origin_, operation.value,
            std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        foregroundFlags(NormalWorkKind::sti, result.total);
        plain(4 + 9 + 5 + 6 + 2, result.total); //Return wrappers;090C JMP then0845 ADD SP,2.
    }

    void gameplayPlayerCombat(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (operation.count != 1 || operation.value != gameplayPlayerCombatEvent
            || !phase_.sequenceSelection || !phase_.sequenceSelection->published
            || phase_.sequenceSelection->songOrdinal != 2 || !phase_.sequenceSelection->level1DirectResetCompleted
            || !phase_.callback || !phase_.callback->program || !phase_.callback->active
            || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || !phase_.callback->driverEnabled || phase_.callback->program->handleStatus != 0
            || phase_.callback->fade != 0 || !phase_.soundBlasterInitialization
            || !phase_.soundBlasterInitialization->driverEnabled || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("player combat audio requires the retained running Level1 owner");
        }
        plain(3 + 18 + 23 + 3, result.total); //0356..035A,B3E6,0478/dispatcher,216A prefix.
        foregroundFlags(NormalWorkKind::cli, result.total);
        auto direct = soundBlasterDirectAdvance(*phase_.callback, gameplayPlayerCombatDirectRecord,
            SoundBlasterDirectCaller::gameplayPlayerCombat);
        foregroundOplBlocks(direct.blocks, result);
        phase_.callback = std::move(direct.state);
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::gameplayPlayerCombatDirectState,
            position() - origin_, operation.value,
            std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        foregroundFlags(NormalWorkKind::sti, result.total);
        plain(4 + 9 + 5 + 6 + 1, result.total); //Return wrappers and035D cleanup.
    }

    void gameplayLevel2PlayerCombat(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (operation.count != 1 || operation.value != gameplayPlayerCombatEvent
            || !phase_.sequenceSelection || !phase_.sequenceSelection->published
            || phase_.sequenceSelection->songOrdinal != level2MusicIdentity || !phase_.sequenceSelection->level1DirectResetCompleted
            || !phase_.successAudio || phase_.successAudio->stage != SuccessAudioStage::returned
            || !phase_.callback || !phase_.callback->program || !phase_.callback->active
            || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || !phase_.callback->driverEnabled || phase_.callback->program->handleStatus != 0
            || phase_.callback->fade != 0 || phase_.callback->program->repeatCount != 0xffff
            || phase_.callback->sequence != phase_.sequenceSelection->prepared.sequence
            || !phase_.soundBlasterInitialization
            || !phase_.soundBlasterInitialization->driverEnabled || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("player combat audio requires the retained running Level2 owner");
        }
        plain(3 + 18 + 23 + 3, result.total); //0356..035A,B3E6,0478/dispatcher,216A prefix.
        foregroundFlags(NormalWorkKind::cli, result.total);
        auto direct = soundBlasterDirectAdvance(*phase_.callback, gameplayPlayerCombatDirectRecord,
            SoundBlasterDirectCaller::gameplayPlayerCombat);
        foregroundOplBlocks(direct.blocks, result);
        phase_.callback = std::move(direct.state);
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::gameplayLevel2PlayerCombatDirectState,
            position() - origin_, operation.value,
            std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        foregroundFlags(NormalWorkKind::sti, result.total);
        plain(4 + 9 + 5 + 6 + 1, result.total); //Return wrappers and035D cleanup.
    }

    void startupLogoFrame(const NormalWorkOperation& operation, SoundBlasterClockStep& step) {
        constexpr std::uint8_t frameCount = 17;
        constexpr std::uint16_t frameCountdown = 10;
        if (!phase_.sequenceSelection || !phase_.sequenceSelection->logoEventPlayed || operation.count != 1) {
            throw std::invalid_argument("logo frame requires the retained direct event");
        }
        auto& selection = *phase_.sequenceSelection;
        if (operation.value != selection.logoFramesCompleted || operation.value >= frameCount
            || !selection.countdownReturned) {
            throw std::invalid_argument("logo frame ordinal or wait boundary differs from retained source");
        }
        if (operation.kind == NormalWorkKind::startupLogoCountdown && !selection.logoFrameActive) {
            dispatch(Operation::plain, step); //6001 MOV countdown10, before copies/drawing.
            phase_.timer->globalCount = frameCountdown;
            selection.countdownReturned = false;
            selection.logoFrameActive = true;
            return;
        }
        if (operation.kind == NormalWorkKind::startupLogoFrameReturn && selection.logoFrameActive) {
            dispatch(Operation::plain, step); //606E RET, after completed6638 wait/epilogue.
            selection.logoFrameActive = false;
            ++selection.logoFramesCompleted;
            return;
        }
        throw std::invalid_argument("repeated or incomplete source logo frame");
    }

    void startupLogoSound(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (!phase_.sequenceSelection || !phase_.callback || !phase_.callback->program
            || phase_.sequenceSelection->logoFramesCompleted != 17 || phase_.sequenceSelection->logoFrameActive) {
            throw std::invalid_argument("logo sound-end requires the complete retained animation");
        }
        auto& selection = *phase_.sequenceSelection;
        if (operation.kind == NormalWorkKind::startupLogoOnce) {
            if (operation.count != 1 || operation.value != 1 || selection.logoSoundWaiting
                || phase_.callback->program->repeatCount != 0xffff) {
                throw std::invalid_argument("repeated or unsupported source logo once store");
            }
            dispatch(Operation::plain, result.total); //664C MOV handle.repeat,1.
            phase_.callback->program->repeatCount = 1;
            selection.logoSoundWaiting = true;
            result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::repeatOnce, position() - origin_, 1});
            return;
        }
        if (!selection.logoSoundWaiting || selection.logoSoundReturned || operation.count == 0
            || operation.count > 10000 || operation.value != 0) {
            throw std::invalid_argument("unsupported source logo handle-status poll");
        }
        for (std::uint64_t index = 0; index < operation.count; ++index) {
            dispatch(Operation::plain, result.total); //6651 CMP handle.status,FFFF.
            const auto read = phase_.callback->program->handleStatus;
            result.sequenceEndLastRead = read;
            ++result.sequenceEndPolls;
            plain(1, result.total); //6655 JNE, using the captured read even if IRQ changes status.
            if (read != 0xffff) { continue; }
            plain(3 + 2, result.total); //6657..665A return,624F cleanup/6252 JMP.
            selection.logoSoundReturned = true;
            result.sequenceEndReturned = true;
            return;
        }
    }

    void startupLogoStop(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (operation.count != 1 || operation.value != 0 || !phase_.sequenceSelection
            || !phase_.sequenceSelection->logoSoundReturned || phase_.sequenceSelection->logoCurrentCleared
            || !phase_.callback || !phase_.callback->program || phase_.callback->active
            || phase_.callback->program->handleStatus != 0xffff || phase_.callback->program->repeatCount != 1
            || phase_.callback->fade != 0 || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("source logo stop requires the retained completed sound wait");
        }
        plain(7 + 23 + 2, result.total); //625D/51A6 caller7,0478/dispatcher23,2D83/2D89.
        foregroundFlags(NormalWorkKind::cli, result.total); //2D90.
        auto stopped = soundBlasterStopAdvance(*phase_.callback);
        foregroundOplBlocks(stopped.blocks, result);
        phase_.callback = std::move(stopped.state);
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::stopComplete, position() - origin_, 0x0e});
        foregroundFlags(NormalWorkKind::sti, result.total); //2D94.
        plain(3 + 9 + 5 + 1, result.total); //2D95..2D9F,dispatcher,wrapper,51BA cleanup.
        dispatch(Operation::plain, result.total); //51BD clears B94, preserving allocation and physical storage.
        phase_.callback = soundBlasterDeselectState(*phase_.callback);
        phase_.sequenceSelection->logoCurrentCleared = true;
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::sequenceClear, position() - origin_, 0});
        plain(1, result.total); //51C3 RETF, returns6262.
    }

    void startupMenuDecision(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (operation.count != 1 || !phase_.timer || !phase_.sequenceSelection
            || !phase_.sequenceSelection->menuEntryCountdownWritten
            || phase_.sequenceSelection->recoveredMenuInput
            || phase_.sequenceSelection->beginDecisions >= 3
            || phase_.sequenceSelection->beginFadeRequests != 0
            || operation.value != (phase_.sequenceSelection->beginDecisions == 2 ? 1 : 0)) {
            throw std::invalid_argument("menu decision requires the retained neutral/press/release order");
        }
        plain(1,result.total); //160F MOV SI,AX.
        dispatch(Operation::plain,result.total); //1611 CMP [1DC1],0.
        const auto countdown = phase_.timer->globalCount;
        result.menuDecisionCountdownRead = countdown;
        plain(1,result.total); //1616 JNZ, using the captured count.
        if (countdown == 0) {
            throw std::invalid_argument("menu demo expiry is outside native Begin admission");
        }
        ++phase_.sequenceSelection->beginDecisions;
        plain(operation.value == 0 ? 18 : 3,result.total); //Loop includes next160C CALL; release ends1656.
    }

    void startupMenuInputDecision(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        const bool noAttract=operation.kind==NormalWorkKind::playableMenuNoAttractDecision;
        if(noAttract&&(!phase_.playableMenuNoAttract||phase_.playableCampaign||!phase_.timer||phase_.timer->activeMask==0
            ||(!retainedMenuProgram(phase_)&&!playableStartupMenuProgram(phase_)))) {
            throw std::invalid_argument("native no-attract poll requires its retained menu timer/program");
        }
        if (operation.count != 1 || operation.value > 1 || !phase_.timer || !phase_.sequenceSelection
            || !phase_.sequenceSelection->menuEntryCountdownWritten
            || phase_.sequenceSelection->beginDecisions != 0 || phase_.sequenceSelection->beginFadeRequests != 0
            || phase_.sequenceSelection->recoveredMenuActivated
            || operation.value != phase_.sequenceSelection->recoveredMenuResult
            || (operation.value == 1 && !phase_.sequenceSelection->recoveredMenuInput)) {
            throw std::invalid_argument("menu input decision requires its retained source poll result");
        }
        plain(1,result.total); //160F MOV SI,AX.
        dispatch(Operation::plain,result.total); //1611 CMP live countdown.
        const auto countdown = phase_.timer->globalCount;
        result.menuDecisionCountdownRead = countdown;
        plain(1,result.total); //1616 JNZ.
        if (countdown == 0) {
            if(!noAttract) {throw std::invalid_argument("menu demo expiry is outside native menu input admission");}
            dispatch(Operation::plain,result.total); //1618 MOV [1DC1],30000.
            phase_.timer->globalCount=menuCountdownRawIrqs;
            //Explicit native unavailable-attract provider: retain SI and owners; no source provider-call receipt.
        }
        phase_.sequenceSelection->recoveredMenuInput = true;
        phase_.sequenceSelection->recoveredMenuActivated = operation.value == 1;
        plain(operation.value == 0 ? 18 : 3,result.total);
    }

    void startupBeginFade(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        const auto menuEof=retainedMenuProgram(phase_) && retainedMenuEof(*phase_.callback);
        if (operation.count != 1 || !phase_.sequenceSelection || !phase_.callback
            || !phase_.callback->program || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || ((!phase_.callback->active || phase_.callback->program->handleStatus != 0) && !menuEof)
            || phase_.sequenceSelection->songOrdinal != 1 || !phase_.sequenceSelection->published
            || (phase_.sequenceSelection->recoveredMenuInput
                ? !phase_.sequenceSelection->recoveredMenuActivated
                    || phase_.sequenceSelection->recoveredMenuResult != 1
                    || phase_.sequenceSelection->beginDecisions != 0
                : phase_.sequenceSelection->beginDecisions != 3)
            || phase_.sequenceSelection->beginFadeRequests >= 2
            || operation.value != phase_.sequenceSelection->beginFadeRequests + 1
            || phase_.callback->program->handleVolume == 0 || phase_.callback->program->handleVolume > maximumMusicVolume
            || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("Begin fade requires an ordered positive-volume SNG1 request");
        }
        plain(29,result.total); //0478/17C8/2D3F/2D60 through handle-volume read; no CLI.
        const auto volume = phase_.callback->program->handleVolume;
        dispatch(Operation::plain,result.total); //2D6A AC store; AA remains inherited.
        phase_.callback->fade = volume;
        ++phase_.sequenceSelection->beginFadeRequests;
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::fadeRequest,position()-origin_,0x0c});
        plain(12,result.total); //2D6F..return/dispatcher through POP BP.
        foregroundFlags(NormalWorkKind::popFlagsEnabled,result.total); //17E3 POPF.
        plain(6,result.total); //17E4 RETF and0478 return.
    }

    void startupBeginCountdown(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        constexpr std::uint16_t beginRawIrqs = 480;
        if (operation.count != 1 || operation.value != beginRawIrqs || !phase_.timer
            || !phase_.sequenceSelection || phase_.sequenceSelection->beginFadeRequests != 2
            || phase_.sequenceSelection->beginCountdownWritten) {
            throw std::invalid_argument("Begin countdown requires both retained fade requests");
        }
        dispatch(Operation::plain,result.total); //015A MOV [1DC1],480.
        phase_.timer->globalCount = beginRawIrqs;
        phase_.sequenceSelection->beginCountdownWritten = true;
    }

    void startupBeginWait(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (operation.count == 0 || operation.count > 10000 || operation.value != 0
            || !phase_.timer || !phase_.sequenceSelection
            || !phase_.sequenceSelection->beginCountdownWritten
            || phase_.sequenceSelection->beginCountdownReturned) {
            throw std::invalid_argument("unsupported retained Begin countdown poll");
        }
        for (std::uint64_t index = 0; index < operation.count; ++index) {
            plain(1,result.total); //663B CALL87CA;6638 prologue is owned by caller.
            dispatch(Operation::plain,result.total); //87CA MOV AX,[1DC1].
            const auto read = phase_.timer->globalCount;
            result.countdownLastRead = read;
            ++result.countdownPolls;
            plain(3,result.total); //87CD RET;663E OR;6640 JNZ.
            if (read != 0) { continue; }
            plain(3,result.total); //6642..6645 epilogue.
            phase_.sequenceSelection->beginCountdownReturned = true;
            result.countdownReturned = true;
            return;
        }
    }

    void startupBeginStop(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (operation.count != 1 || operation.value != 0 || !phase_.sequenceSelection
            || !phase_.sequenceSelection->beginCountdownReturned
            || phase_.sequenceSelection->beginCurrentCleared || !phase_.callback
            || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || !phase_.callback->program || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("Begin stop requires the retained completed480-IRQ wait");
        }
        plain(6+23+2,result.total); //51A6 prefix,0478/dispatcher,2D83/2D89; outer017B CALLF excluded.
        foregroundFlags(NormalWorkKind::cli,result.total); //2D90.
        auto stopped = soundBlasterStopAdvance(*phase_.callback);
        foregroundOplBlocks(stopped.blocks,result);
        phase_.callback = std::move(stopped.state); //Key-off metadata only; status is still live.
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::stopComplete,position()-origin_,0x0e});
        foregroundFlags(NormalWorkKind::sti,result.total); //2D94.
        plain(1,result.total); //2D95 MOV BX; consumes the one-instruction STI inhibit.
        dispatch(Operation::plain,result.total); //2D9A status store, after any eligible callback.
        phase_.callback->program->handleStatus = 0xffff;
        phase_.callback->active = false;
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::sequenceEnd,position()-origin_,0xffff});
        plain(16,result.total); //2D9F RET,dispatcher9,wrapper5,51BA cleanup1.
        dispatch(Operation::plain,result.total); //51BD current clear, without releasing storage.
        phase_.callback = phase_.playableStartup ? soundBlasterLevel2RetryDeselectState(*phase_.callback)
            : soundBlasterDeselectState(*phase_.callback);
        phase_.sequenceSelection->beginCurrentCleared = true;
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::sequenceClear,position()-origin_,0});
        plain(1,result.total); //51C3 RETF ->0180.
    }

    void successMusic(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        using Stage = SuccessAudioStage;
        auto& owner = phase_.successAudio;
        if (operation.count != 1 || !owner || !phase_.callback || !phase_.callback->program
            || !phase_.timer || !phase_.sequenceSelection
            || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("success music requires its bound retained driver/timer");
        }
        if (operation.kind == NormalWorkKind::successCountdownStore) {
            if (owner->stage != Stage::playing || operation.value != 992) {
                throw std::invalid_argument("7072 requires its actual03E0 countdown once");
            }
            dispatch(Operation::plain,result.total); //7072 MOV[1DC1],03E0.
            phase_.timer->globalCount = operation.value;
            owner->stage = Stage::countdownStored;
            return;
        }
        if (operation.value != 0) {
            throw std::invalid_argument("success music child has no numeric payload");
        }
        if (operation.kind == NormalWorkKind::successMusicActivate) {
            if (owner->stage != Stage::entryStopped
                || phase_.callback->domain != SoundBlasterCallbackDomain::successNoHandle) {
                throw std::invalid_argument("success activation requires its source current clear");
            }
            plain(4 + 14,result.total); //6F65..6F6C,02E6..030A index0/non-null.
            dispatch(Operation::plain,result.total); //030C repeatFFFF on owned target handle.
            phase_.callback->program->repeatCount = 0xffff;
            result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::successActivationState,
                position()-origin_,0,std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
            plain(4 + 11 + 12,result.total); //0311..0316,0478,dispatcher CALL17E5.
            const auto activation = soundBlasterSequenceOperations(owner->sequence,DriverSequenceAdmission::successInheritedChannels);
            std::uint64_t cursor = 0;
            for (std::size_t index = 0; index < activation.effects.size(); ++index) {
                const auto& effect = activation.effects[index];
                plain(static_cast<std::size_t>(effect.afterDispatch-cursor),result.total);
                cursor = effect.afterDispatch;
                phase_.callback = applyDriverSequenceEffect(*phase_.callback,effect);
                result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::successActivationState,
                    position()-origin_,static_cast<std::uint16_t>(index+1),
                    std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
            }
            std::uint64_t dispatches = 0;
            for (const auto& work : activation.operations) {
                if (work.kind != NormalWorkKind::instructions) {
                    throw std::logic_error("success activation contains an unmapped driver operation");
                }
                dispatches += work.count;
            }
            plain(static_cast<std::size_t>(dispatches-cursor) + 7,result.total); //Kernel RET/dispatcher pops.
            foregroundFlags(NormalWorkKind::popFlagsEnabled,result.total); //17E3 restores entryIF.
            plain(1 + 5 + 3,result.total); //17E4;0478 return;031B/031E/0323.
            dispatch(Operation::plain,result.total); //0325 publication after eligible IRQ work.
            auto selected = *phase_.callback;
            selected.domain = SoundBlasterCallbackDomain::sequence;
            selected.sequence = owner->sequence;
            selected.position = 36;
            selected.wait = 0;
            selected.operation = 2;
            selected.active = true;
            phase_.callback = std::move(selected);
            phase_.sequenceSelection->prepared = *phase_.callback;
            phase_.sequenceSelection->songOrdinal = 3;
            phase_.sequenceSelection->published = true;
            result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::successSequencePublish,
                position()-origin_,3,std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
            plain(5 + 3,result.total); //0329..032F;6F71/6F74=485A/6F77.
            dispatch(Operation::plain,result.total); //6F79 repeat1; callbacks before this store seeFFFF.
            phase_.callback->program->repeatCount = 1;
            result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::repeatOnce,position()-origin_,1});
            owner->stage = Stage::playing;
            return;
        }
        const bool entering = operation.kind == NormalWorkKind::successMusicEntryStop;
        if ((!entering && operation.kind != NormalWorkKind::successMusicReturnStop)
            || owner->stage != (entering ? Stage::resourceBound : Stage::countdownStored)
            || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || phase_.sequenceSelection->songOrdinal != (entering ? 2 : 3)) {
            throw std::invalid_argument("success stop requires its source-selected handle/stage");
        }
        plain(6 + 23 + 2,result.total); //51A6 prefix,0478/dispatcher,2D83/2D89.
        foregroundFlags(NormalWorkKind::cli,result.total);
        auto stopped = soundBlasterStopAdvance(*phase_.callback,SoundBlasterStopProfile::retainedDirectTimers);
        foregroundOplBlocks(stopped.blocks,result);
        phase_.callback = std::move(stopped.state);
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::successStopComplete,position()-origin_,0x0e});
        foregroundFlags(NormalWorkKind::sti,result.total);
        plain(1,result.total); //2D95 consumes STI inhibit.
        dispatch(Operation::plain,result.total); //2D9A follows any newly eligible callback.
        phase_.callback->program->handleStatus = 0xffff;
        phase_.callback->active = false;
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::sequenceEnd,position()-origin_,0xffff});
        plain(16,result.total); //2D9F,dispatcher/wrapper,51BA cleanup.
        dispatch(Operation::plain,result.total); //51BD clearsB94, without freeing the handle.
        phase_.callback = soundBlasterSuccessDeselectState(*phase_.callback);
        owner->stage = entering ? Stage::entryStopped : Stage::returned;
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::successCurrentClear,position()-origin_,0,
            std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        plain(1,result.total); //51C3 RETF;6F60/7204 CALLF excluded.
    }

    enum class Level2MusicDomain { successReturn, retry };
    void level2Music(const NormalWorkOperation& operation, NormalWorkTiming& result,
        const Level2MusicDomain domain = Level2MusicDomain::successReturn) {
        const bool retry = domain == Level2MusicDomain::retry;
        if (operation.count != 1 || operation.value != 0
            || (!retry && (!phase_.successAudio || phase_.successAudio->stage != SuccessAudioStage::returned))
            || (retry && !phase_.level2RetryMusic) || !phase_.callback
            || !phase_.callback->program || !phase_.timer || !phase_.sequenceSelection
            || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("Level2 music requires the completed retained success owner");
        }
        if (operation.kind == NormalWorkKind::level2MusicEmptyStop) {
            if (phase_.callback->domain != SoundBlasterCallbackDomain::successNoHandle) {
                throw std::invalid_argument("Level2 cleanup51A6 requires actual current0");
            }
            plain(4,result.total); //51A6/51AB/51BD/51C3; outer CALL belongs to parent.
            return;
        }
        auto& selection = *phase_.sequenceSelection;
        if (selection.songOrdinal != level2MusicIdentity) {
            throw std::invalid_argument("Level2 music requires its explicit resource provider binding");
        }
        const bool activate = operation.kind == (retry ? NormalWorkKind::level2RetryMusicActivate
            : NormalWorkKind::level2MusicActivate);
        const auto activationKind = retry ? SoundBlasterForegroundKind::level2RetryActivationState
            : SoundBlasterForegroundKind::level2ActivationState;
        const auto resetKind = retry ? SoundBlasterForegroundKind::level2RetryResetState
            : SoundBlasterForegroundKind::level2ResetState;
        if (activate) {
            if (selection.published || selection.level1DirectResetCompleted
                || phase_.callback->domain != (retry ? SoundBlasterCallbackDomain::level2RetryNoHandle
                    : SoundBlasterCallbackDomain::successNoHandle)
                || (retry && phase_.level2RetryMusic->stage != AudioClockLevel2RetryMusicStage::resourceBound)) {
                throw std::invalid_argument("Level2 activation requires the stopped retained source handle");
            }
            plain(4+14,result.total); //5098..509F;02E6..030A index0/non-null.
            dispatch(Operation::plain,result.total); //030C repeatFFFF.
            phase_.callback->program->repeatCount = 0xffff;
            result.soundBlasterEffects.push_back({activationKind,
                position()-origin_,0,std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
            plain(4+11+12,result.total); //0311..0316,0478,dispatcher CALL17E5.
        } else if (operation.kind != (retry ? NormalWorkKind::level2RetryDirectReset : NormalWorkKind::level2DirectReset)
            || (retry && phase_.level2RetryMusic->stage != AudioClockLevel2RetryMusicStage::playing) || !selection.published
            || selection.level1DirectResetCompleted || phase_.callback->domain != SoundBlasterCallbackDomain::sequence
            || !phase_.callback->active || phase_.callback->program->handleStatus != 0) {
            throw std::invalid_argument("Level2 reset requires its selected destination once");
        } else {
            plain(3+11+12,result.total); //01A8/01AB/01AC;0478/dispatcher CALL206B.
        }
        const auto body = activate
            ? soundBlasterSequenceOperations(selection.prepared.sequence,DriverSequenceAdmission::inheritedChannels)
            : soundBlasterDirectResetOperations();
        std::uint64_t cursor = 0;
        for (std::size_t index=0;index<body.effects.size();++index) {
            const auto& effect=body.effects[index];
            plain(static_cast<std::size_t>(effect.afterDispatch-cursor),result.total);
            cursor=effect.afterDispatch;
            phase_.callback=applyDriverSequenceEffect(*phase_.callback,effect);
            result.soundBlasterEffects.push_back({activate ? activationKind : resetKind,
                position()-origin_,static_cast<std::uint16_t>(index+1),
                std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        }
        std::uint64_t dispatches=0;
        for (const auto& work:body.operations) {
            if (work.kind!=NormalWorkKind::instructions) {
                throw std::logic_error("Level2 music body contains unrepresented driver work");
            }
            dispatches+=work.count;
        }
        plain(static_cast<std::size_t>(dispatches-cursor)+7,result.total);
        foregroundFlags(NormalWorkKind::popFlagsEnabled,result.total); //17E3 restores entryIF.
        if (activate) {
            plain(1+5+3,result.total); //17E4;0478 return;031B/031E/0323.
            dispatch(Operation::plain,result.total); //0325 actual current publication.
            auto selected=*phase_.callback;
            selected.domain=SoundBlasterCallbackDomain::sequence;
            selected.sequence=selection.prepared.sequence;
            selected.position=level2MusicBodyOffset;selected.wait=0;selected.operation=2;selected.active=true;
            phase_.callback=std::move(selected);
            selection.prepared=*phase_.callback;
            selection.published=true;
            if (retry) {phase_.level2RetryMusic->stage=AudioClockLevel2RetryMusicStage::playing;}
            result.soundBlasterEffects.push_back({retry ? SoundBlasterForegroundKind::level2RetrySequencePublish
                    : SoundBlasterForegroundKind::level2SequencePublish,
                position()-origin_,level2MusicIdentity,std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
            plain(5+4,result.total); //0329..032F;50A4/50A7/50AA/50AC.
            return;
        }
        plain(1+5+1,result.total); //17E4;0478 return;01B1 cleanup.
        selection.level1DirectResetCompleted=true; //Existing01A8 completion flag, destination remains explicit4.
        if (retry) {phase_.level2RetryMusic->stage=AudioClockLevel2RetryMusicStage::reset;}
        result.soundBlasterEffects.push_back({resetKind,
            position()-origin_,0,std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
    }

    void level2RetryMusicStop(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (operation.count != 1 || operation.value != 0 || !phase_.level2RetryMusic
            || !phase_.sequenceSelection || phase_.sequenceSelection->songOrdinal != level2MusicIdentity
            || !phase_.sequenceSelection->published || !phase_.sequenceSelection->level1DirectResetCompleted
            || !phase_.callback || !phase_.callback->program || phase_.interrupts != AudioClockInterrupts::enabled) {
            throw std::invalid_argument("Level2 retry stop requires its retained destination owner");
        }
        auto& stage=phase_.level2RetryMusic->stage;
        if (operation.kind == NormalWorkKind::level2RetryMusicEmptyStop) {
            if (stage != AudioClockLevel2RetryMusicStage::stopped
                || phase_.callback->domain != SoundBlasterCallbackDomain::level2RetryNoHandle) {
                throw std::invalid_argument("Level2 retry empty stop requires the actual prior current clear");
            }
            plain(4,result.total); //51A6/51AB/51BD/51C3; parent owns4F50 CALL.
            stage=AudioClockLevel2RetryMusicStage::emptyStopped;
            return;
        }
        if (stage != AudioClockLevel2RetryMusicStage::entered
            || !retainedLevel2Program(phase_)
            || phase_.callback->sequence != phase_.sequenceSelection->prepared.sequence) {
            throw std::invalid_argument("Level2 retry first stop requires the selected owned handle");
        }
        plain(6+23+2,result.total); //51A6..51B5;0478/dispatcher;2D83/2D89.
        foregroundFlags(NormalWorkKind::cli,result.total);
        auto stopped=soundBlasterStopAdvance(*phase_.callback,SoundBlasterStopProfile::retainedDirectTimers);
        foregroundOplBlocks(stopped.blocks,result);
        phase_.callback=std::move(stopped.state);
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::level2RetryStopComplete,position()-origin_,0x0e});
        foregroundFlags(NormalWorkKind::sti,result.total);
        plain(1,result.total); //2D95 consumes STI inhibit.
        dispatch(Operation::plain,result.total); //2D9A follows any eligible callback.
        phase_.callback->program->handleStatus=0xffff;phase_.callback->active=false;
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::sequenceEnd,position()-origin_,0xffff});
        plain(16,result.total);
        dispatch(Operation::plain,result.total); //51BD current=NULL; physical handle remains allocated.
        phase_.callback=soundBlasterLevel2RetryDeselectState(*phase_.callback);
        stage=AudioClockLevel2RetryMusicStage::stopped;
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::level2RetryCurrentClear,position()-origin_,0,
            std::make_shared<const SoundBlasterCallbackState>(*phase_.callback)});
        plain(1,result.total);
    }

    void level1RetryMusicStop(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (operation.count != 1 || operation.value != 0 || !phase_.sequenceSelection
            || phase_.sequenceSelection->songOrdinal != 2 || !phase_.sequenceSelection->published
            || !phase_.sequenceSelection->level1DirectResetCompleted || !phase_.callback
            || phase_.interrupts != AudioClockInterrupts::enabled
            || std::any_of(phase_.callback->voices.directTimers.begin(),phase_.callback->voices.directTimers.end(),
                [](const auto timer) { return timer != 0; })) {
            throw std::invalid_argument("retry stop requires the retained Level1 owner and qualified no-handle timers");
        }
        if (phase_.sequenceSelection->level1RetryCurrentCleared) {
            if (phase_.callback->domain != SoundBlasterCallbackDomain::noHandle) {
                throw std::invalid_argument("retry second stop requires the actual prior current clear");
            }
            plain(4,result.total); //51A6/51AB/51BD/51C3;4F50 outer CALL excluded.
            return;
        }
        if (phase_.callback->domain != SoundBlasterCallbackDomain::sequence || !phase_.callback->program) {
            throw std::invalid_argument("retry first stop requires the selected owned handle");
        }
        plain(6+23+2,result.total); //51A6..51B5;0478/dispatcher;2D83/2D89.
        foregroundFlags(NormalWorkKind::cli,result.total);
        auto stopped = soundBlasterStopAdvance(*phase_.callback,SoundBlasterStopProfile::retainedDirectTimers);
        foregroundOplBlocks(stopped.blocks,result);
        phase_.callback = std::move(stopped.state);
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::stopComplete,position()-origin_,0x0e});
        foregroundFlags(NormalWorkKind::sti,result.total);
        plain(1,result.total); //2D95 consumes STI's one-instruction inhibit.
        dispatch(Operation::plain,result.total); //2D9A after any eligible callback.
        phase_.callback->program->handleStatus = 0xffff;
        phase_.callback->active = false;
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::sequenceEnd,position()-origin_,0xffff});
        plain(16,result.total); //2D9F,dispatcher/wrapper return,51BA cleanup.
        dispatch(Operation::plain,result.total); //51BD clears current; physical handle remains allocated.
        phase_.sequenceSelection->level1RetryStoppedHandle = *phase_.callback;
        phase_.callback = soundBlasterDeselectState(*phase_.callback);
        phase_.sequenceSelection->level1RetryCurrentCleared = true;
        result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::sequenceClear,position()-origin_,0});
        plain(1,result.total); //51C3 RETF;0464 outer CALL excluded.
    }

    void timerCallbackEffect(const NormalWorkOperation& operation, SoundBlasterClockStep& step) {
        using Kind = NormalWorkKind;
        using Registration = AudioClockCallbackRegistrationStage;
        if (!phase_.timerInstall || !phase_.timer
            || phase_.timerInstall->stage != AudioClockTimerInstallStage::installed || operation.count != 1) {
            throw std::invalid_argument("callback registration requires the installed native timer");
        }
        Registration before;
        Registration after;
        std::uint16_t value;
        switch (operation.kind) {
        case Kind::timerCallbackReload:
            before = Registration::fresh; after = Registration::reloadWritten; value = nativeCallbackPeriod;
            break;
        case Kind::timerCallbackCount:
            before = Registration::reloadWritten; after = Registration::countWritten; value = nativeCallbackPeriod;
            break;
        case Kind::timerCallbackOffset:
            before = Registration::countWritten; after = Registration::offsetWritten; value = nativeCallbackOffset;
            break;
        case Kind::timerCallbackSegment:
            before = Registration::offsetWritten; after = Registration::segmentWritten; value = nativeCallbackSegment;
            break;
        case Kind::timerCallbackActivate:
            before = Registration::segmentWritten; after = Registration::active; value = soundBlasterCallbackMask;
            break;
        default:
            throw std::invalid_argument("unsupported callback registration effect");
        }
        if (phase_.timerInstall->registration != before || operation.value != value || phase_.timer->activeMask != 0
            || phase_.interrupts != (after == Registration::active ? AudioClockInterrupts::masked : AudioClockInterrupts::enabled)) {
            throw std::invalid_argument("callback registration requires ordered source writes and flags");
        }
        if (after == Registration::active) {
            if (!phase_.callback || phase_.callback->domain != SoundBlasterCallbackDomain::noHandle) {
                throw std::invalid_argument("fresh callback registration requires an initialized no-handle driver");
            }
            soundBlasterCallbackAdvance(*phase_.callback);
        }
        dispatch(Operation::plain, step);
        switch (operation.kind) {
        case Kind::timerCallbackReload: phase_.timer->callbackReload = value; break;
        case Kind::timerCallbackCount: phase_.timer->callbackCount = value; break;
        case Kind::timerCallbackOffset: phase_.timerInstall->callbackOffset = value; break;
        case Kind::timerCallbackSegment: phase_.timerInstall->callbackSegment = value; break;
        case Kind::timerCallbackActivate: phase_.timer->activeMask = value; break;
        default: break;
        }
        phase_.timerInstall->registration = after;
    }

    void timerInstallEffect(const NormalWorkOperation& operation, SoundBlasterClockStep& step) {
        using Kind = NormalWorkKind;
        using Stage = AudioClockTimerInstallStage;
        if (!phase_.timerInstall || operation.count != 1) {
            throw std::invalid_argument("timer install effect requires one owned source instruction");
        }
        auto& install = *phase_.timerInstall;
        Stage before;
        Stage after;
        std::uint16_t value;
        auto dispatchKind = Operation::plain;
        switch (operation.kind) {
        case Kind::timerInstallClearMask:
            before = Stage::fresh; after = Stage::maskCleared; value = 0;
            break;
        case Kind::timerInstallSaveBiosVector:
            before = Stage::maskCleared; after = Stage::biosVectorSaved; value = 0;
            break;
        case Kind::timerInstallBiosReload:
            before = Stage::biosVectorSaved; after = Stage::biosReloadWritten; value = installedBiosReload;
            break;
        case Kind::timerInstallBiosCount:
            before = Stage::biosReloadWritten; after = Stage::biosCountWritten; value = installedBiosReload;
            break;
        case Kind::timerInstallDivisor:
            before = Stage::biosCountWritten; after = Stage::divisorWritten; value = installedPitDivisor;
            break;
        case Kind::timerInstallPitControl:
            before = Stage::divisorWritten; after = Stage::controlWritten; value = 0x36;
            dispatchKind = Operation::write;
            break;
        case Kind::timerInstallPitLow:
            before = Stage::controlWritten; after = Stage::lowByteWritten; value = installedPitDivisor & 0xff;
            dispatchKind = Operation::write;
            break;
        case Kind::timerInstallPitHigh:
            before = Stage::lowByteWritten; after = Stage::highByteWritten; value = installedPitDivisor >> 8;
            dispatchKind = Operation::write;
            break;
        case Kind::timerInstallPicMask:
            before = Stage::highByteWritten; after = Stage::picMaskWritten; value = installedPicMask;
            dispatchKind = Operation::write;
            break;
        case Kind::timerInstallVector:
            before = Stage::picMaskWritten; after = Stage::vectorInstalled; value = 0;
            break;
        case Kind::timerInstallComplete:
            before = Stage::vectorInstalled; after = Stage::installed; value = 1;
            break;
        default:
            throw std::invalid_argument("unsupported timer install effect");
        }
        const bool maskedWrite = before >= Stage::divisorWritten && before < Stage::vectorInstalled;
        if (install.stage != before || operation.value != value
            || (maskedWrite && phase_.interrupts != AudioClockInterrupts::masked)
            || (before < Stage::divisorWritten && phase_.interrupts != AudioClockInterrupts::enabled)
            || (after == Stage::installed && phase_.interrupts != AudioClockInterrupts::enabled)) {
            throw std::invalid_argument("timer installation requires ordered source effects and flags");
        }
        if (operation.kind == Kind::timerInstallSaveBiosVector || operation.kind == Kind::timerInstallVector) {
            const auto callerInterrupts = phase_.interrupts;
            dispatch(Operation::plain, step); // Source INT21 software entry.
            phase_.interrupts = AudioClockInterrupts::masked;
            foregroundFlags(Kind::sti, step); // Default DOS stub F000:D100.
            dispatch(Operation::plain, step); // D101 FE38 callback, after the STI shadow.
            install.stage = after;
            if (operation.kind == Kind::timerInstallVector) {
                phase_.interruptDelivery = AudioClockInterruptDelivery::residentTimerIrq0;
            }
            refillReady(); // Host callback return runs PIC without a failed-loop charge.
            dispatch(Operation::plain, step); // D105 IRET, after any newly owned IRQ.
            phase_.interrupts = callerInterrupts;
            if (callerInterrupts == AudioClockInterrupts::enabled && phase_.irqCheckRequested) {
                refillReady();
            }
            return;
        }
        dispatch(dispatchKind, step);
        const auto now = phase_.devices->picTicks + localPosition() / static_cast<double>(cyclesPerMillisecond);
        switch (operation.kind) {
        case Kind::timerInstallClearMask:
            phase_.timer->activeMask = 0;
            break;
        case Kind::timerInstallBiosReload:
            phase_.timer->biosReload = value;
            break;
        case Kind::timerInstallBiosCount:
            phase_.timer->biosCount = value;
            break;
        case Kind::timerInstallDivisor:
            install.pitDivisor = value;
            break;
        case Kind::timerInstallPitControl:
            // The old mode3 first-half output is high before new_mode is set.
            if (phase_.devices->pitCycleBase != 0 || now < phase_.devices->pitStartMilliseconds
                || now >= phase_.devices->pitStartMilliseconds + phase_.pitPeriodMilliseconds / 2) {
                throw std::invalid_argument("timer control requires the owned old PIT first half");
            }
            phase_.events.erase(std::remove_if(phase_.events.begin(), phase_.events.end(), [](const auto& event) {
                return event.kind == AudioClockEventKind::pit;
            }), phase_.events.end());
            raiseTimerInput();
            break;
        case Kind::timerInstallPitLow:
            install.pitLowByte = static_cast<std::uint8_t>(value);
            break;
        case Kind::timerInstallPitHigh:
            phase_.pitPeriodMilliseconds = installedPitDivisor * 1000.0 / pitInputFrequency;
            phase_.devices->pitStartMilliseconds = now;
            phase_.devices->pitNowMilliseconds = now;
            phase_.devices->pitCycleBase = 0;
            addClockEvent({localPosition() / static_cast<double>(cyclesPerMillisecond)
                + phase_.pitPeriodMilliseconds, AudioClockEventKind::pit, 0});
            raiseTimerInput();
            break;
        case Kind::timerInstallPicMask:
            install.masterPicMask = static_cast<std::uint8_t>(value);
            break;
        default:
            break;
        }
        install.stage = after;
    }

    void string(const std::uint64_t count, NormalWorkTiming& result) {
        for (std::uint64_t index = 0; index < count; ++index) {
            dispatch(Operation::plain, result.total);
            --phase_.remainingCycles;
            ++result.stringIterations;
            checkHorizon();
        }
    }

    void repeatString(std::uint64_t count, NormalWorkTiming& result) {
        do {
            dispatch(Operation::plain, result.total);
            auto transfers = std::min(count, maximumRepTransfers);
            while (transfers > 0) {
                --count;
                --transfers;
                --phase_.remainingCycles;
                ++result.stringIterations;
                if (phase_.remainingCycles <= 0) {
                    break;
                }
            }
            checkHorizon();
            if (count > 0) {
                ++result.stringRestarts;
            }
        } while (count > 0);
    }

    BiosClockTiming biosChain() {
        BiosClockTiming result{};
        runBiosChain(result.total, &result.checkpoints);
        finish(result.total, origin_);
        result.phase = phase_;
        return result;
    }

    PageFlipClockTiming pageFlip(const PageFlipClockState& state) {
        constexpr std::uint16_t firstPage = 0xa000;
        const std::uint16_t secondPage = state.layout == PageFlipLayout::compatibility ? 0xa400 : 0xa800;
        constexpr std::uint16_t colorCrtcPort = 0x3d4;
        if (!phase_.devices || !phase_.devices->frame
            || phase_.frameStart != AudioClockFrameStart::activeUnlockedVgaSurface
            || phase_.devices->retraceStartMilliseconds >= phase_.devices->retraceEndMilliseconds
            || phase_.devices->retraceEndMilliseconds >= phase_.devices->framePeriodMilliseconds
            || state.crtcPort != colorCrtcPort
            || (state.layout != PageFlipLayout::compatibility && state.layout != PageFlipLayout::recoveredStartup)
            || !((state.backPage == firstPage && state.frontPage == secondPage)
                || (state.backPage == secondPage && state.frontPage == firstPage))
            || (state.synchronization != PageFlipSynchronization::disabled
                && state.synchronization != PageFlipSynchronization::enabled)) {
            throw std::invalid_argument("unsupported source page-flip profile");
        }
        PageFlipClockTiming result{};
        result.state = state;
        auto& step = result.timing.total;
        // Main02d8..02dd,22b1 JMPF, resident1023..1030.
        plain(12, step);
        std::swap(result.state.backPage, result.state.frontPage);
        //1032 XCHG,1036 store, four SHR, BDA/CRTC setup through104d.
        plain(11, step);
        dispatch(Operation::wordWrite, step);
        phase_.devices->displayStart = (phase_.devices->displayStart & 0xff00ffU)
            | (static_cast<std::uint32_t>((state.backPage >> 4) & 0xff) << 8);
        plain(3, step); //1050 argument, OR, conditional branch.
        if (state.synchronization == PageFlipSynchronization::enabled) {
            plain(2, step); // MOV CL,8 / MOV DX,3DA.
            const auto retrace = [&]() {
                dispatch(Operation::statusRead, step);
                const auto& devices = *phase_.devices;
                const auto inFrame = devices.picTicks
                    + localPosition() / static_cast<double>(cyclesPerMillisecond)
                    - devices.frameStartMilliseconds;
                const bool high = inFrame >= devices.retraceStartMilliseconds
                    && inFrame <= devices.retraceEndMilliseconds;
                plain(2, step); // AND AL,CL and Jcc sample the retained read value.
                return high;
            };
            bool high;
            do {
                ++result.lowReads;
                high = retrace();
            } while (high);
            do {
                ++result.highReads;
                high = retrace();
            } while (!high);
        }
        plain(4, step); // POP DS,ES,BP / RETF;02e2 excluded.
        finish(step, origin_);
        result.timing.phase = phase_;
        result.timing.callbacks = callbacks_;
        result.timing.vgaServices = vgaServices_;
        result.timing.keyboardIrqs = keyboardIrqs_;
        return result;
    }

    PaletteClockTiming palette(const PaletteWorkState& state, const PaletteRequest& request) {
        constexpr std::uint16_t dacModePort = 0x3c7;
        constexpr std::uint16_t dacIndexPort = dacModePort + 1;
        constexpr std::uint16_t dacDataPort = dacModePort + 2;
        if (!phase_.devices || !phase_.devices->frame
            || phase_.frameStart != AudioClockFrameStart::activeUnlockedVgaSurface
            || (phase_.interruptDelivery != AudioClockInterruptDelivery::residentTimerIrq0
                && phase_.interruptDelivery != AudioClockInterruptDelivery::residentTimerKeyboard)
            || phase_.devices->retraceStartMilliseconds >= phase_.devices->retraceEndMilliseconds
            || phase_.devices->retraceEndMilliseconds >= phase_.devices->framePeriodMilliseconds) {
            throw std::invalid_argument("unsupported source palette clock profile");
        }
        PaletteClockTiming result{};
        result.state = transformPalette(state, request);
        if (state.dac.mode == PaletteDacMode::write && state.dac.pelIndex == 2 && !state.dac.latch) {
            throw std::invalid_argument("missing retained DAC latch for preliminary RGB commit");
        }
        auto latch = state.dac.latch.value_or(std::array<std::uint8_t, vgaPaletteComponents>{});
        auto& timing = result.timing;
        auto& step = timing.total;
        const auto components = request.count * vgaPaletteComponents;
        plain(30, step); // Loaded5dfa through5e39.
        for (std::size_t i = 0; i < components; ++i) {
            plain(1, step);
            string(1, timing); // LODSB from the immutable source bank.
            plain(11, step);
            string(1, timing); // STOSB into the distinct output bank.
            plain(7, step);
            if ((i + 1) % vgaPaletteComponents == 0) {
                plain(2, step); // Reset target-component pointer every RGB triplet.
            }
            plain(1, step);
        }
        plain(2, step); // PUSH CS/CALL068a; upload is nested once.
        plain(10, step); //068a through069b, before MOV DX,3DA.
        result.waitEntryCycles = position() - origin_;
        plain(1, step);
        bool high;
        do {
            ++result.highReads;
            dispatch(Operation::statusRead, step);
            const auto& devices = *phase_.devices;
            const auto inFrame = devices.picTicks
                + localPosition() / static_cast<double>(cyclesPerMillisecond)
                - devices.frameStartMilliseconds;
            high = inFrame >= devices.retraceStartMilliseconds
                && inFrame <= devices.retraceEndMilliseconds;
            plain(2, step); // AND8/JZ: only wait for high, never first wait for low.
        } while (!high);

        const auto write = [&](const std::uint16_t port, const std::uint8_t value) {
            dispatch(Operation::write, step);
            result.writes.push_back({position() - origin_, port, value});
            auto& dac = result.state.dac;
            if (port == dacIndexPort) {
                dac.mode = PaletteDacMode::write;
                dac.writeIndex = value;
                dac.readIndex = static_cast<std::uint16_t>((value + vgaPaletteColors - 1) % vgaPaletteColors);
                dac.pelIndex = 0;
                return;
            }
            latch[dac.pelIndex] = value;
            if (++dac.pelIndex == vgaPaletteComponents) {
                std::copy(latch.begin(), latch.end(), dac.colors.begin() + dac.writeIndex * vgaPaletteComponents);
                result.commits.push_back({position() - origin_, dac.writeIndex, latch});
                dac.pelIndex = 0;
                dac.readIndex = dac.writeIndex;
                dac.writeIndex = static_cast<std::uint16_t>((dac.writeIndex + 1) % vgaPaletteColors);
            }
        };
        plain(1, step); // MOV DX,3C7.
        dispatch(Operation::statusRead, step); // Generic ISA read, no OPL recovery delay.
        plain(3, step); // AND3/CMP3/JZ.
        if (state.dac.mode == PaletteDacMode::write) {
            plain(1, step);
            write(dacDataPort, static_cast<std::uint8_t>(state.dac.mode));
        }
        plain(2, step); // MOV DX,3C8/MOV AX,BX.
        write(dacIndexPort, static_cast<std::uint8_t>(request.start));
        plain(1, step); // MOV DX,3C9.
        result.uploadStartCycles = position() - origin_;
        foregroundFlags(NormalWorkKind::cli, step);
        const auto begin = request.start * vgaPaletteComponents;
        for (std::size_t i = begin; i < begin + components; ++i) {
            string(1, timing);
            write(dacDataPort, result.state.output[i]);
            plain(1, step); // LOOP.
        }
        result.uploadEndCycles = position() - origin_;
        foregroundFlags(NormalWorkKind::sti, step);
        plain(4, step); // First POP owns the STI shadow, then source upload RETF.
        plain(7, step); //077a cleanup and RETF; resident1b38 suffix excluded.
        result.state.dac.latch = latch;
        finish(step, origin_);
        timing.phase = phase_;
        timing.callbacks = callbacks_;
        timing.vgaServices = vgaServices_;
        timing.keyboardIrqs = keyboardIrqs_;
        return result;
    }

    PaletteClockTiming playableDecoyPalette(const PaletteWorkState& paletteState) {
        constexpr std::uint16_t indexPort=0x3c8;
        constexpr std::uint16_t dataPort=0x3c9;
        if(!phase_.playableCampaign || !phase_.devices || !phase_.devices->frame || !phase_.timer
            || phase_.frameStart!=AudioClockFrameStart::activeUnlockedVgaSurface
            || (phase_.interruptDelivery!=AudioClockInterruptDelivery::residentTimerIrq0
                && phase_.interruptDelivery!=AudioClockInterruptDelivery::residentTimerKeyboard)
            || phase_.devices->retraceStartMilliseconds>=phase_.devices->retraceEndMilliseconds
            || phase_.devices->retraceEndMilliseconds>=phase_.devices->framePeriodMilliseconds) {
            throw std::invalid_argument("decoy upload requires its explicit retained native clock");
        }
        transformPalette(paletteState,{0,1,0,vgaComponentMaximum});
        if(paletteState.dac.mode==PaletteDacMode::write && paletteState.dac.pelIndex==2
            && !paletteState.dac.latch) {
            throw std::invalid_argument("resident upload needs its retained DAC latch");
        }
        PaletteClockTiming upload{};
        upload.state=paletteState;
        auto& timing=upload.timing;
        auto& step=timing.total;
        auto latch=paletteState.dac.latch.value_or(std::array<std::uint8_t,vgaPaletteComponents>{});
        const auto write=[&](const std::uint16_t port,const std::uint8_t value) {
            dispatch(Operation::write,step);
            upload.writes.push_back({position()-origin_,port,value});
            auto& dac=upload.state.dac;
            if(port==indexPort) {
                dac.mode=PaletteDacMode::write;
                dac.writeIndex=value;
                dac.readIndex=static_cast<std::uint16_t>((value+vgaPaletteColors-1)%vgaPaletteColors);
                dac.pelIndex=0;
                return;
            }
            //516B writes raw bytes; the actual six-bit DAC masks the latch.
            latch[dac.pelIndex]=static_cast<std::uint8_t>(value&vgaComponentMaximum);
            if(++dac.pelIndex==vgaPaletteComponents) {
                std::copy(latch.begin(),latch.end(),dac.colors.begin()+dac.writeIndex*vgaPaletteComponents);
                upload.commits.push_back({position()-origin_,dac.writeIndex,latch});
                dac.pelIndex=0;
                dac.readIndex=dac.writeIndex;
                dac.writeIndex=static_cast<std::uint16_t>((dac.writeIndex+1)%vgaPaletteColors);
            }
        };
        plain(10,step); //516B..517E.
        upload.waitEntryCycles=position()-origin_;
        bool high;
        do {
            ++upload.highReads;
            dispatch(Operation::statusRead,step);
            const auto& devices=*phase_.devices;
            const auto inFrame=devices.picTicks+localPosition()/static_cast<double>(cyclesPerMillisecond)
                -devices.frameStartMilliseconds;
            high=inFrame>=devices.retraceStartMilliseconds && inFrame<=devices.retraceEndMilliseconds;
            plain(2,step);
        } while(!high);
        plain(1,step);
        dispatch(Operation::statusRead,step); //5189 IN3C7 uses generic ISA8 read.
        plain(3,step);
        if(paletteState.dac.mode==PaletteDacMode::write) {
            plain(1,step);
            write(dataPort,0);
        }
        plain(2,step);
        write(indexPort,0);
        plain(1,step);
        upload.uploadStartCycles=position()-origin_;
        foregroundFlags(NormalWorkKind::cli,step);
        for(const auto component:playableDecoyPaletteBytes()) {
            string(1,timing);
            write(dataPort,component);
            plain(1,step);
        }
        upload.uploadEndCycles=position()-origin_;
        foregroundFlags(NormalWorkKind::sti,step);
        plain(3,step); //POPSI shadow,POPBP,nearRET.
        upload.state.dac.latch=latch;
        finish(step,origin_);
        timing.phase=phase_;
        timing.callbacks=callbacks_;
        timing.vgaServices=vgaServices_;
        timing.keyboardIrqs=keyboardIrqs_;
        return upload;
    }

    SuccessPaletteClockTiming successPalette(const SuccessPaletteState& state,
        const PaletteWorkState& paletteState, const SuccessPaletteExecution execution,
        const SuccessPaletteCompletionSource completion) {
        constexpr std::uint8_t minimumValue = 16;
        constexpr std::uint16_t firstColor = 16;
        constexpr std::uint16_t indexPort = 0x3c8;
        constexpr std::uint16_t dataPort = 0x3c9;
        if ((execution != SuccessPaletteExecution::initial && execution != SuccessPaletteExecution::advance)
            || (completion != SuccessPaletteCompletionSource::soundHandle && completion != SuccessPaletteCompletionSource::timer)
            || (state.channel != SuccessPaletteChannel::green && state.channel != SuccessPaletteChannel::blue)
            || !phase_.devices || !phase_.devices->frame || !phase_.timer
            || phase_.frameStart != AudioClockFrameStart::activeUnlockedVgaSurface
            || (phase_.interruptDelivery != AudioClockInterruptDelivery::residentTimerIrq0
                && phase_.interruptDelivery != AudioClockInterruptDelivery::residentTimerKeyboard)
            || phase_.devices->retraceStartMilliseconds >= phase_.devices->retraceEndMilliseconds
            || phase_.devices->retraceEndMilliseconds >= phase_.devices->framePeriodMilliseconds
            || (completion == SuccessPaletteCompletionSource::soundHandle
                && (!phase_.callback || !phase_.callback->program
                    || phase_.callback->domain != SoundBlasterCallbackDomain::sequence))) {
            throw std::invalid_argument("unsupported retained success palette profile");
        }
        // Reuse the DAC owner's validation without changing either persistent bank.
        transformPalette(paletteState, {0,1,0,vgaComponentMaximum});
        SuccessPaletteClockTiming result{};
        result.state = state;
        result.palette.state = paletteState;
        auto& upload = result.palette;
        auto& timing = upload.timing;
        auto& step = timing.total;
        for (std::size_t index = 0; index < state.values.size(); ++index) {
            if (state.values[index] < minimumValue || state.values[index] > vgaComponentMaximum
                || (state.directions[index] != -1 && state.directions[index] != 1)
                || (execution == SuccessPaletteExecution::initial
                    && (state.values[index] != vgaComponentMaximum-index || state.directions[index] != 1))) {
                throw std::invalid_argument("unsupported retained success oscillator");
            }
        }
        const auto complete = [&]() {
            finish(step, origin_);
            timing.phase = phase_;
            timing.callbacks = callbacks_;
            timing.vgaServices = vgaServices_;
            timing.keyboardIrqs = keyboardIrqs_;
        };
        if (execution == SuccessPaletteExecution::initial) {
            plain(7,step); //705C..7069 arguments and CALL516B.
        } else {
            const bool sound = completion == SuccessPaletteCompletionSource::soundHandle;
            plain(2,step); //7078 CMPmusicEnabled and707D JZ.
            bool stopped;
            if (sound) {
                plain(1,step); //707F load descriptor.
                dispatch(Operation::plain,step); //7083 CMP live descriptor+16.
                stopped = phase_.callback->program->handleStatus == 0xffff;
                plain(1,step); //7087 JNZ.
            } else {
                stopped = true;
            }
            if (stopped) {
                plain(2,step); //7089 CMPmusicEnabled/708E JZ.
                if (sound) {
                    plain(1,step); //7090 JMP7204.
                    result.boundary = SuccessPaletteBoundary::presentationComplete;
                    complete();
                    return result;
                }
                dispatch(Operation::plain,step); //7093 CMP live global countdown.
                const auto remaining = phase_.timer->globalCount;
                plain(1,step); //7098 JNZ.
                if (remaining == 0) {
                    plain(1,step); //709A JMP7204.
                    result.boundary = SuccessPaletteBoundary::presentationComplete;
                    complete();
                    return result;
                }
            }
            plain(5,step); //709D..70AA; graphics enabled, pointer/direction setup.
            const bool blue = state.channel == SuccessPaletteChannel::blue;
            for (std::size_t index = 0; index < state.values.size(); ++index) {
                auto& value = result.state.values[index];
                auto& direction = result.state.directions[index];
                plain(3,step); //70AD INC/70AE OR/70B0 JZ.
                if (blue) { plain(1,step); } //70B2 INC.
                plain(2,step); //70B3 CMPmin/70B6 JA.
                if (value <= minimumValue) {
                    plain(2,step); //70B8 direction+1/70BC JMP.
                    direction = 1;
                } else {
                    plain(2,step); //70BE CMPmax/70C1 JC.
                    if (value >= vgaComponentMaximum) {
                        plain(1,step); //70C3 direction-1.
                        direction = -1;
                    }
                }
                plain(2,step); //70C7 direction load/70CA ADDbyte.
                value = static_cast<std::uint8_t>(value + direction);
                plain(3,step); //70CC INC/70CD OR/70CF JNZ.
                if (!blue) { plain(1,step); } //70D1 INC.
                plain(3,step); //70D2..70D6 loop.
            }
            plain(7,step); //70D8..70E5 arguments and CALL516B.
            result.boundary = SuccessPaletteBoundary::repeat;
        }
        if (paletteState.dac.mode == PaletteDacMode::write && paletteState.dac.pelIndex == 2
            && !paletteState.dac.latch) {
            throw std::invalid_argument("resident upload needs its retained DAC latch");
        }
        auto latch = paletteState.dac.latch.value_or(std::array<std::uint8_t,vgaPaletteComponents>{});
        const auto write = [&](const std::uint16_t port,const std::uint8_t value) {
            dispatch(Operation::write,step);
            upload.writes.push_back({position()-origin_,port,value});
            auto& dac = upload.state.dac;
            if (port == indexPort) {
                dac.mode = PaletteDacMode::write;
                dac.writeIndex = value;
                dac.readIndex = static_cast<std::uint16_t>((value+vgaPaletteColors-1)%vgaPaletteColors);
                dac.pelIndex = 0;
                return;
            }
            latch[dac.pelIndex] = value;
            if (++dac.pelIndex == vgaPaletteComponents) {
                std::copy(latch.begin(),latch.end(),dac.colors.begin()+dac.writeIndex*vgaPaletteComponents);
                upload.commits.push_back({position()-origin_,dac.writeIndex,latch});
                dac.pelIndex = 0;
                dac.readIndex = dac.writeIndex;
                dac.writeIndex = static_cast<std::uint16_t>((dac.writeIndex+1)%vgaPaletteColors);
            }
        };
        plain(10,step); //516B..517E setup.
        upload.waitEntryCycles = position()-origin_;
        bool high;
        do {
            ++upload.highReads;
            dispatch(Operation::statusRead,step); //5181 IN3DA.
            const auto& devices = *phase_.devices;
            const auto inFrame = devices.picTicks+localPosition()/static_cast<double>(cyclesPerMillisecond)
                -devices.frameStartMilliseconds;
            high = inFrame >= devices.retraceStartMilliseconds && inFrame <= devices.retraceEndMilliseconds;
            plain(2,step); //5182 AND/5184 JZ; high-only wait.
        } while (!high);
        plain(1,step); //5186 MOVDX3C7.
        dispatch(Operation::statusRead,step); //5189 generic ISA8 IN, no OPL recovery.
        plain(3,step); //518A/518C/518E.
        if (paletteState.dac.mode == PaletteDacMode::write) {
            plain(1,step); //5190 MOVDX3C9.
            write(dataPort,0); //5193 preliminary mode byte, may finish a retained triplet.
        }
        plain(2,step); //5194 MOVDX3C8/5197 MOVAXfirst.
        write(indexPort,firstColor); //5199.
        plain(1,step); //519A MOVDX3C9.
        upload.uploadStartCycles = position()-origin_;
        foregroundFlags(NormalWorkKind::cli,step); //519D.
        for (const auto value : result.state.values) {
            std::array<std::uint8_t,vgaPaletteComponents> color{};
            color[state.channel == SuccessPaletteChannel::blue ? 2 : 1] = value;
            for (const auto component : color) {
                string(1,timing); //519E LODSB from stack palette.
                write(dataPort,component); //519F.
                plain(1,step); //51A0 LOOP.
            }
        }
        upload.uploadEndCycles = position()-origin_;
        foregroundFlags(NormalWorkKind::sti,step); //51A2.
        plain(3,step); //51A3 shadowed POPSI/51A4 POPBP/51A5 nearRET.
        plain(execution == SuccessPaletteExecution::initial ? 1 : 2,step); //706C or70E8/70EB.
        upload.state.dac.latch = latch;
        complete();
        return result;
    }

    SuccessPaletteStepTiming successPaletteStep(const SuccessPaletteResidentCursor& cursor,
        const PaletteWorkState& paletteState, const std::uint64_t foregroundBudget) {
        const auto& state = cursor.state;
        const auto execution = cursor.execution;
        const auto completion = cursor.completion;
        constexpr std::uint8_t minimumValue = 16;
        constexpr std::uint16_t firstColor = 16;
        constexpr std::uint16_t indexPort = 0x3c8;
        constexpr std::uint16_t dataPort = 0x3c9;
        if ((cursor.stage != SuccessPaletteResidentStage::entry
                && cursor.stage != SuccessPaletteResidentStage::waitRetrace
                && cursor.stage != SuccessPaletteResidentStage::upload)
            || foregroundBudget == 0 || foregroundBudget > maximumWorkOperations
            || (execution != SuccessPaletteExecution::initial && execution != SuccessPaletteExecution::advance)
            || (completion != SuccessPaletteCompletionSource::soundHandle && completion != SuccessPaletteCompletionSource::timer)
            || (state.channel != SuccessPaletteChannel::green && state.channel != SuccessPaletteChannel::blue)
            || !phase_.devices || !phase_.devices->frame || !phase_.timer
            || phase_.frameStart != AudioClockFrameStart::activeUnlockedVgaSurface
            || (phase_.interruptDelivery != AudioClockInterruptDelivery::residentTimerIrq0
                && phase_.interruptDelivery != AudioClockInterruptDelivery::residentTimerKeyboard)
            || phase_.devices->retraceStartMilliseconds >= phase_.devices->retraceEndMilliseconds
            || phase_.devices->retraceEndMilliseconds >= phase_.devices->framePeriodMilliseconds
            || (completion == SuccessPaletteCompletionSource::soundHandle
                && (!phase_.callback || !phase_.callback->program
                    || phase_.callback->domain != SoundBlasterCallbackDomain::sequence))) {
            throw std::invalid_argument("unsupported retained success palette profile");
        }
        // Reuse the DAC owner's validation without changing either persistent bank.
        transformPalette(paletteState, {0,1,0,vgaComponentMaximum});
        SuccessPaletteStepTiming result{};
        result.cursor = cursor;
        result.palette.state = paletteState;
        auto& upload = result.palette;
        auto& timing = upload.timing;
        auto& step = timing.total;
        for (std::size_t index = 0; index < state.values.size(); ++index) {
            if (state.values[index] < minimumValue || state.values[index] > vgaComponentMaximum
                || (state.directions[index] != -1 && state.directions[index] != 1)
                || (execution == SuccessPaletteExecution::initial
                    && (state.values[index] != vgaComponentMaximum-index || state.directions[index] != 1))) {
                throw std::invalid_argument("unsupported retained success oscillator");
            }
        }
        const auto validateLatch = [&]() {
            if (paletteState.dac.mode == PaletteDacMode::write && paletteState.dac.pelIndex == 2
                && !paletteState.dac.latch) {
                throw std::invalid_argument("resident upload needs its retained DAC latch");
            }
        };
        const auto complete = [&]() {
            finish(step, origin_);
            timing.phase = phase_;
            timing.callbacks = callbacks_;
            timing.vgaServices = vgaServices_;
            timing.keyboardIrqs = keyboardIrqs_;
        };
        if (cursor.stage == SuccessPaletteResidentStage::entry) {
            if (execution == SuccessPaletteExecution::initial) {
                plain(7,step); //705C..7069 arguments and CALL516B.
            } else {
                const bool sound = completion == SuccessPaletteCompletionSource::soundHandle;
                plain(2,step); //7078 CMPmusicEnabled and707D JZ.
                bool stopped;
                if (sound) {
                    plain(1,step); //707F load descriptor.
                    dispatch(Operation::plain,step); //7083 CMP live descriptor+16.
                    stopped = phase_.callback->program->handleStatus == 0xffff;
                    plain(1,step); //7087 JNZ.
                } else {
                    stopped = true;
                }
                if (stopped) {
                    plain(2,step); //7089 CMPmusicEnabled/708E JZ.
                    if (sound) {
                        plain(1,step); //7090 JMP7204.
                        result.boundary = SuccessPaletteBoundary::presentationComplete;
                        result.cursor.stage = SuccessPaletteResidentStage::finished;
                        complete();
                        return result;
                    }
                    dispatch(Operation::plain,step); //7093 CMP live global countdown.
                    const auto remaining = phase_.timer->globalCount;
                    plain(1,step); //7098 JNZ.
                    if (remaining == 0) {
                        plain(1,step); //709A JMP7204.
                        result.boundary = SuccessPaletteBoundary::presentationComplete;
                        result.cursor.stage = SuccessPaletteResidentStage::finished;
                        complete();
                        return result;
                    }
                }
                plain(5,step); //709D..70AA; graphics enabled, pointer/direction setup.
                const bool blue = state.channel == SuccessPaletteChannel::blue;
                for (std::size_t index = 0; index < state.values.size(); ++index) {
                    auto& value = result.cursor.state.values[index];
                    auto& direction = result.cursor.state.directions[index];
                    plain(3,step); //70AD INC/70AE OR/70B0 JZ.
                    if (blue) { plain(1,step); } //70B2 INC.
                    plain(2,step); //70B3 CMPmin/70B6 JA.
                    if (value <= minimumValue) {
                        plain(2,step); //70B8 direction+1/70BC JMP.
                        direction = 1;
                    } else {
                        plain(2,step); //70BE CMPmax/70C1 JC.
                        if (value >= vgaComponentMaximum) {
                            plain(1,step); //70C3 direction-1.
                            direction = -1;
                        }
                    }
                    plain(2,step); //70C7 direction load/70CA ADDbyte.
                    value = static_cast<std::uint8_t>(value + direction);
                    plain(3,step); //70CC INC/70CD OR/70CF JNZ.
                    if (!blue) { plain(1,step); } //70D1 INC.
                    plain(3,step); //70D2..70D6 loop.
                }
                plain(7,step); //70D8..70E5 arguments and CALL516B.

            }
            validateLatch();
            plain(10,step); //516B..517E setup.
            upload.waitEntryCycles = position()-origin_;
            result.cursor.stage = SuccessPaletteResidentStage::waitRetrace;
            complete();
            return result; //BEFORE5181; computed stack palette and oscillator survive once.
        }
        validateLatch();
        if (cursor.stage == SuccessPaletteResidentStage::waitRetrace) {
            std::uint64_t foreground = 0;
            do {
                ++upload.highReads;
                dispatch(Operation::statusRead,step); //5181 IN3DA.
                const auto& devices = *phase_.devices;
                const auto inFrame = devices.picTicks+localPosition()/static_cast<double>(cyclesPerMillisecond)
                    -devices.frameStartMilliseconds;
                const auto high = inFrame >= devices.retraceStartMilliseconds && inFrame <= devices.retraceEndMilliseconds;
                plain(2,step); //Retain the IN result across IRQ service before AND/JZ.
                foreground += 3;
                if (high) {
                    result.cursor.stage = SuccessPaletteResidentStage::upload;
                    break; //BEFORE5186; never re-read status after observing high.
                }
            } while (foreground < foregroundBudget);
            complete();
            return result; //Whole5181/5182/5184 iteration; no extra source dispatch.
        }
        auto latch = paletteState.dac.latch.value_or(std::array<std::uint8_t,vgaPaletteComponents>{});
        const auto write = [&](const std::uint16_t port,const std::uint8_t value) {
            dispatch(Operation::write,step);
            upload.writes.push_back({position()-origin_,port,value});
            auto& dac = upload.state.dac;
            if (port == indexPort) {
                dac.mode = PaletteDacMode::write;
                dac.writeIndex = value;
                dac.readIndex = static_cast<std::uint16_t>((value+vgaPaletteColors-1)%vgaPaletteColors);
                dac.pelIndex = 0;
                return;
            }
            latch[dac.pelIndex] = value;
            if (++dac.pelIndex == vgaPaletteComponents) {
                std::copy(latch.begin(),latch.end(),dac.colors.begin()+dac.writeIndex*vgaPaletteComponents);
                upload.commits.push_back({position()-origin_,dac.writeIndex,latch});
                dac.pelIndex = 0;
                dac.readIndex = dac.writeIndex;
                dac.writeIndex = static_cast<std::uint16_t>((dac.writeIndex+1)%vgaPaletteColors);
            }
        };
        plain(1,step); //5186 MOVDX3C7.
        dispatch(Operation::statusRead,step); //5189 generic ISA8 IN, no OPL recovery.
        plain(3,step); //518A/518C/518E.
        if (paletteState.dac.mode == PaletteDacMode::write) {
            plain(1,step); //5190 MOVDX3C9.
            write(dataPort,0); //5193 preliminary mode byte, may finish a retained triplet.
        }
        plain(2,step); //5194 MOVDX3C8/5197 MOVAXfirst.
        write(indexPort,firstColor); //5199.
        plain(1,step); //519A MOVDX3C9.
        upload.uploadStartCycles = position()-origin_;
        foregroundFlags(NormalWorkKind::cli,step); //519D.
        for (const auto value : result.cursor.state.values) {
            std::array<std::uint8_t,vgaPaletteComponents> color{};
            color[state.channel == SuccessPaletteChannel::blue ? 2 : 1] = value;
            for (const auto component : color) {
                string(1,timing); //519E LODSB from stack palette.
                write(dataPort,component); //519F.
                plain(1,step); //51A0 LOOP.
            }
        }
        upload.uploadEndCycles = position()-origin_;
        foregroundFlags(NormalWorkKind::sti,step); //51A2.
        plain(3,step); //51A3 shadowed POPSI/51A4 POPBP/51A5 nearRET.
        plain(execution == SuccessPaletteExecution::initial ? 1 : 2,step); //706C or70E8/70EB.
        upload.state.dac.latch = latch;
        result.cursor.stage = SuccessPaletteResidentStage::finished;
        result.boundary = execution == SuccessPaletteExecution::initial
            ? SuccessPaletteBoundary::initialUploadComplete : SuccessPaletteBoundary::repeat;
        complete();
        return result;
    }

    SoundBlasterClockPhase retainedPhase() const {
        return phase_;
    }

    const std::vector<ScheduledSoundBlasterCallback>& scheduledCallbacks() const {
        return callbacks_;
    }

    const std::vector<VgaServiceReceipt>& vgaServices() const {
        return vgaServices_;
    }

    const std::vector<KeyboardIrqReceipt>& keyboardIrqs() const {
        return keyboardIrqs_;
    }

    void controllerEffect(const NormalWorkOperation& operation, SoundBlasterClockStep& step) {
        if (!keyboardAccess_ || operation.count != 1) {
            throw std::invalid_argument("controller effect requires one bound source instruction");
        }
        dispatch(Operation::plain, step);
        if (operation.kind == NormalWorkKind::keyboardConsumeHead) {
            keyboardAccess_->consumeHead();
        } else {
            keyboardAccess_->storeConsumedScan(operation.value);
        }
    }

    void queueUp(const KeyboardEdge edge) {
        if (!phase_.keyboard || !phase_.devices) {
            throw std::invalid_argument("keyboard enqueue requires owned transport/device phase");
        }
        const auto change = keyboardUpEdge(*phase_.keyboard, edge);
        applyKeyboard(change, localPosition() / static_cast<double>(cyclesPerMillisecond));
    }

    void keyboardPort(const NormalWorkOperation& operation, NormalWorkTiming& result) {
        if (!phase_.keyboard || operation.count != 1) {
            throw std::invalid_argument("keyboard port work requires one owned device instruction");
        }
        if (operation.kind == NormalWorkKind::keyboardControlWrite) {
            if (operation.value > std::numeric_limits<std::uint8_t>::max()) {
                throw std::invalid_argument("keyboard port write exceeds byte domain");
            }
            dispatch(Operation::write, result.total);
            applyKeyboard(keyboardWrite61(*phase_.keyboard, static_cast<std::uint8_t>(operation.value)),
                localPosition() / static_cast<double>(cyclesPerMillisecond));
            return;
        }
        if (operation.value != 0) {
            throw std::invalid_argument("keyboard read has no immediate operand");
        }
        dispatch(Operation::statusRead, result.total);
        const auto index = localPosition() / static_cast<double>(cyclesPerMillisecond);
        if (operation.kind == NormalWorkKind::keyboardDataRead) {
            const auto read = keyboardRead60(*phase_.keyboard);
            result.keyboardReads.push_back({KeyboardPort::data, position() - origin_, read.value});
            applyKeyboard(read.change, index);
            return;
        }
        result.keyboardReads.push_back({KeyboardPort::control, position() - origin_,
            keyboardRead61(*phase_.keyboard, phase_.devices->picTicks + index)});
    }

    void readLoop(const std::size_t count, SoundBlasterClockStep& step) {
        for (std::size_t index = 0; index < count; ++index) {
            dispatch(Operation::read, step);
            dispatch(Operation::plain, step);
        }
    }

    void enableInterrupts(SoundBlasterClockStep& step) {
        if (phase_.interruptDelivery != AudioClockInterruptDelivery::blockedByIrq0 || phase_.irq0ActivationPossible) {
            throw std::invalid_argument("unsupported callback interrupt delivery domain");
        }
        dispatch(Operation::plain, step);
        phase_.interrupts = AudioClockInterrupts::enabled;
        if (phase_.irqCheckRequested) {
            // Normal-core STI forces one following instruction before PIC service.
            phase_.deferredCycles += phase_.remainingCycles - 1;
            phase_.remainingCycles = 1;
        }
    }

    void restoreMaskedInterrupts(SoundBlasterClockStep& step) {
        dispatch(Operation::plain, step);
        phase_.interrupts = AudioClockInterrupts::masked;
    }

    void foregroundFlags(const NormalWorkKind kind, SoundBlasterClockStep& step) {
        if (phase_.interruptDelivery != AudioClockInterruptDelivery::residentTimerIrq0
            && phase_.interruptDelivery != AudioClockInterruptDelivery::residentTimerKeyboard
            && phase_.interruptDelivery != AudioClockInterruptDelivery::defaultBiosIrq0) {
            throw std::invalid_argument("unsupported foreground flags delivery domain");
        }
        dispatch(Operation::plain, step);
        phase_.interrupts = kind == NormalWorkKind::cli
            ? AudioClockInterrupts::masked : AudioClockInterrupts::enabled;
        if (!phase_.irqCheckRequested || kind == NormalWorkKind::cli) {
            return;
        }
        if (kind == NormalWorkKind::sti) {
            phase_.deferredCycles += phase_.remainingCycles - 1;
            phase_.remainingCycles = 1;
            return;
        }
        // POPF's CPU_PIC_CHECK exits directly, without a failed-loop charge.
        refillReady();
    }

    void finish(SoundBlasterClockStep& step, const std::int64_t start) {
        // Settle a spent slice, but do not charge the next boundary instruction.
        settle(step);
        snapshotStep(step, start);
    }

    void snapshotStep(SoundBlasterClockStep& step, const std::int64_t start) const {
        step.elapsedCycles = position() - start;
        step.remainingCycles = phase_.remainingCycles;
        step.deferredCycles = phase_.deferredCycles;
        step.linesDone = phase_.linesDone;
        step.millisecondRollovers = rollovers_;
        step.boundedEvents.clear();
        const auto horizon = localPosition() + horizonCycles_;
        for (const auto& event : phase_.events) {
            if (event.deadlineMilliseconds * cyclesPerMillisecond > horizon) {
                break;
            }
            if (event.kind == AudioClockEventKind::unsupported) {
                throw std::invalid_argument("unsupported bounded OPL clock event");
            }
            step.boundedEvents.push_back(event);
        }
    }

    std::int64_t position() const {
        return rollovers_ * cyclesPerMillisecond + localPosition();
    }

private:
    void foregroundOplBlocks(const std::vector<SoundBlasterWorkBlock>& blocks, NormalWorkTiming& result) {
        for (const auto& block : blocks) {
            if (block.kind == SoundBlasterWorkKind::plain) {
                plain(static_cast<std::size_t>(block.dispatches), result.total);
                continue;
            }
            if (block.kind != SoundBlasterWorkKind::registerWrite
                || block.dispatches != soundBlasterOplWork().dispatches) {
                throw std::invalid_argument("unsupported foreground device work");
            }
            oplAddress(*this, result.total);
            const auto effect = oplData(*this, result.total, origin_, OplDataBoundary::afterOut);
            plain(1, result.total); //26E5 writer RET, already included in its block.
            result.soundBlasterEffects.push_back({SoundBlasterForegroundKind::runtimeOplWrite,
                effect.elapsedCycles, static_cast<std::uint16_t>((block.registerNumber << 8) | block.value)});
        }
    }

    void validateKeyboard() const {
        const auto transfers = std::count_if(phase_.events.begin(), phase_.events.end(), [](const auto& event) {
            return event.kind == AudioClockEventKind::keyboardTransfer;
        });
        if (!phase_.keyboard) {
            if (transfers != 0 || phase_.instructionKeyboardEdge
                || (phase_.devices && (phase_.devices->irq1Requested || phase_.devices->irq1Input || phase_.devices->irq1InService))) {
                throw std::invalid_argument("keyboard event/PIC request has no retained transport owner");
            }
            return;
        }
        if (!phase_.devices || transfers != (phase_.keyboard->scheduled ? 1 : 0)
            || phase_.devices->irq1Input != phase_.keyboard->port60Pending
            || (!phase_.devices->irq1InService && phase_.devices->irq1Requested != phase_.devices->irq1Input)
            || (phase_.keyboard->used != 0 && !phase_.keyboard->scheduled && !phase_.keyboard->port60Pending)) {
            throw std::invalid_argument("inconsistent retained keyboard event/PIC phase");
        }
        for (const auto& event : phase_.events) {
            if (event.kind == AudioClockEventKind::keyboardTransfer && event.value != 0) {
                throw std::invalid_argument("unsupported keyboard transfer event value");
            }
        }
        keyboardRead61(*phase_.keyboard, phase_.devices->picTicks
            + localPosition() / static_cast<double>(cyclesPerMillisecond));
    }

    void addClockEvent(const AudioClockEvent event) {
        if (phase_.events.size() >= maximumQueuedEvents) {
            throw std::invalid_argument("keyboard event exceeds bounded PIC queue");
        }
        phase_.events.push_back(event);
        std::stable_sort(phase_.events.begin(), phase_.events.end(), [](const auto& left, const auto& right) {
            return left.deadlineMilliseconds < right.deadlineMilliseconds;
        });
        const auto until = phase_.events.front().deadlineMilliseconds * cyclesPerMillisecond - localPosition();
        if (static_cast<std::int64_t>(until) < phase_.remainingCycles) {
            phase_.deferredCycles += phase_.remainingCycles;
            phase_.remainingCycles = 0;
        }
    }

    void requestIrqCheck(const double base) {
        if (phase_.irqPending) {
            return;
        }
        phase_.irqPending = true;
        addClockEvent({base + phase_.irqDelayMilliseconds, AudioClockEventKind::irqCheck, 0});
    }

    void raiseTimerInput() {
        auto& install = *phase_.timerInstall;
        if (install.irq0InputHigh) {
            return;
        }
        install.irq0InputHigh = true;
        phase_.devices->irq0Requested = true;
        if ((install.masterPicMask & 1) == 0 && !phase_.devices->irq0InService) {
            requestIrqCheck(localPosition() / static_cast<double>(cyclesPerMillisecond));
            phase_.irq0ActivationPossible = false;
        }
    }

    void applyKeyboard(const KeyboardTransportChange& change, const double base) {
        phase_.keyboard = change.state;
        if (change.intent.scheduleTransfer) {
            addClockEvent({base + static_cast<double>(keyboardTransferDelayMilliseconds),
                AudioClockEventKind::keyboardTransfer, 0});
        }
        if (change.intent.raiseIrq1 && !phase_.devices->irq1Input) {
            phase_.devices->irq1Input = true;
            phase_.devices->irq1Requested = true;
            if (!phase_.devices->irq0InService && !phase_.devices->irq1InService) {
                requestIrqCheck(base);
            }
        }
        if (change.intent.lowerIrq1) {
            phase_.devices->irq1Input = false;
            phase_.devices->irq1Requested = false;
            if (phase_.devices->irq0Requested && !phase_.devices->irq0InService) {
                requestIrqCheck(base);
            }
        }
    }

    static bool positive(const double value) {
        return std::isfinite(value) && value > 0;
    }

    std::int64_t localPosition() const {
        return cyclesPerMillisecond - phase_.remainingCycles - phase_.deferredCycles;
    }

    void checkHorizon() const {
        if (position() - origin_ > horizonCycles_) {
            throw std::invalid_argument("OPL clock exceeds bounded horizon: elapsed="
                +std::to_string(position()-origin_)+", bound="+std::to_string(horizonCycles_)
                +", callbacks="+std::to_string(callbacks_.size()));
        }
    }

    void settle(SoundBlasterClockStep& step) {
        while (phase_.remainingCycles <= 0) {
            --phase_.remainingCycles;
            ++step.terminalCharges;
            refillReady();
            checkHorizon();
        }
        if (timerIrqPending_) {
            timerIrqPending_ = false;
            runTimerIrq(step);
            settle(step);
        }
        if (biosIrqPending_) {
            biosIrqPending_ = false;
            runBiosChain(step, nullptr);
            settle(step);
        }
        if (keyboardIrqPending_) {
            keyboardIrqPending_ = false;
            runKeyboardIrq(step);
            settle(step);
        }
        if (!keyboardAccess_ && phase_.keyboard && phase_.devices->irq1Requested
            && phase_.interrupts == AudioClockInterrupts::enabled && !phase_.devices->irq0InService) {
            throw std::invalid_argument("enabled keyboard IRQ1 requires recovered resident service");
        }
    }

    void recordVgaService(const VgaServiceKind kind, const AudioClockEvent& event) {
        if (!phase_.devices) {
            return;
        }
        const auto& devices = *phase_.devices;
        vgaServices_.push_back({kind, position() - origin_,
            devices.picTicks + event.deadlineMilliseconds,
            devices.picTicks + localPosition() / static_cast<double>(cyclesPerMillisecond),
            devices.frameStartMilliseconds, devices.realStart, phase_.linesDone, phase_.linesTotal});
    }

    void service(AudioClockEvent event) {
        switch (event.kind) {
        case AudioClockEventKind::keyboardTransfer:
            if (!phase_.keyboard || event.value != 0) {
                throw std::invalid_argument("keyboard transfer lacks admitted owner");
            }
            // Event-created PIC deadlines use srv_lag, not late service time.
            applyKeyboard(keyboardTransfer(*phase_.keyboard), event.deadlineMilliseconds);
            break;
        case AudioClockEventKind::scanline:
            if (phase_.linesDone >= phase_.linesTotal) {
                throw std::invalid_argument("unsupported inactive OPL clock scanline");
            }
            recordVgaService(VgaServiceKind::scanLine, event);
            ++phase_.linesDone;
            if (phase_.linesDone < phase_.linesTotal) {
                event.deadlineMilliseconds += phase_.singleLinePeriodMilliseconds;
                phase_.events.push_back(event);
            } else if (phase_.devices && phase_.devices->frame) {
                auto& frame = *phase_.devices->frame;
                if (frame.framesSinceTimeBase == maximumFrameCount) {
                    throw std::invalid_argument("VGA frame counter outside retained domain");
                }
                ++frame.framesSinceTimeBase;
            }
            break;
        case AudioClockEventKind::dmaDac:
            if (event.value != 0 || phase_.dmaDacRate == 0) {
                throw std::invalid_argument("unsupported OPL clock DMA event");
            }
            event.deadlineMilliseconds += 1000.0 / phase_.dmaDacRate;
            phase_.events.push_back(event);
            break;
        case AudioClockEventKind::pit:
            if (!phase_.devices && phase_.pitPeriodMilliseconds * 0.95 * cyclesPerMillisecond <= horizonCycles_) {
                throw std::invalid_argument("PIT recurrence could reenter bounded writer");
            }
            if (phase_.devices) {
                phase_.devices->irq0Requested = true;
                if (phase_.timerInstall) {
                    // PIT0_Event lowers then raises the edge-triggered input.
                    phase_.timerInstall->irq0InputHigh = true;
                }
            }
            if (phase_.irq0ActivationPossible && !phase_.irqPending) {
                phase_.irqPending = true;
                phase_.events.push_back({event.deadlineMilliseconds + phase_.irqDelayMilliseconds,
                                         AudioClockEventKind::irqCheck, 0});
            }
            phase_.irq0ActivationPossible = false;
            if (phase_.devices) {
                auto& devices = *phase_.devices;
                devices.pitNowMilliseconds = devices.picTicks + localPosition() / static_cast<double>(cyclesPerMillisecond);
                const auto half = phase_.pitPeriodMilliseconds / 2;
                if (devices.pitNowMilliseconds >= devices.pitStartMilliseconds + half) {
                    devices.pitCycleBase = (devices.pitCycleBase + 1) & 1;
                    devices.pitStartMilliseconds += half;
                    if (devices.pitNowMilliseconds >= devices.pitStartMilliseconds + half) {
                        const auto count = static_cast<unsigned>(std::floor(
                            (devices.pitNowMilliseconds - devices.pitStartMilliseconds) / half));
                        devices.pitCycleBase = (devices.pitCycleBase + count) & 1;
                        devices.pitStartMilliseconds += count * half;
                    }
                }
                devices.pitNowMilliseconds = std::max(devices.pitNowMilliseconds, devices.pitStartMilliseconds);
                const auto error = devices.picTicks + event.deadlineMilliseconds - devices.pitStartMilliseconds;
                auto delay = phase_.pitPeriodMilliseconds - error * 0.05;
                if (delay <= 0) {
                    delay = phase_.pitPeriodMilliseconds * 0.95;
                }
                event.deadlineMilliseconds += delay;
                phase_.events.push_back(event);
            }
            break;
        case AudioClockEventKind::irqCheck:
            phase_.irqPending = false;
            phase_.irqCheckRequested = true;
            break;
        case AudioClockEventKind::pendingVgaInterrupt:
            if (phase_.vgaInterrupt != AudioClockVgaInterrupt::fixedVgaNoDemand
                || event.value != 0) {
                throw std::invalid_argument("unsupported VGA vertical-interrupt clock domain");
            }
            // Non-demand MCH_VGA only latches vret_triggered: no IRQ or CPU/PIC/scanline change.
            // The VGA latch itself is outside this timing ledger.
            if (phase_.devices) {
                auto& devices = *phase_.devices;
                if ((devices.verticalRetraceEndRegister & 0x30) == 0x10) {
                    devices.verticalRetraceTriggered = true;
                }
            }
            break;
        case AudioClockEventKind::displayStartLatch:
        case AudioClockEventKind::panningLatch:
            if (!phase_.devices || event.value != 0) {
                throw std::invalid_argument("unsupported retained VGA latch domain");
            }
            if (event.kind == AudioClockEventKind::displayStartLatch) {
                phase_.devices->realStart = phase_.devices->displayStart & phase_.devices->memoryMask;
                phase_.devices->drawnBytesSkip = phase_.devices->configuredBytesSkip;
            } else {
                phase_.devices->drawnPanning = phase_.devices->pelPanning;
            }
            break;
        case AudioClockEventKind::frameStart: {
            if (phase_.frameStart != AudioClockFrameStart::activeUnlockedVgaSurface || phase_.linesDone != phase_.linesTotal) {
                throw std::invalid_argument("unsupported VGA frame-start render domain");
            }
            if (!phase_.devices && phase_.minimumLatchDelayMilliseconds * cyclesPerMillisecond <= horizonCycles_) {
                throw std::invalid_argument("VGA latch/interrupt could reenter bounded writer");
            }
            const auto skip = static_cast<float>(phase_.horizontalPeriodMilliseconds * phase_.verticalBlankSkip);
            const auto delay = static_cast<float>(phase_.horizontalPeriodMilliseconds / 4.0 + skip);
            if (!std::isfinite(delay)) {
                throw std::invalid_argument("invalid VGA first-line delay");
            }
            phase_.linesDone = 0;
            phase_.events.push_back({event.deadlineMilliseconds + delay, AudioClockEventKind::scanline, 0});
            if (phase_.devices) {
                auto& devices = *phase_.devices;
                devices.frameStartMilliseconds = devices.picTicks + event.deadlineMilliseconds;
                auto recurrence = static_cast<double>(static_cast<float>(devices.framePeriodMilliseconds));
                if (devices.frame) {
                    const auto& frame = *devices.frame;
                    const auto expected = frame.framesSinceTimeBase * 1000.0 / frame.framesPerSecond + frame.timeBaseMilliseconds;
                    const auto adjustment = std::clamp((expected - devices.frameStartMilliseconds) * frameDriftFraction,
                        -maximumFrameDriftMilliseconds, maximumFrameDriftMilliseconds);
                    recurrence = std::max(1.0, recurrence + adjustment);
                }
                phase_.events.push_back({event.deadlineMilliseconds + recurrence,
                    AudioClockEventKind::frameStart, 0});
                phase_.events.push_back({event.deadlineMilliseconds + static_cast<float>(devices.retraceStartMilliseconds),
                    AudioClockEventKind::displayStartLatch, 0});
                phase_.events.push_back({event.deadlineMilliseconds + static_cast<float>(devices.retraceEndMilliseconds),
                    AudioClockEventKind::panningLatch, 0});
                phase_.events.push_back({event.deadlineMilliseconds + static_cast<float>(devices.displayEndMilliseconds + 0.005),
                    AudioClockEventKind::pendingVgaInterrupt, 0});
            }
            recordVgaService(VgaServiceKind::scanStart, event);
            break;
        }
        case AudioClockEventKind::pendingCmos: {
            if (!phase_.rtc || event.value != 0) {
                throw std::invalid_argument("CMOS service outside supported clock family");
            }
            auto rtc = *phase_.rtc;
            const double index = phase_.devices->picTicks
                + localPosition() / static_cast<double>(cyclesPerMillisecond);
            // Periodic remainder is dead in the admitted interrupt-disabled domain.
            if (index >= rtc.endedMilliseconds + rtcSecondMilliseconds - rtcTickToleranceMilliseconds) {
                if (rtc.endedMilliseconds < index - rtcSecondMilliseconds) {
                    rtc.endedMilliseconds = index - rtcSecondMilliseconds;
                }
                rtc.endedMilliseconds -= std::fmod(rtc.endedMilliseconds, rtcSecondMilliseconds);
                rtc.endedMilliseconds += rtcSecondMilliseconds;
                if (!rtc.locked) {
                    tickRtc(rtc);
                }
            }
            const auto delay = static_cast<float>(std::max(rtcMinimumDelayMilliseconds,
                rtc.endedMilliseconds + rtcSecondMilliseconds - index));
            event.deadlineMilliseconds += static_cast<double>(delay);
            if (!std::isfinite(index) || !std::isfinite(rtc.endedMilliseconds)
                || !std::isfinite(delay) || !std::isfinite(event.deadlineMilliseconds)) {
                throw std::invalid_argument("RTC recurrence outside finite source domain");
            }
            phase_.events.push_back(event);
            phase_.rtc = rtc;
            break;
        }
        default:
            throw std::invalid_argument("unmodeled OPL clock event service");
        }
    }

    void refill() {
        phase_.deferredCycles += phase_.remainingCycles;
        phase_.remainingCycles = 0;
        if (phase_.deferredCycles <= 0) {
            phase_.deferredCycles += cyclesPerMillisecond;
            ++rollovers_;
            if (phase_.devices) {
                ++phase_.devices->picTicks;
            }
            for (auto& event : phase_.events) {
                event.deadlineMilliseconds -= 1.0;
            }
            if (phase_.keyboard) {
                applyKeyboard(keyboardTick(*phase_.keyboard),
                    localPosition() / static_cast<double>(cyclesPerMillisecond));
            }
        }
        const auto current = localPosition();
        serviceInterruptCheck();
        std::size_t services = 0;
        while (!phase_.events.empty() && phase_.events.front().deadlineMilliseconds * cyclesPerMillisecond <= current) {
            if (++services > maximumServicesPerSlice) {
                throw std::invalid_argument("OPL clock event service exceeds bounded domain");
            }
            const auto event = phase_.events.front();
            phase_.events.erase(phase_.events.begin());
            service(event);
            std::stable_sort(phase_.events.begin(), phase_.events.end(), [](const auto& left, const auto& right) {
                return left.deadlineMilliseconds < right.deadlineMilliseconds;
            });
        }
        auto budget = phase_.deferredCycles;
        if (!phase_.events.empty()) {
            const auto until = phase_.events.front().deadlineMilliseconds * cyclesPerMillisecond - current;
            if (until < budget) {
                const auto cycles = static_cast<std::int64_t>(until);
                budget = std::min(budget, cycles == 0 ? 1 : cycles);
            }
        }
        if (budget <= 0) {
            throw std::invalid_argument("invalid queued-event OPL clock budget");
        }
        phase_.remainingCycles = budget;
        phase_.deferredCycles -= budget;
        serviceInterruptCheck();
    }

    void refillReady() {
        std::size_t slices = 0;
        do {
            if (++slices > maximumServicesPerSlice) {
                throw std::invalid_argument("retained clock refill exceeds bounded domain");
            }
            refill();
            // A new PIC event can end the assigned slice before normal-core entry.
            // The core's initial zero-budget guard returns without a loop charge.
        } while (phase_.remainingCycles <= 0);
    }

    void serviceInterruptCheck() {
        if (phase_.interrupts != AudioClockInterrupts::enabled || !phase_.irqCheckRequested) {
            return;
        }
        if (!keyboardAccess_ && phase_.devices && phase_.devices->irq1Requested && !phase_.devices->irq0InService
            && !phase_.devices->irq0Requested) {
            throw std::invalid_argument("native resident keyboard IRQ1 service remains unsupported");
        }
        const bool defaultBios = phase_.interruptDelivery == AudioClockInterruptDelivery::defaultBiosIrq0;
        if (defaultBios || phase_.interruptDelivery == AudioClockInterruptDelivery::residentTimerIrq0
            || phase_.interruptDelivery == AudioClockInterruptDelivery::residentTimerKeyboard) {
            phase_.irqCheckRequested = false;
            if (!phase_.devices->irq0Requested) {
                if (phase_.devices->irq1Requested && !phase_.devices->irq1InService) {
                    if (!keyboardAccess_) {
                        throw std::invalid_argument("keyboard IRQ1 lacks resident controller binding");
                    }
                    phase_.devices->irq1Requested = false;
                    phase_.devices->irq1InService = true;
                    phase_.interrupts = AudioClockInterrupts::masked;
                    phase_.interruptDelivery = AudioClockInterruptDelivery::blockedByIrq1;
                    keyboardIrqPending_ = true;
                    requestIrqCheck(localPosition() / static_cast<double>(cyclesPerMillisecond));
                }
                return;
            }
            if (!defaultBios && ((phase_.timer->biosCount == 1 && (!phase_.bios || phase_.timer->biosReload == 0))
                || ((phase_.timer->activeMask & soundBlasterCallbackMask) != 0
                    && phase_.timer->callbackCount == 1 && !phase_.callback))) {
                throw std::invalid_argument("unsupported timer callback or BIOS-chain IRQ");
            }
            if (defaultBios || phase_.timer->biosCount == 1) {
                validateBiosHost(phase_);
            }
            phase_.devices->irq0Requested = false;
            phase_.devices->irq0InService = true;
            phase_.irq0ActivationPossible = false;
            phase_.interrupts = AudioClockInterrupts::masked;
            phase_.interruptDelivery = AudioClockInterruptDelivery::blockedByIrq0;
            timerIrqPending_ = !defaultBios;
            biosIrqPending_ = defaultBios;
            if (!phase_.irqPending) {
                phase_.irqPending = true;
                phase_.events.push_back({localPosition() / static_cast<double>(cyclesPerMillisecond)
                    + phase_.irqDelayMilliseconds, AudioClockEventKind::irqCheck, 0});
                std::stable_sort(phase_.events.begin(), phase_.events.end(), [](const auto& left, const auto& right) {
                    return left.deadlineMilliseconds < right.deadlineMilliseconds;
                });
                // PIC AddEntry ends the assigned CPU slice when the new event is earlier.
                const auto until = phase_.events.front().deadlineMilliseconds * cyclesPerMillisecond - localPosition();
                if (static_cast<std::int64_t>(until) < phase_.remainingCycles) {
                    phase_.deferredCycles += phase_.remainingCycles;
                    phase_.remainingCycles = 0;
                }
            }
            return;
        }
        if (phase_.interruptDelivery != AudioClockInterruptDelivery::blockedByIrq0) {
            throw std::invalid_argument("unsupported callback interrupt delivery domain");
        }
        phase_.irqCheckRequested = false;
    }

    void runKeyboardIrq(SoundBlasterClockStep& step) {
        if (!keyboardAccess_ || !phase_.devices->irq1InService
            || phase_.interrupts != AudioClockInterrupts::masked) {
            throw std::invalid_argument("resident IRQ1 requires masked bound keyboard owner");
        }
        finish(step, origin_);
        auto entry = step;
        const auto entryPhase = phase_;
        plain(12, step); //010b..011c.
        dispatch(Operation::statusRead, step); //011e IN60; handler effect follows ISA delay.
        const auto read = keyboardRead60(*phase_.keyboard);
        const auto scan = read.value;
        applyKeyboard(read.change, localPosition() / static_cast<double>(cyclesPerMillisecond));
        keyboardAccess_->validateScan(scan);
        const auto ringPath = scan == 0x48 ? keyboardAccess_->makeRingPath() : KeyboardMakeRingPath::emptyLinear;
        plain(2, step); //0120,0123.
        dispatch(Operation::statusRead, step); //0125 IN61.
        const auto control = keyboardRead61(*phase_.keyboard, phase_.devices->picTicks
            + localPosition() / static_cast<double>(cyclesPerMillisecond));
        plain(2, step); //0126 MOV AH;0128 OR80.
        dispatch(Operation::write, step); //012a pulse.
        applyKeyboard(keyboardWrite61(*phase_.keyboard, static_cast<std::uint8_t>(control | 0x80)),
            localPosition() / static_cast<double>(cyclesPerMillisecond));
        plain(1, step); //012b MOV AL,AH.
        dispatch(Operation::write, step); //012d restore.
        applyKeyboard(keyboardWrite61(*phase_.keyboard, control),
            localPosition() / static_cast<double>(cyclesPerMillisecond));
        if (scan == 0xe0) {
            plain(13, step); //012e..0174 extended-prefix exit.
        } else {
            const bool make = scan == 0x48;
            plain(17, step); //012e..0181, unmodified scan48.
            const auto metadata = static_cast<std::uint8_t>((keyboardAccess_->metadata()
                & (make ? 0xfe : 0xff)) ^ 1);
            dispatch(Operation::plain, step); //0184 metadata store.
            keyboardAccess_->storeMetadata(metadata);
            plain(make ? 15 : 18, step); //0188..0201 or01cb role branch.
            dispatch(Operation::plain, step);
            keyboardAccess_->storeRole(make ? 7 : 0);
            if (make) {
                plain(13, step); //0205..0267, BIOS modifiers zero.
            }
            dispatch(Operation::plain, step); //0269 or01d0 last key.
            keyboardAccess_->storeLastKey(make ? 0x4800 : 0);
            if (make) {
                plain(ringPath == KeyboardMakeRingPath::emptyTailWrap ? 6 : 9, step); //026c..0287 source full predicate.
                dispatch(Operation::plain, step); //0289 BIOS slot.
                keyboardAccess_->storeQueuedWord(0x4800);
                plain(ringPath == KeyboardMakeRingPath::emptyTailWrap ? 4 : 3, step); //028c..0294;0291 reloads ring start at wrap.
                dispatch(Operation::plain, step); //0297 BIOS tail.
                keyboardAccess_->advanceTail();
                plain(7, step); //029c..02f6, no special action.
            } else {
                plain(5, step); //01d6..01e8, nonmodifier release.
            }
        }
        plain(1, step); //0334 MOV AL,20.
        dispatch(Operation::write, step); //0336 EOI.
        if (phase_.devices->irq0InService || phase_.devices->irq1Requested) {
            throw std::invalid_argument("unsupported IRQ1 EOI priority or reactivation");
        }
        phase_.devices->irq1InService = false;
        if (phase_.devices->irq0Requested) {
            requestIrqCheck(localPosition() / static_cast<double>(cyclesPerMillisecond));
        }
        plain(7, step); //0338..033e register restore.
        finish(step, origin_);
        auto preIret = step;
        const auto preIretPhase = phase_;
        outerIret(step); //033f restores selected common delivery; one dispatch.
        auto completed = step;
        snapshotStep(completed, origin_);
        keyboardIrqs_.push_back({scan, std::move(entry), entryPhase, std::move(preIret), preIretPhase,
            std::move(completed), phase_, control});
    }

    void runTimerIrq(SoundBlasterClockStep& step) {
        auto& timer = *phase_.timer;
        const auto decremented = static_cast<std::uint16_t>(timer.globalCount - 1);
        timer.globalCount = decremented ^ ((decremented & 0x8000) ? 0xffff : 0);
        const bool biosExpired = timer.biosCount == 1;
        --timer.biosCount;
        const bool callbackActive = (timer.activeMask & soundBlasterCallbackMask) != 0;
        const bool expired = callbackActive && timer.callbackCount == 1;
        if (callbackActive) {
            --timer.callbackCount;
        }
        if (expired) {
            const bool retainedLevel1 = retainedLevel1Program(phase_);
            const bool retainedLevel2 = retainedLevel2Program(phase_)
                && phase_.sequenceSelection->prepared.sequence == phase_.callback->sequence;
            auto advance = (phase_.playableCampaign || playableStartupProgram(phase_))
                ? soundBlasterPlayableCallbackAdvance(*phase_.callback)
                : retainedMenuProgram(phase_) ? soundBlasterMenuCallbackAdvance(*phase_.callback)
                : retainedLevel1 ? soundBlasterLevel1CallbackAdvance(*phase_.callback)
                : retainedLevel2 ? soundBlasterLevel2CallbackAdvance(*phase_.callback)
                : soundBlasterCallbackAdvance(*phase_.callback);
            if (phase_.callback->program && phase_.callback->fade != 0 && advance.state.program
                && advance.state.program->handleStatus == 0xffff && advance.state.program->repeatCount == 0) {
                //The kernel is qualified, but its inner2D94 STI window is not.
                throw std::invalid_argument("terminal program-fade interrupt window is not admitted");
            }
            plain(timerIrqCallbackEntryDispatches, step);
            finish(step, origin_);
            const auto callbackStart = position();
            SoundBlasterCallbackClock callback{};
            callback.total = step;
            executeCallbackWork(*this, advance.blocks, advance.kind, callback, origin_);
            const auto callbackHorizon = callbackWorkHorizon(advance.blocks, advance.kind);
            if (position() - callbackStart > callbackHorizon) {
                throw std::invalid_argument("timer callback exceeds recovered bounded horizon");
            }
            step = callback.total;
            phase_.callback = advance.state;
            callbacks_.push_back({callbackStart - origin_, std::move(advance), std::move(callback)});
            timer.callbackCount = timer.callbackReload;
            plain(biosExpired ? timerIrqBiosCallbackExitDispatches : timerIrqCallbackExitDispatches, step);
        } else if (callbackActive) {
            plain(biosExpired ? timerIrqBiosPrefixDispatches : timerIrqPrefixDispatches, step);
        } else {
            plain(biosExpired ? emptyTimerIrqBiosPrefixDispatches : emptyTimerIrqPrefixDispatches, step);
        }
        if (biosExpired) {
            timer.biosCount = timer.biosReload;
            runBiosChain(step, nullptr);
            return;
        }
        acknowledgeIrq0(step);
        plain(1, step);
        outerIret(step);
    }

    void acknowledgeIrq0(SoundBlasterClockStep& step) {
        dispatch(Operation::write, step);
        if (phase_.devices->irq0Requested && !phase_.timerInstall) {
            throw std::invalid_argument("unsupported PIT reactivation inside timer IRQ");
        }
        phase_.devices->irq0InService = false;
        phase_.irq0ActivationPossible = true;
        if (phase_.devices->irq0Requested || (keyboardAccess_ && phase_.devices->irq1Requested)) {
            requestIrqCheck(localPosition() / static_cast<double>(cyclesPerMillisecond));
        }
    }

    void outerIret(SoundBlasterClockStep& step) {
        dispatch(Operation::plain, step);
        phase_.interrupts = AudioClockInterrupts::enabled;
        if (keyboardAccess_) {
            phase_.interruptDelivery = AudioClockInterruptDelivery::residentTimerKeyboard;
        } else if (phase_.timerInstall && phase_.timerInstall->stage < AudioClockTimerInstallStage::vectorInstalled) {
            phase_.interruptDelivery = AudioClockInterruptDelivery::defaultBiosIrq0;
        } else {
            phase_.interruptDelivery = AudioClockInterruptDelivery::residentTimerIrq0;
        }
        if (phase_.irqCheckRequested) {
            // CPU_PIC_CHECK returns directly; no failed normal-loop decrement.
            refillReady();
        }
    }

    void runBiosChain(SoundBlasterClockStep& step, std::vector<BiosClockCheckpoint>* checkpoints) {
        validateBiosHost(phase_);
        if (!phase_.devices || !phase_.devices->irq0InService || phase_.irq0ActivationPossible
            || phase_.interrupts != AudioClockInterrupts::masked
            || phase_.interruptDelivery != AudioClockInterruptDelivery::blockedByIrq0) {
            throw std::invalid_argument("unsupported BIOS chain entry phase");
        }
        const auto checkpoint = [&](const BiosClockBoundary boundary) {
            if (checkpoints) {
                settle(step);
                auto timing = step;
                finish(timing, origin_);
                checkpoints->push_back({boundary, std::move(timing), phase_});
            }
        };
        enableInterrupts(step); // STI preserves its one-instruction shadow.
        checkpoint(BiosClockBoundary::preInt8);
        dispatch(Operation::plain, step); // FE38 exits the core for INT8.
        ++phase_.bios->timerTicks;
        refillReady(); // Callback return bypasses the failed-loop charge.
        checkpoint(BiosClockBoundary::postInt8);
        plain(3, step); // PUSH DS,AX,DX.
        checkpoint(BiosClockBoundary::preInt1c);
        dispatch(Operation::plain, step); // INT1C pushes one software frame.
        phase_.interrupts = AudioClockInterrupts::masked;
        checkpoint(BiosClockBoundary::int1cEntry);
        dispatch(Operation::plain, step); // Default INT1C FE38 callback.
        refillReady();
        dispatch(Operation::plain, step); // Software IRET restores BIOS IF.
        phase_.interrupts = AudioClockInterrupts::enabled;
        if (phase_.irqCheckRequested) {
            refillReady();
        }
        checkpoint(BiosClockBoundary::postInt1c);
        restoreMaskedInterrupts(step); // CLI.
        plain(1, step); // MOV AL,20h.
        checkpoint(BiosClockBoundary::preEoi);
        acknowledgeIrq0(step);
        plain(3, step); // POP DX,AX,DS.
        checkpoint(BiosClockBoundary::preIret);
        outerIret(step);
    }

    SoundBlasterClockPhase phase_;
    std::int64_t horizonCycles_;
    std::int64_t origin_ = 0;
    std::uint32_t rollovers_ = 0;
    bool timerIrqPending_ = false;
    bool biosIrqPending_ = false;
    bool keyboardIrqPending_ = false;
    KeyboardClockAccess* keyboardAccess_ = nullptr;
    std::vector<ScheduledSoundBlasterCallback> callbacks_;
    std::vector<VgaServiceReceipt> vgaServices_;
    std::vector<KeyboardIrqReceipt> keyboardIrqs_;
};

void oplAddress(OplWorkClock& clock, SoundBlasterClockStep& step) {
    constexpr std::size_t addressReads = 6;
    clock.plain(4, step);
    clock.dispatch(Operation::write, step);
    clock.plain(2, step);
    clock.readLoop(addressReads, step);
    clock.plain(3, step);
}

SoundBlasterClockStep oplData(OplWorkClock& clock, SoundBlasterClockStep& step, const std::int64_t start,
    const OplDataBoundary boundary) {
    constexpr std::size_t dataReads = 37;
    clock.dispatch(Operation::write, step);
    if (boundary == OplDataBoundary::afterOut) {
        clock.snapshotStep(step, start);
    } else {
        clock.finish(step, start);
    }
    const auto effect = step;
    clock.plain(3, step);
    clock.readLoop(dataReads, step);
    clock.plain(3, step);
    return effect;
}

void executeCallbackWork(OplWorkClock& clock, const std::vector<SoundBlasterWorkBlock>& blocks,
    const SoundBlasterCallbackKind kind, SoundBlasterCallbackClock& result, const std::int64_t start) {
    if (blocks.empty() || blocks.size() > (programCallback(kind)
        ? maximumProgramCallbackBlocks : maximumCallbackBlocks)) {
        throw std::invalid_argument("invalid bounded timer callback work blocks");
    }
    const auto horizon = callbackWorkHorizon(blocks, kind);
    auto& total = result.total;
    const auto writerDispatches = soundBlasterOplWork().dispatches;
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        const auto& block = blocks[index];
        if (block.dispatches == 0 || block.dispatches > horizon) {
            throw std::invalid_argument("invalid callback work dispatch count");
        }
        if (block.kind == SoundBlasterWorkKind::plain) {
            if (block.voice != -1) {
                throw std::invalid_argument("plain callback work must not select a voice");
            }
            if (callbackEnableTail(kind) && index + 1 == blocks.size()) {
                if (block.dispatches < keyOffEnableTailDispatches) {
                    throw std::invalid_argument("missing recovered callback interrupt tail");
                }
                clock.plain(static_cast<std::size_t>(block.dispatches) - keyOffEnableTailDispatches, total);
                clock.finish(total, start);
                result.interruptBoundaries.push_back(total);
                clock.enableInterrupts(total);
                clock.plain(keyOffEnableWindowDispatches, total);
                clock.finish(total, start);
                result.interruptBoundaries.push_back(total);
                clock.restoreMaskedInterrupts(total);
                clock.plain(keyOffRestoreTailDispatches, total);
            } else {
                clock.plain(static_cast<std::size_t>(block.dispatches), total);
            }
            continue;
        }
        if (kind == SoundBlasterCallbackKind::wait || kind == SoundBlasterCallbackKind::noHandle
            || (block.kind != SoundBlasterWorkKind::frequencyLow && block.kind != SoundBlasterWorkKind::frequencyHigh
             && block.kind != SoundBlasterWorkKind::carrierLevel && block.kind != SoundBlasterWorkKind::registerWrite)
            || block.dispatches != writerDispatches
            || (block.kind != SoundBlasterWorkKind::registerWrite && (block.voice < 0 || block.voice >= soundBlasterVoiceCount))
            || (block.kind == SoundBlasterWorkKind::registerWrite && !programCallback(kind))) {
            throw std::invalid_argument("invalid recovered callback OPL block");
        }
        std::array<SoundBlasterClockStep, 3> writer{};
        clock.finish(total, start);
        writer[0] = total;
        oplAddress(clock, total);
        clock.finish(total, start);
        writer[1] = total;
        result.oplEffects.push_back(oplData(clock, total, start,
            block.kind == SoundBlasterWorkKind::registerWrite ? OplDataBoundary::afterOut : OplDataBoundary::settled));
        clock.finish(total, start);
        writer[2] = total;
        result.opl.push_back(std::move(writer));
        clock.plain(1, total); // Writer RET precedes the caller's next work.
    }
    if (callbackEnableTail(kind) && result.interruptBoundaries.size() != 2) {
        throw std::invalid_argument("missing recovered callback interrupt tail");
    }
    clock.finish(total, start);
}

}  // namespace

BiosClockTiming biosChainTiming(const SoundBlasterClockPhase& phase) {
    OplWorkClock clock(phase, biosHorizonCycles);
    return clock.biosChain();
}

PageFlipClockTiming pageFlipClockTiming(const SoundBlasterClockPhase& phase,
    const PageFlipClockState& state, const std::int64_t horizonCycles) {
    return pageFlipClockTimingBound(phase, state, horizonCycles, nullptr);
}

PageFlipClockTiming pageFlipClockTimingBound(const SoundBlasterClockPhase& phase,
    const PageFlipClockState& state, const std::int64_t horizonCycles, KeyboardClockAccess* access) {
    const auto retainedProgram = state.waitDomain == PageFlipWaitDomain::retainedProgram;
    if ((state.waitDomain != PageFlipWaitDomain::bounded && !retainedProgram)
        || (retainedProgram && (state.layout != PageFlipLayout::recoveredStartup
            || state.synchronization != PageFlipSynchronization::enabled || !phase.callback || !phase.callback->program))) {
        throw std::invalid_argument("unsupported page-flip wait domain");
    }
    const auto maximumHorizon = retainedProgram ? maximumProgramPageFlipHorizonCycles : maximumNormalWorkHorizonCycles;
    if (horizonCycles <= 0 || horizonCycles > maximumHorizon) {
        throw std::invalid_argument("invalid bounded page-flip horizon");
    }
    OplWorkClock clock(phase, horizonCycles, EntryPolicy::retained, access);
    return clock.pageFlip(state);
}

PaletteClockTiming paletteClockTiming(const SoundBlasterClockPhase& phase,
    const PaletteWorkState& state, const PaletteRequest& request, const std::int64_t horizonCycles) {
    return paletteClockTimingBound(phase, state, request, horizonCycles, nullptr);
}

PaletteClockTiming paletteClockTimingBound(const SoundBlasterClockPhase& phase,
    const PaletteWorkState& state, const PaletteRequest& request, const std::int64_t horizonCycles, KeyboardClockAccess* access) {
    const auto fadingProgram = phase.callback && phase.callback->program && phase.callback->fade != 0;
    const auto retainedLevel2 = phase.sequenceSelection && phase.sequenceSelection->published
        && phase.sequenceSelection->songOrdinal==level2MusicIdentity
        && phase.sequenceSelection->level1DirectResetCompleted
        && phase.callback && phase.callback->domain==SoundBlasterCallbackDomain::sequence
        && phase.callback->program && phase.successAudio && phase.successAudio->stage==SuccessAudioStage::returned;
    const auto playableProgram = phase.playableCampaign && phase.callback && phase.callback->program;
    const auto maximumHorizon = playableProgram || playableStartupProgram(phase) || fadingProgram || retainedLevel1Program(phase) || retainedLevel2
        || retainedLevel2RetryPaletteProgram(phase) ? maximumProgramPaletteHorizonCycles : maximumNormalWorkHorizonCycles;
    if (horizonCycles <= 0 || horizonCycles > maximumHorizon) {
        throw std::invalid_argument("invalid bounded palette horizon");
    }
    OplWorkClock clock(phase, horizonCycles, EntryPolicy::retained, access);
    return clock.palette(state, request);
}

PaletteClockTiming playableDecoyPaletteClockTimingBound(const SoundBlasterClockPhase& phase,
    const PaletteWorkState& state,const std::int64_t horizonCycles,KeyboardClockAccess* access) {
    if(!phase.playableCampaign || horizonCycles<=0 || horizonCycles>maximumProgramPaletteHorizonCycles) {
        throw std::invalid_argument("invalid retained decoy palette horizon/domain");
    }
    OplWorkClock clock(phase,horizonCycles,EntryPolicy::retained,access);
    return clock.playableDecoyPalette(state);
}

SuccessPaletteClockTiming successPaletteClockTiming(const SoundBlasterClockPhase& phase,
    const SuccessPaletteState& state, const PaletteWorkState& palette,
    const SuccessPaletteExecution execution, const SuccessPaletteCompletionSource completion,
    const std::int64_t horizonCycles) {
    return successPaletteClockTimingBound(phase,state,palette,execution,completion,horizonCycles,nullptr);
}

SuccessPaletteClockTiming successPaletteClockTimingBound(const SoundBlasterClockPhase& phase,
    const SuccessPaletteState& state, const PaletteWorkState& palette,
    const SuccessPaletteExecution execution, const SuccessPaletteCompletionSource completion,
    const std::int64_t horizonCycles, KeyboardClockAccess* access) {
    if (horizonCycles <= 0 || horizonCycles > maximumNormalWorkHorizonCycles) {
        throw std::invalid_argument("invalid bounded success palette horizon");
    }
    OplWorkClock clock(phase,horizonCycles,EntryPolicy::retained,access);
    return clock.successPalette(state,palette,execution,completion);
}

SuccessPaletteStepTiming successPaletteStepTimingBound(const SoundBlasterClockPhase& phase,
    const SuccessPaletteResidentCursor& cursor, const PaletteWorkState& palette,
    const std::uint64_t foregroundBudget, const std::int64_t horizonCycles, KeyboardClockAccess* access) {
    if (horizonCycles <= 0 || horizonCycles > maximumNormalWorkHorizonCycles) {
        throw std::invalid_argument("invalid bounded success palette step horizon");
    }
    OplWorkClock clock(phase,horizonCycles,EntryPolicy::retained,access);
    return clock.successPaletteStep(cursor,palette,foregroundBudget);
}

SuccessPaletteStepTiming successPaletteStepTiming(const SoundBlasterClockPhase& phase,
    const SuccessPaletteResidentCursor& cursor, const PaletteWorkState& palette,
    const std::uint64_t foregroundBudget, const std::int64_t horizonCycles) {
    return successPaletteStepTimingBound(phase,cursor,palette,foregroundBudget,horizonCycles,nullptr);
}

NormalWorkTiming normalWorkTiming(const SoundBlasterClockPhase& phase,
    const std::vector<NormalWorkOperation>& operations, const std::int64_t horizonCycles) {
    return normalWorkTimingBound(phase, operations, horizonCycles, nullptr);
}

NormalWorkTiming normalWorkTimingBound(const SoundBlasterClockPhase& phase,
    const std::vector<NormalWorkOperation>& operations, const std::int64_t horizonCycles, KeyboardClockAccess* access) {
    if (operations.empty() || operations.size() > maximumWorkOperations
        || horizonCycles <= 0 || horizonCycles > maximumNormalWorkHorizonCycles) {
        throw std::invalid_argument("invalid bounded normal clock work");
    }
    OplWorkClock clock(phase, horizonCycles, EntryPolicy::retained, access);
    NormalWorkTiming result{};
    const auto start = clock.position();
    for (const auto& operation : operations) {
        if (operation.count > static_cast<std::uint64_t>(horizonCycles)) {
            throw std::invalid_argument("normal clock operation exceeds bounded horizon");
        }
        switch (operation.kind) {
        case NormalWorkKind::instructions:
            clock.plain(static_cast<std::size_t>(operation.count), result.total);
            break;
        case NormalWorkKind::string:
            clock.string(operation.count, result);
            break;
        case NormalWorkKind::repeatString:
            clock.repeatString(operation.count, result);
            break;
        case NormalWorkKind::port8Write:
        case NormalWorkKind::port16Write:
            for (std::uint64_t index = 0; index < operation.count; ++index) {
                clock.dispatch(operation.kind == NormalWorkKind::port8Write ? Operation::write : Operation::wordWrite,
                    result.total);
            }
            break;
        case NormalWorkKind::cli:
        case NormalWorkKind::sti:
        case NormalWorkKind::popFlagsEnabled:
            if (operation.count != 1) {
                throw std::invalid_argument("foreground flag operation requires one instruction");
            }
            clock.foregroundFlags(operation.kind, result.total);
            break;
        case NormalWorkKind::keyboardDataRead:
        case NormalWorkKind::keyboardControlRead:
        case NormalWorkKind::keyboardControlWrite:
            clock.keyboardPort(operation, result);
            break;
        case NormalWorkKind::keyboardConsumeHead:
        case NormalWorkKind::keyboardStoreScan:
            clock.controllerEffect(operation, result.total);
            break;
        case NormalWorkKind::timerInstallClearMask:
        case NormalWorkKind::timerInstallSaveBiosVector:
        case NormalWorkKind::timerInstallBiosReload:
        case NormalWorkKind::timerInstallBiosCount:
        case NormalWorkKind::timerInstallDivisor:
        case NormalWorkKind::timerInstallPitControl:
        case NormalWorkKind::timerInstallPitLow:
        case NormalWorkKind::timerInstallPitHigh:
        case NormalWorkKind::timerInstallPicMask:
        case NormalWorkKind::timerInstallVector:
        case NormalWorkKind::timerInstallComplete:
            clock.timerInstallEffect(operation, result.total);
            break;
        case NormalWorkKind::timerCallbackReload:
        case NormalWorkKind::timerCallbackCount:
        case NormalWorkKind::timerCallbackOffset:
        case NormalWorkKind::timerCallbackSegment:
        case NormalWorkKind::timerCallbackActivate:
            clock.timerCallbackEffect(operation, result.total);
            break;
        case NormalWorkKind::soundBlasterOplWrite:
        case NormalWorkKind::soundBlasterDriverEnable:
            clock.soundBlasterEffect(operation, result);
            break;
        case NormalWorkKind::soundBlasterSequencePublish:
        case NormalWorkKind::startupMenuCountdown:
        case NormalWorkKind::timerGlobalCount:
            clock.sequenceEffect(operation, result);
            break;
        case NormalWorkKind::startupCountdownPoll:
            clock.startupCountdownPoll(operation, result);
            break;
        case NormalWorkKind::playableStartupLogoCountdown:
        case NormalWorkKind::playableStartupLogoWaitStore:
        case NormalWorkKind::playableStartupLogoEmptyReturn:
        case NormalWorkKind::playableStartupMenuCountdown:
        case NormalWorkKind::playableStartupCreditsCountdown:
        case NormalWorkKind::playableStartupCreditsInput:
        case NormalWorkKind::playableStartupCreditsPoll:
        case NormalWorkKind::playableStartupCreditsSkip:
        case NormalWorkKind::playableStartupMenuEntryCountdown:
        case NormalWorkKind::playableStartupBeginFadeRequest:
        case NormalWorkKind::playableStartupBeginEmptyReturn:
            clock.playableStartupMusicOperation(operation,result);
            break;
        case NormalWorkKind::playableSuccessCountdownStore:
            clock.playableSuccessCountdownStore(operation,result);
            break;
        case NormalWorkKind::playableIrqWaitStore:
        case NormalWorkKind::playableIrqWaitPoll:
            clock.playableIrqWait(operation,result);
            break;
        case NormalWorkKind::playableAttractMenuDecision:
        case NormalWorkKind::playableAttractCountdownStore:
        case NormalWorkKind::playableAttractCountdownRead:
        case NormalWorkKind::playableAttractWaitPoll:
        case NormalWorkKind::playableAttractCreditsPoll:
        case NormalWorkKind::playableAttractLogoOnce:
        case NormalWorkKind::playableAttractLogoEndPoll:
            clock.playableAttract(operation,result);
            break;
        case NormalWorkKind::playableJoystickBriefingRefresh:
            clock.playableJoystickBriefingRefresh(operation,result);break;
        case NormalWorkKind::playableJoystickDebounceStore:
        case NormalWorkKind::playableJoystickDebouncePoll:
            clock.playableJoystickDebounce(operation,result);break;
        case NormalWorkKind::playableStoryWaitStore:
        case NormalWorkKind::playableStoryWaitPoll:
            clock.playableStoryWait(operation,result);
            break;
        case NormalWorkKind::startupMenuInputDecision:
        case NormalWorkKind::playableMenuNoAttractDecision:
            clock.startupMenuInputDecision(operation,result);
            break;
        case NormalWorkKind::startupMenuDecision:
            clock.startupMenuDecision(operation,result);
            break;
        case NormalWorkKind::startupBeginFadeRequest:
            clock.startupBeginFade(operation,result);
            break;
        case NormalWorkKind::startupBeginCountdown:
            clock.startupBeginCountdown(operation,result);
            break;
        case NormalWorkKind::startupBeginWait:
            clock.startupBeginWait(operation,result);
            break;
        case NormalWorkKind::startupBeginStop:
            clock.startupBeginStop(operation,result);
            break;
        case NormalWorkKind::successMusicEntryStop:
        case NormalWorkKind::successMusicActivate:
        case NormalWorkKind::successCountdownStore:
        case NormalWorkKind::successMusicReturnStop:
            clock.successMusic(operation,result);
            break;
        case NormalWorkKind::level2MusicEmptyStop:
        case NormalWorkKind::level2MusicActivate:
        case NormalWorkKind::level2DirectReset:
            clock.level2Music(operation,result);
            break;
        case NormalWorkKind::level2RetryMusicStop:
        case NormalWorkKind::level2RetryMusicEmptyStop:
            clock.level2RetryMusicStop(operation,result);
            break;
        case NormalWorkKind::level2RetryMusicActivate:
        case NormalWorkKind::level2RetryDirectReset:
            clock.level2Music(operation,result,OplWorkClock::Level2MusicDomain::retry);
            break;
        case NormalWorkKind::level1RetryMusicStop:
            clock.level1RetryMusicStop(operation,result);
            break;
        case NormalWorkKind::startupMenuEntryCountdown:
            clock.startupMenuEntryCountdown(operation, result);
            break;
        case NormalWorkKind::startupCreditsCountdown:
        case NormalWorkKind::startupCreditsFirstInput:
            clock.startupCreditsInput(operation, result);
            break;
        case NormalWorkKind::startupMenuReentryCountdown:
            clock.startupMenuReentryCountdown(operation,result);
            break;
        case NormalWorkKind::startupLogoEventSuppressed:
            clock.startupLogoEventSuppressed(operation, result);
            break;
        case NormalWorkKind::startupLogoEvent:
            clock.startupLogoEvent(operation, result);
            break;
        case NormalWorkKind::gameplayPrimaryFireEvent:
            clock.gameplayPrimaryFire(operation, result);
            break;
        case NormalWorkKind::gameplayLevel2PrimaryFireEvent:
        case NormalWorkKind::gameplayLevel2EnemyFireEvent:
            clock.gameplayLevel2Fire(operation, result);
            break;
        case NormalWorkKind::gameplayLevel2EnemyType20Event:
            clock.gameplayEnemyType20(operation,result,OplWorkClock::DirectCampaignDomain::level2);
            break;
        case NormalWorkKind::gameplayLevel2SolidContactEvent:
            clock.gameplaySolidContact(operation,result,OplWorkClock::DirectCampaignDomain::level2);
            break;
        case NormalWorkKind::gameplayEnemyFireEvent:
            clock.gameplayEnemyFire(operation, result);
            break;
        case NormalWorkKind::gameplayGuardianFireEvent:
        case NormalWorkKind::gameplayPlayerHitEvent:
        case NormalWorkKind::failureExplosionEvent:
            clock.gameplayPlayerHit(operation, result);
            break;
        case NormalWorkKind::gameplayTargetHitEvent:
            clock.gameplayTargetHit(operation, result);
            break;
        case NormalWorkKind::failureLevel2ExplosionEvent:
        case NormalWorkKind::gameplayLevel2TargetHitEvent:
            clock.gameplayLevel2TargetHit(operation, result);
            break;
        case NormalWorkKind::gameplayBonusPickupEvent:
            clock.gameplayBonusPickup(operation,result);
            break;
        case NormalWorkKind::gameplayArrivalStopEvent:
            clock.gameplayArrivalStop(operation,result);
            break;
        case NormalWorkKind::gameplayLevel2ArrivalStopEvent:
            clock.gameplayArrivalStop(operation,result,OplWorkClock::DirectCampaignDomain::level2);
            break;
        case NormalWorkKind::gameplayArrivalEvent:
            clock.gameplayArrival(operation,result);
            break;
        case NormalWorkKind::gameplayLevel2ArrivalEvent:
            clock.gameplayArrival(operation,result,OplWorkClock::DirectCampaignDomain::level2);
            break;
        case NormalWorkKind::gameplaySolidContactEvent:
            clock.gameplaySolidContact(operation,result);
            break;
        case NormalWorkKind::gameplayGuardianDefeatEvent:
            clock.gameplayGuardianDefeat(operation,result);
            break;
        case NormalWorkKind::gameplayLevel2GuardianDefeatEvent:
            clock.gameplayGuardianDefeat(operation,result,OplWorkClock::DirectCampaignDomain::level2);
            break;
        case NormalWorkKind::gameplayLevel2PlayerCombatEvent:
            clock.gameplayLevel2PlayerCombat(operation, result);
            break;
        case NormalWorkKind::gameplayPlayerCombatEvent:
            clock.gameplayPlayerCombat(operation, result);
            break;
        case NormalWorkKind::startupLogoCountdown:
        case NormalWorkKind::startupLogoFrameReturn:
            clock.startupLogoFrame(operation, result.total);
            break;
        case NormalWorkKind::startupLogoOnce:
        case NormalWorkKind::startupLogoEndPoll:
            clock.startupLogoSound(operation, result);
            break;
        case NormalWorkKind::startupLogoStop:
            clock.startupLogoStop(operation, result);
            break;
        default:
            throw std::invalid_argument("unsupported normal clock operation");
        }
    }
    clock.finish(result.total, start);
    result.phase = clock.retainedPhase();
    result.callbacks = clock.scheduledCallbacks();
    result.vgaServices = clock.vgaServices();
    result.keyboardIrqs = clock.keyboardIrqs();
    return result;
}

SoundBlasterClockPhase queueUpKey(const SoundBlasterClockPhase& phase, const KeyboardEdge edge) {
    OplWorkClock clock(phase, 1, EntryPolicy::retained);
    clock.queueUp(edge);
    return clock.retainedPhase();
}

std::array<SoundBlasterClockStep, 2> soundBlasterOplTiming(const SoundBlasterClockPhase& phase) {
    OplWorkClock clock(phase, writerHorizonCycles);
    std::array<SoundBlasterClockStep, 2> steps{};
    const auto start = clock.position();
    auto& prefix = steps[0];
    oplAddress(clock, prefix);
    clock.finish(prefix, start);
    const auto middle = clock.position();
    auto& suffix = steps[1];
    oplData(clock, suffix, start);
    clock.finish(suffix, middle);
    return steps;
}

SoundBlasterCallbackClock soundBlasterCallbackTiming(
    const SoundBlasterClockPhase& phase,
    const std::vector<SoundBlasterWorkBlock>& blocks,
    const SoundBlasterCallbackKind kind
) {
    if (blocks.empty() || blocks.size() > (programCallback(kind)
            ? maximumProgramCallbackBlocks : maximumCallbackBlocks)
        || (kind != SoundBlasterCallbackKind::wait && kind != SoundBlasterCallbackKind::explicitKeyOff
         && kind != SoundBlasterCallbackKind::explicitNoteOn && kind != SoundBlasterCallbackKind::noHandle
         && !programCallback(kind))) {
        throw std::invalid_argument("invalid bounded callback work blocks");
    }
    const auto horizon = callbackWorkHorizon(blocks, kind);
    OplWorkClock clock(phase, horizon);
    SoundBlasterCallbackClock result{};
    const auto start = clock.position();
    executeCallbackWork(clock, blocks, kind, result, start);
    return result;
}

}  // namespace stellar7
