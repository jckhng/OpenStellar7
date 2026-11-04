#include "stellar7/gameplay.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>

namespace stellar7 {

struct GameplayModelTestAccess {
    enum class Heading { aligned, unaligned };
    enum class RewardPool { available, unknown, full };

    static void seededHit(GameplayModel& model, Heading heading,
        RewardPool rewardPool = RewardPool::available) {
        model.level2Entities_.clear();
        model.initializeCampaignPools(4, 0);
        model.phase_ = GameplayPhase::level5;
        model.level2Visibility_.resetLevel();
        model.level5GuardianRadius_ = 1508;
        model.level5GuardianAnimationLimit_ = 6;
        model.level2Player_ = {};
        model.level2Player_.type = 0;
        model.level2Player_.collisionRadius = 330;
        model.level2Player_.stateBytes4dTo52[1] = 1;
        model.level2PlayerArmor_ = 680;

        Level1Entity shot{};
        shot.type = 23;
        shot.collisionRadius = 134;
        shot.position = {0, 5000, 0};
        shot.stateBytes4dTo52 = {0xfd, 64, 0, 0, 0, 0};
        shot.ownerIsPlayer = true;
        shot.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;

        Level1Entity guardian{};
        guardian.type = 44;
        guardian.collisionRadius = 1508;
        guardian.position = shot.position;
        guardian.targetHeading = presenterTargetHeading(
            guardian.position, model.level2Player_.position);
        guardian.yaw = static_cast<std::uint16_t>(guardian.targetHeading
            + (heading == Heading::aligned ? 0U : 0x1000U));
        guardian.stateBytes4dTo52 = {0xff, 1, 2, 0, 5, 0};
        guardian.renderProfile = WorldActorRenderProfile::heapSelectorDeferredAnimation;

        model.level2Entities_ = {shot, guardian};
        model.level2EntityActorPoolSlots_ = {model.level2ActorPoolSize_, 0};
        model.level2EntityProjectilePoolSlots_ = {0, model.level2ProjectilePoolSize_};
        model.level2EntityDeathEffectPoolSlots_ = {
            model.level2DeathEffectPoolSize_, model.level2DeathEffectPoolSize_};
        model.level2EntityFixedPoolSlots_ = {
            model.level2FixedObjectPoolSize_, model.level2FixedObjectPoolSize_};
        model.level2ActorPool_[0] = guardian;
        model.level2ActorPoolActive_[0] = true;
        model.level2ActorIncarnations_[0] = 1;
        model.level2ProjectilePool_[0] = shot;
        model.level2ProjectilePoolActive_[0] = true;
        model.level2AllocationListOrder_ = {1, 0};
        model.level2ObjectListOrder_ = model.level2AllocationListOrder_;
        model.level2ActiveListOrder_ = {0, 1};
        model.level2PostWaveObjectIndex_ = 1;
        model.level2ActiveEnemyCount_ = 1;
        model.level2SpawnedEnemyCount_ = 17;
        model.level2LoopTick_ = 16;
        model.level2HasAdvanced_ = true;
        model.level2TailConsumed_ = true;

        // 48D4 resets only the first eight fixed records. The old record's
        // unspecified scratch must survive Type 25's partial 4506 write.
        auto& fixed = model.level2FixedPool_[0];
        fixed.stateBytes4dTo52[3] = 0x5a;
        fixed.xVelocity = 17;
        fixed.weaponSlotTimers = {3, 4, 5};
        fixed.collisionContactAllocationIndex = 7;
        if (rewardPool == RewardPool::unknown) {
            for (std::size_t slot = 0; slot < 8; ++slot) {
                model.level2FixedPool_[slot].stateBytes4dTo52[1] = 1;
            }
            model.level2FixedPoolKnown_[8] = false;
        } else if (rewardPool == RewardPool::full) {
            for (std::size_t slot = 0; slot < model.level2FixedPool_.size(); ++slot) {
                model.level2FixedPool_[slot].stateBytes4dTo52[1] = 1;
                model.level2FixedPoolKnown_[slot] = true;
            }
        }
        model.snapshotLevel2Presentation();
    }

    static void existingRewardPointer(GameplayModel& model) {
        model.level2PickupIndex_ = 99;
    }

