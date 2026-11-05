#include "stellar7/gameplay.hpp"
#include "ordinary_behavior.hpp"
#include <algorithm>
#include <stdexcept>

namespace stellar7 {

void GameplayModel::applyCampaignGroundRidingVelocity(Level1Entity& entity, const Level1Entity& behaviorTarget) noexcept {
    // FUN_1000:9a34 steers Type 13 and the settled Type 47 form toward
    // the player. Outside the 2000-unit axis corridor it selects signed
    // level-speed components directly; inside it uses target heading.
    const auto deltaX = static_cast<std::int64_t>(entity.position.x)
        - behaviorTarget.position.x;
    const auto deltaY = static_cast<std::int64_t>(entity.position.y)
        - behaviorTarget.position.y;
    entity.xVelocity = deltaX < 0 ? 112 : -112;
    entity.yVelocity = deltaY < 0 ? 112 : -112;
    const auto magnitude = [](const std::int64_t value) noexcept {
        return value < 0 ? -value : value;
    };
    if (magnitude(deltaX) <= 1999 || magnitude(deltaY) <= 1999) {
        Level1Entity headingVelocity{};
        headingVelocity.yaw = entity.targetHeading;
        headingVelocity.forwardVelocity = 224;
        advanceEntityKinematics(headingVelocity);
        entity.xVelocity = static_cast<std::int16_t>(
            headingVelocity.position.x
        );
        entity.yVelocity = static_cast<std::int16_t>(
            headingVelocity.position.y
        );
    }
}

void GameplayModel::advanceCampaignType48(Level1Entity& entity) noexcept {
    if (entity.position.z > 0) {
        entity.position.z -= 50;
    }
    if (entity.behaviorTimer != 0) {
        --entity.behaviorTimer;
        if (static_cast<std::int8_t>(entity.behaviorTimer) <= 12) {
            entity.stateBytes4dTo52[2] = static_cast<std::uint8_t>(
                13 - static_cast<std::int8_t>(entity.behaviorTimer)
            );
        }
    } else {
        entity.type = 47;
        if (entity.renderProfile != WorldActorRenderProfile::inlineSelectorNoCallback) {
            entity.renderProfile = WorldActorRenderProfile::unknown;
        }
        entity.behaviorTimer = 60;
        entity.forwardVelocity = 100;
        entity.yaw = level2Rng_.next();
    }
}

std::int32_t GameplayModel::arithmeticShiftRightFour(const std::int32_t value) {
    if (value >= 0) {
        return value / 16;
    }
    return -((-value + 15) / 16);
}

void GameplayModel::advanceCampaignType10(const std::size_t index) {
    constexpr std::uint8_t aimedShotState = 0;
    constexpr std::uint8_t followUpShotState = 1;
    constexpr std::uint8_t offsetShotState = 2;
    constexpr std::uint8_t cooldownState = 3;
    auto& entity = level2Entities_[index];
    const auto behavior = entity.stateBytes4dTo52[ordinaryBehaviorStateByte];
    const auto turnMagnitude = static_cast<std::uint16_t>(
        entity.yawVelocity < 0
            ? -static_cast<std::int32_t>(entity.yawVelocity)
            : entity.yawVelocity
    );
    const auto turnTicks = static_cast<std::uint8_t>(
        turnMagnitude == 0 ? 0 : 0x2000U / turnMagnitude
    );
    if (behavior == aimedShotState) {
        const auto difference = static_cast<std::int16_t>(
            entity.targetHeading - entity.yaw
        );
        const auto magnitude = difference < 0
            ? -static_cast<std::int32_t>(difference)
            : static_cast<std::int32_t>(difference);
        if (
            entity.presenterDistance <= 0x6000U
            && magnitude < 0x200
        ) {
            fireLevel2EnemyProjectile(index, 0);
            level2Entities_[index].behaviorTimer = turnTicks;
            level2Entities_[index].stateBytes4dTo52[ordinaryBehaviorStateByte] = followUpShotState;
        }
        return;
    }
    if (behavior == followUpShotState) {
        if (entity.behaviorTimer == 0) {
            level2Entities_[index].behaviorTimer = turnTicks;
            if (static_cast<std::uint8_t>(level2Rng_.next()) < 210) {
                level2Entities_[index].stateBytes4dTo52[ordinaryBehaviorStateByte] = offsetShotState;
                fireLevel2EnemyProjectile(index, 0);
            } else {
                level2Entities_[index].stateBytes4dTo52[ordinaryBehaviorStateByte] = cooldownState;
            }
        } else {
            --level2Entities_[index].behaviorTimer;
        }
        return;
    }
    if (behavior == offsetShotState) {
        if (entity.behaviorTimer == 0) {
            if (
                static_cast<std::uint8_t>(level2Rng_.next()) < 128
            ) {
                const auto oldSize = level2Entities_.size();
                const auto projectilePoolActiveBefore =
                    level2ProjectilePoolActive_;
                if (
                    fireLevel2EnemyProjectile(index, 0)
                    && level2Entities_.size() > oldSize
                ) {
                    // The native callback adjusts the projectile
                    // returned by FUN_995e.  Pool order is not vector
                    // order, so the new projectile is not necessarily
                    // the last entity after insertion.
                    for (
                        std::size_t poolIndex = 0;
                        poolIndex < level2ProjectilePoolSize_;
                        ++poolIndex
                    ) {
                        if (
                            projectilePoolActiveBefore[poolIndex]
                            || !level2ProjectilePoolActive_[poolIndex]
                        ) {
                            continue;
                        }
                        const auto projectileIt = std::find(
                            level2EntityProjectilePoolSlots_.begin(),
                            level2EntityProjectilePoolSlots_.end(),
                            poolIndex
                        );
                        if (
                            projectileIt
                            == level2EntityProjectilePoolSlots_.end()
                        ) {
                            continue;
                        }
                        auto& projectile = level2Entities_[
                            static_cast<std::size_t>(
                                projectileIt
                                - level2EntityProjectilePoolSlots_.begin()
                            )
                        ];
                        projectile.yaw = static_cast<std::uint16_t>(
                            static_cast<std::int16_t>(projectile.yaw) < 1
                                ? projectile.yaw + 0x6000U
                                : projectile.yaw - 0x6000U
                        );
                        break;
                    }
                }
            }
            level2Entities_[index].behaviorTimer = 0x3f;
            level2Entities_[index].stateBytes4dTo52[ordinaryBehaviorStateByte] = cooldownState;
        } else {
            --level2Entities_[index].behaviorTimer;
        }
        return;
    }
    if (behavior == cooldownState) {
        if (entity.behaviorTimer == 0) {
            level2Entities_[index].stateBytes4dTo52[ordinaryBehaviorStateByte] = aimedShotState;
        } else {
            --level2Entities_[index].behaviorTimer;
        }
        return;
    }
}

void GameplayModel::advanceCampaignType12(const std::size_t index) {
    auto& entity = level2Entities_[index];
    // FUN_1000:952c is the Level 3 Type 12 callback.  Its first
    // state is the only part reached by the original neutral route,
    // but the complete bounded state machine is kept here so later
    // Level 3 probes do not silently fall through to the generic
    // Type 1-4 callback.
    auto behavior = entity.stateBytes4dTo52[ordinaryBehaviorStateByte];
    const auto callbackActorSlot = level2EntityActorPoolSlots_[index];
    // The native callback consumes the shared contact word before
    // dispatching the state table. The portable model stores that
    // word as either of these two fields.
    if (
        entity.collisionContactAllocationIndex
            != static_cast<std::size_t>(-1)
        || entity.collisionContactIsPlayer
    ) {
        entity.collisionContactAllocationIndex =
            static_cast<std::size_t>(-1);
        entity.collisionContactIsPlayer = false;
        behavior = 5;
    }
    if (behavior == 0) {
        if (entity.behaviorTimer == 0) {
            // FUN_1000:9571 loads A00[type] into forward velocity,
            // then FUN_1000:9842 selects the signed packed yaw
            // velocity from A44[type] and retains its high-byte
            // timer. Level 3 Type 12 has A00=360 and A44=2.
            entity.forwardVelocity = 360;
            const auto choice = level2Rng_.next();
            if (static_cast<std::uint8_t>(choice) < 90) {
                entity.yawVelocity = 0;
            } else {
                const auto remainder = static_cast<std::uint16_t>(
                    choice % static_cast<std::uint16_t>(2U << 7U)
                );
                auto yawVelocity = static_cast<std::int16_t>(
                    static_cast<std::uint16_t>(
                        static_cast<std::uint16_t>(
                            static_cast<std::uint8_t>(
                                (remainder >> 8U) + 1U
                            )
                        ) << 8U
                    ) | static_cast<std::uint8_t>(remainder)
                );
                if ((choice & 0x0100U) != 0) {
                    yawVelocity = static_cast<std::int16_t>(
                        -yawVelocity
                    );
                }
                entity.yawVelocity = yawVelocity;
            }
            entity.behaviorTimer = static_cast<std::uint8_t>(
                (choice >> 8U) & 0x7FU
            );
        } else {
            --entity.behaviorTimer;
            if (entity.behaviorTimer == 0) {
                behavior = 1;
            }
        }
    } else if (behavior == 1) {
        const auto difference = static_cast<std::int16_t>(
            entity.targetHeading - entity.yaw
        );
        entity.yawVelocity = static_cast<std::int16_t>(
            std::clamp<std::int32_t>(difference, -0x10f, 0x10f)
        );
        if (
            static_cast<std::uint8_t>(level2Rng_.next()) < 8
        ) {
            behavior = 2;
        }
    } else if (behavior == 2) {
        entity.position.z -= 50;
        if (entity.position.z < 200) {
            behavior = 3;
        }
    } else if (behavior == 3) {
        const auto difference = static_cast<std::int16_t>(
            entity.targetHeading - entity.yaw
        );
        entity.yawVelocity = static_cast<std::int16_t>(
            std::clamp<std::int32_t>(difference, -0x10f, 0x10f)
        );
        const auto insideAttackRange = entity.position.z < 0
            || entity.presenterDistance <= 0x6000U;
        // FUN_1000:98dc gates the attack path before either the
        // weapon-slot timer or the heading test. A held slot timer
        // therefore suppresses the miss gate as well.
        if (
            insideAttackRange
            && entity.weaponSlotTimers[0] == 0
        ) {
            const auto headingDifference = static_cast<std::int32_t>(
                static_cast<std::int16_t>(
                    entity.targetHeading - entity.yaw
                )
            );
            const auto headingMagnitude = headingDifference < 0
                ? -headingDifference
                : headingDifference;
            if (headingMagnitude < 0x100) {
                if (fireLevel2EnemyProjectile(index, 0)) {
                    behavior = 4;
                    // Projectile insertion can precede this actor in
                    // the portable vector. Re-find the owner by its
                    // stable native actor-pool slot after insertion.
                    const auto owner = std::find(
                        level2EntityActorPoolSlots_.begin(),
                        level2EntityActorPoolSlots_.end(),
                        callbackActorSlot
                    );
                    if (owner != level2EntityActorPoolSlots_.end()) {
                        level2Entities_[static_cast<std::size_t>(
                            owner - level2EntityActorPoolSlots_.begin()
                        )].behaviorTimer = 0x3f;
                    }
                }
            } else if (entity.type12MissGate < 2) {
                // Native 1000:9616 sends only the retained low-byte
                // values 0 and 1 to the altitude-return state.
                behavior = 5;
            }
        }
    } else if (behavior == 4) {
        if (entity.behaviorTimer != 0) {
            --entity.behaviorTimer;
        } else {
            behavior = 5;
        }
    } else if (behavior == 5) {
        // The native signed high-word tests leave values already
        // above 1000 untouched; values at or below 1000 rise by 100
        // and clamp to exactly 1000 before returning to state 0.
        if (entity.position.z <= 1000) {
            entity.position.z += 100;
            if (entity.position.z >= 1000) {
                entity.position.z = 1000;
                behavior = 0;
            }
        }
    }
    auto callbackEntityIndex = index;
    if (
        callbackEntityIndex >= level2Entities_.size()
        || level2EntityActorPoolSlots_[callbackEntityIndex]
            != callbackActorSlot
    ) {
        const auto owner = std::find(
            level2EntityActorPoolSlots_.begin(),
            level2EntityActorPoolSlots_.end(),
            callbackActorSlot
        );
        if (owner == level2EntityActorPoolSlots_.end()) {
            return;
        }
        callbackEntityIndex = static_cast<std::size_t>(
            owner - level2EntityActorPoolSlots_.begin()
        );
    }
    auto& callbackEntity = level2Entities_[callbackEntityIndex];
    callbackEntity.stateBytes4dTo52[ordinaryBehaviorStateByte] = behavior;
    // 1000:966e is shared by every Type 12 state path. It decrements
    // the first weapon-slot timer and applies the signed 16-bit roll
    // damping generated by the native assembly at 1000:9677.
    if (callbackEntity.weaponSlotTimers[0] != 0) {
        --callbackEntity.weaponSlotTimers[0];
    }
    const auto rollInput = static_cast<std::int16_t>(
        static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(callbackEntity.yawVelocity)
                << 4U
        )
            + callbackEntity.roll
    );
    const auto rollDelta = arithmeticShiftRightFour(-rollInput);
    callbackEntity.roll = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(callbackEntity.roll) + rollDelta
    );
}

