#include "stellar7/native_session.hpp"
#include "stellar7/level1_loader_work.hpp"
#include "stellar7/recovered_frontend.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace {
void require(const bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template<class Callback>
void rejects(Callback callback) {
    bool rejected = false;
    try {
        callback();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "adoption accepted a noninitial owner");
}

void sameRng(stellar7::StellarRng actual, stellar7::StellarRng expected) {
    require(actual.calls() == expected.calls() && actual.indexA() == expected.indexA()
        && actual.indexB() == expected.indexB(), "adoption consumed or reset RNG");
    for (std::size_t index = 0; index < stellar7::StellarRng::wordCount; ++index) {
        require(actual.next() == expected.next(), "adoption changed retained RNG words");
    }
}
}

namespace stellar7 {
struct GameplayModelTestAccess {
    static void pools(const GameplayModel& model) {
        require(model.level2Entities_.size() == 6 && model.level2ActiveEnemyCount_ == 2
            && model.level2SpawnedEnemyCount_ == 2, "adoption lost actual initial counts");
        require(model.level2ActiveListOrder_.empty() && model.level2ObjectListOrder_.empty()
            && model.level2AllocationListOrder_.size() == 6 && model.level2Visibility_.refreshRequested(),
            "adoption consumed initial visibility");
        require(model.level1_.entities().empty() && !model.level1_.missionActive(), "source Level1 still owns combat actors");
        for (std::size_t index = 0; index < model.level2Entities_.size(); ++index) {
            const auto& actor = model.level2Entities_[index];
            const auto& physical = index < 2 ? model.level2ActorPool_[index] : model.level2FixedPool_[index - 2];
            require(actor.type == physical.type && actor.position == physical.position
                && actor.stateBytes4dTo52 == physical.stateBytes4dTo52 && actor.renderProfile == physical.renderProfile,
                "initial actor lost physical pool alias");
            require((index < 2 ? model.level2EntityActorPoolSlots_[index] : model.level2EntityFixedPoolSlots_[index])
                == (index < 2 ? index : index - 2), "adoption assigned a different physical slot");
        }
        for (std::size_t slot = 0; slot < model.level2ProjectilePoolSize_; ++slot) {
            require(!model.level2ProjectilePoolActive_[slot] && !model.level2ProjectilePool_[slot].active()
                && model.level2ProjectilePool_[slot].stateBytes4dTo52[0] == 0xfd,
                "adoption lost initial projectile reset");
        }
        for (std::size_t slot = 0; slot < model.level2DeathEffectPoolSize_; ++slot) {
            require(!model.level2DeathEffectPoolActive_[slot] && !model.level2DeathEffectPool_[slot].active()
                && model.level2DeathEffectPool_[slot].stateBytes4dTo52[0] == 0xfc,
                "adoption lost initial effect reset");
        }
    }
};

struct NativeSessionTestAccess {
    static void adopt(NativeGameplaySession& session) { session.adoptPlayableLevel1(); }
    static void commonRadarAllocations(const NativeGameplaySession& session) {
        constexpr std::size_t projectileBase = 32;
        constexpr std::size_t effectBase = projectileBase + Level1State::projectilePoolSize;
        for (std::size_t slot = 0; slot < Level1State::projectilePoolSize; ++slot) {
            static_cast<void>(session.radar_->projection(projectileBase + slot));
        }
        for (std::size_t slot = 0; slot < Level1State::deathEffectPoolSize; ++slot) {
            static_cast<void>(session.radar_->projection(effectBase + slot));
        }
    }
};
}

namespace {
using namespace stellar7;

std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("missing original resource: " + path.string());
    }
    return {std::istreambuf_iterator<char>(file), {}};
}

GameplayAssets assets(const std::filesystem::path& root) {
    const auto cockpit = read(root / "COCKPIT.RES");
    const auto main = read(root / "STELLAR.RES");
    const auto art = read(root / "STELART.RES");
    GameplayAssets result{};
    result.artGroups = decodeBitmapGroups(art);
    const auto palettes = decodePalettes(art);
    result.uiPalette = palettes.back();
    for (const auto color : palettes.at(2)) {
        for (const auto component : {color.red, color.green, color.blue}) {
            result.cockpitDacPalette.push_back(component >> 2);
        }
    }
    result.cockpit = applyVgaDacPalette(decodeFirstIndexedScreenResource(cockpit), result.cockpitDacPalette);
    const auto fonts = decodeFontResources(main);
    result.titleFont = fonts.at(0);
    result.scoreFont = fonts.at(1);
    result.soundSequences = decodeTaggedPayloads(cockpit, {'S','N','G',':'});
    result.soundBlasterInstrumentResource = decodeTaggedPayloads(main, {'0','0','3',':'}).at(0);
    const auto songs = decodeTaggedPayloads(main, {'S','N','G',':'});
    result.logoMusicSequence = songs.at(0);
    result.presentationMusicSequence = songs.at(1);
    result.soundBlasterVolumeTable = decodeSoundBlasterVolumeTable(decodeTaggedPayloads(main, {'S','B','L',':'}).at(0));
    result.models = decodeFirstModelTableResource(cockpit);
    for (unsigned level = 1; level <= 7; ++level) {
        const auto name = "LEVEL" + std::to_string(level) + ".RES";
        const auto bytes = read(root / name);
        result.campaignLevels.push_back({name, decodeBitmapGroups(bytes),
            decodeTaggedPayloads(bytes, {'S','N','G',':'}).at(0), decodeFirstModelTableResource(bytes)});
    }
    const auto& local = result.campaignLevels.front().models;
    result.models.insert(result.models.end(), local.begin(), local.end());
    result.sharedModelRegistry = {256, 256};
    result.modelColorTables = decodeByteArrays(main);
    result.cockpitSettings = CockpitSettings{82, {1, 0, 1, 1, 0, 0}};
    HighscoreTable table({}, {});
    table.setConfiguration(*result.cockpitSettings);
    result.highscoreTable = table;
    return result;
}

