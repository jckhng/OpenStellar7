#include "../src/campaign_guardian_children.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace {
using namespace stellar7;
using Action = CampaignActorHitAction;
using Pool = std::array<Level1Entity, Level1State::projectilePoolSize>;
constexpr std::uint32_t model22Radius = 134;

Level1Entity guardian() {
    Level1Entity actor{};
    actor.type = 44;
    actor.position = {1000, 2000, 500};
    actor.yaw = 0x4000;
    actor.presenterDistance = 9000;
    actor.stateBytes4dTo52 = {0xff, 40, 2, 0, 5, 0};
    return actor;
}

Pool retainedPool() {
    Pool pool{};
    for (std::size_t slot = 0; slot < pool.size(); ++slot) {
        auto& actor = pool[slot];
        actor.allocationIndex = 32 + slot;
        actor.type = 23;
        actor.flags = 0x20;
        actor.stateBytes4dTo52 = {0xfd, 0, 3, 7, 0xf3, 9};
        actor.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
        actor.presenterDistance = 4567;
        actor.targetHeading = 0x6123;
        actor.behaviorTimer = 22;
        actor.weaponCooldown = 31;
        actor.weaponSlotTimers = {10, 20, 30};
        actor.collisionContactAllocationIndex = 12;
        actor.modelAnimationVariant = 4;
        actor.presentationModelAnimationVariant = 5;
        actor.ownerIsPlayer = true;
        actor.zVelocity = 700;
    }
    pool[0].stateBytes4dTo52[1] = 1;
    return pool;
}

void unchangedFields(const Level1Entity& actual, const Level1Entity& inherited) {
    assert(actual.allocationIndex == inherited.allocationIndex);
    assert(actual.flags == inherited.flags && actual.renderProfile == inherited.renderProfile);
    assert(actual.presenterDistance == inherited.presenterDistance && actual.targetHeading == inherited.targetHeading);
    assert(actual.behaviorTimer == inherited.behaviorTimer && actual.weaponCooldown == inherited.weaponCooldown);
    assert(actual.weaponSlotTimers == inherited.weaponSlotTimers);
    assert(actual.collisionContactAllocationIndex == inherited.collisionContactAllocationIndex);
    assert(actual.stateBytes4dTo52[0] == 0xfd && actual.stateBytes4dTo52[3] == 7 && actual.stateBytes4dTo52[5] == 9);
    assert(actual.modelAnimationVariant == inherited.modelAnimationVariant);
    assert(actual.presentationModelAnimationVariant == inherited.presentationModelAnimationVariant);
}

void aimedUsesRetainedMotionAndLevelAltitude() {
    auto pool = retainedPool();
    auto& inherited = pool[1];
    inherited.forwardVelocity = 90;
    inherited.xVelocity = 17;
    inherited.yVelocity = 19;
    const auto owner = guardian();
    const FixedPosition player{1000, 40000, 0};
    constexpr std::array<std::int32_t, 7> expectedAltitude{{2500, 500, 700, 2500, 500, 500, 500}};
    for (std::uint8_t level = 0; level < expectedAltitude.size(); ++level) {
        const auto birth = prepareCampaignGuardianProjectile(pool, owner, 3, player, level,
            model22Radius, CampaignGuardianAttack::aimedProjectile);
        assert(birth && birth->physicalSlot == 1 && birth->eventId == 6);
        const auto& shot = birth->actor;
        assert((shot.position == FixedPosition{1017, 2109, expectedAltitude[level]}));
        assert(shot.type == 22 && shot.collisionRadius == model22Radius);
        assert(shot.forwardVelocity == 500 && shot.zVelocity == 0);
        assert(shot.xVelocity == 17 && shot.yVelocity == 19);
        assert(shot.yaw == fixedHeadingBetween(shot.position, player));
        assert(shot.pitch == fixedHeadingBetween({}, {expectedAltitude[level], 9000, 0}));
        assert(shot.stateBytes4dTo52[1] == 80 && shot.stateBytes4dTo52[2] == 1 && shot.stateBytes4dTo52[4] == 0);
        assert(!shot.ownerIsPlayer && shot.ownerAllocationIndex == 3);
        unchangedFields(shot, inherited);
    }
    assert(pool[1].stateBytes4dTo52[1] == 0 && pool[1].zVelocity == 700);
}

