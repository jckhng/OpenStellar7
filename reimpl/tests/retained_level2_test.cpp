#include "stellar7/retained_level2.hpp"

#include <array>
#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <vector>

// Synthetic admission/exception fixture; no complete source frame is rendered.
static_assert(!std::is_copy_constructible_v<stellar7::RetainedLevel2Renderer>);
static_assert(!std::is_copy_assignable_v<stellar7::RetainedLevel2Renderer>);
static_assert(!std::is_move_constructible_v<stellar7::RetainedLevel2Renderer>);
static_assert(!std::is_move_assignable_v<stellar7::RetainedLevel2Renderer>);

namespace {
using namespace stellar7;
constexpr std::int64_t horizonCycles = 120000;
constexpr std::uint16_t keyboardRingBegin = 0x1e;

std::vector<std::uint8_t> instrumentBank() {
    constexpr std::size_t headerSize = 19;
    constexpr std::size_t halfSize = 0x540;
    std::vector<std::uint8_t> bank(headerSize + halfSize * 2 + 2, 0);
    bank[0] = 0x89;
    bank[1] = 0x11;
    bank[headerSize + halfSize] = 0xab;
    bank[headerSize + halfSize + 1] = 0xcd;
    for (const std::size_t patch : {headerSize, headerSize + 13}) {
        bank[patch + 1] = 1;
        bank[patch + 3] = 15;
        bank[patch + 4] = 7;
        bank[patch + 7] = 7;
    }
    return bank;
}

std::vector<std::uint8_t> music() {
    constexpr std::size_t headerSize = 36;
    std::vector<std::uint8_t> result(headerSize, 0);
    result[0] = 0x84;
    result[5] = 1;
    result[6] = 0x24;
    const std::vector<std::uint8_t> notes{0xc1, 0, 0, 0x91, 60, 64, 1, 0x81, 60, 64, 0xfc};
    result.insert(result.end(), notes.begin(), notes.end());
    return result;
}

SoundBlasterClockPhase initialPhase() {
    SoundBlasterClockPhase phase{};
    phase.remainingCycles = 5000;
    phase.interrupts = AudioClockInterrupts::enabled;
    phase.isa8DelayNanoseconds = 1020;
    phase.dmaDacRate = 44100;
    phase.pitPeriodMilliseconds = 55;
    phase.irqDelayMilliseconds = 0.002;
    phase.singleLinePeriodMilliseconds = 0.03;
    phase.horizontalPeriodMilliseconds = 0.03;
    phase.linesDone = 400;
    phase.linesTotal = 400;
    phase.minimumLatchDelayMilliseconds = 0.001;
    phase.frameStart = AudioClockFrameStart::activeUnlockedVgaSurface;
    phase.interruptDelivery = AudioClockInterruptDelivery::residentTimerIrq0;
    phase.vgaInterrupt = AudioClockVgaInterrupt::fixedVgaNoDemand;
    phase.devices = AudioClockDevicePhase{};
    phase.devices->fixedPitAndMaskedDma = true;
    phase.devices->memoryMask = 0x1fffff;
    phase.devices->framePeriodMilliseconds = 14;
    phase.devices->displayEndMilliseconds = 12;
    phase.devices->retraceStartMilliseconds = 13;
    phase.devices->retraceEndMilliseconds = 13.1;
    phase.devices->frame = AudioClockVgaFramePhase{1000.0 / 14, 0, 0};
    phase.timer = AudioClockTimerPhase{1, 0, 5, 20, 20, 5};
    phase.bios = AudioClockBiosPhase{};
    phase.bios->domain = AudioClockBiosDomain::normalVgaDefaultTimer;
    phase.events = {{14, AudioClockEventKind::frameStart, 0}};
    return phase;
}

class Bootstrap final : public Level2FrameRenderer {
public:
    Bootstrap(RetainedFrameExecutor& executor, FrameController& controller, CockpitPageState& pages)
        : executor_(executor), controller_(controller), pages_(pages) {}

    void render(Level2RenderState frame) override {
        // A semantic producer only; this fixture certifies no source render body.
        frame.ground.advance(frame.player.yawVelocity, frame.player.forwardVelocity, frame.rng);
    }

