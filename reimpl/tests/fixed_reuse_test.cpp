#include "stellar7/gameplay.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace stellar7 {

struct Level1ReloadTestAccess {
    static void streak(Level1State& state, std::int16_t type, std::uint16_t count) {
        state.lastDestroyedType_ = type;
        state.consecutiveDestroyedTypeCount_ = count;
    }
};

struct GameplayModelTestAccess {
    static void emptyLevel5(GameplayModel& model) {
        model.level2Entities_.clear();
        model.initializeCampaignPools(4, 0);
        model.phase_ = GameplayPhase::level5;
        model.level2ActiveEnemyCount_ = 5;
        model.level2SpawnedEnemyCount_ = 5;
        model.level2Player_ = {};
        model.level2Player_.type = 0;
        model.level2Player_.collisionRadius = 330;
        model.level2Player_.stateBytes4dTo52[1] = 1;
        model.level2PlayerArmor_ = 680;
    }

    static void streak(GameplayModel& model, std::int16_t type, std::uint16_t count) {
        Level1ReloadTestAccess::streak(model.level1_, type, count);
    }

    static void registerBonus(GameplayModel& model, std::int16_t type) {
        Level1Entity victim{};
        victim.type = type;
        victim.position = {1200, -3400, 200};
        model.registerLevel5DestroyedTypeBonus(victim);
    }

    static void occupyRecord(GameplayModel& model, std::size_t slot) {
        model.level2FixedPool_[slot].stateBytes4dTo52[1] = 1;
        model.level2FixedPoolKnown_[slot] = true;
    }

    static void makeUnknown(GameplayModel& model, std::size_t slot) {
        model.level2FixedPoolKnown_[slot] = false;
    }

    static void prepareContact(GameplayModel& model) {
        assert(model.level2Entities_.size() == 1);
        model.level2ActiveListOrder_ = {0};
        model.level2Player_.position = model.level2Entities_[0].position;
        model.level2HasAdvanced_ = false;
        model.level2LoopTick_ = 1;
    }

    static void prepareEmptyBaseline(GameplayModel& model, FixedPosition playerPosition) {
        model.level2Player_.position = playerPosition;
        model.level2HasAdvanced_ = false;
        model.level2LoopTick_ = 1;
    }

    static void advance(GameplayModel& model) {
        model.advanceLevel2({}, nullptr);
    }

    static void finishTail(GameplayModel& model, std::uint16_t tick,
        FixedPosition playerPosition) {
        model.level2LoopTick_ = tick;
        model.level2Player_.position = playerPosition;
        model.consumeLevel2DeferredTail();
    }

    static void setRetiredPosition(GameplayModel& model, FixedPosition position) {
        model.level2FixedPool_[0].position = position;
    }

    static Level1Entity& live(GameplayModel& model) {
        return model.level2Entities_.at(0);
    }

    static const Level1Entity& record(const GameplayModel& model, std::size_t slot) {
        return model.level2FixedPool_[slot];
    }

    static std::size_t fixedSlot(const GameplayModel& model, std::size_t index) {
        return model.level2EntityFixedPoolSlots_.at(index);
    }

    static bool known(const GameplayModel& model, std::size_t slot) {
        return model.level2FixedPoolKnown_[slot];
    }

    static std::size_t entityCount(const GameplayModel& model) {
        return model.level2Entities_.size();
    }

    static bool activeEmpty(const GameplayModel& model) {
        return model.level2ActiveListOrder_.empty();
    }

    static void seedLoaderHistory(GameplayModel& model) {
        model.level2FixedPoolKnown_[9] = true;
        auto& upper = model.level2FixedPool_[9];
        upper.type = 42;
        upper.position = {31415, -27182, 99};
        upper.stateBytes4dTo52 = {0xfe, 0, 3, 0x5a, 0, 3};
        upper.pitchVelocity = 77;
        upper.presenterDistance = 4321;
        upper.ownerAllocationIndex = 27;
        model.level2FixedPool_[0].stateBytes4dTo52[3] = 0x7b;
        model.level2FixedPool_[0].pitchVelocity = 33;
    }