void volleyCopiesPoseWithoutAimingOrMotion() {
    auto pool = retainedPool();
    pool[1].forwardVelocity = 90;
    pool[1].pitchVelocity = 11;
    pool[1].rollVelocity = 12;
    pool[1].yawVelocity = 13;
    pool[1].xVelocity = 17;
    pool[1].yVelocity = 19;
    auto owner = guardian();
    owner.pitch = 0x1000;
    owner.roll = 0x2000;
    owner.weaponCooldown = 99;
    owner.weaponSlotCount = 0;
    owner.targetHeading = 0x8000;
    const auto birth = prepareCampaignGuardianProjectile(pool, owner, 5, {-50000, -60000, 0}, 6,
        model22Radius, CampaignGuardianAttack::volleyProjectile80);
    assert(birth && birth->eventId == 15);
    const auto& shot = birth->actor;
    assert(shot.position == owner.position);
    assert(shot.pitch == owner.pitch && shot.roll == owner.roll && shot.yaw == owner.yaw);
    assert(shot.pitchVelocity == 11 && shot.rollVelocity == 12 && shot.yawVelocity == 13);
    assert(shot.xVelocity == 17 && shot.yVelocity == 19 && shot.zVelocity == 0);
    assert(shot.forwardVelocity == 500 && shot.stateBytes4dTo52[1] == 80);
    assert(shot.stateBytes4dTo52[2] == 0 && shot.stateBytes4dTo52[4] == 0);
    assert(owner.weaponCooldown == 99 && owner.weaponSlotCount == 0);
    unchangedFields(shot, pool[1]);
}

void projectileExhaustionAndAncestry() {
    auto pool = retainedPool();
    const auto owner = guardian();
    for (auto& shot : pool) {
        shot.stateBytes4dTo52[1] = 1;
    }
    assert(!prepareCampaignGuardianProjectile(pool, owner, 0, {}, 3,
        model22Radius, CampaignGuardianAttack::aimedProjectile));
    pool[6].stateBytes4dTo52[1] = 0;
    auto birth = prepareCampaignGuardianProjectile(pool, owner, 0, {}, 3,
        model22Radius, CampaignGuardianAttack::aimedProjectile);
    assert(birth && birth->physicalSlot == 6);
    pool[6].renderProfile = WorldActorRenderProfile::unknown;
    auto rejected = false;
    try {
        prepareCampaignGuardianProjectile(pool, owner, 0, {}, 3,
            model22Radius, CampaignGuardianAttack::aimedProjectile);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

void guardianHitOrderAndSignedDamage() {
    auto target = guardian();
    auto hit = campaignActorHit(target, 17, 3);
    assert(hit.health == 23 && !hit.lethal && hit.eventId == 5);
    assert(hit.effectFirstFrame == 0 && hit.effectTerminalFrame == 12);
    assert((hit.actions == std::vector<Action>{Action::effect, Action::audio}));
    target.stateBytes4dTo52[1] = 16;
    target.presenterDistance = 0x1964 * 9;
    hit = campaignActorHit(target, 17, 3);
    assert(hit.health == 0 && hit.lethal && hit.eventId == 12);
    assert(hit.effectFirstFrame == 28 && hit.effectTerminalFrame == 32);
    assert((hit.actions == std::vector<Action>{Action::registerBonusDeath, Action::effect,
        Action::retire, Action::decrementEnemyCount, Action::audio, Action::guardianScore,
        Action::sortObjects, Action::guardianReward}));
    hit = campaignActorHit(target, 20, 6);
    assert((hit.actions == std::vector<Action>{Action::registerBonusDeath, Action::effect,
        Action::retire, Action::decrementEnemyCount, Action::audio, Action::guardianScore,
        Action::sortObjects, Action::victory, Action::deactivateLevel}));
    target.stateBytes4dTo52[1] = 1;
    hit = campaignActorHit(target, 130, 6);
    assert(hit.health == 127 && !hit.lethal); //SUB byte wraps before signed CMP.
    target.stateBytes4dTo52[1] = 129;
    assert(campaignActorHit(target, 0, 6).lethal);
    target.stateBytes4dTo52[1] = 0;
    assert(campaignActorHit(target, 17, 3).lethal); //OLD can retain a retired pointer.
}

void contactHitClassesAndRewardProtection() {
    auto target = guardian();
    target.type = 13;
    target.stateBytes4dTo52[4] = 0;
    target.stateBytes4dTo52[1] = 20;
    auto hit = campaignActorHit(target, 30, 3);
    assert(hit.lethal && hit.eventId == 18);
    assert((hit.actions == std::vector<Action>{Action::registerBonusDeath, Action::effect,
        Action::retire, Action::decrementEnemyCount, Action::audio}));
    target.stateBytes4dTo52[1] = 1;
    assert(campaignActorHit(target, 1, 3).eventId == 6);
    target.stateBytes4dTo52[0] = 0xfe;
    target.stateBytes4dTo52[4] = 0xf3;
    hit = campaignActorHit(target, 20, 6);
    assert(hit.actions.back() == Action::specialRewardIfEligible);
    assert(std::find(hit.actions.begin(), hit.actions.end(), Action::decrementEnemyCount) == hit.actions.end());
    for (const auto type : {25, 26}) {
        target.type = static_cast<std::int16_t>(type);
        hit = campaignActorHit(target, 30, 3);
        assert(hit.health == 100 && !hit.lethal && hit.eventId == 5);
        assert((hit.actions == std::vector<Action>{Action::effect, Action::audio}));
    }
}
} //namespace

int main() {
    aimedUsesRetainedMotionAndLevelAltitude();
    volleyCopiesPoseWithoutAimingOrMotion();
    projectileExhaustionAndAncestry();
    guardianHitOrderAndSignedDamage();
    contactHitClassesAndRewardProtection();
}
