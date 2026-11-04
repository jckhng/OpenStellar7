#include "stellar7/raster_line_work.hpp"
#include "stellar7/failure_model_work.hpp"
#include "stellar7/polygon_work.hpp"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace stellar7 {
namespace {
using Kind = NormalWorkKind;
using Operations = std::vector<NormalWorkOperation>;
constexpr Viewport qualifiedViewport{24, 294, 24, 145};
constexpr std::size_t polygonCapacity = 20;

void instructions(Operations& operations, const std::uint64_t count) {
    operations.push_back({Kind::instructions, count});
}
//349B..350E follows the two coordinate copies. Its nine dispatches can
//span several instruction blocks in the collapsed owner; never cross a REP/I/O.
void removeUniformPolygonPrefix(Operations& operations) {
    constexpr std::uint64_t sourcePrefixDispatches = 9;
    auto remaining = sourcePrefixDispatches;
    std::size_t consumed = 0;
    while (remaining != 0) {
        if (consumed == operations.size() || operations[consumed].kind != Kind::instructions) {
            throw std::logic_error("uniform polygon prefix leaves its source instruction interval");
        }
        auto& operation = operations[consumed];
        const auto count = std::min(remaining, operation.count);
        operation.count -= count;
        remaining -= count;
        if (operation.count == 0) { ++consumed; }
    }
    operations.erase(operations.begin(), operations.begin() + consumed);
}

void string(Operations& operations) { operations.push_back({Kind::string, 1}); }
void port8(Operations& operations) { operations.push_back({Kind::port8Write, 1}); }
void port16(Operations& operations) { operations.push_back({Kind::port16Write, 1}); }
void repeat(Operations& operations, const std::uint64_t count) { operations.push_back({Kind::repeatString, count}); }
bool inside(const ProjectedPoint point, const Viewport viewport) {
    return point.x >= viewport.left && point.x <= viewport.right && point.y >= viewport.top && point.y <= viewport.bottom;
}
void spanOperations(Operations& operations, const int left, const int right) {
    instructions(operations, 9); //0205..0217
    if (((left ^ right) & ~3) == 0) {
        instructions(operations, 14); port16(operations); instructions(operations, 1);
        string(operations); instructions(operations, 1);
        return;
    }
    instructions(operations, 11); port16(operations); instructions(operations, 2);
    string(operations); instructions(operations, 1); port8(operations); instructions(operations, 6);
    const auto bulkPixels = right - left + 1 - 4 + (left & 3);
    if ((bulkPixels >> 2) & 1) { string(operations); }
    repeat(operations, bulkPixels >> 3);
    instructions(operations, 2); port8(operations); instructions(operations, 1);
    string(operations); instructions(operations, 1);
}
void pixelOperations(Operations& operations) {
    instructions(operations, 7); port8(operations); instructions(operations, 4); string(operations);
}
}

