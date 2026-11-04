#include "stellar7/visibility_state.hpp"
#include "stellar7/playable_system_effects.hpp"

#include <cstdint>
#include <stdexcept>

namespace stellar7 {
namespace {

constexpr std::size_t supportedAllocationCount = 7;
constexpr std::size_t pooledAllocationCount = 48;
constexpr std::int64_t sourceListRefreshDistance = 1024;
constexpr std::uint16_t sourceVisibilityLimit = 65000;
constexpr std::uint8_t projectedFlag = 4;

std::uint32_t sourceMagnitude(const std::int32_t camera,
    const std::int32_t actor) noexcept {
    const auto difference = static_cast<std::uint32_t>(camera)
        - static_cast<std::uint32_t>(actor);
    if ((difference & 0x80000000U) == 0) {
        return difference;
    }
    if (difference == 0xffff8000U) {
        return 32767;
    }
    return 0U - difference;
}

bool sourceVisibleAxis(const std::int32_t camera,
    const std::int32_t actor) noexcept {
    const auto magnitude = sourceMagnitude(camera, actor);
    const auto high = static_cast<std::int16_t>(magnitude >> 16);
    return high < 0 || (high == 0 && (magnitude & 0xffffU) < sourceVisibilityLimit);
}

} // namespace

VisibilityTracking VisibilityState::tracking() const noexcept {
    return tracking_;
}

void VisibilityState::invalidate() noexcept {
    tracking_ = VisibilityTracking::unknown;
}

void VisibilityState::resetLevel() noexcept {
    tracking_ = VisibilityTracking::sourceInitialized;
    refreshRequested_ = true;
}

bool VisibilityState::refreshRequested() const noexcept {
    return refreshRequested_;
}

const std::optional<FixedPosition>& VisibilityState::lastCamera() const noexcept {
    return lastCamera_;
}

void VisibilityState::allocationSucceeded() noexcept {
    refreshRequested_ = true;
}

void VisibilityState::removalSucceeded() noexcept {
    refreshRequested_ = true;
}

VisibilityListResult VisibilityState::refresh(const FixedPosition camera,
    std::vector<Level1Entity>& actors,
    const std::vector<std::size_t>& allocationOrder,
    const std::vector<std::size_t>& activeOrder, const VisibilityDomain domain) {
    if (tracking_ == VisibilityTracking::unknown) {
        throw std::invalid_argument("visibility state tracking is unknown");
    }
    const bool playable = domain == VisibilityDomain::playableCampaign;
    if (playable && (camera.z < 0 || camera.z > maximumPlayableJumpAltitude
        || (lastCamera_ && (lastCamera_->z < 0 || lastCamera_->z > maximumPlayableJumpAltitude)))) {
        throw std::invalid_argument("playable visibility requires bounded Jump camera height");
    }
    const auto maximumAllocations = domain == VisibilityDomain::nativeGameplay || playable
        ? pooledAllocationCount : supportedAllocationCount;
    if (allocationOrder.size() > maximumAllocations) {
        throw std::invalid_argument(domain == VisibilityDomain::nativeGameplay || playable
            ? "native visibility exceeds pooled actor capacity"
            : "visibility state supports at most seven allocations");
    }
    std::vector<bool> allocated(actors.size());
    for (const auto index : allocationOrder) {
        if (index >= actors.size() || allocated[index]) {
            throw std::invalid_argument("invalid visibility allocation order");
        }
        if (!actors[index].active()) {
            throw std::invalid_argument("visibility state excludes dead-actor removal");
        }
        allocated[index] = true;
    }
    std::vector<bool> active(actors.size());
    for (const auto index : activeOrder) {
        if (index >= actors.size() || !allocated[index] || active[index]) {
            throw std::invalid_argument("invalid visibility active order");
        }
        active[index] = true;
    }
    if (!refreshRequested_) {
        if (!lastCamera_) {
            throw std::invalid_argument("visibility state excludes camera-motion branches");
        }
        const auto deltaX = static_cast<std::int64_t>(camera.x) - lastCamera_->x;
        const auto deltaY = static_cast<std::int64_t>(camera.y) - lastCamera_->y;
        constexpr std::int64_t unsupportedDelta = 0x80000000LL;
        if (deltaX <= -unsupportedDelta || deltaX >= unsupportedDelta
            || deltaY <= -unsupportedDelta || deltaY >= unsupportedDelta) {
            throw std::invalid_argument("visibility state excludes wrapped camera deltas");
        }
        const bool moved = deltaX <= -sourceListRefreshDistance
            || deltaX >= sourceListRefreshDistance
            || deltaY <= -sourceListRefreshDistance
            || deltaY >= sourceListRefreshDistance;
        if ((!playable && camera.z != lastCamera_->z) || (moved && domain == VisibilityDomain::qualifiedFrame)) {
            throw std::invalid_argument("visibility state excludes camera-motion branches");
        }
        if (!moved) {
            return {activeOrder, false};
        }
    }

    VisibilityListResult result{{}, true};
    result.activeOrder.reserve(allocationOrder.size());
    // Source 3828 clears the request before copying the camera or visiting actors.
    refreshRequested_ = false;
    lastCamera_ = camera;
    for (const auto index : allocationOrder) {
        auto& actor = actors[index];
        if (sourceVisibleAxis(camera.x, actor.position.x)
            && sourceVisibleAxis(camera.y, actor.position.y)) {
            result.activeOrder.push_back(index);
        } else {
            actor.flags &= static_cast<std::uint8_t>(~projectedFlag);
        }
    }
    return result;
}

} // namespace stellar7
