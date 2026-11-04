#include "failure_page.hpp"
#include "stellar7/failure_effect_work.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <tuple>

namespace {
using namespace stellar7;
constexpr Viewport modelClip{0, 318, 0, 199};
constexpr Viewport backgroundClip{0, 319, 0, 199};
constexpr std::size_t effectIndex = 0;
constexpr std::size_t effectSlot = 2;
enum class CallbackDomain { source, playable };

void require(const bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

void sameRng(StellarRng left, StellarRng right) {
    require(left.calls() == right.calls() && left.indexA() == right.indexA()
        && left.indexB() == right.indexB(), "destruction callback changed RNG cursors");
    for (std::size_t index = 0; index < StellarRng::wordCount; ++index) {
        require(left.next() == right.next(), "destruction callback changed RNG words");
    }
}

auto record(const Level1Entity& actor) {
    return std::tie(actor.allocationIndex, actor.flags, actor.type, actor.collisionRadius, actor.position.x,
        actor.position.y, actor.position.z, actor.pitch, actor.roll, actor.yaw,
        actor.pitchVelocity, actor.rollVelocity, actor.yawVelocity, actor.xVelocity,
        actor.yVelocity, actor.zVelocity, actor.forwardVelocity, actor.stateBytes4dTo52,
        actor.modelAnimationVariant, actor.primaryType11AnimationSelector, actor.behaviorTimer,
        actor.type12MissGate, actor.type6CloakTimer, actor.presenterDistance, actor.targetHeading,
        actor.weaponSlotCount, actor.weaponCooldown, actor.weaponSlotTimers,
        actor.collisionContactAllocationIndex, actor.collisionContactIsPlayer,
        actor.presentationModelAnimationVariant, actor.animatedModelPresenterEnabled,
        actor.ownerIsPlayer, actor.ownerAllocationIndex, actor.renderProfile);
}

std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "missing immutable effect resource");
    return {std::istreambuf_iterator<char>(input), {}};
}
} //namespace

namespace stellar7 {
struct GameplayModelTestAccess {
    static void fixture(GameplayModel& model, const FixedPosition center, const CallbackDomain domain,
        const Model3d& type11, const Model3d& type1) {
        //Declared callback component: retained canonical FC and two existing FF owners.
        //No natural lethal ancestry, constructor capacity or renderer CPU claim.
        model.phase_ = GameplayPhase::failureExplosion;
        model.activeCampaignLevel_ = 3;
        model.playableCampaignFrames_ = domain == CallbackDomain::playable;
        model.failureFromLevel2_ = true;
        model.failureView_ = {{}, makeFixedMatrixQ14(0, 0, 0), modelClip, 128};
        Level1Entity effect{};
        effect.allocationIndex = effectIndex;
        effect.type = -1;
        effect.flags = 0x88;
        effect.position = center;
        effect.stateBytes4dTo52 = {0xfc, 15, 0, 0, 17, 0};
        effect.renderProfile = WorldActorRenderProfile::bitmapEffectCallback;
        effect.animatedModelPresenterEnabled = false;
        Level1Entity staticActor{};
        staticActor.allocationIndex = 1;
        staticActor.type = 11;
        staticActor.collisionRadius = type11.collisionRadius;
        staticActor.stateBytes4dTo52 = {0xff, 3, 5, 1, 0, 0};
        staticActor.animatedModelPresenterEnabled = true;
        staticActor.renderProfile = WorldActorRenderProfile::heapSelectorDeferredAnimation;
        auto last = staticActor;
        last.allocationIndex = 2;
        last.type = 1;
        last.collisionRadius = type1.collisionRadius;
        last.stateBytes4dTo52 = {0xff, 1, 0, 0, 0, 0};
        last.animatedModelPresenterEnabled = false;
        last.renderProfile = WorldActorRenderProfile::noHandles;
        model.campaignModelsBound_ = true;
        model.campaignModels_[11].animationLimit = type11.animationLimits.at(0);
        model.level2Entities_ = {effect, staticActor, last};
        model.level2EntityDeathEffectPoolSlots_ = {effectSlot, model.level2DeathEffectPoolSize_, model.level2DeathEffectPoolSize_};
        model.level2EntityActorPoolSlots_ = {model.level2ActorPoolSize_, 0, 1};
        model.level2EntityProjectilePoolSlots_.assign(3, model.level2ProjectilePoolSize_);
        model.level2EntityFixedPoolSlots_.assign(3, model.level2FixedObjectPoolSize_);
        model.level2DeathEffectPool_[effectSlot] = effect;
        model.level2DeathEffectPoolActive_[effectSlot] = true;
        model.level2ActorPool_[0] = staticActor;
        model.level2ActorPool_[1] = last;
        model.level2ActorPoolActive_[0] = model.level2ActorPoolActive_[1] = true;
        model.level2AllocationListOrder_ = model.level2ObjectListOrder_ = model.level2ActiveListOrder_ = {0, 1, 2};
        model.failureMissionEntities_ = model.level2Entities_;
        model.failureVisibleItems_ = {{FailureVisibleKind::mission, 0}};
    }

