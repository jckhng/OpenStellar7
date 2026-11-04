#include "stellar7/blackout_work.hpp"
#include "stellar7/cockpit_state.hpp"
#include "stellar7/presentation_clock.hpp"

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
    p.linesTotal = p.linesDone = 400;
    p.minimumLatchDelayMilliseconds = 0.001;
    p.frameStart = AudioClockFrameStart::activeUnlockedVgaSurface;
    p.interruptDelivery = AudioClockInterruptDelivery::residentTimerIrq0;
    p.vgaInterrupt = AudioClockVgaInterrupt::fixedVgaNoDemand;
    p.devices = AudioClockDevicePhase{};
    p.devices->fixedPitAndMaskedDma = true;
    p.devices->framePeriodMilliseconds = 14;
    p.devices->displayEndMilliseconds = 12;
    p.devices->retraceStartMilliseconds = 0.001;
    p.devices->retraceEndMilliseconds = 10;
    p.devices->frame = AudioClockVgaFramePhase{1000.0 / 14, 0, 0};
    p.events = {{14, AudioClockEventKind::frameStart, 0}};
    p.timer = AudioClockTimerPhase{1, 0, 5, 20, 20, 5};
    return p;
}

stellar7::BlackoutWorkState state() {
    stellar7::BlackoutWorkState s{};
    for (std::size_t i = 0; i < s.palette.source.size(); ++i) {
        s.palette.source[i] = static_cast<std::uint8_t>((i * 7 + 1) % 64);
    }
    s.palette.output.fill(41);
    s.palette.dac.colors.fill(42);
    s.palette.dac.mode = stellar7::PaletteDacMode::read;
    s.surface.firstColor = 55;
    s.surface.secondColor = 12;
    s.surface.sequencer = stellar7::BlackoutSequencerState{0, 15};
    return s;
}

stellar7::SoundBlasterSynth liveSynth() {
    std::vector<std::uint8_t> sequence(36, 0);
    sequence[0] = 0x84;
    sequence[5] = 1;
    sequence[6] = 0x04;
    sequence.insert(sequence.end(), {0xb1, 0x4e, 127, 127, 0xfc});
    stellar7::SoundBlasterSynth synth({}, {}, sequence);
    synth.startMusic();
    synth.render(1);
    return synth;
}
}

