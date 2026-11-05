#include "campaign_contact.hpp"
#include "playable_decoy.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>

namespace stellar7 {

GameplayModel::CampaignActorWalkResult GameplayModel::advanceCampaignActorWalk(
    const CampaignContactStart& start, const Level1Entity& behaviorTarget,
    const CampaignProjectileContactResult& projectileContact, const CampaignCombatCallbacks& combat,
    const bool physicalLevel2Guardian) {
    const auto campaignGuardians = start.guardianHitActions;
    // FUN_1000:9298 walks the inherited pointer list. Rebuilds and vector
    // insertion can change portable indices without changing those pointer
    // identities, so remap the boundary-start pool records after membership
    // changes instead of substituting the newly rebuilt index list.
    const auto behaviorOrder = remapLevel2BoundaryOrder(start.identities);
    CampaignActorWalkResult result;
    const auto behaviorCount = activeCampaignLevel_ == 4 || campaignGuardians || physicalLevel2Guardian
        ? start.identities.size() : behaviorOrder.size();
    for (std::size_t position = 0; position < behaviorCount; ++position) {
        auto index = static_cast<std::size_t>(-1);
        if (activeCampaignLevel_ == 4 || campaignGuardians || physicalLevel2Guardian) {
            const auto identity = start.identities[position];
            if (identity.pool != 0) {
                if(playableCampaignFrames_ && level1_.systemTimers_[0]!=0) {
                    auto& slots=identity.pool==1 ? level2EntityProjectilePoolSlots_
                        : identity.pool==2 ? level2EntityDeathEffectPoolSlots_ : level2EntityFixedPoolSlots_;
                    const auto live=std::find(slots.begin(),slots.end(),identity.slot);
                    auto& physical=identity.pool==1 ? level2ProjectilePool_[identity.slot]
                        : identity.pool==2 ? level2DeathEffectPool_[identity.slot] : level2FixedPool_[identity.slot];
                    if(live!=slots.end()) {
                        auto& actor=level2Entities_[static_cast<std::size_t>(live-slots.begin())];
                        refreshPlayableTarget(actor,behaviorTarget);physical=actor;
                    } else {refreshPlayableTarget(physical,behaviorTarget);}
                }
                continue;
            }
            const auto owner = std::find(level2EntityActorPoolSlots_.begin(),
                level2EntityActorPoolSlots_.end(), identity.slot);
            if (owner == level2EntityActorPoolSlots_.end()) {
                //481a removed allocation membership;9298 still owns this pool
                //pointer. Common motion already ran before the separated hit.
                auto& retired = level2ActorPool_[identity.slot];
                if (physicalLevel2Guardian) {
                    if (retired.type == 44 || retired.type == 45) {
                        advancePhysicalLevel2Guardian(identity.slot, behaviorTarget, projectileContact, result.guardianAllocatedThisBoundary);
                    } else if (retired.type == 48) {
                        refreshPlayableTarget(retired, behaviorTarget);
                        advanceCampaignType48(retired);
                    }
                    continue;
                }
                if(playableCampaignFrames_ && level1_.systemTimers_[0]!=0) {refreshPlayableTarget(retired,behaviorTarget);}
                if (campaignGuardians) {
                    if (retired.type == 44) {
                        auto shadow = retired;
                        shadow.presenterDistance = approximatePlanarDistance(behaviorTarget.position, shadow.position);
                        shadow.targetHeading = presenterTargetHeading(shadow.position, behaviorTarget.position);
                        advanceCampaignGuardian(shadow, identity.slot, start.identities, combat);
                    } else if (retired.type == 11 || retired.type == 5 || retired.type == 8) {
                        retired.presenterDistance = approximatePlanarDistance(behaviorTarget.position, retired.position);
                        retired.targetHeading = presenterTargetHeading(retired.position, behaviorTarget.position);
                        advanceCampaignOrdinaryWithFire(retired, identity.slot, behaviorTarget);
                    }
                    continue;
                }
                retired.presenterDistance = approximatePlanarDistance(behaviorTarget.position, retired.position);
                retired.targetHeading = presenterTargetHeading(retired.position, behaviorTarget.position);
                if (retired.type == 11 || retired.type == 5 || retired.type == 8) {
                    advanceCampaignOrdinaryWithFire(retired, identity.slot, behaviorTarget);
                } else if (retired.type == 13) {
                    if (retired.collisionContactAllocationIndex != static_cast<std::size_t>(-1)) {
                        throw std::invalid_argument("Level5 removed Type13 contact requires retained pointer identity");
                    }
                    advanceCampaignType13(retired, behaviorTarget);
                } else if (retired.type == 44) {
                    advanceLevel5Guardian(retired, identity.slot);
                } else {
                    throw std::invalid_argument("Level5 removed ordinary requires its inherited behavior callback");
                }
                continue;
            }
            index = static_cast<std::size_t>(owner - level2EntityActorPoolSlots_.begin());
        } else {
            index = behaviorOrder[position];
        }
        if (index >= level2Entities_.size()) {
            continue;
        }
        if (!physicalLevel2Guardian && !campaignActorExistedAtStart(index, start)) {
            continue;
        }
        auto& entity = level2Entities_[index];
        if ((physicalLevel2Guardian && entity.type >= 44 && entity.type <= 48)
            || (playableCampaignFrames_ && level1_.systemTimers_[0] != 0)) {
            refreshPlayableTarget(entity, behaviorTarget);
        }
        if (entity.type == 7) {
            const auto action = advanceCampaignArrivalCraft(entity);
            if (action == CampaignArrivalAction::deployGuardian) {
                result.postWaveSource = entity;
            } else if (action == CampaignArrivalAction::retire) {
                result.retireCompletionObject = index;
            }
            continue;
        }
        if (entity.type == 47 || entity.type == 48) {
            if (entity.type == 48) {
                advanceCampaignType48(entity);
                continue;
            }
            if (entity.position.z > 0) {
                entity.position.z -= 50;
            }
            if (entity.behaviorTimer != 0) {
                --entity.behaviorTimer;
                advanceEntityKinematics(entity);
            } else {
                entity.yaw = entity.targetHeading;
                entity.stateBytes4dTo52[2] = static_cast<std::uint8_t>(
                    (entity.stateBytes4dTo52[2] + 1U) & 0x03U
                );
                if(activeCampaignLevel_==2) {
                    constexpr std::uint8_t alternateStride=4;
                    auto stride=entity.presentationModelAnimationVariant&alternateStride;
                    if(entity.stateBytes4dTo52[2]==0) {stride^=alternateStride;}
                    entity.presentationModelAnimationVariant=static_cast<std::uint8_t>(stride|entity.stateBytes4dTo52[2]);
                }
                applyCampaignGroundRidingVelocity(entity, behaviorTarget);
            }
            continue;
        }
        if (entity.type == 44 || entity.type == 45) {
            if (physicalLevel2Guardian) {
                advancePhysicalLevel2Guardian(level2EntityActorPoolSlots_[index], behaviorTarget, projectileContact, result.guardianAllocatedThisBoundary);
                continue;
            }
            if (campaignGuardians) {
                if (entity.type != 44) {
                    throw std::invalid_argument("campaign guardian has no Type45 callback");
                }
                const auto ownerSlot = level2EntityActorPoolSlots_[index];
                auto shadow = entity;
                advanceCampaignGuardian(shadow, ownerSlot, start.identities, combat);
                continue;
            }
            if (activeCampaignLevel_ == 0 || activeCampaignLevel_ == 3 || activeCampaignLevel_ == 5 || activeCampaignLevel_ == 6) {
                throw std::invalid_argument("later guardian requires explicit playable campaign execution");
            }
            if (activeCampaignLevel_ == 4) {
                if (entity.type != 44) {
                    throw std::invalid_argument("Level5 has no Type45 guardian model");
                }
                const auto ownerSlot = level2EntityActorPoolSlots_[index];
                auto current = entity;
                advanceLevel5Guardian(current, ownerSlot);
                const auto owner = std::find(level2EntityActorPoolSlots_.begin(), level2EntityActorPoolSlots_.end(), ownerSlot);
                level2Entities_[static_cast<std::size_t>(owner - level2EntityActorPoolSlots_.begin())] = current;
                continue;
            }
            if (activeCampaignLevel_ == 2) {
                if (auto burst = advanceLevel3Guardian(entity)) {
                    result.level3Burst = std::move(burst);
                }
                continue;
            }
            const auto choice = level2Rng_.next();
            auto& behavior = entity.stateBytes4dTo52[5];
            const auto guardianSlot = level2EntityActorPoolSlots_[index];
            const auto retiredContact = guardianSlot < level2ActorPoolSize_
                && projectileContact.guardianRetiredContact[guardianSlot];
            if (behavior == 0) {
                entity.forwardVelocity = 100;
                const auto difference = static_cast<std::int16_t>(
                    entity.yaw - entity.targetHeading
                );
                entity.yawVelocity = static_cast<std::int16_t>(
                    std::clamp<std::int32_t>(
                        difference == std::numeric_limits<std::int16_t>::min()
                            ? 0x8000
                            : difference,
                        -0x200,
                        0x200
                    )
                );
                if (entity.renderProfile != WorldActorRenderProfile::heapSelectorDeferredAnimation) {
                    entity.renderProfile = WorldActorRenderProfile::unknown;
                }
                entity.type = 44;
                if (choice < 300U) {
                    behavior = 1;
                    entity.behaviorTimer = 36;
                }
                if (
                    (choice & 0x003fU) == 0
                    && level2CompletionObjectIndex_
                        == static_cast<std::size_t>(-1)
                    && entity.collisionContactAllocationIndex
                        == static_cast<std::size_t>(-1)
                    && !entity.collisionContactIsPlayer
                ) {
                    result.spawnGuardianType47 = activeCampaignLevel_ == 2;
                    result.spawnGuardianType48 = !result.spawnGuardianType47 && !retiredContact;
                    result.guardianType48Source = entity;
                }
                // FUN_1000:a92e consumes the shared contact pointer after
                // every behavior-0 callback that does not return through a
                // successful Type 48 launch. A successful launch necessarily
                // entered with a null pointer, so clearing both portable
                // contact representations preserves that path as well.
                entity.collisionContactAllocationIndex =
                    static_cast<std::size_t>(-1);
                entity.collisionContactIsPlayer = false;
            } else if (behavior == 1) {
                const auto timerDistance = std::abs(
                    static_cast<int>(entity.behaviorTimer) - 20
                );
                entity.presentationModelAnimationVariant =
                    static_cast<std::uint8_t>(timerDistance >> 2);
                if (activeCampaignLevel_ == 1) {
                    entity.modelAnimationVariant =
                        entity.presentationModelAnimationVariant;
                }
                entity.yawVelocity = 0;
                entity.forwardVelocity = 0;
                entity.type = 45;
                if (entity.renderProfile != WorldActorRenderProfile::heapSelectorDeferredAnimation) {
                    entity.renderProfile = WorldActorRenderProfile::unknown;
                }
                --entity.behaviorTimer;
                if (
                    entity.behaviorTimer == 20
                    && !retiredContact
                    && entity.collisionContactAllocationIndex
                        == static_cast<std::size_t>(-1)
                    && !entity.collisionContactIsPlayer
                ) {
                    Level1Entity emitted{};
                    emitted.type = 46;
                    emitted.collisionRadius = entity.collisionRadius == 0
                        ? 0
                        : 2260;
                    emitted.position = entity.position;
                    emitted.stateBytes4dTo52 = {
                        254, 100, 0, 0, 0, 0,
                    };
                    allocateLevel2FixedObject(emitted);
                } else if (entity.behaviorTimer == 0) {
                    behavior = 0;
                }
            }
            continue;
        }
        if (entity.type == 25 || entity.type == 26) {
            // FE gates have no 094F actor callback. The playable 0784 epilogue owns their oscillator.
            if (playableCampaignFrames_) {continue;}
            auto& presentation = entity.stateBytes4dTo52[2];
            auto& direction = entity.stateBytes4dTo52[5];
            if (static_cast<std::int8_t>(presentation) < 1) {
                direction = 1;
            } else if (static_cast<std::int8_t>(presentation) > 2) {
                direction = 0xff;
            }
            presentation = static_cast<std::uint8_t>(
                presentation + static_cast<std::int8_t>(direction)
            );
            continue;
        }
        if (entity.type == 12) {
            advanceCampaignType12(index);
            continue;
        }
        if ((activeCampaignLevel_ == 4 || playableCampaignFrames_)
            && (entity.type == 11 || entity.type == 5 || entity.type == 8)) {
            advanceLiveCampaignOrdinary(index, behaviorTarget);
            continue;
        }
        if (entity.type >= 1 && entity.type <= 5) {
            const auto type = entity.type;
            constexpr std::array<std::uint8_t, 6> choice2LowerByType{{
                0, 50, 32, 48, 50, 16,
            }};
            constexpr std::array<std::uint8_t, 6> choice3LowerByType{{
                0, 102, 96, 128, 96, 110,
            }};
            constexpr std::array<std::int16_t, 6> cruiseSpeedByType{{
                0, 225, 144, 216, 180, 135,
            }};
            constexpr std::array<std::int16_t, 6> reverseSpeedByType{{
                0, 45, 72, 72, 90, 67,
            }};
            constexpr std::array<std::int16_t, 6> turnFactorByType{{
                0, 1, 2, 3, 2, 2,
            }};
            constexpr std::array<std::uint8_t, 6> behavior2ResetByType{{
                0, 12, 6, 10, 8, 4,
            }};
            constexpr std::array<std::uint8_t, 6> behavior3ResetByType{{
                0, 5, 2, 6, 6, 4,
            }};
            const auto typeIndex = static_cast<std::size_t>(type);
            const auto choice2Lower = choice2LowerByType[typeIndex];
            const auto choice3Lower = choice3LowerByType[typeIndex];
            const auto turnLimit = static_cast<std::int16_t>(
                turnFactorByType[typeIndex] * 128 + 15
            );
            const auto cruiseSpeed = cruiseSpeedByType[typeIndex];
            const auto reverseSpeed = reverseSpeedByType[typeIndex];
            const auto turnFactor = turnFactorByType[typeIndex];
            const auto behavior2Reset = behavior2ResetByType[typeIndex];
            const auto behavior3Reset = behavior3ResetByType[typeIndex];
            auto behavior = entity.stateBytes4dTo52[5];
            const auto insideAttackRange = entity.position.z < 0
                || entity.presenterDistance <= 0x6000U;
            if (behavior == 0) {
                entity.behaviorTimer = 0;
                const auto choice = static_cast<std::uint8_t>(
                    level2Rng_.next()
                );
                behavior = choice < choice2Lower
                    ? 1
                    : (choice < choice3Lower ? 2 : 3);
            }
            if (behavior == 1) {
                auto& item = level2Entities_[index];
                if (item.behaviorTimer == 0) {
                    item.forwardVelocity = cruiseSpeed;
                    const auto choice = level2Rng_.next();
                    if (static_cast<std::uint8_t>(choice) < 90) {
                        item.yawVelocity = 0;
                    } else {
                        auto turn = static_cast<std::int16_t>(
                            static_cast<std::int16_t>(choice)
                                % (turnFactor << 7)
                            + 256
                        );
                        if ((choice & 0x0100U) != 0) {
                            turn = static_cast<std::int16_t>(-turn);
                        }
                        item.yawVelocity = turn;
                    }
                    item.behaviorTimer = static_cast<std::uint8_t>(
                        (choice >> 8U) & 0x7FU
                    );
                } else {
                    --item.behaviorTimer;
                    if (item.behaviorTimer == 0) {
                        behavior = 0;
                    }
                    if (insideAttackRange) {
                        fireLevel2EnemyProjectile(index);
                    }
                }
            } else if (behavior == 2) {
                auto& item = level2Entities_[index];
                item.forwardVelocity = 0;
                const auto difference = static_cast<std::int16_t>(
                    item.targetHeading - item.yaw
                );
                item.yawVelocity = static_cast<std::int16_t>(
                    std::clamp<std::int32_t>(
                        difference,
                        -turnLimit,
                        turnLimit
                    )
                );
                if (
                    !insideAttackRange
                    || fireLevel2EnemyProjectile(index)
                ) {
                    behavior = 0;
                }
                if (
                    static_cast<std::uint8_t>(level2Rng_.next())
                    < behavior2Reset
                ) {
                    behavior = 0;
                }
            } else if (behavior == 3) {
                auto& item = level2Entities_[index];
                const auto difference = static_cast<std::int16_t>(
                    item.targetHeading - item.yaw
                );
                item.yawVelocity = static_cast<std::int16_t>(
                    std::clamp<std::int32_t>(
                        difference,
                        -turnLimit,
                        turnLimit
                    )
                );
                if (insideAttackRange) {
                    fireLevel2EnemyProjectile(index);
                }
                if (
                    static_cast<std::uint8_t>(level2Rng_.next())
                    < behavior3Reset
                ) {
                    behavior = 0;
                }
            } else if (behavior == 4) {
                auto& item = level2Entities_[index];
                item.collisionContactAllocationIndex =
                    static_cast<std::size_t>(-1);
                item.collisionContactIsPlayer = false;
                if (item.behaviorTimer == 0) {
                    const auto choice = level2Rng_.next();
                    item.forwardVelocity = item.forwardVelocity < 1
                        ? cruiseSpeed
                        : static_cast<std::int16_t>(-reverseSpeed);
                    const auto turn = static_cast<std::int16_t>(
                        turnFactor << 8
                    );
                    item.yawVelocity = (choice & 0x0200U) != 0
                        ? static_cast<std::int16_t>(-turn)
                        : turn;
                    item.behaviorTimer = 0x3F;
                } else {
                    --item.behaviorTimer;
                    if (item.behaviorTimer == 0) {
                        behavior = 0;
                        item.forwardVelocity = cruiseSpeed;
                    }
                    if (insideAttackRange) {
                        fireLevel2EnemyProjectile(index);
                    }
                }
            } else if (behavior == 5) {
                auto& item = level2Entities_[index];
                item.forwardVelocity = cruiseSpeed;
                const auto choice = level2Rng_.next();
                if (static_cast<std::uint8_t>(choice) < 90) {
                    item.yawVelocity = 0;
                } else {
                    auto turn = static_cast<std::int16_t>(
                        static_cast<std::int16_t>(choice)
                            % (turnFactor << 7)
                        + 256
                    );
                    if ((choice & 0x0100U) != 0) {
                        turn = static_cast<std::int16_t>(-turn);
                    }
                    item.yawVelocity = turn;
                }
                item.behaviorTimer = static_cast<std::uint8_t>(
                    (choice >> 8U) & 0x7FU
                );
            }
            level2Entities_[index].stateBytes4dTo52[5] = behavior;
            continue;
        }
        if (entity.type == 8) {
            const auto difference = static_cast<std::int16_t>(
                entity.targetHeading - entity.yaw
            );
            entity.yawVelocity = static_cast<std::int16_t>(
                std::clamp<std::int32_t>(difference, -143, 143)
            );

            if (entity.presenterDistance <= 0x6000U) {
                fireLevel2EnemyProjectile(index);
            }
            continue;
        }
        if (playableCampaignFrames_ && entity.type == 6) {
            advanceCampaignType6(index);
            continue;
        }
        if (playableCampaignFrames_ && entity.type == 9) {
            advanceCampaignType9(index);
            continue;
        }
        if (entity.type == 10) {
            advanceCampaignType10(index);
            continue;
        }
        if (entity.type != 13) {
            continue;
        }
        advanceCampaignType13(entity, behaviorTarget);
    }
    return result;
}

} // namespace stellar7
