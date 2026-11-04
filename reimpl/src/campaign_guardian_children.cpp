#include "campaign_guardian_children.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::uint8_t projectileCategory = 0xfd;
constexpr std::uint8_t ordinaryCategory = 0xff;
constexpr std::uint8_t guardianClass = 5;
constexpr std::uint8_t specialRewardClass = 0xf3;
constexpr std::uint8_t finalCampaignLevel = 6;
constexpr std::size_t ordinaryPoolSize = 32;
constexpr std::int16_t guardianProjectileType = 22;
constexpr std::int16_t projectileSpeed = 500;
constexpr std::uint8_t projectileLifetime = 80;
constexpr std::array<std::int16_t, 7> aimedAltitude{{2000, 0, 200, 2000, 0, 0, 0}};

void validateLevel(const std::uint8_t level) {
    if (level > finalCampaignLevel) {
        throw std::invalid_argument("guardian child requires a campaign level in0..6");
    }
}
} //namespace

std::optional<CampaignGuardianProjectileBirth> prepareCampaignGuardianProjectile(
    const std::array<Level1Entity, Level1State::projectilePoolSize>& pool,
    const Level1Entity& owner, const std::size_t ownerPhysicalSlot,
    const FixedPosition playerPosition, const std::uint8_t campaignLevel,
    const std::uint32_t model22Radius, const CampaignGuardianAttack attack) {
    validateLevel(campaignLevel);
    if (owner.stateBytes4dTo52[4] != guardianClass || model22Radius == 0
        || ownerPhysicalSlot >= ordinaryPoolSize) {
        throw std::invalid_argument("guardian projectile requires its physical owner and actual model22 radius");
    }
    if (attack != CampaignGuardianAttack::aimedProjectile
        && attack != CampaignGuardianAttack::volleyProjectile80) {
        throw std::invalid_argument("guardian projectile requires ACEC or995E");
    }
    const auto selected = std::find_if(pool.begin(), pool.end(), [](const auto& actor) {
        return !actor.active();
    });
    if (selected == pool.end()) {
        return {};
    }
    if (selected->stateBytes4dTo52[0] != projectileCategory
        || selected->renderProfile != WorldActorRenderProfile::inlineSelectorNoCallback) {
        throw std::invalid_argument("guardian projectile requires reset-owned FD selector ancestry");
    }
    auto actor = *selected;
    actor.stateBytes4dTo52[4] = 0;
    actor.zVelocity = 0;
    actor.position = owner.position;
    const auto aimed = attack == CampaignGuardianAttack::aimedProjectile;
    actor.stateBytes4dTo52[2] = aimed ? 1 : 0;
    if (aimed) {
        actor.position.z = static_cast<std::int32_t>(static_cast<std::uint32_t>(actor.position.z)
            + static_cast<std::uint32_t>(aimedAltitude[campaignLevel]));
        actor.yaw = static_cast<std::uint16_t>(owner.yaw - 0x4000U);
        advanceEntityKinematics(actor);
        actor.yaw = fixedHeadingBetween(actor.position, playerPosition);
        actor.pitch = fixedHeadingBetween({}, {actor.position.z,
            static_cast<std::int32_t>(owner.presenterDistance), 0});
    } else {
        actor.pitch = owner.pitch;
        actor.roll = owner.roll;
        actor.yaw = owner.yaw;
    }
    actor.forwardVelocity = projectileSpeed;
    actor.type = guardianProjectileType;
    actor.collisionRadius = model22Radius;
    actor.ownerIsPlayer = false;
    actor.ownerAllocationIndex = ownerPhysicalSlot;
    actor.stateBytes4dTo52[1] = projectileLifetime;
    return CampaignGuardianProjectileBirth{
        static_cast<std::size_t>(selected - pool.begin()), actor,
        static_cast<std::uint16_t>(aimed ? 6 : 15)};
}

CampaignActorHitResult campaignActorHit(const Level1Entity& target,
    const std::uint8_t damage, const std::uint8_t campaignLevel) {
    validateLevel(campaignLevel);
    using Action = CampaignActorHitAction;
    const auto subtracted = static_cast<std::uint8_t>(target.stateBytes4dTo52[1] - damage);
    const auto lethal = subtracted == 0 || subtracted >= 0x80;
    if (!lethal || target.type == 25 || target.type == 26) {
        return {lethal ? std::uint8_t{100} : subtracted, false, 0, 12, 5,
            {Action::effect, Action::audio}};
    }
    //E0D0 returns a signed word quotient; distances beyond this domain would
    //produce unsupported negative effect arguments after word truncation.
    constexpr std::uint32_t distanceStep = 0x1964;
    const auto rawBucket = target.presenterDistance / distanceStep;
    if (target.presenterDistance > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
        || rawBucket > static_cast<std::uint32_t>(std::numeric_limits<std::int16_t>::max())) {
        throw std::invalid_argument("actor1176 distance exceeds the positive effect domain");
    }
    const auto bucket = std::min<std::uint32_t>(rawBucket, 5);
    const auto guardian = target.stateBytes4dTo52[4] == guardianClass;
    CampaignActorHitResult result{0, true, static_cast<std::uint8_t>(3 * bucket + 13),
        static_cast<std::uint8_t>(3 * bucket + 17),
        static_cast<std::uint16_t>(guardian ? 12 : damage >= 5 ? 18 : 6),
        {Action::registerBonusDeath, Action::effect, Action::retire}};
    if (target.stateBytes4dTo52[0] == ordinaryCategory) {
        result.actions.push_back(Action::decrementEnemyCount);
    }
    result.actions.push_back(Action::audio);
    if (guardian) {
        result.actions.push_back(Action::guardianScore);
        result.actions.push_back(Action::sortObjects);
        if (campaignLevel == finalCampaignLevel) {
            result.actions.push_back(Action::victory);
            result.actions.push_back(Action::deactivateLevel);
        } else {
            result.actions.push_back(Action::guardianReward);
        }
    }
    if (target.stateBytes4dTo52[4] == specialRewardClass) {
        result.actions.push_back(Action::specialRewardIfEligible);
    }
    return result;
}

} //namespace stellar7
