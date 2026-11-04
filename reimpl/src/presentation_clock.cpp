#include "stellar7/presentation_clock.hpp"
#include "stellar7/success_palette_clock.hpp"
#include "stellar7/success_palette_step.hpp"
#include "keyboard_clock_access.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
SuccessPaletteStepTiming successPaletteStepTimingBound(const SoundBlasterClockPhase&,
    const SuccessPaletteResidentCursor&, const PaletteWorkState&, std::uint64_t, std::int64_t,
    KeyboardClockAccess*);
namespace {

constexpr std::uint64_t nativePitInputClock = 1'193'182;
constexpr std::uint64_t nativePitDivisor = 0xffff / 13;
constexpr std::uint64_t nativeIrqsPerAudioCallback = 4;
constexpr std::uint64_t nanosecondsPerSecond = 1'000'000'000;
constexpr std::uint64_t frameNanoseconds = 14'268'064;
constexpr std::uint64_t retraceStartNanoseconds = 13'092'354;
constexpr std::uint64_t retraceEndNanoseconds = 13'155'909;
constexpr std::uint64_t cycleNanoseconds = 200; // Fixed 5000 normal-core cycles/ms.
constexpr std::uint64_t paletteComponents = 256 * 3;
constexpr std::uint64_t computeInstructions = 17'450;
constexpr std::uint64_t computeStringCycles = paletteComponents * 2;
constexpr std::uint64_t readDelayCycles = 5; // Default ISA8.3: floor(5000 * 1020 / 1e6).
constexpr std::uint64_t writeDelayCycles = 3;
constexpr std::uint64_t pollCycles = 3 + readDelayCycles;
constexpr std::uint64_t uploadSetupCycles = 11 + readDelayCycles + writeDelayCycles * 2;
constexpr std::uint64_t uploadCycles = 1 + paletteComponents * (4 + writeDelayCycles);
constexpr std::uint64_t interruptShadowCycles = 2; // STI and following POP DI.
constexpr std::uint64_t returnCycles = 13;

std::uint64_t ceilDivide(const std::uint64_t value, const std::uint64_t divisor) {
    return value / divisor + (value % divisor != 0);
}

std::size_t samplesAt(const std::uint64_t nanoseconds) {
    return static_cast<std::size_t>(ceilDivide(
        nanoseconds * SoundBlasterSynth::sampleRate, nanosecondsPerSecond
    ));
}

void append(std::vector<std::int16_t>& output, const std::vector<std::int16_t>& samples) {
    output.insert(output.end(), samples.begin(), samples.end());
}

SoundBlasterClockPhase workPhase(const SoundBlasterClockPhase& phase, const SoundBlasterSynth& synth) {
    auto incoming = phase;
    if (!incoming.timer || incoming.timer->activeMask != 0) {
        // Only a registered callback borrows live sequencer state.
        incoming.callback = synth.callbackWorkState();
    }
    return incoming;
}

}  // namespace

