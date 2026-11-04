#include "stellar7/native_session.hpp"
#include "stellar7/level1_trace.hpp"

#include <array>
#include <cassert>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace stellar7;

GameplayAssets assets() {
    GameplayAssets result;
    result.cockpit = {std::vector<std::uint8_t>(320 * 200, 0), std::vector<Rgb>(256)};
    result.cockpitDacPalette.resize(256 * 3);
    result.horizonFrames = {{256, 16, std::vector<std::uint8_t>(256 * 16, 7)}};
    result.artGroups.resize(6, std::vector<IndexedBitmap>(64, {1, 1, {5}}));
    constexpr std::size_t header = 19;
    constexpr std::size_t half = 0x540;
    auto& bank = result.soundBlasterInstrumentResource;
    bank.resize(header + half * 2 + 2);
    bank[0] = 0x89;
    bank[1] = 0x11;
    bank[header + half] = 0xab;
    bank[header + half + 1] = 0xcd;
    for (const auto offset : {header, header + 13}) {
        bank[offset + 1] = 1;
        bank[offset + 3] = 15;
        bank[offset + 4] = 7;
        bank[offset + 7] = 7;
    }
    auto& music = result.levelMusicSequence;
    music.resize(36);
    music[0] = 0x84;
    music[5] = 1;
    music[6] = 0x04;
    music.insert(music.end(), {0xc1, 0, 0, 0x91, 60, 64, 3, 0xfc});
    return result;
}

bool sameWrites(const SoundBlasterSynth& left, const SoundBlasterSynth& right) {
    const auto& a = left.registerWrites();
    const auto& b = right.registerWrites();
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t index = 0; index < a.size(); ++index) {
        if (a[index].registerNumber != b[index].registerNumber || a[index].value != b[index].value) {
            return false;
        }
    }
    return true;
}

void assertCompletedLevel1Tail() {
    auto model = std::make_unique<GameplayModel>();
    const auto rejectCompletion = [&]() {
        const auto before = level1StateTraceJson(model->level1State(), "completion-control");
        const auto ticks = model->ticks();
        bool rejected = false;
        try {
            model->finishLevel1Frame();
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected && model->ticks() == ticks);
        assert(level1StateTraceJson(model->level1State(), "completion-control") == before);
    };
    rejectCompletion();
    model->tickRetainedLevel1({});
    assert(model->level1State().loopTick() == 1 && model->level1State().rng().calls() == 42);
    model->finishLevel1Frame();
    assert(model->ticks() == 1 && model->level1State().loopTick() == 2);
    assert(model->level1State().rng().calls() == 42);
    assert(model->level1State().entities()[1].stateBytes4dTo52[3] == 2);
    rejectCompletion();
    model->tickRetainedLevel1({});
    assert(model->level1State().loopTick() == 2 && model->level1State().rng().calls() == 53);
    assert(model->level1State().entities()[1].stateBytes4dTo52[3] == 1);
    model->finishLevel1Frame();
    assert(model->ticks() == 2 && model->level1State().loopTick() == 3);
    assert(model->level1State().rng().calls() == 53);
    assert(model->level1State().entities()[1].stateBytes4dTo52[3] == 0);
    rejectCompletion();
    model = std::make_unique<GameplayModel>();
    GameplayInput fire;
    fire.fire = true;
    model->tickRetainedLevel1(fire);
    rejectCompletion();
    model->tickRetainedLevel1({});
    assert(model->level1State().playerWeaponCooldown() != 0);
    rejectCompletion();
    model->startLevel2();
    rejectCompletion();
}

void assertLevel1GroundOwnership() {
    struct Renderer final : Level1FrameRenderer {
        std::vector<std::uint64_t> before, after;
        std::vector<std::uint8_t> animation;
        void render(Level1RenderState state) override {
            before.push_back(state.rng.calls());
            animation.push_back(state.entities.at(1).stateBytes4dTo52[3]);
            state.ground.advance(state.player.yawVelocity,state.player.forwardVelocity,state.rng);
            after.push_back(state.rng.calls());
        }
    } renderer;
    auto actual = std::make_unique<GameplayModel>(GameplayModel::fromNativeStartup());
    auto reference = std::make_unique<GameplayModel>(GameplayModel::fromNativeStartup());
    assert(actual->lastCockpitInitialization().displaySteps == 2);
    assert(actual->lastCockpitInitialization().states[2].x == actual->level2CockpitIndicatorX());
    assert(actual->level1State().rng().calls() == 16);
    assert(actual->level1State().loopTick() == 0);
    assert(actual->level1State().activeListOrder().empty());
    assert(actual->level1State().entities()[1].stateBytes4dTo52[3] == 0);
    for (std::size_t frame = 0; frame < 2; ++frame) {
        actual->tickRetainedLevel1({},renderer);
        reference->tickRetainedLevel1({});
        assert(level1StateTraceJson(actual->level1State(),"ground-owner")
            == level1StateTraceJson(reference->level1State(),"ground-owner"));
        actual->finishLevel1Frame();
        reference->finishLevel1Frame();
        assert(level1StateTraceJson(actual->level1State(),"ground-owner")
            == level1StateTraceJson(reference->level1State(),"ground-owner"));
    }
    assert(renderer.before == std::vector<std::uint64_t>({16,32}));
    assert(renderer.after == std::vector<std::uint64_t>({28,42}));
    assert(renderer.animation == std::vector<std::uint8_t>({0,4}));
}