static RasterLineWork lineOperations(ProjectedPoint first, ProjectedPoint second,
    const PolygonClipMode clipping) {
    RasterLineWork result{};
    auto& operations = result.operations;
    instructions(operations, 14 + (clipping == PolygonClipMode::Enabled ? 20 : 1));
    instructions(operations, 4);
    if (first.x > second.x) {
        std::swap(first, second);
        instructions(operations, 2);
    }
    instructions(operations, 1); //2298 resident CALLF
    instructions(operations, 18); //0283..02af
    const auto deltaX = int{second.x} - first.x;
    const auto signedY = int{second.y} - first.y;
    const auto deltaY = std::abs(signedY);
    const auto yStep = signedY < 0 ? -1 : 1;
    if (deltaX == 0 && deltaY == 0) {
        instructions(operations, 8); port16(operations); instructions(operations, 2);
        string(operations); instructions(operations, 3);
        result.pixels.push_back(first);
    } else if (deltaY == 0) {
        instructions(operations, 11);
        spanOperations(operations, first.x, second.x);
        instructions(operations, 3);
        for (auto x = int{first.x}; x <= second.x; ++x) {
            result.pixels.push_back({static_cast<std::int16_t>(x), first.y});
        }
    } else if (deltaX == 0) {
        instructions(operations, 9); port16(operations); instructions(operations, 4);
        if (signedY < 0) { instructions(operations, 4); }
        instructions(operations, 3);
        for (auto y = std::min(first.y, second.y); y <= std::max(first.y, second.y); ++y) {
            instructions(operations, 1); string(operations); instructions(operations, 2);
            result.pixels.push_back({first.x, y});
        }
        instructions(operations, 3);
    } else {
        instructions(operations, 3 + 4 + (signedY < 0 ? 2 : 0)); //02b6..0332
        if (deltaX == deltaY) {
            instructions(operations, 3 + 4); port8(operations); instructions(operations, 1);
            for (auto step = 0; step <= deltaX; ++step) {
                pixelOperations(operations); instructions(operations, 6);
                result.pixels.push_back({static_cast<std::int16_t>(first.x + step),
                    static_cast<std::int16_t>(first.y + step * yStep)});
            }
            instructions(operations, 3);
        } else if (deltaX > deltaY) {
            instructions(operations, 5 + 3); port8(operations);
            instructions(operations, 18); port8(operations); instructions(operations, 4);
            string(operations); instructions(operations, 4);
            result.pixels.push_back(first);
            auto error = 2 * deltaY - deltaX;
            auto y = int{first.y};
            for (auto step = 1; step <= deltaX; ++step) {
                instructions(operations, 2);
                if (error >= 0) {
                    instructions(operations, 3);
                    y += yStep;
                    error += 2 * deltaY - 2 * deltaX;
                } else {
                    instructions(operations, 1);
                    error += 2 * deltaY;
                }
                pixelOperations(operations); instructions(operations, 5);
                result.pixels.push_back({static_cast<std::int16_t>(first.x + step), static_cast<std::int16_t>(y)});
            }
            instructions(operations, 3);
        } else {
            instructions(operations, 5 + 4);
            if (signedY < 0) { instructions(operations, 7); }
            instructions(operations, 16); port8(operations); instructions(operations, 4);
            string(operations); instructions(operations, 4);
            auto current = signedY < 0 ? second : first;
            const auto xStep = signedY < 0 ? -1 : 1;
            result.pixels.push_back(current);
            auto error = 2 * deltaX - deltaY;
            for (auto step = 1; step <= deltaY; ++step) {
                instructions(operations, 3);
                ++current.y;
                if (error >= 0) {
                    instructions(operations, 3);
                    current.x = static_cast<std::int16_t>(current.x + xStep);
                    error += 2 * deltaX - 2 * deltaY;
                } else {
                    instructions(operations, 1);
                    error += 2 * deltaX;
                }
                pixelOperations(operations); instructions(operations, 5);
                result.pixels.push_back(current);
            }
            instructions(operations, 2);
        }
    }
    instructions(operations, 5); //229c..22a1
    return result;
}

RasterLineWork rasterLineOperations(ProjectedPoint first, ProjectedPoint second,
    const Viewport viewport, const PolygonClipMode clipping) {
    if (!(viewport == qualifiedViewport) || !inside(first, viewport) || !inside(second, viewport)) {
        throw std::invalid_argument("unsupported clipped raster line");
    }
    return lineOperations(first, second, clipping);
}

static RasterLineWork failureLineOperations(ProjectedPoint first, ProjectedPoint second,
    const Viewport viewport, const PolygonClipMode clipping) {
    if (clipping == PolygonClipMode::Disabled) {
        if (!inside(first, viewport) || !inside(second, viewport)) {
            throw std::invalid_argument("unclipped failure line leaves viewport");
        }
        return lineOperations(first, second, clipping);
    }
    std::uint64_t clipDispatches = 0;
    bool rejected = false;
    enum class Boundary { top, left, bottom, right };
    for (const auto boundary : {Boundary::top, Boundary::left, Boundary::bottom, Boundary::right}) {
        const auto horizontal = boundary == Boundary::left || boundary == Boundary::right;
        const auto lower = boundary == Boundary::top || boundary == Boundary::left;
        const auto limit = boundary == Boundary::top ? viewport.top : boundary == Boundary::left ? viewport.left
            : boundary == Boundary::bottom ? viewport.bottom : viewport.right;
        const auto coordinate = [horizontal](const ProjectedPoint point) { return horizontal ? point.x : point.y; };
        const auto outside = [lower, limit, coordinate](const ProjectedPoint point) {
            return lower ? coordinate(point) < limit : static_cast<std::uint16_t>(coordinate(point)) > limit;
        };
        clipDispatches += 5;
        const auto firstOutside = outside(first);
        if (!firstOutside && !outside(second)) {
            continue;
        }
        if (firstOutside && outside(second)) {
            ++clipDispatches; //2218 jump to the shared return.
            rejected = true;
            break;
        }
        if (!firstOutside) {
            std::swap(first, second);
            clipDispatches += 3;
        }
        const auto denominator = int{coordinate(second)} - coordinate(first);
        const auto distance = int{limit} - coordinate(first);
        const auto otherFirst = horizontal ? first.y : first.x;
        const auto otherSecond = horizontal ? second.y : second.x;
        const auto intersection = otherFirst + (int{otherSecond} - otherFirst) * distance / denominator;
        if (horizontal) {
            first = {limit, static_cast<std::int16_t>(intersection)};
        } else {
            first = {static_cast<std::int16_t>(intersection), limit};
        }
        clipDispatches += 11; //21EB/221B/2248/2275 signed multiply/divide intersection.
    }
    if (rejected) {
        return {{}, {{Kind::instructions, 14 + clipDispatches + 5}}};
    }
    auto result = lineOperations(first, second, clipping);
    result.operations.front().count = 14 + clipDispatches;
    return result;
}

