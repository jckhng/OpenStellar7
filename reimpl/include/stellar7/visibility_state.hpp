#pragma once

#include "stellar7/level1.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace stellar7 {

struct VisibilityListResult {
    std::vector<std::size_t> activeOrder;
    bool refreshed;
};

enum class VisibilityTracking {
    sourceInitialized,
    unknown,
};
enum class VisibilityDomain { qualifiedFrame, nativeGameplay, playableCampaign };

// Retained 1296/417c ownership. Keep this instance across level pool resets.
class VisibilityState {
public:
    VisibilityTracking tracking() const noexcept;
    void invalidate() noexcept;
    void resetLevel() noexcept;
    bool refreshRequested() const noexcept;
    const std::optional<FixedPosition>& lastCamera() const noexcept;
    void allocationSucceeded() noexcept;
    void removalSucceeded() noexcept;

    // For alive allocation lists of at most seven entries, retained reuse
    // requires unchanged height and per-axis planar deltas strictly inside
    // (-1024, 1024). Native gameplay also admits XY-triggered rebuilds and the
    //48 pooled actors; the separately owned player is not an actor-list input.
    //Explicit playable Jump permits bounded Z changes. Source3768 still uses
    //only dirty/XY thresholds; a Z-only change retains the old camera/list.
    VisibilityListResult refresh(FixedPosition camera,
        std::vector<Level1Entity>& actors,
        const std::vector<std::size_t>& allocationOrder,
        const std::vector<std::size_t>& activeOrder,
        VisibilityDomain domain = VisibilityDomain::qualifiedFrame);

private:
    VisibilityTracking tracking_ = VisibilityTracking::sourceInitialized;
    bool refreshRequested_ = true;
    std::optional<FixedPosition> lastCamera_;
};

} // namespace stellar7
