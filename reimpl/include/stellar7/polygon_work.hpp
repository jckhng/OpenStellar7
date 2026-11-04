#pragma once
#include "stellar7/render_work.hpp"
namespace stellar7 {
struct PolygonWork {
    RasterSolidPolygon raster;
    std::vector<NormalWorkOperation> operations;
};
// Full caller CALLF through3467 RETF; native scale, equal colors, noncollapsed clipping.
PolygonWork solidPolygonOperations(const std::vector<ProjectedPoint>& points,
    Viewport viewport, PolygonClipMode clipping);
}