RasterLineWork level1CompletionBeamLineOperations(const ProjectedPoint first,const ProjectedPoint second,
    const Viewport viewport,const PolygonClipMode clipping) {
    if (!(viewport==qualifiedViewport) || first.x!=second.x || first.x<19 || first.x>298
        || first.y<24 || first.y>192 || second.y<24 || second.y>192
        || (clipping!=PolygonClipMode::Enabled && clipping!=PolygonClipMode::Disabled)) {
        throw std::invalid_argument("unsupported completion beam line");
    }
    return clipping==PolygonClipMode::Disabled ? lineOperations(first,second,clipping)
        : failureLineOperations(first,second,viewport,clipping);
}

RasterLineWork level1CombatLinePolygonOperations(const std::array<ProjectedPoint, 2> points,
    const Viewport viewport) {
    // Keep endpoint differences within the source signed-word IDIV domain.
    constexpr int maximumCoordinate = 16383;
    if (!(viewport == qualifiedViewport) || !std::all_of(points.begin(), points.end(), [maximumCoordinate](const auto point) {
        return std::abs(int{point.x}) <= maximumCoordinate && std::abs(int{point.y}) <= maximumCoordinate;
    })) {
        throw std::invalid_argument("unsupported combat line polygon");
    }
    auto first = points[1];
    auto second = points[0];
    std::uint64_t clipDispatches = 0;
    bool rejected = false;
    enum class Boundary { top, left, bottom, right };
    for (const auto boundary : {Boundary::top, Boundary::left, Boundary::bottom, Boundary::right}) {
        const auto horizontal = boundary == Boundary::left || boundary == Boundary::right;
        const auto lower = boundary == Boundary::top || boundary == Boundary::left;
        const auto limit = boundary == Boundary::top ? viewport.top : boundary == Boundary::left ? viewport.left
            : boundary == Boundary::bottom ? viewport.bottom : viewport.right;
        const auto coordinate = [horizontal](const ProjectedPoint point) { return horizontal ? point.x : point.y; };
        const auto outside = [lower, limit, coordinate](const ProjectedPoint point) {
            return lower ? coordinate(point) < limit : static_cast<std::uint16_t>(coordinate(point)) > limit;
        };
        clipDispatches += 5;
        const auto firstOutside = outside(first);
        if (!firstOutside && !outside(second)) {
            continue;
        }
        if (firstOutside && outside(second)) {
            ++clipDispatches; //2218 jump to the shared return.
            rejected = true;
            break;
        }
        if (!firstOutside) {
            std::swap(first, second);
            clipDispatches += 3;
        }
        const auto denominator = int{coordinate(second)} - coordinate(first);
        const auto distance = int{limit} - coordinate(first);
        const auto otherFirst = horizontal ? first.y : first.x;
        const auto otherSecond = horizontal ? second.y : second.x;
        const auto intersection = otherFirst + (int{otherSecond} - otherFirst) * distance / denominator;
        if (horizontal) {
            first = {limit, static_cast<std::int16_t>(intersection)};
        } else {
            first = {static_cast<std::int16_t>(intersection), limit};
        }
        clipDispatches += 11; //21EB/221B/2248/2275 signed multiply/divide intersection.
    }
    RasterLineWork result;
    auto& operations = result.operations;
    instructions(operations, 19); repeat(operations, 2);
    instructions(operations, 3); repeat(operations, 2);
    instructions(operations, 11); string(operations); instructions(operations, 5);
    if (rejected) {
        instructions(operations, 14 + clipDispatches + 5);
    } else {
        auto line = lineOperations(first, second, PolygonClipMode::Enabled);
        line.operations.front().count = 14 + clipDispatches;
        operations.insert(operations.end(), line.operations.begin(), line.operations.end());
        result.pixels = std::move(line.pixels);
    }
    instructions(operations, 13); //38C5..38CB,34E0 and388F..38AD.
    return result;
}

