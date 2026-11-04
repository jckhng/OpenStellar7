#pragma once

#include "stellar7/level1.hpp"

namespace stellar7 {
enum class NativePointerCapability { unavailable, portablePointer };
enum class NativePointerPhase { sample, cancel };
struct NativePointerInput {
    ProjectedPoint position{};
    bool primary = false;
    bool secondary = false;
    NativePointerPhase phase = NativePointerPhase::sample;
};
} // namespace stellar7
