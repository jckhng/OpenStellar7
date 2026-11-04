#include "stellar7/native_session.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace stellar7 {
struct GameplayModelTestAccess {
    static void endingReturn(GameplayModel& model) {
        model.phase_ = GameplayPhase::endingReturn;
        model.activeCampaignLevel_ = 7;
        model.campaignSuccess_ = true;
        model.presentationFrame_ = endingScenePageCount + 1;
        model.level2Score_ = 1250;
        model.level2PlayerArmor_ = 136;
        model.level2LoopTick_ = 976;
        model.cockpitIndicatorX_ = 139;
        model.cockpitIndicatorVelocity_ = -4;
        model.level2Rng_ = StellarRng({}, 1, 2, 123, 77);
    }
};

struct NativeSessionTestAccess {
    static void stage(NativeGameplaySession& session, const GameplayAssets& assets,
        const bool swapped) {
        GameplayModelTestAccess::endingReturn(session.model_);
        session.initialized_ = true;
        if (swapped) {
            session.pages_.flip();
        }
        session.pages_.selectFront();
        session.pixels_.emplace();
        for (std::int16_t y = 0; y < 200; ++y) {
            for (std::int16_t x = 0; x < 320; ++x) {
                const ProjectedPoint point{x, y};
                session.pixels_->write(session.pages_.frontPage(), point, 0);
                session.pixels_->write(session.pages_.backPage(), point, 0);
                session.pixels_->write(session.pages_.cachePage(), point,
                    static_cast<std::uint8_t>((x + 3 * y + 17) % 251));
            }
        }
        session.radar_.emplace(std::vector<IndexedBitmap>{{120, 56,
            std::vector<std::uint8_t>(120 * 56)}});
        session.frame_ = {std::vector<std::uint8_t>(320 * 200), std::vector<Rgb>(256)};
        PaletteWorkState palette{};
        std::copy(assets.cockpitDacPalette.begin(), assets.cockpitDacPalette.end(),
            palette.source.begin());
        session.endingPalette_ = completedFullPaletteUpload(palette,
            {0, vgaPaletteColors, 0, 3});
        for (std::size_t color = 0; color < vgaPaletteColors; ++color) {
            const auto expand = [&session, color](const std::size_t component) {
                const auto value = session.endingPalette_->dac.colors[color * vgaPaletteComponents + component];
                return static_cast<std::uint8_t>((value << 2) | (value >> 4));
            };
            session.frame_.palette[color] = {expand(0), expand(1), expand(2)};
        }
        session.synth_.stopMusic();
        session.synth_.loadSampledVoice(voice());
        session.synth_.startSampledVoice();
        session.clock_.renderSamples(1000);
        assert(session.synth_.sampledVoiceObservation().phase == SampledVoicePhase::playing);
        session.synth_.stopSampledVoice();
        session.synth_.releaseSampledVoice();
    }

    static std::vector<std::uint8_t> cache(const NativeGameplaySession& session) {
        return *session.level2CachePagePixels();
    }
    static void removeTable(NativeGameplaySession& session) { session.highscoreTable_.reset(); }
    static void setTable(NativeGameplaySession& session, HighscoreTable table) {
        session.highscoreTable_ = std::move(table);
    }
    static void occupyMidi(NativeGameplaySession& session) { session.synth_.startMusic(); }
    static void releaseMidi(NativeGameplaySession& session) { session.synth_.stopMusic(); }
    static bool pressed(const NativeGameplaySession& session) { return session.menuPressed_.has_value(); }

private:
    static std::vector<std::uint8_t> voice() {
        constexpr std::size_t headerSize = 81;
        constexpr std::size_t payloadSize = 1000;
        std::vector<std::uint8_t> bytes(headerSize);
        bytes.resize(headerSize + payloadSize, 201);
        bytes[0] = 0x84;
        bytes[2] = 2;
        bytes[36] = 0xfc;
        bytes[51] = 0xc0;
        bytes[52] = 0x2b;
        bytes[69] = static_cast<std::uint8_t>(payloadSize);
        bytes[70] = static_cast<std::uint8_t>(payloadSize >> 8);
        return bytes;
    }
};
} // namespace stellar7

