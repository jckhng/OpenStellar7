#pragma once

#include "stellar7/level1.hpp"

#include <string>

namespace stellar7 {

std::string level1StateTraceJson(
    const Level1State& state,
    const std::string& boundary
);

}  // namespace stellar7
