#pragma once
#include "playable_campaign_pilot.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace naturalRoute {
using namespace stellar7;
constexpr std::size_t noObject=static_cast<std::size_t>(-1);
std::size_t find(const GameplayModel& model, const std::int16_t first, const std::int16_t last) {
    const auto& actors = model.level2Entities();
    const auto actor = std::find_if(actors.begin(), actors.end(), [first,last](const auto& value) {
        return value.active() && value.type >= first && value.type <= last;
    });
    return actor == actors.end() ? noObject : static_cast<std::size_t>(actor - actors.begin());
}

GameplayInput approach(const GameplayModel& model, const Level1Entity& target, const test::CampaignPilotGoal goal) {
    const auto combat = goal == test::CampaignPilotGoal::combat;
    const auto& player = model.level2Player();
    const auto error = static_cast<std::int16_t>(presenterTargetHeading(player.position,target.position) - player.yaw);
    const auto braking = static_cast<int>(error) - 4 * player.yawVelocity;
    const auto distance = approximatePlanarDistance(player.position,target.position);
    GameplayInput input{};
    input.left = braking > 120;
    input.right = braking < -120;
    input.fire = combat && std::abs(static_cast<int>(error)) < 1500;
    input.forward = (combat ? distance > 6500 : true) && std::abs(static_cast<int>(error)) < 4000;
    input.reverse = combat && distance < 3500;
    return input;
}

GameplayInput a50eWindowControls(const GameplayModel& model, const Level1Entity& guardian) {
    constexpr std::uint32_t shotSpeed = 500;
    constexpr std::uint32_t guardianSpeed = 300;
    constexpr std::uint32_t shotRadius = 134;
    constexpr std::uint32_t emissionDefenseRange = 6500;
    const auto& player = model.level2Player();
    const Level1Entity* target = &guardian;
    auto distance = approximatePlanarDistance(player.position, guardian.position);
    for (const auto& actor : model.level2Entities()) {
        if (actor.type != 47 || !actor.active() || actor.position.z > 500) {
            continue;
        }
        const auto candidate = approximatePlanarDistance(player.position, actor.position);
        if (candidate < emissionDefenseRange && candidate < distance) {
            target = &actor;
            distance = candidate;
        }
    }
    auto next = *target;
    advanceEntityKinematics(next);
    const auto state = guardian.stateBytes4dTo52[5];
    const auto closing = target == &guardian && state <= 2 ? guardianSpeed : 0U;
    const auto flight = static_cast<std::int32_t>((distance + shotSpeed + closing - 1) / (shotSpeed + closing));
    auto aim = target->position;
    aim.x += (next.position.x - target->position.x) * flight;
    aim.y += (next.position.y - target->position.y) * flight;
    const auto error = static_cast<std::int16_t>(presenterTargetHeading(player.position, aim) - player.yaw);
    const auto braking = static_cast<int>(error) - 4 * player.yawVelocity;
    GameplayInput input{};
    input.left = braking > 120;
    input.right = braking < -120;
    input.reverse = target != &guardian || state <= 2
        || approximatePlanarDistance(player.position, next.position) < distance;
    input.forward = !input.reverse && distance > 10000;

    //3A1C halves the sum of radii: only late descent/hold/early climb is hittable.
    auto impactHeight = target->position.z;
    if (target != &guardian) {
        impactHeight = std::max(0, impactHeight - 50 * flight);
    } else if (state == 1) {
        const auto descent = std::max(0, (impactHeight - 500 + 99) / 100);
        const auto hold = descent == 0 ? guardian.behaviorTimer : 20;
        impactHeight = std::max(500, impactHeight - 100 * flight);
        if (flight > descent + hold) {
            impactHeight += 100 * (flight - descent - hold);
        }
    } else if (state == 2) {
        impactHeight += 100 * std::max(0, flight - guardian.behaviorTimer);
    }
    const auto hitHeight = static_cast<std::int32_t>((target->collisionRadius + shotRadius) >> 1U);
    input.fire = std::abs(static_cast<int>(error)) < 1500 && impactHeight <= hitHeight;
    return input;
}



GameplayInput combatControls(const GameplayModel& model) {
    const auto reward = find(model,25,25);
    if (reward != noObject) {
        if (model.campaignLevel() == 1) {
            auto input = approach(model,model.level2Entities()[reward],test::CampaignPilotGoal::collect);
            input.fire = true;
            return input;
        }
        return test::campaignPilotControls(model,model.level2Entities()[reward],test::CampaignPilotGoal::collect,test::CampaignPilotPrediction::angular);
    }
    const auto guardian = find(model,44,45);
    if (guardian != noObject) {
        return model.campaignLevel() == 2 ? a50eWindowControls(model,model.level2Entities()[guardian])
            : approach(model,model.level2Entities()[guardian],test::CampaignPilotGoal::combat);
    }
    const Level1Entity* nearest = nullptr;
    auto distance = std::uint32_t{0xffffffffU};
    for (const auto& actor : model.level2Entities()) {
        if (!actor.active() || actor.stateBytes4dTo52[0] != 255 || actor.type < 1 || actor.type > 16 || actor.type == 7) {
            continue;
        }
        const auto candidate = approximatePlanarDistance(model.level2Player().position,actor.position);
        if (candidate < distance) {
            nearest = &actor;
            distance = candidate;
        }
    }
    if (!nearest) {
        return {};
    }
    if (model.campaignLevel() == 2) {
        return test::campaignPilotControls(model,*nearest,test::CampaignPilotGoal::combat,test::CampaignPilotPrediction::angular);
    }
    constexpr std::uint32_t primarySpeed = 500;
    constexpr std::uint32_t primaryLifetime = 64;
    constexpr std::uint32_t engagementRange = primarySpeed * primaryLifetime / 2;
    auto input = test::campaignPilotControls(model,*nearest,test::CampaignPilotGoal::combat,
        test::CampaignPilotPrediction::angular);
    const auto error = static_cast<std::int16_t>(presenterTargetHeading(model.level2Player().position,
        nearest->position) - model.level2Player().yaw);
    input.forward = distance > engagementRange;
    input.reverse = distance <= engagementRange && std::abs(static_cast<int>(error)) < 4000;
    return input;
}

}