    static void loadLevel5(GameplayModel& model) {
        std::vector<Model3d> models(14);
        models[0].collisionRadius = 100;
        models[5].collisionRadius = 1008;
        models[5].flags = 1;
        models[8].collisionRadius = 860;
        models[8].flags = 1;
        models[11].collisionRadius = 748;
        models[11].flags = 1;
        models[11].animationLimits = {8};
        models[13].collisionRadius = 559;
        model.initializeLevel5(models, StellarRng{}, 680);
    }

    static void prepareCollectAndKill(GameplayModel& model, bool shotBeforeFixed) {
        loadLevel5(model);
        for (unsigned frame = 0; frame < 2; ++frame) {
            model.tick({});
            model.finishLevel5Frame();
        }
        GameplayInput fire{};
        fire.fire = true;
        model.tick(fire);
        model.finishLevel5Frame();
        model.tick({});
        model.finishLevel5Frame();

        // Three source-shaped streak updates create the old fixed pointer.
        for (unsigned kill = 0; kill < 3; ++kill) {
            registerBonus(model, 11);
        }
        assert(model.level2Entities_.size() == model.level2AllocationListOrder_.size());
        const auto shot = std::find(model.level2EntityProjectilePoolSlots_.begin(),
            model.level2EntityProjectilePoolSlots_.end(), 0);
        const auto fixed = std::find(model.level2EntityFixedPoolSlots_.begin(),
            model.level2EntityFixedPoolSlots_.end(), 0);
        const auto victim = std::find(model.level2EntityActorPoolSlots_.begin(),
            model.level2EntityActorPoolSlots_.end(), 0);
        assert(shot != model.level2EntityProjectilePoolSlots_.end());
        assert(fixed != model.level2EntityFixedPoolSlots_.end());
        assert(victim != model.level2EntityActorPoolSlots_.end());
        const auto shotIndex = static_cast<std::size_t>(
            shot - model.level2EntityProjectilePoolSlots_.begin());
        const auto fixedIndex = static_cast<std::size_t>(
            fixed - model.level2EntityFixedPoolSlots_.begin());
        const auto victimIndex = static_cast<std::size_t>(
            victim - model.level2EntityActorPoolSlots_.begin());
        assert(shotIndex != fixedIndex && shotIndex != victimIndex && fixedIndex != victimIndex);

        for (std::size_t index = 0; index < model.level2Entities_.size(); ++index) {
            if (index == shotIndex || index == fixedIndex) {
                continue;
            }
            auto& actor = model.level2Entities_[index];
            actor.position = {30000, 30000, 0};
            actor.behaviorTimer = 7;
            actor.stateBytes4dTo52[5] = 0;
            actor.forwardVelocity = 0;
            actor.xVelocity = actor.yVelocity = actor.zVelocity = 0;
        }
        auto& shotActor = model.level2Entities_[shotIndex];
        shotActor.position = {0, 5000, 0};
        auto& victimActor = model.level2Entities_[victimIndex];
        assert(victimActor.type == 11);
        victimActor.position = {0, 5500, 0};
        victimActor.weaponSlotTimers[0] = 4;
        victimActor.presenterDistance = 5500;
        auto& oldBonus = model.level2Entities_[fixedIndex];
        assert(oldBonus.type == 42 && model.level2EntityFixedPoolSlots_[fixedIndex] == 0);
        oldBonus.position = {0, 0, 100};
        oldBonus.yaw = oldBonus.roll = 0;
        oldBonus.xVelocity = oldBonus.yVelocity = oldBonus.zVelocity = 0;
        oldBonus.forwardVelocity = 0;
        model.level2Player_.position = {};
        streak(model, 11, 2);

        model.level2ActiveListOrder_.clear();
        if (shotBeforeFixed) {
            model.level2ActiveListOrder_ = {shotIndex, fixedIndex, victimIndex};
        } else {
            model.level2ActiveListOrder_ = {fixedIndex, shotIndex, victimIndex};
        }
        for (const auto index : model.level2AllocationListOrder_) {
            if (index != shotIndex && index != fixedIndex && index != victimIndex) {
                model.level2ActiveListOrder_.push_back(index);
            }
        }
    }

