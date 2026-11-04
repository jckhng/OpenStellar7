#include "retained_failure_scene.hpp"
#include "stellar7/render_work.hpp"
#include "stellar7/level1_owned_view_work.hpp"
#include "level1_departure_model.hpp"
#include "stellar7/level1_bonus_render_work.hpp"
#include "stellar7/level1_arrival_render_work.hpp"
#include "stellar7/level1_deployment_render_work.hpp"
#include "stellar7/failure_view_work.hpp"
#include "stellar7/failure_model_work.hpp"
#include "stellar7/ui_fill_work.hpp"
#include "stellar7/level1_banked_render_work.hpp"
#include "stellar7/level1_projectile_render_work.hpp"
#include "stellar7/ground_steering_work.hpp"
#include "stellar7/ui_bitmap_work.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {

std::vector<NormalWorkOperation> bitmapOperations(const IndexedBitmap& bitmap, std::int16_t x,
    PlanarBlitMode mode);

constexpr std::uint64_t identityDispatches = 27;
constexpr std::uint64_t twoAxisWrapperDispatches = 65;
constexpr std::uint64_t threeAxisWrapperDispatches = 83;
constexpr std::uint64_t twoTermDotDispatches = 48;
constexpr std::uint64_t thirdTermDispatches = 25;
constexpr std::uint64_t negateCoordinateDispatches = 4;
constexpr std::uint64_t negateCoefficientDispatches = 2;
constexpr std::uint64_t negateProductDispatches = 6;
constexpr std::uint64_t cachedVertexDispatches = 13;
constexpr std::uint64_t vertexMissWrapperDispatches = 53;
constexpr std::uint64_t shortIdentityDispatches = 13;
constexpr std::uint64_t shortTwoAxisDispatches = 47;
constexpr std::uint64_t shortThreeAxisDispatches = 80;
constexpr std::uint64_t spanHeaderDispatches = 20;
constexpr std::uint64_t spanRowLoadDispatches = 5;
constexpr std::uint64_t spanDrawSetupDispatches = 11;
constexpr std::uint64_t spanSingleGroupDispatches = 18;
constexpr std::uint64_t spanBulkDispatches = 29;
constexpr std::uint64_t spanLoopDispatches = 2;
constexpr std::uint64_t spanNextRowDispatches = 2;
constexpr std::uint64_t spanReturnDispatches = 2;
constexpr std::uint16_t planarGroupMask = 0x0FFC;
constexpr std::uint16_t edgeWriterCapacity = 400;
constexpr std::uint16_t planarScreenWidth = 320;
constexpr std::uint16_t planarScreenHeight = 200;
constexpr std::uint16_t pixelsPerPlanarByte = 4;
constexpr std::uint64_t planarCopyFixedDispatches = 45;
constexpr std::uint64_t planarCopyRowDispatches = 7;
constexpr std::uint64_t opaqueBlitFixedDispatches = 37;
constexpr std::uint64_t opaqueBlitRowDispatches = 5;
constexpr std::uint64_t transparentBlitPrefixDispatches = 51;
constexpr std::uint64_t transparentBlitFixedTailDispatches = 115;
constexpr std::uint64_t transparentBlitRowDispatches = 7;
constexpr std::uint64_t transparentPixelDispatches = 6;
constexpr std::uint64_t transparentStoreDispatches = 2;
constexpr std::uint64_t generalBlitPrefixDispatches = 73;
constexpr std::uint64_t generalBlitFixedTailDispatches = 197;
constexpr std::uint64_t generalBlitRowDispatches = 5;
constexpr std::uint64_t generalPixelDispatches = 5;
constexpr std::uint64_t generalPlaneRolloverDispatches = 4;
constexpr std::uint64_t bitmapHeaderWords = 5;
constexpr std::uint64_t planarPagePortInstructions = 3;
constexpr std::uint64_t transparentBlitPortInstructions = 4;
constexpr std::uint64_t generalBlitPortInstructions = 5;
constexpr std::uint64_t positiveXPrefixDispatches = 3;
constexpr std::uint64_t transparentPlaneRolloverDispatches = 2;
constexpr std::uint64_t cockpitControlBaseDispatches = 108;
constexpr std::int16_t cockpitIndicatorUpperBound = 176;
constexpr std::int16_t cockpitIndicatorLowerBound = 132;
constexpr std::int16_t cockpitIndicatorSpeed = 4;
constexpr std::int16_t cockpitOverlayStage = 70;
constexpr std::int16_t minimumPlayableDetail = 51;
constexpr std::int16_t cockpitIndicatorStage = 50;
constexpr std::int16_t panoramaStage = 20;
constexpr std::int16_t maximumPlayableDetail = 82;
constexpr std::uint64_t edgeWriterSetupDispatches = 19;
constexpr std::uint64_t edgeWriterRowDispatches = 4;
constexpr std::uint64_t edgeWriterReturnDispatches = 1;
constexpr std::uint64_t edgeDeltaXDispatches = 5;
constexpr std::uint64_t edgeDeltaYDispatches = 7;
constexpr std::uint64_t edgeSlopeCompareDispatches = 2;
constexpr std::uint64_t edgeShallowCompareDispatches = 1;
constexpr std::uint64_t edgeColumnCompareDispatches = 2;
constexpr std::uint64_t edgeCallDispatches = 1;
constexpr std::uint64_t edgeLoopJumpDispatches = 1;
constexpr std::uint64_t verticalEdgeSetupDispatches = 11;
constexpr std::uint64_t horizontalEdgeSetupDispatches = 12;
constexpr std::uint64_t diagonalEdgeSetupDispatches = 14;
constexpr std::uint64_t steepEdgeSetupDispatches = 24;
constexpr std::uint64_t leftShallowEdgeSetupDispatches = 25;
constexpr std::uint64_t rightShallowEdgeSetupDispatches = 29;
constexpr unsigned edgeFractionBits = 16;
constexpr std::size_t polygonPointCapacity = 20;
constexpr std::uint64_t polygonInputCommonDispatches = 26;
constexpr std::uint64_t polygonInputCopyDispatches = 7;
constexpr std::uint64_t polygonInputDoubleSetupDispatches = 11;
constexpr std::uint64_t polygonInputDoublePointDispatches = 8;
constexpr std::uint64_t polygonInputOutlineDispatches = 18;
constexpr std::int32_t polygonDoubleScale = 2;
constexpr std::uint64_t polygonPostClipGuardDispatches = 4;
constexpr std::uint64_t polygonSpanCallDispatches = 1;
constexpr std::uint64_t polygonUniformTailDispatches = 11;
constexpr std::uint64_t polygonDoubleRestoreDispatches = 4;
constexpr std::int16_t horizontalSlopeSentinel = 0x7FFF;
constexpr std::uint64_t polygonCleanupSetupDispatches = 15;
constexpr std::uint64_t polygonCleanupFinishDispatches = 13;
constexpr std::uint64_t polygonCoordinateCompareDispatches = 3;
constexpr std::uint64_t polygonExtremumUpdateDispatches = 3;
constexpr std::uint64_t polygonTieCompareDispatches = 2;
constexpr std::uint64_t polygonCleanupTailDispatches = 6;
constexpr std::uint64_t polygonCopyDispatches = 12;
constexpr std::uint64_t polygonReverseSetupDispatches = 18;
constexpr std::uint64_t polygonReversePointDispatches = 8;
constexpr std::uint64_t windingNextDispatches = 10;
constexpr std::uint64_t windingPreviousDispatches = 8;
constexpr std::uint64_t windingHorizontalDispatches = 4;
constexpr std::uint64_t windingSignTestDispatches = 2;
constexpr std::uint64_t windingNegativeReorderDispatches = 4;
constexpr std::uint64_t windingQuotientDispatches = 10;
constexpr std::uint64_t windingFractionDispatches = 8;
//3719..37af assembly plus37b1..37d3 loading,382f..3871 switch/header.
constexpr std::uint64_t polygonChainFixedDispatches = 80;
constexpr std::uint64_t polygonChainEdgeDispatches = 24;
constexpr std::uint64_t clipOptionalDispatches = 3;
constexpr std::uint64_t clipEntryDispatches = 4;
constexpr std::uint64_t clipHorizontalSetupDispatches = 10;
constexpr std::uint64_t clipVerticalSetupDispatches = 11;
constexpr std::uint64_t clipPointReadDispatches = 10;
constexpr std::uint64_t clipPointStoreDispatches = 6;
constexpr std::uint64_t clipCommonSideDispatches = 3;
constexpr std::uint64_t clipSideTestDispatches = 2;
constexpr std::uint64_t clipIntersectionDispatches = 13;
constexpr std::uint64_t clipPointLoopDispatches = 6;
constexpr std::uint64_t clipPassFinishDispatches = 5;
constexpr std::uint64_t clipCollapsedCopyDispatches = 11;
constexpr std::uint8_t clipLowBit = 1;
constexpr std::uint8_t clipHighBit = 2;
constexpr std::int16_t horizonTop = 24;
constexpr std::int16_t horizonBottom = 145;
constexpr std::uint16_t horizonWidth = 272;
constexpr std::uint16_t horizonPageWidth = 256;
constexpr std::uint64_t groundDrawPrefixDispatches = 47;
constexpr std::uint64_t groundDrawSuffixDispatches = 7;

enum class ClipAxis { Horizontal, Vertical };

std::vector<ProjectedPoint> clipAxis(
    const std::vector<ProjectedPoint>& source,
    const std::int16_t low,
    const std::int16_t high,
    const ClipAxis axis,
    SpanWork& work
) {
    work.dispatches += axis == ClipAxis::Horizontal ? clipHorizontalSetupDispatches : clipVerticalSetupDispatches;
    const auto coordinate = [axis](const ProjectedPoint point) { return axis == ClipAxis::Horizontal ? point.x : point.y; };
    const auto outcode = [&](const ProjectedPoint point) {
        std::uint8_t code = 0;
        if (coordinate(point) < low) {
            code |= clipLowBit;
            ++work.dispatches;
        }
        if (coordinate(point) > high) {
            code |= clipHighBit;
            ++work.dispatches;
        }
        return code;
    };
    std::vector<ProjectedPoint> output;
    output.reserve(source.size() + 2);
    const auto append = [&](const ProjectedPoint point) {
        if (output.size() == polygonPointCapacity) {
            throw std::invalid_argument("clipped polygon exceeds original scratch capacity");
        }
        output.push_back(point);
    };
    const auto intersect = [&](const ProjectedPoint outside, const ProjectedPoint other, const std::int16_t boundary) {
        const auto origin = coordinate(outside);
        const auto denominator = static_cast<std::int16_t>(coordinate(other) - origin);
        const auto distance = static_cast<std::int16_t>(boundary - origin);
        const auto outsideOther = axis == ClipAxis::Horizontal ? outside.y : outside.x;
        const auto otherCoordinate = axis == ClipAxis::Horizontal ? other.y : other.x;
        const auto delta = static_cast<std::int16_t>(otherCoordinate - outsideOther);
        if (denominator == 0) {
            throw std::invalid_argument("polygon clipping requires a nonzero signed16 divisor");
        }
        const auto quotient = (std::int32_t{delta} * distance) / denominator;
        if (quotient < std::numeric_limits<std::int16_t>::min() || quotient > std::numeric_limits<std::int16_t>::max()) {
            throw std::invalid_argument("polygon clipping quotient exceeds original IDIV width");
        }
        const auto value = static_cast<std::int16_t>(outsideOther + quotient);
        append(axis == ClipAxis::Horizontal ? ProjectedPoint{boundary, value} : ProjectedPoint{value, boundary});
        work.dispatches += clipIntersectionDispatches;
    };
    const auto emitIntersection = [&](const ProjectedPoint outside, const ProjectedPoint other, const std::uint8_t code) {
        work.dispatches += clipSideTestDispatches;
        if ((code & clipLowBit) != 0) {
            intersect(outside, other, low);
            ++work.dispatches;
            return;
        }
        work.dispatches += clipSideTestDispatches;
        intersect(outside, other, high);
    };
    const auto emitSegment = [&](const ProjectedPoint previous, const ProjectedPoint current,
                                 const std::uint8_t previousCode, const std::uint8_t currentCode) {
        if ((previousCode | currentCode) == 0) {
            append(current);
            work.dispatches += clipPointStoreDispatches;
            return;
        }
        work.dispatches += clipCommonSideDispatches;
        if ((previousCode & currentCode) != 0) {
            ++work.dispatches;
            return;
        }
        work.dispatches += clipSideTestDispatches;
        if (previousCode == 0) {
            emitIntersection(current, previous, currentCode);
            // Low exit5724/595b jumps through tail5758/598f; high falls into it.
            ++work.dispatches;
            return;
        }
        work.dispatches += clipSideTestDispatches;
        if (currentCode != 0) {
            ++work.dispatches;
        }
        emitIntersection(previous, current, previousCode);
        if (currentCode == 0) {
            append(current);
            work.dispatches += clipPointStoreDispatches;
            return;
        }
        emitIntersection(current, previous, currentCode);
    };
    auto previous = source.back();
    auto previousCode = outcode(previous);
    for (std::size_t index = 0; index < source.size(); ++index) {
        const auto current = source[index];
        work.dispatches += clipPointReadDispatches;
        const auto currentCode = outcode(current);
        emitSegment(previous, current, previousCode, currentCode);
        work.dispatches += clipPointLoopDispatches + (index + 1 < source.size() ? 1 : 0);
        previous = current;
        previousCode = currentCode;
    }
    return output;
}

struct PolygonWinding {
    bool keep;
    std::uint64_t dispatches;
};

std::uint32_t quantizedSlope(const int numerator, const int denominator) noexcept {
    const auto whole = static_cast<std::uint32_t>(numerator / denominator);
    const auto fraction = (static_cast<std::uint32_t>(numerator % denominator) << edgeFractionBits) / denominator;
    return (whole << edgeFractionBits) | fraction;
}

PolygonWinding polygonWinding(const std::vector<ProjectedPoint>& points, const std::size_t upper) noexcept {
    const auto origin = points[upper];
    const auto next = points[(upper + 1) % points.size()];
    const auto previous = points[(upper + points.size() - 1) % points.size()];
    int nextX = next.x - origin.x;
    int nextY = next.y - origin.y;
    int previousX = previous.x - origin.x;
    int previousY = previous.y - origin.y;
    auto dispatches = windingNextDispatches + windingPreviousDispatches + (upper == 0 ? 1 : 0);
    if (nextY == 0) {
        dispatches += windingHorizontalDispatches + (nextX < 0 ? 1 : 0);
        nextY = 1;
        nextX = nextX < 0 ? -horizontalSlopeSentinel : horizontalSlopeSentinel;
    }
    const auto previousHorizontalNegative = previousY == 0 && previousX < 0;
    if (previousY == 0) {
        dispatches += windingHorizontalDispatches + (previousX >= 0 ? 1 : 0);
        previousY = 1;
        previousX = previousX < 0 ? -horizontalSlopeSentinel : horizontalSlopeSentinel;
    }
    if (!previousHorizontalNegative) {
        dispatches += windingSignTestDispatches;
    }
    dispatches += windingSignTestDispatches;
    std::uint32_t firstSlope;
    std::uint32_t secondSlope;
    if (previousX < 0) {
        if (nextX >= 0) {
            return {true, dispatches};
        }
        dispatches += windingNegativeReorderDispatches;
        firstSlope = quantizedSlope(-nextX, nextY);
        secondSlope = quantizedSlope(-previousX, previousY);
    } else {
        if (nextX <= 0) {
            return {false, dispatches};
        }
        ++dispatches;
        firstSlope = quantizedSlope(previousX, previousY);
        secondSlope = quantizedSlope(nextX, nextY);
    }
    dispatches += windingQuotientDispatches;
    const auto firstWhole = firstSlope >> edgeFractionBits;
    const auto secondWhole = secondSlope >> edgeFractionBits;
    if (secondWhole > firstWhole) {
        return {true, dispatches};
    }
    ++dispatches;
    if (secondWhole < firstWhole) {
        return {false, dispatches};
    }
    dispatches += windingFractionDispatches;
    return {secondSlope >= firstSlope, dispatches};
}

std::uint64_t productSignDispatches(
    const std::int32_t coordinate,
    const std::int16_t coefficient
) noexcept {
    const auto negativeCoordinate = coordinate < 0;
    const auto negativeCoefficient = coefficient < 0;
    return (negativeCoordinate ? negateCoordinateDispatches : 0)
        + (negativeCoefficient ? negateCoefficientDispatches : 0)
        + (negativeCoordinate != negativeCoefficient ? negateProductDispatches : 0);
}

void appendInstructions(std::vector<NormalWorkOperation>& operations, const std::uint64_t count) {
    operations.push_back({NormalWorkKind::instructions, count});
}

void appendFilledRectangle(std::vector<NormalWorkOperation>& operations, const std::uint16_t width,
    const std::uint16_t height, const std::uint64_t setupDispatches) {
    using Kind = NormalWorkKind;
    constexpr std::uint64_t rowSetupDispatches = 26;
    constexpr std::uint64_t rowTailDispatches = 7;
    constexpr std::uint64_t returnDispatches = 5;
    const auto bulk = static_cast<std::uint64_t>(width) - pixelsPerPlanarByte;
    //2e25 equal-color filled path calls0205 per row, with no border/scale/page mapping.
    appendInstructions(operations, setupDispatches);
    for (std::uint16_t row = 0; row < height; ++row) {
        appendInstructions(operations, rowSetupDispatches);
        operations.push_back({Kind::port16Write, 1});
        appendInstructions(operations, 2);
        operations.push_back({Kind::string, 1});
        appendInstructions(operations, 1);
        operations.push_back({Kind::port8Write, 1});
        appendInstructions(operations, 6);
        operations.push_back({Kind::string, 1});
        operations.push_back({Kind::repeatString, bulk >> 3});
        appendInstructions(operations, 2);
        operations.push_back({Kind::port8Write, 1});
        appendInstructions(operations, 1);
        operations.push_back({Kind::string, 1});
        appendInstructions(operations, rowTailDispatches);
    }
    appendInstructions(operations, returnDispatches);
}

void appendHorizonRectangle(std::vector<NormalWorkOperation>& operations, const std::uint16_t height) {
    constexpr std::uint64_t clippedSetupDispatches = 83;
    appendFilledRectangle(operations, horizonWidth, height, clippedSetupDispatches);
}

enum class HorizonTile { primary, rightSliver, culledLeft };

void appendHorizonBitmap(std::vector<NormalWorkOperation>& operations, const std::uint16_t height,
    const HorizonTile tile) {
    using Kind = NormalWorkKind;
    constexpr std::uint64_t headerSetupDispatches = 13;
    constexpr std::uint64_t visiblePrefixDispatches = 62;
    constexpr std::uint64_t rightClipDispatches = 8;
    constexpr std::uint64_t culledPrefixAndReturnDispatches = 59;
    constexpr std::array<std::uint64_t, 4> planeSetupDispatches{10, 31, 32, 33};
    constexpr std::array<std::uint64_t, 4> sourceSetupDispatches{30, 14, 14, 14};
    constexpr std::uint64_t rowTailDispatches = 4;
    constexpr std::uint64_t returnDispatches = 9;
    constexpr auto sliverWidth = horizonWidth - horizonPageWidth;
    appendInstructions(operations, headerSetupDispatches);
    operations.push_back({Kind::repeatString, bitmapHeaderWords});
    if (tile == HorizonTile::culledLeft) {
        appendInstructions(operations, culledPrefixAndReturnDispatches);
        return;
    }
    appendInstructions(operations, visiblePrefixDispatches + (tile == HorizonTile::rightSliver ? rightClipDispatches : 0));
    operations.push_back({Kind::port8Write, 1});
    const auto rowBytes = static_cast<std::uint64_t>(
        (tile == HorizonTile::primary ? horizonPageWidth : sliverWidth) / pixelsPerPlanarByte);
    //1f18 selects four source-plane row loops208b/2101/21a0/223f; both X positions align.
    for (std::size_t plane = 0; plane < planeSetupDispatches.size(); ++plane) {
        appendInstructions(operations, planeSetupDispatches[plane]);
        operations.push_back({Kind::port8Write, 1});
        appendInstructions(operations, sourceSetupDispatches[plane]);
        for (std::uint16_t row = 0; row < height; ++row) {
            appendInstructions(operations, 1);
            operations.push_back({Kind::repeatString, rowBytes});
            appendInstructions(operations, rowTailDispatches);
        }
    }
    appendInstructions(operations, returnDispatches);
}