std::array<EntryFadeStep, levelEntryFadeSteps> levelEntrySchedule(
    const std::uint64_t initialVideoPhaseNanoseconds
) {
    const auto origin = initialVideoPhaseNanoseconds % frameNanoseconds;
    auto time = origin;
    std::size_t emittedSamples = 0;
    std::array<EntryFadeStep, levelEntryFadeSteps> schedule{};
    for (std::size_t index = 0; index < schedule.size(); ++index) {
        // Outer loop and far-call wrapper precede the calibrated compute prefix.
        const std::uint64_t callCycles = index == 0 ? 20 : 22;
        time += (callCycles + computeInstructions + computeStringCycles) * cycleNanoseconds;
        time += (2 + readDelayCycles) * cycleNanoseconds; // MOV DX; first IN.
        const auto phase = time % frameNanoseconds;
        if (phase < retraceStartNanoseconds || phase >= retraceEndNanoseconds) {
            const auto wait = phase < retraceStartNanoseconds
                ? retraceStartNanoseconds - phase
                : frameNanoseconds - phase + retraceStartNanoseconds;
            time += ceilDivide(wait, pollCycles * cycleNanoseconds) * pollCycles * cycleNanoseconds;
        }
        // The original waits for retrace high, not for a low-to-high edge.
        time += (2 + uploadSetupCycles) * cycleNanoseconds;
        auto& step = schedule[index];
        step.uploadNanoseconds = time - origin;
        time += (uploadCycles + interruptShadowCycles) * cycleNanoseconds;
        step.releaseNanoseconds = time - origin;
        time += returnCycles * cycleNanoseconds;
        if (index + 1 == schedule.size()) {
            time += 6 * cycleNanoseconds; // Final loop test, POP SI and RET.
        }
        step.elapsedNanoseconds = time - origin;
        const auto uploadSample = samplesAt(step.uploadNanoseconds);
        const auto releaseSample = samplesAt(step.releaseNanoseconds);
        const auto returnSample = samplesAt(step.elapsedNanoseconds);
        step.beforeUploadSamples = uploadSample - emittedSamples;
        step.maskedSamples = releaseSample - uploadSample;
        step.afterUploadSamples = returnSample - releaseSample;
        emittedSamples = returnSample;
    }
    return schedule;
}

std::array<EntryFadeStep, levelEntryFadeSteps> PresentationClock::entrySchedule() const {
    // A cold portable start has phase zero. Do not import one recording's phase.
    const auto seconds = elapsedSamples_ / SoundBlasterSynth::sampleRate;
    const auto remainder = elapsedSamples_ % SoundBlasterSynth::sampleRate;
    const auto phase = ((seconds % frameNanoseconds) * nanosecondsPerSecond
        + remainder * nanosecondsPerSecond / SoundBlasterSynth::sampleRate) % frameNanoseconds;
    return levelEntrySchedule(phase);
}

bool startsLevelEntry(const GameplayPhase previous, const GameplayPhase current) noexcept {
    return (previous == GameplayPhase::level5LoaderReturn && current == GameplayPhase::level5)
        || (previous == GameplayPhase::successPresentation && current == GameplayPhase::level2Handoff)
        || (previous == GameplayPhase::level2SuccessPresentation && current == GameplayPhase::level3Handoff)
        || (previous == GameplayPhase::continuePrompt
            && (current == GameplayPhase::level1RetryHandoff || current == GameplayPhase::level2Handoff));
}

std::uint64_t pageFlipNanoseconds(
    const std::uint64_t initialVideoPhaseNanoseconds,
    const FrameSynchronization synchronization
) {
    constexpr std::uint64_t unsynchronizedCycles = 28;
    constexpr std::uint64_t firstReadCycles = 32;
    constexpr std::uint64_t returnAfterReadCycles = 6;
    if (synchronization == FrameSynchronization::disabled) {
        return unsynchronizedCycles * cycleNanoseconds;
    }
    const auto origin = initialVideoPhaseNanoseconds % frameNanoseconds;
    auto time = origin + firstReadCycles * cycleNanoseconds;
    auto phase = time % frameNanoseconds;
    if (phase >= retraceStartNanoseconds && phase < retraceEndNanoseconds) {
        time += ceilDivide(retraceEndNanoseconds - phase, pollCycles * cycleNanoseconds)
            * pollCycles * cycleNanoseconds;
    }
    // Unlike palette upload, the page flip consumes a low sample first.
    time += pollCycles * cycleNanoseconds;
    phase = time % frameNanoseconds;
    if (phase < retraceStartNanoseconds || phase >= retraceEndNanoseconds) {
        const auto wait = phase < retraceStartNanoseconds
            ? retraceStartNanoseconds - phase
            : frameNanoseconds - phase + retraceStartNanoseconds;
        time += ceilDivide(wait, pollCycles * cycleNanoseconds) * pollCycles * cycleNanoseconds;
    }
    return time - origin + returnAfterReadCycles * cycleNanoseconds;
}

