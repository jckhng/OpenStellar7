#pragma once

#include "stellar7/normal_clock.hpp"
#include "stellar7/resource_codec.hpp"

namespace stellar7 {

struct Level1AnimationState {
    std::uint8_t reload;
    std::uint8_t countdown;
    std::uint8_t variant;
};

struct Level1AnimationWork {
    Level1AnimationState state;
    std::vector<NormalWorkOperation> operations;
};

//130C through near RET, including Type11's zero-count CFBC/CFEC metadata path.
//Excludes caller arguments/CALL/cleanup; the returned selector is retained once.
Level1AnimationWork level1AnimationOperations(Level1AnimationState state, const Model3d& model);

}
