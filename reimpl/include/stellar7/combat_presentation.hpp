#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>

namespace stellar7 {
inline constexpr std::uint64_t combatFeedbackLifetime=6;
struct CombatCue {
    std::uint64_t born;
    std::size_t level;
    int amount;
    bool active(std::size_t currentLevel,std::uint64_t tick) const {
        return level==currentLevel&&tick>=born&&tick-born<combatFeedbackLifetime;
    }
};
struct CombatFeedback {
    std::optional<CombatCue> hit,damage;
};
}