void GameplayModel::advanceCampaignType13(Level1Entity& entity, const Level1Entity& behaviorTarget) {
    if (entity.collisionContactIsPlayer) {
        return;
    }
    if (entity.collisionContactAllocationIndex < level2Entities_.size()) {
        const auto& contact = level2Entities_[entity.collisionContactAllocationIndex];
        if (contact.type != 13) {
            entity.position.z = std::min<std::int32_t>(1000, entity.position.z + 45);
            entity.collisionContactAllocationIndex = static_cast<std::size_t>(-1);
        }
        return;
    }
    applyCampaignGroundRidingVelocity(entity, behaviorTarget);
    entity.position.z = std::max<std::int32_t>(0, entity.position.z - 45);
}

void GameplayModel::advanceCampaignOrdinaryWithFire(Level1Entity& actor, const std::size_t ownerSlot,
    const Level1Entity& behaviorTarget) {
    if (actor.type == 11) {
        actor.stateBytes4dTo52[5] = advanceType11Behavior(actor, behaviorTarget, level2Rng_,
            [this, ownerSlot](Level1Entity& current) {
                return fireLevel2EnemyProjectile(current, ownerSlot, 0);
            });
        return;
    }
    const auto fire = [this, ownerSlot](Level1Entity& current) {
        return fireLevel2EnemyProjectile(current, ownerSlot, -1);
    };
    if (actor.type == 5) {
        advanceType5Behavior(actor, level2Rng_, static_cast<std::uint16_t>(activeCampaignLevel_ + 1), fire);
    } else {
        advanceType8Behavior(actor, fire);
    }
}

