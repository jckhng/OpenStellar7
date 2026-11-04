#include "stellar7/gameplay.hpp"
#include "ordinary_behavior.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace stellar7 {
struct GameplayModelTestAccess {
    static void scene(GameplayModel& model, std::vector<Level1Entity> actors) {
        //Declared renderer component entry; no campaign progression ancestry.
        model.phase_ = GameplayPhase::playable;
        model.activeCampaignLevel_ = 6;
        model.playableCampaignFrames_ = true;
        model.level2Player_ = {};
        model.level2Player_.stateBytes4dTo52[1] = 1;
        model.level2PresentationEntities_ = std::move(actors);
        model.level2PresentationActiveListOrder_.clear();
        for (std::size_t index = 0; index < model.level2PresentationEntities_.size(); ++index) {
            model.level2PresentationActiveListOrder_.push_back(index);
        }
    }
};
}

namespace {
using namespace stellar7;
constexpr std::uint8_t hiddenMeshFlag = 0x80;
constexpr std::uint8_t effectColor = 231;

void require(const bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<Model3d> models(const std::filesystem::path& root, const std::string& name) {
    std::ifstream file(root / name,std::ios::binary);
    require(static_cast<bool>(file), "missing original model resource");
    return decodeFirstModelTableResource({std::istreambuf_iterator<char>(file),{}});
}

void visibility(const std::filesystem::path& root) {
    GameplayAssets assets;
    assets.models = models(root,"COCKPIT.RES");
    assets.campaignLevels.resize(7);
    assets.campaignLevels[6].models = models(root,"LEVEL7.RES");
    assets.cockpit.pixels.assign(320 * 200,0);
    assets.cockpit.palette.resize(256);
    assets.artGroups.resize(6);
    assets.artGroups[5].resize(12,{8,8,std::vector<std::uint8_t>(64,effectColor)});

    StellarRng rng;
    auto actors = constructCampaignLoaderActors(campaignLevelDefinition(6,0).table,rng,
        assets.models,assets.campaignLevels[6].models);
    const auto tank = std::find_if(actors.begin(),actors.end(),[](const auto& actor) { return actor.type == 6; });
    require(tank != actors.end(), "actual Level7 loader lost Type6");
    auto actor = *tank;
    actor.position = {0,8000,0};
    actor.pitch = actor.roll = actor.yaw = 0;
    actor.type6CloakTimer = 200;
    require(actor.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback
        && !actor.animatedModelPresenterEnabled && (actor.flags & hiddenMeshFlag) == 0,
        "Type6 component lost its scanner-inactive inline profile");

    auto model = std::make_unique<GameplayModel>();
    const auto draw = [&](const std::vector<Level1Entity>& scene) {
        GameplayModelTestAccess::scene(*model,scene);
        return renderLevel2ViewportIndexed(assets,*model).pixels;
    };
    const auto empty = draw({});
    const auto visible = draw({actor});
    require(visible != empty, "visible actual Type6 mesh did not paint the component viewport");

    const auto fire = [](Level1Entity&) { return false; };
    advanceType6Behavior(actor,rng,{actor.type6CloakTimer,6,Type6ScannerState::inactive},fire);
    require((actor.flags & hiddenMeshFlag) != 0 && actor.type6CloakTimer == 199,
        "actual Type6 callback did not enter its cloak blink");
    require(draw({actor}) == empty, "cloaked Type6 still painted its main mesh");

    advanceType6Behavior(actor,rng,{actor.type6CloakTimer,6,Type6ScannerState::inactive},fire);
    require((actor.flags & hiddenMeshFlag) == 0 && draw({actor}) == visible,
        "uncloaked same Type6 did not restore its main mesh");

    Level1Entity effect{};
    effect.type = -1;
    effect.flags = 0x88;
    effect.position = actor.position;
    effect.stateBytes4dTo52 = {0xfc,2,0,0,12,0};
    effect.renderProfile = WorldActorRenderProfile::bitmapEffectCallback;
    const auto bitmap = draw({effect});
    require(std::count(bitmap.begin(),bitmap.end(),effectColor) > 0,
        "FC88 bitmap dispatch was incorrectly skipped with hidden mesh geometry");
    std::cout << "Actual Type6 scanner-inactive visible/cloaked/uncloaked main mesh; FC88 bitmap preserved\n";
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    try {
        visibility(argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
