#include "stellar7/polygon_work.hpp"
#include <algorithm>
#include <stdexcept>
namespace stellar7 {
namespace {
using Operations = std::vector<NormalWorkOperation>;
using Kind = NormalWorkKind;
constexpr std::uint16_t planarGroupMask = 0x0ffc;
constexpr std::uint16_t pixelsPerPlanarByte = 4;
constexpr std::uint64_t chainFixedDispatches = 80;
constexpr std::uint64_t chainEdgeDispatches = 24;
void ins(Operations& operations, std::uint64_t count) {
    operations.push_back({Kind::instructions, count});
}
void edgeOperations(Operations& operations, ProjectedPoint first, ProjectedPoint second, PolygonEdgeSide side) {
    const auto edge = rasterEdge(first, second, side);
    if (first.y == second.y) {
        ins(operations, edge.work.dispatches - 2);
        operations.push_back({Kind::string, 1});
        ins(operations, 1);
        return;
    }
    const auto rows = edge.samples.size();
    ins(operations, edge.work.dispatches - 4 * rows - 2);
    for (std::size_t row = 0; row < rows; ++row) {
        operations.push_back({Kind::string, 1});
        ins(operations, 3);
    }
    ins(operations, 2);
}
void spans(Operations& operations, const std::vector<RasterSpan>& rows) {
    ins(operations, 3);
    operations.push_back({Kind::string, 1});
    ins(operations, 2);
    operations.push_back({Kind::string, 1});
    ins(operations, 13);
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const auto row = rows[index];
        operations.push_back({Kind::string, 1});
        ins(operations, 1);
        operations.push_back({Kind::string, 1});
        ins(operations, 2);
        if (row.right < row.left) {
            ins(operations, 1);
        } else {
            ins(operations, 11);
            const auto left = static_cast<std::uint16_t>(row.left);
            const auto right = static_cast<std::uint16_t>(row.right);
            if (((left ^ right) & planarGroupMask) == 0) {
                ins(operations, 14);
                operations.push_back({Kind::port16Write, 1});
                ins(operations, 1);
                operations.push_back({Kind::string, 1});
                ins(operations, 1);
            } else {
                const auto bulk = static_cast<std::uint16_t>(right - left + 1 - pixelsPerPlanarByte
                    + (left & (pixelsPerPlanarByte - 1)));
                ins(operations, 11);
                operations.push_back({Kind::port16Write, 1});
                ins(operations, 2);
                operations.push_back({Kind::string, 1});
                ins(operations, 1);
                operations.push_back({Kind::port8Write, 1});
                ins(operations, 6);
                if ((bulk >> 2) & 1) {
                    operations.push_back({Kind::string, 1});
                }
                operations.push_back({Kind::repeatString, static_cast<std::uint64_t>(bulk >> 3)});
                ins(operations, 2);
                operations.push_back({Kind::port8Write, 1});
                ins(operations, 1);
                operations.push_back({Kind::string, 1});
            }
        }
        ins(operations, index + 1 < rows.size() ? 4 : 2);
    }
    ins(operations, 2);
}
}
PolygonWork solidPolygonOperations(const std::vector<ProjectedPoint>& source,
    Viewport viewport, PolygonClipMode clipping) {
    const auto clip = clipPolygon(source, viewport, clipping);
    if (clip.work.stringIterations != 0 || std::any_of(clip.points.begin(), clip.points.end(),
        [viewport](ProjectedPoint point) {
            return point.x < viewport.left || point.x > viewport.right
                || point.y < viewport.top || point.y > viewport.bottom;
        })) {
        throw std::invalid_argument("unsupported ordered solid polygon clipping path");
    }
    PolygonWork result{solidPolygon(source, viewport, PolygonScale::Native, clipping), {}};
    auto& operations = result.operations;
    const auto& polygon = result.raster.polygon;
    const auto& points = polygon.normalizedPoints;
    std::vector<ProjectedPoint> cleaned;
    auto last = clip.points.front();
    for (auto current = clip.points.rbegin(); current != clip.points.rend(); ++current) {
        if (current->x != last.x || current->y != last.y) {
            cleaned.push_back(*current);
            last = *current;
        }
    }
    const auto keep = std::equal(cleaned.begin(), cleaned.end(), points.begin(), points.end(),
        [](ProjectedPoint first, ProjectedPoint second) { return first.x == second.x && first.y == second.y; });
    std::size_t upper = 0;
    std::size_t lower = 0;
    for (std::size_t index = 1; index < cleaned.size(); ++index) {
        const auto point = cleaned[index];
        if (point.y < cleaned[upper].y || (point.y == cleaned[upper].y && point.x > cleaned[upper].x)) {
            upper = index;
        }
        if (point.y > cleaned[lower].y || (point.y == cleaned[lower].y && point.x <= cleaned[lower].x)) {
            lower = index;
        }
    }
    //36bd reverses the selected vertex indices with the points, preserving ties.
    if (!keep) {
        upper = points.size() - 1 - upper;
        lower = points.size() - 1 - lower;
    }
    struct Edge {ProjectedPoint first; ProjectedPoint second; PolygonEdgeSide side;};
    std::vector<Edge> edges;
    std::size_t rightCount = 0;
    const auto chain = [&](std::size_t start, PolygonEdgeSide side) {
        auto current = start;
        do {
            const auto next = (current + 1) % points.size();
            edges.push_back({points[current], points[next], side});
            current = next;
        } while (side == PolygonEdgeSide::Right ? points[current].y < points[lower].y : points[current].y > points[upper].y);
    };
    chain(upper, PolygonEdgeSide::Right);
    rightCount = edges.size();
    chain(lower, PolygonEdgeSide::Left);
    const auto copyDispatches = keep ? 12 : 18 + 8 * points.size();
    // All cleanup/winding blocks before36bd contain no string or port instructions.
    const auto cleanup = polygon.preparationWork.dispatches - polygon.edgeWork.dispatches
        - chainFixedDispatches - chainEdgeDispatches * edges.size() - copyDispatches;
    ins(operations, 19);
    operations.push_back({Kind::repeatString, source.size()});
    ins(operations, 3);
    operations.push_back({Kind::repeatString, source.size()});
    ins(operations, 9 + clip.work.dispatches + 4 + cleanup);
    if (keep) {
        ins(operations, 4);
        operations.push_back({Kind::repeatString, points.size()});
        ins(operations, 4);
        operations.push_back({Kind::repeatString, points.size()});
        ins(operations, 2);
    } else {
        ins(operations, 5);
        for (std::size_t index = 0; index < points.size(); ++index) {
            operations.push_back({Kind::string, 1});
            ins(operations, 3);
        }
        ins(operations, 5);
        for (std::size_t index = 0; index < points.size(); ++index) {
            operations.push_back({Kind::string, 1});
            ins(operations, 3);
        }
        ins(operations, 8);
    }
    ins(operations, 46 + 11 * edges.size());
    for (std::size_t index = 0; index < edges.size(); ++index) {
        if (index == rightCount) {
            ins(operations, 11);
        }
        ins(operations, index == 0 || index == rightCount ? 12 : 13);
        edgeOperations(operations, edges[index].first, edges[index].second, edges[index].side);
    }
    ins(operations, 25 + 1);
    spans(operations, polygon.rows);
    ins(operations, 11);
    return result;
}
}