RasterLineWork rasterArmorColumnOperations(const std::int16_t x) {
    constexpr std::int16_t firstColumn = 112;
    constexpr std::int16_t lastColumn = 196;
    if (x < firstColumn || x > lastColumn || (x & 1) != 0) {
        throw std::invalid_argument("unsupported armor column");
    }
    return lineOperations({x, 189}, {x, 195}, PolygonClipMode::Disabled);
}

RasterLineWork rasterCombatArmorColumnOperations(const std::int16_t x) {
    constexpr std::int16_t firstColumn = 72;
    constexpr std::int16_t lastColumn = 196;
    if (x < firstColumn || x > lastColumn || (x & 1) != 0) {
        throw std::invalid_argument("unsupported combat armor column");
    }
    return lineOperations({x, 189}, {x, 195}, PolygonClipMode::Disabled);
}

RasterLineWork rasterLevel2LifecycleArmorColumnOperations(const std::int16_t x) {
    constexpr std::int16_t firstColumn = 0;
    constexpr std::int16_t lastColumn = 196;
    if (x < firstColumn || x > lastColumn || (x & 1) != 0) {
        throw std::invalid_argument("unsupported Level2 lifecycle armor column");
    }
    return lineOperations({x, 189}, {x, 195}, PolygonClipMode::Disabled);
}

