#pragma once
#include "stellar7/raster_line_work.hpp"

namespace stellar7 {
// Native Mode-X23ee body through RETF; caller CALLF excluded. Exact main
// viewport24..294/24..145 disables clipping; radar112..206/146..179 enables it.
// Radar outside points return source rejection work without a resident call.
RasterLineWork rasterPointOperations(ProjectedPoint point, Viewport viewport);
}