namespace {
using namespace stellar7;

std::vector<std::uint8_t> song() {
    std::vector<std::uint8_t> bytes(36);
    bytes[0] = 0x84;
    bytes[5] = 1;
    bytes[6] = 4;
    bytes.insert(bytes.end(), {0x91, 60, 64, 1, 0xfc});
    return bytes;
}

HighscoreTable table(const std::uint32_t score) {
    HighscoreTable::Entries rows{};
    for (auto& row : rows) {
        row = {score, "ACE"};
    }
    return HighscoreTable({}, rows);
}

GameplayAssets gameAssets() {
    GameplayAssets assets;
    assets.cockpit = {std::vector<std::uint8_t>(320 * 200), std::vector<Rgb>(256)};
    assets.cockpitDacPalette.resize(vgaPaletteBytes);
    for (std::size_t i = 0; i < assets.cockpitDacPalette.size(); ++i) {
        assets.cockpitDacPalette[i] = static_cast<std::uint8_t>((i * 7) % 64);
    }
    assets.cockpitSettings.emplace();
    assets.cockpitSettings->options[2] = 1;
    assets.highscoreTable = table(2000);
    assets.uiPalette = std::vector<Rgb>(256, {12, 20, 28});
    assets.soundBlasterVolumeTable.resize(16 * 64);
    for (std::size_t i = 0; i < assets.soundBlasterVolumeTable.size(); ++i) {
        assets.soundBlasterVolumeTable[i] = static_cast<std::uint8_t>(i % 64);
    }
    assets.horizonFrames = {{256, 16, std::vector<std::uint8_t>(256 * 16)}};
    assets.artGroups.resize(6, std::vector<IndexedBitmap>(64, {1, 1, {5}}));
    auto& art = assets.artGroups[0];
    art[0] = {16, 20, std::vector<std::uint8_t>(16 * 20)};
    art[0].pixels[0] = 5;
    art[11] = {4, 5, std::vector<std::uint8_t>(4 * 5, 9)};
    art[12] = {40, 40, std::vector<std::uint8_t>(40 * 40, 6)};
    art[16] = {112, 16, std::vector<std::uint8_t>(112 * 16, 3)};
    art[17] = art[18] = {116, 30, std::vector<std::uint8_t>(116 * 30, 8)};
    art[20] = {264, 2, std::vector<std::uint8_t>(264 * 2, 4)};
    art[21] = art[22] = {8, 5, std::vector<std::uint8_t>(8 * 5, 7)};
    assets.titleFont = {32, 6, 0,
        std::vector<IndexedBitmap>(64, {8, 6, std::vector<std::uint8_t>(48, 1)})};
    assets.logoMusicSequence = song();
    assets.presentationMusicSequence = song();
    assets.levelMusicSequence = song();
    return assets;
}

NativeStartupAssets startupAssets() {
    NativeStartupAssets result;
    result.logoScreen = {std::vector<std::uint8_t>(320 * 200, 9), std::vector<Rgb>(256)};
    result.menuScreen = result.logoScreen;
    result.logoFrames.resize(8, {1, 1, {7}});
    return result;
}

std::unique_ptr<NativeAudioOwner> adoptedAudio(const GameplayAssets& assets) {
    const auto resources = startupAssets();
    NativeStartupSequence startup(assets, resources);
    startup.start();
    startup.acceptSkipInput();
    while (startup.phase() != NativeStartupPhase::menu) {
        startup.advance(1024);
    }
    startup.requestBegin();
    while (startup.phase() != NativeStartupPhase::ready) {
        startup.advance(1024);
    }
    return startup.releaseAudio();
}

void rejectsReturnUnchanged(NativeGameplaySession& session, const GameplayInput& input = {}) {
    const auto ticks = session.model().ticks();
    const auto rng = session.model().level2Rng().calls();
    const auto samples = session.elapsedSamples();
    const auto driver = session.synth().driverState();
    const auto voice = session.synth().sampledVoiceObservation();
    const auto writes = session.synth().registerWrites().size();
    const auto roles = session.pageObservation();
    const auto frame = session.frame().pixels;
    const auto back = session.level2BackPagePixels();
    const auto cache = session.level2CachePagePixels();
    bool rejected = false;
    try {
        session.advance(input);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && session.model().ticks() == ticks && session.model().level2Rng().calls() == rng);
    assert(session.elapsedSamples() == samples && session.synth().registerWrites().size() == writes);
    assert(session.synth().driverState().clockPhase == driver.clockPhase
        && session.synth().driverState().directTimers == driver.directTimers);
    assert(session.synth().sampledVoiceObservation().phase == voice.phase
        && session.synth().sampledVoiceObservation().dacValue == voice.dacValue);
    const auto after = session.pageObservation();
    assert(after.cache == roles.cache && after.back == roles.back && after.front == roles.front
        && after.copy == roles.copy && after.draw == roles.draw);
    assert(session.frame().pixels == frame && session.level2BackPagePixels() == back
        && session.level2CachePagePixels() == cache && !session.menu());
}

void assertReturn(const bool swapped) {
    auto assets = gameAssets();
    NativeGameplaySession session(assets, GameplayModel{}, NativeSessionStart::level1,
        adoptedAudio(assets));
    NativeSessionTestAccess::stage(session, assets, swapped);
    assert(session.synth().startupPhase() == SoundBlasterStartupPhase::gameplay);
    const auto cache = NativeSessionTestAccess::cache(session);
    const auto roles = session.pageObservation();
    const auto firstSample = session.elapsedSamples();
    const auto firstPhase = session.synth().driverState().clockPhase;
    const auto held = session.synth().sampledVoiceObservation();
    const auto rng = session.model().level2Rng().calls();
    const auto ticks = session.model().ticks();

    NativeSessionTestAccess::removeTable(session);
    rejectsReturnUnchanged(session);
    NativeSessionTestAccess::setTable(session, table(1000));
    rejectsReturnUnchanged(session);
    NativeSessionTestAccess::setTable(session, table(2000));
    GameplayInput heldInput{};
    heldInput.forward = true;
    rejectsReturnUnchanged(session, heldInput);
    assets.artGroups[0][0].width = 15;
    rejectsReturnUnchanged(session);
    assets.artGroups[0][0].width = 16;
    const auto color = assets.cockpitDacPalette[0];
    assets.cockpitDacPalette[0] = static_cast<std::uint8_t>(color + 1);
    rejectsReturnUnchanged(session);
    assets.cockpitDacPalette[0] = color;
    NativeSessionTestAccess::occupyMidi(session);
    rejectsReturnUnchanged(session);
    NativeSessionTestAccess::releaseMidi(session);

    const auto delivery = session.advance({});
    assert(delivery.firstSample >= firstSample && delivery.samples.empty() && delivery.events.empty());
    assert(delivery.entry.size() == 2 && delivery.entry[0].elapsedNanoseconds > 0
        && delivery.entry[1].elapsedNanoseconds > delivery.entry[0].elapsedNanoseconds);
    assert(session.model().phase() == GameplayPhase::nativeMenu
        && session.model().presentationFrame() == 0 && session.model().ticks() == ticks + 1);
    assert(session.model().score() == 1250 && session.model().campaignLevel() == 7
        && session.model().level2LoopTick() == 976 && session.model().playerArmor() == 136
        && session.model().level2Rng().calls() == rng);
    assert(session.model().level2CockpitIndicatorX() == 139
        && session.model().level2CockpitIndicatorVelocity() == -4);
    assert(session.menu() && session.menu()->selection() == 0
        && session.synth().startupPhase() == SoundBlasterStartupPhase::presentation
        && session.synth().startupCountdown() == 30000);
    assert(session.synth().driverState().clockPhase != firstPhase
        && session.synth().sampledVoiceObservation().phase == SampledVoicePhase::unloaded
        && session.synth().sampledVoiceObservation().speakerEnabled == held.speakerEnabled
        && session.synth().sampledVoiceObservation().dacValue == held.dacValue);
    const auto after = session.pageObservation();
    assert(after.projection == NativePageProjection::retainedMenu && after.cache == roles.cache
        && after.front == roles.front && after.back == roles.back && after.copy == after.back
        && after.draw == after.back);
    assert(NativeSessionTestAccess::cache(session) == cache);
    assert(delivery.entry[0].indexed.pixels == std::vector<std::uint8_t>(320 * 200));
    assert(delivery.entry[1].indexed.pixels == session.frame().pixels);
    const auto back = *session.level2BackPagePixels();
    const auto front = session.frame().pixels;
    assert(front[69 * 320 + 163] == 5 && back[69 * 320 + 163] != 5);
    for (std::size_t i = 0; i < front.size(); ++i) {
        if (i != 69 * 320 + 163) {
            assert(front[i] == back[i]);
        }
    }
    IndexedPages expected;
    for (std::int16_t y = 0; y < 200; ++y) {
        for (std::int16_t x = 0; x < 320; ++x) {
            expected.write(after.back, {x, y}, 0);
        }
    }
    expected.apply(mainMenuPageEffects(assets.artGroups[0], assets.titleFont, after.back));
    for (std::int16_t y = 0; y < 200; ++y) {
        for (std::int16_t x = 0; x < 320; ++x) {
            assert(back[y * 320 + x] == expected.read(after.back, {x, y}));
        }
    }

    const auto pollStart = session.elapsedSamples();
    const auto pollPhase = session.synth().driverState();
    const auto countdown = session.synth().startupCountdown();
    const auto a = session.advanceMenu({}, 137);
    const auto b = session.advanceMenu({}, 512);
    assert(a.firstSample == pollStart && a.samples.size() == 137 && a.entry.empty());
    assert(b.firstSample == pollStart + 137 && b.samples.size() == 512 && b.entry.empty());
    assert(session.elapsedSamples() == pollStart + 649 && session.model().ticks() == ticks + 3
        && session.model().presentationFrame() == 2 && session.model().level2Rng().calls() == rng);
    const auto rawPeriod = pollPhase.clockPeriod / 4;
    const auto rawEdges = (pollPhase.clockPhase % rawPeriod + 649 * 1'193'182ULL) / rawPeriod;
    assert(session.synth().startupCountdown() == countdown - rawEdges);
    assert(session.frame().pixels == front && *session.level2BackPagePixels() == back
        && NativeSessionTestAccess::cache(session) == cache);

    session.advanceMenu({true, false, false}, 0);
    assert(session.menu()->selection() == 4);
    session.advanceMenu({true, false, false}, 0);
    assert(session.menu()->selection() == 4);
    session.advanceMenu({}, 0);
    session.advanceMenu({false, true, false}, 0);
    assert(session.menu()->selection() == 0);
    session.advanceMenu({}, 0);
    session.advanceMenu({false, false, true}, 0);
    assert(NativeSessionTestAccess::pressed(session));
    session.advanceMenu({true, false, true}, 0);
    assert(!NativeSessionTestAccess::pressed(session) && session.menu()->selection() == 4);
    session.advanceMenu({}, 0);
    session.advanceMenu({false, true, false}, 0);
    session.advanceMenu({}, 0);
    session.advanceMenu({false, false, true}, 0);
    const auto beforeReject = session.elapsedSamples();
    const auto beforeTicks = session.model().ticks();
    const auto pressedFrame = session.frame().pixels;
    bool rejected = false;
    try {
        session.advanceMenu({}, 137);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && NativeSessionTestAccess::pressed(session)
        && session.elapsedSamples() == beforeReject && session.model().ticks() == beforeTicks
        && session.frame().pixels == pressedFrame);
    rejected = false;
    try {
        const auto driver = session.synth().driverState();
        const auto idleBoundarySamples = (session.synth().startupCountdown() * (driver.clockPeriod / 4)
            + 1'193'182 - 1) / 1'193'182;
        session.advanceMenu({false, false, true}, idleBoundarySamples);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && session.elapsedSamples() == beforeReject && session.model().ticks() == beforeTicks);
}
} // namespace

int main() {
    try {
        assertReturn(false);
        assertReturn(true);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "retained title menu, physical roles, audio and bounded polls\n";
}
