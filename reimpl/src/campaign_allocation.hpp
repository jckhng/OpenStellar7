#pragma once

#include "stellar7/gameplay.hpp"
#include <limits>

namespace stellar7 {

// Borrowed frame target and temporary rebuild bookkeeping, not another pool owner.
struct GameplayModel::CampaignRetirement {
    const Level1Entity& behaviorTarget;
    Level2Continuation continuation;
    std::size_t lastNonEffectRemoval = std::numeric_limits<std::size_t>::max();
    std::size_t effectCount = 0;
};

} // namespace stellar7
