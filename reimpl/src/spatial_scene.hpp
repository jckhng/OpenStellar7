#pragma once
#include "stellar7/level1.hpp"
#include "stellar7/resource_codec.hpp"
#include <array>
#include <optional>

namespace stellar7 {
// Native base-camera coordinates: X right, Y forward, Z up; no projection/clipping.
struct SpatialPoint {float x=0,y=0,z=0;};
enum class SpatialMaterial {standard,groundDetail,shadow,ground};
inline constexpr std::int32_t spatialGroundHeight=-300;
struct SpatialSurface {
    std::vector<SpatialPoint> points;
    std::uint8_t color=0;
    bool filled=true;
    double lighting=1,opacity=1;
    std::optional<Rgb> tint;
    std::size_t allocation=0;
    std::int16_t type=0;
    std::optional<std::uint64_t> incarnation;
    SpatialMaterial material=SpatialMaterial::standard;
    std::vector<std::array<float,2>> uv;
    float floorContactLift=0;
    std::optional<SpatialPoint> playerShotCenter;
    float strokeRadius=0; // Finite energy edges/points; zero retains ordinary raster primitives.
};
enum class BillboardAlpha {colorZero,opaque};
enum class BillboardDepth {testOnly,write};
struct SpatialBillboard {
    SpatialPoint center;
    float width=0,height=0;
    IndexedBitmap image;
    std::optional<std::array<SpatialPoint,4>> corners;
    BillboardAlpha alpha=BillboardAlpha::colorZero;
    BillboardDepth depth=BillboardDepth::testOnly;
};
struct SpatialLight {SpatialPoint position;Rgb color;float radius=0,strength=0;};
struct SpatialWorld {
    ViewTransform pilotCamera{};
    FixedPosition anchor{};
    std::uint8_t skyColor=0,groundColor=0;
    std::vector<IndexedBitmap> panorama;
    std::array<std::int64_t,2> translation{};
    std::uint64_t tick=0;
    // Zero is inactive; live shots carry their render-boundary age plus one.
    std::array<unsigned,2> playerShotAges{};
};
struct SpatialScene {
    ViewTransform camera{};
    std::vector<std::uint8_t> background;
    // -1 is transparent; other entries address the frame's current palette.
    std::vector<std::int16_t> overlay;
    std::vector<SpatialSurface> surfaces;
    std::vector<SpatialBillboard> billboards;
    std::optional<SpatialWorld> world;
    std::vector<SpatialLight> lights;
};
using SpatialMatrix=std::array<float,16>;
SpatialMatrix spatialIdentity();
SpatialMatrix spatialProjection(const ViewTransform& camera,double displayAspect,SpatialPoint eye={});
struct SpatialVertex {float x,y,z,r,g,b,a,u,v;float nx=0,ny=0,nz=0;};
struct SpatialGeometry {
    std::vector<SpatialVertex> triangles,lines,points;
    std::vector<SpatialVertex> translucentTriangles,translucentLines,translucentPoints;
    std::vector<SpatialVertex> groundTriangles;
    std::vector<SpatialVertex> plainGroundTriangles;
};
SpatialGeometry spatialGeometry(const SpatialScene& scene,const std::vector<Rgb>& palette);
} // namespace stellar7
