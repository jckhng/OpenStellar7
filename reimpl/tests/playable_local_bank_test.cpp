#include "stellar7/gameplay.hpp"

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
