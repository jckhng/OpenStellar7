#include "stellar7/gameplay.hpp"
#include "campaign_guardian_children.hpp"
#include "playable_decoy.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {

bool GameplayModel::fireLevel2EnemyProjectile(
    const std::size_t entityIndex,
    const int explicitWeaponSlot
) {
    if (entityIndex >= level2Entities_.size()) {
        return false;
    }
    return fireLevel2EnemyProjectile(level2Entities_[entityIndex],
        level2EntityActorPoolSlots_[entityIndex], explicitWeaponSlot);
}

bool GameplayModel::fireLevel2EnemyProjectile(Level1Entity& owner,
    const std::size_t ownerSlot, const int explicitWeaponSlot) {
    const auto campaignGuardian = playableCampaignFrames_ && owner.stateBytes4dTo52[4] == 5
        && (activeCampaignLevel_ == 0 || activeCampaignLevel_ == 3 || activeCampaignLevel_ == 5 || activeCampaignLevel_ == 6);
    std::optional<CampaignGuardianProjectileBirth> guardianBirth;
    if (campaignGuardian) {
        if (!campaignModelsBound_) {
            throw std::invalid_argument("playable guardian fire requires its actual model registry");
        }
        auto pool = level2ProjectilePool_;
        for (std::size_t index = 0; index < level2Entities_.size(); ++index) {
            const auto slot = level2EntityProjectilePoolSlots_[index];
            if (slot < pool.size()) {
                pool[slot] = level2Entities_[index];
            }
        }
        guardianBirth = prepareCampaignGuardianProjectile(pool, owner, ownerSlot,
            playableBehaviorTarget(level2Player_,campaignPlayerProxy_,level1_.systemTimers_[0]).position,
            static_cast<std::uint8_t>(activeCampaignLevel_), campaignModels_[22].radius,
            activeCampaignLevel_ == 6 ? CampaignGuardianAttack::volleyProjectile80 : CampaignGuardianAttack::aimedProjectile);
        if (!guardianBirth) {
            return false;
        }
    }
    const bool level5GuardianShot = activeCampaignLevel_ == 4 && owner.type == 44;
    if (!campaignGuardian && !level5GuardianShot && owner.weaponCooldown != 0) {
        --owner.weaponCooldown;
        return false;
    }
    const auto difference = static_cast<std::int16_t>(
        owner.targetHeading - owner.yaw
    );
    const auto magnitude = difference < 0
        ? -static_cast<std::int32_t>(difference)
        : static_cast<std::int32_t>(difference);
    if (
        !campaignGuardian && !level5GuardianShot && ((magnitude >= 0x200 && owner.type != 10)
            || owner.weaponSlotCount == 0)
    ) {
        return false;
    }

    // FUN_1000:995e indexes the campaign actor table at DS:09dc rather
    // than using one global owner-to-shot mapping.  Level 3's Type 3 and
    // Type 12 therefore emit the distinct native projectile records 20 and
    // 17; the earlier levels retain their original mappings.
    const auto projectileType = static_cast<std::int16_t>(
        (activeCampaignLevel_ == 4 || playableCampaignFrames_) && owner.type == 11 ? 20
        : activeCampaignLevel_ == 2 && owner.type == 3 ? 20
        : activeCampaignLevel_ == 2 && owner.type == 12 ? 17
        : owner.type == 1 ? 17
        : owner.type == 2 || owner.type == 10 ? 18
        : owner.type == 4 ? 20
        : owner.type == 5 || owner.type == 9 || owner.type == 44 ? 22
        : 19
    );
    const auto guardianShot = projectileType == 22 && owner.stateBytes4dTo52[4] == 5;
    const auto lifetime = static_cast<std::uint8_t>(
        projectileType == 17 ? 54
        : projectileType == 18 ? 66
        : projectileType == 20 ? 60
        : level5GuardianShot ? 64
        : guardianShot ? 80
        : 84
    );
    // FUN_1000:995e obtains the radius from the current model registry.
    // Native Level 2 v7 loops 625/1000 retain 441/134 for Types 17/20;
    // Level 3's captured records for those same types have radius zero.
    const auto radius = playableCampaignFrames_ && campaignModelsBound_ ? campaignModels_[projectileType].radius : static_cast<std::uint32_t>(
        activeCampaignLevel_ == 1 && projectileType == 17 ? 441
        : (activeCampaignLevel_ == 1 || activeCampaignLevel_ == 4) && projectileType == 20 ? 134
        : projectileType == 17 || projectileType == 18 || projectileType == 20 ? 0
        : projectileType == 22 ? 134 : 441
    );
    const auto cooldown = static_cast<std::uint8_t>(
        owner.type == 5 ? 12 : projectileType == 17 ? 8 : (projectileType == 19 ? 12 : 0)
    );
    const auto eventId = static_cast<std::uint16_t>(
        campaignGuardian ? guardianBirth->eventId : guardianShot ? 6 : projectileType < 20 ? 2 : 15
    );

    auto launched = false;
    auto launchedPoolIndex = level2ProjectilePoolSize_;
    Level1Entity launchedProjectile{};
    if (guardianBirth) {
        launched = true;
        launchedPoolIndex = guardianBirth->physicalSlot;
        launchedProjectile = guardianBirth->actor;
        level2ProjectilePool_[launchedPoolIndex] = launchedProjectile;
        level2ProjectilePoolActive_[launchedPoolIndex] = true;
        level2PrefixGuards_.reset();
    }
    const auto firstSlot = explicitWeaponSlot < 0
        ? std::size_t{0}
        : static_cast<std::size_t>(explicitWeaponSlot);
    const auto endSlot = level5GuardianShot ? std::size_t{1} : explicitWeaponSlot < 0
        ? static_cast<std::size_t>(owner.weaponSlotCount)
        : firstSlot + 1;
    if (!campaignGuardian && !level5GuardianShot && firstSlot >= owner.weaponSlotCount) {
        return false;
    }
    for (std::size_t slot = firstSlot; !campaignGuardian && slot < endSlot; ++slot) {
        auto& timer = owner.weaponSlotTimers[slot];
        if (!level5GuardianShot && explicitWeaponSlot < 0 && timer != 0) {
            --timer;
            continue;
        }
        if (launched) {
            continue;
        }
        const auto poolSlot = std::find(
            level2ProjectilePoolActive_.begin(),
            level2ProjectilePoolActive_.end(),
            false
        );
        if (poolSlot == level2ProjectilePoolActive_.end()) {
            continue;
        }
        level2PrefixGuards_.reset();
        launchedPoolIndex = static_cast<std::size_t>(
            poolSlot - level2ProjectilePoolActive_.begin()
        );
        launchedProjectile = level2ProjectilePool_[launchedPoolIndex];
        launchedProjectile.type = projectileType;
        launchedProjectile.collisionRadius = radius;
        launchedProjectile.position = owner.position;
        if (guardianShot && !level5GuardianShot) {
            launchedProjectile.position.z += 2000;
        }
        if (level5GuardianShot) {
            // ACEC copies XYZ only; 44BA preserves the slot's motion fields.
            launchedProjectile.zVelocity = 0; //44DE clears +3A before the inherited step.
            launchedProjectile.yaw = static_cast<std::uint16_t>(owner.yaw - 0x4000U);
            advanceEntityKinematics(launchedProjectile);
            launchedProjectile.forwardVelocity = 500;
            const auto& target=playableCampaignFrames_
                ? playableBehaviorTarget(level2Player_,campaignPlayerProxy_,level1_.systemTimers_[0]) : level2Player_;
            launchedProjectile.yaw = fixedHeadingBetween(launchedProjectile.position,target.position);
            launchedProjectile.pitch = fixedHeadingBetween({}, {launchedProjectile.position.z,
                static_cast<std::int32_t>(owner.presenterDistance), 0});
        } else {
            launchedProjectile.pitch = owner.pitch;
            launchedProjectile.roll = owner.roll;
            launchedProjectile.yaw = owner.yaw;
            launchedProjectile.forwardVelocity = 500;
            launchedProjectile.pitchVelocity = 0;
            launchedProjectile.rollVelocity = 0;
            launchedProjectile.yawVelocity = 0;
            launchedProjectile.xVelocity = 0;
            launchedProjectile.yVelocity = 0;
            launchedProjectile.zVelocity = 0;
        }
        if (level5GuardianShot) {
            launchedProjectile.stateBytes4dTo52[1] = lifetime;
            launchedProjectile.stateBytes4dTo52[2] = 1;
            launchedProjectile.stateBytes4dTo52[4] = 0;
        } else {
            launchedProjectile.stateBytes4dTo52 = {0xfd, lifetime, 0, 0, 0, 0};
        }
        if (
            projectileType == 20
            && level2ProjectilePoolWasActiveAtBoundaryStart_[
                launchedPoolIndex
            ]
        ) {
            // Reusing a pointer that was live at boundary start leaves it in
            // the current presenter list, which refreshes the launch state.
            // A normally free slot retains its cached presenter fields.
            launchedProjectile.presenterDistance = owner.presenterDistance;
            launchedProjectile.targetHeading = owner.targetHeading;
        }
        if (guardianShot) {
            launchedProjectile.yaw = fixedHeadingBetween(
                launchedProjectile.position,
                level2Player_.position
            );
            launchedProjectile.stateBytes4dTo52[2] = 1;
        }
        if (!level5GuardianShot) {
            launchedProjectile.weaponSlotCount = 0;
            launchedProjectile.weaponCooldown = 0;
            launchedProjectile.weaponSlotTimers = {};
            launchedProjectile.collisionContactAllocationIndex = static_cast<std::size_t>(-1);
            launchedProjectile.collisionContactIsPlayer = false;
        }
        launchedProjectile.ownerIsPlayer = false;
        launchedProjectile.ownerAllocationIndex = ownerSlot;
        level2ProjectilePoolActive_[launchedPoolIndex] = true;
        if (!level5GuardianShot) {
            owner.weaponCooldown = cooldown;
            timer = lifetime;
        }
        if (owner.stateBytes4dTo52[4] != 5) {
            owner.behaviorTimer = 1;
        }
        launched = true;
    }
    if (!launched) {
        return false;
    }
    if (playableCampaignFrames_) {
        launchedProjectile.animatedModelPresenterEnabled = false;
    }

    auto insertionIndex = level2Entities_.size();
    for (std::size_t candidate = 0;
         candidate < level2EntityProjectilePoolSlots_.size();
         ++candidate) {
        const auto occupiedPoolIndex =
            level2EntityProjectilePoolSlots_[candidate];
        if (
            occupiedPoolIndex < level2ProjectilePoolSize_
            && occupiedPoolIndex > launchedPoolIndex
        ) {
            insertionIndex = candidate;
            break;
        }
    }
    for (auto& allocationIndex : level2AllocationListOrder_) {
        if (allocationIndex >= insertionIndex) {
            ++allocationIndex;
        }
    }
    for (auto& objectIndex : level2ObjectListOrder_) {
        if (objectIndex >= insertionIndex) {
            ++objectIndex;
        }
    }
    launchedProjectile.allocationIndex = insertionIndex;
    ++level2AllocationRevision_;
    level2RetiredEffectSemanticOrderRebuilt_ = false;
    level2Entities_.insert(
        level2Entities_.begin()
            + static_cast<std::ptrdiff_t>(insertionIndex),
        launchedProjectile
    );
    level2EntityProjectilePoolSlots_.insert(
        level2EntityProjectilePoolSlots_.begin()
            + static_cast<std::ptrdiff_t>(insertionIndex),
        launchedPoolIndex
    );
    level2EntityActorPoolSlots_.insert(
        level2EntityActorPoolSlots_.begin()
            + static_cast<std::ptrdiff_t>(insertionIndex),
        level2ActorPoolSize_
    );
    insertLevel2FixedPoolSlot(insertionIndex, level2FixedObjectPoolSize_);
    level2EntityDeathEffectPoolSlots_.insert(
        level2EntityDeathEffectPoolSlots_.begin()
            + static_cast<std::ptrdiff_t>(insertionIndex),
        level2DeathEffectPoolSize_
    );
    for (std::size_t candidate = insertionIndex + 1;
         candidate < level2Entities_.size();
         ++candidate) {
        level2Entities_[candidate].allocationIndex = candidate;
    }
    level2AllocationListOrder_.push_back(insertionIndex);
    level2ObjectListOrder_ = level2AllocationListOrder_;
    level2ActiveListOrder_ = level2AllocationListOrder_;
    level2AudioEvents_.push_back({level5GuardianShot
        ? static_cast<std::uint16_t>(level2LoopTick_ - 1) : level2LoopTick_, eventId});
    return true;
}

} // namespace stellar7
