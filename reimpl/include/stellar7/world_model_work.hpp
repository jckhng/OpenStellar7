#pragma once

#include "stellar7/render_work.hpp"

namespace stellar7 {

enum class WorldModelMatrixProfile { identityOrYaw, level1Banked, level1InlineSpin, level1Arrival, level1Deployment, level2Constructor };

struct WorldModelGeometryInput {
    FixedMatrixQ14 matrix;
    FixedPosition translation;
    std::vector<std::size_t> groupOrder;
    std::uint16_t projectionShift = 7;
    std::int16_t nearPlane = 128;
    ProjectedPoint screenCenter{159, 84};
    std::array<std::int16_t, 3> light{256, -256, 362};
    std::array<std::uint8_t, 256> shadeLevels{};
    std::array<std::uint8_t, 256> lightingAdjustments{};
    std::array<std::uint8_t, 256> colorRemap{};
    std::uint16_t vertexPrecision = 0;
    FixedPosition fullCenter;
    PolygonClipMode clipping = PolygonClipMode::Disabled;
    //Owned heap bytes; empty selects the original null animation pointer path.
    std::vector<std::uint8_t> animationSelectors;
    WorldModelMatrixProfile matrixProfile = WorldModelMatrixProfile::identityOrYaw;
};

struct WorldModelPolygonWork {
    std::size_t groupIndex;
    std::size_t shapeIndex;
    std::vector<ProjectedPoint> points;
    std::uint8_t primaryColor;
    std::uint8_t secondaryColor;
    bool filled;
    // Ends before492d:4aac CALLF. Raster work must include that call exactly once.
    std::vector<NormalWorkOperation> prefix;
};

struct WorldModelGeometryWork {
    std::vector<WorldModelPolygonWork> polygons;
    // Remaining geometry and cb7a far epilogue after the last polygon body.
    std::vector<NormalWorkOperation> suffix;
};

// cec1 throughcb7a RETF: first variants, kind0, VGA colors, no near clipping,
// default precision, identity/yaw matrix and present lighting/remap tables.
// Polygon bodies are intentionally separated, including collapsed/line bodies.
WorldModelGeometryWork worldModelGeometryOperations(
    const ModelLod& lod,
    const WorldModelGeometryInput& input
);

WorldModelGeometryWork level2ConstructorModelGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input, std::array<std::uint16_t,3> actorAngles,
    const WorldModelViewInput& view, std::int16_t type);

}
