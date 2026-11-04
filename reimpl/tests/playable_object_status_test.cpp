// Controlled component entries with original models; no natural campaign ancestry.
#include "stellar7/gameplay.hpp"
#include "playable_startup_resources.hpp"
#include "failure_page.hpp"
#ifdef STELLAR7_ENHANCED
#include "world_presentation.hpp"
#endif
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
std::vector<stellar7::Model3d> models(const std::filesystem::path& root, const std::string& name) {
    std::ifstream file(root / name, std::ios::binary);
    require(bool(file), "missing original object-status resource");
    return stellar7::decodeFirstModelTableResource({std::istreambuf_iterator<char>(file), {}});
}
}
namespace stellar7 {
struct Level1ReloadTestAccess {
    static void thirdKill(Level1State& systems, std::int16_t type) {
        systems.lastDestroyedType_ = type;
        systems.consecutiveDestroyedTypeCount_ = 2;
    }
    static void domain(Level1State& systems) {
        systems.systemStatusLevels_.fill(1);
        systems.systemTimers_.fill(0);
        systems.systemTimers_[3] = 17;
        systems.selectedSystem_ = 2;
    }
    static void jumping(Level1State& systems) {
        systems.systemStatusLevels_[2] = -2;
        systems.systemTimers_[2] = 220;
    }
    static bool thirdKillConsumed(const Level1State& systems) {
        return systems.consecutiveDestroyedTypeCount_ == 0;
    }
};
struct GameplayModelTestAccess {
    static std::unique_ptr<GameplayModel> entry(std::uint8_t bank, const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local, std::vector<Level1Entity> actors, const StellarRng& rng) {
        auto model = std::make_unique<GameplayModel>();
        model->playableCampaignFrames_ = true;
        model->initializeRetainedCampaignLevel(bank, std::move(actors), rng, shared, local);
        if (bank == 2) { model->completeLevel3Preloop(); }
        else { model->completePlayableCampaignPreloop(); }
        model->level2ActiveListOrder_ = model->level2AllocationListOrder_;
        return model;
    }
    static std::unique_ptr<GameplayModel> pickup(std::uint8_t bank, const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local, std::uint8_t type) {
        StellarRng rng;
        auto actors = constructCampaignLoaderActors(campaignLevelDefinition(bank, 0).table, rng, shared, local);
        auto model = entry(bank, shared, local, std::move(actors), rng);
        Level1ReloadTestAccess::thirdKill(model->level1_, type);
        Level1Entity source{};
        source.type = type;
        source.position = model->level2Player_.position;
        require(model->registerLevel5DestroyedTypeBonus(source).has_value(), "third kill did not allocate bonus");
        model->level2ActiveListOrder_ = model->level2AllocationListOrder_;
        Level1ReloadTestAccess::domain(model->level1_);
        return model;
    }
    static std::unique_ptr<GameplayModel> target(std::uint8_t bank, const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local) {
        StellarRng rng;
        const std::uint8_t type = bank == 4 ? 5 : 1;
        CampaignOrdinaryConstructor constructor(bank, {type}, shared, local);
        auto actor = constructor.construct(type, rng);
        actor.position = {0, 2000, 0};
        actor.pitch = actor.roll = actor.yaw = 0;
        actor.forwardVelocity = actor.yawVelocity = 0;
        auto model = entry(bank, shared, local, {actor}, rng);
        Level1ReloadTestAccess::thirdKill(model->level1_, actor.type);
        return model;
    }
    static std::unique_ptr<GameplayModel> targets(const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local) {
        StellarRng rng;
        CampaignOrdinaryConstructor constructor(0, {1}, shared, local);
        std::vector<Level1Entity> actors;
        for (int distance : {2000, 5000, 8000}) {
            auto actor = constructor.construct(1, rng);
            actor.position = {0, distance, 0};
            actor.pitch = actor.roll = actor.yaw = 0;
            actor.forwardVelocity = actor.yawVelocity = 0;
            actors.push_back(actor);
        }
        auto model = entry(0, shared, local, std::move(actors), rng);
        model->level2SpawnedEnemyCount_ = campaignLevelDefinition(0, 0).table.enemyLimit;
        return model;
    }
    static void jumping(GameplayModel& model) { Level1ReloadTestAccess::jumping(model.level1_); }
    static void placePickup(GameplayModel& model) {
        for(auto& actor:model.level2Entities_) {
            if(actor.type==42) {actor.position={20000,6000,100};}
        }
        model.snapshotLevel2Presentation({});
    }
    static bool thirdKillConsumed(const GameplayModel& model) {
        return Level1ReloadTestAccess::thirdKillConsumed(model.level1_);
    }
    static void guardianGate(const std::vector<Model3d>& shared, const std::vector<Model3d>& local) {
        constexpr std::int32_t activeListLimit = 65000; // Original3768's per-axis bound.
        for (const auto distance : {activeListLimit - 1, activeListLimit}) {
            StellarRng rng;
            CampaignOrdinaryConstructor constructor(0, {1}, shared, local);
            auto actor = constructor.construct(1, rng);
            actor.position = {distance, 0, 0};
            auto model = entry(0, shared, local, {actor}, rng);
            model->level2SpawnedEnemyCount_ = campaignLevelDefinition(0, 0).table.enemyLimit;
            model->level2Visibility_.resetLevel();
            const auto visible = model->level2Visibility_.refresh({}, model->level2Entities_,
                model->level2AllocationListOrder_, model->level2ActiveListOrder_, VisibilityDomain::playableCampaign);
            model->level2ActiveListOrder_ = visible.activeOrder;
            model->snapshotLevel2Presentation();
            model->runLevel2CompletionCheck();
            const auto craft = std::any_of(model->level2Entities_.begin(), model->level2Entities_.end(),
                [](const auto& value) { return value.type == 7; });
            require(craft == (distance == activeListLimit), "guardian gate used global enemy count instead of completed active list");
            require(model->level2Entities_.front().active() && model->level2Entities_.front().type == 1,
                "guardian gate removed the surviving sandsled");
            std::cout << "guardian gate live sandsled distance=" << distance << " delivery=" << craft << '\n';
        }
    }
    static void excludedTail(const std::vector<Model3d>& shared, const std::vector<Model3d>& local) {
        StellarRng rng;
        CampaignOrdinaryConstructor constructor(0, {1}, shared, local);
        auto actor = constructor.construct(1, rng);
        actor.position = {65000, 0, 0};
        auto model = entry(0, shared, local, {actor}, rng);
        model->level2SpawnedEnemyCount_ = campaignLevelDefinition(0, 0).table.enemyLimit;
        model->advancePlayableCampaign({});
        require(model->level2PresentationActiveListOrder_.empty(), "far sandsled entered the completed active list");
        const auto position = model->level2Entities_.front().position;
        model->finishPlayableCampaignFrame();
        require(model->level2Entities_.front().position == position,
            "frame tail wrapped a sandsled excluded from the completed active list");
        unsigned frame = 1;
        const auto guardian = [&]() {
            return std::any_of(model->level2Entities_.begin(), model->level2Entities_.end(),
                [](const auto& value) { return value.type == 44; });
        };
        for (; frame < 240 && !guardian(); ++frame) {
            model->advancePlayableCampaign({});
            model->finishPlayableCampaignFrame();
            if (!(model->level2Entities_.front().position == position && model->level2Entities_.front().active())) {
                const auto& changed=model->level2Entities_.front();
                std::cout<<"excluded frame="<<frame<<" position="<<changed.position.x<<','<<changed.position.y
                    <<" expected="<<position.x<<','<<position.y<<'\n';
            }
            require(model->level2Entities_.front().position == position && model->level2Entities_.front().active(),
                "retained guardian delivery moved or retired the excluded sandsled");
        }
        require(guardian(), "guardian did not deploy with a retained off-list sandsled alive");
        std::cout << "retained guardian with living sandsled frames=" << frame << '\n';
    }
    static void scene(GameplayModel& model, Level1Entity actor, std::uint8_t variant) {
        actor.modelAnimationVariant = actor.presentationModelAnimationVariant = variant;
        actor.position = {0, 5000, 1000};
        actor.pitch = actor.roll = 0;
        actor.yaw = 0x2000;
        model.phase_ = GameplayPhase::playable;
        model.activeCampaignLevel_ = 2;
        model.playableCampaignFrames_ = true;
        model.level2Player_ = {};
        model.level2Player_.stateBytes4dTo52[1] = 1;
        model.level2PresentationEntities_ = {actor};
        model.level2PresentationActiveListOrder_ = {0};
    }
};
}
namespace {
using namespace stellar7;
const Level1Entity* bonus(const GameplayModel& model) {
    const auto& actors = model.level2Entities();
    const auto found = std::find_if(actors.begin(), actors.end(), [](const auto& actor) { return actor.type == 42; });
    return found == actors.end() ? nullptr : &*found;
}
void collect(const std::filesystem::path& root, const std::vector<Model3d>& shared) {
    constexpr std::array<std::uint8_t, 6> sources{1, 4, 5, 6, 11, 13};
    for (std::uint8_t bank = 0; bank < 7; ++bank) {
        const auto local = models(root, "LEVEL" + std::to_string(bank + 1) + ".RES");
        for (const auto type : sources) {
            auto model = GameplayModelTestAccess::pickup(bank, shared, local, type);
            const auto* pickup = bonus(*model);
            require(pickup, "allocated bonus is absent");
            const auto payload = pickup->stateBytes4dTo52[5];
            const auto score = model->score();
            const auto timers = model->level1State().systemTimers();
            auto status = model->level1State().systemStatusLevels();
            status.at(payload - 1) = 3;
            const auto count = [&]() {
                const auto& events = model->level2AudioEvents();
                return std::count_if(events.begin(), events.end(), [](const auto& event) { return event.eventId == 24; });
            };
            const auto events = count();
            model->advancePlayableCampaign({});
            std::cout << "collect bank=" << unsigned(bank + 1) << " payload=" << unsigned(payload) << '\n';
            require(!bonus(*model), "collected bonus retained live ownership");
            require(model->level1State().systemStatusLevels() == status, "pickup awarded score without repairing its system");
            require(model->level1State().systemTimers() == timers && model->level1State().selectedSystem() == 2,
                "repair reset an unrelated timer or selection");
            require(model->score() == score + 100 && count() == events + 1, "repair score/audio was not once-only");
            model->finishPlayableCampaignFrame();
            model->advancePlayableCampaign({});
            require(model->score() == score + 100 && count() == events + 1, "collected bonus acted twice");
            model->finishPlayableCampaignFrame();
        }
        auto flying = GameplayModelTestAccess::pickup(bank, shared, local, 11);
        GameplayModelTestAccess::jumping(*flying);
        const auto score = flying->score();
        bool rejected = false;
        try { flying->advancePlayableCampaign({}); }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected && bonus(*flying) && flying->score() == score
            && flying->level1State().systemStatusLevels()[2] == -2,
            "unqualified Jump repair was admitted or consumed its pickup");
    }
}
void produce(const std::filesystem::path& root, const std::vector<Model3d>& shared) {
    for (std::uint8_t bank = 0; bank < 7; ++bank) {
        const auto local = models(root, "LEVEL" + std::to_string(bank + 1) + ".RES");
        auto model = GameplayModelTestAccess::target(bank, shared, local);
        for (unsigned frame = 0; frame < 40 && !bonus(*model); ++frame) {
            GameplayInput input;
            input.fire = true;
            model->advancePlayableCampaign(input);
            model->finishPlayableCampaignFrame();
        }
        std::cout << "produce bank=" << unsigned(bank + 1) << '\n';
        require(bonus(*model), "own lethal primary hit omitted third-kill bonus");
        require(bonus(*model)->stateBytes4dTo52[5] == (bank == 4 ? 6 : 7), "third kill selected wrong bonus payload");
        require(bonus(*model)->renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback,
            "bonus lost its retained inline render selector");
        require(GameplayModelTestAccess::thirdKillConsumed(*model)
            && std::count_if(model->level2Entities().begin(),model->level2Entities().end(),
                [](const auto& actor) { return actor.type == 42; }) == 1,
            "lethal hit registered its third kill more than once");
    }
}
void produceChain(const std::filesystem::path& root, const std::vector<Model3d>& shared) {
    const auto assets = naturalRoute::assets(root);
    auto model = GameplayModelTestAccess::targets(shared, assets.campaignLevels.front().models);
    unsigned frame = 0;
    for (; frame < 240 && !bonus(*model); ++frame) {
        GameplayInput input;
        input.fire = true;
        model->advancePlayableCampaign(input);
        model->finishPlayableCampaignFrame();
    }
    std::cout << "retained three-kill chain frames=" << frame << '\n';
    require(bonus(*model), "three own consecutive kills did not produce a bonus");
    require(bonus(*model)->stateBytes4dTo52[5] == 7 && GameplayModelTestAccess::thirdKillConsumed(*model),
        "three own kills selected the wrong payload or retained the kill counter");
#ifdef STELLAR7_ENHANCED
    model->advancePlayableCampaign({});
    IndexedScreen screen;
    { WorldPresentationCapture capture({}, PresentationGeometry::spatial);
      screen = renderPlayableLevel2ViewportIndexed(assets, *model); }
    const auto& surfaces = screen.presentation->chase->spatial->surfaces;
    require(std::any_of(surfaces.begin(), surfaces.end(), [](const auto& surface) { return surface.type == 42; }),
        "retained bonus was omitted from enhanced spatial geometry");
    std::cout << "retained bonus spatial mesh submitted\n";
    model->finishPlayableCampaignFrame();
#endif
}
void pickupMotion(const std::filesystem::path& root,const std::vector<Model3d>& shared) {
    const auto assets=naturalRoute::assets(root);
    std::string failures;
    for(std::uint8_t bank=0;bank<7;++bank) {
        auto model=GameplayModelTestAccess::pickup(bank,shared,assets.campaignLevels[bank].models,4);
        GameplayModelTestAccess::placePickup(*model);
        for(unsigned frame=0;frame<8;++frame) {
            const auto before=*bonus(*model);
            model->advancePlayableCampaign({});
            const auto* after=bonus(*model);
            require(after,"distant bonus disappeared during rotation");
            if(after->yaw!=static_cast<std::uint16_t>(before.yaw+before.yawVelocity)
                ||after->roll!=static_cast<std::uint16_t>(before.roll+before.rollVelocity)) {
                failures+=std::to_string(bank)+",";
                break;
            }
            require(after->stateBytes4dTo52[2]==2&&after->stateBytes4dTo52[5]==2,"shield bonus changed its repair selector");
            model->finishPlayableCampaignFrame();
        }
        std::cout<<"bonus own spin bank="<<unsigned(bank)<<" frames=8\n";
    }
    if(!failures.empty()) {throw std::runtime_error("bonus spin lost its own movement in banks="+failures);}
}
void animation(const std::filesystem::path& root, const std::vector<Model3d>& shared) {
    GameplayAssets assets;
    assets.models = shared;
    assets.campaignLevels.resize(7);
    assets.campaignLevels[2].models = models(root, "LEVEL3.RES");
    assets.cockpit.pixels.assign(320 * 200, 0);
    assets.cockpit.palette.resize(256);
    assets.artGroups.resize(6);
    assets.artGroups[5].resize(12, {8, 8, std::vector<std::uint8_t>(64, 231)});
    CampaignOrdinaryConstructor constructor(2, {11, 12}, shared, assets.campaignLevels[2].models);
    StellarRng rng;
    for (const std::uint8_t type : {11, 12}) {
        const auto actor = constructor.construct(type, rng);
        auto model = GameplayModelTestAccess::entry(2,shared,assets.campaignLevels[2].models,{actor},rng);
        require(actor.animatedModelPresenterEnabled && actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation,
            "constructor disabled the ordinary heap animation callback");
        GameplayModelTestAccess::scene(*model, actor, 0);
        const auto first = renderPlayableLevel2ViewportIndexed(assets, *model);
        GameplayModelTestAccess::scene(*model, actor, 1);
        const auto next = renderPlayableLevel2ViewportIndexed(assets, *model);
        std::cout << "animation type=" << unsigned(type) << '\n';
        require(first.pixels != next.pixels, "ordinary animated mesh ignored its heap selector");
#ifdef STELLAR7_ENHANCED
        WorldPresentationCapture enabled;
        const auto captured = renderPlayableLevel2ViewportIndexed(assets, *model);
        require(captured.pixels == next.pixels && captured.presentation, "animation capture changed indexed authority");
        require(std::any_of(captured.presentation->draws.begin(), captured.presentation->draws.end(),
            [type](const auto& draw) {
                return draw.face && draw.face->identity && draw.face->identity->type == type && draw.face->identity->variant == 1;
            }), "enhanced geometry ignored ordinary animation selector");
#endif
        auto retainedActor = actor;
        retainedActor.position = {0, 12000, 1000};
        retainedActor.forwardVelocity = retainedActor.yawVelocity = 0;
        auto retained = GameplayModelTestAccess::entry(2,shared,assets.campaignLevels[2].models,{retainedActor},rng);
        std::uint8_t selector = 0, interval = 0;
        const auto callback = [&]() {
            if (interval != 0) { --interval; return; }
            interval = 5; // Original441D interval and130C countdown, main plus radar.
            selector = static_cast<std::uint8_t>((selector + 1) % shared.at(type).animationLimits.at(0));
        };
        for (unsigned frame = 0; frame < 8; ++frame) {
            if (frame != 0) { callback(); } // Previous radar settles at the next prefix.
            const auto presented = selector;
            callback();
            retained->advancePlayableCampaign({});
            const auto& live = retained->level2Entities().front();
            require(live.type == type && live.modelAnimationVariant == selector
                && live.presentationModelAnimationVariant == presented && live.stateBytes4dTo52[3] == interval,
                "ordinary animation advanced at the wrong main/radar cadence");
            const auto calls = retained->level2Rng().calls();
            renderPlayableLevel2ViewportIndexed(assets,*retained);
            renderPlayableLevel2ViewportIndexed(assets,*retained);
            require(live.modelAnimationVariant == selector && live.stateBytes4dTo52[3] == interval
                && retained->level2Rng().calls() == calls,"pixel observation reran animation or RNG");
            retained->finishLevel3Radar();
            retained->finishPlayableCampaignFrame();
        }
    }
}
}
int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) { return 2; }
    try {
        const auto shared = models(argv[1], "COCKPIT.RES");
        const std::string mode = argc == 3 ? argv[2] : "all";
        if (mode == "all" || mode == "--animation") { animation(argv[1], shared); }
        if (mode == "all" || mode == "--produce") { produce(argv[1], shared); }
        if (mode == "all" || mode == "--chain") { produceChain(argv[1], shared); }
        if (mode == "all" || mode == "--pickup-motion") { pickupMotion(argv[1], shared); }
        if (mode == "all" || mode == "--guardian-gate") {
            GameplayModelTestAccess::guardianGate(shared, models(argv[1], "LEVEL1.RES"));
        }
        if (mode == "all" || mode == "--excluded-tail") {
            GameplayModelTestAccess::excludedTail(shared, models(argv[1], "LEVEL1.RES"));
        }
        if (mode == "all" || mode == "--collect") { collect(argv[1], shared); }
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
