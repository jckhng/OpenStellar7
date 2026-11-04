#include "failure_page.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace {
using namespace stellar7;
constexpr Viewport backgroundClip{0, 319, 0, 199};
constexpr Viewport modelClip{0, 318, 0, 199};
void require(const bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(static_cast<bool>(file), "missing immutable failure bitmap resource");
    return {std::istreambuf_iterator<char>(file), {}};
}
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "malformed or strict destruction bitmap did not reject");
}
void sameRng(StellarRng left, StellarRng right) {
    require(left.calls() == right.calls() && left.indexA() == right.indexA() && left.indexB() == right.indexB(),
        "bitmap renderer changed RNG cursors");
    for (std::size_t index = 0; index < StellarRng::wordCount; ++index) {
        require(left.next() == right.next(), "bitmap renderer changed RNG words");
    }
}
} //namespace

namespace stellar7 {
struct GameplayModelTestAccess {
    static void fixture(GameplayModel& model, const FixedPosition center) {
        model.phase_ = GameplayPhase::failureExplosion;
        model.activeCampaignLevel_ = 3;
        model.failureFromLevel2_ = true;
        model.presentationFrame_ = 0;
        model.failureView_ = {{}, makeFixedMatrixQ14(0, 0, 0), modelClip, 128};
        Level1Entity effect{};
        effect.type = -1;
        effect.flags = 0x88;
        effect.position = center;
        effect.stateBytes4dTo52 = {0xfc, 15, 0, 0, 17, 0};
        effect.renderProfile = WorldActorRenderProfile::bitmapEffectCallback;
        effect.animatedModelPresenterEnabled = false;
        model.failureMissionEntities_ = {effect};
        model.failureVisibleItems_ = {{FailureVisibleKind::mission, 0}};
    }
    static Level1Entity& effect(GameplayModel& model) { return model.failureMissionEntities_.at(0); }
    static void removeEffect(GameplayModel& model) {
        model.failureMissionEntities_.clear();
        model.failureVisibleItems_.clear();
    }
};
} //namespace stellar7

namespace {
IndexedScreen expectedOverlay(IndexedScreen baseline, const IndexedBitmap& bitmap, const ProjectedPoint anchor) {
    const auto left = anchor.x - bitmap.height / 2;
    const auto top = anchor.y - bitmap.height / 2;
    for (unsigned row = 0; row < bitmap.height; ++row) {
        for (unsigned column = 0; column < bitmap.width; ++column) {
            const auto x = left + static_cast<int>(column);
            const auto y = top + static_cast<int>(row);
            const auto color = bitmap.pixels[row * bitmap.width + column];
            if (color == 0 || x < modelClip.left || x > modelClip.right || y < modelClip.top || y > modelClip.bottom) {
                continue;
            }
            baseline.pixels[y * 320 + x] = color;
        }
    }
    return baseline;
}

void pixels(const GameplayAssets& assets, const FixedPosition center, const ProjectedPoint anchor) {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::fixture(*model, center);
    auto blank = std::make_unique<GameplayModel>(*model);
    GameplayModelTestAccess::removeEffect(*blank);
    IndexedScreen destination{std::vector<std::uint8_t>(64000, 77), assets.cockpit.palette};
    rejects([&] { (void)renderFailurePageIndexed(assets, *model, destination, backgroundClip); });
    const auto before = model->failureMissionEntities()[0];
    const auto random = model->failureRng();
    const auto ticks = model->ticks();
    const auto baseline = renderPlayableFailurePageIndexed(assets, *blank, destination, backgroundClip);
    const auto result = renderPlayableFailurePageIndexed(assets, *model, destination, backgroundClip);
    const auto& after = model->failureMissionEntities()[0];
    require(after.type == before.type && after.flags == before.flags && after.position == before.position
        && after.stateBytes4dTo52 == before.stateBytes4dTo52 && after.collisionRadius == before.collisionRadius
        && after.renderProfile == before.renderProfile && model->ticks() == ticks,
        "bitmap renderer changed its model-owned callback record");
    sameRng(model->failureRng(), random);
    if (center.y < 128 || anchor.x < modelClip.left || anchor.x > modelClip.right
        || anchor.y < modelClip.top || anchor.y > modelClip.bottom) {
        require(result.pixels == baseline.pixels, "unprojected FC painted bitmap pixels");
        return;
    }
    const auto expected = expectedOverlay(baseline, assets.artGroups.at(5).at(15), anchor);
    require(expected.pixels != baseline.pixels, "FC pixel regression would accept silently deleting the actor");
    require(result.pixels == expected.pixels, "FC pixels differ from independent height-centered/clipped overlay");
}
} //namespace

int main(int argc, char** argv) {
    if (argc != 2) { return 2; }
    try {
        const std::filesystem::path root(argv[1]);
        GameplayAssets assets{};
        assets.artGroups = decodeBitmapGroups(read(root / "STELART.RES"));
        assets.cockpit.palette = decodePalettes(read(root / "STELART.RES")).back();
        assets.campaignLevels.resize(4);
        assets.campaignLevels[3].bitmapGroups = decodeBitmapGroups(read(root / "LEVEL4.RES"));
        require(assets.artGroups.at(5).at(15).width == 96 && assets.artGroups.at(5).at(15).height == 68,
            "actual frame15 must expose asymmetric centering");
        pixels(assets, {0, 2048, 0}, {159, 99});
        pixels(assets, {2544, 2048, -1600}, {318, 199});
        pixels(assets, {0, 127, 0}, {159, 99});
        pixels(assets, {2560, 2048, 0}, {319, 99});
        auto model = std::make_unique<GameplayModel>();
        GameplayModelTestAccess::fixture(*model, {0, 2048, 0});
        IndexedScreen destination{std::vector<std::uint8_t>(64000, 77), assets.cockpit.palette};
        const auto valid = GameplayModelTestAccess::effect(*model);
        const auto malformed = [&](const auto mutate) {
            GameplayModelTestAccess::effect(*model) = valid;
            mutate(GameplayModelTestAccess::effect(*model));
            rejects([&] { (void)renderPlayableFailurePageIndexed(assets, *model, destination, backgroundClip); });
        };
        malformed([](auto& effect) { effect.type = -2; });
        malformed([](auto& effect) { effect.stateBytes4dTo52[0] = 0xff; });
        malformed([](auto& effect) { effect.collisionRadius = 1; });
        malformed([](auto& effect) { effect.flags = 0; });
        malformed([](auto& effect) { effect.renderProfile = WorldActorRenderProfile::unknown; });
        malformed([](auto& effect) { effect.stateBytes4dTo52[1] = 0; });
        malformed([](auto& effect) { effect.stateBytes4dTo52[4] = 18; });
        GameplayModelTestAccess::effect(*model) = valid;
        auto badAssets = assets;
        badAssets.artGroups[5][15].pixels.pop_back();
        rejects([&] { (void)renderPlayableFailurePageIndexed(badAssets, *model, destination, backgroundClip); });
        badAssets.artGroups[5].resize(15);
        rejects([&] { (void)renderPlayableFailurePageIndexed(badAssets, *model, destination, backgroundClip); });
        GameplayModelTestAccess::effect(*model).flags = 0x8c;
        (void)renderPlayableFailurePageIndexed(assets, *model, destination, backgroundClip);
        std::cout << "Declared retained FC: strict rejection, actual frame15 pixels, height centering, clipping and malformed guards PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