void appendYawHorizonBitmap(std::vector<NormalWorkOperation>& operations, const std::uint16_t height,
    const int x, const int left = 24, const int right = 295) {
    using Kind = NormalWorkKind;
    appendInstructions(operations, 13);
    operations.push_back({Kind::repeatString, bitmapHeaderWords});
    appendInstructions(operations, 32); //1F33..1F8A, clipping enabled.
    const auto rightTrim = std::max(0, x + int{horizonPageWidth} - 1 - right);
    if (rightTrim != 0) {
        appendInstructions(operations, 8);
        if (rightTrim >= horizonPageWidth) {
            appendInstructions(operations, 8);
            return;
        }
    }
    appendInstructions(operations, 11); //Bottom/top inside, then left test.
    const auto leftTrim = std::max(0, left - x);
    if (leftTrim != 0) {
        appendInstructions(operations, 8);
        if (leftTrim >= horizonPageWidth) {
            appendInstructions(operations, 8);
            return;
        }
    }
    const auto columns = horizonPageWidth - rightTrim - leftTrim;
    appendInstructions(operations, 19);
    operations.push_back({Kind::port8Write, 1});
    auto sourcePhase = leftTrim & 3;
    auto destinationPhase = (x + leftTrim) & 3;
    for (int plane = 0; plane < pixelsPerPlanarByte; ++plane) {
        const auto rollover = destinationPhase == 4;
        appendInstructions(operations, plane == 0 ? 10 : 30 + sourcePhase + (sourcePhase == 0) + (rollover ? 4 : 0));
        operations.push_back({Kind::port8Write, 1});
        destinationPhase = rollover ? 1 : destinationPhase + 1;
        const auto bytes = static_cast<std::uint64_t>((columns + 3 - plane) / pixelsPerPlanarByte);
        appendInstructions(operations, (plane == 0 ? 28 + sourcePhase : 12) + (bytes != 0 ? 2 : 0));
        if (bytes != 0) {
            for (std::uint16_t row = 0; row < height; ++row) {
                appendInstructions(operations, 1);
                operations.push_back({Kind::repeatString, bytes});
                appendInstructions(operations, 4);
            }
        }
        sourcePhase = (sourcePhase + 1) & 3;
    }
    appendInstructions(operations, 9);
}

}  // namespace

RasterInput preparePolygon(const std::vector<ProjectedPoint>& source, const Viewport viewport,
    const PolygonScale scale, const PolygonColorRelation colors) {
    if (source.size() < 3 || source.size() > polygonPointCapacity) {
        throw std::invalid_argument("filled polygon input count outside original table capacity");
    }
    if (colors == PolygonColorRelation::Different && source.size() == polygonPointCapacity) {
        // The closing X/Y words alias the original adjacent outline/count tables.
        throw std::invalid_argument("polygon outline closure aliases original scratch tables");
    }
    RasterInput input{source, {}, viewport, {polygonInputCommonDispatches, 0, 0}};
    if (scale == PolygonScale::Double) {
        const auto doubled = [](const std::int16_t value) {
            return static_cast<std::int16_t>(std::int32_t{value} * polygonDoubleScale);
        };
        for (auto& point : input.points) {
            point = {doubled(point.x), doubled(point.y)};
        }
        input.viewport = {doubled(viewport.left), doubled(viewport.right), doubled(viewport.top), doubled(viewport.bottom)};
        input.work.dispatches += polygonInputDoubleSetupDispatches + polygonInputDoublePointDispatches * source.size();
        input.work.stringIterations = 4 * source.size();
    } else {
        input.work.dispatches += polygonInputCopyDispatches;
        input.work.stringIterations = 2 * source.size();
    }
    if (colors == PolygonColorRelation::Different) {
        input.outlinePoints = input.points;
        input.outlinePoints.push_back(input.points.front());
        input.work.dispatches += polygonInputOutlineDispatches;
        input.work.stringIterations += 2 * source.size() + 4;
    }
    return input;
}

RasterClip clipPolygon(const std::vector<ProjectedPoint>& source, const Viewport viewport, const PolygonClipMode mode) {
    if (source.size() > polygonPointCapacity || viewport.left > viewport.right || viewport.top > viewport.bottom) {
        throw std::invalid_argument("polygon clipping outside original point/bounds domain");
    }
    RasterClip clip{source, {clipOptionalDispatches, 0, 0}};
    if (mode == PolygonClipMode::Disabled) {
        return clip;
    }
    clip.work.dispatches += 1 + clipEntryDispatches;
    if (source.size() <= 1) {
        clip.work.dispatches += 2;
        return clip;
    }
    auto horizontal = clipAxis(source, viewport.left, viewport.right, ClipAxis::Horizontal, clip.work);
    clip.work.dispatches += clipPassFinishDispatches;
    if (horizontal.size() <= 1) {
        clip.work.dispatches += clipCollapsedCopyDispatches;
        clip.work.stringIterations = 2 * horizontal.size();
        clip.points = std::move(horizontal);
        return clip;
    }
    clip.points = clipAxis(horizontal, viewport.top, viewport.bottom, ClipAxis::Vertical, clip.work);
    clip.work.dispatches += clipPassFinishDispatches;
    return clip;
}

RasterPolygon rasterPolygon(const std::vector<ProjectedPoint>& source) {
    if (source.size() < 3 || source.size() > polygonPointCapacity) {
        throw std::invalid_argument("filled polygon point count outside original table capacity");
    }
    const auto extremaX = std::minmax_element(source.begin(), source.end(), [](const auto first, const auto second) {
        return first.x < second.x;
    });
    if (int{extremaX.second->x} - extremaX.first->x > std::numeric_limits<std::int16_t>::max()
        || std::any_of(source.begin(), source.end(), [](const auto point) { return point.y < 0 || point.y >= edgeWriterCapacity; })) {
        throw std::invalid_argument("filled polygon outside original post-clipping arithmetic domain");
    }

    std::vector<ProjectedPoint> points;
    points.reserve(source.size());
    auto last = source.front();
    std::size_t upper = 0;
    std::size_t lower = 0;
    ProjectedPoint upperPoint{-horizontalSlopeSentinel, horizontalSlopeSentinel};
    ProjectedPoint lowerPoint{horizontalSlopeSentinel, -horizontalSlopeSentinel};
    SpanWork preparation{polygonCleanupSetupDispatches, 0, 0};
    for (auto current = source.rbegin(); current != source.rend(); ++current) {
        preparation.dispatches += polygonCoordinateCompareDispatches;
        if (current->y == last.y) {
            preparation.dispatches += polygonCoordinateCompareDispatches;
            if (current->x == last.x) {
                preparation.dispatches += 2;
                continue;
            }
            ++preparation.dispatches;
        }
        last = *current;
        points.push_back(*current);
        const auto index = points.size() - 1;
        preparation.dispatches += 4;
        if (current->y <= upperPoint.y) {
            ++preparation.dispatches;
            if (current->y == upperPoint.y) {
                preparation.dispatches += polygonTieCompareDispatches;
            }
            if (current->y < upperPoint.y || current->x > upperPoint.x) {
                upper = index;
                upperPoint = *current;
                preparation.dispatches += polygonExtremumUpdateDispatches;
            }
        }
        preparation.dispatches += 2;
        if (current->y >= lowerPoint.y) {
            ++preparation.dispatches;
            if (current->y == lowerPoint.y) {
                preparation.dispatches += polygonTieCompareDispatches;
            }
            if (current->y > lowerPoint.y || current->x <= lowerPoint.x) {
                lower = index;
                lowerPoint = *current;
                preparation.dispatches += polygonExtremumUpdateDispatches;
            }
        }
        preparation.dispatches += polygonCleanupTailDispatches;
    }
    if (points.size() < 3 || points[upper].y == points[lower].y) {
        throw std::invalid_argument("collapsed filled polygon requires the separate original line path");
    }
    preparation.dispatches += polygonCleanupFinishDispatches;
    const auto winding = polygonWinding(points, upper);
    preparation.dispatches += winding.dispatches;
    preparation.stringIterations = 2 * points.size();
    if (winding.keep) {
        preparation.dispatches += polygonCopyDispatches;
    } else {
        preparation.dispatches += polygonReverseSetupDispatches + polygonReversePointDispatches * points.size();
        std::reverse(points.begin(), points.end());
        upper = points.size() - 1 - upper;
        lower = points.size() - 1 - lower;
    }

    const auto firstY = points[upper].y;
    const auto lastY = points[lower].y;
    RasterPolygon polygon{firstY, std::vector<RasterSpan>(lastY - firstY + 1), points, {0, 0, 0}, preparation};
    std::uint64_t edgeCount = 0;
    const auto writeChain = [&](const std::size_t start, const PolygonEdgeSide side) {
        auto current = start;
        do {
            const auto next = (current + 1) % points.size();
            const auto edge = rasterEdge(points[current], points[next], side);
            ++edgeCount;
            polygon.edgeWork.dispatches += edge.work.dispatches;
            polygon.edgeWork.stringIterations += edge.work.stringIterations;
            for (const auto sample : edge.samples) {
                auto& row = polygon.rows[static_cast<std::size_t>(sample.y - firstY)];
                if (side == PolygonEdgeSide::Left) {
                    row.left = sample.x;
                } else {
                    row.right = sample.x;
                }
            }
            current = next;
        } while (side == PolygonEdgeSide::Right ? points[current].y < lastY : points[current].y > firstY);
    };
    writeChain(upper, PolygonEdgeSide::Right);
    writeChain(lower, PolygonEdgeSide::Left);
    polygon.preparationWork.dispatches += polygonChainFixedDispatches + polygonChainEdgeDispatches * edgeCount
        + polygon.edgeWork.dispatches;
    polygon.preparationWork.stringIterations += polygon.edgeWork.stringIterations;
    return polygon;
}

RasterSolidPolygon solidPolygon(const std::vector<ProjectedPoint>& source, const Viewport viewport,
    const PolygonScale scale, const PolygonClipMode clipping) {
    const auto input = preparePolygon(source, viewport, scale, PolygonColorRelation::Same);
    const auto clip = clipPolygon(input.points, input.viewport, clipping);
    auto polygon = rasterPolygon(clip.points);
    const SpanWork prefix{input.work.dispatches + clip.work.dispatches + polygonPostClipGuardDispatches
            + polygon.preparationWork.dispatches,
        input.work.stringIterations + clip.work.stringIterations + polygon.preparationWork.stringIterations, 0};
    auto suffix = solidSpanWork(polygon.rows);
    suffix.dispatches += polygonSpanCallDispatches + polygonUniformTailDispatches;
    auto returnedViewport = input.viewport;
    if (scale == PolygonScale::Double) {
        // Original cleanup uses logical SHR, not signed division.
        const auto restored = [](const std::int16_t value) {
            return static_cast<std::int16_t>(static_cast<std::uint16_t>(value) >> 1);
        };
        returnedViewport = {restored(input.viewport.left), restored(input.viewport.right),
            restored(input.viewport.top), restored(input.viewport.bottom)};
        suffix.dispatches += polygonDoubleRestoreDispatches;
    }
    const SpanWork total{prefix.dispatches + suffix.dispatches, prefix.stringIterations + suffix.stringIterations,
        prefix.portInstructions + suffix.portInstructions};
    return {std::move(polygon), returnedViewport, prefix, suffix, total};
}

RasterEdge rasterEdge(ProjectedPoint first, ProjectedPoint second, const PolygonEdgeSide side) {
    const auto deltaX = std::abs(int{second.x} - first.x);
    const auto deltaY = std::abs(int{second.y} - first.y);
    if (first.y < 0 || second.y < 0 || first.y >= edgeWriterCapacity || second.y >= edgeWriterCapacity
        || deltaX > std::numeric_limits<std::int16_t>::max()) {
        throw std::invalid_argument("polygon edge outside original post-clipping arithmetic domain");
    }

    const auto count = static_cast<std::uint16_t>(deltaY + 1);
    auto work = edgeWriterWork(count);
    work.dispatches += edgeDeltaXDispatches + edgeCallDispatches + edgeLoopJumpDispatches;
    std::uint16_t fraction = 0;
    std::int16_t wholeStep = 0;
    if (deltaX == 0) {
        work.dispatches += verticalEdgeSetupDispatches + (first.y <= second.y ? 1 : 0);
        if (first.y > second.y) {
            std::swap(first, second);
        }
    } else {
        work.dispatches += edgeDeltaYDispatches;
        if (deltaY == 0) {
            // Inline horizontal write: no near CALL, shared writer or RET.
            work = {edgeDeltaXDispatches + edgeDeltaYDispatches + horizontalEdgeSetupDispatches
                        + (first.x >= second.x ? 1 : 0) + (side == PolygonEdgeSide::Right ? 1 : 0), 1, 0};
            const auto x = side == PolygonEdgeSide::Left ? std::min(first.x, second.x) : std::max(first.x, second.x);
            return {{{x, first.y}}, work};
        }
        work.dispatches += edgeSlopeCompareDispatches;
        if (deltaX == deltaY) {
            work.dispatches += edgeShallowCompareDispatches + diagonalEdgeSetupDispatches;
            if (first.y >= second.y) {
                std::swap(first, second);
                work.dispatches += 2;
            }
            wholeStep = first.x < second.x ? 1 : -1;
            work.dispatches += wholeStep < 0 ? 1 : 0;
        } else if (deltaX < deltaY) {
            work.dispatches += steepEdgeSetupDispatches;
            if (first.x >= second.x) {
                std::swap(first, second);
                work.dispatches += 2;
            }
            fraction = static_cast<std::uint16_t>((static_cast<std::uint32_t>(deltaX + 1) << edgeFractionBits) / count);
        } else {
            work.dispatches += edgeShallowCompareDispatches + edgeColumnCompareDispatches;
            const auto left = side == PolygonEdgeSide::Left;
            work.dispatches += left ? leftShallowEdgeSetupDispatches : rightShallowEdgeSetupDispatches;
            if (left ? first.x >= second.x : first.x <= second.x) {
                std::swap(first, second);
                work.dispatches += 2;
            }
            const auto numerator = deltaX + 1;
            const auto quotient = numerator / count;
            fraction = static_cast<std::uint16_t>((static_cast<std::uint32_t>(numerator % count) << edgeFractionBits) / count);
            wholeStep = static_cast<std::int16_t>(quotient);
            if (!left) {
                wholeStep = static_cast<std::int16_t>(-quotient - (fraction == 0 ? 0 : 1));
                fraction = static_cast<std::uint16_t>(0U - fraction);
            }
        }
    }

    RasterEdge edge{{}, work};
    edge.samples.reserve(count);
    auto x = static_cast<std::uint16_t>(first.x);
    auto accumulator = fraction;
    auto y = first.y;
    const auto yStep = first.y <= second.y ? 1 : -1;
    for (std::uint16_t row = 0; row < count; ++row) {
        edge.samples.push_back({static_cast<std::int16_t>(x), y});
        const auto sum = static_cast<std::uint32_t>(accumulator) + fraction;
        accumulator = static_cast<std::uint16_t>(sum);
        x = static_cast<std::uint16_t>(x + wholeStep + (sum >> edgeFractionBits));
        y = static_cast<std::int16_t>(y + yStep);
    }
    return edge;
}

SpanWork edgeWriterWork(const std::uint16_t rowCount) {
    if (rowCount > edgeWriterCapacity) {
        throw std::invalid_argument("edge row count exceeds original unrolled writer capacity");
    }
    return {edgeWriterSetupDispatches + edgeWriterReturnDispatches + edgeWriterRowDispatches * rowCount,
            rowCount, 0};
}

SpanWork planarCopyWork(const std::uint16_t x, const std::uint16_t width, const std::uint16_t height) {
    const auto alignment = x % pixelsPerPlanarByte;
    //106a adds the source alignment twice before dividing the covered width.
    const auto groups = (width + 2 * alignment + pixelsPerPlanarByte - 1) / pixelsPerPlanarByte;
    if (width == 0 || height == 0 || height > planarScreenHeight
        || x >= planarScreenWidth || width > planarScreenWidth - x
        || x / pixelsPerPlanarByte + groups > planarScreenWidth / pixelsPerPlanarByte) {
        throw std::invalid_argument("planar copy outside bounded screen domain");
    }
    return {planarCopyFixedDispatches + planarCopyRowDispatches * height,
            static_cast<std::uint64_t>(groups) * height, planarPagePortInstructions};
}

std::vector<NormalWorkOperation> planarCopyOperations(const std::uint16_t x, const std::uint16_t width,
    const std::uint16_t height) {
    const auto work = planarCopyWork(x, width, height);
    const auto groups = work.stringIterations / height;
    constexpr std::uint64_t prefixDispatches = 31;
    constexpr std::uint64_t portSetupDispatches = 2;
    constexpr std::uint64_t rowSetupDispatches = 3;
    constexpr std::uint64_t rowTailDispatches = 3;
    constexpr std::uint64_t returnDispatches = 6;
    using Kind = NormalWorkKind;
    std::vector<NormalWorkOperation> operations{{Kind::instructions, prefixDispatches},
        {Kind::port16Write, 1}, {Kind::instructions, portSetupDispatches},
        {Kind::port16Write, 1}, {Kind::instructions, 1}};
    for (std::uint16_t row = 0; row < height; ++row) {
        operations.push_back({Kind::instructions, rowSetupDispatches});
        operations.push_back({Kind::repeatString, groups});
        operations.push_back({Kind::instructions, rowTailDispatches});
    }
    operations.push_back({Kind::instructions, portSetupDispatches});
    operations.push_back({Kind::port16Write, 1});
    operations.push_back({Kind::instructions, returnDispatches});
    return operations;
}

SpanWork planarBitmapWork(const IndexedBitmap& bitmap, const std::int16_t x, const PlanarBlitMode mode) {
    if (bitmap.width == 0 || bitmap.height == 0 || bitmap.height > planarScreenHeight
        || x < 0 || x >= planarScreenWidth || bitmap.width > planarScreenWidth - x
        || bitmap.width % pixelsPerPlanarByte != 0
        || bitmap.pixels.size() != static_cast<std::size_t>(bitmap.width) * bitmap.height) {
        throw std::invalid_argument("planar bitmap outside unclipped screen domain");
    }
    if (mode == PlanarBlitMode::opaque) {
        if (x % pixelsPerPlanarByte != 0) {
            throw std::invalid_argument("opaque planar bitmap requires aligned X");
        }
        return {opaqueBlitFixedDispatches + opaqueBlitRowDispatches * bitmap.height,
                static_cast<std::uint64_t>(bitmap.width / pixelsPerPlanarByte) * bitmap.height, planarPagePortInstructions};
    }
    if ((mode != PlanarBlitMode::transparent && mode != PlanarBlitMode::generalTransparent)
        || bitmap.width > std::numeric_limits<std::uint8_t>::max()) {
        throw std::invalid_argument("unsupported transparent planar bitmap domain");
    }
    const auto stored = static_cast<std::uint64_t>(std::count_if(bitmap.pixels.begin(), bitmap.pixels.end(),
        [](const std::uint8_t value) { return value != 0; }));
    if (mode == PlanarBlitMode::generalTransparent) {
        const auto dispatches = generalBlitPrefixDispatches + generalBlitFixedTailDispatches
            + pixelsPerPlanarByte * generalBlitRowDispatches * bitmap.height
            + generalPixelDispatches * bitmap.pixels.size() + transparentStoreDispatches * stored
            + (x % pixelsPerPlanarByte == 0 ? 0 : generalPlaneRolloverDispatches);
        return {dispatches, bitmapHeaderWords + bitmap.pixels.size() + stored, generalBlitPortInstructions};
    }
    auto dispatches = transparentBlitPrefixDispatches + transparentBlitFixedTailDispatches
        + pixelsPerPlanarByte * transparentBlitRowDispatches * bitmap.height
        + transparentPixelDispatches * bitmap.pixels.size() + transparentStoreDispatches * stored;
    if (x == 0) {
        dispatches -= positiveXPrefixDispatches;
    }
    if (x % pixelsPerPlanarByte != 0) {
        dispatches += transparentPlaneRolloverDispatches;
    }
    return {dispatches, bitmapHeaderWords + bitmap.pixels.size() + stored, transparentBlitPortInstructions};
}

std::uint64_t cockpitControlDispatches(const std::int16_t indicatorX) noexcept {
    return cockpitControlBaseDispatches + (indicatorX > cockpitIndicatorUpperBound ? 1 : 0);
}

enum class RenderDetailDomain { qualifiedFull, playable, fullPlayable };

