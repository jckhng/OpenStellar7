#include "stellar7/gameplay.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <stdexcept>

namespace stellar7 {

struct GameplayModelTestAccess {
    enum class ProjectileRecord { fresh, reused };

    static void beamReady(GameplayModel& model) {
        model.level2Entities_.clear();
        model.initializeCampaignPools(4, 0);
        model.phase_ = GameplayPhase::level5;
        model.level2Visibility_.resetLevel();
        model.level2Player_ = {};
        model.level2Player_.type = 0;
        model.level2Player_.collisionRadius = 330;
        model.level2Player_.stateBytes4dTo52[1] = 1;
        model.level2PlayerArmor_ = 680;

        Level1Entity craft{};
        craft.type = 7;
        craft.collisionRadius = 13052;
        craft.position = {0, 15000, 5000};
        craft.stateBytes4dTo52 = {0xff, 100, 0, 0, 5, 3};
        craft.behaviorTimer = 30;
        craft.flags = 8;
        craft.renderProfile = WorldActorRenderProfile::inlineSelectorBeamCallback;
        assert(model.allocateLevel2Actor(craft, GameplayModel::ActorInitialization::validatedOrdinary) == 0);
        model.level2ActiveListOrder_ = {0};
        model.level2CompletionObjectIndex_ = 0;
        model.level2ActiveEnemyCount_ = 1;
        model.level2SpawnedEnemyCount_ = 17;
        model.level2LoopTick_ = 15; // Birth main uses source counter 15; next inherited main uses 16.
        model.level2HasAdvanced_ = true;
        model.level2TailConsumed_ = true;
        model.snapshotLevel2Presentation();
    }

    static void decodedLevel5GuardianModel(GameplayModel& model) {
        model.level5GuardianRadius_ = 1508;
        model.level5GuardianAnimationLimit_ = 6;
    }

    static void suppressWithExistingGuardianPointer(GameplayModel& model) {
        model.level2PostWaveObjectIndex_ = 0;
    }

    static void fillRemainingOrdinaryPool(GameplayModel& model) {
        for (std::size_t slot = 1; slot < model.level2ActorPoolActive_.size(); ++slot) {
            model.level2ActorPoolActive_[slot] = true;
        }
    }

    static void advance(GameplayModel& model) {
        model.advanceLevel2({}, nullptr);
    }

    static std::size_t guardianIndex(const GameplayModel& model) {
        return model.level2PostWaveObjectIndex_;
    }

    static const auto& inheritedOrder(const GameplayModel& model) {
        return model.level2ActiveListOrder_;
    }

    static Level1Entity& guardian(GameplayModel& model) {
        return model.level2Entities_.at(model.level2PostWaveObjectIndex_);
    }

    static const Level1Entity& presentedGuardian(const GameplayModel& model) {
        return model.level2PresentationEntities_.at(model.level2PostWaveObjectIndex_);
    }

    static void seedProjectileRecord(GameplayModel& model, ProjectileRecord provenance) {
        auto& record = model.level2ProjectilePool_[0];
        record = {};
        record.stateBytes4dTo52[0] = 0xfd;
        record.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
        if (provenance == ProjectileRecord::reused) {
            record.forwardVelocity = 200;
            record.xVelocity = 27;
            record.yVelocity = -43;
            record.yawVelocity = 32;
            record.stateBytes4dTo52[3] = 9;
            record.stateBytes4dTo52[5] = 7;
            record.weaponSlotCount = 2;
            record.weaponCooldown = 5;
            record.weaponSlotTimers = {3, 4, 5};
            record.collisionContactAllocationIndex = 6;
            record.collisionContactIsPlayer = true;
        }
    }

    static const Level1Entity& projectileRecord(const GameplayModel& model) {
        return model.level2ProjectilePool_[0];
    }

    static void setGuardianWeaponScratch(GameplayModel& model) {
        auto& owner = guardian(model);
        owner.weaponSlotCount = 0;
        owner.weaponCooldown = 7;
        owner.weaponSlotTimers = {8, 9, 10};
        owner.behaviorTimer = 23;
    }

    static bool fireGuardian(GameplayModel& model) {
        const auto index = model.level2PostWaveObjectIndex_;
        return model.fireLevel2EnemyProjectile(model.level2Entities_.at(index),
            model.level2EntityActorPoolSlots_.at(index), 0);
    }

