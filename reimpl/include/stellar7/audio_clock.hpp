#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "stellar7/audio_work.hpp"
#include "stellar7/keyboard_transport.hpp"

namespace stellar7 {

enum class AudioClockEventKind { scanline, dmaDac, pit, irqCheck, frameStart, unsupported, pendingVgaInterrupt,
    displayStartLatch, panningLatch, pendingCmos, keyboardTransfer };
enum class AudioClockInterrupts { enabled, masked };
enum class AudioClockFrameStart { unsupported, activeUnlockedVgaSurface };
enum class AudioClockInterruptDelivery { unsupported, blockedByIrq0, residentTimerIrq0,
    residentTimerKeyboard, blockedByIrq1, defaultBiosIrq0 };
enum class AudioClockVgaInterrupt { unsupported, fixedVgaNoDemand };

struct AudioClockEvent {
    double deadlineMilliseconds;
    AudioClockEventKind kind;
    std::uint64_t value;
};

struct AudioClockVgaFramePhase {
    // Standard vsync only; manual/faithful host synchronization is unsupported.
    double framesPerSecond;
    double timeBaseMilliseconds;
    std::uint32_t framesSinceTimeBase;
};

struct AudioClockDevicePhase {
    // Double phase is validated at floor/ceil CPU-cycle bounds, not native FP80 bits.
    std::uint64_t picTicks = 0;
    double pitStartMilliseconds = 0;
    double pitNowMilliseconds = 0;
    std::uint32_t pitCycleBase = 0;
    // Fixed square-wave PIT, unchanged counter, no VGA synchronization; masked DMA.
    bool fixedPitAndMaskedDma = false;
    double framePeriodMilliseconds = 0;
    double frameStartMilliseconds = 0;
    double displayEndMilliseconds = 0;
    double retraceStartMilliseconds = 0;
    double retraceEndMilliseconds = 0;
    std::uint32_t displayStart = 0;
    std::uint32_t realStart = 0;
    std::uint32_t memoryMask = 0;
    std::uint32_t configuredBytesSkip = 0;
    std::uint32_t drawnBytesSkip = 0;
    std::uint32_t pelPanning = 0;
    std::uint32_t drawnPanning = 0;
    std::uint32_t verticalRetraceEndRegister = 0;
    bool verticalRetraceTriggered = false;
    bool irq0Requested = false;
    bool irq0InService = false;
    bool irq1Requested = false;
    bool irq1Input = false;
    bool irq1InService = false;
    std::optional<AudioClockVgaFramePhase> frame;
};

enum class AudioClockBiosDomain { unsupported, normalVgaDefaultTimer };

struct AudioClockBiosPhase {
    // Ownership certifies default INT8/INT1C, ordinary VGA and normal PIC without NMI.
    AudioClockBiosDomain domain = AudioClockBiosDomain::unsupported;
    std::uint32_t timerTicks = 0;
    std::uint8_t rollover = 0;
    std::uint8_t keyboardState = 0;
    std::uint8_t keyboardLeds = 0;
    bool keyboardLedSync = false;
    bool syncTime = false;
    bool cmosSyncPending = false;
    std::uint8_t floppyTimeout = 0;
    std::uint8_t motorBits = 0;
};

enum class AudioClockRtcDomain { unsupported, inactiveNoAccess };

struct AudioClockRtcPhase {
    AudioClockRtcDomain domain = AudioClockRtcDomain::unsupported;
    std::uint8_t registerB = 0;
    std::uint8_t registerC = 0;
    bool acknowledged = false;
    double endedMilliseconds = 0;
    float delayMilliseconds = 0;
    std::uint8_t divider = 0;
    std::uint8_t selected = 0;
    bool nmi = false;
    bool locked = false;
    bool bcd = false;
    bool ampm = false;
    // second, minute, hour, weekday, day, month, year; source carry rules apply.
    std::array<std::uint16_t, 7> calendar{};
    std::int64_t clockTime = 0;
};

struct AudioClockTimerPhase {
    std::uint16_t activeMask = 0;
    std::uint16_t globalCount = 0;
    std::uint16_t biosCount = 0;
    std::uint16_t callbackCount = 0;
    std::uint16_t callbackReload = 0;
    std::uint16_t biosReload = 0;
};

enum class AudioClockTimerInstallStage { fresh, maskCleared, biosVectorSaved,
    biosReloadWritten, biosCountWritten, divisorWritten, controlWritten,
    lowByteWritten, highByteWritten, picMaskWritten, vectorInstalled, installed };
enum class AudioClockCallbackRegistrationStage { fresh, reloadWritten, countWritten,
    offsetWritten, segmentWritten, active };

// Declared native installer ownership; absent on historical restored clock paths.
struct AudioClockTimerInstallPhase {
    AudioClockTimerInstallStage stage = AudioClockTimerInstallStage::fresh;
    std::uint8_t masterPicMask = 0xf8;
    std::uint8_t pitLowByte = 0;
    std::uint16_t pitDivisor = 0;
    bool irq0InputHigh = false;
    AudioClockCallbackRegistrationStage registration = AudioClockCallbackRegistrationStage::fresh;
    std::uint16_t callbackOffset = 0;
    std::uint16_t callbackSegment = 0;
};

struct AudioClockSoundBlasterInitializationPhase {
    std::uint16_t oplWrites = 0;
    bool driverEnabled = false;
};

struct AudioClockSequenceSelectionPhase {
    SoundBlasterCallbackState prepared;
    bool published = false;
    bool countdownWritten = false;
    bool countdownReturned = false;
    bool logoEventPlayed = false;
    bool logoEventSuppressed = false;
    std::uint8_t logoFramesCompleted = 0;
    bool logoFrameActive = false;
    bool logoSoundWaiting = false;
    bool logoSoundReturned = false;
    bool logoCurrentCleared = false;
    bool logoTimerFallback = false;
    bool logoEmptyCurrentReturned = false;
    std::uint8_t songOrdinal = 0;
    bool menuCountdownWritten = false;
    bool creditsCountdownWritten = false;
    bool creditsInputReturned = false;
    bool creditsSkipReturned = false;
    bool menuEntryCountdownWritten = false;
    std::uint8_t beginDecisions = 0;
    bool recoveredMenuInput = false;
    std::uint8_t recoveredMenuResult = 0;
    bool recoveredMenuActivated = false;
    std::uint8_t beginFadeRequests = 0;
    bool beginCountdownWritten = false;
    bool beginCountdownReturned = false;
    bool beginCurrentCleared = false;
    bool beginEmptyCurrentReturned = false;
    bool level1DirectResetCompleted = false;
    bool level1RetryCurrentCleared = false;
    std::optional<SoundBlasterCallbackState> level1RetryStoppedHandle;
};

enum class SuccessAudioStage { resourceBound, entryStopped, playing, countdownStored, returned };

struct AudioClockSuccessAudioPhase {
    // Native provider binds the already-loaded0086 SSM0 payload; no allocator work.
    std::vector<std::uint8_t> sequence;
    SuccessAudioStage stage = SuccessAudioStage::resourceBound;
};

enum class AudioClockLevel2RetryMusicStage { entered, stopped, emptyStopped, resourceBound, playing, reset };
struct AudioClockLevel2RetryMusicPhase {
    AudioClockLevel2RetryMusicStage stage = AudioClockLevel2RetryMusicStage::entered;
};

enum class AudioClockAttractBorrow { inactive, menuPresentation };

struct SoundBlasterClockPhase {
    // Budget before the normal loop's pre-dispatch decrement.
    std::int64_t remainingCycles;
    std::int64_t deferredCycles;
    AudioClockInterrupts interrupts;
    std::uint32_t isa8DelayNanoseconds;
    std::uint32_t dmaDacRate;
    double pitPeriodMilliseconds;
    double irqDelayMilliseconds;
    bool irqPending;
    bool irq0ActivationPossible;
    double singleLinePeriodMilliseconds;
    double horizontalPeriodMilliseconds;
    std::uint32_t linesDone;
    std::uint32_t linesTotal;
    std::uint32_t verticalBlankSkip;
    double minimumLatchDelayMilliseconds;
    AudioClockFrameStart frameStart;
    std::vector<AudioClockEvent> events;
    bool irqCheckRequested = false;
    AudioClockInterruptDelivery interruptDelivery = AudioClockInterruptDelivery::unsupported;
    AudioClockVgaInterrupt vgaInterrupt = AudioClockVgaInterrupt::unsupported;
    std::optional<AudioClockDevicePhase> devices;
    std::optional<AudioClockTimerPhase> timer;
    std::optional<SoundBlasterCallbackState> callback;
    std::optional<AudioClockBiosPhase> bios;
    std::optional<AudioClockRtcPhase> rtc;
    std::optional<KeyboardTransportState> keyboard;
    // Physical edge delivered after the next instruction's existing precharge.
    std::optional<KeyboardEdge> instructionKeyboardEdge;
    std::optional<AudioClockTimerInstallPhase> timerInstall;
    std::optional<AudioClockSoundBlasterInitializationPhase> soundBlasterInitialization;
    std::optional<AudioClockSequenceSelectionPhase> sequenceSelection;
    std::optional<AudioClockSuccessAudioPhase> successAudio;
    std::optional<AudioClockLevel2RetryMusicPhase> level2RetryMusic;
    // Explicit portable campaign: unmeasured foreground work, retained device time.
    bool playableCampaign = false;
    bool playableJoystickControls = false;
    bool playableStartup = false;
    bool playableMusicOff = false;
    bool playableSuccessCountdownWritten = false;
    //Native capability metadata, not recovered DOS state or an attract-provider receipt.
    bool playableMenuNoAttract = false;
    //Explicit native attract provider; never grants a recovered-source receipt.
    bool playableAutomaticAttract = false;
    AudioClockAttractBorrow attractBorrow=AudioClockAttractBorrow::inactive;
};

struct SoundBlasterClockStep {
    std::uint64_t dispatches = 0;
    std::int64_t delayCycles = 0;
    std::uint64_t terminalCharges = 0;
    std::int64_t elapsedCycles = 0;
    std::int64_t remainingCycles = 0;
    std::int64_t deferredCycles = 0;
    std::uint32_t linesDone = 0;
    std::uint32_t millisecondRollovers = 0;
    std::vector<AudioClockEvent> boundedEvents;
};

enum class BiosClockBoundary { preInt8, postInt8, preInt1c, int1cEntry, postInt1c, preEoi, preIret };

struct BiosClockCheckpoint {
    BiosClockBoundary boundary;
    SoundBlasterClockStep timing;
    SoundBlasterClockPhase phase;
};

struct BiosClockTiming {
    SoundBlasterClockStep total;
    SoundBlasterClockPhase phase;
    std::vector<BiosClockCheckpoint> checkpoints;
};

// BIOS STI entry, masked IF and IRQ0 in service; excludes resident wrapper.
// Checkpoints retain the budget before the next normal pre-dispatch decrement.
// Completed phase includes final outer IRET and source PIC recheck; no CPU register model.
BiosClockTiming biosChainTiming(const SoundBlasterClockPhase& phase);

// Fixed normal5000/ISA8 writer, IF masked, at most120us. Two ordered phases:
// entry->data OUT and data OUT->RET. RET/CALL and distant device work excluded.
std::array<SoundBlasterClockStep, 2> soundBlasterOplTiming(const SoundBlasterClockPhase& phase);

struct SoundBlasterCallbackClock {
    SoundBlasterClockStep total;
    // Cumulative entry->writer entry, data OUT and pre-RET ledgers.
    std::vector<std::array<SoundBlasterClockStep, 3>> opl;
    // Hardware write takes effect after the data OUT dispatch and I/O delay.
    std::vector<SoundBlasterClockStep> oplEffects;
    // Before the sequencer STI and resident POPF; absent on the wait branch.
    std::vector<SoundBlasterClockStep> interruptBoundaries;
};

// Ordered recovered callback work, masked entry: wait/key-off400us, note-on500us.
// Each OPL block
// includes RET. Only the incoming phase is supplied; no interior state imports.
SoundBlasterCallbackClock soundBlasterCallbackTiming(
    const SoundBlasterClockPhase& phase,
    const std::vector<SoundBlasterWorkBlock>& blocks,
    SoundBlasterCallbackKind kind
);

}  // namespace stellar7