static CockpitWork cockpitOperationsImpl(const CockpitWorkState& state, const CockpitWorkBitmaps& bitmaps,
    const RenderDetailDomain domain) {
    if ((state.pages != CockpitPageRelation::differing && state.pages != CockpitPageRelation::matching)
        || (domain == RenderDetailDomain::qualifiedFull ? state.stage <= cockpitOverlayStage
            : state.stage < (domain == RenderDetailDomain::fullPlayable ? 0 : minimumPlayableDetail)
                || state.stage > maximumPlayableDetail)) {
        throw std::invalid_argument("unsupported cockpit control path");
    }
    CockpitWork result{state, {}, {}};
    if (state.stage > cockpitIndicatorStage) {
        if (state.indicatorX < cockpitIndicatorLowerBound) {
            result.state.indicatorVelocity = cockpitIndicatorSpeed;
        } else if (state.indicatorX > cockpitIndicatorUpperBound) {
            result.state.indicatorVelocity = -cockpitIndicatorSpeed;
        }
        result.state.indicatorX = static_cast<std::int16_t>(state.indicatorX + result.state.indicatorVelocity);
    }
    const auto append = [&result](const std::uint64_t control, const std::vector<NormalWorkOperation>& body) {
        result.operations.push_back({NormalWorkKind::instructions, control});
        result.operations.insert(result.operations.end(), body.begin(), body.end());
    };
    // Control counts include each CALL and one resident JMPF stub, in original order.
    constexpr std::uint64_t copyControl = 17;
    constexpr std::uint64_t matchingBackgroundControl = 13;
    constexpr std::uint64_t matchingCopyControl = 13;
    constexpr std::uint64_t stripControl = 11;
    constexpr std::uint64_t indicatorControl = 16;
    if (state.pages == CockpitPageRelation::matching) {
        append(matchingBackgroundControl,
            planarBitmapOperations(bitmaps.matchingPageBackground, 96, PlanarBlitMode::opaque));
        result.effects.emplace_back(CockpitBitmapPlacement{
            CockpitBitmapRole::matchingPageBackground, 96, 24, PlanarBlitMode::opaque});
    }
    append(state.pages == CockpitPageRelation::matching ? matchingCopyControl : copyControl,
        planarCopyOperations(96, 120, 18));
    result.effects.emplace_back(CockpitCopyRegion{96, 24, 120, 18});
    if (state.stage <= cockpitIndicatorStage) {
        //6D82 cleanup, both detail guards, POP SI and RET; indicator state is retained.
        result.operations.push_back({NormalWorkKind::instructions, 7});
        return result;
    }
    append(stripControl, planarBitmapOperations(bitmaps.strip, 128, PlanarBlitMode::opaque));
    result.effects.emplace_back(CockpitBitmapPlacement{CockpitBitmapRole::strip, 128, 121, PlanarBlitMode::opaque});
    append(indicatorControl + (state.indicatorX > cockpitIndicatorUpperBound ? 1 : 0),
        planarBitmapOperations(bitmaps.indicator, result.state.indicatorX, PlanarBlitMode::generalTransparent));
    result.effects.emplace_back(CockpitBitmapPlacement{CockpitBitmapRole::indicator,
        result.state.indicatorX, 122, PlanarBlitMode::generalTransparent});
    if (state.stage <= cockpitOverlayStage) {
        //6DE0 cleanup,6DE3/6DE8 guard,6E69/6E6A return. The indicator still advances.
        result.operations.push_back({NormalWorkKind::instructions, 5});
        return result;
    }
    constexpr std::array<std::int16_t, 6> overlayX{24, 277, 24, 260, 24, 292};
    constexpr std::array<std::int16_t, 6> overlayY{38, 37, 24, 24, 141, 140};
    constexpr std::array<CockpitBitmapRole, 6> overlayRoles{CockpitBitmapRole::overlay0,
        CockpitBitmapRole::overlay1, CockpitBitmapRole::overlay2, CockpitBitmapRole::overlay3,
        CockpitBitmapRole::overlay4, CockpitBitmapRole::overlay5};
    constexpr std::array<std::uint64_t, 6> overlayControl{14, 10, 9, 10, 10, 8};
    for (std::size_t index = 0; index < bitmaps.overlays.size(); ++index) {
        append(overlayControl[index], planarBitmapOperations(bitmaps.overlays[index], overlayX[index], PlanarBlitMode::transparent));
        result.effects.emplace_back(CockpitBitmapPlacement{overlayRoles[index], overlayX[index],
            overlayY[index], PlanarBlitMode::transparent});
    }
    result.operations.push_back({NormalWorkKind::instructions, 3}); // Cleanup, POP SI, RET.
    return result;
}

std::vector<NormalWorkOperation> fullFrontClearOperations() {
    constexpr std::uint64_t unclippedSetupDispatches = 63;
    std::vector<NormalWorkOperation> operations;
    appendFilledRectangle(operations, planarScreenWidth, planarScreenHeight, unclippedSetupDispatches);
    return operations;
}

CockpitWork cockpitOperations(const CockpitWorkState& state, const CockpitWorkBitmaps& bitmaps) {
    return cockpitOperationsImpl(state, bitmaps, RenderDetailDomain::qualifiedFull);
}

CockpitWork playableCockpitOperations(const CockpitWorkState& state, const CockpitWorkBitmaps& bitmaps) {
    return cockpitOperationsImpl(state, bitmaps, RenderDetailDomain::playable);
}

CockpitWork playableFullCockpitOperations(const CockpitWorkState& state, const CockpitWorkBitmaps& bitmaps) {
    return cockpitOperationsImpl(state, bitmaps, RenderDetailDomain::fullPlayable);
}

enum class HorizonExecutionDomain { ordinary, level2, playableOrdinary, playableLevel2,
    fullPlayableOrdinary, fullPlayableLevel2 };

static std::vector<NormalWorkOperation> horizonOperationsImpl(const HorizonWorkState& state,
    const IndexedBitmap* overlay, const HorizonExecutionDomain domain = HorizonExecutionDomain::ordinary) {
    const auto middle = horizonTop + (horizonBottom - horizonTop) / 2;
    const auto full = domain == HorizonExecutionDomain::fullPlayableOrdinary
        || domain == HorizonExecutionDomain::fullPlayableLevel2;
    const auto playable = full || domain == HorizonExecutionDomain::playableOrdinary
        || domain == HorizonExecutionDomain::playableLevel2;
    const auto level2 = domain == HorizonExecutionDomain::level2
        || domain == HorizonExecutionDomain::playableLevel2 || domain == HorizonExecutionDomain::fullPlayableLevel2;
    if (state.viewTop != horizonTop || state.viewBottom != horizonBottom
        || (static_cast<std::uint16_t>(state.cameraYaw) & 15U) != 0
        || (!level2 && state.cameraYaw != 0
            && state.background != HorizonBackground::gradient)
        || (playable ? state.stage < (full ? 0 : minimumPlayableDetail) || state.stage > maximumPlayableDetail
            : state.stage <= cockpitOverlayStage)
        || (state.background != HorizonBackground::solid && state.background != HorizonBackground::gradient)
        || (state.overlay != HorizonOverlay::disabled && state.overlay != HorizonOverlay::enabled)
        || state.bitmapWidth != horizonPageWidth || state.bitmapHeight == 0 || state.bitmapHeight >= middle - horizonTop) {
        throw std::invalid_argument("unsupported ordinary horizon work path");
    }
    constexpr std::uint64_t prefixDispatches = 64;
    constexpr std::uint64_t groundControlDispatches = 11;
    constexpr std::uint64_t panoramaControlDispatches = 10;
    constexpr std::uint64_t overlayGuardDispatches = 4;
    constexpr std::uint64_t rightControlDispatches = 15;
    constexpr std::uint64_t leftControlDispatches = 12;
    constexpr std::uint64_t returnDispatches = 12;
    std::vector<NormalWorkOperation> operations;
    // Caller/control counts include7c08 viewport setup and each resident JMPF stub.
    if (state.background == HorizonBackground::solid) {
        appendInstructions(operations, prefixDispatches);
        appendHorizonRectangle(operations, static_cast<std::uint16_t>(middle - state.bitmapHeight - horizonTop));
        appendInstructions(operations, groundControlDispatches);
    } else {
        constexpr std::uint8_t gradientLimit = 254;
        appendInstructions(operations, 61); //Through6BFF: Level1/native palette and nonempty sky.
        auto color = state.skyColor;
        for (int row = middle - state.bitmapHeight - 1; row >= horizonTop; --row) {
            appendInstructions(operations, 7); //6C01..6C11, including2E25 CALLF.
            appendHorizonRectangle(operations, 1);
            appendInstructions(operations, 3); //6C16 cleanup and saturation test.
            if (color < gradientLimit) {
                appendInstructions(operations, 3); //6C20..6C27.
                ++color;
            }
            appendInstructions(operations, 4); //6C2A..6C33 row decrement/loop.
        }
        appendInstructions(operations, 4 + 10); //6C35..6C3E reset;6C5B..6C76 ground CALLF.
    }
    appendHorizonRectangle(operations, static_cast<std::uint16_t>(horizonBottom + 1 - middle));
    if (state.cameraYaw != 0) {
        const auto angle = state.cameraYaw >= 0 ? state.cameraYaw / 64 : -((-int{state.cameraYaw} + 63) / 64);
        appendInstructions(operations, 3); //6C7B cleanup and overlay-enabled gate.
        if (state.overlay == HorizonOverlay::enabled) {
            appendInstructions(operations, 2);
            if (angle < 320) {
                appendInstructions(operations, 2);
                if (angle > 0) {
                    if (state.stage <= cockpitOverlayStage) {
                        appendInstructions(operations, 2); //6C92 CMP/6C97 JNG; no overlay call.
                    } else {
                        appendInstructions(operations, 11); //Stage guard and2C1B caller.
                        IndexedBitmap culledOverlay{32, 27, std::vector<std::uint8_t>(32 * 27)};
                        if (!overlay && angle - 40 + 31 >= 24 && angle - 40 <= 295) {
                            throw std::invalid_argument("visible horizon overlay requires its source pixels");
                        }
                        const auto& bitmap = overlay ? *overlay : culledOverlay;
                        const auto expectedWidth = level2 ? 72 : 32;
                        const auto expectedHeight = level2 ? 38 : 27;
                        if (bitmap.width != expectedWidth || bitmap.height != expectedHeight) {
                            throw std::invalid_argument("horizon overlay differs from its source campaign resource");
                        }
                        const auto work = uiBitmapOperations(bitmap, {static_cast<std::int16_t>(angle - 40), 24},
                            {24, 295, 24, 145}, 0xa000);
                        operations.insert(operations.end(), work.operations.begin(), work.operations.end());
                        appendInstructions(operations, 1);
                    }
                }
            }
        }
        if (state.stage <= panoramaStage) {
            appendInstructions(operations, 14); //6CB6/BB and6D1A..6D38 plus resident stub.
            appendHorizonRectangle(operations, state.bitmapHeight);
            appendInstructions(operations, 6); //6D3D..6D45.
            return operations;
        }
        const auto x = 24 + static_cast<int>(static_cast<std::uint16_t>(angle) & 255U);
        appendInstructions(operations, 7); //6CB6..6CC4 plus2C1F JMPF.
        appendYawHorizonBitmap(operations, state.bitmapHeight, x);
        appendInstructions(operations, 5);
        if (x + horizonPageWidth < 295) {
            appendInstructions(operations, 10);
            appendYawHorizonBitmap(operations, state.bitmapHeight, x + horizonPageWidth);
            appendInstructions(operations, 1);
        }
        appendInstructions(operations, 11);
        appendYawHorizonBitmap(operations, state.bitmapHeight, x - horizonPageWidth);
        appendInstructions(operations, returnDispatches);
        return operations;
    }
    if (state.stage <= panoramaStage) {
        appendInstructions(operations, 3 + (state.overlay == HorizonOverlay::enabled ? overlayGuardDispatches : 0));
        appendInstructions(operations, 14); //6CB6/BB and6D1A..6D38 plus resident stub.
        appendHorizonRectangle(operations, state.bitmapHeight);
        appendInstructions(operations, 6);
        return operations;
    }
    appendInstructions(operations, panoramaControlDispatches + (state.overlay == HorizonOverlay::enabled ? overlayGuardDispatches : 0));
    appendHorizonBitmap(operations, state.bitmapHeight, HorizonTile::primary);
    appendInstructions(operations, rightControlDispatches);
    appendHorizonBitmap(operations, state.bitmapHeight, HorizonTile::rightSliver);
    appendInstructions(operations, leftControlDispatches);
    appendHorizonBitmap(operations, state.bitmapHeight, HorizonTile::culledLeft);
    appendInstructions(operations, returnDispatches);
    return operations;
}

std::vector<NormalWorkOperation> horizonOperations(const HorizonWorkState& state) {
    return horizonOperationsImpl(state, nullptr);
}

static HorizonExecutionWork horizonExecutionImpl(const HorizonWorkState& state,
    const std::vector<IndexedBitmap>& frames, const HorizonExecutionDomain domain) {
    auto operations = horizonOperationsImpl(state, frames.size() > 4 ? &frames[4] : nullptr, domain);
    constexpr std::size_t panoramaPages = 4;
    constexpr std::uint8_t gradientLimit = 254;
    if (frames.size() < panoramaPages) {
        throw std::invalid_argument("horizon execution requires the four source panorama pages");
    }
    for (std::size_t index = 0; index < panoramaPages; ++index) {
        const auto& frame = frames[index];
        if (frame.width != state.bitmapWidth || frame.height != state.bitmapHeight
            || frame.pixels.size() != static_cast<std::size_t>(frame.width) * frame.height) {
            throw std::invalid_argument("horizon bitmap differs from the admitted source work profile");
        }
    }
    HorizonExecutionWork result{std::move(operations), {}, {24, 295, horizonTop, horizonBottom},
        state.groundColor, state.groundColor};
    const auto middle = horizonTop + (horizonBottom - horizonTop) / 2;
    const auto panoramaTop = middle - state.bitmapHeight;
    const auto row = [&](const int y, const std::uint8_t color) {
        for (int x = result.viewport.left; x <= result.viewport.right; ++x) {
            result.pixels.push_back({{static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)}, color});
        }
    };
    if (state.background == HorizonBackground::gradient) {
        auto color = state.skyColor;
        for (int y = panoramaTop - 1; y >= horizonTop; --y) {
            row(y, color);
            if (color < gradientLimit) {
                ++color;
            }
        }
    } else {
        for (int y = horizonTop; y < panoramaTop; ++y) {
            row(y, state.skyColor);
        }
    }
    for (int y = middle; y <= horizonBottom; ++y) {
        row(y, state.groundColor);
    }
    const auto angle = state.cameraYaw >= 0 ? state.cameraYaw / 64 : -((-int{state.cameraYaw} + 63) / 64);
    if (state.stage > cockpitOverlayStage && state.overlay == HorizonOverlay::enabled && angle > 0 && angle < 320) {
        if (frames.size() <= 4) {
            throw std::invalid_argument("horizon overlay requires its source bitmap");
        }
        const auto work = uiBitmapOperations(frames[4], {static_cast<std::int16_t>(angle - 40), 24},
            result.viewport, 0xa000);
        for (const auto& effect : work.effects) {
            const auto& pixel = std::get<RadarPixelWrite>(effect);
            result.pixels.push_back({pixel.point, pixel.color});
        }
    }
    if (state.stage <= panoramaStage) {
        //6D1D reloads the original sky color after the gradient/ground writes.
        for (int y = panoramaTop; y < middle; ++y) {
            row(y, state.skyColor);
        }
        result.lineColor = state.skyColor;
        result.fillColor = state.skyColor;
        return result;
    }
    const auto primary = static_cast<unsigned>((-((static_cast<std::uint16_t>(angle) >> 8) & 255U)) & 3U);
    const auto primaryX = result.viewport.left + static_cast<int>(static_cast<std::uint16_t>(angle) & 255U);
    for (const auto neighbor : {0, 1, -1}) {
        const auto index = (primary + neighbor) & 3U;
        const auto x = primaryX + neighbor * int{horizonPageWidth};
        if (neighbor == 1 && x >= 295) {
            continue;
        }
        const auto& frame = frames[index];
        const auto trim = std::max(0, result.viewport.left - x);
        for (std::uint16_t plane = 0; plane < pixelsPerPlanarByte; ++plane) {
            for (std::uint16_t y = 0; y < frame.height; ++y) {
                for (int column = trim + plane; column < frame.width; column += pixelsPerPlanarByte) {
                    const auto destinationX = x + column;
                    if (destinationX < result.viewport.left || destinationX > result.viewport.right) {
                        continue;
                    }
                    result.pixels.push_back({{static_cast<std::int16_t>(destinationX),
                        static_cast<std::int16_t>(panoramaTop + y)}, frame.pixels[y * frame.width + column]});
                }
            }
        }
    }
    return result;
}


HorizonExecutionWork horizonExecution(const HorizonWorkState& state, const std::vector<IndexedBitmap>& frames) {
    return horizonExecutionImpl(state, frames, HorizonExecutionDomain::ordinary);
}

HorizonExecutionWork playableHorizonExecution(const HorizonWorkState& state, const std::vector<IndexedBitmap>& frames) {
    return horizonExecutionImpl(state, frames, HorizonExecutionDomain::playableOrdinary);
}

HorizonExecutionWork playableFullHorizonExecution(const HorizonWorkState& state, const std::vector<IndexedBitmap>& frames) {
    return horizonExecutionImpl(state, frames, HorizonExecutionDomain::fullPlayableOrdinary);
}

static HorizonExecutionWork level2HorizonExecutionImpl(const HorizonWorkState& state,
    const std::vector<IndexedBitmap>& frames, const HorizonExecutionDomain domain) {
    constexpr std::uint16_t panoramaHeight = 18;
    constexpr std::uint16_t overlayWidth = 72;
    constexpr std::uint16_t overlayHeight = 38;
    if (state.background != HorizonBackground::solid || state.bitmapHeight != panoramaHeight
        || state.skyColor != 64 || state.groundColor != 78 || frames.size() != 5
        || frames[4].width != overlayWidth || frames[4].height != overlayHeight
        || frames[4].pixels.size() != static_cast<std::size_t>(overlayWidth) * overlayHeight) {
        throw std::invalid_argument("Level2 horizon requires its solid sky and original bitmap dimensions");
    }
    //6BF2 selects solid sky independently of yaw;6CA7 supplies this campaign's fifth bitmap.
    return horizonExecutionImpl(state, frames, domain);
}

HorizonExecutionWork level2HorizonExecution(const HorizonWorkState& state, const std::vector<IndexedBitmap>& frames) {
    return level2HorizonExecutionImpl(state, frames, HorizonExecutionDomain::level2);
}

HorizonExecutionWork playableLevel2HorizonExecution(const HorizonWorkState& state,
    const std::vector<IndexedBitmap>& frames) {
    return level2HorizonExecutionImpl(state, frames, HorizonExecutionDomain::playableLevel2);
}

HorizonExecutionWork playableFullLevel2HorizonExecution(const HorizonWorkState& state, const std::vector<IndexedBitmap>& frames) {
    return level2HorizonExecutionImpl(state, frames, HorizonExecutionDomain::fullPlayableLevel2);
}

GroundControlWork groundControlOperations(const GroundControlState& state) {
    constexpr std::int16_t subpixelScale = 32;
    constexpr std::int16_t turnScale = 16;
    constexpr std::int16_t minimumX = 24;
    constexpr std::int16_t maximumX = 295;
    constexpr std::int16_t minimumY = 90;
    constexpr std::int16_t maximumY = 146;
    constexpr std::int16_t shadeScale = 8;
    constexpr std::uint64_t callerPrefixDispatches = 5;
    constexpr std::uint64_t routinePrefixDispatches = 11;
    constexpr std::uint64_t nextPointDispatches = 1;
    constexpr std::uint64_t routineReturnDispatches = 5;
    constexpr std::uint64_t callerCleanupDispatches = 1;
    //4c54 uses signed SAR4; turns0..15 all take the stationary branch.
    if (state.turn < 0 || state.turn >= turnScale || state.speed != 0 || state.cameraHeight != 0) {
        throw std::invalid_argument("unsupported stationary ground control path");
    }
    GroundControlWork result{{{NormalWorkKind::instructions,
        callerPrefixDispatches + routinePrefixDispatches}}, {},
        {{NormalWorkKind::instructions, routineReturnDispatches + callerCleanupDispatches}}};
    for (std::size_t index = 0; index < state.points.size(); ++index) {
        const auto point = state.points[index];
        // Negative coordinates necessarily reset; division is exact for admitted positives.
        if (point.x < 0 || point.y < 0) {
            throw std::invalid_argument("ground reset requires RNG work");
        }
        const ProjectedPoint screen{static_cast<std::int16_t>(point.x / subpixelScale),
            static_cast<std::int16_t>(point.y / subpixelScale)};
        if (screen.x < minimumX || screen.x > maximumX || screen.y < minimumY || screen.y > maximumY) {
            throw std::invalid_argument("ground reset requires RNG work");
        }
        result.draws[index] = {screen,
            static_cast<std::uint16_t>((maximumY - screen.y) / shadeScale),
            {{NormalWorkKind::instructions, groundDrawPrefixDispatches}},
            {{NormalWorkKind::instructions, groundDrawSuffixDispatches
                + (index + 1 < state.points.size() ? nextPointDispatches : 0)}}};
    }
    return result;
}