    static void assertCollectAndKillResult(const GameplayModel& model,
        bool shotBeforeFixed) {
        assert(model.level2Score_ == 150);
        assert(model.level1_.lastDestroyedType() == 11);
        assert(model.level1_.consecutiveDestroyedTypeCount() == 0);
        assert(std::count_if(model.level2Entities_.begin(), model.level2Entities_.end(),
            [](const auto& actor) { return actor.type == 42; }) == 1);
        const auto fixed = std::find(model.level2EntityFixedPoolSlots_.begin(),
            model.level2EntityFixedPoolSlots_.end(), 0);
        assert(fixed != model.level2EntityFixedPoolSlots_.end());
        const auto index = static_cast<std::size_t>(
            fixed - model.level2EntityFixedPoolSlots_.begin());
        const auto& reborn = model.level2Entities_[index];
        assert(reborn.type == 42 && reborn.stateBytes4dTo52[1] == 1);
        assert(reborn.stateBytes4dTo52[2] == 3 && reborn.stateBytes4dTo52[5] == 3);
        assert(reborn.collisionContactIsPlayer && model.level2FixedPool_[0].collisionContactIsPlayer);
        const auto expectedAngle = shotBeforeFixed ? std::uint16_t{0x0800} : std::uint16_t{0};
        assert(reborn.yaw == expectedAngle && reborn.roll == expectedAngle);
    }

    static void assertReusedTail(GameplayModel& model) {
        const auto slot = std::find(model.level2EntityFixedPoolSlots_.begin(),
            model.level2EntityFixedPoolSlots_.end(), 0);
        const auto index = static_cast<std::size_t>(slot - model.level2EntityFixedPoolSlots_.begin());
        auto expected = model.level2Entities_[index].position;
        expected.x -= 100;
        expected.y += 200;
        finishTail(model, 1, {100, -200, 0});
        assert(model.level2Entities_[index].position == expected);
    }
};

}  // namespace stellar7