void assertResidentSuccessPalette(const GameplayAssets& data) {
    SoundBlasterSynth synth(data.soundSequences, data.soundBlasterInstrumentResource,
        data.musicForLevel(0), data.presentationMusicSequence);
    PresentationClock clock(synth);
    auto state = initialSuccessPalette(SuccessPaletteChannel::blue);
    //Incoming high is admitted immediately; the routine does not seek a new edge.
    constexpr std::uint64_t highSample = 578;
    clock.renderSamples(highSample);
    const auto initial = clock.renderSuccessPalette(state, SuccessPaletteExecution::initial);
    assert(initial.highReads == 1);
    assert(initial.writes.size() == 146 && initial.commits.size() == 48);
    assert(initial.writes[0].port == 0x3c9 && initial.writes[0].value == 0);
    assert(initial.writes[1].port == 0x3c8 && initial.writes[1].value == 16);
    assert((initial.commits.front().color == std::array<std::uint8_t, 3>{0, 0, 63}));
    assert((initial.commits.back().color == std::array<std::uint8_t, 3>{0, 0, 16}));
    auto totalTime = highSample * 1'000'000'000ULL / SoundBlasterSynth::sampleRate
        + initial.elapsedNanoseconds;
    auto previous = clock.elapsedSamples();
    for (unsigned iteration = 0; iteration < 94; ++iteration) {
        const auto work = clock.renderSuccessPalette(state, SuccessPaletteExecution::advance);
        state = work.state;
        assert(clock.elapsedSamples() == previous + work.samples.size());
        assert(work.writes.size() == 146 && work.commits.size() == 48);
        if (iteration == 0) {
            assert(state.values[0] == 62 && state.values[1] == 63 && state.values[47] == 17);
            assert(work.highReads > 1);
        }
        totalTime += work.elapsedNanoseconds;
        const auto expectedSamples = (totalTime * SoundBlasterSynth::sampleRate + 999'999'999) / 1'000'000'000;
        assert(clock.elapsedSamples() == expectedSamples);
        previous = clock.elapsedSamples();
    }
    assert(state.values == initial.state.values);
    auto invalid = state;
    invalid.values[0] = 64;
    bool rejected = false;
    try {
        clock.renderSuccessPalette(invalid, SuccessPaletteExecution::advance);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && clock.elapsedSamples() == previous);
    assert(clock.renderFrame().size() == gameplayAudioSamplesPerFrame);
    assert(synth.driverState().clockSource == SoundBlasterClockSource::samples);
}
}

int main() {
    assertLevel1GroundOwnership();
    assertCompletedLevel1Tail();
    static_assert(!std::is_move_constructible_v<NativeGameplaySession>);
    static_assert(!std::is_copy_constructible_v<NativeGameplaySession>);
    const auto data = assets();
    assertResidentSuccessPalette(data);
    NativeGameplaySession session(data, GameplayModel{});
    bool rejected = false;
    try {
        session.advance({});
    } catch (const std::logic_error&) {
        rejected = true;
    }
    assert(rejected);
    GameplayModel reference;
    SoundBlasterSynth synth(data.soundSequences, data.soundBlasterInstrumentResource, data.musicForLevel(0),
        data.presentationMusicSequence, data.musicForLevel(1), data.musicForLevel(2), data.logoMusicSequence);
    synth.startLevel1();
    PresentationClock clock(synth);
    const auto initial = session.initialize();
    assert(initial.firstSample == 0 && initial.entry.size() == levelEntryFadeSteps);
    const auto beforeRecoveredFailure = session.elapsedSamples();
    rejected = false;
    try {
        session.advanceRecoveredFailure(FrameCopyProfile::modeXNoop);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && session.elapsedSamples() == beforeRecoveredFailure && !session.recoveredFailureFrame());
    const auto schedule = clock.entrySchedule();
    for (std::size_t index = 0; index < schedule.size(); ++index) {
        assert(initial.entry[index].samples == clock.renderEntryStep(schedule[index]));
    }
    rejected = false;
    try {
        session.initialize();
    } catch (const std::logic_error&) {
        rejected = true;
    }
    assert(rejected);
    std::size_t cursor = 0;
    const std::array<GameplayInput, 6> inputs{{{}, {false, false, true}, {false, false, true, false, true},
        {true, false, true}, {}, {false, true}}};
    for (const auto& input : inputs) {
        const auto start = clock.elapsedSamples();
        reference.setSuccessAudioActive(synth.successActive());
        reference.tick(input);
        const auto before = cursor;
        const auto& events = reference.level1State().audioEvents();
        while (cursor < events.size()) {
            synth.triggerLevel1Event(events[cursor++].eventId);
        }
        const auto delivery = session.advance(input);
        assert(delivery.firstSample == start && delivery.entry.empty());
        assert(delivery.samples == clock.renderFrame());
        assert(delivery.events.size() == cursor - before);
        for (std::size_t index = 0; index < delivery.events.size(); ++index) {
            assert(delivery.events[index].eventId == events[before + index].eventId);
        }
        assert(session.elapsedSamples() == clock.elapsedSamples());
        assert(level1StateTraceJson(session.model().level1State(), "same") ==
            level1StateTraceJson(reference.level1State(), "same"));
        const auto frame = renderGameplayIndexed(data, reference);
        assert(session.frame().pixels == frame.pixels && session.frame().palette == frame.palette);
        assert(sameWrites(session.synth(), synth));
        assert(session.synth().driverState().clockSource == SoundBlasterClockSource::samples);
    }
}
