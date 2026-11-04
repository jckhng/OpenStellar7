#include "stellar7/presentation_clock.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace stellar7;

constexpr std::uint64_t nanosecondsPerSecond = 1'000'000'000;
constexpr std::uint64_t cycleNanoseconds = 200;
constexpr std::int16_t width = 320;
constexpr std::int16_t height = 200;
constexpr std::size_t prerollSamples = 137;

std::size_t sampleAt(const std::uint64_t time) {
    const auto numerator = time * SoundBlasterSynth::sampleRate;
    return static_cast<std::size_t>(numerator / nanosecondsPerSecond
        + (numerator % nanosecondsPerSecond != 0));
}

std::uint64_t sampleTime(const std::size_t samples) {
    return samples / SoundBlasterSynth::sampleRate * nanosecondsPerSecond
        + samples % SoundBlasterSynth::sampleRate * nanosecondsPerSecond / SoundBlasterSynth::sampleRate;
}

void append(std::vector<std::int16_t>& target, const std::vector<std::int16_t>& values) {
    target.insert(target.end(), values.begin(), values.end());
}

SoundBlasterSynth musicSynth() {
    std::vector<std::uint8_t> sequence(36, 0);
    sequence[0] = 0x84;
    sequence[5] = 1;
    sequence[6] = 0x04;
    sequence.insert(sequence.end(), {0xb1, 0x4e, 127, 127, 0xfc});
    SoundBlasterSynth synth({}, {}, sequence);
    synth.startMusic();
    return synth;
}

BlackoutWorkState state(const std::uint8_t seed, const BlackoutClip clipping) {
    BlackoutWorkState result{};
    for (std::size_t i = 0; i < result.palette.source.size(); ++i) {
        result.palette.source[i] = static_cast<std::uint8_t>((i * 11 + seed) % 64);
    }
    result.palette = completedFullPaletteUpload(result.palette, {0, vgaPaletteColors, 0, 17});
    assert(result.palette.source != result.palette.output);
    result.surface.clipping = clipping;
    result.surface.firstColor = 37;
    result.surface.secondColor = 18;
    result.surface.sequencer = BlackoutSequencerState{0, 15};
    return result;
}

void checkFade(const NativeBlackoutWork& work, const BlackoutWorkState& incoming) {
    assert(work.state.palette.source == incoming.palette.source);
    assert(work.state.surface.clipping == incoming.surface.clipping);
    assert(work.state.surface.firstColor == 0 && work.state.surface.secondColor == 0);
    assert(work.state.surface.sequencer && work.state.surface.sequencer->index == 2
        && work.state.surface.sequencer->mapMask == 0);
    std::uint64_t preceding = 0;
    std::size_t commits = 0;
    for (std::size_t i = 0; i < work.steps.size(); ++i) {
        const auto& step = work.steps[i];
        const auto intensity = static_cast<std::uint8_t>(63 - i * 4);
        const auto expected = completedFullPaletteUpload(incoming.palette,
            {0, vgaPaletteColors, 0, intensity});
        assert(step.palette.source == incoming.palette.source);
        assert(step.palette.output == expected.output && step.palette.dac.colors == expected.dac.colors);
        assert(step.palette.dac.mode == PaletteDacMode::write && step.palette.dac.pelIndex == 0);
        assert(step.writes.size() == 770 && step.commits.size() == vgaPaletteColors);
        assert(step.writes[0].port == 0x3c9 && step.writes[0].value == 0);
        assert(step.writes[1].port == 0x3c8 && step.writes[1].value == 0);
        assert(step.writes.back().port == 0x3c9 && step.writes.back().value == expected.output.back());
        assert(step.commits.front().index == 0 && step.commits.back().index == 255);
        assert(step.commits.front().color[0] == expected.output[0]
            && step.commits.front().color[1] == expected.output[1]
            && step.commits.front().color[2] == expected.output[2]);
        assert(step.commits.back().color[2] == expected.output.back());
        assert(step.highReads != 0 && step.elapsedNanoseconds > preceding);
        assert(step.writes[0].cycleOffset < step.writes[1].cycleOffset
            && step.writes[1].cycleOffset < step.writes.back().cycleOffset);
        preceding = step.elapsedNanoseconds;
        commits += step.commits.size();
    }
    assert(commits == blackoutPaletteSteps * vgaPaletteColors);
    assert(work.elapsedNanoseconds > preceding);
    const auto expected = completedFullPaletteUpload(incoming.palette, {0, vgaPaletteColors, 0, 3});
    assert(work.state.palette.output == expected.output && work.state.palette.dac.colors == expected.dac.colors);
}

