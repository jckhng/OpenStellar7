#include "campaign_allocation.hpp"
#include "campaign_pool_identity.hpp"
#include <algorithm>

namespace stellar7 {

void GameplayModel::retireCampaignEntity(const std::size_t removed, CampaignRetirement& retirement) noexcept {
    constexpr auto fixedContactBase = retiredFixedContactBase_;
    level2PrefixGuards_.reset();
    const auto projectileSlot =
        level2EntityProjectilePoolSlots_[removed];
    const auto actorSlot = level2EntityActorPoolSlots_[removed];
    const auto deathEffectSlot =
        level2EntityDeathEffectPoolSlots_[removed];
    if (actorSlot >= level2ActorPoolSize_ && projectileSlot >= level2ProjectilePoolSize_
        && deathEffectSlot >= level2DeathEffectPoolSize_) {
        ++level2FixedAllocationRevision_;
        const auto fixedSlot = removed < level2EntityFixedPoolSlots_.size()
            ? level2EntityFixedPoolSlots_[removed] : level2FixedObjectPoolSize_;
        if (fixedSlot < level2FixedObjectPoolSize_) {
            level2FixedPool_[fixedSlot] = level2Entities_[removed];
            level2FixedPoolKnown_[fixedSlot] = true;
        }
    }
    if (projectileSlot < level2ProjectilePoolSize_) {
        auto retiredProjectile = level2Entities_[removed];
        // FUN_1000:9298 refreshes the presenter fields before invoking
        // the projectile callback that retires this record.  The freed
        // slot therefore carries current distance/heading into reuse.
        if (retirement.continuation != Level2Continuation::successReturn) {
            retiredProjectile.presenterDistance = approximatePlanarDistance(
                retirement.behaviorTarget.position,
                retiredProjectile.position
            );
            retiredProjectile.targetHeading = presenterTargetHeading(
                retiredProjectile.position,
                retirement.behaviorTarget.position
            );
        }
        retiredProjectile.stateBytes4dTo52[1] = 0;
        level2ProjectilePool_[projectileSlot] = retiredProjectile;
        level2ProjectilePoolActive_[projectileSlot] = false;
    }
    if (actorSlot < level2ActorPoolSize_) {
        level2ActorPool_[actorSlot] = level2Entities_[removed];
        level2ActorPoolActive_[actorSlot] = false;
    }
    if (deathEffectSlot < level2DeathEffectPoolSize_) {
        level2DeathEffectPool_[deathEffectSlot] =
            level2Entities_[removed];
        level2DeathEffectPoolActive_[deathEffectSlot] = false;
    }
    if (deathEffectSlot >= level2DeathEffectPoolSize_) {
        level2RetiredEffectSemanticOrderRebuilt_ = false;
        // Expired effects remain in the semantic presenter snapshot.
        // Removing one from the live vector must not collapse another
        // retired effect's identity (native Level 2 loop 786 retires two).
        // Other removals really do leave that snapshot and shift it.
        for (auto& retired : level2RetiredEffectSemanticEntities_) {
            if (retired.allocationIndex > removed) {
                --retired.allocationIndex;
            }
        }
    }
    const auto allocation = std::find(
        level2AllocationListOrder_.begin(),
        level2AllocationListOrder_.end(),
        removed
    );
    if (allocation != level2AllocationListOrder_.end()) {
        *allocation = level2AllocationListOrder_.back();
        level2AllocationListOrder_.pop_back();
    }
    if (playableCampaignFrames_) {
        //Portable indices move when an earlier physical record retires.
        //DS1292 retains its dead physical guardian; its erased compact view
        //must not become a reference to the actor shifted into that index.
        if (level2PostWaveObjectIndex_ == removed) {
            level2PostWaveObjectIndex_ = retiredGuardianIndex;
        }
        for (auto* pointer:{&level2CompletionObjectIndex_,&level2PostWaveObjectIndex_,&level2PickupIndex_}) {
            if (*pointer>removed && *pointer<postWaveActorKeyBase_) {--*pointer;}
        }
    }
    level2Entities_.erase(
        level2Entities_.begin()
            + static_cast<std::ptrdiff_t>(removed)
    );
    level2EntityFixedPoolSlots_.resize(level2Entities_.size() + 1, level2FixedObjectPoolSize_);
    level2EntityFixedPoolSlots_.erase(level2EntityFixedPoolSlots_.begin() + removed);
    level2EntityProjectilePoolSlots_.erase(
        level2EntityProjectilePoolSlots_.begin()
            + static_cast<std::ptrdiff_t>(removed)
    );
    level2EntityActorPoolSlots_.erase(
        level2EntityActorPoolSlots_.begin()
            + static_cast<std::ptrdiff_t>(removed)
    );
    level2EntityDeathEffectPoolSlots_.erase(
        level2EntityDeathEffectPoolSlots_.begin()
            + static_cast<std::ptrdiff_t>(removed)
    );
    for (auto& index : level2AllocationListOrder_) {
        if (index > removed) {
            --index;
        }
    }
    level2ObjectListOrder_ = level2AllocationListOrder_;
    level2ActiveListOrder_ = level2AllocationListOrder_;
    for (auto& entity : level2Entities_) {
        if (entity.allocationIndex > removed) {
            --entity.allocationIndex;
        }
        if (entity.collisionContactAllocationIndex == removed) {
            entity.collisionContactAllocationIndex =
                static_cast<std::size_t>(-1);
        } else if (
            entity.collisionContactAllocationIndex > removed
            && entity.collisionContactAllocationIndex < fixedContactBase
        ) {
            --entity.collisionContactAllocationIndex;
        }
    }
    if (deathEffectSlot >= level2DeathEffectPoolSize_) {
        // A terminal effect is retired by its presenter before the
        // ordinary pointer walk and does not suppress FUN_1000:14b0's
        // distance sort. Actor/projectile membership changes still
        // force the allocation-list rebuild path.
        retirement.lastNonEffectRemoval = removed;
    } else {
        ++retirement.effectCount;
    }
}

bool GameplayModel::spawnCampaignTransientEffect(const Level1Entity& source,
    const std::uint8_t firstFrame, const std::uint8_t terminalFrame) noexcept {
    const auto effectSlot = std::find_if(
        level2DeathEffectPoolActive_.begin(),
        level2DeathEffectPoolActive_.end(),
        [this](const bool& active) {
            const auto slot = static_cast<std::size_t>(
                &active - level2DeathEffectPoolActive_.data()
            );
            return !active
                && !level2DeathEffectPoolPendingPresenterRetirement_[slot];
        }
    );
    if (effectSlot == level2DeathEffectPoolActive_.end()) {
        return false;
    }
    level2PrefixGuards_.reset();
    const auto poolIndex = static_cast<std::size_t>(
        effectSlot - level2DeathEffectPoolActive_.begin()
    );
    auto effect = level2DeathEffectPool_[poolIndex];
    level2DeathEffectPoolPendingPresenterRetirement_[poolIndex] = false;
    effect.flags = 0x8c;
    effect.type = -1;
    effect.renderProfile = WorldActorRenderProfile::bitmapEffectCallback;
    effect.collisionRadius = 0;
    effect.position = source.position;
    effect.stateBytes4dTo52 = {
        252,
        firstFrame,
        0,
        0,
        terminalFrame,
        0,
    };
    effect.weaponSlotCount = 0;
    effect.weaponCooldown = 0;
    effect.weaponSlotTimers = {};
    const auto insertionIndex = level2Entities_.size();
    effect.allocationIndex = insertionIndex;
    ++level2AllocationRevision_;
    level2RetiredEffectSemanticOrderRebuilt_ = false;
    ++level2NonProjectileAllocationRevision_;
    level2Entities_.push_back(effect);
    level2EntityFixedPoolSlots_.resize(insertionIndex, level2FixedObjectPoolSize_);
    level2EntityFixedPoolSlots_.push_back(level2FixedObjectPoolSize_);
    level2EntityActorPoolSlots_.push_back(level2ActorPoolSize_);
    level2EntityProjectilePoolSlots_.push_back(
        level2ProjectilePoolSize_
    );
    level2EntityDeathEffectPoolSlots_.push_back(poolIndex);
    level2DeathEffectPoolActive_[poolIndex] = true;
    level2AllocationListOrder_.push_back(insertionIndex);
    level2ObjectListOrder_ = level2AllocationListOrder_;
    level2ActiveListOrder_ = level2AllocationListOrder_;
    return true;
}

} // namespace stellar7
