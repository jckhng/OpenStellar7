#include "stellar7/native_session.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <stdexcept>

namespace stellar7 {

struct GameplayModelTestAccess {
    static void returned(GameplayModel& model) {
        model.level2Entities_.clear();
        model.initializeCampaignPools(4, 0);
        model.activeCampaignLevel_ = 6;
        model.phase_ = GameplayPhase::level5SuccessReturn;
        model.level2TailConsumed_ = true;
        model.level5TransientStateKnownInactive_ = true;
        model.successAudioOwned_ = true;
        model.successAudioActive_ = false;
        model.level2PlayerArmor_ = 136;
        model.level2Score_ = 1250;
        model.level2LoopTick_ = 976;
        model.level2Player_.yaw = 2336;
        model.level2Player_.yawVelocity = 432;
        model.level2Player_.forwardVelocity = 190;
    }

    static void removeSuccess(GameplayModel& model) { model.campaignSuccess_ = false; }

    static void assertExit(const GameplayModel& model) {
        assert(model.campaignExitFadeTicks_ == 1 && model.campaignSuccess_);
        assert(!model.level5Constructor_ && model.level5GuardianRadius_ == 0);
    }
};

struct NativeSessionTestAccess {
    static void prepare(NativeGameplaySession& session, const GameplayAssets& assets) {
        GameplayModelTestAccess::returned(session.model_);
        session.initialized_ = true;
        session.pixels_.emplace();
        for (const auto page : {session.pages_.cachePage(), session.pages_.backPage(), session.pages_.frontPage()}) {
            const auto color = static_cast<std::uint8_t>(page == session.pages_.frontPage() ? 3 : 1);
            for (std::int16_t y = 0; y < 200; ++y) {
                for (std::int16_t x = 0; x < 320; ++x) {
                    session.pixels_->write(page, {x, y}, color);
                }
            }
        }
        session.radar_.emplace(std::vector<IndexedBitmap>{{120, 56, std::vector<std::uint8_t>(120 * 56)}});
        session.frame_ = {std::vector<std::uint8_t>(320 * 200, 3), std::vector<Rgb>(256, {12, 24, 36})};
        const auto& level = assets.campaignLevels.at(4);
        session.campaignResources_ = NativeGameplaySession::CampaignResourceBorrow{
            4, &level.bitmapGroups, &level.models, &level.musicSequence};
        session.synth_.loadCampaignMusic(level.musicSequence);
        session.clock_.renderSamples(37);
        session.synth_.stopMusic();
    }

    static bool ownsLevel(const NativeGameplaySession& session) {
        return session.campaignResources_ && session.campaignResources_->bitmaps
            && session.campaignResources_->models && session.campaignResources_->music;
    }

    static bool ownsScene(const NativeGameplaySession& session) {
        return session.ending_.has_value() && session.endingAssets_;
    }

    static void assertScenePalette(const NativeGameplaySession& session) {
        assert(session.endingPalette_);
        const auto& palette = *session.endingPalette_;
        assert(palette.source == palette.output && palette.output == palette.dac.colors);
        assert(palette.source[0] == 4 && palette.source[1] == 8 && palette.source[2] == 12);
        assert(session.endingPaletteRequest_.intensity == 0 && session.endingPaletteRequest_.targetColor == 0);
    }

    static void invalidateVoiceSelector(NativeGameplaySession& session) { session.sceneVoiceSegment_ = 0; }
    static bool hasCompletedWait(const NativeGameplaySession& session) { return session.endingWait_.remaining == 0; }

    static bool canReadRetainedScene(NativeGameplaySession& session) {
        return session.ending_->execute(0).present;
    }
};
} // namespace stellar7