std::vector<std::int16_t> PresentationClock::renderEntryStep(const EntryFadeStep& step) {
    if (workClockActive_) {
        throw std::logic_error("entry presentation requires recovered normal work after clock activation");
    }
    residentPaletteNanoseconds_.reset();
    auto samples = synth_.render(step.beforeUploadSamples);
    append(samples, synth_.renderInterruptMasked(step.maskedSamples));
    append(samples, synth_.render(step.afterUploadSamples));
    elapsedSamples_ += samples.size();
    return samples;
}

std::vector<std::int16_t> PresentationClock::renderEntry() {
    std::vector<std::int16_t> samples;
    for (const auto& step : entrySchedule()) {
        append(samples, renderEntryStep(step));
    }
    return samples;
}

std::vector<std::int16_t> PresentationClock::renderFrame() {
    return renderSamples(gameplayAudioSamplesPerFrame);
}

std::vector<std::int16_t> PresentationClock::renderSamples(const std::size_t count) {
    if (workClockActive_) {
        throw std::logic_error("frame presentation requires recovered normal work after clock activation");
    }
    residentPaletteNanoseconds_.reset();
    auto samples = synth_.render(count);
    elapsedSamples_ += samples.size();
    return samples;
}

void PresentationClock::validateNativeIrqWait() const {
    constexpr auto period = SoundBlasterSynth::sampleRate * nativePitDivisor * nativeIrqsPerAudioCallback;
    const auto state = synth_.driverState();
    if (workClockActive_ || state.clockSource != SoundBlasterClockSource::samples
        || state.clockPeriod != period || state.clockPhase >= period + nativePitInputClock) {
        throw std::invalid_argument("raw IRQ wait requires the retained native SBL PIT/sample phase");
    }
}

std::size_t PresentationClock::nativeIrqWaitSamples(const std::uint16_t count) const {
    validateNativeIrqWait();
    if (count == 0) {
        return 0;
    }
    const auto state = synth_.driverState();
    const auto irqPeriod = state.clockPeriod / nativeIrqsPerAudioCallback;
    return static_cast<std::size_t>(ceilDivide(count * irqPeriod - state.clockPhase % irqPeriod, nativePitInputClock));
}

std::vector<std::int16_t> PresentationClock::renderIrqWait(NativeIrqWait& wait, const std::size_t sampleBudget) {
    const auto required = nativeIrqWaitSamples(wait.remaining);
    const auto irqPeriod = synth_.driverState().clockPeriod / nativeIrqsPerAudioCallback;
    std::vector<std::int16_t> samples;
    samples.reserve(std::min(sampleBudget, required));
    while (wait.remaining != 0 && samples.size() < sampleBudget) {
        const auto before = synth_.driverState().clockPhase % irqPeriod;
        const auto next = renderSamples(1);
        samples.push_back(next.front());
        const auto after = synth_.driverState().clockPhase % irqPeriod;
        //A pending fourth-IRQ callback services an edge already counted earlier.
        if (after < before) {
            --wait.remaining;
        }
    }
    return samples;
}

SuccessPaletteState initialSuccessPalette(const SuccessPaletteChannel channel) {
    if (channel != SuccessPaletteChannel::green && channel != SuccessPaletteChannel::blue) {
        throw std::invalid_argument("unsupported success palette channel");
    }
    SuccessPaletteState result;
    result.channel = channel;
    for (std::size_t index = 0; index < result.values.size(); ++index) {
        result.values[index] = vgaComponentMaximum - static_cast<std::uint8_t>(index);
        result.directions[index] = 1;
    }
    return result;
}

