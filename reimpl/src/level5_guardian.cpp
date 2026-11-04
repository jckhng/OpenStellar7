#include "level5_guardian.hpp"

#include <algorithm>
#include <cstdint>

namespace stellar7 {
namespace {
constexpr std::int32_t maximumDistance = 0x7000;
constexpr std::int32_t lateralDistance = 0x1000;
constexpr std::int16_t alignedAngle = 0x300;
constexpr std::int16_t lateralAngle = 0x3000;
constexpr std::int16_t lateralSpeed = 120;
constexpr std::int16_t turnLimit = 160;

std::int16_t sourceMagnitude(const std::int16_t value) noexcept {
    return value < 0 ? static_cast<std::int16_t>(-static_cast<std::int32_t>(value)) : value;
}

void enableAnimation(Level1Entity& actor) noexcept {
    actor.renderProfile = WorldActorRenderProfile::heapSelectorDeferredAnimation;
    actor.animatedModelPresenterEnabled = true;
}

void moveSideways(Level1Entity& actor, const std::int16_t amount) noexcept {
    if (actor.yaw == 0) {
        actor.position.x += amount;
        return;
    }
    // AB80 uses the same staged Q14 products as zero-pitch 3B3A at yaw+4000h.
    Level1Entity movement{};
    movement.yaw = static_cast<std::uint16_t>(actor.yaw + 0x4000U);
    movement.forwardVelocity = amount;
    advanceEntityKinematics(movement);
    actor.position.x += movement.position.x;
    actor.position.y += movement.position.y;
}

void steer(Level1Entity& actor, std::int16_t difference) noexcept {
    if (difference == static_cast<std::int16_t>(0x8000U)) {
        difference = 0x7fff;
    }
    actor.yawVelocity = static_cast<std::int16_t>(std::clamp<int>(difference, -turnLimit, turnLimit));
    if (actor.position.z >= 500) {
        const auto combined = static_cast<std::int16_t>(actor.yawVelocity + actor.roll);
        const auto negated = static_cast<std::int16_t>(-static_cast<std::int32_t>(combined));
        const auto adjustment = negated >= 0 ? negated / 16 : -((-static_cast<int>(negated) + 15) / 16);
        actor.roll = static_cast<std::uint16_t>(actor.roll + adjustment);
    }
}
}  // namespace

void advanceLevel5GuardianBehavior(Level1Entity& actor, const std::uint16_t sourceTick,
    const std::function<void(Level1Entity&)>& fire) {
    const auto difference = static_cast<std::int16_t>(actor.targetHeading - actor.yaw);
    actor.renderProfile = WorldActorRenderProfile::heapSelectorNoCallback;
    actor.animatedModelPresenterEnabled = false;
    if (static_cast<std::int32_t>(actor.presenterDistance) > maximumDistance) {
        actor.yaw = actor.targetHeading;
        return;
    }
    const auto magnitude = sourceMagnitude(difference);
    if (magnitude < alignedAngle) {
        if ((sourceTick & 15U) == 0) {
            fire(actor);
        }
        return;
    }
    enableAnimation(actor);
    if (static_cast<std::int32_t>(actor.presenterDistance) > lateralDistance && magnitude < lateralAngle) {
        actor.yawVelocity = 0;
        auto amount = difference > 0 ? lateralSpeed : static_cast<std::int16_t>(-lateralSpeed);
        if (actor.collisionContactIsPlayer || actor.collisionContactAllocationIndex != static_cast<std::size_t>(-1)) {
            actor.collisionContactIsPlayer = false;
            actor.collisionContactAllocationIndex = static_cast<std::size_t>(-1);
            amount = static_cast<std::int16_t>(-amount);
        }
        moveSideways(actor, amount);
        return;
    }
    actor.stateBytes4dTo52[2] = 1;
    steer(actor, difference);
}

}  // namespace stellar7