RasterLineWork collapsedPolygonOperations(const std::vector<ProjectedPoint>& points,
    const Viewport viewport, const PolygonClipMode clipping) {
    if (!(viewport == qualifiedViewport) || points.empty() || points.size() > polygonCapacity
        || (clipping == PolygonClipMode::Disabled && !std::all_of(points.begin(), points.end(),
            [viewport](const auto point) { return inside(point, viewport); }))
        || (clipping == PolygonClipMode::Enabled && points.size() < 3)) {
        throw std::invalid_argument("unsupported collapsed polygon profile");
    }
    const auto clip = clipPolygon(points, viewport, clipping);
    const auto horizontalEmpty = clipping == PolygonClipMode::Enabled && clip.points.empty()
        && (std::all_of(points.begin(), points.end(),
                [viewport](const auto point) { return point.x < viewport.left; })
            || std::all_of(points.begin(), points.end(),
                [viewport](const auto point) { return point.x > viewport.right; }));
    if (clip.work.stringIterations != 0 || (clip.points.empty() && !horizontalEmpty)
        || !std::all_of(clip.points.begin(), clip.points.end(),
            [viewport](const auto point) { return inside(point, viewport); })) {
        throw std::invalid_argument("unsupported collapsed polygon clipping path");
    }
    RasterLineWork result{};
    auto& operations = result.operations;
    instructions(operations, 1 + 18); repeat(operations, points.size());
    instructions(operations, 3); repeat(operations, points.size());
    if (horizontalEmpty) {
        constexpr std::uint64_t uniformPostCopyDispatches = 9;
        constexpr std::uint64_t horizontalCopyTailDispatches = 6;
        constexpr std::uint64_t shortClipReturnDispatches = 14;
        instructions(operations, uniformPostCopyDispatches + clip.work.dispatches - horizontalCopyTailDispatches);
        repeat(operations, 0); //5b03/5b0d dispatch even when clipping produces no points.
        instructions(operations, 3);
        repeat(operations, 0);
        instructions(operations, shortClipReturnDispatches);
        return result;
    }
    instructions(operations, 1);
    if (points.size() < 2) {
        instructions(operations, 4 + 8);
        return result;
    }
    if (points.size() == 2) {
        instructions(operations, 7 + 3); string(operations); instructions(operations, 5);
        auto line = rasterLineOperations(points[1], points[0], viewport, clipping);
        operations.insert(operations.end(), line.operations.begin(), line.operations.end());
        result.pixels = std::move(line.pixels);
        instructions(operations, 4 + 1 + 8);
        return result;
    }
    instructions(operations, 12 + clip.work.dispatches + 15); //uniform/clip guards + cleanup setup
    auto last = clip.points.front();
    ProjectedPoint upper{-32767, 32767}, lower{32767, -32767};
    std::size_t unique = 0;
    for (auto current = clip.points.rbegin(); current != clip.points.rend(); ++current) {
        instructions(operations, 3);
        if (current->y == last.y) {
            instructions(operations, 3);
            if (current->x == last.x) {
                instructions(operations, 2);
                continue;
            }
            instructions(operations, 1);
        }
        last = *current;
        ++unique;
        instructions(operations, 4);
        if (current->y <= upper.y) {
            instructions(operations, 1);
            if (current->y == upper.y) { instructions(operations, 2); }
            if (current->y < upper.y || current->x > upper.x) {
                upper = *current;
                instructions(operations, 3);
            }
        }
        instructions(operations, 2);
        if (current->y >= lower.y) {
            instructions(operations, 1);
            if (current->y == lower.y) { instructions(operations, 2); }
            if (current->y > lower.y || current->x <= lower.x) {
                lower = *current;
                instructions(operations, 3);
            }
        }
        instructions(operations, 6);
    }
    instructions(operations, 2);
    if (upper.y != lower.y) {
        instructions(operations, 4);
        if (unique > 2) { throw std::invalid_argument("polygon remains a filled area after cleanup"); }
        if (unique < 2) {
            instructions(operations, 2 + 8);
            return result;
        }
    }
    instructions(operations, 7);
    auto line = rasterLineOperations(lower, upper, viewport, clipping);
    operations.insert(operations.end(), line.operations.begin(), line.operations.end());
    result.pixels = std::move(line.pixels);
    instructions(operations, 2 + 8);
    return result;
}
RasterLineWork ownedCollapsedPolygonOperations(const std::vector<ProjectedPoint>& points,
    const Viewport viewport, const PolygonClipMode clipping) {
    constexpr int maximumCoordinate = 16383;
    if ((!(viewport == qualifiedViewport) && !(viewport == Viewport{0, 318, 0, 199}))
        || points.size() < 3 || points.size() > polygonCapacity
        || (clipping != PolygonClipMode::Disabled && clipping != PolygonClipMode::Enabled)
        || !std::all_of(points.begin(), points.end(), [maximumCoordinate](const auto point) {
            return std::abs(int{point.x}) <= maximumCoordinate && std::abs(int{point.y}) <= maximumCoordinate;
        })
        || (clipping == PolygonClipMode::Disabled && !std::all_of(points.begin(), points.end(),
            [viewport](const auto point) { return inside(point, viewport); }))) {
        throw std::invalid_argument("unsupported owned collapsed polygon profile");
    }
    const auto clip = clipPolygon(points, viewport, clipping);
    const auto horizontalEmpty = clipping == PolygonClipMode::Enabled && clip.points.empty()
        && (std::all_of(points.begin(), points.end(),
                [viewport](const auto point) { return point.x < viewport.left; })
            || std::all_of(points.begin(), points.end(),
                [viewport](const auto point) { return point.x > viewport.right; }));
    if ((clip.work.stringIterations != 0 && clip.points.size() > 1)
        || (clip.points.size() > 1 && !std::all_of(clip.points.begin(), clip.points.end(),
            [viewport](const auto point) { return inside(point, viewport); }))) {
        throw std::invalid_argument("unsupported collapsed polygon clipping path");
    }
    RasterLineWork result{};
    auto& operations = result.operations;
    instructions(operations, 1 + 18); repeat(operations, points.size());
    instructions(operations, 3); repeat(operations, points.size());
    if (clip.points.size() <= 1 && !horizontalEmpty && clip.work.stringIterations == 0) {
        instructions(operations, 9 + clip.work.dispatches + 13);
        return result;
    }
    if (clip.points.size() <= 1) {
        constexpr std::uint64_t uniformPostCopyDispatches = 9;
        constexpr std::uint64_t horizontalCopyTailDispatches = 6;
        constexpr std::uint64_t shortClipReturnDispatches = 14;
        instructions(operations, uniformPostCopyDispatches + clip.work.dispatches - horizontalCopyTailDispatches);
        repeat(operations, clip.points.size()); //5B03, including an empty transfer.
        instructions(operations, 3);
        repeat(operations, clip.points.size());
        instructions(operations, shortClipReturnDispatches);
        return result;
    }
    instructions(operations, 1);
    if (clip.points.size() == 2) {
        instructions(operations, 8 + clip.work.dispatches + 10);
        string(operations);
        instructions(operations, 5);
        auto line = lineOperations(clip.points[1], clip.points[0], clipping);
        operations.insert(operations.end(), line.operations.begin(), line.operations.end());
        result.pixels = std::move(line.pixels);
        instructions(operations, 13);
        return result;
    }
    instructions(operations, 12 + clip.work.dispatches + 15); //uniform/clip guards + cleanup setup
    auto last = clip.points.front();
    ProjectedPoint upper{-32767, 32767}, lower{32767, -32767};
    std::size_t unique = 0;
    for (auto current = clip.points.rbegin(); current != clip.points.rend(); ++current) {
        instructions(operations, 3);
        if (current->y == last.y) {
            instructions(operations, 3);
            if (current->x == last.x) {
                instructions(operations, 2);
                continue;
            }
            instructions(operations, 1);
        }
        last = *current;
        ++unique;
        instructions(operations, 4);
        if (current->y <= upper.y) {
            instructions(operations, 1);
            if (current->y == upper.y) { instructions(operations, 2); }
            if (current->y < upper.y || current->x > upper.x) {
                upper = *current;
                instructions(operations, 3);
            }
        }
        instructions(operations, 2);
        if (current->y >= lower.y) {
            instructions(operations, 1);
            if (current->y == lower.y) { instructions(operations, 2); }
            if (current->y > lower.y || current->x <= lower.x) {
                lower = *current;
                instructions(operations, 3);
            }
        }
        instructions(operations, 6);
    }
    instructions(operations, 2);
    if (upper.y != lower.y) {
        instructions(operations, 4);
        if (unique > 2) { throw std::invalid_argument("polygon remains a filled area after cleanup"); }
        if (unique < 2) {
            instructions(operations, 2 + 8);
            return result;
        }
    }
    instructions(operations, 7);
    auto line = lineOperations(lower, upper, clipping);
    operations.insert(operations.end(), line.operations.begin(), line.operations.end());
    result.pixels = std::move(line.pixels);
    instructions(operations, 2 + 8);
    return result;
}


