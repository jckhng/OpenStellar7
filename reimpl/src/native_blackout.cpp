#include "stellar7/presentation_clock.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::uint64_t nanosecondsPerSecond = 1'000'000'000;
constexpr std::uint64_t cycleNanoseconds = 200;
constexpr std::uint64_t frameNanoseconds = 14'268'064;
constexpr std::uint64_t retraceStartNanoseconds = 13'092'354;
constexpr std::uint64_t retraceEndNanoseconds = 13'155'909;
constexpr std::uint64_t isaReadCycles = 6;
constexpr std::uint64_t isaWriteCycles = 4;
constexpr std::uint64_t maximumRepTransfers = 4;
constexpr std::uint64_t pitInputClock = 1'193'182;
constexpr std::uint64_t pitDivisor = 0xffff / 13;
constexpr std::uint64_t rawIrqsPerCallback = 4;
constexpr std::uint64_t paletteComputeDispatches = 17'450;
constexpr std::uint64_t paletteComputeTransfers = vgaPaletteBytes * 2;
constexpr std::uint16_t dacIndexPort = 0x3c8;
constexpr std::uint16_t dacDataPort = 0x3c9;

std::uint64_t ceilDivide(const std::uint64_t value, const std::uint64_t divisor) {
    return value / divisor + (value % divisor != 0);
}

std::uint64_t foregroundCycles(const std::vector<NormalWorkOperation>& operations) {
    std::uint64_t cycles = 0;
    for (const auto& operation : operations) {
        switch (operation.kind) {
        case NormalWorkKind::instructions:
            cycles += operation.count;
            break;
        case NormalWorkKind::string:
            cycles += operation.count * 2;
            break;
        case NormalWorkKind::repeatString:
            // Native foreground budget excludes slice/refill/IRQ restart charges.
            cycles += operation.count + std::max<std::uint64_t>(1, ceilDivide(operation.count, maximumRepTransfers));
            break;
        case NormalWorkKind::port8Write:
            cycles += operation.count * isaWriteCycles;
            break;
        case NormalWorkKind::port16Write:
            cycles += operation.count * 3;
            break;
        default:
            throw std::invalid_argument("unsupported native blackout foreground operation");
        }
    }
    return cycles;
}
} //namespace

NativePaletteWork PresentationClock::renderNativePalette(const PaletteWorkState& state,
    const PaletteRequest& request, const std::uint64_t prefixDispatches, const std::uint64_t suffixDispatches) {
    constexpr auto irqPeriod = SoundBlasterSynth::sampleRate * pitDivisor;
    const auto driver = synth_.driverState();
    //A preceding CLI upload may leave one native raw IRQ pending beyond the callback edge.
    if (workClockActive_ || driver.clockSource != SoundBlasterClockSource::samples
        || driver.clockPeriod != irqPeriod * rawIrqsPerCallback
        || driver.clockPhase >= driver.clockPeriod + irqPeriod + pitInputClock) {
        throw std::invalid_argument("native palette requires the retained sample/PIT phase and at most one masked IRQ");
    }
    if (request.start != 0 || request.count != vgaPaletteColors || request.targetColor != 0
        || state.dac.mode != PaletteDacMode::write || state.dac.pelIndex != 0
        || synth_.sampledVoiceObservation().phase != SampledVoicePhase::unloaded) {
        throw std::invalid_argument("native palette requires a full bank0 request, completed DAC and released speech");
    }
    NativePaletteWork result{};
    result.palette = completedFullPaletteUpload(state, request);
    const auto sampleTime = elapsedSamples_ / SoundBlasterSynth::sampleRate * nanosecondsPerSecond
        + elapsedSamples_ % SoundBlasterSynth::sampleRate * nanosecondsPerSecond / SoundBlasterSynth::sampleRate;
    const auto origin = residentPaletteNanoseconds_.value_or(sampleTime);
    auto time = origin + (prefixDispatches + paletteComputeDispatches + paletteComputeTransfers + 1)
        * cycleNanoseconds; //Through069B, then MOV DX,3DA.
    bool high;
    do {
        time += isaReadCycles * cycleNanoseconds;
        const auto phase = time % frameNanoseconds;
        high = phase >= retraceStartNanoseconds && phase < retraceEndNanoseconds;
        time += 2 * cycleNanoseconds;
        ++result.highReads;
    } while (!high);
    time += (1 + isaReadCycles + 3) * cycleNanoseconds;
    const auto recordWrite = [&](const std::uint16_t port, const std::uint8_t value) {
        result.writes.push_back({static_cast<std::int64_t>((time - origin) / cycleNanoseconds), port, value});
    };
    time += (1 + isaWriteCycles) * cycleNanoseconds;
    recordWrite(dacDataPort, 0); //Incoming component0: no preliminary RGB commit.
    time += (2 + isaWriteCycles) * cycleNanoseconds;
    recordWrite(dacIndexPort, 0);
    time += 2 * cycleNanoseconds; //MOV DX,3C9; CLI completes before IF clears.
    const auto emitUntil = [&](const std::uint64_t end, const bool masked) {
        const auto target = end / nanosecondsPerSecond * SoundBlasterSynth::sampleRate
            + ceilDivide(end % nanosecondsPerSecond * SoundBlasterSynth::sampleRate, nanosecondsPerSecond);
        const auto count = static_cast<std::size_t>(target - elapsedSamples_);
        const auto samples = masked ? synth_.renderInterruptMasked(count) : synth_.render(count);
        result.samples.insert(result.samples.end(), samples.begin(), samples.end());
        elapsedSamples_ = target;
    };
    emitUntil(time, false);
    for (std::size_t component = 0; component < vgaPaletteBytes; ++component) {
        time += (2 + isaWriteCycles) * cycleNanoseconds;
        recordWrite(dacDataPort, result.palette.output[component]);
        if ((component + 1) % vgaPaletteComponents == 0) {
            const auto color = component / vgaPaletteComponents;
            const auto begin = result.palette.output.begin() + color * vgaPaletteComponents;
            result.commits.push_back({static_cast<std::int64_t>((time - origin) / cycleNanoseconds),
                static_cast<std::uint16_t>(color), {begin[0], begin[1], begin[2]}});
        }
        time += cycleNanoseconds; //LOOP.
    }
    time += 2 * cycleNanoseconds; //STI and first POP's interrupt shadow.
    emitUntil(time, true);
    time += (10 + suffixDispatches) * cycleNanoseconds;
    emitUntil(time, false);
    residentPaletteNanoseconds_ = time;
    result.elapsedNanoseconds = time - origin;
    return result;
}