    static void display(GameplayModel& model, const unsigned ordinal) {
        model.presentationFrame_ = ordinal;
        model.rebuildFailureVisibleItems(ordinal);
        model.advanceFailureMissionAnimation();
    }
    static const Level1Entity& live(const GameplayModel& model) { return model.level2Entities_[effectIndex]; }
    static const Level1Entity& physical(const GameplayModel& model) { return model.level2DeathEffectPool_[effectSlot]; }
    static bool physicalActive(const GameplayModel& model) { return model.level2DeathEffectPoolActive_[effectSlot]; }
    static const auto& allocations(const GameplayModel& model) { return model.level2AllocationListOrder_; }
    static void heap(const GameplayModel& model, const std::uint8_t selected,
        const std::uint8_t countdown, const std::uint8_t drawn) {
        const auto& source = model.level2Entities_[1];
        require(source.modelAnimationVariant == selected && source.stateBytes4dTo52[3] == countdown
            && source.presentationModelAnimationVariant == drawn
            && model.failureMissionEntities_[1].presentationModelAnimationVariant == drawn,
            "destruction heap did not snapshot then execute exactly one MAIN130C");
        require(record(source) == record(model.level2ActorPool_[0]),
            "destruction heap animation diverged from its actual FF owner");
    }
    static void onlyEffect(GameplayModel& model) {
        auto& items = model.failureVisibleItems_;
        items.erase(std::remove_if(items.begin(), items.end(), [](const auto& item) {
            return item.kind != FailureVisibleKind::mission || item.index != effectIndex;
        }), items.end());
    }
    static void blank(GameplayModel& model) { model.failureVisibleItems_.clear(); }
};
} //namespace stellar7