NativeSuccessPaletteWork PresentationClock::renderSuccessPalette(const SuccessPaletteState& state,
    const SuccessPaletteExecution execution) {
    constexpr std::uint8_t minimumValue = 16;
    constexpr std::uint16_t firstColor = 16;
    constexpr std::uint16_t indexPort = 0x3c8;
    constexpr std::uint16_t dataPort = 0x3c9;
    if (workClockActive_ || (execution != SuccessPaletteExecution::initial && execution != SuccessPaletteExecution::advance)
        || (state.channel != SuccessPaletteChannel::green && state.channel != SuccessPaletteChannel::blue)) {
        throw std::invalid_argument("unsupported native resident success palette domain");
    }
    NativeSuccessPaletteWork result;
    result.state = state;
    std::uint64_t callerInstructions = 7; //705c..7069 argument setup and CALL.
    if (execution == SuccessPaletteExecution::advance) {
        //7078..70e5: live-audio status, color branch,48 iterations and arguments.
        //Blue's extra INC before the value balances green's INC after it.
        callerInstructions = 17 + state.values.size() * 16;
    }
    for (std::size_t index = 0; index < state.values.size(); ++index) {
        auto& value = result.state.values[index];
        auto& direction = result.state.directions[index];
        if (value < minimumValue || value > vgaComponentMaximum || (direction != -1 && direction != 1)) {
            throw std::invalid_argument("unsupported resident success palette oscillator");
        }
        if (execution == SuccessPaletteExecution::initial) {
            if (value != vgaComponentMaximum - index || direction != 1) {
                throw std::invalid_argument("resident success entry requires the initial source palette");
            }
            continue;
        }
        if (value <= minimumValue) {
            direction = 1;
        } else if (value >= vgaComponentMaximum) {
            direction = -1;
            ++callerInstructions; //70c3 upper-end direction assignment.
        }
        value = static_cast<std::uint8_t>(value + direction);
    }
    const auto sampleTime = elapsedSamples_ / SoundBlasterSynth::sampleRate * nanosecondsPerSecond
        + elapsedSamples_ % SoundBlasterSynth::sampleRate * nanosecondsPerSecond / SoundBlasterSynth::sampleRate;
    const auto origin = residentPaletteNanoseconds_.value_or(sampleTime);
    auto time = origin + (callerInstructions + 10) * cycleNanoseconds; //516b..517e.
    bool high = false;
    do {
        time += (1 + readDelayCycles) * cycleNanoseconds; //5181 IN3da.
        const auto phase = time % frameNanoseconds;
        high = phase >= retraceStartNanoseconds && phase < retraceEndNanoseconds;
        time += 2 * cycleNanoseconds; //AND/JZ; high-only, no preceding low wait.
        ++result.highReads;
    } while (!high);
    time += (5 + readDelayCycles) * cycleNanoseconds; //5186..518e;3c7 write mode.
    time += (2 + writeDelayCycles) * cycleNanoseconds; //5190 MOV/OUT0; no triplet commit.
    const auto recordWrite = [&](const std::uint16_t port, const std::uint8_t value) {
        result.writes.push_back({static_cast<std::int64_t>((time - origin) / cycleNanoseconds), port, value});
    };
    recordWrite(dataPort, 0);
    time += (3 + writeDelayCycles) * cycleNanoseconds; //5194/5197/5199.
    recordWrite(indexPort, firstColor);
    time += 2 * cycleNanoseconds; //519a MOV;519d CLI.
    const auto emitUntil = [&](const std::uint64_t end, const bool masked) {
        const auto target = end / nanosecondsPerSecond * SoundBlasterSynth::sampleRate
            + ceilDivide(end % nanosecondsPerSecond * SoundBlasterSynth::sampleRate, nanosecondsPerSecond);
        const auto count = static_cast<std::size_t>(target - elapsedSamples_);
        append(result.samples, masked ? synth_.renderInterruptMasked(count) : synth_.render(count));
        elapsedSamples_ = target;
    };
    emitUntil(time, false);
    for (std::size_t index = 0; index < state.values.size(); ++index) {
        std::array<std::uint8_t, vgaPaletteComponents> color{};
        color[state.channel == SuccessPaletteChannel::blue ? 2 : 1] = result.state.values[index];
        for (const auto component : color) {
            time += (2 + writeDelayCycles) * cycleNanoseconds; //LODSB/OUT.
            recordWrite(dataPort, component);
            time += cycleNanoseconds; //LOOP.
        }
        result.commits.push_back({static_cast<std::int64_t>((time - origin - cycleNanoseconds) / cycleNanoseconds),
            static_cast<std::uint16_t>(firstColor + index), color});
    }
    time += 2 * cycleNanoseconds; //STI and following POP SI interrupt shadow.
    emitUntil(time, true);
    synth_.resumeSampleInterrupts(); //Pending PIT IRQ dispatches before POP BP.
    time += (2 + (execution == SuccessPaletteExecution::initial ? 1 : 2)) * cycleNanoseconds;
    emitUntil(time, false); //POP BP/RET, caller cleanup and repeat JMP when present.
    residentPaletteNanoseconds_ = time;
    result.elapsedNanoseconds = time - origin;
    return result;
}

