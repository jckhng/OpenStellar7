#pragma once

#include "stellar7/gameplay.hpp"

namespace stellar7 {
int wrap(int value, int modulus);
int signedAngle(std::int32_t angle);
int arithmeticShiftRightSix(int value);
bool isHostile(const Level1Entity& entity);
const Level1Entity* primaryHostile(const GameplayModel& model);
bool isLevel3Phase(GameplayPhase phase) noexcept;
bool isPlayableCampaignPhase(GameplayPhase phase) noexcept;
bool isLevel2Phase(GameplayPhase phase) noexcept;
bool isCampaignGameplayPhase(GameplayPhase phase) noexcept;
} // namespace stellar7
