#pragma once

#include "stellar7/indexed_render.hpp"
#include "stellar7/level1.hpp"

#include <functional>

namespace stellar7 {

struct NativeEntryFrame {
    IndexedScreen indexed;
    std::vector<std::int16_t> samples;
    std::uint64_t elapsedNanoseconds;
};

struct NativeTimedFrame {
    std::uint64_t sampleEnd;
    IndexedScreen indexed;
};

struct NativeSessionDelivery {
    std::uint64_t firstSample = 0;
    std::vector<NativeEntryFrame> entry;
    std::vector<std::int16_t> samples;
    std::vector<Level1AudioEvent> events;
    std::vector<NativeTimedFrame> presentations;
    bool exitRequested = false;
    std::optional<ProjectedPoint> pointerPosition;
};

// Synchronous, borrowed delivery after source qualification; never reenter its owner.
using NativeDeliverySink = std::function<void(const NativeSessionDelivery&)>;

} // namespace stellar7