namespace {

void checkGroundViewport(const Viewport viewport) {
    if (viewport.left < 0 || viewport.right >= planarScreenWidth || viewport.top < 0
        || viewport.bottom >= planarScreenHeight || viewport.left > viewport.right
        || viewport.top > viewport.bottom) {
        throw std::invalid_argument("unsupported ground bitmap viewport");
    }
}

std::uint16_t groundBitmapRows(const IndexedBitmap& bitmap, const ProjectedPoint screen,
    const Viewport viewport) {
    if (screen.x < viewport.left || screen.y < viewport.top
        || screen.x > viewport.right) {
        throw std::invalid_argument("ground bitmap crosses viewport boundary");
    }
    if (screen.y > viewport.bottom) {
        return 0;
    }
    return std::min<std::uint16_t>(bitmap.height, viewport.bottom - screen.y + 1);
}

enum class GroundBitmapClipDomain { legacy, level1Sprites, level2Sprites };

std::uint16_t groundBitmapColumns(const IndexedBitmap& bitmap, const ProjectedPoint screen,
    const Viewport viewport,const GroundBitmapClipDomain domain=GroundBitmapClipDomain::legacy) {
    if (groundBitmapRows(bitmap, screen, viewport) == 0) {
        return 0;
    }
    const auto columns = std::min<std::uint16_t>(bitmap.width, viewport.right - screen.x + 1);
    constexpr std::uint16_t minimumGroundColumns = pixelsPerPlanarByte - 1;
    constexpr std::uint16_t qualifiedThinSourceWidth = 4 * pixelsPerPlanarByte;
    constexpr std::uint16_t minimumThinSourceHeight = 2;
    constexpr std::uint16_t maximumThinSourceHeight = 4;
    constexpr Viewport qualifiedThinViewport{24, 295, 24, 145};
    constexpr std::uint16_t level1SourceWidth=2*pixelsPerPlanarByte;
    const auto level1Source=domain==GroundBitmapClipDomain::level1Sprites
        && bitmap.width==level1SourceWidth && bitmap.height>=1 && bitmap.height<=maximumThinSourceHeight;
    constexpr std::uint16_t level2LargeSourceWidth=6*pixelsPerPlanarByte;
    constexpr std::uint16_t level2LargeSourceHeight=6;
    const auto level2Source=domain==GroundBitmapClipDomain::level2Sprites
        && ((bitmap.width==level2LargeSourceWidth && bitmap.height==level2LargeSourceHeight)
            || (bitmap.width==level1SourceWidth && bitmap.height==1));
    const auto thinSource = (level1Source || level2Source || (bitmap.width == qualifiedThinSourceWidth
        && bitmap.height >= minimumThinSourceHeight && bitmap.height <= maximumThinSourceHeight))
        && viewport.left == qualifiedThinViewport.left && viewport.right == qualifiedThinViewport.right
        && viewport.top == qualifiedThinViewport.top && viewport.bottom == qualifiedThinViewport.bottom;
    if (columns < minimumGroundColumns && !thinSource) {
        throw std::invalid_argument("unsupported ground bitmap right clipping");
    }
    return columns;
}

std::vector<NormalWorkOperation> groundBitmapOperations(const IndexedBitmap& bitmap,
    const ProjectedPoint screen, const Viewport viewport,
    const GroundBitmapClipDomain domain=GroundBitmapClipDomain::legacy) {
    constexpr std::uint64_t interiorClipDispatches = 15;
    constexpr std::uint64_t bottomTrimDispatches = 4;
    constexpr std::uint64_t rightTrimDispatches = 8;
    constexpr std::size_t bitmapPostHeaderOperation = 2;
    const auto rows = groundBitmapRows(bitmap, screen, viewport);
    if (rows == 0) {
        constexpr std::uint64_t headerPrefixDispatches = 13;
        constexpr std::uint64_t bottomCullSuffixDispatches = 53;
        planarBitmapWork(bitmap, screen.x, PlanarBlitMode::generalTransparent);
        const auto rightClip = static_cast<std::int32_t>(screen.x) + bitmap.width - 1 >= viewport.right;
        return {{NormalWorkKind::instructions, headerPrefixDispatches},
            {NormalWorkKind::repeatString, bitmapHeaderWords},
            {NormalWorkKind::instructions, bottomCullSuffixDispatches + (rightClip ? rightTrimDispatches : 0)}};
    }
    const auto columns = groundBitmapColumns(bitmap, screen, viewport,domain);
    auto visible = bitmap;
    if (rows != bitmap.height || columns != bitmap.width) {
        planarBitmapWork(bitmap, screen.x, PlanarBlitMode::generalTransparent);
        visible.width = columns;
        visible.height = rows;
        visible.pixels.clear();
        visible.pixels.reserve(static_cast<std::size_t>(columns) * rows);
        for (std::uint16_t row = 0; row < rows; ++row) {
            const auto first = bitmap.pixels.begin() + static_cast<std::size_t>(row) * bitmap.width;
            visible.pixels.insert(visible.pixels.end(), first, first + columns);
        }
    }
    // The work view crops rows; pixel traversal retains the original source pitch.
    if (rows == bitmap.height && columns == bitmap.width) {
        planarBitmapWork(bitmap, screen.x, PlanarBlitMode::generalTransparent);
    }
    auto body = bitmapOperations(visible, screen.x, PlanarBlitMode::generalTransparent);
    auto clipping = interiorClipDispatches;
    if (static_cast<std::int32_t>(screen.y) + bitmap.height - 1 >= viewport.bottom) {
        clipping += bottomTrimDispatches;
    }
    if (static_cast<std::int32_t>(screen.x) + bitmap.width - 1 >= viewport.right) {
        clipping += rightTrimDispatches;
    }
    body[bitmapPostHeaderOperation].count += clipping;
    return body;
}

}  // namespace

std::vector<NormalWorkOperation> groundClippedOperations(const GroundControlState& state,
    const std::array<IndexedBitmap, 8>& bitmaps, const Viewport viewport) {
    const auto control = groundControlOperations(state);
    checkGroundViewport(viewport);
    std::vector<NormalWorkOperation> operations = control.prefix;
    const auto append = [&operations](const std::vector<NormalWorkOperation>& body) {
        operations.insert(operations.end(), body.begin(), body.end());
    };
    for (const auto& draw : control.draws) {
        append(draw.prefix);
        append(groundBitmapOperations(bitmaps[draw.shadeIndex], draw.screen, viewport));
        append(draw.suffix);
    }
    append(control.suffix);
    return operations;
}

GroundExecutionWork groundClippedExecution(const GroundControlState& state, const StellarRng& rng,
    const std::array<IndexedBitmap, 8>& bitmaps, const Viewport viewport) {
    constexpr std::int16_t turnScale = 16;
    if (state.turn < 0 || state.turn >= turnScale) {
        throw std::invalid_argument("unsupported ground execution turn");
    }
    return groundSteeringExecution(state,rng,bitmaps,viewport);
}

static GroundExecutionWork groundSteeringPlan(const GroundControlState& state, const StellarRng& rng,
    const std::array<IndexedBitmap, 8>& bitmaps, const Viewport viewport,const GroundBitmapClipDomain domain) {
    constexpr std::int16_t turnScale = 16;
    constexpr std::int16_t turnLimit = 0x200;
    constexpr std::int16_t forwardSpeedLimit = 200;
    constexpr std::int16_t groundHorizontalCenter = 0x13E0;
    constexpr std::int16_t groundVerticalCenter = 0x0AA0;
    constexpr std::int32_t subpixelScale = 32;
    constexpr std::uint64_t prefixDispatches = 16;
    constexpr std::uint64_t suffixDispatches = 6;
    constexpr std::uint64_t movementDispatches = 51;
    constexpr std::uint64_t multiplyFastDispatches = 11;
    constexpr std::uint64_t multiplyFullDispatches = 19;
    constexpr std::uint64_t resetControlDispatches = 15;
    constexpr std::uint64_t resetGuardDispatches = 2;
    constexpr std::uint64_t resetBodyDispatches = 31;
    constexpr std::uint64_t rngDispatches = 20;
    constexpr std::uint64_t resetSuffixDispatches = 6;
    constexpr std::uint16_t maximumRngIndex = (StellarRng::wordCount - 1) * 2;
    if (state.turn < -turnLimit || state.turn > turnLimit || state.speed < -forwardSpeedLimit
        || state.speed > forwardSpeedLimit || state.cameraHeight != 0
        || rng.indexA() > maximumRngIndex || rng.indexB() > maximumRngIndex
        || rng.indexA() % 2 != 0 || rng.indexB() % 2 != 0) {
        throw std::invalid_argument("unsupported ground execution path");
    }
    checkGroundViewport(viewport);
    GroundExecutionWork result{Level1GroundState(state.points), rng,
        {{NormalWorkKind::instructions, prefixDispatches}}};
    auto workRng = rng;
    const auto wrappedDelta = [](const std::int16_t value, const std::int16_t center) noexcept {
        const auto word = static_cast<std::uint16_t>(static_cast<std::uint16_t>(value)
            - static_cast<std::uint16_t>(center));
        return static_cast<std::int16_t>(word);
    };
    for (std::size_t index = 0; index < state.points.size(); ++index) {
        const auto point = state.points[index];
        const auto movedPoint = advanceGroundPoint(point, state.turn, state.speed);
        if (state.speed != 0) {
            const auto yDelta = wrappedDelta(point.y, groundVerticalCenter);
            const auto xDelta = wrappedDelta(point.x, groundHorizontalCenter);
            const auto multiplyDispatches = [speed = state.speed, multiplyFastDispatches, multiplyFullDispatches](const std::int16_t delta) {
                return delta >= 0 && speed >= 0 ? multiplyFastDispatches : multiplyFullDispatches;
            };
            appendInstructions(result.operations,
                movementDispatches + multiplyDispatches(yDelta) + multiplyDispatches(xDelta));
        }
        if (state.turn < 0 || state.turn >= turnScale) {
            appendInstructions(result.operations, 12); //4CDD..4CF6: signed coarse-turn shear.
        }
        const auto coordinate = [subpixelScale](const std::int16_t value) {
            return static_cast<std::int16_t>((value >= 0 ? value : value - (subpixelScale - 1)) / subpixelScale);
        };
        const ProjectedPoint screen{coordinate(movedPoint.x), coordinate(movedPoint.y)};
        const auto continues = index + 1 < state.points.size() ? 1U : 0U;
        std::uint64_t guard = 0;
        if (screen.x > 295) {
            guard = 0;
        } else if (screen.x < 24) {
            guard = 1;
        } else if (screen.y > 146) {
            guard = 2;
        } else if (screen.y < 90) {
            guard = 3;
        } else {
            appendInstructions(result.operations, groundDrawPrefixDispatches);
            const auto& bitmap = bitmaps[(146 - screen.y) / 8];
            const auto body = groundBitmapOperations(bitmap, screen, viewport,domain);
            result.operations.insert(result.operations.end(), body.begin(), body.end());
            //1280 traverses source planes and stores only nonzero pixels.
            const auto rows = groundBitmapRows(bitmap, screen, viewport);
            const auto columns = groundBitmapColumns(bitmap, screen, viewport,domain);
            for (std::uint16_t plane = 0; plane < pixelsPerPlanarByte; ++plane) {
                for (std::uint16_t row = 0; row < rows; ++row) {
                    for (std::uint16_t column = plane; column < columns; column += pixelsPerPlanarByte) {
                        const auto color = bitmap.pixels[static_cast<std::size_t>(row) * bitmap.width + column];
                        if (color != 0) {
                            result.pixels.push_back({{static_cast<std::int16_t>(screen.x + column),
                                static_cast<std::int16_t>(screen.y + row)}, color});
                        }
                    }
                }
            }
            appendInstructions(result.operations, groundDrawSuffixDispatches + continues);
            continue;
        }
        appendInstructions(result.operations, resetControlDispatches + resetGuardDispatches * guard);
        auto reset = resetBodyDispatches;
        for (unsigned call = 0; call < 2; ++call) {
            reset += rngDispatches + (workRng.indexA() == 0 ? 1U : 0U) + (workRng.indexB() == 0 ? 1U : 0U);
            workRng.next();
        }
        appendInstructions(result.operations, reset);
        appendInstructions(result.operations, resetSuffixDispatches + continues);
    }
    appendInstructions(result.operations, suffixDispatches);
    result.ground.advance(state.turn, state.speed, result.rng);
    return result;
}

GroundExecutionWork groundSteeringExecution(const GroundControlState& state,const StellarRng& rng,
    const std::array<IndexedBitmap,8>& bitmaps,const Viewport viewport) {
    return groundSteeringPlan(state,rng,bitmaps,viewport,GroundBitmapClipDomain::legacy);
}

GroundExecutionWork level1GroundSpriteExecution(const GroundControlState& state,const StellarRng& rng,
    const std::array<IndexedBitmap,8>& bitmaps,const Viewport viewport) {
    return groundSteeringPlan(state,rng,bitmaps,viewport,GroundBitmapClipDomain::level1Sprites);
}

GroundExecutionWork level2GroundSpriteExecution(const GroundControlState& state,const StellarRng& rng,
    const std::array<IndexedBitmap,8>& bitmaps,const Viewport viewport) {
    return groundSteeringPlan(state,rng,bitmaps,viewport,GroundBitmapClipDomain::level2Sprites);
}

WorldViewSetupWork worldViewSetupOperations(const WorldViewSetupInput& state) {
    constexpr std::uint8_t qualifiedViewFlags = 0x88;
    constexpr std::uint8_t qualifiedProjectionShift = 7;
    constexpr Viewport qualifiedViewport{24, 294, 24, 145};
    constexpr FixedPosition qualifiedLight{256, -256, 362};
    const auto nonzero = [](const std::uint16_t angle) { return angle != 0; };
    if (state.flags != qualifiedViewFlags || state.projectionShift != qualifiedProjectionShift
        || !(state.viewport == qualifiedViewport) || !(state.light == qualifiedLight)
        || std::any_of(state.viewAngles.begin(), state.viewAngles.end(), nonzero)
        || state.actorAngles[0] != 0 || state.actorAngles[1] != 0 || (state.actorAngles[2] & 15U) != 0) {
        throw std::invalid_argument("unsupported world view setup path");
    }

    auto matrix = makeFixedMatrixQ14(state.actorAngles[0], state.actorAngles[1], state.actorAngles[2]);
    transpose(matrix);
    constexpr std::uint64_t positionPrefixDispatches = 15;
    constexpr std::uint64_t positionWords = 6;
    constexpr std::uint64_t matrixAndAngleCopyDispatches = 55;
    constexpr std::uint64_t configurationSuffixDispatches = 149;
    const auto yaw = state.actorAngles[2];
    const auto fold = [](const std::uint16_t angle) { return ((angle >> 4) & 0x800U) != 0; };
    const auto yawExtra = yaw == 0 ? 0U : 66U + 2U * (fold(yaw)
        + fold(static_cast<std::uint16_t>(yaw + 0xc000U))) - 39U + 34U;
    const auto light = transformToView(state.light, {{}, matrix, {}, 0});
    return {{state.origin, matrix, state.viewport, std::int32_t{1} << state.projectionShift}, light,
        {{NormalWorkKind::instructions, positionPrefixDispatches},
         {NormalWorkKind::repeatString, positionWords},
         {NormalWorkKind::instructions, matrixAndAngleCopyDispatches},
         {NormalWorkKind::string, 1}, {NormalWorkKind::string, 1}, {NormalWorkKind::string, 1},
         {NormalWorkKind::instructions, configurationSuffixDispatches + yawExtra}}};
}

namespace {
enum class RadarViewExecution { sourceWork, nativePixels };

WorldViewSetupWork radarViewSetup(const WorldViewSetupInput& state, const RadarViewExecution execution) {
    constexpr std::array<std::uint16_t, 3> radarAngles{0xc000, 0, 0};
    constexpr Viewport radarViewport{112, 205, 146, 180};
    constexpr FixedPosition radarLight{256, -256, 362};
    constexpr std::int32_t cameraHeight = 200000;
    constexpr std::uint8_t projectionShift = 7;
    constexpr std::uint8_t radarViewFlags = 0x88;
    if (state.flags != radarViewFlags || state.projectionShift != projectionShift || state.origin.z != cameraHeight
        || state.viewAngles != radarAngles || state.actorAngles[0] != 0 || state.actorAngles[1] != 0
        || (execution == RadarViewExecution::sourceWork && (state.actorAngles[2] & 0x000fU) != 0)
        || !(state.viewport == radarViewport) || !(state.light == radarLight)) {
        throw std::invalid_argument("unsupported radar view setup path");
    }

    // b807 passes its source scratch offset in CX;38ff retains it in mixed-mode products.
    constexpr std::uint16_t radarMatrixScratchOffset = 0x5240;
    const auto yaw = state.actorAngles[2];
    auto matrix = composeFixedMatricesQ14(makeFixedMatrixQ14(radarAngles[0], radarAngles[1], radarAngles[2]),
        makeFixedMatrixQ14(0, 0, yaw), radarMatrixScratchOffset);
    const auto atanDispatches = [](const std::int16_t first, const std::int16_t second) {
        const auto negativeFirst = first < 0;
        const auto negativeSecond = second < 0;
        const auto a = static_cast<std::uint16_t>(negativeFirst ? -static_cast<std::int32_t>(first) : first);
        const auto b = static_cast<std::uint16_t>(negativeSecond ? -static_cast<std::int32_t>(second) : second);
        auto count = std::uint64_t{13} + 2 * (negativeFirst + negativeSecond);
        if (a == 0) {
            count += 4;
            if (b == 0) {
                return count + 3;
            }
            count += 2;
        } else {
            count += 2;
            if (b == 0) {
                count += 2;
            } else {
                count += 2;
                count += a == b ? 2 : 1 + (a > b ? 12 : 11);
            }
        }
        return count + 11 + 3 * (negativeFirst + negativeSecond);
    };
    const auto recoveredAngles = 30 + 24 + atanDispatches(matrix.values[0], matrix.values[1]);
    transpose(matrix);
    const auto light = transformToView(state.light, {{}, matrix, {}, 0});
    constexpr std::uint64_t positionPrefix = 15;
    constexpr std::uint64_t radarMatrixBuilder = 345;
    constexpr std::uint64_t identityCameraBuilder = 39;
    constexpr std::uint64_t yawCameraBuilder = 66;
    const auto sineAngle = static_cast<std::uint16_t>(yaw + 0xc000U);
    const auto cameraMatrixBuilder = yaw == 0 ? identityCameraBuilder : yawCameraBuilder
        + 2 * (((yaw >> 4) & 0x0800U) != 0) + 2 * (((sineAngle >> 4) & 0x0800U) != 0);
    constexpr std::uint64_t matrixCaller = 6 + 10 + 8;
    constexpr std::uint64_t composePrefix = 23;
    constexpr std::uint64_t composeSuffix = 7;
    constexpr std::uint64_t yawComposition = 140;
    constexpr std::uint64_t transposeDispatches = 16;
    constexpr std::uint64_t lightTransform = 88;
    constexpr std::uint64_t configurationCaller = 6 + 5 + 39 + 26;
    if (yaw != 0) {
        return {{state.origin, matrix, state.viewport, std::int32_t{1} << projectionShift}, light,
            {{NormalWorkKind::instructions, positionPrefix}, {NormalWorkKind::repeatString, 6},
             {NormalWorkKind::instructions, matrixCaller + radarMatrixBuilder + cameraMatrixBuilder
                 + yawComposition + recoveredAngles + transposeDispatches + lightTransform + configurationCaller}}};
    }
    return {{state.origin, matrix, state.viewport, std::int32_t{1} << projectionShift}, light,
        {{NormalWorkKind::instructions, positionPrefix}, {NormalWorkKind::repeatString, 6},
         {NormalWorkKind::instructions, matrixCaller + radarMatrixBuilder + cameraMatrixBuilder + composePrefix},
         {NormalWorkKind::repeatString, 9},
         {NormalWorkKind::instructions, composeSuffix + recoveredAngles + transposeDispatches
             + lightTransform + configurationCaller}}};
}
}

WorldViewSetupWork radarViewSetupOperations(const WorldViewSetupInput& state) {
    return radarViewSetup(state, RadarViewExecution::sourceWork);
}

WorldViewSetupWork radarViewSetupPixels(const WorldViewSetupInput& state) {
    auto result = radarViewSetup(state, RadarViewExecution::nativePixels);
    result.operations.clear();
    return result;
}

WorldFirstCenterWork worldFirstCenterOperations(const ViewTransform& view, const WorldActorWorkInput& actor) {
    return worldFirstCenterOperations(view, actor, WorldSourceFlagsDomain::ordinary);
}

WorldFirstCenterWork worldFirstCenterOperations(const ViewTransform& view, const WorldActorWorkInput& actor,
    const WorldSourceFlagsDomain domain) {
    const auto identity = makeFixedMatrixQ14(0, 0, 0);
    if ((domain != WorldSourceFlagsDomain::ordinary && domain != WorldSourceFlagsDomain::flagsWithoutCallback)
        || (actor.flags != 0 && (domain != WorldSourceFlagsDomain::flagsWithoutCallback || actor.flags != 4))
        || view.matrix.mode > 1
        || (view.matrix.mode == 0 && view.matrix.values != identity.values)) {
        throw std::invalid_argument("unsupported world first center path");
    }

    const auto subtract = [](const std::int32_t value, const std::int32_t origin) {
        return static_cast<std::int32_t>(static_cast<std::uint32_t>(value) - static_cast<std::uint32_t>(origin));
    };
    const FixedPosition relative{subtract(actor.position.x, view.origin.x),
        subtract(actor.position.y, view.origin.y), subtract(actor.position.z, view.origin.z)};
    constexpr std::uint64_t callerDispatches = 45;
    return {relative, transformToView(actor.position, view), {{NormalWorkKind::instructions, callerDispatches},
        {NormalWorkKind::instructions, vectorTransformDispatches(relative, view.matrix)}}};
}

