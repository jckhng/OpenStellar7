#include "stellar7/native_checkpoint.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <vector>

namespace stellar7 {

struct GameplayNativeCheckpointAccess {
    static void load(
        GameplayModel& model,
        const std::vector<std::uint8_t>& data
    ) {
        if (data.size() != 0x10000U) {
            throw std::runtime_error(
                "native Level 3 checkpoint DS dump must be 64 KiB"
            );
        }
        const auto u16 = [&data](const std::size_t offset) {
            return static_cast<std::uint16_t>(
                data.at(offset)
                | (static_cast<std::uint16_t>(data.at(offset + 1)) << 8U)
            );
        };
        const auto s16 = [&u16](const std::size_t offset) {
            return static_cast<std::int16_t>(u16(offset));
        };
        const auto u32 = [&data](const std::size_t offset) {
            return static_cast<std::uint32_t>(
                data.at(offset)
                | (static_cast<std::uint32_t>(data.at(offset + 1)) << 8U)
                | (static_cast<std::uint32_t>(data.at(offset + 2)) << 16U)
                | (static_cast<std::uint32_t>(data.at(offset + 3)) << 24U)
            );
        };
        const auto s32 = [&u32](const std::size_t offset) {
            return static_cast<std::int32_t>(u32(offset));
        };
        const auto decodeEntity = [&] (
            const std::uint16_t pointer,
            const std::size_t allocationIndex
        ) {
            if (pointer + 0x61U > data.size()) {
                throw std::runtime_error(
                    "native Level 3 entity pointer leaves DS dump"
                );
            }
            Level1Entity entity{};
            entity.allocationIndex = allocationIndex;
            entity.flags = data.at(pointer);
            entity.type = s16(pointer + 0x02U);
            entity.collisionRadius = u32(pointer + 0x04U);
            entity.position = {
                s32(pointer + 0x08U),
                s32(pointer + 0x0cU),
                s32(pointer + 0x10U),
            };
            entity.pitch = u16(pointer + 0x14U);
            entity.roll = u16(pointer + 0x16U);
            entity.yaw = u16(pointer + 0x18U);
            entity.pitchVelocity = s16(pointer + 0x30U);
            entity.rollVelocity = s16(pointer + 0x32U);
            entity.yawVelocity = s16(pointer + 0x34U);
            entity.xVelocity = s16(pointer + 0x36U);
            entity.yVelocity = s16(pointer + 0x38U);
            entity.zVelocity = s16(pointer + 0x3aU);
            entity.forwardVelocity = s16(pointer + 0x3cU);
            for (std::size_t index = 0; index < 6; ++index) {
                entity.stateBytes4dTo52[index] = data.at(
                    pointer + 0x4dU + index
                );
            }
            entity.behaviorTimer = data.at(pointer + 0x54U);
            entity.presenterDistance = u32(pointer + 0x49U);
            entity.targetHeading = u16(pointer + 0x57U);
            entity.weaponSlotCount = static_cast<std::uint8_t>(
                std::min<std::size_t>(3, data.at(pointer + 0x59U))
            );
            entity.weaponCooldown = data.at(pointer + 0x5aU);
            // The native Level 3 player reports zero weapon slots, while its
            // fire routine scans the two fixed timer bytes at +5b/+5c.
            for (std::size_t index = 0;
                 index < entity.weaponSlotTimers.size(); ++index) {
                entity.weaponSlotTimers[index] = data.at(
                    pointer + 0x5bU + index
                );
            }
            if (entity.type == 11 || entity.type == 44) {
                const auto animationPointer = u16(pointer + 0x1cU);
                if (animationPointer >= data.size()) {
                    throw std::runtime_error(
                        "native Level 3 animation pointer leaves DS dump"
                    );
                }
                entity.presentationModelAnimationVariant = data.at(
                    animationPointer
                );
                entity.modelAnimationVariant =
                    entity.presentationModelAnimationVariant;
            }
            entity.ownerIsPlayer = entity.type == 23;
            return entity;
        };

        const auto entityCountSigned = s16(0x51fcU);
        if (entityCountSigned <= 0 || entityCountSigned > 256) {
            throw std::runtime_error("invalid native Level 3 entity count");
        }
        const auto entityCount = static_cast<std::size_t>(entityCountSigned);
        std::vector<std::uint16_t> activePointers;
        activePointers.reserve(entityCount);
        for (std::size_t index = 0; index < entityCount; ++index) {
            activePointers.push_back(u16(0x45d2U + index * 2U));
        }
        auto sortedPointers = activePointers;
        std::sort(sortedPointers.begin(), sortedPointers.end());
        if (
            std::adjacent_find(sortedPointers.begin(), sortedPointers.end())
            != sortedPointers.end()
        ) {
            throw std::runtime_error(
                "native Level 3 active list contains duplicate pointers"
            );
        }
        std::map<std::uint16_t, std::size_t> allocationByPointer;
        for (std::size_t index = 0; index < sortedPointers.size(); ++index) {
            allocationByPointer.emplace(sortedPointers[index], index);
        }

        model.startLevel3();
        model.phase_ = GameplayPhase::level3;
        model.presentationFrame_ = 0;
        model.level2LoopTick_ = u16(0x12c8U);
        // This isolated diagnostic begins at a campaign-loop boundary, so
        // keep the frontend recording clock on that same logical tick base.
        model.ticks_ = model.level2LoopTick_;
        model.level2HasAdvanced_ = true;
        model.level2PendingInput_ = {};
        model.level2AudioEvents_.clear();
        model.level2PendingTerminalEffectPresentations_.clear();
        model.level2RetiredEffectSemanticEntities_.clear();
        model.level2RetiredEffectSemanticActiveOrder_.clear();
        model.level2RetiredEffectSemanticActivePositions_.clear();
        model.level2EffectSemanticActivePositionOverrides_.clear();
        model.level2RetiredEffectRebuildReported_ = false;
        model.cockpitIndicatorX_ = s16(0x1378U);
        model.cockpitIndicatorVelocity_ = s16(0x137aU);
        model.level2PlayerArmor_ = s16(0x47f8U);
        model.level2ActiveEnemyCount_ = s16(0x12aaU);
        model.level2SpawnedEnemyCount_ = s16(0x12acU);
        model.level2Score_ = u32(0x51f6U);
        model.level2VisualRandomState_ = u32(0x3988U);
        model.level2PlayerCollisionMomentum_ = 0;
        model.level2Type46ContactSlowdown_ = false;
        model.alternateSightPhase_ = false;
        model.level2Complete_ = false;
        model.level3CompletionByPickup_ = false;

        const auto playerPointer = u16(0x522eU);
        model.level2Player_ = decodeEntity(playerPointer, entityCount);

        model.level2Entities_.resize(entityCount);
        for (const auto pointer : sortedPointers) {
            const auto index = allocationByPointer.at(pointer);
            model.level2Entities_[index] = decodeEntity(pointer, index);
        }
        model.level2ActiveListOrder_.clear();
        for (const auto pointer : activePointers) {
            model.level2ActiveListOrder_.push_back(
                allocationByPointer.at(pointer)
            );
        }

        const auto allocationCountSigned = s16(0x4e44U);
        if (allocationCountSigned < 0 || allocationCountSigned > 256) {
            throw std::runtime_error("invalid native allocation-list count");
        }
        const auto allocationCount = static_cast<std::size_t>(
            allocationCountSigned
        );
        model.level2AllocationListOrder_.clear();
        for (std::size_t index = 0; index < allocationCount; ++index) {
            const auto pointer = u16(0x49f8U + index * 2U);
            const auto found = allocationByPointer.find(pointer);
            if (found == allocationByPointer.end()) {
                throw std::runtime_error(
                    "native allocation pointer is absent from active state"
                );
            }
            model.level2AllocationListOrder_.push_back(found->second);
        }
        model.level2ObjectListOrder_ = model.level2AllocationListOrder_;

        model.level2ActorPool_ = {};
        model.level2ActorPoolActive_.fill(false);
        model.level2ProjectilePool_ = {};
        model.level2ProjectilePoolActive_.fill(false);
        model.level2ProjectilePoolWasActiveAtBoundaryStart_.fill(false);
        model.level2DeathEffectPool_ = {};
        model.level2DeathEffectPoolActive_.fill(false);
        model.level2DeathEffectPoolPendingPresenterRetirement_.fill(false);
        model.level2EntityActorPoolSlots_.assign(
            entityCount,
            model.level2ActorPoolSize_
        );
        model.level2EntityProjectilePoolSlots_.assign(
            entityCount,
            model.level2ProjectilePoolSize_
        );
        model.level2EntityDeathEffectPoolSlots_.assign(
            entityCount,
            model.level2DeathEffectPoolSize_
        );

        const auto actorPointer = [](const std::size_t slot) {
            return static_cast<std::uint16_t>(
                slot == 0
                    ? 0xbab2U
                    : 0xbbf8U + (slot - 1U) * 0x66U
            );
        };
        for (std::size_t slot = 0;
             slot < model.level2ActorPoolSize_; ++slot) {
            const auto pointer = actorPointer(slot);
            const auto found = allocationByPointer.find(pointer);
            const auto index = found == allocationByPointer.end()
                ? slot
                : found->second;
            model.level2ActorPool_[slot] = decodeEntity(pointer, index);
            if (found != allocationByPointer.end()) {
                model.level2ActorPoolActive_[slot] =
                    model.level2ActorPool_[slot].active();
                model.level2EntityActorPoolSlots_[found->second] = slot;
            }
        }
        for (std::size_t slot = 0;
             slot < model.level2ProjectilePoolSize_; ++slot) {
            const auto pointer = u16(0x4972U + slot * 2U);
            const auto found = allocationByPointer.find(pointer);
            const auto index = found == allocationByPointer.end()
                ? slot
                : found->second;
            model.level2ProjectilePool_[slot] = decodeEntity(pointer, index);
            const auto active = found != allocationByPointer.end()
                && model.level2ProjectilePool_[slot].active();
            model.level2ProjectilePoolActive_[slot] = active;
            model.level2ProjectilePoolWasActiveAtBoundaryStart_[slot] = active;
            if (found != allocationByPointer.end()) {
                model.level2EntityProjectilePoolSlots_[found->second] = slot;
            }
        }
        for (std::size_t slot = 0;
             slot < model.level2DeathEffectPoolSize_; ++slot) {
            const auto pointer = u16(0x47d8U + slot * 2U);
            const auto found = allocationByPointer.find(pointer);
            const auto index = found == allocationByPointer.end()
                ? slot
                : found->second;
            model.level2DeathEffectPool_[slot] = decodeEntity(pointer, index);
            model.level2DeathEffectPoolActive_[slot] =
                found != allocationByPointer.end()
                && model.level2DeathEffectPool_[slot].active();
            if (found != allocationByPointer.end()) {
                model.level2EntityDeathEffectPoolSlots_[found->second] = slot;
            }
        }

        const auto importContact = [&] (
            Level1Entity& entity,
            const std::uint16_t pointer
        ) {
            const auto contact = u16(pointer + 0x5fU);
            if (contact == playerPointer) {
                entity.collisionContactIsPlayer = true;
            } else {
                const auto found = allocationByPointer.find(contact);
                if (found != allocationByPointer.end()) {
                    entity.collisionContactAllocationIndex = found->second;
                }
            }
        };
        importContact(model.level2Player_, playerPointer);
        for (const auto pointer : sortedPointers) {
            importContact(
                model.level2Entities_[allocationByPointer.at(pointer)],
                pointer
            );
        }
        // Refresh pool copies after contact-pointer resolution.
        for (std::size_t index = 0; index < entityCount; ++index) {
            const auto actorSlot = model.level2EntityActorPoolSlots_[index];
            const auto projectileSlot =
                model.level2EntityProjectilePoolSlots_[index];
            const auto effectSlot =
                model.level2EntityDeathEffectPoolSlots_[index];
            if (actorSlot < model.level2ActorPoolSize_) {
                model.level2ActorPool_[actorSlot] = model.level2Entities_[index];
            }
            if (projectileSlot < model.level2ProjectilePoolSize_) {
                model.level2ProjectilePool_[projectileSlot] =
                    model.level2Entities_[index];
            }
            if (effectSlot < model.level2DeathEffectPoolSize_) {
                model.level2DeathEffectPool_[effectSlot] =
                    model.level2Entities_[index];
            }
        }

        model.level2CompletionObjectIndex_ = static_cast<std::size_t>(-1);
        model.level2PostWaveObjectIndex_ = static_cast<std::size_t>(-1);
        model.level2PickupIndex_ = static_cast<std::size_t>(-1);
        for (std::size_t index = 0; index < entityCount; ++index) {
            const auto type = model.level2Entities_[index].type;
            if (type == 7) {
                model.level2CompletionObjectIndex_ = index;
            } else if (
                (type == 44 || type == 45)
                && model.level2PostWaveObjectIndex_
                    == static_cast<std::size_t>(-1)
            ) {
                model.level2PostWaveObjectIndex_ = index;
            } else if (type == 25 || type == 26) {
                model.level2PickupIndex_ = index;
            }
        }

        std::array<std::uint16_t, StellarRng::wordCount> rngTable{};
        for (std::size_t index = 0; index < rngTable.size(); ++index) {
            rngTable[index] = u16(0x19d6U + index * 2U);
        }
        const auto rngIndexA = u16(0x19d0U);
        const auto rngIndexB = u16(0x19d2U);
        std::uint32_t rngCalls = 0;
        for (; rngCalls < StellarRng::wordCount; ++rngCalls) {
            const auto expectedA = static_cast<std::uint16_t>(
                (0x30 + StellarRng::wordCount * 2 - rngCalls * 2)
                % (StellarRng::wordCount * 2)
            );
            const auto expectedB = static_cast<std::uint16_t>(
                (0x6e + StellarRng::wordCount * 2 - rngCalls * 2)
                % (StellarRng::wordCount * 2)
            );
            if (rngIndexA == expectedA && rngIndexB == expectedB) {
                break;
            }
        }
        if (rngCalls == StellarRng::wordCount) {
            throw std::runtime_error(
                "native Level 3 RNG indices are inconsistent"
            );
        }
        model.level2Rng_ = StellarRng(
            rngTable,
            rngIndexA,
            rngIndexB,
            u16(0x19d4U),
            rngCalls
        );

        const auto groundPointer = u16(0x0730U);
        const auto groundCount = static_cast<std::size_t>(s16(0x4188U));
        if (groundCount != Level1GroundState::pointCount) {
            throw std::runtime_error("native Level 3 ground count is not six");
        }
        std::array<GroundPoint, Level1GroundState::pointCount> ground{};
        for (std::size_t index = 0; index < ground.size(); ++index) {
            ground[index] = {
                s16(groundPointer + index * 6U),
                s16(groundPointer + index * 6U + 2U),
            };
        }
        model.level2Ground_ = Level1GroundState(ground);
        model.level2PresentationEntities_ = model.level2Entities_;
        model.level2PresentationActiveListOrder_ =
            model.level2ActiveListOrder_;
    }
};

void loadLevel3NativeCheckpoint(
    GameplayModel& model,
    const std::vector<std::uint8_t>& data
) {
    GameplayNativeCheckpointAccess::load(model, data);
}

}  // namespace stellar7
