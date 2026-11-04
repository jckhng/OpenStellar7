#include "stellar7/gameplay.hpp"
#include "stellar7/resource_codec.hpp"

#include <array>
#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace stellar7 {
struct GameplayModelTestAccess {
    static void contact(GameplayModel& model, const std::uint16_t sourceTick = 4930) {
        model.level2Entities_.assign(7, Level1Entity{});
        for (std::size_t index = 0; index < 7; ++index) {
            auto& actor = model.level2Entities_[index];
            actor.allocationIndex = index;
            actor.type = index < 2 ? 30 : index < 6 ? 46 : 25;
            actor.stateBytes4dTo52 = {0xfe, 100, 3, 0, 0, 1};
            actor.collisionRadius = index == 6 ? 897 : 100;
            actor.position = {static_cast<std::int32_t>(10000 + index * 2000), 20000, 0};
            actor.presenterDistance = static_cast<std::uint32_t>(100 + index);
            actor.targetHeading = static_cast<std::uint16_t>(300 + index);
            actor.flags = 4;
            actor.renderProfile = WorldActorRenderProfile::noHandles;
            actor.forwardVelocity = index < 6 ? 200 : 0; //FE skip must not integrate these.
        }
        model.level2Entities_[6].position = {120, 270, 0};
        model.level2Entities_[6].yawVelocity = -3328;
        model.initializeCampaignPools(1, 0);
        constexpr std::array<std::size_t, 7> slots{11, 2, 7, 0, 14, 4, 9};
        model.level2EntityFixedPoolSlots_.assign(slots.begin(), slots.end());
        for (std::size_t index = 0; index < slots.size(); ++index) {
            model.level2FixedPool_[slots[index]] = model.level2Entities_[index];
            model.level2FixedPoolKnown_[slots[index]] = true;
        }
        model.level2ActiveListOrder_ = {4, 3, 5, 2, 0, 1, 6};
        model.level2PickupIndex_ = 6;
        model.phase_ = GameplayPhase::level2;
        model.level2LoopTick_ = sourceTick;
        model.level2Player_ = {};
        model.level2Player_.collisionRadius = 723;
        model.level2Player_.position = {120, 80, 0};
        model.level2Player_.forwardVelocity = 190;
        model.level2Player_.yawVelocity = 80;
        model.level2PlayerArmor_ = 48;
        model.level2Score_ = 2540;
        model.level2ActiveEnemyCount_ = 1;
        model.level2SpawnedEnemyCount_ = 19;
        model.tick({});
        assert(model.phase_ == GameplayPhase::level2SuccessPresentation);
        assert(model.level2SuccessContinuation_);
        model.setSuccessAudioActive(true);
    }

    static void corrupt(GameplayModel& model, const unsigned which) {
        switch (which) {
        case 0: ++model.level2AllocationRevision_; break;
        case 1: model.level2ActiveListOrder_[0] = model.level2ActiveListOrder_[1]; break;
        case 2: model.level2Entities_[0].type = 42; break;
        case 3: model.level2ProjectilePoolActive_[0] = true; break;
        case 4: model.level2DeathEffectPoolActive_[0] = true; break;
        case 5: model.level2SecondaryPickupKnownNull_ = false; break;
        case 6: model.level2Entities_[6].type = 26; break;
        case 7: model.level2EntityFixedPoolSlots_[0] = model.level2EntityFixedPoolSlots_[1]; break;
        case 8: model.level2AllocationListOrder_[0] = model.level2AllocationListOrder_[1]; break;
        case 9: model.level2Player_.weaponCooldown = 1; break;
        case 10: model.level2DeathEffectPoolPendingPresenterRetirement_[0] = true; break;
        default: assert(false);
        }
    }
    static bool nullSecondary(const GameplayModel& model) { return model.level2SecondaryPickupKnownNull_; }
    static void armor(GameplayModel& model, const std::int16_t value) { model.level2PlayerArmor_ = value; }
    static void invalidatePreloop(GameplayModel& model, const unsigned which) {
        switch (which) {
        case 0: ++model.level2LoopTick_; break;
        case 1: model.level2HasAdvanced_ = true; break;
        case 2: model.level2TailConsumed_ = true; break;
        case 3: model.level2ActiveListOrder_.push_back(0); break;
        case 4: model.level2Complete_ = true; break;
        case 5: model.level2Player_.position.x = 1; break;
        default: assert(false);
        }
    }
    static void allocateSecondary(GameplayModel& model) {
        Level1Entity other{};
        other.type = 26;
        other.stateBytes4dTo52 = {0xfe, 100, 0, 0, 0, 1};
        assert(model.allocateLevel2FixedObject(other) != static_cast<std::size_t>(-1));
    }
    static void samePhysical(const GameplayModel& model) {
        for (std::size_t index = 0; index < model.level2Entities_.size(); ++index) {
            const auto& physical = model.level2FixedPool_[model.level2EntityFixedPoolSlots_[index]];
            const auto& live = model.level2Entities_[index];
            assert(physical.position == live.position && physical.stateBytes4dTo52 == live.stateBytes4dTo52);
            assert(physical.flags == live.flags && physical.presenterDistance == live.presenterDistance);
            assert(physical.targetHeading == live.targetHeading);
        }
    }
};
} //namespace stellar7