NativePaletteWork PresentationClock::renderMenuPalette(const PaletteWorkState& state) {
    constexpr std::uint64_t menuPrefix = 8; //3546..3552, including the far CALL.
    constexpr std::uint64_t residentPrefix = 11;
    constexpr std::uint64_t residentSuffix = 3;
    constexpr std::uint64_t menuSuffix = 2; //3557 ADD SP; RET.
    return renderNativePalette(state, {0, vgaPaletteColors, 0, vgaComponentMaximum},
        menuPrefix + residentPrefix, residentSuffix + menuSuffix);
}

NativeBlackoutWork PresentationClock::renderNativeBlackout(const BlackoutWorkState& state,
    CockpitPageState& roles, IndexedPages& pages) {
    validateNativeIrqWait();
    const auto source = blackoutSourcePlan(state.surface, roles.frontPage());
    if (state.palette.dac.mode != PaletteDacMode::write || state.palette.dac.pelIndex != 0
        || synth_.sampledVoiceObservation().phase != SampledVoicePhase::unloaded) {
        throw std::invalid_argument("native blackout requires completed DAC upload and released sampled voice");
    }
    NativeBlackoutWork result{};
    result.state = state;
    auto palette = state.palette;
    for (std::size_t index = 0; index < blackoutPaletteSteps; ++index) {
        palette = completedFullPaletteUpload(palette,
            {0, vgaPaletteColors, 0, static_cast<std::uint8_t>(vgaComponentMaximum - index * 4)});
    }
    const auto clearCycles = foregroundCycles(source.clear);
    std::uint64_t elapsed = 0;
    palette = state.palette;
    for (std::size_t index = 0; index < blackoutPaletteSteps; ++index) {
        const auto before = elapsed;
        auto work = renderNativePalette(palette,
            {0, vgaPaletteColors, 0, static_cast<std::uint8_t>(vgaComponentMaximum - index * 4)},
            source.beforePalette[index], 0);
        palette = work.palette;
        elapsed += work.elapsedNanoseconds;
        work.elapsedNanoseconds = elapsed;
        for (auto& write : work.writes) {
            write.cycleOffset += static_cast<std::int64_t>(before / cycleNanoseconds);
        }
        for (auto& commit : work.commits) {
            commit.cycleOffset += static_cast<std::int64_t>(before / cycleNanoseconds);
        }
        result.steps[index] = std::move(work);
    }
    const auto end = *residentPaletteNanoseconds_ + clearCycles * cycleNanoseconds;
    const auto target = end / nanosecondsPerSecond * SoundBlasterSynth::sampleRate
        + ceilDivide(end % nanosecondsPerSecond * SoundBlasterSynth::sampleRate, nanosecondsPerSecond);
    result.clearSamples = synth_.render(static_cast<std::size_t>(target - elapsedSamples_));
    elapsedSamples_ = target;
    result.state.palette = std::move(palette);
    result.state.surface.firstColor = 0;
    result.state.surface.secondColor = 0;
    result.state.surface.sequencer = BlackoutSequencerState{2, 0};
    roles.selectFront();
    applyBlackoutClear(pages, roles.frontPage());
    residentPaletteNanoseconds_ = end;
    result.elapsedNanoseconds = elapsed + clearCycles * cycleNanoseconds;
    return result;
}
} //namespace stellar7