namespace {
void callback(const GameplayAssets& assets, const std::vector<Model3d>& shared, const FixedPosition center) {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::fixture(*model, center, CallbackDomain::playable, shared.at(11), shared.at(1));
    const auto random = model->failureRng();
    const auto worldRandom = model->level2Rng();
    const auto ticks = model->ticks();
    const auto sourceBefore = GameplayModelTestAccess::live(*model);
    const auto oracle = failureEffectOperations({center, 0,
        {0x88, {}, 0, 15, 17, 0x0ac8}, modelClip, {159, 99}, 0xa000, 3, effectIndex},
        assets.artGroups.at(5));
    require(center.y < 128 || !oracle.pixels.empty(), "visible FC oracle would accept deleting the bitmap");
    GameplayModelTestAccess::display(*model, 0);
    GameplayModelTestAccess::heap(*model, 0, 0, 0);
    require(model->failureMissionEntities()[effectIndex].stateBytes4dTo52[1] == 15,
        "first destruction snapshot lost incoming FC frame15");
    require(GameplayModelTestAccess::live(*model).stateBytes4dTo52[1] == oracle.state.age
        && oracle.state.age == 16 && !oracle.retired && GameplayModelTestAccess::physicalActive(*model),
        "first destruction callback did not increment its canonical FC owner");
    require(record(GameplayModelTestAccess::physical(*model)) == record(GameplayModelTestAccess::live(*model)),
        "destruction FC logical/physical aliases diverged");
    require(GameplayModelTestAccess::live(*model).position == sourceBefore.position,
        "failure snapshot coordinates overwrote canonical world position");

    //Pixel oracle is the already-qualified source-work operation at the OLD age.
    GameplayModelTestAccess::onlyEffect(*model);
    auto blank = std::make_unique<GameplayModel>(*model);
    GameplayModelTestAccess::blank(*blank);
    IndexedScreen destination{std::vector<std::uint8_t>(64000, 77), assets.cockpit.palette};
    auto expected = renderPlayableFailurePageIndexed(assets, *blank, destination, backgroundClip);
    for (const auto& pixel : oracle.pixels) {
        expected.pixels[static_cast<std::size_t>(pixel.point.y) * 320 + pixel.point.x] = pixel.color;
    }
    const auto beforePaint = GameplayModelTestAccess::live(*model);
    require(renderPlayableFailurePageIndexed(assets, *model, destination, backgroundClip).pixels == expected.pixels,
        "destruction painter used incremented live age instead of source incoming age");
    require(record(beforePaint) == record(GameplayModelTestAccess::live(*model)),
        "pixel painter repeated the model callback");

    GameplayModelTestAccess::display(*model, 1);
    GameplayModelTestAccess::heap(*model, 1, 5, 0);
    require(model->failureMissionEntities()[effectIndex].stateBytes4dTo52[1] == 16,
        "terminal destruction display lost old frame16");
    const auto& retired = GameplayModelTestAccess::live(*model);
    require(retired.stateBytes4dTo52[1] == 0 && retired.stateBytes4dTo52[4] == 0
        && retired.renderProfile == WorldActorRenderProfile::noHandles
        && !GameplayModelTestAccess::physicalActive(*model), "terminal FC did not clear callback/age/end in its actual pool slot");
    require(record(GameplayModelTestAccess::physical(*model)) == record(retired),
        "terminal FC pool clear diverged from its retained logical owner");
    require(GameplayModelTestAccess::allocations(*model) == std::vector<std::size_t>{2, 1},
        "FC481A retirement did not swap-remove allocation membership");
    require(model->failureMissionEntities()[effectIndex].renderProfile == WorldActorRenderProfile::bitmapEffectCallback,
        "canonical callback retirement destroyed the current OLD presentation");
    GameplayModelTestAccess::onlyEffect(*model);
    blank = std::make_unique<GameplayModel>(*model);
    GameplayModelTestAccess::blank(*blank);
    expected = renderPlayableFailurePageIndexed(assets, *blank, destination, backgroundClip);
    const auto terminalOracle = failureEffectOperations({center, 0,
        {0x88, {}, 0, 16, 17, 0x0ac8}, modelClip, {159, 99}, 0xa000, 3, effectIndex},
        assets.artGroups.at(5));
    require(terminalOracle.retired && terminalOracle.state.callback == 0,
        "source callback oracle did not qualify terminal retirement");
    for (const auto& pixel : terminalOracle.pixels) {
        expected.pixels[static_cast<std::size_t>(pixel.point.y) * 320 + pixel.point.x] = pixel.color;
    }
    require(renderPlayableFailurePageIndexed(assets, *model, destination, backgroundClip).pixels == expected.pixels,
        "canonical inactive FC suppressed its final OLD-age bitmap delivery");
    require(record(GameplayModelTestAccess::physical(*model)) == record(retired),
        "terminal pixel delivery repeated callback/pool writes");
    GameplayModelTestAccess::display(*model, 2);
    GameplayModelTestAccess::heap(*model, 1, 4, 1);
    require(std::none_of(model->failureVisibleItems().begin(), model->failureVisibleItems().end(),
        [](const auto& item) { return item.kind == FailureVisibleKind::mission && item.index == effectIndex; }),
        "next destruction rebuild retained its retired FC");
    require(model->ticks() == ticks, "display callback introduced world ticks");
    sameRng(model->failureRng(), random);
    sameRng(model->level2Rng(), worldRandom);

    auto strict = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::fixture(*strict, center, CallbackDomain::source, shared.at(11), shared.at(1));
    GameplayModelTestAccess::display(*strict, 0);
    require(GameplayModelTestAccess::live(*strict).stateBytes4dTo52[1] == 15,
        "playable callback proposal changed strict source behavior");
}
} //namespace

int main(int argc, char** argv) {
    if (argc != 2) { return 2; }
    try {
        GameplayAssets assets{};
        assets.cockpit.pixels.resize(64000);
        assets.cockpit.palette.resize(256);
        assets.artGroups = decodeBitmapGroups(read(std::filesystem::path(argv[1]) / "STELART.RES"));
        assets.campaignLevels.resize(7);
        const auto shared = decodeFirstModelTableResource(read(std::filesystem::path(argv[1]) / "COCKPIT.RES"));
        require(shared.at(11).flags == 1 && shared.at(11).animationLimits == std::vector<std::uint8_t>{8},
            "test requires actual shared Type11 single-selector metadata");
        callback(assets, shared, {0, 2048, 0});
        callback(assets, shared, {0, 127, 0});
        std::cout << "Declared destruction FC: old-age pixels, clipped increment, canonical retirement, aliases and strict default PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