// The two CLI boundaries are fixed by the source writes: index OUT, MOV DX,
// CLI; final component OUT, LOOP, STI/first POP interrupt shadow.
void checkManualAudio(const NativeBlackoutWork& work, const std::uint64_t origin,
    SoundBlasterSynth& reference, std::size_t& emitted) {
    const auto renderTo = [&](const std::uint64_t time, const bool masked) {
        const auto target = sampleAt(time);
        assert(target >= emitted);
        const auto count = target - emitted;
        auto samples = masked ? reference.renderInterruptMasked(count) : reference.render(count);
        emitted = target;
        return samples;
    };
    for (const auto& step : work.steps) {
        assert(step.writes.size() == 770);
        std::vector<std::int16_t> expected;
        append(expected, renderTo(origin + (step.writes[1].cycleOffset + 2) * cycleNanoseconds, false));
        append(expected, renderTo(origin + (step.writes.back().cycleOffset + 3) * cycleNanoseconds, true));
        append(expected, renderTo(origin + step.elapsedNanoseconds, false));
        assert(step.samples == expected);
    }
    assert(work.clearSamples == renderTo(origin + work.elapsedNanoseconds, false));
}

void checkPages(const IndexedPages& pages, const std::uint16_t front,
    const std::uint16_t back, const std::uint16_t cache) {
    for (std::int16_t y = 0; y < height; ++y) {
        for (std::int16_t x = 0; x < width; ++x) {
            const ProjectedPoint point{x, y};
            assert(pages.known(front, point) && pages.read(front, point) == 0);
            assert(pages.known(back, point) == (x == 319 && y == 199));
            assert(pages.known(cache, point) == (x == 0 && y == 0));
        }
    }
    assert(pages.read(back, {319, 199}) == 37 && pages.read(cache, {0, 0}) == 21);
}

void twoRetainedFades(const bool swapped) {
    auto synth = musicSynth();
    auto reference = musicSynth();
    PresentationClock clock(synth);
    const auto preroll = clock.renderSamples(prerollSamples);
    assert(preroll == reference.render(prerollSamples));
    std::size_t emitted = prerollSamples;
    auto origin = sampleTime(prerollSamples);
    CockpitPageState roles;
    if (swapped) {
        roles.flip();
    }
    roles.beginFrame();
    roles.selectCockpitCopy();
    IndexedPages pages;
    pages.write(roles.frontPage(), {0, 0}, 33);
    pages.write(roles.backPage(), {319, 199}, 37);
    pages.write(roles.cachePage(), {0, 0}, 21);
    const auto firstFront = roles.frontPage();
    const auto firstBack = roles.backPage();
    const auto firstCopy = roles.copyPage();
    const auto firstState = state(3, BlackoutClip::enabled);
    const auto first = clock.renderNativeBlackout(firstState, roles, pages);
    checkFade(first, firstState);
    checkManualAudio(first, origin, reference, emitted);
    assert(clock.elapsedSamples() == emitted && roles.frontPage() == firstFront
        && roles.backPage() == firstBack && roles.drawPage() == firstFront && roles.copyPage() == firstCopy);
    checkPages(pages, firstFront, firstBack, roles.cachePage());

    // The caller's 22B1 flip and 1A22 palette selection consume no new native
    // sample budget in this test; the next 9D4C retains the fractional epoch.
    roles.flip();
    origin += first.elapsedNanoseconds;
    const auto secondState = state(29, BlackoutClip::disabled);
    const auto second = clock.renderNativeBlackout(secondState, roles, pages);
    checkFade(second, secondState);
    checkManualAudio(second, origin, reference, emitted);
    assert(clock.elapsedSamples() == emitted && roles.frontPage() == firstBack
        && roles.backPage() == firstFront && roles.drawPage() == firstBack && roles.copyPage() == firstCopy);
    for (std::int16_t y = 0; y < height; ++y) {
        for (std::int16_t x = 0; x < width; ++x) {
            const ProjectedPoint point{x, y};
            assert(pages.known(firstFront, point) && pages.read(firstFront, point) == 0);
            assert(pages.known(firstBack, point) && pages.read(firstBack, point) == 0);
            assert(pages.known(roles.cachePage(), point) == (x == 0 && y == 0));
        }
    }
    assert(pages.read(roles.cachePage(), {0, 0}) == 21);
    assert(synth.driverState().clockSource == SoundBlasterClockSource::samples);
    assert(synth.driverState().clockPhase == reference.driverState().clockPhase);
    const auto& writes = synth.registerWrites();
    const auto& controlWrites = reference.registerWrites();
    assert(writes.size() == controlWrites.size());
    for (std::size_t i = 0; i < writes.size(); ++i) {
        assert(writes[i].registerNumber == controlWrites[i].registerNumber
            && writes[i].value == controlWrites[i].value);
    }
}

std::vector<std::uint8_t> voice(const std::size_t count) {
    std::vector<std::uint8_t> bytes(81);
    bytes.resize(81 + count, 201);
    bytes[0] = 0x84;
    bytes[2] = 2;
    bytes[36] = 0xfc;
    bytes[51] = 0xc0;
    bytes[52] = 0x2b;
    bytes[69] = static_cast<std::uint8_t>(count);
    bytes[70] = static_cast<std::uint8_t>(count >> 8);
    return bytes;
}