void modelAdoption(const GameplayAssets& data) {
    const std::vector<Model3d> shared(data.models.begin(), data.models.begin() + 44);
    const auto& local = data.campaignLevels.front().models;
    auto model = std::make_unique<GameplayModel>(GameplayModel::fromNativeStartup());
    const auto random = model->level1State().rng();
    const auto ground = model->level1State().ground().points();
    const auto actors = model->level1State().entities();
    const auto indicator = model->level2CockpitIndicatorX();
    const auto statuses = model->level1State().systemStatusLevels();
    auto bad = shared;
    bad[29].collisionRadius = 0;
    rejects([&]() { model->adoptPlayableLevel1(bad, local); });
    require(model->phase() == GameplayPhase::level1 && model->ticks() == 0, "rejected adoption changed source phase");
    sameRng(model->level1State().rng(), random);
    model->adoptPlayableLevel1(shared, local);
    require(model->phase() == GameplayPhase::playablePreloopReturn && model->campaignLevel() == 0
        && model->ticks() == 0 && model->level2LoopTick() == 0, "adoption advanced constructor/preloop/main");
    require(model->score() == 0 && model->playerArmor() == 680 && model->level2Ground().points() == ground
        && model->level2CockpitIndicatorX() == indicator && model->level1State().systemStatusLevels() == statuses,
        "adoption rebuilt initial ground/cockpit/system/score state");
    sameRng(model->level2Rng(), random);
    GameplayModelTestAccess::pools(*model);
    for (std::size_t index = 0; index < actors.size(); ++index) {
        const auto& actor = model->level2Entities()[index];
        require(actor.type == actors[index].type && actor.position == actors[index].position
            && actor.stateBytes4dTo52 == actors[index].stateBytes4dTo52, "adoption reconstructed a table actor");
    }
    require(model->level2Entities()[0].renderProfile == WorldActorRenderProfile::noHandles
        && model->level2Entities()[1].renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation,
        "adoption lost implicit source presenter ownership");
    rejects([&]() { model->adoptPlayableLevel1(shared, local); });
    sameRng(model->level2Rng(), random);
    auto advanced = std::make_unique<GameplayModel>(GameplayModel::fromNativeStartup());
    advanced->tick({});
    const auto advancedRng = advanced->level1State().rng();
    rejects([&]() { advanced->adoptPlayableLevel1(shared, local); });
    sameRng(advanced->level1State().rng(), advancedRng);
}