namespace {
enum class ModelCullProfile { legacy, level1CombatLift, level1CombatLiftUpperClip, level1CombatLiftOutsideUpperClip, level1Bonus };

WorldModelCullWork modelCullOperations(const FixedPosition center, const std::uint32_t collisionRadius,
    const ModelCullProfile profile) {
    constexpr std::int32_t maximumLiftAltitude = 1000;
    constexpr std::uint32_t liftRadius = 748;
    constexpr std::int64_t nearPlane = 128;
    constexpr std::int64_t perspectiveScale = 128;
    constexpr std::int64_t screenCenterX = 159;
    constexpr std::int64_t screenCenterY = 84;
    constexpr std::int64_t viewportRight = 294;
    constexpr std::int64_t viewportTop = 24;
    constexpr std::uint64_t nearCullDispatches = 22;
    constexpr std::uint64_t shallowCullDispatches = 25;
    constexpr std::uint64_t lateralCullSuffix = 24;
    constexpr std::int64_t signedWordLimit = 32768;
    constexpr std::int64_t unsignedWordLimit = 65536;
    const auto radius = static_cast<std::int64_t>(collisionRadius);
    const auto farDepth = static_cast<std::int64_t>(center.y) + radius;
    if (center.z < 0 || center.z > maximumLiftAltitude || radius == 0 || radius >= signedWordLimit) {
        throw std::invalid_argument("unsupported world model cull path");
    }
    const auto minimumDepth = std::max(nearPlane, static_cast<std::int64_t>(center.y) - radius);
    const auto projectedRadius = static_cast<std::uint16_t>(std::min<std::int64_t>(256,
        radius * perspectiveScale / minimumDepth));
    if (farDepth < 0) {
        return {true, false, projectedRadius, {{NormalWorkKind::instructions, nearCullDispatches}}};
    }
    if (farDepth < nearPlane) {
        return {true, false, projectedRadius, {{NormalWorkKind::instructions, shallowCullDispatches}}};
    }
    const auto bonus = profile == ModelCullProfile::level1Bonus;
    const auto upperClip = bonus || profile == ModelCullProfile::level1CombatLiftUpperClip
        || profile == ModelCullProfile::level1CombatLiftOutsideUpperClip;
    if (!bonus && center.z != 0 && (collisionRadius != liftRadius || (center.z != maximumLiftAltitude && !upperClip))) {
        throw std::invalid_argument("unsupported world model cull path");
    }

    const auto absoluteX = std::abs(static_cast<std::int64_t>(center.x));
    const auto combatLift = profile == ModelCullProfile::level1CombatLift
        || profile == ModelCullProfile::level1CombatLiftUpperClip;
    if (combatLift && (collisionRadius != liftRadius || (!upperClip && center.z != maximumLiftAltitude)
        || absoluteX > radius || farDepth >= signedWordLimit)) {
        throw std::invalid_argument("unsupported combat lift sphere");
    }
    if (farDepth >= unsignedWordLimit || (!combatLift && !bonus && absoluteX <= radius)
        || (absoluteX + radius) * perspectiveScale >= 0x80000000LL) {
        throw std::invalid_argument("unsupported world model projection path");
    }
    const auto negativeXDispatches = center.x < 0 ? 4U : 0U;
    constexpr std::uint64_t firstProjectionPrefix = 61;
    constexpr std::uint64_t signedProjectionDispatches = 232;
    constexpr std::uint64_t positiveProjectionDispatches = 214;
    constexpr std::uint64_t wideProjectionExtraDispatches = 6; //5392: two DIVs add three instructions per X/Z.
    std::vector<NormalWorkOperation> operations{
        {NormalWorkKind::instructions, firstProjectionPrefix + negativeXDispatches},
        {NormalWorkKind::instructions, (center.z < radius ? signedProjectionDispatches : positiveProjectionDispatches)
            + (absoluteX < radius ? signedProjectionDispatches - positiveProjectionDispatches : 0)
            + (farDepth >= signedWordLimit ? wideProjectionExtraDispatches : 0)}};
    const auto nearestX = screenCenterX + (absoluteX - radius) * perspectiveScale / farDepth;
    if (nearestX > viewportRight) {
        operations.push_back({NormalWorkKind::instructions, lateralCullSuffix});
        return {true, false, projectedRadius, std::move(operations)};
    }

    const auto nearestY = screenCenterY - (center.z - radius) * perspectiveScale / farDepth;
    if (nearestY < viewportTop) {
        throw std::invalid_argument("unsupported world model vertical cull path");
    }

    if (static_cast<std::int64_t>(center.y) - radius < nearPlane) {
        throw std::invalid_argument("unsupported world model near-depth projection path");
    }

    const auto farthestX = screenCenterX + (absoluteX + radius) * perspectiveScale / minimumDepth;
    const auto upperY = screenCenterY - (center.z + radius) * perspectiveScale / minimumDepth;
    const auto rightClipped = farthestX > viewportRight;
    const auto clipped = rightClipped || upperY < viewportTop;
    if (combatLift && (rightClipped || (!upperClip && clipped))) {
        throw std::invalid_argument("unsupported combat lift clipping path");
    }
    constexpr std::uint64_t secondProjectionPrefix = 70;
    constexpr std::uint64_t insideSuffix = 27;
    constexpr std::uint64_t rightClippedSuffix = 20;
    constexpr std::uint64_t negativeUpperSuffix = 24;
    operations.push_back({NormalWorkKind::instructions, secondProjectionPrefix + negativeXDispatches});
    operations.push_back({NormalWorkKind::instructions, positiveProjectionDispatches
        + (minimumDepth >= signedWordLimit ? wideProjectionExtraDispatches : 0)});
    // CEA7 skips the low-word tests only when projected upperY is negative.
    operations.push_back({NormalWorkKind::instructions, rightClipped ? rightClippedSuffix
        : upperClip && upperY < 0 ? negativeUpperSuffix : insideSuffix});
    return {false, clipped, projectedRadius, std::move(operations)};
}

}

WorldModelCullWork worldModelCullOperations(const FixedPosition center, const std::uint32_t collisionRadius) {
    return modelCullOperations(center, collisionRadius, ModelCullProfile::legacy);
}

WorldModelCullWork level1CombatLiftModelCullOperations(const FixedPosition center) {
    constexpr std::uint32_t liftRadius = 748;
    if (center.z != 1000) {
        throw std::invalid_argument("unsupported combat lift altitude");
    }
    if (std::abs(static_cast<std::int64_t>(center.x)) > liftRadius) {
        return worldModelCullOperations(center, liftRadius);
    }
    if (static_cast<std::int64_t>(center.y) - liftRadius < 128) {
        throw std::invalid_argument("unsupported combat lift near depth");
    }
    return modelCullOperations(center, liftRadius, ModelCullProfile::level1CombatLift);
}

WorldModelCullWork level1CombatClippedLiftModelCullOperations(const FixedPosition center) {
    constexpr std::uint32_t liftRadius = 748;
    if (center.z < 0 || center.z > 1000) {
        throw std::invalid_argument("unsupported clipped combat lift sphere");
    }
    if (std::abs(static_cast<std::int64_t>(center.x)) > liftRadius) {
        return modelCullOperations(center, liftRadius, ModelCullProfile::level1CombatLiftOutsideUpperClip);
    }
    if (static_cast<std::int64_t>(center.y) - liftRadius < 128) {
        throw std::invalid_argument("unsupported clipped combat lift sphere");
    }
    return modelCullOperations(center, liftRadius, ModelCullProfile::level1CombatLiftUpperClip);
}

WorldModelCullWork level1ProjectileModelCullOperations(const FixedPosition center) {
    constexpr std::int64_t radius = 134;
    constexpr std::int64_t nearPlane = 128;
    constexpr std::int64_t perspective = 128;
    constexpr std::int64_t screenX = 159;
    constexpr std::int64_t screenY = 84;
    const auto absoluteX = std::abs(static_cast<std::int64_t>(center.x));
    const auto farDepth = static_cast<std::int64_t>(center.y) + radius;
    if (center.z != 0 || center.y < 0 || absoluteX > radius || farDepth >= 32768) {
        throw std::invalid_argument("unsupported Level1 projectile sphere");
    }
    const auto minimumDepth = std::max(nearPlane, static_cast<std::int64_t>(center.y) - radius);
    const auto projectedRadius = static_cast<std::uint16_t>(std::min<std::int64_t>(256,
        radius * perspective / minimumDepth));
    if (projectedRadius == 0) {
        throw std::invalid_argument("unsupported zero-radius Level1 projectile sphere");
    }
    const auto farthestX = screenX + (absoluteX + radius) * perspective / minimumDepth;
    const auto upperY = screenY - radius * perspective / minimumDepth;
    const auto rightClipped = farthestX > 294;
    const auto clipped = rightClipped || upperY < 24;
    // CC5E..CEBA: negative lower X/Z numerators, then positive upper numerators.
    constexpr std::uint64_t projectionPrefix = 61;
    constexpr std::uint64_t twoNegativeProjection = 250;
    constexpr std::uint64_t oneNegativeProjection = 232;
    constexpr std::uint64_t positiveProjection = 214;
    constexpr std::uint64_t secondProjectionPrefix = 70;
    const auto negativeX = center.x < 0 ? 4U : 0U;
    const auto nearClamp = center.y - radius < nearPlane ? 1U : 0U;
    const auto suffix = rightClipped ? 20U : upperY < 24 ? 24U : 27U;
    return {false, clipped, projectedRadius,
        {{NormalWorkKind::instructions, projectionPrefix + negativeX},
         {NormalWorkKind::instructions, absoluteX < radius ? twoNegativeProjection : oneNegativeProjection},
         {NormalWorkKind::instructions, secondProjectionPrefix + negativeX + nearClamp},
         {NormalWorkKind::instructions, positiveProjection},
         {NormalWorkKind::instructions, suffix}}};
}

namespace {

enum class ModelSetupProfile { level2Constructor, level1AirborneProjectile, level1AxisTank, identityOrYaw, level1Banked, level1Projectile, level1EnemyProjectile, level1CombatBanked, level1CombatClippedBanked, level1Bonus, level1Arrival, level1Deployment, level1CombatYawLift, sharedCombatProjectile, ownedSharedPrecision0, ownedSharedPrecision1, level1Departure, failureFixedWide, level2Failure };

std::uint64_t bankedBuilderDispatches(const std::uint16_t roll, const std::uint16_t yaw) {
    //34AE mode2: the17 table lookups each execute NEG/ADD only on a reflected index.
    const std::array<int, 17> lookupAngles{0xc000,
        roll - yaw, roll + yaw, roll - yaw, roll + yaw,
        yaw + roll + 0xc000, yaw - roll + 0xc000,
        roll + yaw + 0xc000, roll - yaw + 0xc000,
        yaw, -yaw, roll, -roll, roll + 0xc000, roll + 0xc000,
        yaw + 0xc000, yaw + 0xc000};
    constexpr std::uint64_t mode2BaseDispatches = 327;
    constexpr std::uint64_t reflectionDispatches = 2;
    return mode2BaseDispatches + reflectionDispatches * std::count_if(lookupAngles.begin(), lookupAngles.end(),
        [](const int angle) { return ((static_cast<std::uint16_t>(angle) >> 4) & 0x800U) != 0; });
}

WorldModelSetupWork modelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t, 3> actorAngles, const std::uint16_t projectedRadius,
    const std::int16_t type, const Model3d& model, const std::uint16_t registryScale,
    const std::uint16_t registryCap, const std::optional<WorldModelViewInput>& view,
    const ModelSetupProfile profile) {
    constexpr std::uint16_t qualifiedRegistryProfile = 256;
    constexpr std::int64_t detailLimit = 32000;
    constexpr std::uint16_t smallRadiusLimit = 16300;
    constexpr std::int16_t tankType = 1;
    constexpr std::int16_t liftType = 11;
    constexpr std::int16_t obstacleType = 29;
    const auto ownedShared = profile == ModelSetupProfile::ownedSharedPrecision0;
    const auto level1Radius = type == tankType ? 1342U : type == liftType ? 748U : type == obstacleType ? 862U : 0U;
    const auto level1 = level1Radius != 0;
    const auto absoluteX = std::abs(static_cast<std::int64_t>(center.x));
    const auto absoluteY = std::abs(static_cast<std::int64_t>(center.y));
    const auto absoluteZ = std::abs(static_cast<std::int64_t>(center.z));
    const auto maximumAxis = ownedShared ? std::max({absoluteX,absoluteY,absoluteZ})
        : std::max(absoluteX, static_cast<std::int64_t>(center.y));
    const auto reducedPrecision = maximumAxis + model.collisionRadius > detailLimit;
    const auto clippedCombatBanked = profile == ModelSetupProfile::level1CombatClippedBanked;
    const auto combatYawLift = profile == ModelSetupProfile::level1CombatYawLift;
    const auto raisedLift = type == liftType && (center.z == 1000
        || ((clippedCombatBanked || combatYawLift) && center.z >= 0 && center.z <= 1000));
    const auto bonus = profile == ModelSetupProfile::level1Bonus;
    const auto combatBanked = clippedCombatBanked || profile == ModelSetupProfile::level1CombatBanked;
    const auto banked = combatBanked || profile == ModelSetupProfile::level1Banked
        || ((bonus || ownedShared) && actorAngles[1] != 0);
    const auto sharedProjectile = profile == ModelSetupProfile::sharedCombatProjectile;
    const auto primaryProjectile = profile == ModelSetupProfile::level1Projectile || (sharedProjectile && type == 23);
    const auto enemyType=type==17 || (sharedProjectile && type==19);
    const auto enemyProjectile = profile == ModelSetupProfile::level1EnemyProjectile || (sharedProjectile && enemyType);
    if (sharedProjectile && !primaryProjectile && !enemyProjectile) {
        throw std::invalid_argument("unsupported shared combat projectile setup");
    }
    const auto projectile = primaryProjectile || enemyProjectile;
    if (combatYawLift && (type != liftType || actorAngles[0] != 0 || actorAngles[1] != 0
        || reducedPrecision || !view)) {
        throw std::invalid_argument("unsupported yaw-only combat lift setup");
    }
    if (banked && ((!ownedShared && (bonus ? type != 42 : type != liftType)) || actorAngles[0] != 0 || actorAngles[1] == 0
        || reducedPrecision || !view)) {
        throw std::invalid_argument("unsupported banked Level1 model setup");
    }
    if (bonus && (type != 42 || center.z != 100 || actorAngles[0] != 0
        || actorAngles[1] % 0x800 != 0 || actorAngles[2] % 0x800 != 0 || reducedPrecision || !view
        || model.flags != 1 || model.collisionRadius != 330 || model.boundingRadius != 1895
        || model.lods.size() != 1 || model.lods[0].threshold != 0 || model.lods[0].groups.size() != 5)) {
        throw std::invalid_argument("unsupported Level1 bonus model setup");
    }
    if (primaryProjectile && (type != 23 || actorAngles[0] != 0 || actorAngles[1] != 0 || reducedPrecision
        || !view || model.collisionRadius != 134 || model.lods.size() != 1
        || model.lods[0].threshold != 30 || model.lods[0].groups.size() != 1)) {
        throw std::invalid_argument("unsupported Level1 projectile model setup");
    }
    if (enemyProjectile && (!enemyType || actorAngles[0] != 0 || actorAngles[1] != 0 || reducedPrecision
        || !view || model.collisionRadius != 441 || model.lods.size() != 1
        || model.lods[0].threshold != 0 || model.lods[0].groups.size() != 1
        || (type==19 && (model.flags!=0 || model.boundingRadius!=6)))) {
        throw std::invalid_argument("unsupported Level1 enemy projectile model setup");
    }
    const auto viewMatrix = view ? view->matrix : makeFixedMatrixQ14(0, 0, 0);
    if (viewMatrix.mode > 1 || (viewMatrix.mode == 0 && viewMatrix.values != makeFixedMatrixQ14(0, 0, 0).values)
        || (view && !(transformToView(view->relativePosition, {{}, viewMatrix, {}, 0}) == center))
        || (reducedPrecision && viewMatrix.mode != 0)) {
        throw std::invalid_argument("unsupported world model camera path");
    }
    if (ownedShared && (!view || reducedPrecision || actorAngles[0] != 0 || model.flags > 1
        || registryScale != qualifiedRegistryProfile || registryCap != qualifiedRegistryProfile
        || model.scaleShift != 0 || model.origin != std::array<std::int16_t,3>{}
        || model.collisionRadius == 0 || model.collisionRadius >= smallRadiusLimit
        || model.lods.empty() || projectedRadius == 0)) {
        throw std::invalid_argument("unsupported owned Level1 model setup");
    }
    if (!ownedShared && ((!level1 && !projectile && !bonus && type != 8 && type != 13 && type != 30)
        || (level1 && model.collisionRadius != level1Radius) || registryScale != qualifiedRegistryProfile
        || registryCap != qualifiedRegistryProfile || actorAngles[0] != 0 || (!banked && actorAngles[1] != 0)
        || model.scaleShift != 0 || model.origin != std::array<std::int16_t, 3>{}
        || model.collisionRadius == 0 || model.collisionRadius >= smallRadiusLimit || (center.z != 0 && !raisedLift && !bonus)
        || (primaryProjectile ? center.y < 0 : center.y <= 0) || (reducedPrecision && (actorAngles[2] != 0
            || maximumAxis <= smallRadiusLimit || maximumAxis / 2 > smallRadiusLimit))
        || model.lods.empty() || projectedRadius == 0)) {
        throw std::invalid_argument("unsupported world model setup path");
    }
    if (profile == ModelSetupProfile::level1AxisTank && (type != tankType || !view)) {
        throw std::invalid_argument("unsupported Level1 axis tank setup");
    }
    const auto cull = ownedShared || sharedProjectile ? sharedModelSphereCullOperations(center, model)
        : profile == ModelSetupProfile::level1AxisTank ? level1AxisTankModelCullOperations(center)
        : bonus ? level1BonusModelCullOperations(center)
        : primaryProjectile ? level1ProjectileModelCullOperations(center)
        : enemyProjectile ? level1CombatProjectileModelCullOperations(center, type)
        : (clippedCombatBanked || combatYawLift) ? level1CombatClippedLiftModelCullOperations(center)
        : combatBanked ? level1CombatLiftModelCullOperations(center)
        : worldModelCullOperations(center, model.collisionRadius);
    if (cull.culled || cull.projectedRadius != projectedRadius) {
        throw std::invalid_argument("world model setup radius differs from caller");
    }
    auto selectedLod = std::find_if(model.lods.begin(), model.lods.end(),
        [projectedRadius](const auto& lod) { return projectedRadius <= lod.threshold; });
    const auto terminalLod = selectedLod == model.lods.end() && (ownedShared || bonus || projectile || (type == tankType
        && projectedRadius == 256 && model.lods.size() == 3 && model.lods.back().threshold == 255));
    if (terminalLod) {
        selectedLod = model.lods.end() - 1;
    }
    const auto groupLimit = ownedShared ? 16U : bonus ? 5U : type == tankType ? 9U : type == liftType ? 11U : type == obstacleType ? 3U : 8U;
    if (selectedLod == model.lods.end() || selectedLod->flags != 128
        || selectedLod->groups.size() < (ownedShared || projectile ? 1U : 2U) || selectedLod->groups.size() > groupLimit) {
        throw std::invalid_argument("unsupported world model setup LOD path");
    }
    const auto selectedLodIndex = static_cast<std::size_t>(selectedLod - model.lods.begin());

    const auto actorMatrix = makeFixedMatrixQ14(actorAngles[0], actorAngles[1], actorAngles[2]);
    WorldModelSetupWork result{composeFixedMatricesQ14(actorMatrix, viewMatrix, 0x5240),
        center, selectedLodIndex, {}, {}, static_cast<std::uint8_t>(reducedPrecision), 1, {}};
    if (reducedPrecision) {
        const auto halve = [](const std::int32_t value) {
            return value >= 0 ? value / 2 : static_cast<std::int32_t>(-((-std::int64_t{value} + 1) / 2));
        };
        result.translation = {halve(center.x), halve(center.y), halve(center.z)};
    }
    const auto plain = [&result](const std::uint64_t count) {
        result.operations.push_back({NormalWorkKind::instructions, count});
    };
    const auto strings = [&result](const std::size_t count) {
        for (std::size_t index = 0; index < count; ++index) {
            result.operations.push_back({NormalWorkKind::string, 1});
        }
    };
    constexpr std::uint64_t matrixCallerPrefix = 9;
    constexpr std::uint64_t identityBuilderDispatches = 39;
    constexpr std::uint64_t yawBuilderDispatches = 66;
    constexpr std::uint64_t reflectionDispatches = 2;
    plain(matrixCallerPrefix);
    const auto yaw = actorAngles[2];
    const auto transformed = actorMatrix.mode != 0;
    const auto reflected = [](const std::uint16_t angle) { return ((angle >> 4) & 0x800U) != 0; };
    plain(banked ? bankedBuilderDispatches(actorAngles[1], yaw)
        : transformed ? yawBuilderDispatches + reflectionDispatches
        * (static_cast<unsigned>(reflected(yaw)) + static_cast<unsigned>(reflected(static_cast<std::uint16_t>(yaw + 0xc000U))))
        : identityBuilderDispatches);
    if (transformed) {
        constexpr std::uint64_t composeCallerPrefix = 10;
        constexpr std::uint64_t composePrefix = 23;
        constexpr std::uint64_t composeSuffix = 7;
        plain(composeCallerPrefix);
        if (viewMatrix.mode == 0) {
            plain(composePrefix);
            result.operations.push_back({NormalWorkKind::repeatString, 9});
            plain(composeSuffix);
        } else {
            constexpr std::uint64_t yawComposeDispatches = 105;
            constexpr std::uint64_t bankYawComposeDispatches = 140; //38FF's3D06 branch, incomingCX5240.
            plain(banked ? bankYawComposeDispatches : yawComposeDispatches);
        }
    }
    constexpr std::uint64_t lookupCallerPrefix = 6;
    constexpr std::uint64_t sharedLookupDispatches = 31;
    constexpr std::uint64_t lodCallerPrefix = 8;
    plain(lookupCallerPrefix);
    plain(sharedLookupDispatches);
    plain(lodCallerPrefix);

    constexpr std::uint64_t negativeXDispatches = 4;
    constexpr std::uint64_t yawLodPrefix = 91;
    constexpr std::uint64_t identityLodPrefix = 91;
    constexpr std::uint64_t originTransformCaller = 6;
    constexpr std::uint64_t yawVertexDispatches = 55;
    constexpr std::uint64_t fullVertexDispatches = 88;
    const auto vertexDispatches = banked ? fullVertexDispatches : yawVertexDispatches;
    constexpr std::uint64_t yawLodSuffix = 40;
    constexpr std::uint64_t identityLodSuffix = 41;
    constexpr std::uint64_t xDominantSelectionDispatches = 2;
    constexpr std::uint64_t reducedPrecisionSelectionDispatches = 19; //c844..c8a6: precision branch and one shift.
    plain((transformed ? yawLodPrefix : identityLodPrefix) + (center.x < 0 ? negativeXDispatches : 0)
        + (ownedShared && center.y < 0 ? 4 : 0) + (ownedShared && center.z < 0 ? 4 : 0)
        + (absoluteX > (ownedShared ? absoluteY : center.y) ? xDominantSelectionDispatches : 0)
        + (ownedShared && absoluteZ > std::max(absoluteX,absoluteY) ? 4 : 0)
        + (reducedPrecision ? reducedPrecisionSelectionDispatches : 0));
    strings(3);
    if (transformed) {
        plain(originTransformCaller);
        plain(vertexDispatches);
    }
    constexpr std::uint64_t reducedTranslationDispatches = 12; //C8D3..C918/C953..C994: four instructions per axis and shift.
    plain((transformed ? yawLodSuffix : identityLodSuffix)
        + (reducedPrecision ? reducedTranslationDispatches : 0));
    if (selectedLodIndex != 0) {
        constexpr std::uint64_t lodAdvanceDispatches = 8;
        plain(lodAdvanceDispatches * selectedLodIndex);
    }
    if (terminalLod) {
        constexpr std::uint64_t terminalLodDispatches = 5; //C9CC exhaustion and C9D2 select the last entry.
        plain(terminalLodDispatches);
    }
    if (ownedShared ? selectedLod->groups.size() == 1 : projectile) {
        // CC34..CC5A copies the sole group pointer directly; C9DE is not called.
        plain(13);
        result.groupOrder = {0};
        return result;
    }
    constexpr std::uint64_t sortCallerPrefix = 9;
    constexpr std::uint64_t sortPrefix = 19;
    plain(sortCallerPrefix);
    plain(sortPrefix);
    const auto& groups = selectedLod->groups;
    for (std::size_t index = 0; index < groups.size(); ++index) {
        const auto& group = groups[index];
        if (group.originVertex >= group.vertices.size()) {
            throw std::invalid_argument("world group origin outside immutable vertices");
        }
        const auto& vertex = group.vertices[group.originVertex];
        const auto origin = transformed ? transformToView({vertex[0], vertex[1], vertex[2]}, {{}, result.matrix, {}, 0})
            : FixedPosition{vertex[0], vertex[1], vertex[2]};
        if (origin.x < -32768 || origin.x > 32767 || origin.y < -32768 || origin.y > 32767
            || origin.z < -32768 || origin.z > 32767) {
            throw std::invalid_argument("unsupported world group origin range");
        }
        const auto sortPosition = !transformed && view && viewMatrix.mode != 0 ? view->relativePosition : result.translation;
        const auto depth = static_cast<std::uint32_t>(static_cast<std::int64_t>(origin.x) * sortPosition.x
            + static_cast<std::int64_t>(origin.y) * sortPosition.y
            + static_cast<std::int64_t>(origin.z) * sortPosition.z);
        result.groupOrder.push_back(index);
        result.groupDepths.push_back(static_cast<std::int32_t>(depth));
        constexpr std::uint64_t groupCopyPrefix = 24;
        constexpr std::uint64_t groupTransformCaller = 9;
        constexpr std::uint64_t transformedDotCaller = 6;
        constexpr std::uint64_t directDotCaller = 7;
        constexpr std::uint64_t groupSuffix = 8;
        plain(groupCopyPrefix);
        strings(3);
        if (transformed) {
            plain(groupTransformCaller);
            plain(vertexDispatches);
        }
        plain(transformed ? transformedDotCaller : directDotCaller);
        plain(5); strings(1); plain(4); strings(1); plain(4); strings(1); plain(7);
        plain(groupSuffix);
        if (index + 1 < groups.size()) {
            plain(1);
        }
    }
    constexpr std::uint64_t selectionPrefix = 4;
    constexpr std::uint64_t selectionOuterPrefix = 5;
    constexpr std::uint64_t selectionInnerPrefix = 6;
    constexpr std::uint64_t depthComparePrefix = 7;
    constexpr std::uint64_t selectionInnerSuffix = 4;
    constexpr std::uint64_t swapPrefix = 4;
    constexpr std::uint64_t swapDispatches = 26;
    constexpr std::uint64_t selectionOuterSuffix = 7;
    constexpr std::uint64_t sortReturnDispatches = 5;
    plain(selectionPrefix);
    for (std::size_t destination = 0; destination < groups.size(); ++destination) {
        plain(selectionOuterPrefix);
        auto selected = destination;
        if (destination + 1 < groups.size()) {
            plain(selectionInnerPrefix);
        }
        for (std::size_t candidate = destination + 1; candidate < groups.size(); ++candidate) {
            const auto selectedDepth = result.groupDepths[result.groupOrder[selected]];
            const auto candidateDepth = result.groupDepths[result.groupOrder[candidate]];
            const auto selectedHigh = static_cast<std::int16_t>(static_cast<std::uint32_t>(selectedDepth) >> 16);
            const auto candidateHigh = static_cast<std::int16_t>(static_cast<std::uint32_t>(candidateDepth) >> 16);
            plain(depthComparePrefix);
            if (selectedHigh < candidateHigh) {
                plain(2);
                selected = candidate;
            } else if (selectedHigh == candidateHigh) {
                plain(3);
                if (static_cast<std::uint16_t>(selectedDepth) < static_cast<std::uint16_t>(candidateDepth)) {
                    plain(1);
                    selected = candidate;
                }
            }
            plain(selectionInnerSuffix);
        }
        plain(swapPrefix);
        if (selected != destination) {
            plain(swapDispatches);
            std::swap(result.groupOrder[selected], result.groupOrder[destination]);
        }
        plain(selectionOuterSuffix);
        if (destination + 1 < groups.size()) {
            plain(1);
        }
    }
    plain(sortReturnDispatches);
    return result;
}


