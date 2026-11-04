#include "stellar7/gameplay.hpp"
#include "failure_page.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace {
using namespace stellar7;
void require(const bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
std::vector<Model3d> models(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(static_cast<bool>(file), "missing immutable local model resource");
    return decodeFirstModelTableResource({std::istreambuf_iterator<char>(file), {}});
}
IndexedScreen playableViewport(const GameplayAssets& assets, const GameplayModel& model) {
    return renderPlayableLevel2ViewportIndexed(assets, model);
}
} //namespace

namespace stellar7 {
struct GameplayModelTestAccess {
    static void scene(GameplayModel& model, const std::uint8_t level, const GameplayPhase phase,
        const std::uint32_t radius) {
        //Declared renderer component; no gameplay/guardian-constructor ancestry.
        model.phase_ = phase;
        model.activeCampaignLevel_ = level;
        model.playableCampaignFrames_ = phase == GameplayPhase::playable;
        model.level2Player_ = {};
        Level1Entity guardian{};
        guardian.type = 44;
        guardian.position = {0, 8000, 0};
        guardian.collisionRadius = radius;
        guardian.stateBytes4dTo52 = {0xff, 20, 2, 0, 5, 0};
        guardian.animatedModelPresenterEnabled = false;
        guardian.renderProfile = WorldActorRenderProfile::heapSelectorNoCallback;
        model.level2PresentationEntities_ = {guardian};
        model.level2PresentationActiveListOrder_ = {0};
    }
    static void empty(GameplayModel& model) {
        model.level2PresentationEntities_.clear();
        model.level2PresentationActiveListOrder_.clear();
    }
    static void pendingFailure(GameplayModel& model) {
        model.phase_ = GameplayPhase::failurePreExplosionBlackout;
        model.failureFromLevel2_ = true;
        model.level2HasAdvanced_ = true;
        model.level2TailConsumed_ = false;
        model.level2FailureTailFinished_ = false;
        model.level2PendingInput_ = {};
    }
    static void failure(GameplayModel& model) {
        model.phase_ = GameplayPhase::failureExplosion;
        model.failureFromLevel2_ = true;
        model.failureView_ = {{}, makeFixedMatrixQ14(0, 0, 0), {0, 318, 0, 199}, 128};
        model.failureMissionEntities_ = model.level2PresentationEntities_;
        model.failureVisibleItems_ = {{FailureVisibleKind::mission, 0}};
    }
    static void emptyFailure(GameplayModel& model) {
        model.failureMissionEntities_.clear();
        model.failureVisibleItems_.clear();
    }
    static void hideFailureMesh(GameplayModel& model) {
        model.failureMissionEntities_.at(0).flags |= 0x80;
    }
};
} //namespace stellar7