void nativeAdoption(const GameplayAssets& data, const std::filesystem::path& root) {
    const auto resources = decodeNativeStartupAssets(read(root / "STELART.RES"));
    NativeStartupSequence startup(data, resources);
    startup.start(NativeStartupExecution::recoveredContext);
    startup.initializeSettings(StartupCapabilityProfile::noMouseOrJoystick);
    startup.initializeResources(NativeStartupDeviceProfile::portableOplNoDspProbe);
    startup.initializePresentation();
    startup.initializeFonts();
    while (startup.phase() == NativeStartupPhase::logoCountdownWork) {
        startup.pollLogoCountdown(10000);
    }
    startup.playLogoEvent();startup.animateLogo();startup.finishLogoSound();startup.releaseLogo();
    startup.skipIntro(NativeIntroKey::escape);startup.beginMenuSong();startup.initializeMenuResources();
    startup.skipCredits(NativeIntroKey::escape);startup.enterMenu(NativeMenuEntryProfile::keyboardOnly);
    startup.beginGame(NativeMenuBeginProfile::enterPressRelease);
    auto session = std::make_unique<NativeGameplaySession>(data, startup.releaseGameplay());
    rejects([&]() { NativeSessionTestAccess::adopt(*session); });
    session->initializeRecoveredLevel1();
    const auto random = session->model().level1State().rng();
    const auto samples = session->elapsedSamples();
    const auto cycles = session->synth().driverState().normalClockCycles;
    const auto writes = session->synth().registerWrites();
    const auto pages = session->pageObservation();
    const auto frame = session->frame();
    std::vector<std::vector<std::uint8_t>> pixels;
    for (const auto role : {NativePageRole::cache, NativePageRole::back, NativePageRole::front}) {
        pixels.push_back(*session->retainedPagePixels(role));
    }
    NativeSessionTestAccess::adopt(*session);
    NativeSessionTestAccess::commonRadarAllocations(*session);
    const auto& phaseByte=session->recoveredClockPhase()->callback->program->enemy20McbByte;
    const auto synthByte=session->synth().callbackWorkState().program->enemy20McbByte;
    constexpr std::size_t cacheSourcePixel=4*510+2;
    require(phaseByte && synthByte && *phaseByte==*synthByte
        && phaseByte->value()==data.artGroups.at(3).at(2).pixels.at(cacheSourcePixel),
        "adoption lost the retained menu-cache audio byte owner");
    require(phaseByte->nativeProcessByte(38) == 'T', "adoption lost its supported DOS process identity");
    require(session->elapsedSamples() == samples && session->synth().driverState().normalClockCycles == cycles
        && session->synth().registerWrites().size() == writes.size(), "adoption reset/advanced clock or wrote OPL");
    require(session->pageObservation().front == pages.front && session->pageObservation().back == pages.back
        && session->pageObservation().cache == pages.cache && session->frame().pixels == frame.pixels,
        "adoption replaced retained pages");
    std::size_t index = 0;
    for (const auto role : {NativePageRole::cache, NativePageRole::back, NativePageRole::front}) {
        require(*session->retainedPagePixels(role) == pixels[index++], "adoption redrew an owned page");
    }
    require(session->playableCampaign() && session->recoveredClockPhase()->playableCampaign,
        "adoption retained source admission for unmeasured common combat");
    sameRng(session->model().level2Rng(), random);
    rejects([&]() { session->advanceRecoveredLevel1Frame(FrameCopyProfile::modeXNoop, 10000, {}); });
    rejects([&]() { NativeSessionTestAccess::adopt(*session); });
    for (unsigned adjacent = 0; adjacent < 3; ++adjacent) {
        GameplayInput input{};
        input.forward = input.left = input.fire = true;
        const auto delivery = session->advancePlayableCampaign(input, adjacent == 0 ? 10000 : 257);
        require(delivery.firstSample >= samples && session->model().campaignLevel() == 0
            && session->model().phase() == GameplayPhase::playable,
            "adopted first common main lost Level1/PCM ownership");
    }
    require(session->model().level2LoopTick() == 3, "adoption consumed the first common main twice");
}

void applicationAdoption(const GameplayAssets& data, const std::filesystem::path& root) {
    const auto resources = decodeNativeStartupAssets(read(root / "STELART.RES"));
    auto frontend = std::make_unique<RecoveredFrontend>(data, resources, FrameCopyProfile::modeXNoop,
        10000, std::shared_ptr<HighscoreStore>{}, CampaignExecution::playable);
    unsigned startupCalls = 0;
    unsigned menuPolls = 0;
    while (!frontend->session() && startupCalls < 10000) {
        NativeMenuInput input{};
        if (frontend->startup().phase() == NativeStartupPhase::awaitingMenuDecision) {
            input.confirm = menuPolls++ == 1;
        }
        const auto before = frontend->elapsedSamples();
        const auto delivery = frontend->advanceStartup(input);
        require(delivery.firstSample == before && before + delivery.samples.size() == frontend->elapsedSamples(),
            "application startup lost its PCM prefix");
        ++startupCalls;
    }
    require(frontend->session() && menuPolls == 3, "application did not release actual Begin into initial loader");
    require(frontend->session()->model().phase() == GameplayPhase::level1
        && frontend->session()->model().level1State().rng().calls() == 16,
        "application did not retain its untouched initial Level1 preloop");
    for (unsigned adjacent = 0; adjacent < 3; ++adjacent) {
        GameplayInput input{};
        input.forward = input.left = input.fire = true;
        const auto before = frontend->elapsedSamples();
        const auto delivery = frontend->advanceGameplay(input);
        require(delivery.firstSample == before && before + delivery.samples.size() == frontend->elapsedSamples(),
            "application adoption lost its PCM prefix");
        require(frontend->session()->playableCampaign() && frontend->session()->model().campaignLevel() == 0
            && frontend->session()->model().phase() == GameplayPhase::playable
            && frontend->session()->model().level2LoopTick() == adjacent + 1,
            "explicit playable application did not adopt initial Level1 before common main");
    }
    std::cout << "Device-free application actual startup/Begin press-release -> initial Level1 common+3 PASS\n";
}
}

int main(int argc, char** argv) {
    if (argc != 2) { return 2; }
    try {
        const std::filesystem::path root(argv[1]);
        const auto data = assets(root);
        modelAdoption(data);
        nativeAdoption(data, root);
        auto introduction=data;
        introduction.cockpitSettings->options[5]=255;
        introduction.highscoreTable->setConfiguration(*introduction.cockpitSettings);
        nativeAdoption(introduction, root);
        applicationAdoption(data, root);
        std::cout << "Actual initial Level1 loader/preloop -> common pool adoption without RNG/pages/clock reset +3 PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