void GameplayModel::advanceLiveCampaignOrdinary(const std::size_t index, const Level1Entity& behaviorTarget) {
    const auto ownerSlot = level2EntityActorPoolSlots_[index];
    auto actor = level2Entities_[index];
    advanceCampaignOrdinaryWithFire(actor, ownerSlot, behaviorTarget);
    // Firing can move compact indices. Commit through the original physical slot.
    const auto owner = std::find(level2EntityActorPoolSlots_.begin(),
        level2EntityActorPoolSlots_.end(), ownerSlot);
    if (owner == level2EntityActorPoolSlots_.end()) {
        throw std::logic_error("Type11 behavior lost its ordinary owner");
    }
    level2Entities_[static_cast<std::size_t>(owner - level2EntityActorPoolSlots_.begin())] = actor;
}

void GameplayModel::advanceCampaignType6(const std::size_t index) {
    const auto ownerSlot=level2EntityActorPoolSlots_[index];
    auto actor=level2Entities_[index];
    advanceType6Behavior(actor,level2Rng_,{actor.type6CloakTimer,
        static_cast<std::uint8_t>(activeCampaignLevel_),
            playableCampaignFrames_ && level1_.systemTimers_[4]!=0
                ? Type6ScannerState::active : Type6ScannerState::inactive},
        [this,ownerSlot](auto& owner) { return fireLevel2EnemyProjectile(owner,ownerSlot,-1); });
    const auto owner=std::find(level2EntityActorPoolSlots_.begin(),level2EntityActorPoolSlots_.end(),ownerSlot);
    if (owner==level2EntityActorPoolSlots_.end()) {
        throw std::logic_error("Type6 behavior lost its physical owner");
    }
    level2Entities_[static_cast<std::size_t>(owner-level2EntityActorPoolSlots_.begin())]=actor;
}

void GameplayModel::advanceCampaignType9(const std::size_t index) {
    const auto ownerSlot = level2EntityActorPoolSlots_[index];
    auto current = level2Entities_[index];
    advanceType9Behavior(current,[this,ownerSlot](Level1Entity& owner) {
        return fireLevel2EnemyProjectile(owner,ownerSlot,-1);
    });
    const auto owner = std::find(level2EntityActorPoolSlots_.begin(),level2EntityActorPoolSlots_.end(),ownerSlot);
    if (owner == level2EntityActorPoolSlots_.end()) {
        throw std::logic_error("Type9 fire lost its physical owner");
    }
    level2Entities_[static_cast<std::size_t>(owner-level2EntityActorPoolSlots_.begin())] = current;
}

} // namespace stellar7