    static std::size_t guardianPoolSlot(const GameplayModel& model) {
        return model.level2EntityActorPoolSlots_.at(model.level2PostWaveObjectIndex_);
    }

    static void fillProjectilePool(GameplayModel& model) {
        model.level2ProjectilePoolActive_.fill(true);
    }

    static std::uint16_t loopTick(const GameplayModel& model) {
        return model.level2LoopTick_;
    }
};

} // namespace stellar7

namespace {

constexpr std::size_t noObject = static_cast<std::size_t>(-1);
constexpr std::int16_t guardianType = 44;
constexpr std::uint32_t guardianRadius = 1508;
constexpr std::uint8_t guardianHealth = 50;
constexpr std::uint8_t guardianInterval = 2;
constexpr std::uint8_t guardianClass = 5;
constexpr std::int16_t guardianProjectileType = 22;

std::size_t countType(const stellar7::GameplayModel& model, std::int16_t type) {
    return static_cast<std::size_t>(std::count_if(model.level2Entities().begin(),
        model.level2Entities().end(), [type](const auto& actor) { return actor.type == type; }));
}

void assertBirthAndFirstInheritedBehavior() {
    stellar7::GameplayModel model;
    stellar7::GameplayModel suppressed;
    stellar7::GameplayModelTestAccess::beamReady(model);
    stellar7::GameplayModelTestAccess::beamReady(suppressed);
    stellar7::GameplayModelTestAccess::decodedLevel5GuardianModel(model);
    stellar7::GameplayModelTestAccess::decodedLevel5GuardianModel(suppressed);
    stellar7::GameplayModelTestAccess::suppressWithExistingGuardianPointer(suppressed);

    const auto firstRng = model.level2Rng().calls();
    const auto peerRng = suppressed.level2Rng().calls();
    assert(stellar7::GameplayModelTestAccess::guardianIndex(model) == noObject);

    stellar7::GameplayModelTestAccess::advance(model);
    stellar7::GameplayModelTestAccess::advance(suppressed);

    assert(model.level2Rng().calls() - firstRng
        == suppressed.level2Rng().calls() - peerRng + 2);
    assert(countType(model, guardianType) == 1);
    assert(countType(suppressed, guardianType) == 0);
    assert(std::none_of(model.level2AudioEvents().begin(), model.level2AudioEvents().end(),
        [](const auto& event) { return event.eventId == 17; }));
    assert(model.level2Entities().size() == 2);
    assert(model.level2Entities()[0].behaviorTimer == 29);
    assert((model.level2Entities()[0].flags & 8U) == 0);
    assert(model.level2Entities()[0].renderProfile
        == stellar7::WorldActorRenderProfile::inlineSelectorNoCallback);

    const auto index = stellar7::GameplayModelTestAccess::guardianIndex(model);
    assert(index != noObject);
    assert(index < model.level2Entities().size());
    assert(std::find(stellar7::GameplayModelTestAccess::inheritedOrder(model).begin(),
        stellar7::GameplayModelTestAccess::inheritedOrder(model).end(), index)
        != stellar7::GameplayModelTestAccess::inheritedOrder(model).end());
    const auto& craft = model.level2Entities()[0];
    const auto& guardian = model.level2Entities()[index];
    const auto& presented = stellar7::GameplayModelTestAccess::presentedGuardian(model);
    assert(guardian.type == guardianType);
    assert(guardian.collisionRadius == guardianRadius);
    assert(guardian.stateBytes4dTo52[0] == 0xff);
    assert(guardian.stateBytes4dTo52[1] == guardianHealth);
    assert(guardian.stateBytes4dTo52[2] == guardianInterval);
    assert(guardian.stateBytes4dTo52[3] == guardianInterval);
    assert(guardian.stateBytes4dTo52[4] == guardianClass);
    assert(guardian.stateBytes4dTo52[5] == 0);
    assert(guardian.position.x == craft.position.x);
    assert(guardian.position.y == craft.position.y);
    assert(guardian.position.z == 0);
    assert(guardian.yaw == craft.targetHeading);
    assert(guardian.forwardVelocity == 0);
    assert(guardian.renderProfile
        == stellar7::WorldActorRenderProfile::heapSelectorDeferredAnimation);
    assert(presented.presentationModelAnimationVariant == 0);
    assert(guardian.modelAnimationVariant == 1);
    assert(guardian.presentationModelAnimationVariant == 0);

    const auto birthPosition = guardian.position;
    model.finishLevel5Frame();
    assert(stellar7::GameplayModelTestAccess::guardian(model).position == birthPosition);
    assert(stellar7::GameplayModelTestAccess::guardian(model).modelAnimationVariant == 1);
    assert(stellar7::GameplayModelTestAccess::guardian(model).stateBytes4dTo52[3] == guardianInterval);

    // The next inherited main has aligned heading and source counter 16.
    stellar7::GameplayModelTestAccess::advance(model);
    const auto& aligned = stellar7::GameplayModelTestAccess::guardian(model);
    assert(aligned.type == guardianType);
    assert(aligned.renderProfile == stellar7::WorldActorRenderProfile::heapSelectorNoCallback);
    assert(aligned.modelAnimationVariant == 1);
    assert(aligned.stateBytes4dTo52[3] == guardianInterval - 1);
    assert(stellar7::GameplayModelTestAccess::presentedGuardian(model).presentationModelAnimationVariant == 1);
    assert(countType(model, guardianProjectileType) == 1);
    assert(model.level2AudioEvents().back().eventId == 6);
    assert(model.level2AudioEvents().back().loopTick == 16);
}

void assertUnalignedRestoresCallback() {
    stellar7::GameplayModel model;
    stellar7::GameplayModelTestAccess::beamReady(model);
    stellar7::GameplayModelTestAccess::decodedLevel5GuardianModel(model);
    stellar7::GameplayModelTestAccess::advance(model);
    model.finishLevel5Frame();

    auto& guardian = stellar7::GameplayModelTestAccess::guardian(model);
    const auto before = guardian.position;
    guardian.yaw = static_cast<std::uint16_t>(stellar7::presenterTargetHeading(
        guardian.position, model.level2Player().position) + 0x1000U);
    stellar7::GameplayModelTestAccess::advance(model);

    const auto& restored = stellar7::GameplayModelTestAccess::guardian(model);
    assert(restored.type == guardianType);
    assert(restored.renderProfile
        == stellar7::WorldActorRenderProfile::heapSelectorDeferredAnimation);
    assert(!(restored.position == before));
    assert(restored.yawVelocity == 0);
    assert(restored.stateBytes4dTo52[2] == guardianInterval);
    assert(restored.modelAnimationVariant == 1);
}

void assertConstructorPreflight() {
    stellar7::GameplayModel missingModel;
    stellar7::GameplayModelTestAccess::beamReady(missingModel);
    const auto beforeMissingRng = missingModel.level2Rng().calls();
    bool rejectedMissingModel = false;
    try {
        stellar7::GameplayModelTestAccess::advance(missingModel);
    } catch (const std::exception&) {
        rejectedMissingModel = true;
    }
    assert(rejectedMissingModel);
    assert(missingModel.level2Rng().calls() == beforeMissingRng);
    assert(countType(missingModel, guardianType) == 0);

    stellar7::GameplayModel fullPool;
    stellar7::GameplayModelTestAccess::beamReady(fullPool);
    stellar7::GameplayModelTestAccess::decodedLevel5GuardianModel(fullPool);
    stellar7::GameplayModelTestAccess::fillRemainingOrdinaryPool(fullPool);
    const auto beforeCapacityRng = fullPool.level2Rng().calls();
    bool rejectedCapacity = false;
    try {
        stellar7::GameplayModelTestAccess::advance(fullPool);
    } catch (const std::exception&) {
        rejectedCapacity = true;
    }
    assert(rejectedCapacity);
    assert(fullPool.level2Rng().calls() == beforeCapacityRng);
    assert(countType(fullPool, guardianType) == 0);
}

void assertGuardianShot(stellar7::GameplayModelTestAccess::ProjectileRecord provenance) {
    stellar7::GameplayModel model;
    stellar7::GameplayModelTestAccess::beamReady(model);
    stellar7::GameplayModelTestAccess::decodedLevel5GuardianModel(model);
    stellar7::GameplayModelTestAccess::advance(model);
    stellar7::GameplayModelTestAccess::seedProjectileRecord(model, provenance);
    stellar7::GameplayModelTestAccess::setGuardianWeaponScratch(model);

    const auto owner = stellar7::GameplayModelTestAccess::guardian(model);
    const auto ownerSlot = stellar7::GameplayModelTestAccess::guardianPoolSlot(model);
    auto stepped = stellar7::GameplayModelTestAccess::projectileRecord(model);
    stepped.position = owner.position;
    stepped.zVelocity = 0;
    stepped.yaw = static_cast<std::uint16_t>(owner.yaw - 0x4000U);
    stellar7::advanceEntityKinematics(stepped);
    const auto rngBefore = model.level2Rng().calls();
    const auto eventsBefore = model.level2AudioEvents().size();
    const auto eventTick = static_cast<std::uint16_t>(stellar7::GameplayModelTestAccess::loopTick(model) - 1);

    assert(stellar7::GameplayModelTestAccess::fireGuardian(model));

    assert(model.level2Rng().calls() == rngBefore);
    assert(model.level2AudioEvents().size() == eventsBefore + 1);
    assert(model.level2AudioEvents().back().eventId == 6);
    assert(model.level2AudioEvents().back().loopTick == eventTick);
    assert(countType(model, guardianProjectileType) == 1);
    const auto shot = std::find_if(model.level2Entities().begin(), model.level2Entities().end(),
        [](const auto& actor) { return actor.type == guardianProjectileType; });
    assert(shot != model.level2Entities().end());
    assert(shot->position == stepped.position);
    assert(shot->position.z == owner.position.z);
    assert(shot->collisionRadius == 134);
    assert(shot->stateBytes4dTo52[0] == 0xfd);
    assert(shot->stateBytes4dTo52[1] == 64);
    assert(shot->stateBytes4dTo52[2] == 1);
    assert(shot->stateBytes4dTo52[3] == stepped.stateBytes4dTo52[3]);
    assert(shot->stateBytes4dTo52[4] == 0);
    assert(shot->stateBytes4dTo52[5] == stepped.stateBytes4dTo52[5]);
    assert(shot->forwardVelocity == 500);
    assert(shot->xVelocity == stepped.xVelocity);
    assert(shot->yVelocity == stepped.yVelocity);
    assert(shot->zVelocity == 0);
    assert(shot->yaw == stellar7::fixedHeadingBetween(stepped.position, model.level2Player().position));
    assert(shot->pitch == stellar7::fixedHeadingBetween({},
        {stepped.position.z, static_cast<std::int32_t>(owner.presenterDistance), 0}));
    assert(shot->ownerAllocationIndex == ownerSlot);
    assert(!shot->ownerIsPlayer);
    assert(shot->weaponSlotCount == stepped.weaponSlotCount);
    assert(shot->weaponCooldown == stepped.weaponCooldown);
    assert(shot->weaponSlotTimers == stepped.weaponSlotTimers);
    assert(shot->collisionContactAllocationIndex == stepped.collisionContactAllocationIndex);
    assert(shot->collisionContactIsPlayer == stepped.collisionContactIsPlayer);
    const auto& retainedOwner = stellar7::GameplayModelTestAccess::guardian(model);
    assert(retainedOwner.weaponSlotCount == owner.weaponSlotCount);
    assert(retainedOwner.weaponCooldown == owner.weaponCooldown);
    assert(retainedOwner.weaponSlotTimers == owner.weaponSlotTimers);
    assert(retainedOwner.behaviorTimer == owner.behaviorTimer);
    if (provenance == stellar7::GameplayModelTestAccess::ProjectileRecord::reused) {
        assert(!(shot->position == owner.position));
    }
}

void assertGuardianShotCapacityGuard() {
    stellar7::GameplayModel model;
    stellar7::GameplayModelTestAccess::beamReady(model);
    stellar7::GameplayModelTestAccess::decodedLevel5GuardianModel(model);
    stellar7::GameplayModelTestAccess::advance(model);
    model.finishLevel5Frame();
    stellar7::GameplayModelTestAccess::fillProjectilePool(model);
    const auto beforeRng = model.level2Rng().calls();
    const auto beforeEvents = model.level2AudioEvents().size();
    bool rejected = false;
    try {
        stellar7::GameplayModelTestAccess::advance(model);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    assert(model.level2Rng().calls() == beforeRng);
    assert(model.level2AudioEvents().size() == beforeEvents);
    assert(countType(model, guardianProjectileType) == 0);
}

} // namespace

int main() {
    assertBirthAndFirstInheritedBehavior();
    assertUnalignedRestoresCallback();
    assertConstructorPreflight();
    assertGuardianShot(stellar7::GameplayModelTestAccess::ProjectileRecord::fresh);
    assertGuardianShot(stellar7::GameplayModelTestAccess::ProjectileRecord::reused);
    assertGuardianShotCapacityGuard();
}