int main() {
    using namespace stellar7;
    const auto clear = fullFrontClearOperations();
    assert(clear.front().kind == NormalWorkKind::instructions && clear.front().count == 63);
    assert(clear.back().kind == NormalWorkKind::instructions && clear.back().count == 5);
    std::uint64_t plain = 0, strings = 0, ports = 0, repeats = 0;
    for (const auto& op : clear) {
        if (op.kind == NormalWorkKind::instructions) { plain += op.count; }
        if (op.kind == NormalWorkKind::string) { strings += op.count; }
        if (op.kind == NormalWorkKind::port8Write || op.kind == NormalWorkKind::port16Write) { ports += op.count; }
        if (op.kind == NormalWorkKind::repeatString) { assert(op.count == 39); repeats += op.count; }
    }
    assert(plain == 9068 && strings == 600 && repeats == 7800 && ports == 600);
    const auto clearClock = normalWorkTiming(phase(), clear, 120000);
    assert(clearClock.stringIterations == 8400 && clearClock.total.delayCycles == 1600);
    assert(clearClock.total.dispatches >= 12268 && clearClock.total.dispatches < 18068);
    const auto original = state();
    const auto plan = blackoutClockTiming(phase(), original, 0xa000, 120000);
    assert(plan.steps[0].before.total.dispatches == 33);
    for (std::size_t i = 0; i < blackoutPaletteSteps; ++i) {
        const auto& step = plan.steps[i];
        assert(step.intensity == 63 - i * 4);
        assert(step.palette.state.source == original.palette.source);
        assert(step.palette.state.output == transformPalette(original.palette, {0, 256, 0, step.intensity}).output);
        assert(step.palette.state.dac.colors == step.palette.state.output);
        assert(step.palette.state.dac.readIndex == 255 && step.palette.state.dac.writeIndex == 0);
        assert(step.palette.commits.size() == 256);
        assert(step.palette.writes.size() == (i == 0 ? 769 : 770));
        if (i != 0) { assert(step.before.total.dispatches == 24); }
    }
    assert(plan.state.palette.output == transformPalette(original.palette, {0, 256, 0, 3}).output);
    assert(plan.state.surface.clipping == original.surface.clipping);
    assert(plan.state.surface.firstColor == 0 && plan.state.surface.secondColor == 0);
    assert(plan.state.surface.sequencer->index == 2 && plan.state.surface.sequencer->mapMask == 0);
    struct DisplayControl { BlackoutPageLayout layout; bool swapped; };
    constexpr std::array<DisplayControl,4> displayControls{{
        {BlackoutPageLayout::compatibility,false},{BlackoutPageLayout::compatibility,true},
        {BlackoutPageLayout::recoveredStartup,false},{BlackoutPageLayout::recoveredStartup,true}}};
    std::vector<std::int16_t> referenceAudio;
    std::int64_t referenceClearCycles = 0;
    for (const auto control : displayControls) {
        const auto layout = control.layout;
        auto synth = liveSynth();
        PresentationClock clock(synth);
        auto roles = layout == BlackoutPageLayout::compatibility
            ? CockpitPageState{} : CockpitPageState::fromRecoveredMenu();
        if (control.swapped) { roles.flip(); }
        roles.beginFrame();
        roles.selectCockpitCopy();
        IndexedPages pages;
        pages.write(roles.backPage(), {319, 199}, 37);
        pages.write(roles.cachePage(), {0, 0}, 21);
        pages.write(roles.frontPage(), {0, 0}, 33);
        const auto back = roles.backPage(), front = roles.frontPage(), copy = roles.copyPage();
        const auto result = clock.renderBlackout(phase(), original, roles, pages, 120000,layout);
        assert(roles.drawPage() == front && roles.frontPage() == front && roles.backPage() == back && roles.copyPage() == copy);
        for (std::int16_t y = 0; y < 200; ++y) {
            for (std::int16_t x = 0; x < 320; ++x) {
                assert(pages.known(front, {x, y}) && pages.read(front, {x, y}) == 0);
                assert(pages.known(back, {x, y}) == (x == 319 && y == 199));
                assert(pages.known(roles.cachePage(), {x, y}) == (x == 0 && y == 0));
            }
        }
        assert(pages.read(back, {319, 199}) == 37 && pages.read(roles.cachePage(), {0, 0}) == 21);
        assert(result.state.palette.output == plan.state.palette.output);
        assert(synth.driverState().clockSource == SoundBlasterClockSource::normalWork);
        std::vector<std::int16_t> samples;
        for (const auto& step : result.steps) {
            samples.insert(samples.end(),step.before.audio.samples.begin(),step.before.audio.samples.end());
            samples.insert(samples.end(),step.palette.work.audio.samples.begin(),step.palette.work.audio.samples.end());
        }
        samples.insert(samples.end(),result.clear.audio.samples.begin(),result.clear.audio.samples.end());
        if (referenceAudio.empty()) {
            referenceAudio = samples;
            referenceClearCycles = result.clear.timing.total.elapsedCycles;
        }
        assert(samples == referenceAudio && clock.elapsedSamples() == samples.size());
        assert(result.clear.timing.total.elapsedCycles == referenceClearCycles);
    }
    const auto rejects = [](const BlackoutWorkState& s, const std::uint16_t front, const std::int64_t horizon) {
        try { blackoutClockTiming(phase(), s, front, horizon); }
        catch (const std::invalid_argument&) { return true; }
        return false;
    };
    auto bad = original;
    bad.surface.paletteEnabled = 0;
    assert(rejects(bad, 0xa000, 120000));
    bad = original; bad.surface.scale = 1; assert(rejects(bad, 0xa000, 120000));
    bad = original; bad.surface.remap = 1; assert(rejects(bad, 0xa000, 120000));
    bad = original; bad.surface.rasterStyle = 0; assert(rejects(bad, 0xa000, 120000));
    bad = original; bad.surface.driver = static_cast<BlackoutDriver>(0); assert(rejects(bad, 0xa000, 120000));
    bad = original; bad.surface.clipping = static_cast<BlackoutClip>(2); assert(rejects(bad, 0xa000, 120000));
    assert(rejects(original, 0xa800, 120000) && rejects(original, 0xa000, 1000));
    for (const auto layout : {BlackoutPageLayout::recoveredStartup,static_cast<BlackoutPageLayout>(2)}) {
        bool invalid = false;
        try { blackoutSourcePlan(original.surface,0xa400,layout); }
        catch (const std::invalid_argument&) { invalid = true; }
        assert(invalid);
    }
    auto rejected = liveSynth();
    PresentationClock rejectedClock(rejected);
    CockpitPageState rejectedRoles;
    IndexedPages rejectedPages;
    rejectedPages.write(rejectedRoles.frontPage(), {0, 0}, 7);
    auto lateUnsupported = phase();
    lateUnsupported.events.push_back({100, AudioClockEventKind::unsupported, 0});
    bool refused = false;
    try { rejectedClock.renderBlackout(lateUnsupported, original, rejectedRoles, rejectedPages, 120000); }
    catch (const std::invalid_argument&) { refused = true; }
    assert(refused && rejectedClock.elapsedSamples() == 0 && rejected.driverState().normalClockCycles == 0);
    assert(rejectedRoles.drawPage() == rejectedRoles.backPage() && rejectedPages.read(rejectedRoles.frontPage(), {0, 0}) == 7);
}