WorldModelSetupWork failureSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t, 3> actorAngles, const std::uint16_t projectedRadius,
    const std::int16_t type, const Model3d& model, const std::uint16_t registryScale,
    const std::uint16_t registryCap, const std::optional<WorldModelViewInput>& view,
    const ModelSetupProfile profile) {
    constexpr std::uint16_t qualifiedRegistryProfile = 256;
    constexpr std::int64_t detailLimit = 32000;
    const auto absolute = [profile](const std::int32_t value) {
        return (profile==ModelSetupProfile::ownedSharedPrecision1 || profile==ModelSetupProfile::level1Departure || profile==ModelSetupProfile::failureFixedWide || profile==ModelSetupProfile::level2Failure) && value==-32768
            ? std::int64_t{32767} : std::abs(std::int64_t{value});
    };
    const auto absoluteX = absolute(center.x);
    const auto absoluteY = absolute(center.y);
    const auto absoluteZ = absolute(center.z);
    const auto maximumAxis = std::max({absoluteX, absoluteY, absoluteZ});
    const auto reducedPrecision = maximumAxis + model.collisionRadius > detailLimit;
    const auto level2Failure=profile==ModelSetupProfile::level2Failure;
    const auto fixedFailureWide=profile==ModelSetupProfile::failureFixedWide;
    const auto ownedWide=profile==ModelSetupProfile::ownedSharedPrecision1 || fixedFailureWide;
    const auto arrival=profile==ModelSetupProfile::level1Arrival;
    const auto airborne=profile==ModelSetupProfile::level1AirborneProjectile;
    const auto departure=profile==ModelSetupProfile::level1Departure;
    const auto deployment=profile==ModelSetupProfile::level1Deployment || departure;
    const auto level2=profile==ModelSetupProfile::level2Constructor;
    const auto guardian=deployment && type==44;
    constexpr std::int64_t sortAxisLimit = 0x3FAC; //C877/C89F: setup sorting normalization.
    unsigned sortShifts = reducedPrecision ? 1U : 0U;
    if (deployment || level2 || ownedWide || level2Failure) {
        sortShifts = 0;
        auto axis = maximumAxis;
        while (reducedPrecision && axis > sortAxisLimit) {
            axis >>= 1;
            ++sortShifts;
        }
    }
    const auto sortCenter = [sortShifts](const FixedPosition position) {
        const auto divisor = std::int64_t{1} << sortShifts;
        const auto shifted = [divisor](const std::int32_t value) {
            return static_cast<std::int32_t>(value >= 0 ? value / divisor
                : -((-std::int64_t{value} + divisor - 1) / divisor));
        };
        return FixedPosition{shifted(position.x),shifted(position.y),shifted(position.z)};
    };
    constexpr std::array<std::int16_t, 10> types{0, 1, 11, 23, 29, 36, 37, 38, 39, 40};
    constexpr std::array<std::uint32_t, 10> radii{723, 1342, 748, 134, 862, 171, 107, 88, 107, 157};
    const auto found = std::find(types.begin(), types.end(), type);
    const auto arrivalModel=type==7 && model.flags==1 && model.collisionRadius==13052
        && model.boundingRadius==2522 && model.lods.size()==1 && model.lods[0].threshold==0
        && model.lods[0].groups.size()==9 && actorAngles[0]==0 && center.z==5000;
    const auto guardianModel=type==44 && model.flags==3 && model.collisionRadius==2792
        && model.boundingRadius==16226 && model.animationLimits==std::vector<std::uint8_t>{8,8,8}
        && model.lods.size()==1 && model.lods[0].threshold==0 && model.lods[0].groups.size()==13;
    const auto departureModel=type==7 && model.flags==1 && model.collisionRadius==13052
        && model.boundingRadius==2522 && model.lods.size()==1 && model.lods[0].threshold==0
        && model.lods[0].groups.size()==9;
    const auto rewardModel=type==25 && model.flags==1 && model.collisionRadius==897
        && model.boundingRadius==2265 && model.animationLimits==std::vector<std::uint8_t>{4}
        && model.lods.size()==1 && model.lods[0].threshold==0 && model.lods[0].groups.size()==6;
    const auto deployedProjectile=type==22 && model.flags==1 && model.collisionRadius==134
        && model.boundingRadius==485 && model.lods.size()==1 && model.lods[0].threshold==30
        && model.lods[0].groups.size()==1;
    const auto airborneModel=type==20 && model.flags==1 && model.collisionRadius==134
        && model.boundingRadius==471 && model.lods.size()==1 && model.lods[0].threshold==30
        && model.lods[0].groups.size()==1 && model.lods[0].groups[0].vertices.size()==33
        && model.lods[0].groups[0].animationOffset==0 && model.lods[0].groups[0].primitives.size()==2
        && actorAngles[0]==0 && center.z>=0 && center.z<=1000;
    const auto level2Model=(type==8 || type==13 || type==30) && actorAngles[0]==0 && actorAngles[1]==0
        && center.z==0 && model.collisionRadius==(type==8 ? 860U : type==13 ? 559U : 868U);
    constexpr std::array<std::int16_t,12> level2FailureTypes{0,4,8,13,19,23,30,36,37,38,39,40};
    constexpr std::array<std::uint32_t,12> level2FailureRadii{723,928,860,559,441,134,868,171,107,88,107,157};
    const auto failureType=std::find(level2FailureTypes.begin(),level2FailureTypes.end(),type);
    const auto level2FailureModel=failureType!=level2FailureTypes.end() && model.flags<=1
        && model.collisionRadius==level2FailureRadii[static_cast<std::size_t>(failureType-level2FailureTypes.begin())];
    constexpr std::int64_t sourceSortWordLimit=0xffff; //C877 high word0 normalization family.
    if ((fixedFailureWide && (type!=retainedFailureFixedType || actorAngles!=std::array<std::uint16_t,3>{} || model.collisionRadius!=retainedFailureFixedRadius || model.flags!=0))
        || (level2Failure ? !level2FailureModel : ownedWide ? (actorAngles[0]!=0 || model.flags>1 || !reducedPrecision) : level2 ? !level2Model : airborne ? !airborneModel : arrival ? !arrivalModel : departure ? !departureModel : deployment ? !(guardianModel || departureModel || deployedProjectile || rewardModel) : found==types.end()) || registryScale != qualifiedRegistryProfile || registryCap != qualifiedRegistryProfile
        || model.scaleShift != 0 || model.origin != std::array<std::int16_t, 3>{}
        || (!level2Failure && !ownedWide && !level2 && !airborne && !arrival && !deployment && model.collisionRadius != radii[static_cast<std::size_t>(found - types.begin())])
        || (departure && maximumAxis>sourceSortWordLimit)
        || (!departure && (deployment || level2 || ownedWide || level2Failure ? maximumAxis+model.collisionRadius>64000 : arrival ? maximumAxis>32600 : reducedPrecision))
        || !view || view->matrix.mode > (level2 || airborne || arrival || deployment || ownedWide ? 1 : 3) || model.lods.empty() || projectedRadius == 0
        || !(transformToView(view->relativePosition, {{}, view->matrix, {}, 0}) == center)) {
        throw std::invalid_argument("unsupported failure model setup profile");
    }
    const auto viewMatrix = view->matrix;
    const auto cull = level2Failure ? level2FailureModelCullOperations(center,model)
        : fixedFailureWide ? failureFixedWideCullOperations(center,model)
        : ownedWide ? level1OwnedWideModelCullOperations(center,model)
        : level2 && !reducedPrecision ? sharedModelSphereCullOperations(center,model)
        : level2 ? worldModelCullOperations(center,model.collisionRadius)
        : airborne ? level1AirborneProjectileModelCullOperations(center)
        : deployment ? level1DeploymentModelCullOperations(center,type)
        : arrival ? level1ArrivalModelCullOperations(center)
        : failureModelCullOperations(center, model.collisionRadius, {0, 318, 0, 199});
    if (cull.culled || (fixedFailureWide && cull.clipped) || cull.projectedRadius != projectedRadius) {
        throw std::invalid_argument("failure setup differs from source sphere decision");
    }
    auto selectedLod = std::find_if(model.lods.begin(), model.lods.end(),
        [projectedRadius](const auto& lod) { return projectedRadius <= lod.threshold; });
    const auto terminalLod = selectedLod == model.lods.end();
    if (terminalLod) {
        selectedLod = model.lods.end() - 1;
    }
    if (selectedLod->flags != (guardian ? 0 : 128) || selectedLod->groups.empty() || selectedLod->groups.size() > 16) {
        throw std::invalid_argument("unsupported failure model LOD metadata");
    }
    const auto selectedLodIndex = static_cast<std::size_t>(selectedLod - model.lods.begin());

    const auto actorMatrix = makeFixedMatrixQ14(actorAngles[0], actorAngles[1], actorAngles[2]);
    WorldModelSetupWork result{composeFixedMatricesQ14(actorMatrix, viewMatrix, 0x5240),
        center, selectedLodIndex, {}, {}, static_cast<std::uint8_t>(reducedPrecision), 1, {}};
    if (ownedWide || departure || (level2Failure && reducedPrecision)) {
        result.translation=sortCenter(actorMatrix.mode==0 && viewMatrix.mode!=0 ? view->relativePosition : center);
    } else if (reducedPrecision) {
        const auto halve = [](const std::int32_t value) {
            return value >= 0 ? value / 2 : static_cast<std::int32_t>(-((-std::int64_t{value} + 1) / 2));
        };
        result.translation = {halve(center.x), halve(center.y), halve(center.z)};
    }
    const auto plain = [&result](const std::uint64_t count) {
        result.operations.push_back({NormalWorkKind::instructions, count});
    };
    const auto strings = [&result](const std::size_t count) {
        for (std::size_t index = 0; index < count; ++index) {
            result.operations.push_back({NormalWorkKind::string, 1});
        }
    };
    constexpr std::uint64_t matrixCallerPrefix = 9;
    plain(matrixCallerPrefix);
    const auto built = fixedMatrixBuildOperations(actorAngles);
    result.operations.insert(result.operations.end(), built.operations.begin(), built.operations.end());
    const auto transformed = actorMatrix.mode != 0;
    if (transformed) {
        plain(10); //CBF2..CC0D compose caller, including CALLF.
        const auto composed = fixedMatrixComposeOperations(actorMatrix, viewMatrix, 0x5240);
        result.operations.insert(result.operations.end(), composed.operations.begin(), composed.operations.end());
    }
    constexpr std::uint64_t lookupCallerPrefix = 6;
    constexpr std::uint64_t sharedLookupDispatches = 31;
    constexpr std::uint64_t lodCallerPrefix = 8;
    plain(lookupCallerPrefix);
    plain(sharedLookupDispatches+(guardian ? 13 : 0)); //C114 skips shared44 before local0.
    plain(lodCallerPrefix);

    constexpr std::uint64_t negativeXDispatches = 4;
    constexpr std::uint64_t yawLodPrefix = 91;
    constexpr std::uint64_t identityLodPrefix = 91;
    constexpr std::uint64_t originTransformCaller = 6;
    constexpr std::uint64_t yawVertexDispatches = 55;
    constexpr std::uint64_t fullVertexDispatches = 88;
    const auto vertexDispatches = result.matrix.mode >= 2 ? fullVertexDispatches : yawVertexDispatches;
    constexpr std::uint64_t yawLodSuffix = 40;
    constexpr std::uint64_t identityLodSuffix = 41;
    constexpr std::uint64_t xDominantSelectionDispatches = 2;
    constexpr std::uint64_t reducedPrecisionSelectionDispatches = 19; //C844..C8A6: precision branch and first shift.
    constexpr std::uint64_t sortShiftDispatches = 8; //C890..C8A4: another SAR/RCR iteration, high word zero.
    constexpr std::uint64_t precisionWithoutSortShiftDispatches = 4; //C844..C87C: precision1, normalized axis already <=3FAC.
    const auto signedSelection = [ownedWide, departure, level2Failure, negativeXDispatches](const std::int32_t value) -> std::uint64_t {
        if (departure && value<0) {
            return value==-32768 ? 2 : static_cast<std::uint16_t>(value)==0x8000 ? 6 : negativeXDispatches;
        }
        return value>=0 ? 0 : (ownedWide || level2Failure) && value==-32768 ? 2 : negativeXDispatches;
    };
    const auto precisionSelection = (ownedWide || level2Failure) && sortShifts==0 ? precisionWithoutSortShiftDispatches : reducedPrecisionSelectionDispatches;
    plain((transformed ? yawLodPrefix : identityLodPrefix) + signedSelection(center.x) + signedSelection(center.y) + signedSelection(center.z)
        + (absoluteX > absoluteY ? xDominantSelectionDispatches : 0) + (absoluteZ > std::max(absoluteX, absoluteY) ? 4 : 0)
        + (reducedPrecision ? precisionSelection : 0)
        + (sortShifts > 1 ? (sortShifts - 1) * sortShiftDispatches : 0));
    strings(3);
    if (transformed) {
        plain(originTransformCaller);
        plain(vertexDispatches);
    }
    constexpr std::uint64_t reducedTranslationDispatches = 12; //C8D3..C918/C953..C994: four instructions per axis and shift.
    plain((transformed ? yawLodSuffix : identityLodSuffix)
        + sortShifts * reducedTranslationDispatches);
    if (selectedLodIndex != 0) {
        constexpr std::uint64_t lodAdvanceDispatches = 8;
        plain(lodAdvanceDispatches * selectedLodIndex);
    }
    if (terminalLod) {
        constexpr std::uint64_t terminalLodDispatches = 5; //C9CC exhaustion and C9D2 select the last entry.
        plain(terminalLodDispatches);
    }
    if (selectedLod->groups.size() == 1) {
        // CC34..CC5A copies the sole group pointer directly; C9DE is not called.
        plain(13);
        result.groupOrder = {0};
        return result;
    }
    constexpr std::uint64_t sortCallerPrefix = 9;
    constexpr std::uint64_t sortPrefix = 19;
    plain(sortCallerPrefix);
    plain(sortPrefix);
    const auto& groups = selectedLod->groups;
    for (std::size_t index = 0; index < groups.size(); ++index) {
        const auto& group = groups[index];
        if (group.originVertex >= group.vertices.size()) {
            throw std::invalid_argument("world group origin outside immutable vertices");
        }
        const auto& vertex = group.vertices[group.originVertex];
        const auto origin = transformed ? transformToView({vertex[0], vertex[1], vertex[2]}, {{}, result.matrix, {}, 0})
            : FixedPosition{vertex[0], vertex[1], vertex[2]};
        if (origin.x < -32768 || origin.x > 32767 || origin.y < -32768 || origin.y > 32767
            || origin.z < -32768 || origin.z > 32767) {
            throw std::invalid_argument("unsupported world group origin range");
        }
        //C8D3..C918/C953..C994 use the setup shift count, independently of vertex precision.
        const auto sortPosition = deployment || level2 || ownedWide || level2Failure ? sortCenter(!transformed && viewMatrix.mode != 0
            ? view->relativePosition : center)
            : !transformed && view && viewMatrix.mode != 0 ? view->relativePosition : result.translation;
        const auto depth = static_cast<std::uint32_t>(static_cast<std::int64_t>(origin.x) * sortPosition.x
            + static_cast<std::int64_t>(origin.y) * sortPosition.y
            + static_cast<std::int64_t>(origin.z) * sortPosition.z);
        result.groupOrder.push_back(index);
        result.groupDepths.push_back(static_cast<std::int32_t>(depth));
        constexpr std::uint64_t groupCopyPrefix = 24;
        constexpr std::uint64_t groupTransformCaller = 9;
        constexpr std::uint64_t transformedDotCaller = 6;
        constexpr std::uint64_t directDotCaller = 7;
        constexpr std::uint64_t groupSuffix = 8;
        plain(groupCopyPrefix);
        strings(3);
        if (transformed) {
            plain(groupTransformCaller);
            plain(vertexDispatches);
        }
        plain(transformed ? transformedDotCaller : directDotCaller);
        plain(5); strings(1); plain(4); strings(1); plain(4); strings(1); plain(7);
        plain(groupSuffix);
        if (index + 1 < groups.size()) {
            plain(1);
        }
    }
    constexpr std::uint64_t selectionPrefix = 4;
    constexpr std::uint64_t selectionOuterPrefix = 5;
    constexpr std::uint64_t selectionInnerPrefix = 6;
    constexpr std::uint64_t depthComparePrefix = 7;
    constexpr std::uint64_t selectionInnerSuffix = 4;
    constexpr std::uint64_t swapPrefix = 4;
    constexpr std::uint64_t swapDispatches = 26;
    constexpr std::uint64_t selectionOuterSuffix = 7;
    constexpr std::uint64_t sortReturnDispatches = 5;
    plain(selectionPrefix);
    for (std::size_t destination = 0; destination < groups.size(); ++destination) {
        plain(selectionOuterPrefix);
        auto selected = destination;
        if (destination + 1 < groups.size()) {
            plain(selectionInnerPrefix);
        }
        for (std::size_t candidate = destination + 1; candidate < groups.size(); ++candidate) {
            const auto selectedDepth = result.groupDepths[result.groupOrder[selected]];
            const auto candidateDepth = result.groupDepths[result.groupOrder[candidate]];
            const auto selectedHigh = static_cast<std::int16_t>(static_cast<std::uint32_t>(selectedDepth) >> 16);
            const auto candidateHigh = static_cast<std::int16_t>(static_cast<std::uint32_t>(candidateDepth) >> 16);
            plain(depthComparePrefix);
            if (selectedHigh < candidateHigh) {
                plain(2);
                selected = candidate;
            } else if (selectedHigh == candidateHigh) {
                plain(3);
                if (static_cast<std::uint16_t>(selectedDepth) < static_cast<std::uint16_t>(candidateDepth)) {
                    plain(1);
                    selected = candidate;
                }
            }
            plain(selectionInnerSuffix);
        }
        plain(swapPrefix);
        if (selected != destination) {
            plain(swapDispatches);
            std::swap(result.groupOrder[selected], result.groupOrder[destination]);
        }
        plain(selectionOuterSuffix);
        if (destination + 1 < groups.size()) {
            plain(1);
        }
    }
    plain(sortReturnDispatches);
    return result;
}

}

