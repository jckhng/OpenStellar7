#pragma once

#include "stellar7/ui_bitmap_work.hpp"
#include <optional>
#include <variant>

namespace stellar7 {
struct FailureEffectState {
    std::uint8_t flags;
    ProjectedPoint projected;
    std::uint16_t projectedRadius;
    std::uint8_t age;
    std::uint8_t endAge;
    std::uint16_t callback;
};
struct FailureEffectInput {
    FixedPosition center;
    std::uint32_t collisionRadius;
    FailureEffectState state;
    Viewport viewport;
    ProjectedPoint screenCenter;
    std::uint16_t page;
    std::size_t allocationCount;
    std::optional<std::size_t> matchingIndex;
};
enum class FailureEffectField { flags, projectedRadius, projectedX, projectedY,
    age, callback, endAge, rasterStyle, registryRefresh };
struct FailureEffectScalarWrite { FailureEffectField field; std::int32_t value; };
struct FailureEffectCacheWrite { FixedPosition value; };
using FailureEffectStateWrite = std::variant<FailureEffectScalarWrite,FailureEffectCacheWrite>;
struct FailureEffectRemoval { std::size_t removedIndex; std::size_t lastIndex; };
struct FailureEffectWork {
    std::vector<NormalWorkOperation> operations;
    std::vector<RadarPixelWrite> pixels;
    FailureEffectState state;
    FixedPosition cachedCenter;
    bool projected;
    bool retired;
    std::vector<FailureEffectStateWrite> stateWrites;
    std::optional<FailureEffectRemoval> removal;
};
//BA88..BC09, excludingBC09: flags8 projection, flags80 CB7A early return,
//then one deferred0AC8 callback (radarPass0/bitmapMode0), including481A.
//The caller supplies the current registry match; it applies returned observations
//once to its staged semantic owner. No persistent model/list is owned here.
FailureEffectWork failureEffectOperations(const FailureEffectInput& input,
    const std::vector<IndexedBitmap>& effectFrames);
} // namespace stellar7

namespace stellar7 {
FailureEffectWork level2FailureEffectOperations(const FailureEffectInput&,
    const std::vector<IndexedBitmap>&);
} // namespace stellar7
