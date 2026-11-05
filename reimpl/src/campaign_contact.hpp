#pragma once

#include "stellar7/gameplay.hpp"

namespace stellar7 {

// Borrowed frame observations. Live records and allocation callbacks remain owned
// by GameplayModel and advanceLevel2; a contact pass cannot retain this view.
struct GameplayModel::CampaignContactStart {
    const std::vector<Level2EntityIdentity>& identities;
    const std::array<bool, level2ActorPoolSize_>& actors;
    const std::array<bool, level2ProjectilePoolSize_>& projectiles;
    bool retainedLevel3;
    bool guardianHitActions;
};

struct GameplayModel::CampaignContactMotion {
    const std::array<FixedPosition, level2ActorPoolSize_>& positions;
    const std::array<bool, level2ActorPoolSize_>& valid;
    const std::array<std::size_t, level2ActorPoolSize_>& order;
};

struct GameplayModel::CampaignPlayerContactResult {
    CampaignContactFlow flow = CampaignContactFlow::continueFrame;
    std::size_t retiredType48ActorSlot = level2ActorPoolSize_;
};

struct GameplayModel::CampaignProjectileContactResult {
    CampaignContactFlow flow = CampaignContactFlow::continueFrame;
    std::size_t retiredType48ActorSlot = level2ActorPoolSize_;
    std::array<bool, level2ActorPoolSize_> guardianRetiredContact{};
};

inline bool GameplayModel::campaignActorExistedAtStart(const std::size_t entityIndex,
    const CampaignContactStart& start) const noexcept {
    const auto actorSlot = level2EntityActorPoolSlots_[entityIndex];
    return actorSlot >= level2ActorPoolSize_ || start.actors[actorSlot];
}

} // namespace stellar7