namespace {
void bank(const std::filesystem::path& root, const std::uint8_t level) {
    GameplayAssets assets{};
    assets.models = models(root / "COCKPIT.RES");
    const auto sharedCount = assets.models.size();
    require(sharedCount == 44, "test requires actual shared44 registry");
    const auto firstBank = models(root / "LEVEL1.RES");
    assets.models.insert(assets.models.end(), firstBank.begin(), firstBank.end());
    assets.campaignLevels.resize(7);
    assets.campaignLevels[level].models = models(root / ("LEVEL" + std::to_string(level + 1) + ".RES"));
    assets.level2Models = models(root / "LEVEL2.RES");
    assets.level3Models = models(root / "LEVEL3.RES");
    assets.cockpit.pixels.assign(64000, 0);
    assets.cockpit.palette.resize(256);
    const auto legacyPhase = level == 1 ? GameplayPhase::level2 : GameplayPhase::level3Native;
    const auto radius = assets.campaignLevels[level].models.at(0).collisionRadius;
    require(radius > 0, "actual local44 radius missing");
    auto legacy = std::make_unique<GameplayModel>();
    auto generic = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::scene(*legacy, level, legacyPhase, radius);
    GameplayModelTestAccess::scene(*generic, level, GameplayPhase::playable, radius);
    auto blank = std::make_unique<GameplayModel>(*generic);
    GameplayModelTestAccess::empty(*blank);
    const auto empty = renderLevel2ViewportIndexed(assets, *blank).pixels;
    const auto expected = renderLevel2ViewportIndexed(assets, *legacy).pixels;
    require(expected != empty, "actual qualified legacy local44 mesh painted no pixels");
    require(renderLevel2ViewportIndexed(assets, *generic).pixels == expected,
        "generic playable local44 differs from the actual Level2/3 legacy bank");

    auto fallbackPoison = assets;
    fallbackPoison.models.at(sharedCount) = {};
    require(renderLevel2ViewportIndexed(fallbackPoison, *legacy).pixels == expected,
        "legacy local bank unexpectedly depended on Level1 fallback");
    require(renderLevel2ViewportIndexed(fallbackPoison, *generic).pixels == expected,
        "generic playable local44 still reads the Level1 fallback bank");

    auto currentBankPoison = assets;
    currentBankPoison.campaignLevels[level].models.at(0) = {};
    require(renderLevel2ViewportIndexed(currentBankPoison, *legacy).pixels == expected,
        "source default local-bank lookup changed");
    require(renderLevel2ViewportIndexed(currentBankPoison, *generic).pixels == empty,
        "generic playable lookup did not bind its current campaign bank");

    GameplayModelTestAccess::pendingFailure(*generic);
    auto pendingReference = assets;
    pendingReference.models[sharedCount] = assets.campaignLevels[level].models[0];
    //Same lethal phase/background; independently replace only the known fallback bank.
    const auto pendingExpected = renderLevel2ViewportIndexed(pendingReference, *generic).pixels;
    auto pendingBlank = std::make_unique<GameplayModel>(*generic);
    GameplayModelTestAccess::empty(*pendingBlank);
    const auto pendingEmpty = playableViewport(assets, *pendingBlank).pixels;
    require(pendingExpected != pendingEmpty, "actual lethal MAIN local44 mesh painted no pixels");
    require(playableViewport(assets, *generic).pixels == pendingExpected,
        "private playable lethal MAIN used the wrong local bank");
    require(playableViewport(fallbackPoison, *generic).pixels == pendingExpected,
        "private playable lethal MAIN still reads fallback44");
    require(playableViewport(currentBankPoison, *generic).pixels == pendingEmpty,
        "private playable lethal MAIN did not bind the current bank");
    require(renderLevel2ViewportIndexed(currentBankPoison, *generic).pixels
        == renderLevel2ViewportIndexed(assets, *generic).pixels,
        "strict lethal MAIN bank criteria changed");

    GameplayModelTestAccess::failure(*generic);
    auto emptyFailure = std::make_unique<GameplayModel>(*generic);
    GameplayModelTestAccess::emptyFailure(*emptyFailure);
    constexpr Viewport failureClip{0, 319, 0, 199};
    IndexedScreen destination{std::vector<std::uint8_t>(64000, 77), assets.cockpit.palette};
    auto referenceAssets = assets;
    referenceAssets.models[sharedCount] = assets.campaignLevels[level].models[0];
    //Independent binding control: public legacy failure consumes only fallback44.
    const auto failureExpected = renderFailurePageIndexed(referenceAssets, *generic, destination, failureClip).pixels;
    const auto failureEmpty = renderFailurePageIndexed(assets, *emptyFailure, destination, failureClip).pixels;
    require(failureExpected != failureEmpty, "actual local failure mesh painted no pixels");
    require(renderPlayableFailurePageIndexed(assets, *generic, destination, failureClip).pixels == failureExpected,
        "private playable failure used the Level1 fallback bank");
    require(renderPlayableFailurePageIndexed(fallbackPoison, *generic, destination, failureClip).pixels == failureExpected,
        "private playable failure still depends on global fallback44");
    require(renderPlayableFailurePageIndexed(currentBankPoison, *generic, destination, failureClip).pixels == failureEmpty,
        "private playable failure is not bound to the current bank");
    require(renderFailurePageIndexed(currentBankPoison, *generic, destination, failureClip).pixels
        == renderFailurePageIndexed(assets, *generic, destination, failureClip).pixels,
        "strict source failure bank lookup changed");
    require(renderFailurePageIndexed(fallbackPoison, *generic, destination, failureClip).pixels == failureEmpty,
        "strict source failure no longer retains its legacy fallback criteria");
    GameplayModelTestAccess::hideFailureMesh(*generic);
    require(renderPlayableFailurePageIndexed(assets, *generic, destination, failureClip).pixels == failureEmpty,
        "explicit playable destruction ignored the CB7A flags80 mesh gate");
    require(renderPlayableFailurePageIndexed(fallbackPoison, *generic, destination, failureClip).pixels == failureEmpty,
        "hidden destruction mesh fell back to another geometry bank");
    require(renderFailurePageIndexed(referenceAssets, *generic, destination, failureClip).pixels == failureExpected,
        "playable-only CB7A admission changed strict failure defaults");
    std::cout << "Declared generic Level" << unsigned(level + 1)
        << " local44: actual legacy pixels, fallback poison and current-bank binding PASS\n";
}
} //namespace

int main(int argc, char** argv) {
    if (argc != 2) { return 2; }
    try {
        for (const std::uint8_t level : {1, 2}) { bank(argv[1], level); }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