ClockedPresentationWork PresentationClock::renderWork(const SoundBlasterClockPhase& phase,
    const std::vector<NormalWorkOperation>& operations, const std::int64_t horizonCycles) {
    auto incoming = workPhase(phase, synth_);
    if (incoming.timer && incoming.timer->activeMask == 0
        && std::any_of(operations.begin(), operations.end(), [](const auto& operation) {
            return operation.kind == NormalWorkKind::timerCallbackActivate;
        })) {
        incoming.callback = synth_.callbackWorkState();
    }
    return renderTiming(normalWorkTimingBound(incoming, operations, horizonCycles, keyboardAccess_));
}

ClockedPresentationWork PresentationClock::renderTiming(NormalWorkTiming timing) {
    auto audio = synth_.renderClocked(timing, workCycles_);
    workCycles_ += static_cast<std::uint64_t>(timing.total.elapsedCycles);
    elapsedSamples_ += audio.samples.size();
    workClockActive_ = true;
    return {std::move(timing), std::move(audio)};
}

ClockedCockpitWork PresentationClock::renderCockpit(const SoundBlasterClockPhase& phase,
    const CockpitWorkState& state, const CockpitWorkBitmaps& bitmaps, const std::int64_t horizonCycles) {
    const auto plan = cockpitOperations(state, bitmaps);
    return {plan.state, renderWork(phase, plan.operations, horizonCycles)};
}

ClockedPresentationWork PresentationClock::renderHorizon(const SoundBlasterClockPhase& phase,
    const HorizonWorkState& state, const std::int64_t horizonCycles) {
    return renderWork(phase, horizonOperations(state), horizonCycles);
}

ClockedPageFlipWork PresentationClock::renderPageFlip(const SoundBlasterClockPhase& phase,
    const PageFlipClockState& state, const std::int64_t horizonCycles) {
    auto incoming = workPhase(phase, synth_);
    auto flip = pageFlipClockTimingBound(incoming, state, horizonCycles, keyboardAccess_);
    return {flip.state, renderTiming(std::move(flip.timing)), flip.lowReads, flip.highReads};
}

ClockedPaletteWork PresentationClock::renderPalette(const SoundBlasterClockPhase& phase,
    const PaletteWorkState& state, const PaletteRequest& request, const std::int64_t horizonCycles) {
    auto incoming = workPhase(phase, synth_);
    auto palette = paletteClockTimingBound(incoming, state, request, horizonCycles, keyboardAccess_);
    return {palette.state, renderTiming(std::move(palette.timing)), palette.highReads,
        palette.waitEntryCycles, palette.uploadStartCycles, palette.uploadEndCycles,
        std::move(palette.writes), std::move(palette.commits)};
}