    std::optional<FrameTailState> finish(const FrameTailState& state, const FrameTailGuards& guards) override {
        assert(!guards.firstFrame);
        controller_.initialize({FrameControllerOrigin::sourceColdDefaults, state.loopTick,
            keyboardRingBegin, keyboardRingBegin, {}, 0, false, false, false, false, false, {}});
        executor_.display(pages_);
        executor_.postcopy(FrameCopyProfile::modeXNoop);
        pages_.flip();
        const auto tail = executor_.finish(state, controller_.augmentGuards(guards));
        controller_.consumeEmptyTail(state.loopTick);
        return tail.state.state;
    }

private:
    RetainedFrameExecutor& executor_;
    FrameController& controller_;
    CockpitPageState& pages_;
};

struct Fixture {
    GameplayModel model;
    GameplayAssets assets;
    SoundBlasterSynth synth{{}, instrumentBank(), music()};
    PresentationClock clock{synth};
    RetainedFrameExecutor executor{clock, initialPhase(), horizonCycles};
    FrameController controller;
    CockpitPageState pages;
    RadarState radar{std::vector<IndexedBitmap>{{120, 56, std::vector<std::uint8_t>(120 * 56, 7)}}};
    IndexedPages pixels;
    HorizonWorkState horizon{};
    CockpitSettings settings{};
    std::uint8_t rasterStyle = 1;
    std::optional<std::uint8_t> enemyFlag;

    Fixture() {
        synth.startMusic();
        model.startLevel2();
        model.tick({});
        Bootstrap bootstrap(executor, controller, pages);
        model.tickWithRenderer({}, bootstrap);
        assert(model.level2LoopTick() == controller.expectedLoopTick());
    }

    RetainedLevel2Owners owners() {
        return {model, assets, executor, controller, pages, radar, pixels, horizon, settings,
            rasterStyle, enemyFlag};
    }
};

struct Snapshot {
    std::uint64_t ticks;
    std::uint16_t loopTick;
    std::uint16_t controllerTick;
    std::uint64_t samples;
    SoundBlasterClockPhase phase;
    std::uint16_t frontPage;
    std::uint16_t backPage;
    std::uint16_t drawPage;

    explicit Snapshot(const Fixture& fixture)
        : ticks(fixture.model.ticks()), loopTick(fixture.model.level2LoopTick()),
          controllerTick(fixture.controller.expectedLoopTick()), samples(fixture.clock.elapsedSamples()),
          phase(fixture.executor.phase()), frontPage(fixture.pages.frontPage()),
          backPage(fixture.pages.backPage()), drawPage(fixture.pages.drawPage()) {}

    void unchanged(const Fixture& fixture) const {
        assert(ticks == fixture.model.ticks() && loopTick == fixture.model.level2LoopTick());
        assert(controllerTick == fixture.controller.expectedLoopTick());
        assert(samples == fixture.clock.elapsedSamples());
        const auto& returned = fixture.executor.phase();
        assert(phase.remainingCycles == returned.remainingCycles && phase.deferredCycles == returned.deferredCycles);
        assert(phase.linesDone == returned.linesDone && phase.interrupts == returned.interrupts);
        assert(phase.devices->picTicks == returned.devices->picTicks);
        assert(phase.devices->displayStart == returned.devices->displayStart && phase.devices->realStart == returned.devices->realStart);
        assert(phase.events.size() == returned.events.size());
        for (std::size_t i = 0; i < phase.events.size(); ++i) {
            assert(phase.events[i].kind == returned.events[i].kind && phase.events[i].value == returned.events[i].value);
            assert(phase.events[i].deadlineMilliseconds == returned.events[i].deadlineMilliseconds);
        }
        assert(frontPage == fixture.pages.frontPage() && backPage == fixture.pages.backPage());
        assert(drawPage == fixture.pages.drawPage());
    }
};

template<class Function>
void rejectWithoutAdvance(Fixture& fixture, Function function) {
    const Snapshot before(fixture);
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    before.unchanged(fixture);
}

struct ObserverStop {};
enum class Observation { rejectReentry, throwAfterPrefix };

class Observer final : public RetainedLevel2Observer {
public:
    Observer(Fixture& fixture, const Observation observation) : fixture_(fixture), observation_(observation) {}
    RetainedLevel2Renderer* renderer = nullptr;
    unsigned entries = 0;
    unsigned prefixes = 0;

