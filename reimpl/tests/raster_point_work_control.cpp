#include "stellar7/raster_point_work.hpp"
#include <cassert>
#include <stdexcept>

int main() {
    using namespace stellar7;
    constexpr Viewport viewport{24, 294, 24, 145};
    const auto point = rasterPointOperations({159, 85}, viewport);
    assert(point.pixels.size() == 1 && point.pixels.front().x == 159 && point.pixels.front().y == 85);
    assert(point.operations.size() == 3);
    assert(point.operations[0].kind == NormalWorkKind::instructions && point.operations[0].count == 18);
    assert(point.operations[1].kind == NormalWorkKind::port16Write && point.operations[1].count == 1);
    assert(point.operations[2].kind == NormalWorkKind::instructions && point.operations[2].count == 11);
    bool rejected = false;
    try {
        rasterPointOperations({23, 85}, viewport);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    rejected = false;
    try {
        rasterPointOperations({159, 85}, {24, 295, 24, 145});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    constexpr Viewport radarViewport{112, 206, 146, 179};
    const auto radar = rasterPointOperations({112, 146}, radarViewport);
    assert(radar.pixels == std::vector<ProjectedPoint>({{112, 146}}));
    assert(radar.operations.size() == 3);
    assert(radar.operations[0].kind == NormalWorkKind::instructions && radar.operations[0].count == 28);
    assert(radar.operations[1].kind == NormalWorkKind::port16Write && radar.operations[1].count == 1);
    assert(radar.operations[2].kind == NormalWorkKind::instructions && radar.operations[2].count == 11);
    const std::array<ProjectedPoint, 4> outside{{{111, 145}, {207, 145}, {112, 145}, {112, 180}}};
    const std::array<std::uint64_t, 4> rejectDispatches{{10, 12, 15, 17}};
    for (std::size_t index = 0; index < outside.size(); ++index) {
        const auto clipped = rasterPointOperations(outside[index], radarViewport);
        assert(clipped.pixels.empty());
        assert(clipped.operations.size() == 1);
        assert(clipped.operations.front().kind == NormalWorkKind::instructions);
        assert(clipped.operations.front().count == rejectDispatches[index]);
    }
}
