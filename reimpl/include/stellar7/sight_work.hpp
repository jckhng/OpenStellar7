#pragma once
#include "stellar7/raster_line_work.hpp"
namespace stellar7 {
struct SightPixelWrite {
    ProjectedPoint point;
    std::uint8_t color;
};
struct SightWork {
    bool alternatePhase;
    std::uint8_t color;
    std::vector<NormalWorkOperation> operations;
    std::vector<SightPixelWrite> pixels;
};
struct SightTargetInput {
    std::int16_t type;
    std::uint16_t targetHeading;
};
struct SightRefreshInput {
    std::vector<SightTargetInput> targets;
    std::uint16_t playerYaw;
};
// Caller850c ADD/CALL through674e return, stopping before cockpit8512 CALL.
// Nonrefresh ticks only; retained phase, color and inside-viewport Mode-X profile.
SightWork sightOperations(std::uint16_t loopTick, bool alternatePhase,
    Viewport viewport, std::uint8_t color);
// Refresh admits source12ce0 cached headings in retained active-list order.
SightWork sightOperations(std::uint16_t loopTick, bool alternatePhase,
    Viewport viewport, std::uint8_t color, const SightRefreshInput& refresh);
}