    void prefixEntry(const StationaryLevel2PrefixInput&) override {
        ++entries;
        if (observation_ == Observation::rejectReentry) {
            assert(renderer != nullptr);
            rejectWithoutAdvance(fixture_, [&]() { renderer->advance({}); });
            throw ObserverStop{};
        }
    }

    void prefix(const RetainedPrefixExecution&) override {
        ++prefixes;
        throw ObserverStop{};
    }

private:
    Fixture& fixture_;
    Observation observation_;
};

void stopFromObserver(RetainedLevel2Renderer& renderer) {
    bool stopped = false;
    try {
        renderer.advance({});
    } catch (const ObserverStop&) {
        stopped = true;
    }
    assert(stopped);
}
}

int main() {
    using namespace stellar7;
    Fixture fixture;
    Observer observer(fixture, Observation::throwAfterPrefix);
    RetainedLevel2Renderer renderer(fixture.owners(), FrameCopyProfile::modeXNoop, observer);
    observer.renderer = &renderer;

    constexpr std::array<bool GameplayInput::*, 7> inputFields{
        &GameplayInput::left, &GameplayInput::right, &GameplayInput::forward, &GameplayInput::reverse,
        &GameplayInput::fire, &GameplayInput::exit, &GameplayInput::confirm};
    for (const auto field : inputFields) {
        GameplayInput input{};
        input.*field = true;
        rejectWithoutAdvance(fixture, [&]() { renderer.advance(input); });
    }
    assert(observer.entries == 0 && observer.prefixes == 0);

    Level2FrameRenderer& base = renderer;
    rejectWithoutAdvance(fixture, [&]() { base.prepare({}); });
    rejectWithoutAdvance(fixture, [&]() { base.finish({}, {}); });
    Level1GroundState ground;
    StellarRng rng;
    Level1Entity player{};
    std::vector<Level1Entity> actors;
    std::vector<std::size_t> order;
    bool alternate = false;
    Level2SightExecution sight = Level2SightExecution::pending;
    std::int16_t indicator = 0, velocity = 0;
    Level2CockpitExecution cockpit = Level2CockpitExecution::pending;
    rejectWithoutAdvance(fixture, [&]() {
        base.render({ground, rng, player, actors, order, alternate, sight, indicator, velocity, cockpit});
    });

    const Snapshot beforeObserver(fixture);
    stopFromObserver(renderer);
    assert(observer.entries == 1 && observer.prefixes == 1);
    assert(fixture.model.ticks() == beforeObserver.ticks + 1);
    assert(fixture.clock.elapsedSamples() > beforeObserver.samples);
    assert(fixture.controller.expectedLoopTick() == beforeObserver.controllerTick);
    assert(fixture.pages.frontPage() == beforeObserver.frontPage);
    // The prefix clock already advanced; outer admission failure is not rollback.
    rejectWithoutAdvance(fixture, [&]() { renderer.advance({}); });
    assert(observer.entries == 1 && observer.prefixes == 1);

    Fixture reentryFixture;
    Observer reentryObserver(reentryFixture, Observation::rejectReentry);
    RetainedLevel2Renderer reentryRenderer(reentryFixture.owners(), FrameCopyProfile::modeXNoop, reentryObserver);
    reentryObserver.renderer = &reentryRenderer;
    const Snapshot beforeReentry(reentryFixture);
    stopFromObserver(reentryRenderer);
    assert(reentryObserver.entries == 1 && reentryObserver.prefixes == 0);
    assert(reentryFixture.model.ticks() == beforeReentry.ticks + 1);
    assert(reentryFixture.clock.elapsedSamples() == beforeReentry.samples);
    rejectWithoutAdvance(reentryFixture, [&]() { reentryRenderer.advance({}); });
}