namespace {

using stellar7::GameplayModel;
using stellar7::GameplayModelTestAccess;

void retirementAndSameRecordReuse() {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::emptyLevel5(*model);
    GameplayModelTestAccess::streak(*model, 11, 2);
    GameplayModelTestAccess::registerBonus(*model, 11);
    assert(GameplayModelTestAccess::fixedSlot(*model, 0) == 0);
    auto& first = GameplayModelTestAccess::live(*model);
    first.stateBytes4dTo52[3] = 0x5a;
    first.stateBytes4dTo52[4] = 0x77;
    first.flags = 0x80;
    first.pitch = 0x0400;
    first.pitchVelocity = 17;
    first.xVelocity = 18;
    first.yVelocity = -19;
    first.zVelocity = 20;
    first.forwardVelocity = 21;
    first.presenterDistance = 2468;
    first.targetHeading = 0x1234;
    first.ownerAllocationIndex = 27;
    first.ownerIsPlayer = true;
    first.yaw = 0x0800;
    first.roll = 0x0800;
    GameplayModelTestAccess::prepareContact(*model);
    auto expectedRetired = first;
    expectedRetired.stateBytes4dTo52[1] = 0;
    expectedRetired.collisionContactAllocationIndex = static_cast<std::size_t>(-1);
    expectedRetired.collisionContactIsPlayer = true;
    stellar7::advanceEntityKinematics(expectedRetired);
    expectedRetired.presenterDistance = stellar7::approximatePlanarDistance(
        model->level2Player().position, expectedRetired.position
    );
    expectedRetired.targetHeading = stellar7::presenterTargetHeading(
        expectedRetired.position, model->level2Player().position
    );
    auto baseline = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::emptyLevel5(*baseline);
    GameplayModelTestAccess::prepareEmptyBaseline(*baseline, first.position);
    GameplayModelTestAccess::advance(*baseline);
    const auto score = model->score();
    const auto armor = model->playerArmor();
    GameplayModelTestAccess::advance(*model);
    assert(GameplayModelTestAccess::entityCount(*model) == 0);
    assert(GameplayModelTestAccess::activeEmpty(*model));
    assert(model->score() == score + 100 && model->playerArmor() == armor);
    assert(model->level2AudioEvents().back().eventId == 24);
    const auto& retired = GameplayModelTestAccess::record(*model, 0);
    assert(retired.stateBytes4dTo52[1] == 0);
    assert(retired.position == expectedRetired.position);
    assert(retired.pitch == expectedRetired.pitch && retired.roll == expectedRetired.roll
        && retired.yaw == expectedRetired.yaw);
    assert(retired.presenterDistance == expectedRetired.presenterDistance
        && retired.targetHeading == expectedRetired.targetHeading);
    assert(retired.stateBytes4dTo52[3] == 0x5a && retired.pitchVelocity == 17);
    assert(retired.xVelocity == 18 && retired.yVelocity == -19 && retired.zVelocity == 20);
    assert(retired.forwardVelocity == 21 && retired.ownerAllocationIndex == 27 && retired.ownerIsPlayer);
    assert(retired.collisionContactAllocationIndex == static_cast<std::size_t>(-1));
    assert(retired.collisionContactIsPlayer && GameplayModelTestAccess::known(*model, 0));
    assert(model->level2Rng().calls() == baseline->level2Rng().calls());

    GameplayModelTestAccess::streak(*model, 5, 2);
    const auto beforeReuse = model->level2Rng().calls();
    GameplayModelTestAccess::registerBonus(*model, 5);
    assert(model->level2Rng().calls() == beforeReuse);
    assert(GameplayModelTestAccess::fixedSlot(*model, 0) == 0);
    const auto& reused = GameplayModelTestAccess::live(*model);
    assert((reused.stateBytes4dTo52 == std::array<std::uint8_t, 6>{0xfe, 1, 6, 0x5a, 0, 6}));
    assert(reused.flags == 0);
    assert((reused.position == stellar7::FixedPosition{1200, -3400, 100}));
    assert(reused.type == 42 && reused.collisionRadius == 330);
    assert(reused.pitch == 0 && reused.roll == 0 && reused.yaw == 0);
    assert(reused.yawVelocity == 0x0800 && reused.rollVelocity == 0x0800);
    assert(reused.pitchVelocity == 17 && reused.xVelocity == 18);
    assert(reused.yVelocity == -19 && reused.zVelocity == 20 && reused.forwardVelocity == 21);
    assert(reused.presenterDistance == expectedRetired.presenterDistance
        && reused.targetHeading == expectedRetired.targetHeading);
    assert(reused.ownerAllocationIndex == 27 && reused.ownerIsPlayer);
    assert(reused.collisionContactAllocationIndex == static_cast<std::size_t>(-1));
    assert(reused.collisionContactIsPlayer);
}

void firstFreeUsesRecordHealth() {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::emptyLevel5(*model);
    GameplayModelTestAccess::occupyRecord(*model, 0);
    GameplayModelTestAccess::streak(*model, 13, 2);
    GameplayModelTestAccess::registerBonus(*model, 13);
    assert(GameplayModelTestAccess::entityCount(*model) == 1);
    assert(GameplayModelTestAccess::fixedSlot(*model, 0) == 1);
    assert(GameplayModelTestAccess::record(*model, 0).stateBytes4dTo52[1] == 1);
}

void fullAndUnknownCapacity() {
    auto full = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::emptyLevel5(*full);
    for (std::size_t slot = 0; slot < 16; ++slot) {
        GameplayModelTestAccess::occupyRecord(*full, slot);
    }
    GameplayModelTestAccess::streak(*full, 11, 2);
    const auto calls = full->level2Rng().calls();
    GameplayModelTestAccess::registerBonus(*full, 11);
    assert(GameplayModelTestAccess::entityCount(*full) == 0);
    assert(full->level1State().lastDestroyedType() == 11);
    assert(full->level1State().consecutiveDestroyedTypeCount() == 0);
    assert(full->level2Rng().calls() == calls);

    auto unknown = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::emptyLevel5(*unknown);
    for (std::size_t slot = 0; slot < 8; ++slot) {
        GameplayModelTestAccess::occupyRecord(*unknown, slot);
    }
    GameplayModelTestAccess::makeUnknown(*unknown, 8);
    GameplayModelTestAccess::streak(*unknown, 5, 2);
    bool rejected = false;
    try {
        GameplayModelTestAccess::registerBonus(*unknown, 5);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && GameplayModelTestAccess::entityCount(*unknown) == 0);
    assert(unknown->level1State().lastDestroyedType() == 5);
    assert(unknown->level1State().consecutiveDestroyedTypeCount() == 2);
}

void loaderResetsOnlyLowerHalf() {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::emptyLevel5(*model);
    GameplayModelTestAccess::seedLoaderHistory(*model);
    GameplayModelTestAccess::loadLevel5(*model);
    assert(GameplayModelTestAccess::known(*model, 0));
    assert(GameplayModelTestAccess::record(*model, 0).stateBytes4dTo52[1] == 0);
    assert(GameplayModelTestAccess::record(*model, 0).stateBytes4dTo52[3] == 0);
    assert(GameplayModelTestAccess::record(*model, 0).pitchVelocity == 0);
    assert(GameplayModelTestAccess::known(*model, 9));
    const auto& upper = GameplayModelTestAccess::record(*model, 9);
    assert(upper.type == 42 && (upper.position == stellar7::FixedPosition{31415, -27182, 99}));
    assert((upper.stateBytes4dTo52 == std::array<std::uint8_t, 6>{0xfe, 0, 3, 0x5a, 0, 3}));
    assert(upper.pitchVelocity == 77 && upper.presenterDistance == 4321);
    assert(upper.ownerAllocationIndex == 27);
    assert(!GameplayModelTestAccess::known(*model, 8));
}

void collectionAndThirdKillShareFixedPointer() {
    for (const bool shotBeforeFixed : {false, true}) {
        auto model = std::make_unique<GameplayModel>();
        GameplayModelTestAccess::prepareCollectAndKill(*model, shotBeforeFixed);
        model->tick({});
        GameplayModelTestAccess::assertCollectAndKillResult(*model, shotBeforeFixed);
        GameplayModelTestAccess::assertReusedTail(*model);
    }
}

void retiredRecordDoesNotReceiveRebuiltTail() {
    for (const std::uint16_t tailTick : {1, 16}) {
        auto model = std::make_unique<GameplayModel>();
        GameplayModelTestAccess::emptyLevel5(*model);
        GameplayModelTestAccess::streak(*model, 11, 2);
        GameplayModelTestAccess::registerBonus(*model, 11);
        GameplayModelTestAccess::prepareContact(*model);
        GameplayModelTestAccess::advance(*model);
        GameplayModelTestAccess::setRetiredPosition(*model, {70000, -70000, 100});
        const auto distance = GameplayModelTestAccess::record(*model, 0).presenterDistance;
        GameplayModelTestAccess::finishTail(*model, tailTick, {100, -100, 0});
        const auto expected = stellar7::FixedPosition{70000, -70000, 100};
        assert(GameplayModelTestAccess::record(*model, 0).position == expected);
        assert(GameplayModelTestAccess::record(*model, 0).presenterDistance == distance);
        GameplayModelTestAccess::finishTail(*model, 1, {50, 75, 0});
        assert(GameplayModelTestAccess::record(*model, 0).position == expected);
    }
}

}  // namespace

int main() {
    retirementAndSameRecordReuse();
    firstFreeUsesRecordHealth();
    fullAndUnknownCapacity();
    loaderResetsOnlyLowerHalf();
    collectionAndThirdKillShareFixedPointer();
    retiredRecordDoesNotReceiveRebuiltTail();
}