void releasedVoiceHold() {
    SoundBlasterSynth synth;
    PresentationClock clock(synth);
    synth.loadSampledVoice(voice(1000));
    synth.startSampledVoice();
    clock.renderSamples(1000);
    const auto running = synth.sampledVoiceObservation();
    assert(running.phase == SampledVoicePhase::playing && running.speakerEnabled
        && running.dacValue == 201);
    synth.stopSampledVoice();
    synth.releaseSampledVoice();
    const auto held = synth.sampledVoiceObservation();
    assert(held.phase == SampledVoicePhase::unloaded && held.speakerEnabled
        && held.dacValue == running.dacValue);
    CockpitPageState roles;
    IndexedPages pages;
    const auto work = clock.renderNativeBlackout(state(11, BlackoutClip::enabled), roles, pages);
    assert(!work.steps[0].samples.empty());
    assert(synth.sampledVoiceObservation().dacValue == held.dacValue
        && synth.sampledVoiceObservation().speakerEnabled == held.speakerEnabled);
}

template<class Setup>
void rejectsWithoutMutation(Setup setup) {
    SoundBlasterSynth synth;
    PresentationClock clock(synth);
    CockpitPageState roles;
    IndexedPages pages;
    pages.write(roles.frontPage(), {0, 0}, 31);
    pages.write(roles.backPage(), {319, 199}, 37);
    pages.write(roles.cachePage(), {0, 0}, 21);
    auto input = state(7, BlackoutClip::enabled);
    setup(synth, clock, roles, pages, input);
    const auto front = roles.frontPage();
    const auto back = roles.backPage();
    const auto draw = roles.drawPage();
    const auto copy = roles.copyPage();
    const auto elapsed = clock.elapsedSamples();
    const auto driver = synth.driverState();
    const auto writes = synth.registerWrites().size();
    const auto voiceBefore = synth.sampledVoiceObservation();
    bool rejected = false;
    try {
        clock.renderNativeBlackout(input, roles, pages);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && clock.elapsedSamples() == elapsed && synth.registerWrites().size() == writes);
    assert(synth.driverState().clockPhase == driver.clockPhase
        && synth.driverState().clockSource == driver.clockSource);
    assert(synth.sampledVoiceObservation().phase == voiceBefore.phase
        && synth.sampledVoiceObservation().transferred == voiceBefore.transferred);
    assert(roles.frontPage() == front && roles.backPage() == back
        && roles.drawPage() == draw && roles.copyPage() == copy);
    assert(pages.read(front, {0, 0}) == 31 && pages.read(back, {319, 199}) == 37
        && pages.read(roles.cachePage(), {0, 0}) == 21);
    assert(!pages.known(front, {1, 0}) && !pages.known(back, {0, 0}));
}

void rejectionDomains() {
    rejectsWithoutMutation([](auto&, auto&, auto&, auto&, auto& s) { s.surface.driver = static_cast<BlackoutDriver>(0); });
    rejectsWithoutMutation([](auto&, auto&, auto&, auto&, auto& s) { s.surface.scale = 1; });
    rejectsWithoutMutation([](auto&, auto&, auto&, auto&, auto& s) { s.surface.remap = 1; });
    rejectsWithoutMutation([](auto&, auto&, auto&, auto&, auto& s) { s.surface.rasterStyle = 0; });
    rejectsWithoutMutation([](auto&, auto&, auto&, auto&, auto& s) { s.surface.paletteEnabled = 0; });
    rejectsWithoutMutation([](auto&, auto&, auto&, auto&, auto& s) { s.surface.clipping = static_cast<BlackoutClip>(2); });
    rejectsWithoutMutation([](auto&, auto&, auto&, auto&, auto& s) { s.palette.source.back() = 64; });
    rejectsWithoutMutation([](auto&, auto&, auto&, auto&, auto& s) { s.palette.dac.mode = PaletteDacMode::read; });
    rejectsWithoutMutation([](auto&, auto&, auto&, auto&, auto& s) { s.palette.dac.pelIndex = 1; });
    rejectsWithoutMutation([](auto& synth, auto&, auto&, auto&, auto&) { synth.loadSampledVoice(voice(3)); });
    rejectsWithoutMutation([](auto& synth, auto&, auto&, auto&, auto&) {
        std::vector<std::uint8_t> terminated(36);
        terminated[0] = 0x84;
        terminated.push_back(0xfc);
        synth.loadCampaignMusic(terminated);
        NormalWorkTiming empty;
        empty.phase.callback = synth.callbackWorkState();
        synth.renderClocked(empty, 0);
    });
}
} // namespace

int main() {
    try {
        twoRetainedFades(false);
        twoRetainedFades(true);
        releasedVoiceHold();
        rejectionDomains();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "native 9D4C ordered fades, retained pages/audio and preflight\n";
}