static FailurePolygonWork polygonOperations(const std::vector<ProjectedPoint>& points,
    const Viewport viewport, const PolygonClipMode clipping, const FailurePolygonStyle style,
    const std::uint8_t primary, const std::uint8_t secondary) {
    const auto filled = style == FailurePolygonStyle::filled;
    if ((style != FailurePolygonStyle::filled && style != FailurePolygonStyle::outline)
        || (clipping != PolygonClipMode::Enabled && clipping != PolygonClipMode::Disabled)
        || points.size() > polygonCapacity
        || !std::all_of(points.begin(), points.end(), [](const auto point) {
            return std::abs(int{point.x}) <= 16383 && std::abs(int{point.y}) <= 16383;
        })) {
        throw std::invalid_argument("unsupported failure polygon arithmetic profile");
    }
    FailurePolygonWork result;
    auto& operations = result.operations;
    const auto append = [&](const auto& child) {
        operations.insert(operations.end(), child.begin(), child.end());
    };
    const auto pixels = [&](const RasterLineWork& line, const std::uint8_t color) {
        append(line.operations);
        for (const auto point : line.pixels) {
            result.pixels.push_back({point, color});
        }
    };
    //38AE: one38B7 LODSW between the endpoint pushes;21B4 reverses C arguments.
    const auto outline = [&](const std::vector<ProjectedPoint>& source) {
        instructions(operations, 2); //38AE..38B3.
        for (std::size_t index = 0; index < source.size(); ++index) {
            instructions(operations, 1);
            string(operations);
            instructions(operations, 5);
            pixels(failureLineOperations(source[(index + 1) % source.size()], source[index],
                viewport, clipping), primary);
            instructions(operations, 3); //38C5..38C9.
        }
        instructions(operations, 1); //38CB RET.
    };
    if (points.size() < 2) {
        instructions(operations, 19); repeat(operations, points.size());
        instructions(operations, 3); repeat(operations, points.size());
        instructions(operations, 13); //34CD/34D0/34D2/34E0,388F..38AD.
        return result;
    }
    if (points.size() == 2) {
        instructions(operations, 19); repeat(operations, 2);
        instructions(operations, 3); repeat(operations, 2);
        instructions(operations, 11); string(operations); instructions(operations, 5);
        pixels(failureLineOperations(points[1], points[0], viewport, clipping), primary);
        instructions(operations, 13);
        return result;
    }
    if (!filled) {
        instructions(operations, 19); repeat(operations, points.size());
        instructions(operations, 3); repeat(operations, points.size());
        instructions(operations, 17); //349B/34CD..3501 through CALL38AE.
        outline(points);
        instructions(operations, 9); //3504 and388F..38AD.
        return result;
    }
    const auto clipped = clipPolygon(points, viewport, clipping);
    auto last = clipped.points.empty() ? ProjectedPoint{} : clipped.points.front();
    std::size_t unique = 0;
    for (auto current = clipped.points.rbegin(); current != clipped.points.rend(); ++current) {
        if (current->x != last.x || current->y != last.y) {
            ++unique;
            last = *current;
        }
    }
    const auto collapsed = clipped.points.size() < 3 || unique <= 2
        || std::all_of(clipped.points.begin(), clipped.points.end(),
            [last](const auto point) { return point.y == last.y; });
    if (collapsed) {
        auto raster = ownedCollapsedPolygonOperations(points, viewport, clipping);
        if (primary != secondary) {
            operations.assign(raster.operations.begin(), raster.operations.begin() + 4);
            instructions(operations, 15); //Through350E;3510..351C.
            string(operations); string(operations); instructions(operations, 1); repeat(operations, points.size() - 1);
            string(operations); instructions(operations, 2);
            string(operations); string(operations); instructions(operations, 1); repeat(operations, points.size() - 1);
            string(operations);
            raster.operations.erase(raster.operations.begin(), raster.operations.begin() + 4);
            removeUniformPolygonPrefix(raster.operations);
        }
        pixels(raster, primary);
        return result;
    }
    const auto solid = solidPolygonOperations(points, viewport, clipping);
    operations = solid.operations;
    for (std::size_t row = 0; row < solid.raster.polygon.rows.size(); ++row) {
        const auto span = solid.raster.polygon.rows[row];
        const auto y = static_cast<std::int16_t>(solid.raster.polygon.firstY + row);
        for (int x = span.left; x <= span.right; ++x) {
            result.pixels.push_back({{static_cast<std::int16_t>(x), y}, secondary});
        }
    }
    if (primary == secondary) {
        return result;
    }
    //Split the uniform source program immediately after350E and before3533.
    auto tail = std::vector<NormalWorkOperation>{operations.begin() + 4, operations.end()};
    operations.resize(4);
    instructions(operations, 9 + 6); //common copy tail through350E;3510..351C.
    string(operations); string(operations); instructions(operations, 1); repeat(operations, points.size() - 1);
    string(operations); instructions(operations, 2);
    string(operations); string(operations); instructions(operations, 1); repeat(operations, points.size() - 1);
    string(operations);
    tail.front().count -= 9;
    tail.back().count -= 8; //Retain3878..387F, remove shared far return until after outline.
    append(tail);
    instructions(operations, 5); //3881..388C, includes CALL38AE.
    outline(points);
    instructions(operations, 8); //388F..38AD.
    return result;
}

FailurePolygonWork failurePolygonOperations(const std::vector<ProjectedPoint>& points,
    const Viewport viewport, const PolygonClipMode clipping, const FailurePolygonStyle style,
    const std::uint8_t primary, const std::uint8_t secondary) {
    if (!(viewport == Viewport{0, 318, 0, 199})) {
        throw std::invalid_argument("unsupported failure polygon arithmetic profile");
    }
    return polygonOperations(points, viewport, clipping, style, primary, secondary);
}

FailurePolygonWork ownedPolygonOperations(const std::vector<ProjectedPoint>& points,
    const Viewport viewport, const PolygonClipMode clipping, const FailurePolygonStyle style,
    const std::uint8_t primary, const std::uint8_t secondary) {
    if (!(viewport == qualifiedViewport) && !(viewport == Viewport{0, 318, 0, 199})) {
        throw std::invalid_argument("unsupported owned polygon viewport");
    }
    return polygonOperations(points, viewport, clipping, style, primary, secondary);
}
}
