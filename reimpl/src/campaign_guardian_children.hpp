#pragma once

#include "campaign_guardian.hpp"

#include <array>
#include <optional>

namespace stellar7 {

struct CampaignGuardianProjectileBirth {
    std::size_t physicalSlot;
    Level1Entity actor;
    std::uint16_t eventId;
};

//44BA and ACEC/995E operate on the retained physical FD record. The caller
//commits the returned value and appends that identity after remapping views.
//ACEC returns lifetime80; A440's caller subsequently replaces it with64.
std::optional<CampaignGuardianProjectileBirth> prepareCampaignGuardianProjectile(
    const std::array<Level1Entity, Level1State::projectilePoolSize>& pool,
    const Level1Entity& owner, std::size_t ownerPhysicalSlot,
    FixedPosition playerPosition, std::uint8_t campaignLevel,
    std::uint32_t model22Radius, CampaignGuardianAttack attack);

enum class CampaignActorHitAction {
    registerBonusDeath,
    effect,
    retire,
    decrementEnemyCount,
    audio,
    guardianScore,
    sortObjects,
    guardianReward,
    victory,
    deactivateLevel,
    specialRewardIfEligible,
};

struct CampaignActorHitResult {
    std::uint8_t health;
    bool lethal;
    std::uint8_t effectFirstFrame;
    std::uint8_t effectTerminalFrame;
    std::uint16_t eventId;
    std::vector<CampaignActorHitAction> actions;
};

//1176's actor domain, including signed byte subtraction and class5 dispatch.
//Effect ages are the arguments to0BC8, before4580/presentation increments.
//Children act on caller-held physical identities. ClassF3's final action
//requires the caller's DS1292/contact identity gate before45C6(source,1).
CampaignActorHitResult campaignActorHit(const Level1Entity& target,
    std::uint8_t damage, std::uint8_t campaignLevel);

} //namespace stellar7