namespace {
using namespace stellar7;

void sameActors(const std::vector<Level1Entity>& a, const std::vector<Level1Entity>& b) {
    assert(a.size() == b.size());
    for (std::size_t index = 0; index < a.size(); ++index) {
        assert(a[index].type == b[index].type && a[index].position == b[index].position);
        assert(a[index].stateBytes4dTo52 == b[index].stateBytes4dTo52 && a[index].flags == b[index].flags);
        assert(a[index].presenterDistance == b[index].presenterDistance && a[index].targetHeading == b[index].targetHeading);
        assert(a[index].yaw == b[index].yaw && a[index].forwardVelocity == b[index].forwardVelocity);
    }
}

void returnedOnce() {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::contact(*model);
    const auto before = model->level2Entities();
    const auto player = model->level2Player();
    const auto ground = model->level2Ground().points();
    const auto rng = model->level2Rng();
    const auto indicator = model->level2CockpitIndicatorX();
    const auto loop = model->level2LoopTick();
    for (unsigned poll = 0; poll < 3; ++poll) {
        model->tick({});
        sameActors(before, model->level2Entities());
        assert(model->level2Ground().points() == ground && model->level2Rng().calls() == rng.calls());
        assert(model->level2LoopTick() == loop && model->level2CockpitIndicatorX() == indicator);
    }
    model->setSuccessAudioActive(false);
    model->validateLevel2SuccessReturn({});
    model->tick({});
    assert(model->phase() == GameplayPhase::level2SuccessReturn && model->campaignLevel() == 1);
    assert(model->score() == 2540 && model->playerArmor() == 48);
    assert(model->level2Player().position == FixedPosition({0, 0, 0}));
    assert(model->forwardVelocity() == player.forwardVelocity - 10);
    assert(model->turnVelocity() == player.yawVelocity - 80);
    assert(model->level2LoopTick() == loop && model->level2CockpitIndicatorX() == indicator);
    assert(model->level2Ground().points() == ground && model->level2Rng().calls() == rng.calls());
    auto expectedRng = rng;
    auto actualRng = model->level2Rng();
    for (unsigned index = 0; index < 56; ++index) {
        assert(expectedRng.next() == actualRng.next());
    }
    for (std::size_t index = 0; index < before.size(); ++index) {
        const auto& after = model->level2Entities()[index];
        assert(after.position == FixedPosition({before[index].position.x - player.position.x,
            before[index].position.y - player.position.y, before[index].position.z}));
        assert(after.presenterDistance == before[index].presenterDistance && after.targetHeading == before[index].targetHeading);
        assert(after.flags == before[index].flags && after.yaw == before[index].yaw);
        assert(after.forwardVelocity == before[index].forwardVelocity);
    }
    assert(model->level2Entities()[6].stateBytes4dTo52[2] == 2);
    assert(model->level2Entities()[6].stateBytes4dTo52[5] == 0xff);
    assert(model->weaponCooldown() == 0 && !model->shotActive());
    GameplayModelTestAccess::samePhysical(*model);
    const auto ticks = model->ticks();
    bool rejected = false;
    try { model->tick({}); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && model->ticks() == ticks);
}

std::vector<Model3d> loaderModels() {
    std::vector<Model3d> models(32);
    for (const auto type : {0, 2, 10, 24, 31}) {
        models[type].collisionRadius = static_cast<std::uint16_t>(700 + type);
    }
    for (const auto type : {24, 31}) {
        models[type].flags = 1;
        models[type].animationLimits = {7};
    }
    return models;
}

void loaderCarry(const std::int16_t armor) {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::contact(*model);
    model->setSuccessAudioActive(false);
    model->tick({});
    GameplayModelTestAccess::armor(*model, armor);
    const auto incomingRng = model->level2Rng();
    const auto ground = model->level2Ground().points();
    const auto indicator = model->level2CockpitIndicatorX();
    const auto ticks = model->ticks();
    auto models = loaderModels();
    models[24].animationLimits.clear();
    bool rejected = false;
    try { model->completeLevel3Load(models, {}); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && model->ticks() == ticks && model->campaignLevel() == 1);
    assert(model->level2Rng().calls() == incomingRng.calls());
    assert(model->level2Entities().size() == 7 && model->level2CockpitIndicatorX() == indicator);

    models = loaderModels();
    model->completeLevel3Load(models, {});
    assert(model->phase() == GameplayPhase::level3LoaderReturn && model->campaignLevel() == 2);
    assert(model->ticks() == ticks + 1 && model->score() == 2540);
    assert(model->playerArmor() == (armor == 48 ? 128 : 680));
    assert(model->level2Player().collisionRadius == models[0].collisionRadius);
    assert(model->level2Player().position == FixedPosition({0, 0, 0}));
    assert(model->forwardVelocity() == 0 && model->turnVelocity() == 0);
    assert(model->level2Entities().size() == 6 && model->level2ActiveListOrder().empty());
    assert(model->level2LoopTick() == 0 && model->level2AudioEvents().empty());
    assert(model->level2Ground().points() == ground);
    auto expectedRng = incomingRng;
    for (unsigned index = 0; index < 11; ++index) { expectedRng.next(); }
    auto actualRng = model->level2Rng();
    assert(actualRng.calls() == expectedRng.calls());
    for (unsigned index = 0; index < 56; ++index) { assert(actualRng.next() == expectedRng.next()); }
    const auto& record = model->lastCockpitInitialization();
    assert(record.levelIndex == 2 && record.displaySteps == 2 && record.states[0].x == indicator);
    assert(record.states[2].x == model->level2CockpitIndicatorX());
    assert(model->level2Entities()[4].modelAnimationVariant == 0);
    assert(model->level2Entities()[4].stateBytes4dTo52[3] == 0);
    assert(model->level2PresentationPoolSlot(4) == 16 && model->level2PresentationPoolSlot(5) == 17);
    for (std::size_t index = 0; index < 4; ++index) {
        assert(model->level2PresentationPoolSlot(index) == index);
        assert(model->level2PresentationActorIncarnation(index) == 1);
    }
    const auto loadedTicks = model->ticks();
    rejected = false;
    try { model->completeLevel3Load(models, {}); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && model->ticks() == loadedTicks);
    rejected = false;
    try { model->tick({}); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && model->ticks() == loadedTicks);

    const auto actors = model->level2Entities();
    const auto loadedGround = model->level2Ground().points();
    const auto loadedRng = model->level2Rng();
    const auto loadedIndicator = model->level2CockpitIndicatorX();
    for (unsigned which = 0; which < 6; ++which) {
        auto invalid = std::make_unique<GameplayModel>(*model);
        GameplayModelTestAccess::invalidatePreloop(*invalid, which);
        rejected = false;
        try { invalid->completeLevel3Preloop(); } catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected && invalid->ticks() == loadedTicks);
        assert(invalid->level2Rng().calls() == loadedRng.calls());
        assert(invalid->level2Ground().points() == loadedGround);
    }
    auto preloopRng = loadedRng;
    std::array<GroundPoint, Level1GroundState::pointCount> expectedPoints{};
    for (auto& point : expectedPoints) {
        const auto x = static_cast<std::int16_t>(preloopRng.next());
        const auto y = static_cast<std::int16_t>(preloopRng.next());
        point = {static_cast<std::int16_t>((x % 295) * 32),
            static_cast<std::int16_t>((y % 56 + 90) * 32)};
    }
    model->completeLevel3Preloop();
    assert(model->phase() == GameplayPhase::level3PreloopReturn && model->ticks() == loadedTicks + 1);
    assert(model->level2LoopTick() == 0 && model->level2ActiveListOrder().empty());
    assert(model->score() == 2540 && model->playerArmor() == (armor == 48 ? 128 : 680));
    assert(model->level2Ground().points() == expectedPoints);
    assert(model->level2Rng().calls() == loadedRng.calls() + 12);
    actualRng = model->level2Rng();
    for (unsigned index = 0; index < 56; ++index) { assert(actualRng.next() == preloopRng.next()); }
    sameActors(actors, model->level2Entities());
    assert(model->level2Entities()[4].modelAnimationVariant == 0);
    assert(model->level2Entities()[4].stateBytes4dTo52[3] == 0);
    assert(model->level2CockpitIndicatorX() == loadedIndicator && model->level2AudioEvents().empty());
    for (unsigned which = 0; which < 2; ++which) {
        rejected = false;
        try {
            if (which == 0) { model->completeLevel3Preloop(); } else { model->tick({}); }
        } catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected && model->ticks() == loadedTicks + 1);
        assert(model->level2Rng().calls() == loadedRng.calls() + 12);
    }
    const auto preloopActors = model->level2Entities();
    const auto preloopTicks = model->ticks();
    GameplayInput unsupported{};
    unsupported.forward = true;
    rejected = false;
    try { model->advanceRetainedLevel3(unsupported); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && model->ticks() == preloopTicks);
    model->advanceRetainedLevel3({});
    assert(model->phase() == GameplayPhase::level3Native && model->level2LoopTick() == 1);
    assert(model->ticks() == preloopTicks + 1);
    for (std::size_t index = 0; index < preloopActors.size(); ++index) {
        assert(model->level2Entities()[index].position == preloopActors[index].position);
        assert(model->level2Entities()[index].yaw == preloopActors[index].yaw);
        assert(model->level2Entities()[index].presenterDistance == 0);
    }
    assert(model->level2ActiveListOrder() == std::vector<std::size_t>({0, 1, 2, 3, 4, 5}));
    assert(model->level2PresentationEntities()[4].presentationModelAnimationVariant == 0);
    assert(model->level2Entities()[4].modelAnimationVariant == 1);
    assert(model->level2Entities()[4].stateBytes4dTo52[3] == 2);
    rejected = false;
    try { model->finishLevel3Frame(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && model->level2LoopTick() == 1);
    model->finishLevel3Radar();
    assert(model->level2Entities()[4].modelAnimationVariant == 1);
    assert(model->level2Entities()[4].stateBytes4dTo52[3] == 1);
    rejected = false;
    try { model->finishLevel3Radar(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && model->level2Entities()[4].stateBytes4dTo52[3] == 1);
    model->finishLevel3Frame();
    assert(model->level2LoopTick() == 1);
    assert(model->level2AudioEvents().size() == (armor == 48 ? 1 : 0));
    if (armor == 48) {
        assert(model->level2AudioEvents()[0].loopTick == 0 && model->level2AudioEvents()[0].eventId == 3);
    }
    const auto firstActors = model->level2Entities();
    model->advanceRetainedLevel3({});
    assert(model->level2LoopTick() == 2 && model->ticks() == preloopTicks + 2);
    for (std::size_t index = 0; index < 3; ++index) {
        assert(model->level2Entities()[index].position == firstActors[index].position);
        assert(model->level2Entities()[index].yaw == static_cast<std::uint16_t>(
            firstActors[index].yaw + firstActors[index].yawVelocity));
    }
    assert(model->level2Entities()[3].position.x == firstActors[3].position.x - 144);
    assert(model->level2Entities()[4].modelAnimationVariant == 1);
    assert(model->level2Entities()[4].stateBytes4dTo52[3] == 0);
    assert(model->level2PresentationEntities()[4].presentationModelAnimationVariant == 1);
    model->finishLevel3Radar();
    assert(model->level2Entities()[4].modelAnimationVariant == 2);
    assert(model->level2Entities()[4].stateBytes4dTo52[3] == 2);
    model->finishLevel3Frame();
    assert(model->level2Entities()[5].stateBytes4dTo52[2] == 0);
    assert(model->level2LoopTick() == 2);
    rejected = false;
    const auto finishedRngCalls = model->level2Rng().calls();
    try { model->finishLevel3Frame(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && model->level2Rng().calls() == finishedRngCalls);
}

void rejectsBeforeMutation(const unsigned which, const std::uint16_t sourceTick = 4930,
    const GameplayInput input = {}) {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::contact(*model, sourceTick);
    if (which < 11) {
        GameplayModelTestAccess::corrupt(*model, which);
    }
    model->setSuccessAudioActive(false);
    const auto actors = model->level2Entities();
    const auto player = model->level2Player();
    const auto calls = model->level2Rng().calls();
    const auto ticks = model->ticks();
    const auto loop = model->level2LoopTick();
    bool rejected = false;
    try { model->tick(input); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && model->ticks() == ticks && model->level2LoopTick() == loop);
    assert(model->level2Player().position == player.position && model->level2Rng().calls() == calls);
    sameActors(actors, model->level2Entities());
}
} //namespace

int main() {
    returnedOnce();
    loaderCarry(48);
    loaderCarry(650);
    for (unsigned which = 0; which < 11; ++which) {
        rejectsBeforeMutation(which);
    }
    rejectsBeforeMutation(11, 4928); //Source periodic tail is deliberately not admitted.
    GameplayInput movement{};
    movement.left = true;
    rejectsBeforeMutation(11, 4930, movement);
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::contact(*model);
    assert(GameplayModelTestAccess::nullSecondary(*model));
    GameplayModelTestAccess::allocateSecondary(*model);
    assert(!GameplayModelTestAccess::nullSecondary(*model));
    std::cout << "Level2 FE-only success continuation, source tail and rejection domains\n";
}
