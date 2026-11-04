#include "stellar7/native_startup.hpp"
#include "stellar7/native_session.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <stdexcept>
#include <tuple>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

namespace {
using namespace stellar7;
template<class Function> void rejects(Function function) {
    bool rejected = false;
    try { function(); } catch (const std::logic_error&) { rejected = true; }
    assert(rejected);
}
std::vector<std::uint8_t> bank() {
    std::vector<std::uint8_t> result(96 * 28);
    for (std::size_t program = 0; program < 96; ++program) {
        const auto offset = program * 28;
        for (const auto op : {0U, 13U}) {
            result[offset + op + 1] = 1;
            result[offset + op + 3] = 15;
            result[offset + op + 4] = 7;
            result[offset + op + 7] = 7;
        }
        result[offset + 12] = 1;
    }
    return result;
}

GameplayAssets assets() {
    GameplayAssets game;
    game.cockpit = {std::vector<std::uint8_t>(320 * 200), std::vector<Rgb>(256)};
    game.cockpitDacPalette.resize(256 * 3);
    game.uiPalette.assign(256, {12, 20, 28});
    game.horizonFrames = {{256, 16, std::vector<std::uint8_t>(256 * 16)}};
    game.soundBlasterVolumeTable.resize(16 * 64);
    for (std::size_t i = 0; i < game.soundBlasterVolumeTable.size(); ++i) {
        game.soundBlasterVolumeTable[i] = static_cast<std::uint8_t>(i % 64);
    }
    game.artGroups.resize(6, std::vector<IndexedBitmap>(64, {4, 1, {1, 2, 3, 4}}));
    auto& ui = game.artGroups[0];
    ui[0] = {16, 20, std::vector<std::uint8_t>(320, 5)};
    ui[11] = {4, 5, std::vector<std::uint8_t>(20, 9)};
    ui[12] = {40, 40, std::vector<std::uint8_t>(1600, 6)};
    ui[14] = {24, 22, std::vector<std::uint8_t>(528, 5)};
    ui[16] = {120, 43, std::vector<std::uint8_t>(5160, 3)};
    ui[17] = {120, 33, std::vector<std::uint8_t>(3960, 8)};
    ui[18] = {120, 25, std::vector<std::uint8_t>(3000, 8)};
    ui[20] = {264, 2, std::vector<std::uint8_t>(528, 4)};
    ui[21] = ui[22] = {8, 5, std::vector<std::uint8_t>(40, 7)};
    game.titleFont = {32, 6, 0, std::vector<IndexedBitmap>(64, {8, 6, std::vector<std::uint8_t>(48, 1)})};
    game.scoreFont = {32, 8, 0, std::vector<IndexedBitmap>(96, {4, 8, std::vector<std::uint8_t>(32, 1)})};
    game.modelColorTables.resize(2);
    auto song = std::vector<std::uint8_t>(36);
    song[0] = 0x84; song[5] = 1; song[6] = 4;
    game.levelMusicSequence = song;
    // Enable the synthetic song channel through its recovered controller message.
    game.levelMusicSequence.insert(game.levelMusicSequence.end(), {0xb1, 0x4e, 1, 0, 0xc1, 0, 0, 0x91, 65, 90, 127, 0xfc});
    game.logoMusicSequence = song;
    game.logoMusicSequence[5] = 7;
    game.logoMusicSequence.insert(game.logoMusicSequence.end(), {0xc1, 0, 127, 0xfc});
    game.presentationMusicSequence = song;
    for (unsigned group = 0; group < 8; ++group) {
        game.presentationMusicSequence.insert(game.presentationMusicSequence.end(), {0xc1, 0, 127});
    }
    game.presentationMusicSequence.push_back(0xfc);
    game.soundBlasterInstrumentResource.resize(19 + 2 * 0x540 + 2);
    game.soundBlasterInstrumentResource[0] = 0x89;
    game.soundBlasterInstrumentResource[1] = 0x11;
    const auto patches = bank();
    std::copy(patches.begin(), patches.begin() + 0x540, game.soundBlasterInstrumentResource.begin() + 19);
    game.soundBlasterInstrumentResource[19 + 0x540] = 0xab;
    game.soundBlasterInstrumentResource[19 + 0x540 + 1] = 0xcd;
    std::copy(patches.begin() + 0x540, patches.end(), game.soundBlasterInstrumentResource.begin() + 19 + 0x540 + 2);
    const CockpitSettings configuration{81, {1, 0, 1, 1, 0, 255}};
    HighscoreTable scores({}, {});
    scores.setConfiguration(configuration);
    game.cockpitSettings = configuration;
    game.highscoreTable = scores;
    return game;
}

NativeStartupAssets startupAssets() {
    NativeStartupAssets result;
    result.logoScreen = {std::vector<std::uint8_t>(320*200,9),std::vector<Rgb>(256)};
    result.menuScreen = result.logoScreen;
    result.logoFrames.assign(8,{4,1,{1,0,2,3}});
    return result;
}

std::unique_ptr<NativeStartupSequence> ready(const GameplayAssets& game,const NativeStartupAssets& resources) {
    auto startup = std::make_unique<NativeStartupSequence>(game,resources);
    startup->start(NativeStartupExecution::recoveredContext);
    startup->initializeSettings(StartupCapabilityProfile::noMouseOrJoystick);
    startup->initializeResources(NativeStartupDeviceProfile::portableOplNoDspProbe);
    startup->initializePresentation(); startup->initializeFonts();
    while (startup->phase() == NativeStartupPhase::logoCountdownWork) { startup->pollLogoCountdown(10000); }
    startup->playLogoEvent(); startup->animateLogo(); startup->finishLogoSound(); startup->releaseLogo();
    startup->skipIntro(NativeIntroKey::escape); startup->beginMenuSong(); startup->initializeMenuResources();
    startup->skipCredits(NativeIntroKey::escape); startup->enterMenu(NativeMenuEntryProfile::keyboardOnly);
    return startup;
}

std::uint64_t callbackCount(const NativeStartupMenuInputRun& run) {
    std::uint64_t result = 0;
    for (const auto& packet : run.work) { result += packet.timing.callbacks.size(); }
    return result;
}

auto driver(const SoundBlasterDriverState& state) {
    return std::make_tuple(state.notes,state.velocities,state.directTimers,state.channelVoiceCursors,
        state.sequencePosition,state.sequenceWait,state.runningStatus,state.fade,state.fadeDivider,
        state.normalClockCycles,state.sequenceAllocated,state.sequenceSelected,state.sequenceActive);
}

void cannedMatchesIncremental(const GameplayAssets& game,const NativeStartupAssets& resources) {
    auto canned = ready(game,resources);
    auto split = ready(game,resources);
    const auto first = split->elapsedSamples();
    canned->beginGame(NativeMenuBeginProfile::enterPressRelease);
    assert(split->pollRecoveredMenu({false,false,true}) == NativeMenuAction::none);
    const auto press = *split->recoveredMenuInput();
    assert(press.firstSample == first && press.poll.pressed && press.poll.activeWidget == 1 && press.poll.result == 0);
    assert(split->recoveredBegin() == nullptr);
    assert(split->pollRecoveredMenu({}) == NativeMenuAction::begin);
    const auto& release = *split->recoveredMenuInput();
    assert(release.firstSample == first+press.samples.size() && release.activated && release.poll.result == 1);
    auto pcm = press.samples; pcm.insert(pcm.end(),release.samples.begin(),release.samples.end());
    assert(pcm == canned->recoveredBegin()->samples);
    assert(driver(split->recoveredBegin()->driverAfter) == driver(canned->recoveredBegin()->driverAfter));
    assert(split->frame().pixels == canned->frame().pixels);
    for (const auto role : {NativeMenuPage::front,NativeMenuPage::back,NativeMenuPage::cache}) {
        assert(split->menuPagePixels(role) == canned->menuPagePixels(role));
    }
    assert(split->difficultyParameters() == canned->difficultyParameters());
    const auto end = split->elapsedSamples();
    rejects([&] { split->pollRecoveredMenu({}); });
    rejects([&] { split->beginGame(NativeMenuBeginProfile::enterPressRelease); });
    assert(split->elapsedSamples() == end);
    auto handoff = split->releaseGameplay();
    rejects([&] { split->releaseGameplay(); });
    NativeGameplaySession adopted(game,std::move(handoff));
    assert(adopted.model().ticks() == 0 && adopted.model().level1State().rng().calls() == 16);
}

void neutralHeldAndRejection(const GameplayAssets& game,const NativeStartupAssets& resources) {
    auto startup = ready(game,resources);
    auto cursor = startup->elapsedSamples();
    const auto originalDifficulty = startup->difficultyParameters();
    const auto originalFront = startup->frame().pixels;
    std::uint64_t callbacks = 0;
    for (unsigned poll = 0; poll < 64; ++poll) {
        assert(startup->pollRecoveredMenu({}) == NativeMenuAction::none);
        const auto& run = *startup->recoveredMenuInput();
        assert(run.firstSample == cursor && !run.activated && run.poll.result == 0 && run.poll.activeWidget == 0);
        cursor += run.samples.size(); callbacks += callbackCount(run);
        assert(startup->elapsedSamples() == cursor && startup->recoveredBegin() == nullptr);
        assert(startup->difficultyParameters() == originalDifficulty && startup->frame().pixels == originalFront);
        for (const auto& packet : run.work) {
            assert(packet.timing.soundBlasterEffects.empty());
        }
    }
    assert(callbacks > 0 && startup->phase() == NativeStartupPhase::awaitingMenuDecision);
    const auto untouched = startup->elapsedSamples();
    const auto frame = startup->frame().pixels;
    rejects([&] { startup->pollRecoveredMenu({true,false,false}); });
    rejects([&] { startup->pollRecoveredMenu({false,true,true}); });
    rejects([&] { startup->pollRecoveredMenu({},0); });
    rejects([&] { startup->pollRecoveredMenu({},10001); });
    rejects([&] { startup->beginGame(NativeMenuBeginProfile::enterPressRelease); });
    assert(startup->elapsedSamples() == untouched && startup->frame().pixels == frame);
    assert(startup->pollRecoveredMenu({false,false,true}) == NativeMenuAction::none);
    auto pressed = *startup->recoveredMenuInput();
    assert(pressed.poll.activeWidget == 1 && pressed.poll.activation == 1);
    for (unsigned poll = 0; poll < 4; ++poll) {
        assert(startup->pollRecoveredMenu({false,false,true}) == NativeMenuAction::none);
        const auto& held = *startup->recoveredMenuInput();
        assert(held.poll.pressed && held.poll.result == 0 && held.poll.activeWidget == 1 && held.poll.activation == 1);
        assert(held.poll.front == pressed.poll.front && held.poll.back == pressed.poll.back && held.poll.transfer == pressed.poll.transfer);
        assert(startup->recoveredBegin() == nullptr);
    }
    //Host cancellation between complete operations advances no source work.
    const auto cancelledAt = startup->elapsedSamples();
    assert(startup->recoveredMenuInput() && startup->phase() == NativeStartupPhase::awaitingMenuDecision);
    assert(startup->elapsedSamples() == cancelledAt && startup->recoveredBegin() == nullptr);
    assert(startup->pollRecoveredMenu({}) == NativeMenuAction::begin);
    const auto& result = *startup->recoveredMenuInput();
    assert(result.activated && result.poll.result == 1 && !result.poll.pressed && result.poll.activeWidget == 0);
    assert(startup->phase() == NativeStartupPhase::awaitingGameplayLoader);
}

void clockGuards(const GameplayAssets& game,const NativeStartupAssets& resources) {
    auto startup = ready(game,resources);
    const auto phase = startup->recoveredMenuEntry()->finalPhase;
    const auto decision = NormalWorkOperation{NormalWorkKind::startupMenuInputDecision,1,0};
    rejects([&] { normalWorkTiming(phase,{{NormalWorkKind::startupMenuInputDecision,1,1}},120000); });
    rejects([&] { normalWorkTiming(phase,{{NormalWorkKind::startupMenuInputDecision,2,0}},120000); });
    auto expired = phase; expired.timer->globalCount = 0;
    rejects([&] { normalWorkTiming(expired,{decision},120000); });
    auto mixed = normalWorkTiming(phase,{{NormalWorkKind::startupMenuDecision,1,0}},120000).phase;
    rejects([&] { normalWorkTiming(mixed,{decision},120000); });
    mixed = normalWorkTiming(phase,{decision},120000).phase;
    rejects([&] { normalWorkTiming(mixed,{{NormalWorkKind::startupMenuDecision,1,0}},120000); });
    assert(mixed.sequenceSelection->beginDecisions == 0 && mixed.sequenceSelection->recoveredMenuInput);
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    const auto game = assets(); const auto resources = startupAssets();
    cannedMatchesIncremental(game,resources);
    neutralHeldAndRejection(game,resources);
    clockGuards(game,resources);
}