namespace {
using namespace stellar7;

GameplayAssets assets() {
    GameplayAssets data;
    data.campaignLevels.resize(5);
    auto& level = data.campaignLevels[4];
    level.resourceName = "LEVEL5.RES";
    level.musicSequence.resize(36);
    data.cockpitSettings.emplace();
    data.scoreFont = {32, 8, 0, std::vector<IndexedBitmap>(96, {1, 8, std::vector<std::uint8_t>(8)})};
    auto ending = std::make_shared<EndingSceneAssets>();
    ending->scene.program.pages.resize(endingScenePageCount,
        {{SceneCommandKind::endPage, {}, {}, 0}});
    ending->scene.assets = {std::vector<Rgb>(256), std::vector<IndexedBitmap>(16),
        std::vector<std::uint8_t>(320 * 200)};
    ending->sampledVoice.resize(82);
    ending->sampledVoice[0] = 0x84;
    ending->sampledVoice[2] = 2;
    ending->sampledVoice[36] = 0xfc;
    ending->sampledVoice[51] = 11200 & 255;
    ending->sampledVoice[52] = 11200 >> 8;
    ending->sampledVoice[69] = 1;
    ending->sampledVoice[81] = 128;
    data.endingScene = std::move(ending);
    return data;
}

void assertEntry() {
    auto data = assets();
    NativeGameplaySession session(data, {});
    NativeSessionTestAccess::prepare(session, data);
    const auto samples = session.elapsedSamples();
    const auto state = session.synth().driverState();
    const auto writes = session.synth().registerWrites().size();
    const auto frame = session.frame().pixels;
    const auto back = session.level2BackPagePixels(), cache = session.level2CachePagePixels();
    const auto pages = session.pageObservation();
    const auto delivery = session.advance({});
    assert(delivery.firstSample == samples && delivery.samples.empty() && delivery.entry.empty() && delivery.events.empty());
    assert(session.elapsedSamples() == samples && session.synth().registerWrites().size() == writes);
    assert(session.synth().driverState().clockPhase == state.clockPhase);
    assert(session.synth().driverState().directTimers == state.directTimers);
    assert(session.model().campaignLevel() == 7 && session.model().phase() == GameplayPhase::endingEntry);
    assert(session.model().level2LoopTick() == 976 && session.model().score() == 1250 && session.model().playerArmor() == 136);
    assert(session.model().heading() == 2336 && session.model().turnVelocity() == 432 && session.model().forwardVelocity() == 190);
    assert(session.frame().pixels == frame && session.level2BackPagePixels() == back && session.level2CachePagePixels() == cache);
    const auto after = session.pageObservation();
    assert(after.cache == pages.cache && after.front == pages.front && after.back == pages.back
        && after.copy == pages.copy && after.draw == after.front);
    assert(!NativeSessionTestAccess::ownsLevel(session) && NativeSessionTestAccess::ownsScene(session));
    GameplayModelTestAccess::assertExit(session.model());
    const std::weak_ptr<const EndingSceneAssets> lifetime = data.endingScene;
    data.endingScene.reset();
    assert(!lifetime.expired() && NativeSessionTestAccess::canReadRetainedScene(session));
    bool rejected = false;
    const auto ticks = session.model().ticks();
    try { session.advance({}); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && session.model().ticks() == ticks && session.elapsedSamples() == samples);
}

GameplayAssets firstPageAssets(const std::int16_t voiceEnabled) {
    auto data = assets();
    data.cockpitDacPalette.resize(256 * 3);
    data.cockpitSettings->options[2] = voiceEnabled;
    auto scene = std::make_shared<EndingSceneAssets>(*data.endingScene);
    scene->scene.program.pages[0] = {
        {SceneCommandKind::selectPaletteBank, {0}},
        {SceneCommandKind::loadPalette},
        {SceneCommandKind::loadBitmaps},
        {SceneCommandKind::bindFont},
        {SceneCommandKind::loadBackground},
        {SceneCommandKind::sound, {0, 3}},
        {SceneCommandKind::setDelay, {0}},
        {SceneCommandKind::endPage}};
    auto& colors = std::get<0>(scene->scene.assets[0]);
    colors.assign(256, {16, 32, 48});
    auto& background = std::get<2>(scene->scene.assets[2]);
    for (std::size_t i = 0; i < background.size(); ++i) {
        background[i] = static_cast<std::uint8_t>(i);
    }
    data.endingScene = scene;
    return data;
}

void assertFirstPage(const std::int16_t voiceEnabled) {
    auto data = firstPageAssets(voiceEnabled);
    const auto& background = std::get<2>(data.endingScene->scene.assets[2]);
    NativeGameplaySession session(data, {});
    NativeSessionTestAccess::prepare(session, data);
    session.advance({});
    const auto ticks = session.model().ticks(), samples = session.elapsedSamples();
    const auto driver = session.synth().driverState();
    const auto writes = session.synth().registerWrites().size();
    const auto before = session.pageObservation();
    const auto delivery = session.advance({});
    assert(session.model().ticks() == ticks + 1 && session.model().presentationFrame() == 1);
    assert(session.model().phase() != GameplayPhase::endingEntry && session.model().campaignLevel() == 7);
    assert(session.model().score() == 1250 && session.model().playerArmor() == 136 && session.model().level2LoopTick() == 976);
    assert(session.frame().pixels == background && *session.level2BackPagePixels() == background
        && *session.level2CachePagePixels() == background);
    for (const auto color : session.frame().palette) {
        assert(color.red == 16 && color.green == 32 && color.blue == 48);
    }
    assert(delivery.firstSample == samples && delivery.samples.empty() && delivery.entry.empty() && delivery.events.empty());
    assert(session.elapsedSamples() == samples && session.synth().registerWrites().size() == writes);
    assert(session.synth().driverState().clockPhase == driver.clockPhase
        && session.synth().driverState().directTimers == driver.directTimers);
    const auto voice = session.synth().sampledVoiceObservation();
    assert(voice.phase == (voiceEnabled ? SampledVoicePhase::loaded : SampledVoicePhase::unloaded));
    assert(voice.transferred == 0 && !voice.speakerEnabled && voice.clockPhase == 0);
    const auto after = session.pageObservation();
    assert(after.cache == before.cache && after.back == before.back && after.front == before.front
        && after.copy == after.back && after.draw == after.front);
    NativeSessionTestAccess::assertScenePalette(session);
    bool rejected = false;
    try { session.advance({}); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && session.model().ticks() == ticks + 1 && session.elapsedSamples() == samples);
}

GameplayAssets timedAssets(const std::size_t voiceBytes = 56301) {
    auto data = firstPageAssets(1);
    auto scene = std::const_pointer_cast<EndingSceneAssets>(data.endingScene);
    scene->sampledVoice.resize(81 + voiceBytes, 128);
    scene->sampledVoice[69] = static_cast<std::uint8_t>(voiceBytes);
    scene->sampledVoice[70] = static_cast<std::uint8_t>(voiceBytes >> 8);
    scene->scene.program.pages[1] = {{SceneCommandKind::sound, {0, 0}},
        {SceneCommandKind::setDelay, {6}}, {SceneCommandKind::endPage}};
    return data;
}

void assertTimedPages() {
    auto data = timedAssets();
    for (std::size_t index = 0; index < data.cockpitDacPalette.size(); ++index) {
        data.cockpitDacPalette[index] = static_cast<std::uint8_t>((index * 7) % 64);
    }
    NativeGameplaySession session(data, {});
    NativeSessionTestAccess::prepare(session, data);
    session.advance({});
    session.advance({});
    const auto initialTicks = session.model().ticks();
    const auto initialRng = session.model().level2Rng().calls();
    const auto frame = session.frame().pixels;
    for (std::size_t page = 1; page <= endingScenePageCount; ++page) {
        const auto first = session.elapsedSamples();
        const auto before = session.synth().driverState();
        const auto irqPeriod = before.clockPeriod / 4;
        const auto delivery = session.advance({});
        const auto distance = 24 * irqPeriod - before.clockPhase % irqPeriod;
        const auto expected = (distance + 1'193'182 - 1) / 1'193'182;
        assert(delivery.firstSample == first && delivery.samples.size() == expected);
        assert(delivery.entry.empty() && delivery.events.empty() && session.elapsedSamples() == first + expected);
        assert(session.model().ticks() == initialTicks + page && session.model().presentationFrame() == page + 1);
        assert(session.model().phase() == (page == endingScenePageCount ? GameplayPhase::endingTeardown : GameplayPhase::endingPresentation));
        assert(session.model().level2LoopTick() == 976 && session.model().campaignLevel() == 7
            && session.model().score() == 1250 && session.model().playerArmor() == 136
            && session.model().level2Rng().calls() == initialRng && session.frame().pixels == frame);
        assert(NativeSessionTestAccess::hasCompletedWait(session));
        const auto roles = session.pageObservation();
        assert(roles.copy == roles.back && roles.draw == roles.front);
    }
    assert(session.synth().sampledVoiceObservation().phase == SampledVoicePhase::playing);
    const auto ticks = session.model().ticks(), samples = session.elapsedSamples();
    const auto roles = session.pageObservation();
    const auto cache = session.level2CachePagePixels();
    const auto voice = session.synth().sampledVoiceObservation();
    const auto writes = session.synth().registerWrites().size();
    //A malformed second palette must reject before the first scene release/fade.
    data.cockpitDacPalette.back() = 64;
    bool rejected = false;
    try { session.advance({}); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && session.model().ticks() == ticks && session.elapsedSamples() == samples);
    assert(NativeSessionTestAccess::ownsScene(session) && session.frame().pixels == frame
        && session.synth().sampledVoiceObservation().phase == SampledVoicePhase::playing);
    data.cockpitDacPalette.back() = static_cast<std::uint8_t>(((data.cockpitDacPalette.size() - 1) * 7) % 64);
    const std::weak_ptr<const EndingSceneAssets> lifetime = data.endingScene;
    data.endingScene.reset();
    const auto delivery = session.advance({});
    assert(delivery.firstSample == samples && delivery.samples.empty() && delivery.events.empty()
        && delivery.entry.size() == blackoutPaletteSteps * 2 + 3);
    std::size_t delivered = 0;
    std::uint64_t elapsed = 0;
    for (std::size_t index = 0; index < delivery.entry.size(); ++index) {
        const auto& entry = delivery.entry[index];
        assert(entry.elapsedNanoseconds >= elapsed);
        elapsed = entry.elapsedNanoseconds;
        delivered += entry.samples.size();
        if (index < blackoutPaletteSteps) {
            assert(entry.indexed.pixels == frame);
        } else {
            assert(std::all_of(entry.indexed.pixels.begin(), entry.indexed.pixels.end(),
                [](const auto value) { return value == 0; }));
        }
    }
    assert(delivered != 0 && session.elapsedSamples() == samples + delivered && elapsed != 0);
    assert(lifetime.expired() && !NativeSessionTestAccess::ownsScene(session));
    assert(session.model().ticks() == ticks + 1 && session.model().phase() == GameplayPhase::endingReturn);
    assert(session.model().level2LoopTick() == 976 && session.model().campaignLevel() == 7
        && session.model().score() == 1250 && session.model().playerArmor() == 136
        && session.model().level2Rng().calls() == initialRng);
    const auto after = session.pageObservation();
    assert(after.front == roles.back && after.back == roles.front && after.cache == roles.cache
        && after.draw == after.front && after.copy == after.front);
    assert(session.level2CachePagePixels() == cache);
    const auto held = session.synth().sampledVoiceObservation();
    assert(held.phase == SampledVoicePhase::unloaded && held.speakerEnabled == voice.speakerEnabled
        && held.dacValue == voice.dacValue && session.synth().registerWrites().size() == writes);
    rejected = false;
    try { session.advance({}); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && session.model().ticks() == ticks + 1 && session.elapsedSamples() == samples + delivered);
}

template<class Alter>
void assertTimedRejected(Alter alter, const std::size_t voiceBytes = 56301) {
    auto data = timedAssets(voiceBytes);
    NativeGameplaySession session(data, {});
    NativeSessionTestAccess::prepare(session, data);
    session.advance({});
    session.advance({});
    alter(data, session);
    const auto ticks = session.model().ticks(), samples = session.elapsedSamples();
    const auto frame = session.frame().pixels;
    const auto back = session.level2BackPagePixels(), cache = session.level2CachePagePixels();
    const auto writes = session.synth().registerWrites().size();
    const auto before = session.synth().driverState();
    const auto pages = session.pageObservation();
    bool rejected = false;
    try { session.advance({}); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && session.model().ticks() == ticks && session.elapsedSamples() == samples);
    assert(session.frame().pixels == frame && session.level2BackPagePixels() == back && session.level2CachePagePixels() == cache);
    assert(session.pageObservation().copy == pages.copy && session.pageObservation().draw == pages.draw);
    assert(session.synth().registerWrites().size() == writes && session.synth().driverState().clockPhase == before.clockPhase);
    assert(session.synth().sampledVoiceObservation().phase == SampledVoicePhase::loaded);
}

template<class Alter>
void assertPageRejected(Alter alter, const GameplayInput& input = {}) {
    auto data = firstPageAssets(1);
    NativeGameplaySession session(data, {});
    NativeSessionTestAccess::prepare(session, data);
    session.advance({});
    // Deliberately corrupt a fixture after entry to exercise downstream preflight.
    alter(data, const_cast<GameplayModel&>(session.model()));
    const auto ticks = session.model().ticks(), samples = session.elapsedSamples();
    const auto writes = session.synth().registerWrites().size();
    const auto frame = session.frame();
    const auto back = session.level2BackPagePixels(), cache = session.level2CachePagePixels();
    const auto pages = session.pageObservation();
    bool rejected = false;
    try { session.advance(input); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && session.model().ticks() == ticks && session.elapsedSamples() == samples);
    assert(session.model().phase() == GameplayPhase::endingEntry && session.model().presentationFrame() == 0);
    assert(session.synth().registerWrites().size() == writes
        && session.synth().sampledVoiceObservation().phase == SampledVoicePhase::unloaded);
    assert(session.frame().pixels == frame.pixels && session.level2BackPagePixels() == back && session.level2CachePagePixels() == cache);
    for (std::size_t i = 0; i < frame.palette.size(); ++i) {
        assert(session.frame().palette[i].red == frame.palette[i].red
            && session.frame().palette[i].green == frame.palette[i].green
            && session.frame().palette[i].blue == frame.palette[i].blue);
    }
    const auto after = session.pageObservation();
    assert(after.copy == pages.copy && after.draw == pages.draw && after.projection == pages.projection);
}

template<class Alter>
void assertRejected(Alter alter, const GameplayInput& input = {}) {
    auto data = assets();
    NativeGameplaySession session(data, {});
    NativeSessionTestAccess::prepare(session, data);
    alter(data);
    const auto ticks = session.model().ticks(), samples = session.elapsedSamples();
    const auto writes = session.synth().registerWrites().size();
    const auto page = session.pageObservation();
    bool rejected = false;
    try { session.advance(input); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && session.model().ticks() == ticks && session.elapsedSamples() == samples);
    assert(session.model().campaignLevel() == 6 && NativeSessionTestAccess::ownsLevel(session));
    assert(!NativeSessionTestAccess::ownsScene(session));
    assert(session.synth().registerWrites().size() == writes && session.pageObservation().draw == page.draw);
}

void assertDescriptorRelease() {
    SoundBlasterSynth synth;
    synth.loadCampaignMusic(std::vector<std::uint8_t>(36));
    bool rejected = false;
    const auto writes = synth.registerWrites().size();
    try { synth.releaseCampaignMusic(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && synth.registerWrites().size() == writes && synth.driverState().sequenceActive);
    synth.stopMusic();
    const auto stopped = synth.registerWrites().size();
    const auto state = synth.driverState();
    synth.releaseCampaignMusic();
    assert(synth.registerWrites().size() == stopped && synth.driverState().clockPhase == state.clockPhase);
    assert(synth.driverState().directTimers == state.directTimers);
    rejected = false;
    try { synth.releaseCampaignMusic(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && synth.registerWrites().size() == stopped);
}
}

int main() {
    try {
        assertEntry();
        assertFirstPage(1);
        assertFirstPage(0);
        assertTimedPages();
        assertTimedRejected([](auto&, auto&) {}, 1);
        assertTimedRejected([](auto& data, auto&) { data.cockpitSettings->options[2] = 0; });
        assertTimedRejected([](auto&, auto& session) { NativeSessionTestAccess::invalidateVoiceSelector(session); });
        assertTimedRejected([](auto& data, auto&) {
            auto scene = std::const_pointer_cast<EndingSceneAssets>(data.endingScene);
            scene->scene.program.pages[1].insert(scene->scene.program.pages[1].end() - 1,
                {SceneCommandKind::selectPaletteBank, {0}});
        });
        assertTimedRejected([](auto& data, auto&) {
            std::const_pointer_cast<EndingSceneAssets>(data.endingScene)->scene.program.pages[1][1].arguments[0] = 0;
        });
        assertPageRejected([](auto&, auto& model) { GameplayModelTestAccess::removeSuccess(model); });
        assertPageRejected([](auto& data, auto&) {
            auto scene = std::const_pointer_cast<EndingSceneAssets>(data.endingScene);
            std::get<0>(scene->scene.assets[0])[17].red = 17;
        });
        assertPageRejected([](auto& data, auto&) {
            std::const_pointer_cast<EndingSceneAssets>(data.endingScene)->sampledVoice.pop_back();
        });
        assertPageRejected([](auto& data, auto&) {
            std::const_pointer_cast<EndingSceneAssets>(data.endingScene)->scene.program.pages[0][5].arguments[1] = 2;
        });
        assertDescriptorRelease();
        assertRejected([](auto& data) { data.endingScene.reset(); });
        assertRejected([](auto& data) { data.scoreFont.height = 9; });
        assertRejected([](auto& data) { data.campaignLevels[4].resourceName = "LEVEL7.RES"; });
        assertRejected([](auto& data) {
            auto invalid = std::make_shared<EndingSceneAssets>(*data.endingScene);
            invalid->sampledVoice.clear();
            data.endingScene = std::move(invalid);
        });
        GameplayInput held{};
        held.forward = true;
        assertRejected([](auto&) {}, held);
        assertPageRejected([](auto&, auto&) {}, held);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
