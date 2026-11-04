#include "gameplay_render_support.hpp"

namespace stellar7 {

bool isLevel3Phase(const GameplayPhase phase) noexcept {
    return phase == GameplayPhase::level3Handoff
        || phase == GameplayPhase::level3 || phase == GameplayPhase::level3Native;
}

bool isPlayableCampaignPhase(const GameplayPhase phase) noexcept {
    return phase == GameplayPhase::playable;
}

bool isLevel2Phase(const GameplayPhase phase) noexcept {
    return phase == GameplayPhase::level2Handoff
        || phase == GameplayPhase::level2;
}

bool isCampaignGameplayPhase(const GameplayPhase phase) noexcept {
    return phase == GameplayPhase::level2Handoff
        || phase == GameplayPhase::level2
        || phase == GameplayPhase::level2SuccessPresentation
        || phase == GameplayPhase::level3SuccessPresentation
        || phase == GameplayPhase::level5
        || isPlayableCampaignPhase(phase)
        || isLevel3Phase(phase);
}

int wrap(const int value, const int modulus) {
    int wrapped = value % modulus;
    if (wrapped < 0) {
        wrapped += modulus;
    }
    return wrapped;
}

int signedAngle(const std::int32_t angle) {
    const int lowWord = static_cast<int>(
        static_cast<std::uint16_t>(angle)
    );
    return lowWord < 0x8000 ? lowWord : lowWord - 0x10000;
}

int arithmeticShiftRightSix(const int value) {
    if (value >= 0) {
        return value / 64;
    }
    return -((-value + 63) / 64);
}

bool isHostile(const Level1Entity& entity) {
    return (
        entity.active()
        && static_cast<std::int8_t>(entity.stateBytes4dTo52[0]) == -1
    );
}

const Level1Entity* primaryHostile(const GameplayModel& model) {
    const auto& state = model.level1State();
    const Level1Entity* selected = nullptr;
    std::uint32_t selectedDistance = 0;
    for (const auto& entity : state.entities()) {
        if (!isHostile(entity)) {
            continue;
        }
        const auto distance = approximatePlanarDistance(
            state.player().position,
            entity.position
        );
        if (selected == nullptr || distance < selectedDistance) {
            selected = &entity;
            selectedDistance = distance;
        }
    }
    return selected;
}

}  // namespace stellar7
