#pragma once
#include "stellar7/gameplay.hpp"
#include "stellar7/native_checkpoint.hpp"
#include <algorithm>
#include <stdexcept>

namespace stellar7 {

struct GameplayAnalysisAccess {
    static const auto& failureSourceTick(const GameplayModel& model) noexcept {
        return model.failureSourceLoopTick_;
    }

    static std::int16_t level2ActiveEnemyCount(
        const GameplayModel& model
    ) noexcept {
        return model.level2ActiveEnemyCount_;
    }

    static std::int16_t level2SpawnedEnemyCount(
        const GameplayModel& model
    ) noexcept {
        return model.level2SpawnedEnemyCount_;
    }

    static std::int16_t level2PlayerCollisionMomentum(
        const GameplayModel& model
    ) noexcept {
        return model.level2PlayerCollisionMomentum_;
    }

    static bool level2Type46ContactSlowdown(
        const GameplayModel& model
    ) noexcept {
        return model.level2Type46ContactSlowdown_;
    }

    static const GameplayInput& level2PendingInput(
        const GameplayModel& model
    ) noexcept {
        return model.level2PendingInput_;
    }

    static const std::vector<std::size_t>& level2AllocationListOrder(
        const GameplayModel& model
    ) noexcept {
        return model.level2AllocationListOrder_;
    }

    static const auto& level2DeathEffectPool(
        const GameplayModel& model
    ) noexcept {
        return model.level2DeathEffectPool_;
    }

    static const auto& level2ProjectilePool(
        const GameplayModel& model
    ) noexcept {
        return model.level2ProjectilePool_;
    }

    static const auto& level2ProjectilePoolActive(
        const GameplayModel& model
    ) noexcept {
        return model.level2ProjectilePoolActive_;
    }

    static const auto& level2EntityProjectilePoolSlots(
        const GameplayModel& model
    ) noexcept {
        return model.level2EntityProjectilePoolSlots_;
    }

    static const auto& level2DeathEffectPoolActive(
        const GameplayModel& model
    ) noexcept {
        return model.level2DeathEffectPoolActive_;
    }

    static const auto& level2DeathEffectPoolPendingRetirement(
        const GameplayModel& model
    ) noexcept {
        return model.level2DeathEffectPoolPendingPresenterRetirement_;
    }

    static const auto& level2EntityDeathEffectPoolSlots(
        const GameplayModel& model
    ) noexcept {
        return model.level2EntityDeathEffectPoolSlots_;
    }

    static const auto& level2RetiredEffectSemanticEntities(
        const GameplayModel& model
    ) noexcept {
        return model.level2RetiredEffectSemanticEntities_;
    }

    static const auto& level2RetiredEffectSemanticActiveOrder(
        const GameplayModel& model
    ) noexcept {
        return model.level2RetiredEffectSemanticActiveOrder_;
    }

    static bool level2RetiredEffectRebuildReported(
        const GameplayModel& model
    ) noexcept {
        return model.level2RetiredEffectRebuildReported_;
    }

    static std::size_t level2RetiredEffectSemanticCount(
        const GameplayModel& model
    ) noexcept {
        return model.level2RetiredEffectSemanticEntities_.size();
    }

    static std::vector<Level1Entity> level2SemanticEntities(const GameplayModel& model) {
        return model.level2SemanticEntities();
    }

    static std::vector<std::size_t> level2SemanticAllocationListOrder(
        const GameplayModel& model
    ) {
        auto order = model.level2AllocationListOrder_;
        auto retired = model.level2RetiredEffectSemanticEntities_;
        std::sort(
            retired.begin(),
            retired.end(),
            [](const Level1Entity& left, const Level1Entity& right) {
                return left.allocationIndex < right.allocationIndex;
            }
        );
        for (const auto& item : retired) {
            for (auto& index : order) {
                if (index >= item.allocationIndex) {
                    ++index;
                }
            }
        }
        return order;
    }

    static std::vector<std::size_t> level2SemanticActiveListOrder(const GameplayModel& model) {
        return model.level2SemanticActiveListOrder();
    }

    static void applyLevel2Armor(
        GameplayModel& model,
        const std::int16_t armor
    ) {
        if (
            model.phase_ != GameplayPhase::level2Handoff
            && model.phase_ != GameplayPhase::level2
        ) {
            throw std::runtime_error(
                "Level 2 armor override requires initialized Level 2"
            );
        }
        model.level2PlayerArmor_ = armor;
    }

    static void applyLevel3Armor(
        GameplayModel& model,
        const std::int16_t armor
    ) {
        if (model.phase_ != GameplayPhase::level3) {
            throw std::runtime_error(
                "Level 3 armor override requires active Level 3"
            );
        }
        model.level2PlayerArmor_ = armor;
    }

    static void applyLevel2ActorWarp(
        GameplayModel& model,
        const std::size_t allocationIndex,
        const FixedPosition position,
        const std::uint8_t health,
        const std::int16_t armor
    ) {
        if (model.phase_ != GameplayPhase::level2) {
            throw std::runtime_error(
                "controlled actor warp requires active Level 2"
            );
        }
        const auto actor = std::find_if(
            model.level2Entities_.begin(),
            model.level2Entities_.end(),
            [allocationIndex](const Level1Entity& entity) {
                return entity.allocationIndex == allocationIndex;
            }
        );
        if (actor == model.level2Entities_.end()) {
            throw std::runtime_error(
                "controlled actor warp allocation is not active"
            );
        }
        actor->position = position;
        actor->stateBytes4dTo52[1] = health;
        const auto presented = std::find_if(
            model.level2PresentationEntities_.begin(),
            model.level2PresentationEntities_.end(),
            [allocationIndex](const Level1Entity& entity) {
                return entity.allocationIndex == allocationIndex;
            }
        );
        if (presented != model.level2PresentationEntities_.end()) {
            presented->position = position;
            presented->stateBytes4dTo52[1] = health;
        }
        model.level2PlayerArmor_ = armor;
    }

    // Capture-only setup for comparing the shared terminal presenter without
    // depending on a wall-clock guardian route.  It initializes the real
    // Level 3 state, then places the model at the native completion phase;
    // no renderer or gameplay rule is bypassed after this boundary.
    static void prepareLevel3Success(
        GameplayModel& model,
        const unsigned presentationFrame
    ) {
        model.startLevel3();
        model.phase_ = GameplayPhase::level3SuccessPresentation;
        model.presentationFrame_ = presentationFrame;
    }

    static void prepareLevel3PickupSuccess(
        GameplayModel& model,
        const unsigned presentationFrame
    ) {
        model.startLevel3();
        model.phase_ = GameplayPhase::level3SuccessPresentation;
        model.level3CompletionByPickup_ = true;
        model.presentationFrame_ = presentationFrame;
    }

    // Target-local runtime checkpoint bridge shared by renderer probes and
    // the interactive guardian recorder.
    static void loadLevel3NativeCheckpoint(
        GameplayModel& model,
        const std::vector<std::uint8_t>& data
    ) {
        stellar7::loadLevel3NativeCheckpoint(model, data);
    }
};

}

