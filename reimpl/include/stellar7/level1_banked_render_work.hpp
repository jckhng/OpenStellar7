#pragma once

#include "stellar7/render_work.hpp"

namespace stellar7 {

// CBCA's Type11, pitch0/nonzero-roll path with a flat main camera and precision0.
// The source38FF composition consumes CX5240, including its low-word residue.
WorldModelSetupWork level1BankedModelSetupOperations(FixedPosition center,
    std::array<std::uint16_t, 3> actorAngles, std::uint16_t projectedRadius,
    std::int16_t type, const Model3d& model, std::uint16_t registryScale,
    std::uint16_t registryCap, const WorldModelViewInput& view);

}
