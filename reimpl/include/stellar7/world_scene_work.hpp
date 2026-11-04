#pragma once

#include "stellar7/gameplay.hpp"
#include "stellar7/render_work.hpp"
#include "stellar7/level1_world_work.hpp"

#include <optional>

namespace stellar7 {

struct WorldSceneActor {
    WorldActorWorkInput actor;
    std::array<std::uint16_t, 3> angles;
    std::int16_t type;
    std::uint32_t collisionRadius;
    std::uint16_t drawCallback;
    std::uint16_t animation;
    std::optional<Level1AnimationState> heapAnimation;
    std::optional<std::array<std::uint8_t, 6>> inlineAnimation;
    std::optional<std::uint8_t> inlineSelector; //Owned inline+4F; other inline bytes are unread.
    std::optional<CompletionBeamPresentation> beamPresentation;
};

struct WorldSceneProfile {
    std::uint16_t registryScale;
    std::uint16_t registryCap;
    std::uint16_t beforeCallback;
    std::uint16_t afterCallback;
};

struct WorldPixelWrite {
    ProjectedPoint point;
    std::uint8_t color;
};

struct WorldSceneWork {
    std::vector<NormalWorkOperation> operations;
    std::vector<WorldPixelWrite> pixels;
    std::vector<FixedPosition> centers;
    std::vector<bool> culled;
    std::vector<std::size_t> polygonCounts;
    std::vector<std::optional<Level1AnimationState>> animationStates;
};

enum class WorldSceneMatrixProfile { identityOrYaw, level1Banked, level1Projectile, level1Combat, level1Bonus, level1Arrival, level1Deployment, level2Constructor, level2Combat, level2Periodic };

// b9a5 through world return (runtime linear850c). Includes every model/raster
// child once; excludes preceding view setup and interrupt/device work.
WorldSceneWork worldSceneOperations(const GameplayAssets& assets, const ViewTransform& view,
    const std::vector<WorldSceneActor>& actors, WorldSceneProfile profile,
    std::array<std::int16_t, 3> transformedLight = {256, -256, 362},
    WorldSceneMatrixProfile matrixProfile = WorldSceneMatrixProfile::identityOrYaw);

}
