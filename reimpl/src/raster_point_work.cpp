#include "stellar7/raster_point_work.hpp"
#include <stdexcept>

namespace stellar7 {
RasterLineWork rasterPointOperations(const ProjectedPoint point, const Viewport viewport) {
    constexpr Viewport mainViewport{24, 294, 24, 145};
    constexpr Viewport radarViewport{112, 206, 146, 179};
    const auto radar = viewport == radarViewport;
    if (!(viewport == mainViewport) && !radar) {
        throw std::invalid_argument("unsupported raster point profile");
    }
    if (radar) {
        //23f8..2414 tests signed X before Y; each reject returns before3692.
        constexpr std::uint64_t leftRejectDispatches = 10;
        constexpr std::uint64_t rightRejectDispatches = 12;
        constexpr std::uint64_t topRejectDispatches = 15;
        constexpr std::uint64_t bottomRejectDispatches = 17;
        std::uint64_t rejected = 0;
        if (point.x < viewport.left) {
            rejected = leftRejectDispatches;
        } else if (point.x > viewport.right) {
            rejected = rightRejectDispatches;
        } else if (point.y < viewport.top) {
            rejected = topRejectDispatches;
        } else if (point.y > viewport.bottom) {
            rejected = bottomRejectDispatches;
        }
        if (rejected != 0) {
            return {{}, {{NormalWorkKind::instructions, rejected}}};
        }
    } else if (point.x < viewport.left || point.x > viewport.right
        || point.y < viewport.top || point.y > viewport.bottom) {
        throw std::invalid_argument("unsupported raster point profile");
    }
    const std::uint64_t wrapperDispatches = radar ? 16 : 6;
    constexpr std::uint64_t driverMaskPrefix = 12;
    constexpr std::uint64_t driverWriteAndReturn = 11;
    return {{point}, {{NormalWorkKind::instructions, wrapperDispatches + driverMaskPrefix},
        {NormalWorkKind::port16Write, 1}, {NormalWorkKind::instructions, driverWriteAndReturn}}};
}
}