WorldModelSetupWork worldModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t, 3> actorAngles, const std::uint16_t projectedRadius,
    const std::int16_t type, const Model3d& model, const std::uint16_t registryScale,
    const std::uint16_t registryCap, const std::optional<WorldModelViewInput>& view) {
    return modelSetupOperations(center, actorAngles, projectedRadius, type, model, registryScale,
        registryCap, view, ModelSetupProfile::identityOrYaw);
}

WorldModelSetupWork level1AxisTankModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t, 3> angles, const std::uint16_t radius,
    const Model3d& model, const std::uint16_t scale, const std::uint16_t cap,
    const WorldModelViewInput& view) {
    return modelSetupOperations(center,angles,radius,1,model,scale,cap,view,ModelSetupProfile::level1AxisTank);
}

WorldModelSetupWork level1BankedModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t, 3> actorAngles, const std::uint16_t projectedRadius,
    const std::int16_t type, const Model3d& model, const std::uint16_t registryScale,
    const std::uint16_t registryCap, const WorldModelViewInput& view) {
    return modelSetupOperations(center, actorAngles, projectedRadius, type, model, registryScale,
        registryCap, view, ModelSetupProfile::level1Banked);
}

WorldModelSetupWork level1ProjectileModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t, 3> actorAngles, const std::uint16_t projectedRadius,
    const Model3d& model, const std::uint16_t registryScale, const std::uint16_t registryCap,
    const WorldModelViewInput& view) {
    return modelSetupOperations(center, actorAngles, projectedRadius, 23, model, registryScale,
        registryCap, view, ModelSetupProfile::level1Projectile);
}

WorldModelSetupWork level1CombatLiftModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t, 3> actorAngles, const std::uint16_t projectedRadius,
    const Model3d& model, const std::uint16_t registryScale, const std::uint16_t registryCap,
    const WorldModelViewInput& view) {
    return modelSetupOperations(center, actorAngles, projectedRadius, 11, model, registryScale,
        registryCap, view, ModelSetupProfile::level1CombatBanked);
}

WorldModelSetupWork level1CombatClippedLiftModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t, 3> actorAngles, const std::uint16_t projectedRadius,
    const Model3d& model, const std::uint16_t registryScale, const std::uint16_t registryCap,
    const WorldModelViewInput& view) {
    return modelSetupOperations(center, actorAngles, projectedRadius, 11, model, registryScale,
        registryCap, view, ModelSetupProfile::level1CombatClippedBanked);
}

WorldModelSetupWork level1CombatYawLiftModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t, 3> actorAngles, const std::uint16_t projectedRadius,
    const Model3d& model, const std::uint16_t registryScale, const std::uint16_t registryCap,
    const WorldModelViewInput& view) {
    return modelSetupOperations(center, actorAngles, projectedRadius, 11, model, registryScale,
        registryCap, view, ModelSetupProfile::level1CombatYawLift);
}

WorldModelCullWork level1CombatProjectileModelCullOperations(const FixedPosition center, const std::int16_t type) {
    if (type == 23) {
        constexpr std::int64_t radius = 134;
        constexpr std::int64_t nearPlane = 128;
        constexpr std::int64_t perspectiveScale = 128;
        constexpr std::int64_t signedWordLimit = 32768;
        const auto minimumDepth = std::max(nearPlane, static_cast<std::int64_t>(center.y) - radius);
        if (center.z == 0 && center.y >= 0 && std::abs(static_cast<std::int64_t>(center.x)) <= radius
            && static_cast<std::int64_t>(center.y) + radius < signedWordLimit
            && radius * perspectiveScale / minimumDepth == 0) {
            // CBB1..CBB8 returns through CFB5 before projection/setup/geometry.
            constexpr std::uint64_t zeroRadiusCullDispatches = 27;
            return {true, false, 0, {{NormalWorkKind::instructions, zeroRadiusCullDispatches}}};
        }
        return level1ProjectileModelCullOperations(center);
    }
    if (type != 17 || center.z != 0) {
        throw std::invalid_argument("unsupported Level1 combat projectile sphere");
    }
    auto result = worldModelCullOperations(center, 441);
    if (result.projectedRadius == 0) {
        throw std::invalid_argument("unsupported zero-radius Level1 combat projectile sphere");
    }
    return result;
}

WorldModelCullWork level1BonusModelCullOperations(const FixedPosition center) {
    constexpr std::uint32_t radius = 330;
    if (center.z != 100) { throw std::invalid_argument("unsupported Level1 bonus altitude"); }
    const auto denominator = std::max<std::int64_t>(128, std::int64_t{center.y} - radius);
    if (radius * 128 / denominator == 0) {
        return {true,false,0,{{NormalWorkKind::instructions,27}}}; //CC5E zero-radius early return.
    }
    return modelCullOperations(center,radius,ModelCullProfile::level1Bonus);
}

WorldModelSetupWork level1BonusModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t,3> actorAngles, const std::uint16_t projectedRadius,
    const Model3d& model, const std::uint16_t registryScale, const std::uint16_t registryCap,
    const WorldModelViewInput& view) {
    return modelSetupOperations(center,actorAngles,projectedRadius,42,model,registryScale,
        registryCap,view,ModelSetupProfile::level1Bonus);
}

WorldModelSetupWork level1CombatProjectileModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t, 3> actorAngles, const std::uint16_t projectedRadius, const std::int16_t type,
    const Model3d& model, const std::uint16_t registryScale, const std::uint16_t registryCap,
    const WorldModelViewInput& view) {
    if (type == 23) {
        return level1ProjectileModelSetupOperations(center, actorAngles, projectedRadius,
            model, registryScale, registryCap, view);
    }
    return modelSetupOperations(center, actorAngles, projectedRadius, type, model, registryScale,
        registryCap, view, ModelSetupProfile::level1EnemyProjectile);
}

WorldModelSetupWork level1SharedCombatProjectileModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t, 3> actorAngles, const std::uint16_t projectedRadius, const std::int16_t type,
    const Model3d& model, const std::uint16_t registryScale, const std::uint16_t registryCap,
    const WorldModelViewInput& view) {
    return modelSetupOperations(center, actorAngles, projectedRadius, type, model, registryScale,
        registryCap, view, ModelSetupProfile::sharedCombatProjectile);
}

namespace {

std::vector<NormalWorkOperation> bitmapOperations(const IndexedBitmap& bitmap, const std::int16_t x,
    const PlanarBlitMode mode) {
    using Kind = NormalWorkKind;
    std::vector<NormalWorkOperation> operations;
    const auto plain = [&operations](const std::uint64_t count) {
        operations.push_back({Kind::instructions, count});
    };
    constexpr std::uint64_t portSetupDispatches = 2;
    if (mode == PlanarBlitMode::opaque) {
        constexpr std::uint64_t prefixDispatches = 17;
        constexpr std::uint64_t rowAddressSetupDispatches = 8;
        constexpr std::uint64_t rowTailDispatches = 3;
        constexpr std::uint64_t returnDispatches = 5;
        plain(prefixDispatches);
        operations.push_back({Kind::port16Write, 1});
        plain(portSetupDispatches);
        operations.push_back({Kind::port16Write, 1});
        plain(rowAddressSetupDispatches);
        for (std::uint16_t row = 0; row < bitmap.height; ++row) {
            plain(1);
            operations.push_back({Kind::repeatString, static_cast<std::uint64_t>(bitmap.width / pixelsPerPlanarByte)});
            plain(rowTailDispatches);
        }
        plain(portSetupDispatches);
        operations.push_back({Kind::port16Write, 1});
        plain(returnDispatches);
        return operations;
    }

    constexpr std::uint64_t headerSetupDispatches = 13;
    constexpr std::uint64_t headerCopyDispatches = 1;
    constexpr std::uint64_t transparentPlaneSetupDispatches = 17;
    constexpr std::uint64_t transparentFirstPlaneDispatches = 11;
    constexpr std::uint64_t transparentPlaneSwitchDispatches = 11;
    constexpr std::uint64_t transparentNextPlaneDispatches = 14;
    constexpr std::uint64_t transparentRowSetupDispatches = 2;
    constexpr std::uint64_t transparentRowTailDispatches = 5;
    constexpr std::uint64_t transparentPixelTailDispatches = 3;
    constexpr std::uint64_t transparentReturnDispatches = 8;
    constexpr std::uint64_t generalFirstPlaneDispatches = 9;
    constexpr std::uint64_t generalFirstSourceDispatches = 34;
    constexpr std::uint64_t generalPlaneSwitchDispatches = 30;
    constexpr std::uint64_t generalNextPlaneDispatches = 14;
    constexpr std::uint64_t generalEmptyPlaneSkippedDispatches = 2;
    constexpr std::uint64_t generalRowSetupDispatches = 1;
    constexpr std::uint64_t generalRowTailDispatches = 4;
    constexpr std::uint64_t generalPixelTailDispatches = 2;
    constexpr std::uint64_t generalReturnDispatches = 10;
    constexpr std::uint64_t pixelBranchDispatches = 2;
    plain(headerSetupDispatches);
    operations.push_back({Kind::repeatString, bitmapHeaderWords});
    auto prefix = mode == PlanarBlitMode::transparent ? transparentBlitPrefixDispatches : generalBlitPrefixDispatches;
    if (mode == PlanarBlitMode::transparent && x == 0) {
        prefix -= positiveXPrefixDispatches;
    }
    plain(prefix - headerSetupDispatches - headerCopyDispatches);
    operations.push_back({mode == PlanarBlitMode::transparent ? Kind::port16Write : Kind::port8Write, 1});
    plain(mode == PlanarBlitMode::transparent ? transparentPlaneSetupDispatches : 1);
    for (std::uint16_t plane = 0; plane < pixelsPerPlanarByte; ++plane) {
        const auto destinationRollover = (x % pixelsPerPlanarByte + plane) == pixelsPerPlanarByte;
        if (mode == PlanarBlitMode::transparent) {
            if (plane == 0) {
                plain(transparentFirstPlaneDispatches);
            } else {
                plain(transparentPlaneSwitchDispatches
                    + (destinationRollover ? transparentPlaneRolloverDispatches : 0));
                operations.push_back({Kind::port8Write, 1});
                plain(transparentNextPlaneDispatches);
            }
        } else {
            // Source-plane address selection executes one additional ADD per plane.
            plain(plane == 0 ? generalFirstPlaneDispatches : generalPlaneSwitchDispatches + plane
                + (destinationRollover ? generalPlaneRolloverDispatches : 0));
            operations.push_back({Kind::port8Write, 1});
            plain(plane == 0 ? generalFirstSourceDispatches : generalNextPlaneDispatches
                - (plane >= bitmap.width ? generalEmptyPlaneSkippedDispatches : 0));
            if (plane >= bitmap.width) {
                continue;
            }
        }
        for (std::uint16_t row = 0; row < bitmap.height; ++row) {
            plain(mode == PlanarBlitMode::transparent ? transparentRowSetupDispatches : generalRowSetupDispatches);
            for (std::uint16_t column = plane; column < bitmap.width; column += pixelsPerPlanarByte) {
                operations.push_back({Kind::string, 1});
                plain(pixelBranchDispatches);
                if (bitmap.pixels[static_cast<std::size_t>(row) * bitmap.width + column] != 0) {
                    operations.push_back({Kind::string, 1});
                    plain(1);
                }
                plain(mode == PlanarBlitMode::transparent ? transparentPixelTailDispatches : generalPixelTailDispatches);
            }
            plain(mode == PlanarBlitMode::transparent ? transparentRowTailDispatches : generalRowTailDispatches);
        }
    }
    plain(mode == PlanarBlitMode::transparent ? transparentReturnDispatches : generalReturnDispatches);
    return operations;
}

}  // namespace

std::vector<NormalWorkOperation> planarBitmapOperations(const IndexedBitmap& bitmap, const std::int16_t x,
    const PlanarBlitMode mode) {
    planarBitmapWork(bitmap, x, mode);
    return bitmapOperations(bitmap, x, mode);
}

SpanWork solidSpanWork(const std::vector<RasterSpan>& rows) {
    if (rows.empty() || rows.size() > static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max())) {
        throw std::invalid_argument("solid span work requires a positive signed16 row count");
    }
    SpanWork work{spanHeaderDispatches, 2, 0};
    for (const auto row : rows) {
        work.dispatches += spanRowLoadDispatches;
        work.stringIterations += 2;
        const auto delta = static_cast<std::uint16_t>(row.right - row.left);
        if (row.right < row.left) {
            ++work.dispatches;
        } else {
            work.dispatches += spanDrawSetupDispatches;
            const auto left = static_cast<std::uint16_t>(row.left);
            const auto right = static_cast<std::uint16_t>(row.right);
            if (((left ^ right) & planarGroupMask) == 0) {
                work.dispatches += spanSingleGroupDispatches;
                ++work.stringIterations;
                ++work.portInstructions;
            } else {
                const auto bulkPixels = static_cast<std::uint16_t>(delta + 1 - 4 + (left & 3));
                const auto oddByte = (bulkPixels >> 2) & 1;
                work.dispatches += spanBulkDispatches + oddByte;
                work.stringIterations += 2 + oddByte + (bulkPixels >> 3);
                work.portInstructions += 3;
            }
        }
        work.dispatches += spanLoopDispatches;
    }
    work.dispatches += spanNextRowDispatches * (rows.size() - 1) + spanReturnDispatches;
    return work;
}

std::uint64_t vertexCacheDispatches(const VertexCacheState state, const std::uint16_t matrixMode) noexcept {
    if (state == VertexCacheState::Populated) {
        return cachedVertexDispatches;
    }
    const auto mode = matrixMode & 0xFF;
    if (mode == 0) {
        return vertexMissWrapperDispatches + shortIdentityDispatches;
    }
    return vertexMissWrapperDispatches + (mode == 1 ? shortTwoAxisDispatches : shortThreeAxisDispatches);
}

std::uint64_t vectorTransformDispatches(
    const FixedPosition relativePosition,
    const FixedMatrixQ14& matrix
) noexcept {
    const auto mode = matrix.mode & 0xFF;
    if (mode == 0) {
        return identityDispatches;
    }
    const auto outputs = mode == 1 ? 2U : 3U;
    auto dispatches = mode == 1 ? twoAxisWrapperDispatches : threeAxisWrapperDispatches;
    for (unsigned output = 0; output < outputs; ++output) {
        dispatches += twoTermDotDispatches;
        dispatches += productSignDispatches(relativePosition.x, matrix.values[output]);
        dispatches += productSignDispatches(relativePosition.y, matrix.values[output + 3]);
        // The original skips only a zero third coefficient, not a zero coordinate.
        const auto thirdCoefficient = matrix.values[output + 6];
        if (thirdCoefficient != 0) {
            dispatches += thirdTermDispatches;
            dispatches += productSignDispatches(relativePosition.z, thirdCoefficient);
        }
    }
    return dispatches;
}


FixedMatrixWork fixedMatrixBuildOperations(const std::array<std::uint16_t, 3> angles) {
    const auto matrix = makeFixedMatrixQ14(angles[0], angles[1], angles[2]);
    const auto reflected = [](const int angle) {
        return ((static_cast<std::uint16_t>(angle) >> 4) & 0x800U) != 0;
    };
    std::uint64_t count = 39;
    if (matrix.mode == 1) {
        count = 66 + 2 * (reflected(angles[2]) + reflected(angles[2] + 0xc000));
    } else if (matrix.mode >= 2) {
        const int pitch = angles[0];
        const int roll = angles[1];
        const int yaw = angles[2];
        const std::array<int, 17> lookups{pitch + 0xc000,
            roll - yaw, roll + yaw, roll - yaw, roll + yaw,
            yaw + roll + 0xc000, yaw - roll + 0xc000,
            roll + yaw + 0xc000, roll - yaw + 0xc000,
            pitch - yaw, pitch + yaw, pitch - roll, pitch + roll,
            roll + pitch + 0xc000, roll - pitch + 0xc000,
            yaw + pitch + 0xc000, yaw - pitch + 0xc000};
        //34AE's mode3 skips four mode2 angle-test dispatches; each reflected lookup adds NEG/ADD.
        count = (matrix.mode == 2 ? 327U : 323U)
            + 2 * std::count_if(lookups.begin(), lookups.end(), reflected);
    }
    return {matrix, {{NormalWorkKind::instructions, count}}};
}

FixedMatrixWork fixedMatrixComposeOperations(const FixedMatrixQ14& first,
    const FixedMatrixQ14& second, const std::uint16_t incomingCX) {
    if (first.mode > 3 || second.mode > 3) {
        throw std::invalid_argument("unsupported source matrix mode");
    }
    FixedMatrixWork result{composeFixedMatricesQ14(first, second, incomingCX), {}};
    if (first.mode == 0 || second.mode == 0) {
        result.operations = {{NormalWorkKind::instructions, first.mode == 0 ? 19U : 23U},
            {NormalWorkKind::repeatString, 9}, {NormalWorkKind::instructions, 7}};
        return result;
    }
    const auto count = first.mode == 1 ? (second.mode == 1 ? 105U : 141U)
        : second.mode == 1 ? 140U : 243U;
    result.operations = {{NormalWorkKind::instructions, count}};
    return result;
}

WorldViewSetupWork failureViewSetupOperations(const WorldViewSetupInput& state) {
    constexpr Viewport failureViewport{0, 318, 0, 199};
    constexpr FixedPosition sourceLight{256, -256, 362};
    if (state.flags != 0x88 || state.projectionShift != 7 || !(state.viewport == failureViewport)
        || !(state.light == sourceLight) || state.viewAngles != std::array<std::uint16_t, 3>{}) {
        throw std::invalid_argument("unsupported failure view configuration");
    }
    const auto builder = fixedMatrixBuildOperations(state.actorAngles);
    auto matrix = builder.matrix;
    transpose(matrix);
    const auto light = transformToView(state.light, {{}, matrix, {}, 0});
    //3E12 through RETF: identity21, yaw55, full88. Caller/light copies remain in B7AC's suffix.
    const auto lightDispatches = matrix.mode == 0 ? 21U : matrix.mode == 1 ? 55U : 88U;
    constexpr std::uint64_t identityConfigurationSuffix = 149;
    const auto suffix = identityConfigurationSuffix - 39 - 21
        + builder.operations.front().count + lightDispatches;
    return {{state.origin, matrix, state.viewport, 128}, light,
        {{NormalWorkKind::instructions, 15}, {NormalWorkKind::repeatString, 6},
         {NormalWorkKind::instructions, 55}, {NormalWorkKind::string, 1},
         {NormalWorkKind::string, 1}, {NormalWorkKind::string, 1},
         {NormalWorkKind::instructions, suffix}}};
}