    static void addFixedObstacle(GameplayModel& model) {
        Level1Entity request{};
        request.type = 42;
        request.collisionRadius = 330;
        const auto index = model.allocateLevel2FixedObject(request);
        auto& obstacle = model.level2Entities_.at(index);
        obstacle.position = {0, 6500, 0};
        obstacle.xVelocity = 0;
        obstacle.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
        model.level2FixedPool_[0] = obstacle;
        const auto shot = std::find(model.level2EntityProjectilePoolSlots_.begin(), model.level2EntityProjectilePoolSlots_.end(), 0);
        const auto guardian = std::find(model.level2EntityActorPoolSlots_.begin(), model.level2EntityActorPoolSlots_.end(), 0);
        model.level2ActiveListOrder_ = {
            static_cast<std::size_t>(shot - model.level2EntityProjectilePoolSlots_.begin()),
            static_cast<std::size_t>(guardian - model.level2EntityActorPoolSlots_.begin()), index};
        for (std::size_t actor = 0; actor < model.level2Entities_.size(); ++actor) {
            model.level2Entities_[actor].allocationIndex = actor;
        }
        model.snapshotLevel2Presentation();
    }

    static void advance(GameplayModel& model) {
        model.advanceLevel2({}, nullptr);
    }

    static std::uint32_t score(const GameplayModel& model) {
        return model.level2Score_;
    }

    static std::size_t activeEnemyCount(const GameplayModel& model) {
        return model.level2ActiveEnemyCount_;
    }

    static std::size_t guardianPointer(const GameplayModel& model) {
        return model.level2PostWaveObjectIndex_;
    }

    static const Level1Entity& retiredGuardian(const GameplayModel& model) {
        return model.level2ActorPool_[0];
    }

    static std::size_t rewardPointer(const GameplayModel& model) {
        return model.level2PickupIndex_;
    }

    static std::size_t rewardSlot(const GameplayModel& model) {
        return model.level2EntityFixedPoolSlots_.at(model.level2PickupIndex_);
    }

    static const Level1Entity& fixedRecord(const GameplayModel& model) {
        return model.level2FixedPool_[0];
    }
};

} // namespace stellar7