ClockedPaletteWork PresentationClock::renderPlayableDecoyPalette(const SoundBlasterClockPhase& phase,
    const PaletteWorkState& state,const std::int64_t horizonCycles) {
    auto incoming=workPhase(phase,synth_);
    auto palette=playableDecoyPaletteClockTimingBound(incoming,state,horizonCycles,keyboardAccess_);
    return {palette.state,renderTiming(std::move(palette.timing)),palette.highReads,
        palette.waitEntryCycles,palette.uploadStartCycles,palette.uploadEndCycles,
        std::move(palette.writes),std::move(palette.commits)};
}

ClockedSuccessPaletteWork PresentationClock::renderSuccessPalette(const SoundBlasterClockPhase& phase,
    const SuccessPaletteState& state, const PaletteWorkState& palette,
    const SuccessPaletteExecution execution, const SuccessPaletteCompletionSource completion,
    const std::int64_t horizonCycles) {
    auto incoming = workPhase(phase, synth_);
    auto result = successPaletteClockTimingBound(incoming, state, palette, execution, completion,
        horizonCycles, keyboardAccess_);
    auto& upload = result.palette;
    return {result.state, {upload.state, renderTiming(std::move(upload.timing)), upload.highReads,
        upload.waitEntryCycles, upload.uploadStartCycles, upload.uploadEndCycles,
        std::move(upload.writes), std::move(upload.commits)}, result.boundary};
}

ClockedSuccessPaletteStepWork PresentationClock::renderSuccessPaletteStep(const SoundBlasterClockPhase& phase,
    const SuccessPaletteResidentCursor& cursor, const PaletteWorkState& palette,
    const std::uint64_t foregroundBudget, const std::int64_t horizonCycles) {
    auto incoming = workPhase(phase,synth_);
    auto result = successPaletteStepTimingBound(incoming,cursor,palette,foregroundBudget,horizonCycles,keyboardAccess_);
    auto& upload = result.palette;
    return {result.cursor,{upload.state,renderTiming(std::move(upload.timing)),upload.highReads,
        upload.waitEntryCycles,upload.uploadStartCycles,upload.uploadEndCycles,
        std::move(upload.writes),std::move(upload.commits)},result.boundary};
}

ClockedBlackoutWork PresentationClock::renderBlackout(const SoundBlasterClockPhase& phase,
    const BlackoutWorkState& state, CockpitPageState& roles, IndexedPages& pages,
    const std::int64_t horizonCycles, const BlackoutPageLayout layout) {
    auto result = renderBlackout(phase, state, roles.frontPage(), pages, horizonCycles,layout);
    roles.selectFront();
    return result;
}

ClockedBlackoutWork PresentationClock::renderBlackout(const SoundBlasterClockPhase& phase,
    const BlackoutWorkState& state, const std::uint16_t frontPage, IndexedPages& pages,
    const std::int64_t horizonCycles, const BlackoutPageLayout layout) {
    auto incoming = workPhase(phase, synth_);
    // Qualify the whole continuation on copies before advancing the live synth/pages.
    auto plan = blackoutClockTiming(incoming, state, frontPage, horizonCycles,layout);
    ClockedBlackoutWork result{};
    result.state = std::move(plan.state);
    for (std::size_t i = 0; i < blackoutPaletteSteps; ++i) {
        auto& step = plan.steps[i];
        result.steps[i].before = renderTiming(std::move(step.before));
        auto& palette = step.palette;
        result.steps[i].palette = {std::move(palette.state), renderTiming(std::move(palette.timing)),
            palette.highReads, palette.waitEntryCycles, palette.uploadStartCycles, palette.uploadEndCycles,
            std::move(palette.writes), std::move(palette.commits)};
        result.steps[i].intensity = step.intensity;
    }
    result.clear = renderTiming(std::move(plan.clear));
    applyBlackoutClear(pages, frontPage,layout);
    return result;
}

}  // namespace stellar7
