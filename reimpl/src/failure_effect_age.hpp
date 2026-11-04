#pragma once

#include <cstdint>

namespace stellar7::detail {
struct FailureEffectAge {
    std::uint8_t incremented;
    bool retired;
};
//0AF4 precedes the projection gate; 0BA9 compares the incremented byte.
inline FailureEffectAge failureEffectAge(const std::uint8_t age, const std::uint8_t end) noexcept {
    const auto incremented = static_cast<std::uint8_t>(age + 1U);
    return {incremented, static_cast<std::int8_t>(incremented) >= static_cast<std::int8_t>(end)};
}
} //namespace stellar7::detail