namespace {

using stellar7::GameplayModel;
using stellar7::GameplayModelTestAccess;
using Heading = GameplayModelTestAccess::Heading;
using RewardPool = GameplayModelTestAccess::RewardPool;

constexpr std::size_t noObject = static_cast<std::size_t>(-1);

std::size_t countType(const GameplayModel& model, std::int16_t type) {
    return static_cast<std::size_t>(std::count_if(model.level2Entities().begin(),
        model.level2Entities().end(), [type](const auto& actor) {
            return actor.type == type;
        }));
}

std::size_t countEvent(const GameplayModel& model, std::uint16_t eventId) {
    return static_cast<std::size_t>(std::count_if(model.level2AudioEvents().begin(),
        model.level2AudioEvents().end(), [eventId](const auto& event) {
            return event.eventId == eventId;
        }));
}

void assertGuardianDefeat(Heading heading) {
    auto model = std::make_unique<GameplayModel>();
    auto guarded = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::seededHit(*model, heading);
    GameplayModelTestAccess::seededHit(*guarded, heading);
    GameplayModelTestAccess::existingRewardPointer(*guarded);
    const auto beforeRng = model->level2Rng().calls();
    const auto guardedRng = guarded->level2Rng().calls();

    GameplayModelTestAccess::advance(*model);
    GameplayModelTestAccess::advance(*guarded);

    assert(GameplayModelTestAccess::score(*model) == 1000);
    assert(GameplayModelTestAccess::activeEnemyCount(*model) == 0);
    assert(countEvent(*model, 12) == 1);
    assert(countEvent(*model, 5) == 0 && countEvent(*model, 6)
        == (heading == Heading::aligned ? 1U : 0U));
    assert(GameplayModelTestAccess::guardianPointer(*model) != noObject);
    assert(GameplayModelTestAccess::retiredGuardian(*model).type == 44);
    assert(GameplayModelTestAccess::retiredGuardian(*model).stateBytes4dTo52[1] == 0);
    assert(GameplayModelTestAccess::retiredGuardian(*model).renderProfile
        == (heading == Heading::aligned
            ? stellar7::WorldActorRenderProfile::heapSelectorNoCallback
            : stellar7::WorldActorRenderProfile::heapSelectorDeferredAnimation));
    assert(countType(*model, 44) == 0);
    assert(countType(*model, 22) == (heading == Heading::aligned ? 1U : 0U));
    assert(model->level2Rng().calls() - beforeRng
        == guarded->level2Rng().calls() - guardedRng + 1);

    const auto rewardIndex = GameplayModelTestAccess::rewardPointer(*model);
    assert(rewardIndex != noObject && rewardIndex < model->level2Entities().size());
    assert(GameplayModelTestAccess::rewardSlot(*model) == 0);
    assert(model->level2PresentationPoolSlot(rewardIndex) == 16);
    const auto& reward = model->level2Entities().at(rewardIndex);
    assert(reward.type == 25 && reward.collisionRadius == 897);
    assert(reward.stateBytes4dTo52[0] == 0xfe);
    assert(reward.stateBytes4dTo52[1] == 100);
    //45C6 writes2, then the same0784 epilogue advances128E once before main.
    assert(reward.stateBytes4dTo52[2] == 3);
    assert(reward.stateBytes4dTo52[5] == 1);
    assert(reward.renderProfile
        == stellar7::WorldActorRenderProfile::inlineSelectorNoCallback);
    assert(reward.stateBytes4dTo52[3] == 0x5a);
    assert(reward.xVelocity == 17);
    assert(reward.weaponSlotTimers == (std::array<std::uint8_t, 3>{3, 4, 5}));
    assert(GameplayModelTestAccess::fixedRecord(*model).stateBytes4dTo52[3] == 0x5a);
    model->finishLevel5Frame();
    GameplayModelTestAccess::advance(*model);
    assert(model->level2Entities().at(GameplayModelTestAccess::rewardPointer(*model)).stateBytes4dTo52[2] == 2);
}

void assertExistingRewardSuppressesSecondSpin() {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::seededHit(*model, Heading::unaligned);
    GameplayModelTestAccess::existingRewardPointer(*model);

    GameplayModelTestAccess::advance(*model);

    assert(GameplayModelTestAccess::score(*model) == 1000);
    assert(countEvent(*model, 12) == 1);
    assert(countType(*model, 25) == 0);
    //Portable vector retirement may remap the sentinel; source128E stays non-null.
    assert(GameplayModelTestAccess::rewardPointer(*model) != noObject);
    assert(GameplayModelTestAccess::guardianPointer(*model) != noObject);
}

void assertInheritedCollisionWritesAllContacts() {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::seededHit(*model, Heading::aligned);
    GameplayModelTestAccess::addFixedObstacle(*model);
    GameplayModelTestAccess::advance(*model);
    const auto rewardIndex = GameplayModelTestAccess::rewardPointer(*model);
    const auto& reward = model->level2Entities().at(rewardIndex);
    assert(reward.position == stellar7::FixedPosition({0, 7200, 0}));
    assert(GameplayModelTestAccess::retiredGuardian(*model).collisionContactAllocationIndex == rewardIndex);
    const auto obstacle = std::find_if(model->level2Entities().begin(), model->level2Entities().end(),
        [](const auto& actor) { return actor.type == 42; });
    assert(obstacle != model->level2Entities().end());
    assert(obstacle->collisionContactAllocationIndex == rewardIndex);
    const auto& events = model->level2AudioEvents();
    assert(events.size() == 2 && events[0].eventId == 12 && events[1].eventId == 6);
    assert(events[0].loopTick == 16 && events[1].loopTick == 16);
}

void assertRewardPoolPreflight(RewardPool pool) {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::seededHit(*model, Heading::unaligned, pool);
    const auto beforeRng = model->level2Rng().calls();
    bool rejected = false;
    try {
        GameplayModelTestAccess::advance(*model);
    } catch (const std::exception&) {
        rejected = true;
    }
    assert(rejected);
    assert(model->level2Rng().calls() == beforeRng);
    assert(GameplayModelTestAccess::score(*model) == 0);
    assert(GameplayModelTestAccess::activeEnemyCount(*model) == 1);
    assert(GameplayModelTestAccess::guardianPointer(*model) != noObject);
    assert(countType(*model, 25) == 0);
}

} // namespace

int main() {
    try {
        assertGuardianDefeat(Heading::aligned);
        assertGuardianDefeat(Heading::unaligned);
        assertExistingRewardSuppressesSecondSpin();
        assertInheritedCollisionWritesAllContacts();
        assertRewardPoolPreflight(RewardPool::unknown);
        assertRewardPoolPreflight(RewardPool::full);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