WorldFirstCenterWork failureFirstCenterOperations(const ViewTransform& view, const WorldActorWorkInput& actor) {
    if (view.matrix.mode > 3 || view.perspectiveScale != 128 || !(view.viewport == Viewport{0, 318, 0, 199})
        || (view.matrix.mode == 0 && view.matrix.values != makeFixedMatrixQ14(0, 0, 0).values)
        || (actor.flags != 0 && actor.flags != 4 && actor.flags != 0x88 && actor.flags != 0x8c)) {
        throw std::invalid_argument("unsupported failure center traversal");
    }
    const auto difference = [](const std::int32_t value, const std::int32_t origin) {
        return static_cast<std::int32_t>(static_cast<std::uint32_t>(value) - static_cast<std::uint32_t>(origin));
    };
    const FixedPosition relative{difference(actor.position.x, view.origin.x),
        difference(actor.position.y, view.origin.y), difference(actor.position.z, view.origin.z)};
    const auto center = transformToView(relative, {{}, view.matrix, {}, 0});
    return {relative, center, {{NormalWorkKind::instructions, (actor.flags & 0x80) != 0 ? 47U : 45U},
        {NormalWorkKind::instructions, vectorTransformDispatches(relative, view.matrix)}}};
}

WorldFirstCenterWork failureNextCenterOperations(const ViewTransform& view, const WorldActorWorkInput& actor) {
    auto result = failureFirstCenterOperations(view, actor);
    result.operations.front().count -= 8; //B9D0 omits the first-list setup at B9A5..B9CD.
    return result;
}

enum class FailureHorizonDomain { level1, level2 };

static HorizonExecutionWork failureHorizonPlan(const HorizonWorkState& state,
    const std::vector<IndexedBitmap>& frames, const Viewport incomingHardwareClip, const FailureHorizonDomain domain) {
    const auto level2 = domain == FailureHorizonDomain::level2;
    constexpr std::int16_t fullRight = 319;
    constexpr std::int16_t modelRight = 318;
    constexpr std::int16_t bottom = 199;
    constexpr int middle = 100;
    constexpr int sourceWidth = 320;
    constexpr std::size_t pageCount = 4;
    if (state.viewTop != 0 || state.viewBottom != bottom || state.stage <= 70
        || state.background != (level2 ? HorizonBackground::solid : HorizonBackground::gradient) || state.overlay != HorizonOverlay::enabled
        || state.bitmapWidth != 256 || state.bitmapHeight == 0 || state.bitmapHeight >= middle
        || incomingHardwareClip.left != 0 || incomingHardwareClip.top != 0 || incomingHardwareClip.bottom != bottom
        || (incomingHardwareClip.right != fullRight && incomingHardwareClip.right != modelRight)
        || frames.size() <= pageCount) {
        throw std::invalid_argument("unsupported full failure horizon profile");
    }
    for (std::size_t index = 0; index < pageCount; ++index) {
        if (frames[index].width != state.bitmapWidth || frames[index].height != state.bitmapHeight
            || frames[index].pixels.size() != static_cast<std::size_t>(state.bitmapWidth) * state.bitmapHeight) {
            throw std::invalid_argument("failure panorama differs from source headers");
        }
    }
    if (frames[4].width != (level2 ? 72 : 32) || frames[4].height != (level2 ? 38 : 27)
        || frames[4].pixels.size() != (level2 ? 72U*38U : 32U*27U)) {
        throw std::invalid_argument("failure panorama overlay differs from source header");
    }
    HorizonExecutionWork result{{}, {}, incomingHardwareClip, state.groundColor, state.groundColor};
    const auto append = [&](const std::vector<NormalWorkOperation>& operations) {
        result.operations.insert(result.operations.end(), operations.begin(), operations.end());
    };
    const auto fill = [&](const int y, const int height, const std::uint8_t color) {
        auto work = uiFillOperations({0, static_cast<std::int16_t>(y),
            static_cast<std::int16_t>(incomingHardwareClip.right + 1), static_cast<std::int16_t>(height)}, color, 0xa000);
        //2ED5..2ED9 trims requested width320 against the previous model right318.
        if (incomingHardwareClip.right == modelRight) {
            work.operations.front().count += 3;
        }
        append(work.operations);
        for (const auto& effect : work.effects) {
            const auto& pixel = std::get<RadarPixelWrite>(effect);
            result.pixels.push_back({pixel.point, pixel.color});
        }
    };
    //Direct6BA6 entry, excluding ordinary6B5A/7C08 and the7379 caller.
    appendInstructions(result.operations, level2 ? 36 : 33);
    const auto panoramaTop = middle - state.bitmapHeight;
    auto color = state.skyColor;
    if (level2) {
        fill(0,panoramaTop,color);
    } else {
        for (int y = panoramaTop - 1; y >= 0; --y) {
            appendInstructions(result.operations, 7);
            fill(y, 1, color);
            appendInstructions(result.operations, 3);
            if (color < 254) {
                appendInstructions(result.operations, 3);
                ++color;
            }
            appendInstructions(result.operations, 4);
        }
    }
    appendInstructions(result.operations, level2 ? 11 : 14);
    fill(middle, bottom + 1 - middle, state.groundColor);
    const auto angle = state.cameraYaw >= 0 ? state.cameraYaw / 64 : -((-int{state.cameraYaw} + 63) / 64);
    appendInstructions(result.operations, 5); //6C7B..6C8A, overlay enabled.
    if (angle < sourceWidth) {
        appendInstructions(result.operations, 2);
        if (angle > 0) {
            appendInstructions(result.operations, 11);
            const auto overlay = uiBitmapOperations(frames[4], {static_cast<std::int16_t>(angle - 40), 0},
                incomingHardwareClip, 0xa000);
            append(overlay.operations);
            for (const auto& effect : overlay.effects) {
                const auto& pixel = std::get<RadarPixelWrite>(effect);
                result.pixels.push_back({pixel.point, pixel.color});
            }
            appendInstructions(result.operations, 1);
        }
    }
    const auto x = static_cast<int>(static_cast<std::uint16_t>(angle) & 255U);
    appendInstructions(result.operations, 7);
    appendYawHorizonBitmap(result.operations, state.bitmapHeight, x, 0, incomingHardwareClip.right);
    appendInstructions(result.operations, 5);
    if (x + 256 < sourceWidth) {
        appendInstructions(result.operations, 10);
        appendYawHorizonBitmap(result.operations, state.bitmapHeight, x + 256, 0, incomingHardwareClip.right);
        appendInstructions(result.operations, 1);
    }
    appendInstructions(result.operations, 11);
    appendYawHorizonBitmap(result.operations, state.bitmapHeight, x - 256, 0, incomingHardwareClip.right);
    appendInstructions(result.operations, 6); //6D11 cleanup and6D14..6D19 near return.
    const auto primary = static_cast<unsigned>((-((static_cast<std::uint16_t>(angle) >> 8) & 255U)) & 3U);
    for (const auto neighbor : {0, 1, -1}) {
        const auto destinationX = x + neighbor * 256;
        if (neighbor == 1 && destinationX >= sourceWidth) {
            continue;
        }
        const auto& bitmap = frames[(primary + neighbor) & 3U];
        const auto trim = std::max(0, -destinationX);
        for (std::uint16_t plane = 0; plane < pixelsPerPlanarByte; ++plane) {
            for (std::uint16_t row = 0; row < bitmap.height; ++row) {
                for (int column = trim + plane; column < bitmap.width; column += pixelsPerPlanarByte) {
                    const auto px = destinationX + column;
                    if (px < 0 || px > incomingHardwareClip.right) {
                        continue;
                    }
                    result.pixels.push_back({{static_cast<std::int16_t>(px),
                        static_cast<std::int16_t>(panoramaTop + row)}, bitmap.pixels[row * bitmap.width + column]});
                }
            }
        }
    }
    return result;
}

}  // namespace stellar7

namespace stellar7 {
WorldModelSetupWork failureModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t, 3> angles, const std::uint16_t projectedRadius,
    const std::int16_t type, const Model3d& model, const std::uint16_t registryScale,
    const std::uint16_t registryCap, const WorldModelViewInput& view) {
    return failureSetupOperations(center, angles, projectedRadius, type, model,
        registryScale, registryCap, view, ModelSetupProfile::identityOrYaw);
}

static WorldModelCullWork ownedSphereOperations(const FixedPosition center,
    const std::uint32_t collisionRadius,const Viewport viewport,const bool arrival,const bool deployment=false) {
    constexpr std::int64_t nearPlane = 128;
    const auto radius = static_cast<std::int64_t>(collisionRadius);
    const auto absolute = [](const std::int32_t value) {
        return value == -32768 ? std::int64_t{32767} : std::abs(std::int64_t{value});
    };
    const auto absX = absolute(center.x);
    const auto absZ = absolute(center.z);
    const auto farDepth = std::int64_t{center.y} + radius;
    const auto rawNearDepth = std::int64_t{center.y} - radius;
    const auto nearDepth = std::max(nearPlane, rawNearDepth);
    if (!(viewport == (arrival ? Viewport{24,294,24,145} : Viewport{0,318,0,199}))
        || radius <= 0 || radius >= 16300
        || std::max({absX+radius,absZ+radius,farDepth}) > (deployment ? 1000000 : arrival ? 64000 : 32000)) {
        throw std::invalid_argument("unsupported failure sphere arithmetic domain");
    }
    const auto quotient=[deployment](const std::int64_t value,const std::int64_t depth) {
        auto numerator=std::abs(value)*128;
        auto divisor=depth;
        if (deployment) { while (divisor>65535) { numerator>>=1;divisor>>=1; } }
        return (value<0 ? -1 : 1)*(numerator/divisor);
    };
    const auto projectionExtra=[deployment](const std::int64_t depth,const std::int64_t first,const std::int64_t second) -> std::uint64_t {
        if (!deployment) { return depth>=32768 ? 6 : 0; }
        auto divisor=depth;unsigned shifts=0;
        while (divisor>65535) { divisor>>=1;++shifts; }
        if (shifts!=0) { return 2*(6*shifts-1); }
        const auto high=[](const std::int64_t value) { return (std::abs(value)*128)>>16; };
        const auto signedDivisor=static_cast<std::int16_t>(divisor);
        return 3*((signedDivisor<=high(first))+(signedDivisor<=high(second)));
    };
    const auto projected = static_cast<std::uint16_t>(std::min<std::int64_t>(256, quotient(radius,nearDepth)));
    WorldModelCullWork result{true, false, projected, {}};
    const auto plain = [&result](const std::uint64_t count) {
        result.operations.push_back({NormalWorkKind::instructions, count});
    };
    if (farDepth < 0) {
        plain(22); //CB7A..CBA5,CFB5..CFBA.
        return result;
    }
    if (farDepth < nearPlane) {
        plain(25); //CB7A..CBAE,CFB5..CFBA.
        return result;
    }
    if (projected == 0) {
        plain(27); //CBB8 early return.
        return result;
    }
    const auto absoluteExtra = [](const std::int32_t value) -> std::uint64_t {
        if (value >= 0) { return 0; }
        if (value == -32768) { return 2; }
        return (static_cast<std::uint16_t>(value) == 0x8000) ? 6 : 4;
    };
    const auto signedExtras = absoluteExtra(center.x) + absoluteExtra(center.z);
    plain(61 + signedExtras); //CB7A..CD27, excluding4ABF child.
    plain(214 + (absX < radius ? 18 : 0) + (absZ < radius ? 18 : 0)
        + projectionExtra(farDepth,absX-radius,absZ-radius));
    const auto firstX = 159 + quotient(absX-radius,farDepth);
    const auto middleY=arrival ? 84 : 99;
    const auto firstY = middleY - quotient(absZ-radius,farDepth);
    plain(11); //CD2C..CD4A.
    plain(firstX < 0 ? 5 : 7); //CD4E..CD61 high/low comparisons.
    if (firstX > viewport.right) {
        plain(6); //CD63 JMP and far epilogue.
        return result;
    }
    plain(firstY < 0 ? 4 : firstY >= 65536 ? 5 : 7); //CD66..CD79.
    if (firstY < viewport.top) {
        plain(6); //CD70/CD7B JMP and far epilogue.
        return result;
    }
    plain(45 + signedExtras + (rawNearDepth < 0 ? 1 : rawNearDepth < nearPlane ? 3 : 0));
    plain(214+projectionExtra(nearDepth,absX+radius,absZ+radius)); //CE5E4ABF: positive X/Z DIV branches.
    const auto secondX = 159 + quotient(absX+radius,nearDepth);
    const auto secondY = middleY - quotient(absZ+radius,nearDepth);
    plain(11); //CE63..CE81.
    plain(7); //CE85..CE98: positive X high0/low compare.
    result.culled = false;
    if (secondX > viewport.right) {
        result.clipped = true;
        plain(2); //CE9A JMP,CBC5 clipped1.
        return result;
    }
    plain(secondY < 0 ? 4 : 7); //CE9D..CEB0 high/low comparisons.
    if (secondY < viewport.top) {
        result.clipped = true;
        plain(2); //CEA7/CEB2 JMP,CBC5 clipped1.
        return result;
    }
    plain(2); //CEB5 clipped0,CEBA JMP setup.
    return result;
}
WorldModelCullWork sharedModelSphereCullOperations(const FixedPosition center, const Model3d& model) {
    constexpr std::int64_t precisionLimit = 32000;
    if (model.scaleShift != 0 || model.origin != std::array<std::int16_t,3>{} || model.flags > 1
        || std::max({std::abs(std::int64_t{center.x}),std::abs(std::int64_t{center.y}),
            std::abs(std::int64_t{center.z})}) + model.collisionRadius > precisionLimit) {
        throw std::invalid_argument("unsupported shared model sphere descriptor");
    }
    return ownedSphereOperations(center,model.collisionRadius,{24,294,24,145},true);
}

WorldModelCullWork level1AxisTankModelCullOperations(const FixedPosition center) {
    constexpr std::uint32_t tankRadius=1342;
    constexpr std::int64_t nearPlane=128;
    constexpr std::int64_t precisionLimit=32000;
    if (center.z != 0 || std::abs(std::int64_t{center.x}) > tankRadius
        || std::int64_t{center.y}-tankRadius < nearPlane
        || std::int64_t{center.y}+tankRadius > precisionLimit) {
        throw std::invalid_argument("unsupported Level1 axis tank sphere");
    }
    auto result=ownedSphereOperations(center,tankRadius,{24,294,24,145},true);
    if (result.culled || result.clipped) {
        throw std::invalid_argument("unsupported Level1 axis tank clipping");
    }
    return result;
}

WorldModelCullWork level1AirborneProjectileModelCullOperations(const FixedPosition center) {
    if (center.z<0 || center.z>1000 || std::max({std::abs(std::int64_t{center.x}),
        std::abs(std::int64_t{center.y}),std::int64_t{center.z}})+134>32000) {
        throw std::invalid_argument("unsupported Level1 airborne projectile sphere");
    }
    return ownedSphereOperations(center,134,{24,294,24,145},true);
}
WorldModelSetupWork level1AirborneProjectileModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t,3> angles,const std::uint16_t radius,const Model3d& model,
    const std::uint16_t scale,const std::uint16_t cap,const WorldModelViewInput& view) {
    return failureSetupOperations(center,angles,radius,20,model,scale,cap,view,ModelSetupProfile::level1AirborneProjectile);
}

WorldModelCullWork failureModelCullOperations(const FixedPosition center,
    const std::uint32_t radius,const Viewport viewport) {
    return ownedSphereOperations(center,radius,viewport,false);
}
WorldModelCullWork level1ArrivalModelCullOperations(const FixedPosition center) {
    if (center.z!=5000) { throw std::invalid_argument("unsupported arrival altitude"); }
    return ownedSphereOperations(center,13052,{24,294,24,145},true);
}
WorldModelSetupWork level1ArrivalModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t,3> angles,const std::uint16_t radius,const Model3d& model,
    const std::uint16_t scale,const std::uint16_t cap,const WorldModelViewInput& view) {
    return failureSetupOperations(center,angles,radius,7,model,scale,cap,view,ModelSetupProfile::level1Arrival);
}

WorldModelCullWork level1DeploymentModelCullOperations(const FixedPosition center,const std::int16_t type) {
    const auto radius=type==7 ? 13052U : type==22 ? 134U : type==44 ? 2792U : type==25 ? 897U : 0U;
    if (radius==0) { throw std::invalid_argument("unsupported deployment sphere type"); }
    return ownedSphereOperations(center,radius,{24,294,24,145},true,true);
}
WorldModelSetupWork level1DeploymentModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t,3> angles,const std::uint16_t radius,const std::int16_t type,const Model3d& model,
    const std::uint16_t scale,const std::uint16_t cap,const WorldModelViewInput& view) {
    return failureSetupOperations(center,angles,radius,type,model,scale,cap,view,ModelSetupProfile::level1Deployment);
}
WorldModelSetupWork level1DepartureModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t,3> angles,const std::uint16_t radius,const Model3d& model,
    const std::uint16_t scale,const std::uint16_t cap,const WorldModelViewInput& view) {
    return failureSetupOperations(center,angles,radius,7,model,scale,cap,view,ModelSetupProfile::level1Departure);
}

}

namespace stellar7 {
WorldModelSetupWork level2ConstructorModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t,3> angles,const std::uint16_t projectedRadius,const std::int16_t type,
    const Model3d& model,const std::uint16_t registryScale,const std::uint16_t registryCap,
    const WorldModelViewInput& view) {
    return failureSetupOperations(center,angles,projectedRadius,type,model,registryScale,registryCap,view,
        ModelSetupProfile::level2Constructor);
}
} // namespace stellar7

namespace stellar7 {
WorldModelSetupWork level1OwnedModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t,3> angles,const std::uint16_t radius,
    const Model3d& model,const std::uint16_t scale,const std::uint16_t cap,
    const WorldModelViewInput& view) {
    return modelSetupOperations(center,angles,radius,0,model,scale,cap,view,
        ModelSetupProfile::ownedSharedPrecision0);
}
}

namespace stellar7 {
WorldModelCullWork level1OwnedWideModelCullOperations(const FixedPosition center, const Model3d& model) {
    constexpr std::int64_t precisionLimit = 32000;
    constexpr std::int64_t wideLimit = 64000;
    const auto extent = std::max({std::abs(std::int64_t{center.x}),std::abs(std::int64_t{center.y}),
        std::abs(std::int64_t{center.z})}) + model.collisionRadius;
    if (model.scaleShift != 0 || model.origin != std::array<std::int16_t,3>{} || model.flags > 1
        || extent <= precisionLimit || extent > wideLimit) {
        throw std::invalid_argument("unsupported owned precision1 sphere descriptor");
    }
    return ownedSphereOperations(center,model.collisionRadius,{24,294,24,145},true);
}

WorldModelSetupWork level1OwnedWideModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t,3> angles,const std::uint16_t radius,
    const Model3d& model,const std::uint16_t scale,const std::uint16_t cap,
    const WorldModelViewInput& view) {
    return failureSetupOperations(center,angles,radius,0,model,scale,cap,view,
        ModelSetupProfile::ownedSharedPrecision1);
}
}

namespace stellar7 {
WorldModelCullWork failureFixedWideCullOperations(const FixedPosition center,const Model3d& model) {
    constexpr std::int64_t precisionLimit=32000,extentLimit=64000;
    const auto extent=std::max({std::abs(std::int64_t{center.x}),std::abs(std::int64_t{center.y}),
        std::abs(std::int64_t{center.z})})+model.collisionRadius;
    if(model.collisionRadius!=retainedFailureFixedRadius || model.flags!=0 || model.scaleShift!=0
        || model.origin!=std::array<std::int16_t,3>{} || extent<=precisionLimit || extent>extentLimit) {
        throw std::invalid_argument("unsupported retained failure fixed sphere");
    }
    return ownedSphereOperations(center,model.collisionRadius,{0,318,0,199},false,true);
}
WorldModelSetupWork failureFixedWideSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t,3> angles,const std::uint16_t radius,const Model3d& model,
    const std::uint16_t scale,const std::uint16_t cap,const WorldModelViewInput& view) {
    return failureSetupOperations(center,angles,radius,retainedFailureFixedType,model,scale,cap,view,ModelSetupProfile::failureFixedWide);
}
} //namespace stellar7

namespace stellar7 {
WorldModelCullWork level2FailureModelCullOperations(const FixedPosition center,const Model3d& model) {
    constexpr std::int64_t setupLimit = 64000;
    if (model.flags>1 || model.scaleShift!=0 || model.origin!=std::array<std::int16_t,3>{}
        || std::max({std::abs(std::int64_t{center.x}),std::abs(std::int64_t{center.y}),
            std::abs(std::int64_t{center.z})})+model.collisionRadius>setupLimit) {
        throw std::invalid_argument("unsupported Level2 failure sphere descriptor");
    }
    return ownedSphereOperations(center,model.collisionRadius,{0,318,0,199},false,true);
}
WorldModelSetupWork level2FailureModelSetupOperations(const FixedPosition center,
    const std::array<std::uint16_t,3> angles,const std::uint16_t radius,const std::int16_t type,
    const Model3d& model,const std::uint16_t scale,const std::uint16_t cap,const WorldModelViewInput& view) {
    return failureSetupOperations(center,angles,radius,type,model,scale,cap,view,ModelSetupProfile::level2Failure);
}
} // namespace stellar7

namespace stellar7 {
HorizonExecutionWork failureHorizonExecution(const HorizonWorkState& state,
    const std::vector<IndexedBitmap>& frames,const Viewport clip) {
    return failureHorizonPlan(state,frames,clip,FailureHorizonDomain::level1);
}
HorizonExecutionWork level2FailureHorizonExecution(const HorizonWorkState& state,
    const std::vector<IndexedBitmap>& frames,const Viewport clip) {
    if (state.skyColor!=64 || state.groundColor!=78) {
        throw std::invalid_argument("Level2 failure requires its retained campaign colors");
    }
    return failureHorizonPlan(state,frames,clip,FailureHorizonDomain::level2);
}
} // namespace stellar7
