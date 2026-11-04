#pragma once

#include "stellar7/level1.hpp"

#include <cstddef>

namespace stellar7 {

// An identity in49F8/45D2; index selects entity storage or the persistent pool.
struct Level1ObjectRef {
    Level1PresentationKind kind;
    std::size_t index;
};

constexpr bool operator==(const Level1ObjectRef& first, const Level1ObjectRef& second) noexcept {
    return first.kind == second.kind && first.index == second.index;
}

constexpr bool operator!=(const Level1ObjectRef& first, const Level1ObjectRef& second) noexcept {
    return !(first == second);
}

} // namespace stellar7
