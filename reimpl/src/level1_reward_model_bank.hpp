#pragma once

#include "stellar7/gameplay.hpp"

#include <stdexcept>

namespace stellar7 {

inline std::uint16_t level1RewardModelCapacity(const GameplayAssets& assets) {
    constexpr std::uint16_t sharedModelCount = 44;
    if (assets.models.size() <= sharedModelCount) {
        throw std::invalid_argument("guardian reward requires the decoded Level1 model banks");
    }
    if (assets.campaignLevels.empty()) {
        if (assets.sharedModelRegistry[0] != sharedModelCount) {
            throw std::invalid_argument("guardian reward fixture lacks explicit shared-bank ancestry");
        }
        return sharedModelCount;
    }
    const auto localCount = assets.campaignLevels.front().models.size();
    if (localCount == 0 || localCount > assets.models.size()
        || assets.models.size() - localCount != sharedModelCount) {
        throw std::invalid_argument("guardian reward has inconsistent decoded model-bank ancestry");
    }
    //The legacy registry reserves256 entries; D03C uses the actual loaded bank.
    return sharedModelCount;
}

} // namespace stellar7
