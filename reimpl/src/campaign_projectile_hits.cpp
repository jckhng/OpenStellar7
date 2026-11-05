#include "campaign_contact.hpp"
#include <algorithm>
#include <stdexcept>

namespace stellar7 {

GameplayModel::CampaignProjectileContactResult GameplayModel::resolveCampaignPlayerProjectileHits(
    const CampaignContactStart& start, const CampaignContactMotion& motion,
    const CampaignCombatCallbacks& combat) {
    CampaignProjectileContactResult result;
    const auto projectileOrder = activeCampaignLevel_ == 4
        ? remapLevel2BoundaryOrder(start.identities) : level2ActiveListOrder_;
    if (activeCampaignLevel_ == 4) {
        for (const auto identity : start.identities) {
            if (identity.pool != 3 || std::find(level2EntityFixedPoolSlots_.begin(),
                level2EntityFixedPoolSlots_.end(), identity.slot) != level2EntityFixedPoolSlots_.end()) {
                continue;
            }
            for (const auto index : projectileOrder) {
                const auto slot = level2EntityProjectilePoolSlots_[index];
                if (slot < level2ProjectilePoolSize_ && start.projectiles[slot]
                    && campaignCollides(level2Entities_[index], level2FixedPool_[identity.slot])) {
                    throw std::invalid_argument("Level5 projectile contact requires its retired fixed target ordering");
                }
            }
        }
    }
    for (const auto candidateSourceIndex : projectileOrder) {
        auto sourceIndex = candidateSourceIndex;
        if (sourceIndex >= level2Entities_.size()) {
            continue;
        }
        const auto sourceProjectileSlot =
            level2EntityProjectilePoolSlots_[sourceIndex];
        if (
            level2Entities_[sourceIndex].type != 23
            || sourceProjectileSlot >= level2ProjectilePoolSize_
            || !start.projectiles[sourceProjectileSlot]
        ) {
            continue;
        }
        const auto sourceOrder = std::find(
            projectileOrder.begin(),
            projectileOrder.end(),
            sourceIndex
        );
        auto targetIndex = static_cast<std::size_t>(-1);
        for (const auto candidate : projectileOrder) {
            // 072a accepts fixed objects (category -2), including the
            // post-guardian pickup. Pool ownership is not a collision gate.
            // Keep the broader campaign correction scoped to verified L2.
            if (
                candidate >= level2Entities_.size()
                || candidate == sourceIndex
                || (level2EntityActorPoolSlots_[candidate]
                        >= level2ActorPoolSize_
                    && ((!playableCampaignFrames_ && activeCampaignLevel_ != 1 && activeCampaignLevel_ != 4)
                        || level2Entities_[candidate].stateBytes4dTo52[0] != 254))
                || !campaignActorExistedAtStart(candidate, start)
                || level2Entities_[candidate].type == 46
            ) {
                continue;
            }
            auto collisionTarget = level2Entities_[candidate];
            const auto candidateActorSlot =
                level2EntityActorPoolSlots_[candidate];
            if (
                sourceOrder != projectileOrder.end()
                && candidateActorSlot < level2ActorPoolSize_
                && motion.valid[candidateActorSlot]
                && static_cast<std::size_t>(
                    sourceOrder - projectileOrder.begin()
                ) < motion.order[candidateActorSlot]
            ) {
                // FUN_1000:9298 invokes the Type-23 callback before this
                // later actor updater. Native Level 3 loop 1061->1062 misses
                // Type 44 at its boundary-start center; using the portable
                // pre-integrated center creates a false hit one tick early.
                collisionTarget.position =
                    motion.positions[candidateActorSlot];
            }
            if (campaignCollides(
                level2Entities_[sourceIndex],
                collisionTarget
            )) {
                targetIndex = candidate;
                break;
            }
        }
        if (targetIndex >= level2Entities_.size()) {
            continue;
        }
        level2PrefixGuards_.reset();
        const auto impactProjectile = level2Entities_[sourceIndex];

        // FUN_1000:9298 dispatches callbacks in the existing active-list
        // order. A behavior-3 Type 1/4 actor still consumes its reset choice
        // when it precedes the projectile that destroys it, independent of
        // weapon-slot availability. The separated portable passes must
        // preserve that pre-destruction callback.
        const auto targetOrder = std::find(
            projectileOrder.begin(),
            projectileOrder.end(),
            targetIndex
        );
        auto& targetBeforeHit = level2Entities_[targetIndex];
        if (
            targetOrder < sourceOrder
            && (targetBeforeHit.type == 1 || targetBeforeHit.type == 4)
            && targetBeforeHit.stateBytes4dTo52[1] == 1
            && targetBeforeHit.stateBytes4dTo52[5] == 3
        ) {
            const auto turnLimit = static_cast<std::int16_t>(
                targetBeforeHit.type == 1 ? 143 : 271
            );
            const auto difference = static_cast<std::int16_t>(
                targetBeforeHit.targetHeading - targetBeforeHit.yaw
            );
            targetBeforeHit.yawVelocity = static_cast<std::int16_t>(
                std::clamp<std::int32_t>(
                    difference,
                    -turnLimit,
                    turnLimit
                )
            );
            const auto choice = static_cast<std::uint8_t>(level2Rng_.next());
            const auto resetThreshold = static_cast<std::uint8_t>(
                targetBeforeHit.type == 1 ? 5 : 6
            );
            if (choice < resetThreshold) {
                targetBeforeHit.stateBytes4dTo52[5] = 0;
            }
        }

        auto impactPosition = level2Entities_[targetIndex].position;
        const auto impactActorSlot =
            level2EntityActorPoolSlots_[targetIndex];
        if (
            sourceOrder < targetOrder
            && impactActorSlot < level2ActorPoolSize_
            && motion.valid[impactActorSlot]
        ) {
            // FUN_1000:9298 invokes the projectile callback before the
            // target updater in this inherited active-list order. Route-v12
            // tick 7708 exposes it because the moving Type 47 center would
            // otherwise change before FUN_1000:1176 copies its position.
            impactPosition =
                motion.positions[impactActorSlot];
        }
        if (start.guardianHitActions || (playableCampaignFrames_ && level2Entities_[targetIndex].stateBytes4dTo52[4] == 0xf3)) {
            auto shadow = level2Entities_[targetIndex];
            const auto identity = campaignEntityIdentity(targetIndex);
            const auto guardian = shadow.stateBytes4dTo52[4] == 5;
            auto amount = static_cast<std::uint8_t>(impactProjectile.stateBytes4dTo52[2] == 0 ? 1 : 5);
            auto permitted = true;
            if (guardian && activeCampaignLevel_ == 5) {
                const auto difference = static_cast<std::int16_t>(shadow.yaw - impactProjectile.yaw);
                const auto magnitude = difference < 0 ? static_cast<std::int16_t>(-static_cast<int>(difference)) : difference;
                permitted = magnitude <= 0x2fff;
                amount = impactProjectile.stateBytes4dTo52[2] == 0 ? 20 : 60;
            }
            const auto healthBefore=shadow.stateBytes4dTo52[1];
            const auto killed=permitted&&applyCampaignHit(shadow,amount,identity, combat,impactPosition);
            if(playableCampaignFrames_&&impactProjectile.ownerIsPlayer&&shadow.stateBytes4dTo52[1]<healthBefore) {
                combatFeedback_.hit=CombatCue{ticks_,activeCampaignLevel_,healthBefore-shadow.stateBytes4dTo52[1]};
            }
            if (killed && !guardian) {
                constexpr std::array<std::uint16_t,17> points{{0,20,35,20,30,75,90,0,25,30,40,50,60,10,5,5,5}};
                if (shadow.type >= 0 && static_cast<std::size_t>(shadow.type) < points.size()) {
                    level2Score_ += scaledCampaignAward(points[shadow.type]);
                } else if (shadow.type >= 46) {
                    level2Score_ += scaledCampaignAward(5);
                }
            }
            const auto shot = std::find(level2EntityProjectilePoolSlots_.begin(), level2EntityProjectilePoolSlots_.end(), sourceProjectileSlot);
            if (shot != level2EntityProjectilePoolSlots_.end()) {
                combat.retire(static_cast<std::size_t>(shot - level2EntityProjectilePoolSlots_.begin()));
            }
            if (phase_ == GameplayPhase::endingEntry) { result.flow = CampaignContactFlow::suspendFrame; return result; }
            break;
        }
        auto lethal = false;
        auto& target = level2Entities_[targetIndex];
        if (activeCampaignLevel_ == 4 && target.type == 44 && target.stateBytes4dTo52[1] == 1
            && level2PickupIndex_ == static_cast<std::size_t>(-1)) {
            // Qualify the partial4506 record before committing1176's lethal transaction.
            auto available = false;
            for (std::size_t slot = 0; slot < level2FixedObjectPoolSize_; ++slot) {
                if (!level2FixedPoolKnown_[slot]) {
                    throw std::invalid_argument("Level5 reward requires known fixed-record history");
                }
                const auto& record = level2FixedPool_[slot];
                if (record.active()) {
                    continue;
                }
                if (record.renderProfile != WorldActorRenderProfile::noHandles
                    && record.renderProfile != WorldActorRenderProfile::inlineSelectorNoCallback) {
                    throw std::invalid_argument("Level5 reward requires a known callback-free fixed record");
                }
                available = true;
                break;
            }
            if (!available) {
                throw std::invalid_argument("Level5 reward full-pool fallback is unsupported");
            }
        }
        const auto healthBefore=target.stateBytes4dTo52[1];
        const auto targetWasAlive=healthBefore>0;
        if (target.stateBytes4dTo52[1] > 0) {
            const auto amount=playableCampaignFrames_ && impactProjectile.stateBytes4dTo52[2]!=0 ? 5 : 1;
            target.stateBytes4dTo52[1]=static_cast<std::uint8_t>(
                std::max(0,static_cast<int>(target.stateBytes4dTo52[1])-amount));
        }
        if (target.stateBytes4dTo52[1] == 0) {
            if (target.type == 25 || target.type == 26) {
                target.stateBytes4dTo52[1] = 100;
            } else {
                lethal = true;
            }
        }
        if(playableCampaignFrames_&&impactProjectile.ownerIsPlayer&&target.stateBytes4dTo52[1]<healthBefore) {
            combatFeedback_.hit=CombatCue{ticks_,activeCampaignLevel_,healthBefore-target.stateBytes4dTo52[1]};
        }
        const auto guardianDeath = lethal && (target.type == 44 || target.type == 45);
        if(lethal&&targetWasAlive) {
            recordDestruction(target,impactPosition,guardianDeath?12:
                (playableCampaignFrames_&&impactProjectile.stateBytes4dTo52[2]!=0?18:6));
        }
        // Original1176 selects guardian event12 instead of ordinary death6.
        if (!guardianDeath) {
            level2AudioEvents_.push_back({
                level2LoopTick_,
                static_cast<std::uint16_t>(lethal
                    ? (playableCampaignFrames_ && impactProjectile.stateBytes4dTo52[2]!=0 ? 18 : 6) : 5),
            });
        }

        auto adjustedTargetIndex = targetIndex;
        auto pendingEffect = false;
        Level1Entity pendingEffectSource{};
        std::uint8_t pendingEffectFirstFrame = 0;
        std::uint8_t pendingEffectTerminalFrame = 0;
        auto pendingGuardianPickup = false;
        if (lethal) {
            constexpr std::array<std::uint16_t, 17> scoreByType{{
                0, 20, 35, 20, 30, 75, 90,
                0, 25, 30, 40, 50, 60, 10, 5, 5, 5,
            }};
            const auto destroyedType = target.type;
            const auto destroyed = target;
            if (
                destroyedType == 48
                && impactActorSlot < level2ActorPoolSize_
            ) {
                result.retiredType48ActorSlot = impactActorSlot;
                if (activeCampaignLevel_ == 1) {
                    // 0784 interleaves actor contacts and projectile hits.
                    // At intact loop 2178, a92e still sees +5f=b29e after
                    // that Type-48 record has died. Our separated hit pass
                    // must retain the earlier guardian contact until its
                    // callback; vector retirement must not erase the gate.
                    const auto projectileIdentity = std::find_if(
                        start.identities.begin(),
                        start.identities.end(),
                        [sourceProjectileSlot](const auto& identity) {
                            return identity.pool == 1
                                && identity.slot == sourceProjectileSlot;
                        }
                    );
                    for (std::size_t guardianIndex = 0;
                         guardianIndex < level2Entities_.size();
                         ++guardianIndex) {
                        const auto& guardian = level2Entities_[guardianIndex];
                        const auto guardianSlot =
                            level2EntityActorPoolSlots_[guardianIndex];
                        if ((guardian.type != 44 && guardian.type != 45)
                            || guardianSlot >= level2ActorPoolSize_
                            || projectileIdentity == start.identities.end()
                            || motion.order[guardianSlot]
                                >= projectileIdentity->activePosition) {
                            continue;
                        }
                        auto contactTarget = destroyed;
                        if (motion.order[guardianSlot]
                                < motion.order[impactActorSlot]
                            && motion.valid[impactActorSlot]) {
                            contactTarget.position =
                                motion.positions[impactActorSlot];
                        }
                        if (campaignCollides(guardian, contactTarget)) {
                            result.guardianRetiredContact[guardianSlot] = true;
                        }
                    }
                }
            }
            if (
                destroyedType >= 0
                && static_cast<std::size_t>(destroyedType)
                    < scoreByType.size()
            ) {
                level2Score_ += scaledCampaignAward(scoreByType[ static_cast<std::size_t>(destroyedType) ]);
            } else if (destroyedType >= 46) {
                // FUN_1000_04d8 awards the shared five-point fallback for
                // every projectile-destroyed type at or above 46.
                level2Score_ += scaledCampaignAward(5);
            }
            if (
                target.stateBytes4dTo52[0] == 0xff
                && level2ActiveEnemyCount_ > 0
            ) {
                --level2ActiveEnemyCount_;
            }
            explosionTicksRemaining_ = 18;
            explosionAge_ = 0;
            explosionPosition_ = impactPosition;
            const auto distanceBucket = std::min<std::uint32_t>(
                destroyed.presenterDistance / 0x1964U,
                5U
            );
            pendingEffect = true;
            pendingEffectSource = destroyed;
            pendingEffectSource.position = impactPosition;
            pendingEffectFirstFrame = static_cast<std::uint8_t>(
                distanceBucket * 3U + 15U
            );
            pendingEffectTerminalFrame = static_cast<std::uint8_t>(
                distanceBucket * 3U + 17U
            );
            if (guardianDeath) {
                const auto eventTick = activeCampaignLevel_ == 4
                    ? static_cast<std::uint16_t>(level2LoopTick_ - 1)
                    : level2LoopTick_;
                level2AudioEvents_.push_back({eventTick, 12});
                level2Score_ += scaledCampaignAward(1000);
                pendingGuardianPickup = true;
            }
        } else {
            // FUN_1000:1176 calls FUN_1000:0bc8 with the short frame
            // interval for every surviving hit. At this post-update
            // boundary its first presented frame has advanced to two.
            pendingEffect = true;
            pendingEffectSource = target;
            pendingEffectSource.position = impactPosition;
            pendingEffectFirstFrame = 2;
            pendingEffectTerminalFrame = 12;
        }
        if ((playableCampaignFrames_ || activeCampaignLevel_ == 4) && lethal) {
            const auto projectileIdentity = std::find_if(start.identities.begin(),
                start.identities.end(), [sourceProjectileSlot](const auto& identity) {
                    return identity.pool == 1 && identity.slot == sourceProjectileSlot;
                });
            const auto projectilePosition = projectileIdentity->activePosition;
            const auto allocatedBonus = registerLevel5DestroyedTypeBonus(pendingEffectSource);
            if (allocatedBonus) {
                const auto inherited = std::find_if(start.identities.begin(),
                    start.identities.end(), [allocatedBonus](const auto& identity) {
                        return identity.pool == 3 && identity.slot == *allocatedBonus;
                    });
                if (inherited != start.identities.end()
                    && inherited->activePosition > projectilePosition) {
                    //The old pointer now names the new904c record, before its0784 motion.
                    const auto fixed = std::find(level2EntityFixedPoolSlots_.begin(),
                        level2EntityFixedPoolSlots_.end(), *allocatedBonus);
                    advanceEntityKinematics(level2Entities_[static_cast<std::size_t>(
                        fixed - level2EntityFixedPoolSlots_.begin())]);
                }
            }
            const auto shot = std::find(level2EntityProjectilePoolSlots_.begin(),
                level2EntityProjectilePoolSlots_.end(), sourceProjectileSlot);
            sourceIndex = static_cast<std::size_t>(shot - level2EntityProjectilePoolSlots_.begin());
        }
        // Ordinary impact callbacks retire their projectile pool record,
        // append the explosion, and only then retire a lethal target. Tick
        // 738 makes that order visible in native 0x49f8 swap-back state.
        // The guardian's nested FUN_1000:45c6 callback is different: it
        // appends the effect, removes the guardian, deploys Type 25, and only
        // then returns to the projectile callback for source retirement.
        // Natural Level 3 loop 3292 exposes that order with two surviving
        // projectile pointers.
        const auto allocationOrderBeforeImpact =
            level2AllocationListOrder_;
        const auto deferGuardianProjectileRetirement =
            pendingGuardianPickup;
        if (
            !deferGuardianProjectileRetirement
            && sourceIndex < level2Entities_.size()
        ) {
            combat.retire(sourceIndex);
            if (sourceIndex < adjustedTargetIndex) {
                --adjustedTargetIndex;
            }
        }
        auto spawnedEffect = false;
        if (pendingEffect) {
            spawnedEffect = combat.effect(
                pendingEffectSource,
                pendingEffectFirstFrame,
                pendingEffectTerminalFrame
            );
        }
        if (!lethal && spawnedEffect) {
            // A nonlethal impact changes the just-retired projectile record
            // into its transient hit effect without moving that record in
            // the native allocation list.  Preserve the old pointer-list
            // position while remapping vector indices around the removal.
            const auto effectIndex = level2Entities_.size() - 1;
            level2AllocationListOrder_.clear();
            level2AllocationListOrder_.reserve(
                allocationOrderBeforeImpact.size()
            );
            for (const auto priorIndex : allocationOrderBeforeImpact) {
                if (priorIndex == sourceIndex) {
                    level2AllocationListOrder_.push_back(effectIndex);
                } else {
                    level2AllocationListOrder_.push_back(
                        priorIndex > sourceIndex
                            ? priorIndex - 1
                            : priorIndex
                    );
                }
            }
            level2ObjectListOrder_ = level2AllocationListOrder_;
            level2ActiveListOrder_ = level2AllocationListOrder_;
            const auto effectSlot = level2EntityDeathEffectPoolSlots_[
                effectIndex
            ];
            auto sourceActivePosition =
                start.identities.size();
            for (const auto identity : start.identities) {
                if (
                    identity.pool == 1
                    && identity.slot == sourceProjectileSlot
                ) {
                    sourceActivePosition = identity.activePosition;
                    break;
                }
            }
            level2EffectSemanticActivePositionOverrides_.push_back({
                effectSlot,
                sourceActivePosition,
            });
        }
        if (lethal && adjustedTargetIndex < level2Entities_.size()) {
            combat.retire(adjustedTargetIndex);
        }
        if (pendingGuardianPickup) {
            combat.reward(pendingEffectSource, CampaignExit::greenReward);
        }
        if (deferGuardianProjectileRetirement) {
            const auto deferredProjectile = std::find(
                level2EntityProjectilePoolSlots_.begin(),
                level2EntityProjectilePoolSlots_.end(),
                sourceProjectileSlot
            );
            if (
                deferredProjectile
                    != level2EntityProjectilePoolSlots_.end()
            ) {
                const auto deferredIndex = static_cast<std::size_t>(
                    deferredProjectile
                        - level2EntityProjectilePoolSlots_.begin()
                );
                if (
                    !playableCampaignFrames_ && level2PickupIndex_ > deferredIndex
                    && level2PickupIndex_
                        < static_cast<std::size_t>(-1)
                ) {
                    --level2PickupIndex_;
                }
                combat.retire(deferredIndex);
            }
        }
        break;
    }
    return result;
}

} // namespace stellar7
