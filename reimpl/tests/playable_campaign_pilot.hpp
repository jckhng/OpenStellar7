#pragma once

#include "stellar7/gameplay.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace stellar7::test {

enum class CampaignPilotGoal { combat, collect };
enum class CampaignPilotPrediction { linear, angular };

//Test input driver only: observes retained actors and never changes model state.
inline GameplayInput campaignPilotControls(const GameplayModel& model, const Level1Entity& target,
    const CampaignPilotGoal goal, const CampaignPilotPrediction prediction = CampaignPilotPrediction::linear) {
    constexpr std::uint32_t projectileSpeed = 500;
    constexpr std::uint32_t projectileLifetime = 64;
    constexpr std::uint32_t minimumGuardianClearance = 6500;
    constexpr std::uint32_t hostileProjectileClearance = 3500;
    constexpr std::int32_t escapeDistance = 12000;
    constexpr double hazardMargin = 1600;
    const auto& player = model.level2Player();
    const auto distance = approximatePlanarDistance(player.position, target.position);
    const bool combat = goal == CampaignPilotGoal::combat;
    auto aim = target.position;
    if (combat) {
        if (prediction == CampaignPilotPrediction::angular) {
            auto predicted = target;
            for (std::uint32_t flight = 1; flight <= projectileLifetime; ++flight) {
                advanceEntityKinematics(predicted);
                aim = predicted.position;
                if (approximatePlanarDistance(player.position, aim) <= projectileSpeed * flight) {
                    break;
                }
            }
        } else {
            constexpr std::uint32_t maximumLeadFrames = 40;
            auto next = target;
            advanceEntityKinematics(next);
            const auto lead = static_cast<std::int32_t>(std::min(maximumLeadFrames, distance / projectileSpeed));
            aim.x += (next.position.x - target.position.x) * lead;
            aim.y += (next.position.y - target.position.y) * lead;
        }
    }
    const auto aimError = static_cast<std::int16_t>(presenterTargetHeading(player.position, aim) - player.yaw);
    auto waypoint = aim;
    bool escaping = false;
    const auto lateral = [&](const Level1Entity& threat) {
        const auto left = static_cast<std::uint16_t>(threat.yaw + 0x4000U);
        const auto right = static_cast<std::uint16_t>(threat.yaw - 0x4000U);
        const auto difference = [&](const std::uint16_t heading) {
            return std::abs(static_cast<int>(static_cast<std::int16_t>(heading - player.yaw)));
        };
        Level1Entity direction{};
        direction.yaw = difference(left) <= difference(right) ? left : right;
        direction.forwardVelocity = static_cast<std::int16_t>(escapeDistance);
        advanceEntityKinematics(direction);
        waypoint = {player.position.x + direction.position.x, player.position.y + direction.position.y, 0};
        escaping = true;
    };
    const bool guardian = target.type == 44 || target.type == 45;
    if (combat && guardian) {
        auto next = target;
        advanceEntityKinematics(next);
        const auto clearance = std::max(minimumGuardianClearance, player.collisionRadius + target.collisionRadius + 2500U);
        const bool approaching = approximatePlanarDistance(player.position, next.position) < distance;
        //A50E0/1 is the launch/descent pass;2/3 is the climb/retreat firing window.
        const bool a50ePass = model.campaignLevel() == 2 && target.stateBytes4dTo52[5] <= 1;
        if (distance < clearance * 3 && (approaching || a50ePass)) {
            lateral(target);
        }
    }
    if (combat) {
        for (const auto& actor : model.level2Entities()) {
            if (actor.ownerIsPlayer || actor.stateBytes4dTo52[0] != 0xfd || actor.type < 17 || actor.type > 22) {
                continue;
            }
            const auto separation = approximatePlanarDistance(player.position, actor.position);
            auto next = actor;
            advanceEntityKinematics(next);
            if (separation < hostileProjectileClearance && approximatePlanarDistance(player.position, next.position) < separation) {
                lateral(actor);
                break;
            }
        }
    }
    double nearestHazard = 2;
    const auto routeX = static_cast<double>(waypoint.x) - player.position.x;
    const auto routeY = static_cast<double>(waypoint.y) - player.position.y;
    const auto lengthSquared = routeX * routeX + routeY * routeY;
    if (lengthSquared > 0) {
        for (const auto& actor : model.level2Entities()) {
            if (!actor.active() || actor.type < 14 || actor.type > 16
                || (combat && &actor == &target)) {
                continue;
            }
            const auto x = static_cast<double>(actor.position.x) - player.position.x;
            const auto y = static_cast<double>(actor.position.y) - player.position.y;
            const auto progress = (x * routeX + y * routeY) / lengthSquared;
            if (progress < 0 || progress > 1 || progress >= nearestHazard) {
                continue;
            }
            const auto closestX = routeX * progress - x;
            const auto closestY = routeY * progress - y;
            const auto clearance = actor.collisionRadius + player.collisionRadius + hazardMargin;
            if (closestX * closestX + closestY * closestY >= clearance * clearance) {
                continue;
            }
            const auto length = std::sqrt(lengthSquared);
            const auto side = routeX * y - routeY * x > 0 ? -1.0 : 1.0;
            waypoint.x = actor.position.x + static_cast<std::int32_t>(-routeY / length * clearance * side);
            waypoint.y = actor.position.y + static_cast<std::int32_t>(routeX / length * clearance * side);
            nearestHazard = progress;
        }
    }
    const auto error = static_cast<std::int16_t>(presenterTargetHeading(player.position, waypoint) - player.yaw);
    const auto braking = static_cast<int>(error) - 4 * player.yawVelocity;
    GameplayInput input{};
    input.left = braking > 120;
    input.right = braking < -120;
    input.forward = combat || escaping || std::abs(static_cast<int>(error)) < 4000;
    input.fire = combat && std::abs(static_cast<int>(aimError)) < 1500;
    return input;
}

} //namespace stellar7::test
