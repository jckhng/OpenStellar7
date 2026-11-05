#include "stellar7/gameplay.hpp"
#include <algorithm>
#include <stdexcept>

namespace stellar7 {

void GameplayModel::deployCampaignReward(const Level1Entity& pendingEffectSource, const CampaignExit exit,
    const std::vector<Level2EntityIdentity>& identities, const CampaignRewardDomain domain) {
    const auto retainedReward = activeCampaignLevel_ == 4 || (playableCampaignFrames_
        && (activeCampaignLevel_ == 0 || activeCampaignLevel_ == 3 || activeCampaignLevel_ == 5 || activeCampaignLevel_ == 6));
    if ((exit == CampaignExit::blueWarp && !level2SecondaryPickupKnownNull_)
        || (exit == CampaignExit::greenReward && retainedReward
            && level2PickupIndex_ != static_cast<std::size_t>(-1))) {
        return;
    }

    const auto choice = level2Rng_.next();
    Level1Entity pickup{};
    pickup.type = static_cast<std::int16_t>(exit);
    // LEVEL3.RES has no model-radius record for the shared Type-25
    // completion object.  Native FUN_1000:45c6 therefore leaves its
    // collision radius at zero on the Level 3 allocation path.  The
    // Level 2 resource supplies the ordinary 897-radius pickup.
    pickup.collisionRadius = playableCampaignFrames_ && campaignModelsBound_ ? campaignModels_[pickup.type].radius
        : activeCampaignLevel_ == 2 ? 0 : 897;
    pickup.position = pendingEffectSource.position;
    pickup.position.z = 0;
    pickup.yaw = static_cast<std::uint16_t>(
        pendingEffectSource.targetHeading - 0x8000U
    );
    pickup.forwardVelocity = 100;
    pickup.stateBytes4dTo52 = {254, 100, 2, 0, 0, 1};
    if (playableCampaignFrames_) {
        pickup.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
        pickup.animatedModelPresenterEnabled = false;
    }
    auto pickupIndex = allocateLevel2FixedObject(pickup);
    const auto allocatedPickup =
        pickupIndex != static_cast<std::size_t>(-1);
    if (pickupIndex == static_cast<std::size_t>(-1)) {
        // FUN_1000:45c6 falls back to the first active record whose
        // type exceeds 44 when the 16-record fixed pool is full.
        // Route-v13 tick 10231 recycles a Type 46 this way.
        for (const auto candidate : level2AllocationListOrder_) {
            if (
                candidate < level2Entities_.size()
                && level2Entities_[candidate].type > 44
            ) {
                pickupIndex = candidate;
                pickup.collisionRadius =
                    level2Entities_[candidate].collisionRadius;
                pickup.allocationIndex = candidate;
                level2Entities_[candidate] = pickup;
                break;
            }
        }
    }
    if (pickupIndex < level2Entities_.size()) {
        auto& placedPickup = level2Entities_[pickupIndex];
        if (retainedReward) {
            //45C6 overlays4506's retained fields and attaches its inline selector.
            placedPickup.position = pickup.position;
            placedPickup.pitch = placedPickup.roll = 0;
            placedPickup.yaw = pickup.yaw;
            placedPickup.pitchVelocity = placedPickup.rollVelocity = placedPickup.yawVelocity = 0;
            placedPickup.forwardVelocity = 100;
            placedPickup.stateBytes4dTo52[2] = 2;
            placedPickup.stateBytes4dTo52[5] = 1;
            placedPickup.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
        }
        // FUN_1000:45c6 clears the new pickup against the destroyed
        // guardian record still carried by the callback, not against
        // the impacting projectile.  The accepted Level 3 loop-774
        // collision makes the distinction observable: the native
        // zero-radius pickup advances sixteen times beyond the
        // Type-44 half-radius before stopping at (5904,5583).
        const auto rewardCollision = [&]() {
            if (!retainedReward) {
                return campaignCollides(placedPickup, pendingEffectSource);
            }
            auto collided = false;
            for (const auto identity : identities) {
                if (identity.pool != 0 && identity.pool != 3) {
                    continue;
                }
                auto& slots = identity.pool == 0 ? level2EntityActorPoolSlots_ : level2EntityFixedPoolSlots_;
                const auto live = std::find(slots.begin(), slots.end(), identity.slot);
                auto& candidate = live != slots.end()
                    ? level2Entities_[static_cast<std::size_t>(live - slots.begin())]
                    : identity.pool == 0 ? level2ActorPool_[identity.slot] : level2FixedPool_[identity.slot];
                if (&candidate == &placedPickup || candidate.type == 7
                    || candidate.stateBytes4dTo52[0] == 0xfd || candidate.stateBytes4dTo52[0] == 0xfc) {
                    continue;
                }
                const auto supportedDelta = [this](const std::int64_t delta) {
                    constexpr std::int64_t coordinateLimit = 1000000;
                    //3A1C's -32768 saturation and wrapped arithmetic are separate domains.
                    return (playableCampaignFrames_ || (delta != -32768 && delta != 32768))
                        && delta >= -coordinateLimit && delta <= coordinateLimit;
                };
                if (candidate.collisionRadius >= 32768 || placedPickup.collisionRadius >= 32768
                    || !supportedDelta(static_cast<std::int64_t>(candidate.position.x) - placedPickup.position.x)
                    || !supportedDelta(static_cast<std::int64_t>(candidate.position.y) - placedPickup.position.y)
                    || !supportedDelta(static_cast<std::int64_t>(candidate.position.z) - placedPickup.position.z)) {
                    throw std::invalid_argument("Level5 reward collision arithmetic outside qualified domain");
                }
                if (campaignCollides(candidate, placedPickup)) {
                    candidate.collisionContactAllocationIndex = pickupIndex;
                    candidate.collisionContactIsPlayer = false;
                    collided = true;
                }
            }
            return collided;
        };
        while (rewardCollision()) {
            advanceEntityKinematics(placedPickup);
        }
        placedPickup.forwardVelocity = 0;
        placedPickup.yawVelocity = static_cast<std::int16_t>(
            (
                static_cast<std::int16_t>(
                    (choice >> 8U) & 0x03U
                ) + 10
            ) << 8
        );
        if ((choice & 0x0100U) != 0) {
            placedPickup.yawVelocity = static_cast<std::int16_t>(
                -placedPickup.yawVelocity
            );
        }
        if (allocatedPickup) {
            if (activeCampaignLevel_ == 1 && placedPickup.renderProfile == WorldActorRenderProfile::noHandles) {
                placedPickup.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
            }
            // FUN_1000:45c6 initializes the display selector, but
            // the new fixed-pool pointer was absent from the
            // boundary's presenter walk. Natural Level 3 loop 3292
            // therefore retains zero distance and target heading.
            if (!retainedReward) {
                placedPickup.stateBytes4dTo52[2] = 3;
            }
        } else {
            // A recycled record was already present in the callback
            // order and has current presenter fields on this tick.
            placedPickup.presenterDistance =
                approximatePlanarDistance(
                    level2Player_.position,
                    placedPickup.position
                );
            placedPickup.targetHeading = presenterTargetHeading(
                placedPickup.position,
                level2Player_.position
            );
        }
        if (retainedReward) {
            level2FixedPool_[level2EntityFixedPoolSlots_[pickupIndex]] = placedPickup;
        }
    }
    if (exit == CampaignExit::greenReward) {
        level2PickupIndex_ = pickupIndex;
    }
    if (!retainedReward && domain != CampaignRewardDomain::physicalLevel2Guardian) {
        if (!playableCampaignFrames_) {
            level2PostWaveObjectIndex_ = static_cast<std::size_t>(-1);
        }
        const auto choice = level2Rng_.next();
        const auto behavior = pendingEffectSource.stateBytes4dTo52[5];
        const bool allocation = (behavior == 0 && (choice & 0x003fU) == 0
                && level2CompletionObjectIndex_ == static_cast<std::size_t>(-1))
            || (behavior == 1 && pendingEffectSource.behaviorTimer == 21);
        if (activeCampaignLevel_ == 1 && allocation && pickupIndex < level2Entities_.size()) {
            level2Entities_[pickupIndex].renderProfile = WorldActorRenderProfile::unknown;
        }
    }
}

} // namespace stellar7
