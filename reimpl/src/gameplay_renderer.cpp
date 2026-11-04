#include "stellar7/gameplay.hpp"
#include "gameplay_render_support.hpp"
#include "failure_page.hpp"
#include "playable_detail.hpp"
#include "failure_effect_age.hpp"
#include "stellar7/completion_beam.hpp"
#include "stellar7/render_work.hpp"
#include "stellar7/world_model_work.hpp"
#include "stellar7/palette_clock.hpp"
#include "stellar7/indexed_render.hpp"
#include "world_presentation.hpp"
#include "spatial_view.hpp"
#include "chase_view.hpp"
#include "destruction_view.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace stellar7 {
namespace {
constexpr std::int16_t spatialPlayerPrimaryType=23;
constexpr std::int16_t firstSpatialProjectileType=17,lastSpatialProjectileType=23;
constexpr double spatialStrokeNearDepth=128;
constexpr float nativeEnergyStrokeWidth=1;

constexpr int width = 320;
constexpr int height = 200;
constexpr std::uint8_t transparentIndex = 0;
constexpr std::array<std::array<std::uint8_t, 2>, 7> campaignBackgroundColors{{
    {233, 246}, {64, 78}, {168, 203}, {159, 59}, {169, 47}, {112, 218}, {254, 218}
}};
bool isActiveCampaignPhase(const GameplayPhase phase) noexcept {
    return phase == GameplayPhase::level2Handoff
        || phase == GameplayPhase::level2
        || phase == GameplayPhase::level5
        || isPlayableCampaignPhase(phase)
        || isLevel3Phase(phase);
}

struct IndexedFrame {
    std::vector<std::uint8_t> pixels;
    std::shared_ptr<WorldPresentationRecorder> presentation;
};

struct ModelPolygonTrace {
    std::size_t entityDrawOrdinal;
    std::int16_t entityType;
    FixedPosition entityPosition;
    std::uint16_t entityPitch;
    std::uint16_t entityRoll;
    std::uint16_t entityYaw;
    FixedMatrixQ14 entityMatrix;
    FixedMatrixQ14 viewMatrix;
    FixedMatrixQ14 combinedMatrix;
    Viewport viewport;
    FixedPosition entityViewOrigin;
    std::uint16_t apparentSize;
    std::uint16_t lodThreshold;
    std::size_t groupIndex;
    std::size_t shapeIndex;
    std::uint8_t flags;
    std::uint8_t color;
    std::vector<FixedPosition> cameraPoints;
    std::vector<ProjectedPoint> projectedPoints;
};

struct RadarContactTrace {
    std::size_t entityIndex = 0;
    std::size_t activeOrdinal = 0;
    std::int16_t entityType = 0;
    std::int8_t category = 0;
    FixedPosition position{0, 0, 0};
    FixedPosition local{0, 0, 0};
    ProjectedPoint projected{0, 0};
    std::uint8_t color = 0;
    bool visible = false;
};

void putPixel(
    IndexedFrame& frame,
    const int x,
    const int y,
    const std::uint8_t index
) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        frame.pixels[static_cast<std::size_t>(y * width + x)] = index;
    }
}

void fillRectangle(
    IndexedFrame& frame,
    const int left,
    const int top,
    const int right,
    const int bottom,
    const std::uint8_t index
) {
    for (int y = std::max(0, top); y < std::min(height, bottom); ++y) {
        for (int x = std::max(0, left); x < std::min(width, right); ++x) {
            putPixel(frame, x, y, index);
        }
    }
}

void drawLine(
    IndexedFrame& frame,
    int x0,
    int y0,
    const int x1,
    const int y1,
    const std::uint8_t index
) {
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        putPixel(frame, x0, y0, index);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int doubled = error * 2;
        if (doubled >= dy) {
            error += dy;
            x0 += sx;
        }
        if (doubled <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

void drawFilledEllipse(
    IndexedFrame& frame,
    const int centerX,
    const int centerY,
    const int radiusX,
    const int radiusY,
    const std::uint8_t index
) {
    const int radiusXSquared = radiusX * radiusX;
    const int radiusYSquared = radiusY * radiusY;
    const int limit = radiusXSquared * radiusYSquared;
    for (int y = -radiusY; y <= radiusY; ++y) {
        for (int x = -radiusX; x <= radiusX; ++x) {
            if (
                x * x * radiusYSquared
                    + y * y * radiusXSquared
                <= limit
            ) {
                putPixel(frame, centerX + x, centerY + y, index);
            }
        }
    }
}

void blitTransparent(
    IndexedFrame& destination,
    const IndexedBitmap& source,
    const int left,
    const int top
) {
    if (
        source.pixels.size()
        != static_cast<std::size_t>(source.width) * source.height
    ) {
        throw std::runtime_error("bitmap dimensions do not match its pixels");
    }
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            const std::uint8_t index =
                source.pixels[static_cast<std::size_t>(y * source.width + x)];
            if (index != transparentIndex) {
                putPixel(destination, left + x, top + y, index);
            }
        }
    }
}

void blitOpaque(
    IndexedFrame& destination,
    const IndexedBitmap& source,
    const int left,
    const int top
) {
    if (
        source.pixels.size()
        != static_cast<std::size_t>(source.width) * source.height
    ) {
        throw std::runtime_error("bitmap dimensions do not match its pixels");
    }
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            putPixel(
                destination,
                left + x,
                top + y,
                source.pixels[static_cast<std::size_t>(
                    y * source.width + x
                )]
            );
        }
    }
}

void blitTransparentClipped(
    IndexedFrame& destination,
    const IndexedBitmap& source,
    const int left,
    const int top,
    const int clipLeft,
    const int clipTop,
    const int clipRight,
    const int clipBottom
) {
    if (
        source.pixels.size()
        != static_cast<std::size_t>(source.width) * source.height
    ) {
        throw std::runtime_error("bitmap dimensions do not match its pixels");
    }
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            const int destinationX = left + x;
            const int destinationY = top + y;
            if (
                destinationX < clipLeft
                || destinationX > clipRight
                || destinationY < clipTop
                || destinationY > clipBottom
            ) {
                continue;
            }
            const std::uint8_t index =
                source.pixels[static_cast<std::size_t>(y * source.width + x)];
            if (index != transparentIndex) {
                putPixel(destination, destinationX, destinationY, index);
            }
        }
    }
}

const IndexedBitmap* bitmap(
    const GameplayAssets& assets,
    const std::size_t group,
    const std::size_t frame
) {
    if (
        group >= assets.artGroups.size()
        || frame >= assets.artGroups[group].size()
    ) {
        return nullptr;
    }
    return &assets.artGroups[group][frame];
}

void blitArt(
    IndexedFrame& destination,
    const GameplayAssets& assets,
    const std::size_t group,
    const std::size_t frame,
    const int left,
    const int top
) {
    const IndexedBitmap* source = bitmap(assets, group, frame);
    if (source != nullptr) {
        blitTransparent(destination, *source, left, top);
    }
}

void blitArtOpaque(
    IndexedFrame& destination,
    const GameplayAssets& assets,
    const std::size_t group,
    const std::size_t frame,
    const int left,
    const int top
) {
    const IndexedBitmap* source = bitmap(assets, group, frame);
    if (source != nullptr) {
        blitOpaque(destination, *source, left, top);
    }
}

int signedAngleDelta(
    const std::int32_t first,
    const std::int32_t second
) {
    return signedAngle(first - second);
}

std::vector<Rgb> failurePalette(const std::vector<Rgb>& target) {
    auto scaleComponent = [](const std::uint8_t expanded) {
        const auto dac = static_cast<std::uint8_t>(expanded >> 2);
        const auto scaled = static_cast<std::uint8_t>(
            static_cast<unsigned>(dac) * 60U / 63U
        );
        return static_cast<std::uint8_t>((scaled << 2) | (scaled >> 4));
    };
    std::vector<Rgb> scaled;
    scaled.reserve(target.size());
    for (const auto color : target) {
        scaled.push_back({
            scaleComponent(color.red),
            scaleComponent(color.green),
            scaleComponent(color.blue),
        });
    }
    return scaled;
}

std::vector<Rgb> failureBlackoutPalette(const GameplayAssets& assets) {
    constexpr std::uint8_t finalIntensity = 3; // Source9d4c ends63,59,...3; no zero pass.
    const auto& source = assets.cockpitDacPalette;
    if (source.size() != vgaPaletteBytes
        || std::any_of(source.begin(), source.end(), [](const std::uint8_t component) {
            return component > vgaComponentMaximum;
        })) {
        throw std::runtime_error("failure blackout requires a 256-color six-bit source palette");
    }
    PaletteWorkState state{};
    std::copy(source.begin(), source.end(), state.source.begin());
    const auto transformed = transformPalette(state, {0, vgaPaletteColors, 0, finalIntensity});
    const auto expanded = [&](const std::size_t color, const std::size_t component) {
        const auto value = transformed.output[color * vgaPaletteComponents + component];
        return static_cast<std::uint8_t>((value << 2) | (value >> 4));
    };
    std::vector<Rgb> palette(vgaPaletteColors);
    for (std::size_t color = 0; color < palette.size(); ++color) {
        palette[color] = {expanded(color, 0), expanded(color, 1), expanded(color, 2)};
    }
    return palette;
}

ProjectedPoint projectedPosition(
    const GameplayModel& model,
    const FixedPosition position
) {
    auto view = initialLevel1MainView();
    const auto& state = model.level1State();
    view.origin = state.player().position;
    view.matrix = makeFixedMatrixQ14(
        state.player().pitch,
        state.player().roll,
        state.player().yaw
    );
    transpose(view.matrix);
    const auto local = transformToView(
        position,
        view
    );
    if (local.y <= 0) {
        return {-32768, -32768};
    }
    return projectToViewport(local, view);
}

std::int32_t extractModelQ14(const std::int64_t value) {
    const auto quotient = value / 0x4000;
    const auto remainder = value % 0x4000;
    return static_cast<std::int32_t>(
        quotient - (value < 0 && remainder != 0 ? 1 : 0)
    );
}

FixedPosition transformModelVertex(
    const std::array<std::int16_t, 3>& vertex,
    const FixedMatrixQ14& matrix
) {
    std::array<std::int32_t, 3> transformed{};
    for (std::size_t output = 0; output < transformed.size(); ++output) {
        std::int64_t sum = 0;
        for (std::size_t input = 0; input < vertex.size(); ++input) {
            sum += static_cast<std::int64_t>(vertex[input])
                * matrix.values[output + input * 3];
        }
        transformed[output] = extractModelQ14(sum);
    }
    return {transformed[0], transformed[1], transformed[2]};
}

constexpr std::array<std::uint8_t, 256> level1ShadeLevels{{
    0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 4, 4, 0, 0, 8, 6, 0, 0, 4, 0, 0, 0,
    0, 0, 0, 0, 4, 0, 0, 0, 8, 0, 0, 0, 4, 0, 0, 0,
    0, 0, 0, 0, 4, 0, 0, 0, 8, 0, 0, 0, 4, 0, 0, 0,
    0, 0, 0, 0, 4, 4, 0, 0, 8, 8, 0, 0, 4, 0, 0, 0,
    8, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 4, 0, 6, 0,
    4, 0, 0, 0, 0, 0, 0, 0, 4, 0, 6, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 4, 0, 0, 0, 8, 0, 0, 0, 4, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
}};

constexpr std::array<std::int8_t, 256> faceLightingAdjustments{{
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
    -2, -2, -2, -2, -2, -2, -2, -2,
    -2, -2, -1, -1, -1, -1, -1, -1,
     0,  0,  0,  0,  0,  0,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,
    -3, -3, -3, -3, -3, -3, -3, -3,
    -2, -2, -2, -2, -1, -1, -1, -1,
     0,  0,  0,  0,  1,  1,  1,  1,
     2,  2,  2,  2,  2,  2,  2,  2,
    -4, -4, -4, -4, -4, -4, -3, -3,
    -3, -3, -2, -2, -2, -2, -1, -1,
     0,  0,  1,  1,  1,  1,  2,  2,
     2,  2,  3,  3,  3,  3,  3,  3,
    -5, -5, -5, -5, -5, -5, -4, -4,
    -3, -3, -2, -2, -1, -1, -1, -1,
     0,  0,  1,  1,  2,  2,  3,  3,
     3,  3,  4,  4,  4,  4,  4,  4,
    -6, -6, -6, -6, -6, -6, -5, -5,
    -4, -4, -3, -3, -2, -2, -1, -1,
     0,  0,  1,  1,  2,  2,  3,  3,
     4,  4,  5,  5,  5,  5,  5,  5,
    -7, -7, -7, -7, -7, -6, -6, -5,
    -4, -4, -3, -3, -2, -2, -1, -1,
     0,  0,  1,  1,  2,  2,  3,  3,
     4,  5,  5,  6,  6,  6,  6,  6,
    -8, -8, -8, -8, -8, -7, -7, -6,
    -5, -5, -4, -3, -3, -2, -1, -1,
     0,  0,  1,  2,  2,  3,  4,  4,
     5,  6,  6,  7,  7,  7,  7,  7,
}};

std::int32_t highSignedWord(const std::int64_t value) {
    if (value >= 0) {
        return static_cast<std::int32_t>(value / 0x10000);
    }
    return -static_cast<std::int32_t>(
        (-value + 0xffff) / 0x10000
    );
}

std::uint8_t litModelColor(
    const ModelShape& shape,
    const FixedPosition& normal,
    const FixedPosition& light
) {
    std::uint8_t primary = shape.colors[0];
    const std::uint8_t secondary = shape.colors[1];
    if ((shape.flags & 0xa0U) != 0xa0U) {
        return primary;
    }
    const std::uint8_t level = level1ShadeLevels[secondary];
    if (level == 0 || level > 8) {
        return primary;
    }
    auto direction = highSignedWord(
        static_cast<std::int64_t>(normal.x) * light.x
        + static_cast<std::int64_t>(normal.y) * light.y
        + static_cast<std::int64_t>(normal.z) * light.z
    );
    if (direction == 0x10) {
        direction = 0x0f;
    }
    if (direction < -0x10 || direction > 0x0f) {
        return primary;
    }
    const auto adjustment = faceLightingAdjustments[
        static_cast<std::size_t>(level - 1) * 32
        + static_cast<std::size_t>(direction + 0x10)
    ];
    if (primary == secondary) {
        primary = static_cast<std::uint8_t>(primary + adjustment);
    }
    return primary;
}

std::uint8_t modelAnimationVariant(
    const Level1Entity& entity,
    const Level1State&,
    const std::int16_t animationOffset
) {
    if (animationOffset < 0) {
        return 0;
    }
    if (entity.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback
        || entity.type == 7 || entity.type == 25) {
        // Native +1c points at the delivery craft's inline +4f bytes.
        // cf1c-cf23 adds the group offset; this is not the heap selector
        // used by the animated guardian callback. Intact L2 tick 1839
        // selects hatch variant 2 while the other groups have offset -1.
        // Type 25's six groups use the same +4f pointer: ticks 2270/2271
        // select 1/2. The selector moves the pickup geometry, not its pose.
        const auto selector = static_cast<std::size_t>(animationOffset) + 2U;
        return selector < entity.stateBytes4dTo52.size()
            ? entity.stateBytes4dTo52[selector] : 0;
    }
    if (
        (entity.renderProfile==WorldActorRenderProfile::heapSelectorDeferredAnimation
            ||entity.renderProfile==WorldActorRenderProfile::heapSelectorNoCallback
            ||entity.type == 11 || entity.type == 12 || entity.type == 24 || entity.type == 44
            || (entity.type == 45 && entity.animatedModelPresenterEnabled))
        && animationOffset >= 0
    ) {
        if(animationOffset==0) {return entity.presentationModelAnimationVariant;}
        const auto index=static_cast<std::size_t>(animationOffset-1);
        return index<entity.presentationModelAnimationTail.size()?entity.presentationModelAnimationTail[index]:0;
    }
    return 0;
}

std::uint8_t modelAnimationVariant(
    const Level1Projectile& projectile,
    const Level1State&,
    const std::int16_t animationOffset
) {
    if (animationOffset < 0) {
        return 0;
    }
    const auto selector = static_cast<std::size_t>(animationOffset) + 2U;
    if (selector >= projectile.stateBytes4dTo54.size()) {
        return 0;
    }
    return projectile.stateBytes4dTo54[selector];
}

std::vector<FixedPosition> clipModelPolygonToNearPlane(
    const std::vector<FixedPosition>& points,
    const std::int32_t nearPlane
) {
    std::vector<FixedPosition> clipped;
    if (points.empty()) {
        return clipped;
    }
    clipped.reserve(points.size() + 2);
    auto first = points.back();
    bool firstInside = first.y >= nearPlane;
    for (const auto second : points) {
        const bool secondInside = second.y >= nearPlane;
        if (firstInside != secondInside) {
            const auto numerator = static_cast<std::int64_t>(
                nearPlane - first.y
            );
            const auto denominator = static_cast<std::int64_t>(
                second.y - first.y
            );
            clipped.push_back({
                first.x + static_cast<std::int32_t>(
                    static_cast<std::int64_t>(second.x - first.x)
                    * numerator
                    / denominator
                ),
                nearPlane,
                first.z + static_cast<std::int32_t>(
                    static_cast<std::int64_t>(second.z - first.z)
                    * numerator
                    / denominator
                ),
            });
        }
        if (secondInside) {
            clipped.push_back(second);
        }
        first = second;
        firstInside = secondInside;
    }
    return clipped;
}

std::vector<ProjectedPoint> clipProjectedPolygonToViewport(
    const std::vector<ProjectedPoint>& source,
    const Viewport viewport
) {
    const auto clipVertical = [](
        const std::vector<ProjectedPoint>& input,
        const int boundary,
        const bool keepGreater
    ) {
        std::vector<ProjectedPoint> output;
        if (input.empty()) {
            return output;
        }
        output.reserve(input.size() + 2);
        auto first = input.back();
        auto firstInside = keepGreater
            ? first.x >= boundary
            : first.x <= boundary;
        for (const auto second : input) {
            const auto secondInside = keepGreater
                ? second.x >= boundary
                : second.x <= boundary;
            if (firstInside != secondInside) {
                const auto outside = firstInside ? second : first;
                const auto inside = firstInside ? first : second;
                const auto y = static_cast<int>(
                    outside.y
                    + static_cast<std::int64_t>(inside.y - outside.y)
                        * (boundary - outside.x)
                        / (inside.x - outside.x)
                );
                output.push_back({
                    static_cast<std::int16_t>(boundary),
                    static_cast<std::int16_t>(y),
                });
            }
            if (secondInside) {
                output.push_back(second);
            }
            first = second;
            firstInside = secondInside;
        }
        return output;
    };
    const auto clipHorizontal = [](
        const std::vector<ProjectedPoint>& input,
        const int boundary,
        const bool keepGreater
    ) {
        std::vector<ProjectedPoint> output;
        if (input.empty()) {
            return output;
        }
        output.reserve(input.size() + 2);
        auto first = input.back();
        auto firstInside = keepGreater
            ? first.y >= boundary
            : first.y <= boundary;
        for (const auto second : input) {
            const auto secondInside = keepGreater
                ? second.y >= boundary
                : second.y <= boundary;
            if (firstInside != secondInside) {
                const auto outside = firstInside ? second : first;
                const auto inside = firstInside ? first : second;
                const auto x = static_cast<int>(
                    outside.x
                    + static_cast<std::int64_t>(inside.x - outside.x)
                        * (boundary - outside.y)
                        / (inside.y - outside.y)
                );
                output.push_back({
                    static_cast<std::int16_t>(x),
                    static_cast<std::int16_t>(boundary),
                });
            }
            if (secondInside) {
                output.push_back(second);
            }
            first = second;
            firstInside = secondInside;
        }
        return output;
    };

    auto clipped = clipVertical(source, viewport.left, true);
    clipped = clipVertical(clipped, viewport.right, false);
    clipped = clipHorizontal(clipped, viewport.top, true);
    return clipHorizontal(clipped, viewport.bottom, false);
}

void fillPolygon(
    IndexedFrame& frame,
    const std::vector<ProjectedPoint>& sourcePoints,
    const Viewport viewport,
    const std::uint8_t index,
    const bool filled,
    const PresentationFace* presentation = nullptr
) {
    struct Capture {
        IndexedFrame& frame;
        ~Capture() {if(frame.presentation) {frame.presentation->afterFace(frame.pixels);}}
    } capture{frame};
    if(frame.presentation) {
        PresentationFace face{{},viewport,index,filled,{}};
        if(presentation) {face=*presentation;}
        else {for(const auto point:sourcePoints) {face.points.push_back({static_cast<double>(point.x),static_cast<double>(point.y)});}}
        frame.presentation->beforeFace(frame.pixels,std::move(face));
    }
    const auto clippedStorage = filled && sourcePoints.size() >= 3
        ? clipPolygon(sourcePoints, viewport, PolygonClipMode::Enabled).points
        : clipProjectedPolygonToViewport(sourcePoints, viewport);
    const auto& clipped = clippedStorage;
    std::vector<ProjectedPoint> points;
    points.reserve(clipped.size());
    for (const auto point : clipped) {
        if (
            points.empty()
            || points.back().x != point.x
            || points.back().y != point.y
        ) {
            points.push_back(point);
        }
    }
    const bool hadClosingDuplicate = (
        points.size() > 1
        && points.front().x == points.back().x
        && points.front().y == points.back().y
    );
    if (hadClosingDuplicate) {
        points.pop_back();
    }
    // FUN_1f40_3467 does not emit a sample when a filled face with at least
    // three source vertices collapses to one projected coordinate. Native
    // two-point shapes retain their single-pixel result, as observed for the
    // Type 17 projectiles in the exact Level 1 tick-327 frame.
    if (points.size() == 1 && filled && sourcePoints.size() >= 3) {
        return;
    }
    const auto drawSegment = [&](ProjectedPoint first, ProjectedPoint second) {
        if (first.x > second.x) {
            std::swap(first, second);
        }
        int x0 = first.x;
        int y0 = first.y;
        const int x1 = second.x;
        const int y1 = second.y;
        const int deltaX = x1 - x0;
        const int deltaY = y1 - y0;
        const int yStep = deltaY < 0 ? -1 : 1;
        const int absoluteY = std::abs(deltaY);
        const auto draw = [&](const int x, const int y) {
            if (
                x >= viewport.left
                && x <= viewport.right
                && y >= viewport.top
                && y <= viewport.bottom
            ) {
                putPixel(frame, x, y, index);
            }
        };
        if (deltaX == 0) {
            if (y0 > y1) {
                y0 = y1;
            }
            for (int step = 0; step <= absoluteY; ++step) {
                draw(x0, y0 + step);
            }
            return;
        }
        if (absoluteY <= deltaX) {
            int error = absoluteY * 2 - deltaX;
            const int sameRowAdjustment = absoluteY * 2;
            const int nextRowAdjustment =
                absoluteY * 2 - deltaX * 2;
            for (int step = 0; step <= deltaX; ++step) {
                draw(x0, y0);
                if (step == deltaX) {
                    break;
                }
                ++x0;
                if (error >= 0) {
                    y0 += yStep;
                    error += nextRowAdjustment;
                } else {
                    error += sameRowAdjustment;
                }
            }
            return;
        }
        if (deltaY < 0) {
            x0 = x1;
            y0 = y1;
        }
        const int xStep = deltaY < 0 ? -1 : 1;
        int error = deltaX * 2 - absoluteY;
        const int sameColumnAdjustment = deltaX * 2;
        const int nextColumnAdjustment =
            deltaX * 2 - absoluteY * 2;
        for (int step = 0; step <= absoluteY; ++step) {
            draw(x0, y0);
            if (step == absoluteY) {
                break;
            }
            ++y0;
            if (error >= 0) {
                x0 += xStep;
                error += nextColumnAdjustment;
            } else {
                error += sameColumnAdjustment;
            }
        }
    };
    if (points.size() == 1) {
        putPixel(frame, points[0].x, points[0].y, index);
        return;
    }
    if (points.size() == 2) {
        drawSegment(points[0], points[1]);
        return;
    }
    if (points.size() < 3) {
        return;
    }
    if (!filled) {
        for (std::size_t index = 0; index < points.size(); ++index) {
            drawSegment(points[index], points[(index + 1) % points.size()]);
        }
        return;
    }
    if (std::all_of(
        points.begin(),
        points.end(),
        [&points](const auto point) {
            return point.y == points.front().y;
        }
    )) {
        const auto [left, right] = std::minmax_element(
            points.begin(),
            points.end(),
            [](const auto first, const auto second) {
                return first.x < second.x;
            }
        );
        drawSegment(*left, *right);
        return;
    }
    const auto polygon = rasterPolygon(clipped);
    for (std::size_t row = 0; row < polygon.rows.size(); ++row) {
        const auto y = int{polygon.firstY} + static_cast<int>(row);
        if (y < viewport.top || y > viewport.bottom) {
            continue;
        }
        const auto left = std::max(int{polygon.rows[row].left}, int{viewport.left});
        const auto right = std::min(int{polygon.rows[row].right}, int{viewport.right});
        for (int x = left; x <= right; ++x) {
            putPixel(frame, x, y, index);
        }
    }
}

bool recoveredModelSphereIsVisible(
    const FixedPosition viewOrigin,
    const std::uint16_t radius,
    const ViewTransform& view
) noexcept {
    // FUN_1000_cb7a rejects a model before traversing its primitives when
    // the nearest edge of its bounding sphere projects beyond the symmetric
    // viewport. Keep these calculations wide: projectToViewport deliberately
    // returns wrapped int16 coordinates and therefore cannot implement this
    // object-level gate.
    constexpr std::int64_t nearPlane = 0x80;
    const auto wideRadius = static_cast<std::int64_t>(radius);
    const auto farDepth =
        static_cast<std::int64_t>(viewOrigin.y) + wideRadius;
    if (farDepth < nearPlane) {
        return false;
    }
    const auto absolute = [](const std::int32_t value) noexcept {
        const auto wide = static_cast<std::int64_t>(value);
        return wide < 0 ? -wide : wide;
    };
    const auto nearestHorizontal = absolute(viewOrigin.x) - wideRadius;
    const auto nearestVertical = absolute(viewOrigin.z) - wideRadius;
    const auto projectedHorizontal =
        nearestHorizontal * view.perspectiveScale / farDepth;
    const auto projectedVertical =
        nearestVertical * view.perspectiveScale / farDepth;
    const auto centerX =
        (static_cast<std::int64_t>(view.viewport.left)
            + view.viewport.right)
        / 2;
    const auto centerY =
        (static_cast<std::int64_t>(view.viewport.top)
            + view.viewport.bottom)
        / 2;
    return centerX + projectedHorizontal <= view.viewport.right
        && centerY - projectedVertical >= view.viewport.top;
}

enum class RecoveredModelBank { legacy, playable };
struct PresentationAllocation {std::size_t slot;std::uint64_t incarnation;};
std::optional<PresentationAllocation> presentationAllocation(const GameplayModel& gameplay,const Level1Entity& entity) {
    const auto index=entity.allocationIndex;
    const auto& presented=gameplay.level2PresentationEntities();
    if(index>=presented.size()||&entity!=&presented[index]) {return std::nullopt;}
    const auto slot=gameplay.level2PresentationPoolSlot(index);
    const auto incarnation=gameplay.level2PresentationActorIncarnation(index);
    if(!slot||!incarnation) {return std::nullopt;}
    return PresentationAllocation{*slot,*incarnation};
}
std::optional<PresentationAllocation> presentationAllocation(const GameplayModel&,const Level1Projectile&) {
    return std::nullopt;
}

enum class ModelRenderPass { indexed, enhancedObservation };

template<typename Item>
bool drawRecoveredModel(
    IndexedFrame& frame,
    const GameplayAssets& assets,
    const GameplayModel& gameplay,
    const Item& entity,
    const ViewTransform& view,
    std::vector<ModelPolygonTrace>* traces = nullptr,
    const std::size_t entityDrawOrdinal = 0,
    const RecoveredModelBank bank = RecoveredModelBank::legacy,
    const ModelRenderPass pass = ModelRenderPass::indexed,
    const std::optional<PresentationAllocation> visualAllocation = std::nullopt
) {
    constexpr std::uint8_t hiddenMeshFlag = 0x80;
    if ((bank == RecoveredModelBank::playable || isPlayableCampaignPhase(gameplay.phase())
            || gameplay.pendingLevel2FailureTail())
        && (entity.flags & hiddenMeshFlag) != 0) {
        return true;
    }
    if (frame.presentation && pass==ModelRenderPass::indexed) {
        // A culled actor still must preserve backdrop/bitmap writes before its draw.
        frame.presentation->recordPixels(frame.pixels);
        //Observe richer geometry separately; indexed output, callbacks and RNG stay owned by the source pass.
        IndexedFrame observation{frame.pixels,frame.presentation};
        struct Restore {
            IndexedFrame& frame;
            std::shared_ptr<WorldPresentationRecorder> recorder;
            ~Restore() {frame.presentation=recorder;recorder->afterFace(frame.pixels);}
        } restore{frame,frame.presentation};
        frame.presentation.reset();
        const auto drawn=drawRecoveredModel(frame,assets,gameplay,entity,view,traces,entityDrawOrdinal,bank);
        drawRecoveredModel(observation,assets,gameplay,entity,view,nullptr,entityDrawOrdinal,bank,
            ModelRenderPass::enhancedObservation);
        return drawn;
    }
    const Model3d* modelPointer = nullptr;
    const auto levelLocalModelIndex = [](const std::int16_t entityType) {
        return static_cast<std::size_t>(entityType - 44);
    };
    if ((bank == RecoveredModelBank::playable || isPlayableCampaignPhase(gameplay.phase())
            || gameplay.campaignLevel() == 0
            || gameplay.campaignLevel() == 3 || gameplay.campaignLevel() == 5 || gameplay.campaignLevel() == 6)
        && entity.type >= 44 && gameplay.campaignLevel() < assets.campaignLevels.size()
        && levelLocalModelIndex(entity.type) < assets.campaignLevels[gameplay.campaignLevel()].models.size()) {
        modelPointer = &assets.campaignLevels[gameplay.campaignLevel()].models[levelLocalModelIndex(entity.type)];
    }
    if (
        isLevel2Phase(gameplay.phase())
        && entity.type >= 44
        && levelLocalModelIndex(entity.type) < assets.level2Models.size()
    ) {
        modelPointer = &assets.level2Models[levelLocalModelIndex(entity.type)];
    }
    if (
        modelPointer == nullptr
        &&
        isLevel3Phase(gameplay.phase())
        && entity.type >= 44
        && levelLocalModelIndex(entity.type) < assets.level3Models.size()
    ) {
        modelPointer = &assets.level3Models[levelLocalModelIndex(entity.type)];
    }
    if (
        modelPointer == nullptr
        &&
        gameplay.campaignLevel() == 4 && entity.type >= 44
        && assets.campaignLevels.size() > 4
        && levelLocalModelIndex(entity.type) < assets.campaignLevels[4].models.size()
    ) {
        modelPointer = &assets.campaignLevels[4].models[levelLocalModelIndex(entity.type)];
    }
    if (
        modelPointer == nullptr
        &&
        entity.type >= 0
        && static_cast<std::size_t>(entity.type) < assets.models.size()
    ) {
        modelPointer = &assets.models[static_cast<std::size_t>(entity.type)];
    }
    if (modelPointer == nullptr) {
        return false;
    }
    const auto& model = *modelPointer;
    if (model.lods.empty()) {
        return false;
    }
    const auto& state = gameplay.level1State();
    const auto entityMatrix = makeFixedMatrixQ14(
        entity.pitch,
        entity.roll,
        entity.yaw
    );
    // FUN_1000_cb7a leaves CX=0x5240 after pushing the entity-matrix
    // address, and FUN_26e6_38ff consumes that low-word residue in its
    // mode-specialized multiply.
    const auto combinedMatrix = composeFixedMatricesQ14(
        entityMatrix,
        view.matrix,
        0x5240
    );
    const auto light = transformModelVertex(
        std::array<std::int16_t, 3>{{256, -256, 362}},
        view.matrix
    );
    const auto entityViewOrigin = transformToView(entity.position, view);
    const bool sphereVisible=recoveredModelSphereIsVisible(
            entityViewOrigin,
            entity.collisionRadius,
            view
        );
    const bool spatial=pass==ModelRenderPass::enhancedObservation&&frame.presentation&&frame.presentation->capturesSpatial();
    if (!sphereVisible&&!spatial) {
        return true;
    }
    // FUN_1000_b996 stores collision_radius << projection_shift divided by
    // max(camera_depth - collision_radius, near_plane), then clamps the
    // apparent size to 0x100. FUN_1000_c69a selects the first LOD whose
    // threshold is at least that value.
    const auto collisionRadius = static_cast<std::uint64_t>(
        entity.collisionRadius
    );
    const auto rawDenominator = static_cast<std::int64_t>(
        entityViewOrigin.y
    ) - static_cast<std::int64_t>(collisionRadius);
    const auto denominator = static_cast<std::uint64_t>(
        std::max<std::int64_t>(rawDenominator, 1)
    );
    auto apparentSize = static_cast<std::uint16_t>(
        std::min<std::uint64_t>(
            collisionRadius
                * static_cast<std::uint64_t>(view.perspectiveScale)
                / denominator,
            0x100
        )
    );
    const auto detail = gameplay.playableModelDetail(entity.type);
    if (pass==ModelRenderPass::enhancedObservation) {
        constexpr unsigned enhancedDetailScale=4; //Highest supported presentation raster scale.
        apparentSize=static_cast<std::uint16_t>(std::min<unsigned>(256,unsigned(apparentSize)*enhancedDetailScale));
    }
    if (detail.scale != 256) {
        apparentSize = static_cast<std::uint16_t>(std::min<std::uint32_t>(
            std::uint32_t{apparentSize} * detail.scale / 256, detail.cap));
    }
    const ModelLod* lod = &model.lods.back();
    for (const auto& candidate : model.lods) {
        if (apparentSize <= candidate.threshold) {
            lod = &candidate;
            break;
        }
    }
    float floorContactLift=0;
    if(spatial&&entity.type>=0&&entity.type<=16&&entity.position.z==0) {
        auto base=spatialGroundHeight;
        for(const auto& group:lod->groups) {for(const auto& primitive:group.primitives) {
            if(primitive.kind!=0) {continue;}
            for(const auto& shape:primitive.shapes) {for(const auto index:shape.vertexIndices) {
                if(index<group.vertices.size()) {base=std::min<int>(base,group.vertices[index][2]);}
            }}
        }}
        floorContactLift=float(spatialGroundHeight-base);
    }
    struct ProjectedGroup {
        const ModelGroup* source;
        std::vector<FixedPosition> cameraPoints;
        std::int32_t depth;
    };
    std::vector<ProjectedGroup> groups;
    groups.reserve(lod->groups.size());
    for (const auto& group : lod->groups) {
        ProjectedGroup projected{&group, {}, 0};
        projected.cameraPoints.reserve(group.vertices.size());
        for (const auto& vertex : group.vertices) {
            auto local = transformModelVertex(vertex, combinedMatrix);
            local.x += entityViewOrigin.x;
            local.y += entityViewOrigin.y;
            local.z += entityViewOrigin.z;
            projected.cameraPoints.push_back(local);
        }
        auto origin = transformModelVertex(
            group.vertices[group.originVertex],
            combinedMatrix
        );
        // FUN_1000_c9de orders groups by the signed 32-bit result of
        // FUN_26e6_4051(entity-view origin, transformed group origin).
        // This retains the full fixed-point dot product; it is not the
        // truncated camera-space Y coordinate of the group origin.
        const auto depth = static_cast<std::uint32_t>(
            static_cast<std::int64_t>(entityViewOrigin.x) * origin.x
            + static_cast<std::int64_t>(entityViewOrigin.y) * origin.y
            + static_cast<std::int64_t>(entityViewOrigin.z) * origin.z
        );
        projected.depth = static_cast<std::int32_t>(depth);
        groups.push_back(std::move(projected));
    }
    // FUN_1000_c9de uses an in-place descending selection sort. It retains
    // the first candidate for a direct depth tie, but later maximum swaps
    // can still reorder equal-depth groups globally.
    for (
        std::size_t destination = 0;
        destination < groups.size();
        ++destination
    ) {
        auto selected = destination;
        for (
            std::size_t candidate = destination + 1;
            candidate < groups.size();
            ++candidate
        ) {
            if (groups[selected].depth < groups[candidate].depth) {
                selected = candidate;
            }
        }
        if (selected != destination) {
            std::swap(groups[destination], groups[selected]);
        }
    }
    for (const auto& group : groups) {
        auto variant = static_cast<std::size_t>(modelAnimationVariant(
            entity,
            state,
            group.source->animationOffset
        ));
        if constexpr(std::is_same_v<Item,Level1Entity>) {
            constexpr std::size_t completeMinionGaitPoses=8;
            if(pass==ModelRenderPass::enhancedObservation&&gameplay.campaignLevel()==2&&entity.type==47
                &&group.source->animationOffset==0&&group.source->primitives.size()==completeMinionGaitPoses) {
                variant=entity.presentationModelAnimationVariant;
            }
        }
        if (variant >= group.source->primitives.size()) {
            variant = 0;
        }
        const auto& primitive = group.source->primitives[variant];
        if (primitive.kind != 0) {
            continue;
        }
        for (
            std::size_t shapeIndex = 0;
            shapeIndex < primitive.shapes.size();
            ++shapeIndex
        ) {
            const auto& shape = primitive.shapes[shapeIndex];
            const auto normal = (
                shape.normalVertex < group.source->vertices.size()
                ? transformModelVertex(
                    group.source->vertices[shape.normalVertex],
                    combinedMatrix
                )
                : FixedPosition{}
            );
            const auto cullingMode = shape.flags & 3U;
            if(spatial) {
                std::vector<FixedPosition> points;
                for(const auto index:shape.vertexIndices) {
                    if(index>=group.cameraPoints.size()) {points.clear();break;}
                    points.push_back(group.cameraPoints[index]);
                }
                if(!points.empty()) {
                    SpatialSurface surface;surface.color=litModelColor(shape,normal,light);surface.filled=(shape.flags&0x80U)!=0;
                    surface.floorContactLift=floorContactLift;
                    if(entity.type>=firstSpatialProjectileType&&entity.type<=lastSpatialProjectileType) {
                        const auto& pilot=gameplay.level2Player().position;
                        const auto x=double(entity.position.x)-pilot.x,y=double(entity.position.y)-pilot.y,z=double(entity.position.z)-pilot.z;
                        const auto projection=initialLevel1MainView().perspectiveScale;
                        surface.strokeRadius=float(std::max(spatialStrokeNearDepth,std::sqrt(x*x+y*y+z*z))/(2*projection));
                    }
                    if(entity.type==spatialPlayerPrimaryType&&entity.ownerIsPlayer) {
                        surface.playerShotCenter=spatialFromWorld({float(entity.position.x),float(entity.position.y),float(entity.position.z)},view);
                    }
                    surface.type=entity.type;surface.allocation=std::numeric_limits<std::size_t>::max();
                    if(const auto allocation=visualAllocation?visualAllocation:presentationAllocation(gameplay,entity)) {
                        surface.allocation=allocation->slot;surface.incarnation=allocation->incarnation;
                    }
                    // Spatial geometry belongs to the model/world pose. Reusing
                    // integer raster vertices makes floor contact depend on camera rounding.
                    ViewTransform modelAxes{};modelAxes.matrix=entityMatrix;
                    for(const auto index:shape.vertexIndices) {
                        const auto& vertex=group.source->vertices[index];
                        auto world=spatialFromWorld({float(vertex[0]),float(vertex[1]),float(vertex[2])},modelAxes);
                        world.x+=entity.position.x;world.y+=entity.position.y;world.z+=entity.position.z;
                        surface.points.push_back(spatialFromWorld(world,view));
                    }
                    if(surface.filled&&((entity.type>=1&&entity.type<=16)||(entity.type>=27&&entity.type<=41)||entity.type==44||entity.type==45)) {
                        const auto shade=level1ShadeLevels[shape.colors[1]];
                        const auto sourceLit=(shape.flags&0xa0U)==0xa0U&&shape.colors[0]==shape.colors[1]&&shade>0&&shade<=8;
                        if(!sourceLit) {surface.lighting=enhancedSurfaceLighting(points,light);}
                    }
                    frame.presentation->spatialSurface(std::move(surface));
                }
            }
            if(!sphereVisible) {continue;}
            if (
                cullingMode != 0
                && cullingMode != 2
                && !shape.vertexIndices.empty()
                && shape.normalVertex < group.source->vertices.size()
                && shape.vertexIndices.front()
                    < group.cameraPoints.size()
            ) {
                const auto& first = group.cameraPoints[
                    shape.vertexIndices.front()
                ];
                const auto facing = static_cast<std::int64_t>(normal.x)
                    * first.x
                    + static_cast<std::int64_t>(normal.y) * first.y
                    + static_cast<std::int64_t>(normal.z) * first.z;
                if (facing >= 0) {
                    continue;
                }
            }
            std::vector<FixedPosition> cameraPolygon;
            cameraPolygon.reserve(shape.vertexIndices.size());
            bool valid = true;
            for (const std::uint8_t vertexIndex : shape.vertexIndices) {
                if (vertexIndex >= group.cameraPoints.size()) {
                    valid = false;
                    break;
                }
                cameraPolygon.push_back(group.cameraPoints[vertexIndex]);
            }
            if (!valid) {
                continue;
            }
            cameraPolygon = clipModelPolygonToNearPlane(
                cameraPolygon,
                0x80
            );
            std::vector<ProjectedPoint> polygon;
            polygon.reserve(cameraPolygon.size());
            for (const auto point : cameraPolygon) {
                polygon.push_back(projectToViewport(point, view));
            }
            const auto color = litModelColor(shape, normal, light);
            if (traces != nullptr) {
                traces->push_back({
                    entityDrawOrdinal,
                    entity.type,
                    entity.position,
                    entity.pitch,
                    entity.roll,
                    entity.yaw,
                    entityMatrix,
                    view.matrix,
                    combinedMatrix,
                    view.viewport,
                    entityViewOrigin,
                    apparentSize,
                    lod->threshold,
                    static_cast<std::size_t>(
                    group.source - lod->groups.data()
                    ),
                    shapeIndex,
                    shape.flags,
                    color,
                    cameraPolygon,
                    polygon,
                });
            }
            // At Level 1 tick 550, the surviving Type 23 projectile has
            // apparent size zero and projects to a one-pixel horizontal
            // span. The original presenter does not dispatch that shape to
            // FUN_1f40_3467; retain that exact exception without hiding the
            // natural zero-radius Level 2 guardian or Level 3 cannon.
            if (
                apparentSize == 0
                && gameplay.phase() == GameplayPhase::level1
                && entity.type == 23
            ) {
                continue;
            }
            PresentationFace face{{},view.viewport,color,(shape.flags & 0x80U) != 0,{}};
            if(entity.type>=firstSpatialProjectileType&&entity.type<=lastSpatialProjectileType) {
                face.rasterStrokeWidth=nativeEnergyStrokeWidth;
            }
            bool wideProjection=pass==ModelRenderPass::enhancedObservation;
            if(bank==RecoveredModelBank::playable) {
                constexpr auto wordLow=std::numeric_limits<std::int16_t>::min();
                constexpr auto wordHigh=std::numeric_limits<std::int16_t>::max();
                const auto centerX=(view.viewport.left+view.viewport.right)/2;
                const auto centerY=(view.viewport.top+view.viewport.bottom)/2;
                std::int64_t lowX=wordHigh,lowY=wordHigh,highX=wordLow,highY=wordLow;
                for(const auto point:cameraPolygon) {
                    const auto x=std::int64_t{point.x}*view.perspectiveScale/point.y;
                    const auto y=std::int64_t{point.z}*view.perspectiveScale/point.y;
                    lowX=std::min(lowX,x);highX=std::max(highX,x);lowY=std::min(lowY,y);highY=std::max(highY,y);
                }
                wideProjection=wideProjection||highX-lowX>wordHigh||highY-lowY>wordHigh
                    ||lowX+centerX<wordLow||highX+centerX>wordHigh
                    ||centerY-highY<wordLow||centerY-lowY>wordHigh;
            }
            if(frame.presentation||wideProjection) {
                // Interpolate only a retained physical allocation with a known lifetime.
                if(const auto allocation=visualAllocation?visualAllocation:presentationAllocation(gameplay,entity)) {
                    face.identity=PresentationIdentity{allocation->slot,
                        static_cast<std::size_t>(group.source-lod->groups.data()),shapeIndex,variant,entity.type,lod->threshold,
                        allocation->incarnation};
                }
                const auto centerX=(view.viewport.right-view.viewport.left)/2+view.viewport.left;
                const auto centerY=(view.viewport.bottom-view.viewport.top)/2+view.viewport.top;
                for(const auto point:cameraPolygon) {
                    face.points.push_back({centerX+static_cast<double>(point.x)*view.perspectiveScale/point.y,
                        centerY-static_cast<double>(point.z)*view.perspectiveScale/point.y});
                }
            }
            if(pass==ModelRenderPass::enhancedObservation&&face.filled
                &&((entity.type>=1&&entity.type<=16)||(entity.type>=27&&entity.type<=41)||entity.type==44||entity.type==45)) {
                const auto shade=level1ShadeLevels[shape.colors[1]];
                const auto sourceLit=(shape.flags&0xa0U)==0xa0U&&shape.colors[0]==shape.colors[1]&&shade>0&&shade<=8;
                if(!sourceLit) {face.lighting=enhancedSurfaceLighting(cameraPolygon,light);}
            }
            if(wideProjection) {polygon=boundedPresentationPolygon(face);}
            fillPolygon(
                frame,
                polygon,
                view.viewport,
                color,
                (shape.flags & 0x80U) != 0,
                frame.presentation?&face:nullptr
            );
        }
    }
    return true;
}

ViewTransform level1MainView(const GameplayModel& gameplay) {
    auto view = initialLevel1MainView();
    const auto& player = gameplay.level1State().player();
    view.origin = player.position;
    view.matrix = makeFixedMatrixQ14(
        player.pitch,
        player.roll,
        player.yaw
    );
    transpose(view.matrix);
    return view;
}

void drawHorizon(
    IndexedFrame& frame,
    const std::vector<IndexedBitmap>& sourceFrames,
    const int sourceOffset,
    const int destinationLeft,
    const int destinationTop,
    const int destinationWidth,
    const int stripHeight = 16,
    const int sourceRowOffset = 0
) {
    std::vector<const IndexedBitmap*> strips;
    int panoramaWidth = 0;
    for (const IndexedBitmap& source : sourceFrames) {
        if (source.height == stripHeight && source.width == 256) {
            strips.push_back(&source);
            panoramaWidth += source.width;
        }
    }
    if (strips.empty()) {
        return;
    }

    for (int y = 0; y < stripHeight; ++y) {
        for (int x = 0; x < destinationWidth; ++x) {
            const int panoramaX = (sourceOffset + x) % panoramaWidth;
            const int stripIndex = panoramaX / 256;
            const int stripX = panoramaX % 256;
            const IndexedBitmap& strip = *strips[stripIndex];
            putPixel(
                frame,
                destinationLeft + x,
                destinationTop + y,
                strip.pixels[
                    static_cast<std::size_t>(
                        ((sourceRowOffset + y) % stripHeight) * 256
                        + stripX
                    )
                ]
            );
        }
    }
}

void drawLevel2HandoffViewport(
    IndexedFrame& frame,
    const GameplayAssets& assets,
    const GameplayModel& model,
    const std::optional<std::int16_t> playableDetail = std::nullopt
) {
    // The first ANTARES frame uses a flat black sky, palette entry 78 for
    // the ground, and LEVEL2.RES bitmap group 0 at offset zero. The cockpit
    // overlays later obscure the strip edges exactly as in the target.
    // The Level 3 native page keeps the same viewport geometry but selects
    // the live RIGEL palette entries 168 (sky) and 203 (ground).  The older
    // Level 2 ANTARES page uses 64/78.  Checkpoint 142's display-aligned
    // guardian frame pins these indices before any entity is drawn.
    //6e6c selects DS1386's color pair by campaign index.
    const auto levelIndex = model.campaignLevel();
    const auto skyIndex = campaignBackgroundColors.at(levelIndex)[0];
    const auto groundIndex = campaignBackgroundColors.at(levelIndex)[1];
    fillRectangle(frame, 24, 24, 296, 68, skyIndex);
    fillRectangle(frame, 24, 84, 296, 144, groundIndex);
    fillRectangle(frame, 24, 144, 117, 146, groundIndex);
    fillRectangle(frame, 194, 144, 296, 146, groundIndex);
    fillRectangle(frame, 117, 144, 194, 146, transparentIndex);
    if (
        model.phase() == GameplayPhase::level2SuccessPresentation
        || model.phase() == GameplayPhase::level3SuccessPresentation
    ) {
        // The guardian handoff copies the Level 1 portal surface into the
        // Level 2 viewport before the Level 3 page is selected.  The source
        // is the second 256-pixel page of LEVEL1.RES; its 16-pixel strip is
        // retained at the original viewport origin.
        drawHorizon(
            frame,
            assets.horizonFramesForLevel(0),
            256,
            24,
            68,
            272,
            16,
            5
        );
    } else {
        const auto levelIndex = model.campaignLevel();
        const auto& horizonFrames = assets.horizonFramesForLevel(levelIndex);
        constexpr std::size_t horizonOverlayFrame = 4;
        constexpr int overlayHorizontalBias = 40;
        constexpr int overlayAngleLimit = 320;
        constexpr int overlayViewThreshold = 70;
        const auto overlayAngle = arithmeticShiftRightSix(signedAngle(model.level2Player().yaw));
        if (campaignHorizonOverlayEnabled(static_cast<std::uint8_t>(levelIndex)) && overlayAngle > 0 && overlayAngle < overlayAngleLimit
            && (playableDetail ? *playableDetail > overlayViewThreshold
                : assets.cockpitSettings && assets.cockpitSettings->viewSize > overlayViewThreshold)
            && horizonFrames.size() > horizonOverlayFrame) {
            blitTransparentClipped(frame, horizonFrames[horizonOverlayFrame],
                overlayAngle - overlayHorizontalBias, 24, 24, 24, 295, 145);
        }
        const auto horizon = std::find_if(
            horizonFrames.begin(),
            horizonFrames.end(),
            [](const IndexedBitmap& frame) {
                return frame.width == 256 && frame.height > 0;
            }
        );
        const int stripHeight = horizon == horizonFrames.end()
            ? 18
            : horizon->height;
        if (playableDetail && *playableDetail <= nativeDetailPanoramaThreshold) {
            fillRectangle(frame, 24, 84 - stripHeight, 296, 84, skyIndex);
        } else {
            drawHorizon(
                frame,
                horizonFrames,
                wrap(
                    -arithmeticShiftRightSix(
                        signedAngle(model.level2Player().yaw)
                    ),
                    1024
                ),
                24,
                84 - stripHeight,
                272,
                stripHeight
            );
        }
    }

    const auto& ground = model.level2Ground();
    const auto& points = ground.points();
    for (std::size_t index = 0; index < points.size(); ++index) {
        if (!ground.pointVisible(index)) {
            continue;
        }
        const int screenX = points[index].x / 32;
        const int screenY = points[index].y / 32;
        const int depth = std::min((146 - screenY) / 8, 7);
        const IndexedBitmap* marker = bitmap(
            assets,
            4,
            static_cast<std::size_t>(15 + depth)
        );
        if (marker != nullptr) {
            blitTransparentClipped(
                frame,
                *marker,
                screenX,
                screenY,
                24,
                90,
                295,
                145
            );
        }
    }
}

void drawLevel2Entities(
    IndexedFrame& frame,
    const GameplayAssets& assets,
    const GameplayModel& model,
    std::vector<ModelPolygonTrace>* traces = nullptr,
    const EffectProjector& effectProjector = {},
    const BeamProjectionSink& beamProjection = {},
    const RecoveredModelBank bank = RecoveredModelBank::legacy
) {
    auto view = initialLevel1MainView();
    const auto& player = model.level2Player();
    view.origin = player.position;
    view.matrix = makeFixedMatrixQ14(
        player.pitch,
        player.roll,
        player.yaw
    );
    transpose(view.matrix);
    std::size_t ordinal = 0;
    const auto& entities = model.level2PresentationEntities();
    for (const auto index : model.level2PresentationActiveListOrder()) {
        if (index >= entities.size()) {
            continue;
        }
        const auto& entity = entities[index];
        if (
            entity.type == -1
            && entity.stateBytes4dTo52[0] == 252
            && entity.stateBytes4dTo52[1] != 0
        ) {
            std::optional<ProjectedPoint> projected;
            if (effectProjector) {
                projected = effectProjector(index, view);
            } else {
                const auto local = transformToView(entity.position, view);
                if (local.y > 0) {
                    projected = projectToViewport(local, view);
                }
            }
            const IndexedBitmap* explosion = bitmap(
                assets,
                5,
                static_cast<std::size_t>(
                    entity.stateBytes4dTo52[1] - 1U
                )
            );
            if (projected && explosion != nullptr) {
                const auto screen = *projected;
                // FUN_1000_0ac8 centers both axes with half the bitmap
                // height. Preserve that asymmetric original presenter.
                blitTransparent(
                    frame,
                    *explosion,
                    screen.x - explosion->height / 2,
                    screen.y - explosion->height / 2
                );
            }
            continue;
        }
        const auto* beam = model.level2CompletionBeam(index);
        if (beam && beamProjection) {
            beamProjection(index, *beam);
        }
        drawRecoveredModel(
            frame,
            assets,
            model,
            entity,
            view,
            traces,
            ++ordinal,
            bank
        );
        if (beam) {
            for (const auto& pixel : completionBeamPixels(*beam, view.viewport)) {
                putPixel(frame, pixel.point.x, pixel.point.y, pixel.color);
            }
        }
    }
}

void drawLevel2Radar(
    IndexedFrame& frame,
    const GameplayModel& model,
    std::vector<RadarContactTrace>* traces = nullptr
) {
    constexpr int radarVisibleBottom = 179;
    auto view = initialLevel1RadarView();
    const auto& player = model.level2Player();
    view.origin.x = player.position.x;
    view.origin.y = player.position.y;
    const auto radarLocal = makeFixedMatrixQ14(0xc000, 0, 0);
    const auto playerYaw = makeFixedMatrixQ14(0, 0, player.yaw);
    view.matrix = composeFixedMatricesQ14(radarLocal, playerYaw, 0);
    transpose(view.matrix);
    const auto& entities = model.level2Entities();
    std::size_t activeOrdinal = 0;
    const auto drawContact = [&](const Level1Entity& entity, const std::size_t index) {
        ++activeOrdinal;
        const auto category = static_cast<std::int8_t>(
            entity.stateBytes4dTo52[0]
        );
        if (category < -4 || category > -1) {
            return;
        }
        const auto color = static_cast<std::uint8_t>(
            isLevel3Phase(model.phase())
                    && category == -2
                    && entity.type == 31
                ? 78
                : category == -1
                    ? 228
                    : category == -2 && entity.type == 25
                        ? 234
                        : category == -2
                            ? 138
                            : category == -3 ? 238 : 78
        );
        const auto local = transformToView(entity.position, view);
        const auto point = local.y > 0
            ? projectToViewport(local, view)
            : ProjectedPoint{0, 0};
        const bool visible = local.y > 0
            && point.x >= view.viewport.left
            && point.x <= view.viewport.right
            && point.y >= view.viewport.top
            && point.y <= radarVisibleBottom;
        if (traces != nullptr) {
            traces->push_back({
                index,
                activeOrdinal,
                entity.type,
                category,
                entity.position,
                local,
                point,
                color,
                visible,
            });
        }
        if (!visible) {
            return;
        }
        putPixel(frame, point.x - 4, point.y, color);
    };
    for (const auto index : model.level2ActiveListOrder()) {
        if (index < entities.size()) {
            drawContact(entities[index], index);
        }
    }
    // The original keeps a terminal Type -1 effect in the draw list until
    // after its last radar/sight pass. Use the same retained presentation
    // entries as the viewport; do not resurrect it in the simulation pool.
    const auto& presented = model.level2PresentationEntities();
    for (const auto index : model.level2PresentationActiveListOrder()) {
        if (index >= entities.size() && index < presented.size()
            && presented[index].type == -1
            && presented[index].stateBytes4dTo52[0] == 252) {
            drawContact(presented[index], index);
        }
    }
}

void drawViewportBase(
    IndexedFrame& frame,
    const GameplayAssets& assets,
    const GameplayModel& model
) {
    fillRectangle(frame, 24, 24, 296, 47, 254);
    for (int y = 47; y < 68; ++y) {
        fillRectangle(
            frame,
            24,
            y,
            296,
            y + 1,
            static_cast<std::uint8_t>(300 - y)
        );
    }
    fillRectangle(frame, 24, 84, 296, 144, 246);
    fillRectangle(frame, 24, 144, 117, 146, 246);
    fillRectangle(frame, 194, 144, 296, 146, 246);
    fillRectangle(frame, 117, 144, 194, 146, transparentIndex);
    drawHorizon(
        frame,
        assets.horizonFramesForLevel(0),
        model.panoramaOffset(),
        24,
        68,
        272
    );

    const auto& ground = model.level1State().ground();
    const auto& points = ground.points();
    for (std::size_t index = 0; index < points.size(); ++index) {
        if (!ground.pointVisible(index)) {
            continue;
        }
        const int screenX = points[index].x / 32;
        const int screenY = points[index].y / 32;
        const int depth = std::min((146 - screenY) / 8, 7);
        const IndexedBitmap* marker = bitmap(
            assets,
            4,
            static_cast<std::size_t>(7 + depth)
        );
        if (marker != nullptr) {
            blitTransparentClipped(
                frame,
                *marker,
                screenX,
                screenY,
                24,
                90,
                295,
                145
            );
        }
    }
}

void drawLevel1Header(
    IndexedFrame& frame,
    const bool drawLabel = true
) {
    // Two independent executable frames retain these exact Level 1 cockpit
    // presenter pixels: the entry capture and controlled tick 100.
    for (int y = 26; y <= 38; y += 2) {
        putPixel(frame, 96, y, transparentIndex);
        putPixel(frame, 215, y, transparentIndex);
    }
    putPixel(frame, 96, 41, transparentIndex);
    putPixel(frame, 215, 41, transparentIndex);

    if (!drawLabel) {
        return;
    }
    constexpr std::uint8_t foreground = 0xef;
    constexpr std::array<std::array<std::uint8_t, 5>, 3> glyphRows{{
        {{0x7, 0x8, 0x6, 0x1, 0xe}},  // S
        {{0x6, 0x9, 0x9, 0x9, 0x6}},  // O
        {{0x8, 0x8, 0x8, 0x8, 0xf}},  // L
    }};
    for (
        std::size_t character = 0;
        character < glyphRows.size();
        ++character
    ) {
        for (std::size_t row = 0; row < glyphRows[character].size(); ++row) {
            for (int column = 0; column < 4; ++column) {
                if (
                    (
                        glyphRows[character][row]
                        & static_cast<std::uint8_t>(0x8U >> column)
                    ) != 0
                ) {
                    putPixel(
                        frame,
                        153 + static_cast<int>(character) * 5 + column,
                        30 + static_cast<int>(row),
                        foreground
                    );
                }
            }
        }
    }
}

void drawCampaignHeader(
    IndexedFrame& frame,
    const GameplayPhase phase
) {
    drawLevel1Header(frame, false);
    constexpr std::uint8_t foreground = 0xef;
    constexpr std::array<std::uint8_t, 5> glyphA{
        0x3e, 0x41, 0x7f, 0x41, 0x41,
    };
    constexpr std::array<std::uint8_t, 5> glyphE{
        0x7f, 0x40, 0x7e, 0x40, 0x7f,
    };
    constexpr std::array<std::uint8_t, 5> glyphG{
        0x3e, 0x40, 0x4f, 0x41, 0x3e,
    };
    constexpr std::array<std::uint8_t, 5> glyphI{
        0x3e, 0x08, 0x08, 0x08, 0x3e,
    };
    constexpr std::array<std::uint8_t, 5> glyphL{
        0x40, 0x40, 0x40, 0x40, 0x7f,
    };
    constexpr std::array<std::uint8_t, 5> glyphN{
        0x61, 0x51, 0x49, 0x45, 0x43,
    };
    constexpr std::array<std::uint8_t, 5> glyphR{
        0x7e, 0x41, 0x7e, 0x4c, 0x43,
    };
    constexpr std::array<std::uint8_t, 5> glyphS{
        0x3f, 0x40, 0x3e, 0x01, 0x7e,
    };
    constexpr std::array<std::uint8_t, 5> glyphT{
        0x7f, 0x08, 0x08, 0x08, 0x08,
    };
    const auto glyph = [&](const char character)
        -> const std::array<std::uint8_t, 5>& {
        switch (character) {
        case 'A': return glyphA;
        case 'E': return glyphE;
        case 'G': return glyphG;
        case 'I': return glyphI;
        case 'L': return glyphL;
        case 'N': return glyphN;
        case 'R': return glyphR;
        case 'S': return glyphS;
        default: return glyphT;
        }
    };
    const char* label = isLevel3Phase(phase) ? "RIGEL" : "ANTARES";
    for (std::size_t character = 0; label[character] != '\0'; ++character) {
        const auto& rows = glyph(label[character]);
        for (std::size_t row = 0; row < rows.size(); ++row) {
            for (int column = 0; column < 7; ++column) {
                if (
                    (
                        rows[row]
                        & static_cast<std::uint8_t>(0x40U >> column)
                    ) != 0
                ) {
                    putPixel(
                        frame,
                        132 + static_cast<int>(character) * 8 + column,
                        30 + static_cast<int>(row),
                        foreground
                    );
                }
            }
        }
    }
}

void drawLevel1Score(
    IndexedFrame& frame,
    const unsigned score,
    const int fieldLeft = 240,
    const BitmapFont* font = nullptr
) {
    // FUN_1000_7970 clears a 30x6 field at (240,155), formats the unsigned
    // 32-bit score as decimal, and passes it to the active 4x5 cockpit font.
    constexpr std::array<std::array<std::uint8_t, 5>, 10> digits{{
        {{0x6, 0x9, 0x9, 0x9, 0x6}},
        {{0x2, 0x6, 0x2, 0x2, 0x7}},
        {{0xe, 0x1, 0x6, 0x8, 0xf}},
        {{0xe, 0x1, 0x6, 0x1, 0xe}},
        {{0x2, 0x6, 0xa, 0xf, 0x2}},
        {{0xf, 0x8, 0xe, 0x1, 0xe}},
        {{0xf, 0x8, 0xf, 0x9, 0xf}},
        {{0xf, 0x1, 0x2, 0x4, 0x4}},
        {{0xf, 0x9, 0xf, 0x9, 0xf}},
        {{0x6, 0x9, 0x7, 0x1, 0x6}},
    }};
    fillRectangle(
        frame,
        fieldLeft,
        155,
        fieldLeft + 30,
        161,
        transparentIndex
    );
    const auto text = std::to_string(score);
    int cursorX = fieldLeft;
    if (font != nullptr && !font->glyphs.empty()) {
        for (const unsigned char character : text) {
            const auto index = static_cast<int>(character) - font->firstCharacter;
            if (index < 0 || static_cast<std::size_t>(index) >= font->glyphs.size()) {
                continue;
            }
            const auto& glyph = font->glyphs[static_cast<std::size_t>(index)];
            for (int y = 0; y < glyph.height; ++y) {
                for (int x = 0; x < glyph.width; ++x) {
                    if (glyph.pixels[static_cast<std::size_t>(y) * glyph.width + x]) {
                        putPixel(frame, cursorX + x, 155 + y, 0xef);
                    }
                }
            }
            cursorX += glyph.width;
        }
        return;
    }
    // Resource-free synthetic fixtures retain their legacy test font.
    // Interactive/export asset loading always supplies the original FNT.
    for (
        std::size_t character = 0;
        character < text.size() && character < 5;
        ++character
    ) {
        const auto digit = static_cast<std::size_t>(
            text[character] - '0'
        );
        for (std::size_t row = 0; row < digits[digit].size(); ++row) {
            for (int column = 0; column < 4; ++column) {
                if (
                    (
                        digits[digit][row]
                        & static_cast<std::uint8_t>(0x8U >> column)
                    ) != 0
                ) {
                    putPixel(
                        frame,
                        cursorX + column,
                        155 + static_cast<int>(row),
                        0xef
                    );
                }
            }
        }
        // The original cockpit font metrics give 4, 6, and 7 a five-pixel
        // advance. Other recovered score digits advance six pixels.
        cursorX += digit == 4 || digit == 6 || digit == 7 ? 5 : 6;
    }
}

void drawCockpitComposition(
    IndexedFrame& frame,
    const GameplayAssets& assets,
    const GameplayModel& model,
    const bool redrawCachedHudFields = true
) {
    const bool level2 = isCampaignGameplayPhase(model.phase());
    blitArt(frame, assets, 3, 2, 96, 24);
    blitArtOpaque(frame, assets, 3, 1, 128, 121);
    blitArt(
        frame,
        assets,
        4,
        6,
        level2 ? model.level2CockpitIndicatorX() : model.cockpitIndicatorX(),
        122
    );

    blitArt(frame, assets, 4, 0, 24, 38);
    blitArt(frame, assets, 4, 1, 277, 37);
    blitArt(frame, assets, 4, 2, 24, 24);
    blitArt(frame, assets, 4, 3, 260, 24);
    blitArt(frame, assets, 4, 4, 24, 141);
    blitArt(frame, assets, 4, 5, 292, 140);

    blitArtOpaque(frame, assets, 3, 0, 96, 127);

    constexpr std::array<std::array<int, 4>, 7> panelRectangles{{
        {{7, 151, 29, 17}},
        {{7, 167, 29, 17}},
        {{35, 151, 26, 33}},
        {{60, 151, 30, 17}},
        {{60, 167, 30, 17}},
        {{7, 183, 41, 17}},
        {{47, 183, 43, 17}},
    }};
    constexpr std::array<std::pair<int, int>, 7> iconPositions{{
        {14, 156},
        {17, 170},
        {42, 155},
        {64, 153},
        {66, 172},
        {14, 187},
        {50, 186},
    }};
    constexpr std::array<std::uint8_t, 3> panelStateColors{{
        0x92,
        0x89,
        0xB0,
    }};
    const auto& statusLevels = model.level1State().systemStatusLevels();
    const auto& systemTimers = model.level1State().systemTimers();
    for (std::size_t index = 0; index < iconPositions.size(); ++index) {
        const int presenterState = (
            systemTimers[index] != 0
                ? 2
                : (statusLevels[index] != 0 ? 1 : 0)
        );
        const auto& rectangle = panelRectangles[index];
        fillRectangle(
            frame,
            rectangle[0] + 2,
            rectangle[1] + 2,
            rectangle[0] + rectangle[2] - 2,
            rectangle[1] + rectangle[3] - 2,
            panelStateColors[static_cast<std::size_t>(presenterState)]
        );
        blitArt(
            frame,
            assets,
            4,
            (
                statusLevels[index] == 0 && presenterState == 0
                    ? 50
                    : 41
            ) + index,
            iconPositions[index].first,
            iconPositions[index].second
        );
    }

    constexpr std::array<std::array<std::pair<int, int>, 3>, 7>
        statusPipPositions{{
            {{{10, 154}, {10, 158}, {10, 162}}},
            {{{10, 170}, {10, 174}, {10, 178}}},
            {{{42, 178}, {46, 178}, {50, 178}}},
            {{{84, 154}, {84, 158}, {84, 162}}},
            {{{84, 170}, {84, 174}, {84, 178}}},
            {{{10, 186}, {10, 190}, {10, 194}}},
            {{{84, 186}, {84, 190}, {84, 194}}},
    }};
    for (
        std::size_t system = 0;
        system < statusPipPositions.size();
        ++system
    ) {
        for (
            std::size_t pip = 0;
            pip < statusPipPositions[system].size();
            ++pip
        ) {
            const auto [left, top] = statusPipPositions[system][pip];
            blitArt(
                frame,
                assets,
                4,
                statusLevels[system] <= static_cast<std::int8_t>(pip)
                    ? 48
                    : 49,
                left,
                top
            );
        }
    }

    if (
        redrawCachedHudFields
        && (!level2 || model.phase() == GameplayPhase::level2
            || model.phase() == GameplayPhase::level2Handoff)
        && model.weaponIndicatorVisible()
    ) {
        blitArt(
            frame,
            assets,
            4,
            model.weaponReadyIndicator() ? 40 : 39,
            243,
            170
        );
    }
    if (redrawCachedHudFields) {
        if (level2) {
            drawCampaignHeader(frame, model.phase());
        } else {
            drawLevel1Header(frame);
        }
        drawLevel1Score(
            frame,
            model.score(),
            // The copied Level 2 cockpit retains the cached score at x=239.
            // FUN_1000_7970 redraws later score changes at x=240.
            level2
                && assets.scoreFont.glyphs.empty()
                && model.score() == model.level1State().score()
                    ? 239
                    : 240,
            &assets.scoreFont
        );
    } else {
        // The pristine cockpit page is copied after the cached HUD values
        // already match the simulation. Preserve the original redraw quirk:
        // clear the header artifacts but leave its label blank, and retain
        // the resource's initial zero score.
        drawLevel1Header(frame, false);
        drawLevel1Score(frame, 0, 240, &assets.scoreFont);
    }
}

void overlayLevel3GuardianStaticSurface(
    IndexedFrame& frame,
    const GameplayAssets& assets
) {
    if (assets.level3GuardianFrames.empty()) {
        return;
    }
    const auto& base = assets.level3GuardianFrames.front();
    if (base.pixels.size() != width * height) {
        throw std::runtime_error(
            "Level 3 guardian capture frame is not 320x200"
        );
    }
    // The capture is a static cockpit/HUD surface at loop 700. Preserve the
    // world viewport (where entities, terrain, sight, and indicators remain
    // model-driven) and restore the native running page around it. The
    // running 240-sample sequence additionally proves the upper and lower
    // side trims are static presenter pixels, so copy only those trims inside
    // the otherwise dynamic viewport. This is controlled evidence, not a
    // replacement for the eventual dynamic VGA presenter implementation.
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool dynamicWorld =
                x >= 24 && x < 296 && y >= 68 && y < 146;
            const bool staticUpperTrim =
                y >= 68 && y < 84 && (x < 48 || x >= 277);
            const bool staticLowerTrim =
                y >= 140 && y < 146 && (x < 32 || x >= 292);
            if (dynamicWorld && !staticUpperTrim && !staticLowerTrim) {
                continue;
            }
            frame.pixels[static_cast<std::size_t>(y * width + x)] =
                base.pixels[static_cast<std::size_t>(y * width + x)];
        }
    }
}

void overlayLevel3IndicatorPageTransfer(
    IndexedFrame& frame,
    const GameplayAssets& assets,
    const GameplayModel& model
) {
    if (assets.level3GuardianFrames.empty()) {
        return;
    }
    const auto& cachedPage = assets.level3GuardianFrames.front();
    if (cachedPage.pixels.size() != width * height) {
        throw std::runtime_error(
            "Level 3 guardian capture frame is not 320x200"
        );
    }

    // FUN_1000:6d46 does not redraw this cockpit strip from the ordinary
    // sprite path. It transfers the cached page bytes around the moving
    // indicator. The loop-700 and loop-750/800/824 checkpoints establish the
    // source strip at x=142 and the destination strips at x=142 and x=134.
    // Keep this as a narrow semantic page transfer; it must not import a
    // complete native framebuffer into the portable renderer.
    constexpr int sourceX = 142;
    constexpr int top = 122;
    constexpr int stripWidth = 8;
    const int targetX = model.level2CockpitIndicatorX();
    if (
        targetX < 0
        || targetX + stripWidth > width
        || top < 0
        || top + 5 > height
    ) {
        return;
    }

    for (const int y : {122, 123, 125, 126}) {
        for (int i = 0; i < stripWidth; ++i) {
            frame.pixels[static_cast<std::size_t>(y * width + targetX + i)] =
                cachedPage.pixels[
                    static_cast<std::size_t>(y * width + sourceX + i)
                ];
        }
    }

    if (targetX == sourceX) {
        for (int i = 0; i < stripWidth; ++i) {
            frame.pixels[
                static_cast<std::size_t>(124 * width + targetX + i)
            ] = cachedPage.pixels[
                static_cast<std::size_t>(124 * width + sourceX + i)
            ];
        }
        return;
    }

    // At x=134 the native destination row is not a simple eight-byte copy:
    // the captured page's lane order is [0,1,2,3,-4,+5,-2,+7]. Preserve the
    // measured mapping and use the ordinary source order elsewhere until a
    // later native checkpoint proves another lane permutation.
    constexpr std::array<int, stripWidth> shiftedRow124 = {
        0, 1, 2, 3, -4, 5, -2, 7
    };
    for (int i = 0; i < stripWidth; ++i) {
        const int sourceOffset =
            targetX == 134 ? shiftedRow124[static_cast<std::size_t>(i)] : i;
        frame.pixels[
            static_cast<std::size_t>(124 * width + targetX + i)
        ] = cachedPage.pixels[
            static_cast<std::size_t>(124 * width + sourceX + sourceOffset)
        ];
    }
}

void clearLevel3GuardianStaticRadarContacts(IndexedFrame& frame) {
    // The guardian static frame was captured at loop 700. Its lower cockpit
    // page includes two contact pixels that are dynamic in the native page
    // sequence and must be cleared before the current radar presenter runs.
    // The mask is derived from the independent loop-750/800/824 native
    // post-display series; it removes no cockpit art or world geometry.
    putPixel(frame, 159, 154, 0);
    putPixel(frame, 139, 167, 0);
}

void drawSightPhase(IndexedFrame& frame, const bool alternatePhase) {
    constexpr std::uint8_t targetIndex = 95;
    // FUN_1000_674e draws the selected sight after world primitives. The
    // success presenter calls it over the pristine cockpit portal as well.
    drawLine(frame, 143, 75, 175, 75, targetIndex);
    drawLine(frame, 143, 95, 175, 95, targetIndex);
    putPixel(frame, 159, 85, targetIndex);
    if (!alternatePhase) {
        drawLine(frame, 135, 77, 151, 77, targetIndex);
        drawLine(frame, 183, 77, 167, 77, targetIndex);
        drawLine(frame, 135, 93, 151, 93, targetIndex);
        drawLine(frame, 183, 93, 167, 93, targetIndex);
        putPixel(frame, 135, 78, targetIndex);
        putPixel(frame, 183, 78, targetIndex);
        putPixel(frame, 135, 92, targetIndex);
        putPixel(frame, 183, 92, targetIndex);
    } else {
        drawLine(frame, 155, 77, 163, 77, targetIndex);
        drawLine(frame, 155, 93, 163, 93, targetIndex);
        drawLine(frame, 135, 80, 151, 80, targetIndex);
        drawLine(frame, 135, 90, 151, 90, targetIndex);
        drawLine(frame, 183, 80, 167, 80, targetIndex);
        drawLine(frame, 183, 90, 167, 90, targetIndex);
        putPixel(frame, 135, 81, targetIndex);
        putPixel(frame, 183, 81, targetIndex);
        putPixel(frame, 135, 89, targetIndex);
        putPixel(frame, 183, 89, targetIndex);
    }
}

void drawSight(
    IndexedFrame& frame,
    const GameplayModel& model
) {
    drawSightPhase(frame, model.alternateSightPhase());
}

void drawCombat(
    IndexedFrame& frame,
    const GameplayAssets& assets,
    const GameplayModel& model,
    std::vector<ModelPolygonTrace>* traces = nullptr
) {
    constexpr std::uint8_t targetIndex = 95;
    const auto& state = model.level1State();
    const auto view = level1MainView(model);
    std::size_t entityDrawOrdinal = 0;
    std::vector<const Level1Entity*> deferredBonusEntities;
    for (const auto entityIndex : state.activeListOrder()) {
        if (entityIndex >= state.entities().size()) {
            continue;
        }
        const auto& entity = state.entities()[entityIndex];
        if (!entity.active()) {
            continue;
        }
        // The recovered active-list order at tick 374 places the spawned
        // type-42 bonus after the death-effect actor, although it occupies
        // the ordinary entity pool. Preserve that presentation order.
        if (entity.type == 42) {
            deferredBonusEntities.push_back(&entity);
            continue;
        }
        const auto drawOrdinal = ++entityDrawOrdinal;
        if (
            drawRecoveredModel(
                frame,
                assets,
                model,
                entity,
                view,
                traces,
                drawOrdinal
            )
        ) {
            continue;
        }
        if (!isHostile(entity)) {
            continue;
        }
        const auto enemyScreen = projectedPosition(model, entity.position);
        const int enemyX = enemyScreen.x;
        const int enemyY = enemyScreen.y;
        if (enemyX < 28 || enemyX >= 292) {
            continue;
        }
        const auto range = static_cast<int>(
            approximatePlanarDistance(
                state.player().position,
                entity.position
            ) / 32U
        );
        const int halfWidth = std::clamp(
            (112 - range) * 3 / 4,
            12,
            30
        );
        const int halfHeight = std::clamp(halfWidth / 6, 2, 5);
        constexpr std::uint8_t darkBlueIndex = 48;
        constexpr std::uint8_t blueIndex = 56;
        constexpr std::uint8_t brightBlueIndex = 63;
        constexpr std::uint8_t canopyIndex = 79;
        drawLine(
            frame,
            enemyX - halfWidth,
            enemyY,
            enemyX + halfWidth,
            enemyY,
            darkBlueIndex
        );
        drawLine(
            frame,
            enemyX - halfWidth + 5,
            enemyY - 1,
            enemyX + halfWidth - 5,
            enemyY - 1,
            blueIndex
        );
        drawLine(
            frame,
            enemyX - halfWidth + 10,
            enemyY - 2,
            enemyX + halfWidth - 10,
            enemyY - 2,
            brightBlueIndex
        );
        drawLine(
            frame,
            enemyX - halfWidth,
            enemyY,
            enemyX - halfWidth / 2,
            enemyY + halfHeight,
            blueIndex
        );
        drawLine(
            frame,
            enemyX + halfWidth,
            enemyY,
            enemyX + halfWidth / 2,
            enemyY + halfHeight,
            blueIndex
        );
        drawLine(
            frame,
            enemyX - halfWidth / 2,
            enemyY + halfHeight,
            enemyX + halfWidth / 2,
            enemyY + halfHeight,
            darkBlueIndex
        );
        drawLine(
            frame,
            enemyX - halfHeight,
            enemyY - halfHeight,
            enemyX + halfHeight,
            enemyY - halfHeight,
            canopyIndex
        );
        putPixel(frame, enemyX, enemyY - halfHeight - 1, brightBlueIndex);
    }

    bool usedRecoveredProjectile = false;
    for (const auto& projectile : state.projectiles()) {
        if (!projectile.active()) {
            continue;
        }
        const auto drawOrdinal = ++entityDrawOrdinal;
        usedRecoveredProjectile = drawRecoveredModel(
            frame,
            assets,
            model,
            projectile,
            view,
            traces,
            drawOrdinal
        ) || usedRecoveredProjectile;
    }

    auto drawShot = [&frame, &model](const std::size_t slot) {
        constexpr std::uint8_t projectileIndex = 31;
        constexpr std::array<std::pair<int, int>, 7> radii{{
            {28, 10},
            {12, 5},
            {8, 3},
            {6, 2},
            {4, 2},
            {3, 1},
            {1, 1},
        }};
        const std::size_t stage = std::min<std::size_t>(
            model.shotAge(slot) / 3,
            radii.size() - 1
        );
        drawFilledEllipse(
            frame,
            159,
            84,
            radii[stage].first,
            radii[stage].second,
            projectileIndex
        );
    };
    std::array<std::size_t, GameplayModel::maxShots> shotOrder{0, 1};
    if (
        model.shotActive(0)
        && model.shotActive(1)
        && model.shotAge(0) < model.shotAge(1)
    ) {
        std::swap(shotOrder[0], shotOrder[1]);
    }
    if (!usedRecoveredProjectile) {
        for (const std::size_t slot : shotOrder) {
            if (model.shotActive(slot)) {
                drawShot(slot);
            }
        }
    }

    const auto explosionActor = std::find_if(
        state.deathEffects().begin(),
        state.deathEffects().end(),
        [](const Level1DeathEffect& effect) {
            const auto& entity = effect.entity;
            return (
                effect.listed
                && entity.active()
                && entity.type == -1
                && entity.stateBytes4dTo52[0] == 252
            );
        }
    );
    if (explosionActor != state.deathEffects().end()) {
        const auto& actor = explosionActor->entity;
        const auto explosionScreen =
            projectedPosition(model, actor.position);
        const IndexedBitmap* explosion = bitmap(
            assets,
            5,
            static_cast<std::size_t>(
                actor.stateBytes4dTo52[1] - 1U
            )
        );
        if (explosion != nullptr) {
            // FUN_1000_0ac8 draws the current frame, then advances the actor
            // byte retained by the post-render state boundary. It centers
            // both axes using half the bitmap height.
            blitTransparent(
                frame,
                *explosion,
                explosionScreen.x - explosion->height / 2,
                explosionScreen.y - explosion->height / 2
            );
        }
    } else if (model.explosionActive()) {
        const auto explosionScreen =
            projectedPosition(model, model.explosionPosition());
        constexpr std::array<std::size_t, 12> explosionFrames{
            25, 22, 19, 16, 13, 14, 15, 18, 21, 24, 27, 30,
        };
        const std::size_t sequenceIndex = std::min<std::size_t>(
            model.explosionAge(),
            explosionFrames.size() - 1
        );
        const IndexedBitmap* explosion = bitmap(
            assets,
            5,
            explosionFrames[sequenceIndex]
        );
        if (explosion != nullptr) {
            blitTransparent(
                frame,
                *explosion,
                explosionScreen.x - explosion->width / 2,
                explosionScreen.y - explosion->height / 2
            );
        }
    }

    for (const auto* entity : deferredBonusEntities) {
        drawRecoveredModel(
            frame,
            assets,
            model,
            *entity,
            view,
            traces,
            ++entityDrawOrdinal
        );
    }

    drawSight(frame, model);

}

enum class FailureRenderDomain { source, playable };

const IndexedBitmap& failureBitmap(const GameplayAssets& assets, const Level1Entity& entity) {
    constexpr std::uint8_t nonlethalEnd = 12;
    constexpr std::uint8_t firstLethalEnd = 17;
    constexpr std::uint8_t lastLethalEnd = 32;
    constexpr std::uint8_t lethalStride = 3;
    constexpr std::uint8_t lethalFrames = 4;
    const auto age = entity.stateBytes4dTo52[1];
    const auto end = entity.stateBytes4dTo52[4];
    const bool impact = end == nonlethalEnd && age >= 1 && age < end;
    const bool lethal = end >= firstLethalEnd && end <= lastLethalEnd
        && (end - firstLethalEnd) % lethalStride == 0 && age >= end - lethalFrames && age < end;
    if (entity.type != -1 || entity.stateBytes4dTo52[0] != 0xfc
        || (entity.flags != 0x88 && entity.flags != 0x8c) || entity.collisionRadius != 0
        || entity.renderProfile != WorldActorRenderProfile::bitmapEffectCallback || (!impact && !lethal)) {
        throw std::invalid_argument("unsupported playable destruction bitmap actor");
    }
    const auto* effect = bitmap(assets, 5, age);
    if (!effect || effect->width == 0 || effect->height == 0
        || effect->pixels.size() != static_cast<std::size_t>(effect->width) * effect->height) {
        throw std::invalid_argument("missing playable destruction bitmap frame");
    }
    return *effect;
}

void drawFailurePresenter(
    IndexedFrame& frame,
    const GameplayAssets& assets,
    const GameplayModel& model,
    std::vector<ModelPolygonTrace>* traces = nullptr,
    const std::optional<Viewport> backgroundClip = std::nullopt,
    const FailureRenderDomain domain = FailureRenderDomain::source,
    const std::optional<std::int16_t> playableDetail = std::nullopt
) {
    IndexedFrame background{backgroundClip ? frame.pixels : std::vector<std::uint8_t>{}};
    auto& backdrop = backgroundClip ? background : frame;
    if (model.failureFromLevel2()) {
        const auto level = model.campaignLevel();
        const auto& horizonFrames = assets.horizonFramesForLevel(level);
        const auto strip = std::find_if(horizonFrames.begin(), horizonFrames.end(),
            [](const auto& bitmap) { return bitmap.width == 256 && bitmap.height > 0; });
        const int stripHeight = strip == horizonFrames.end() ? 18 : strip->height;
        constexpr int horizonBottom = 100;
        const auto horizonTop = horizonBottom - stripHeight;
        fillRectangle(backdrop, 0, 0, width, horizonTop, campaignBackgroundColors.at(level)[0]);
        fillRectangle(backdrop, 0, horizonBottom, width, height, campaignBackgroundColors.at(level)[1]);
        const auto cameraYaw = static_cast<std::uint16_t>(
            model.failurePlayer().yaw + 0x4000U
        );
        if (playableDetail && *playableDetail <= nativeDetailPanoramaThreshold) {
            fillRectangle(backdrop, 0, horizonTop, width, horizonBottom, campaignBackgroundColors.at(level)[0]);
        } else {
            drawHorizon(
                backdrop,
                horizonFrames,
                wrap(
                    -arithmeticShiftRightSix(signedAngle(cameraYaw)),
                    1024
                ),
                0,
                horizonTop,
                width,
                stripHeight
            );
        }
    } else {
        fillRectangle(backdrop, 0, 0, width, 63, 254);
        for (int y = 63; y < 84; ++y) {
            fillRectangle(
                backdrop,
                0,
                y,
                width,
                y + 1,
                static_cast<std::uint8_t>(316 - y)
            );
        }
        fillRectangle(backdrop, 0, 84, width, height, 246);
        // FUN_1000_6ba6 selects panorama page
        // (-(camera_yaw >> 14) & 3); yaw 0x4000 starts at page 3.
        if (playableDetail && *playableDetail <= nativeDetailPanoramaThreshold) {
            fillRectangle(backdrop, 0, 84, width, 100, 233);
        } else {
            drawHorizon(
                backdrop,
                assets.horizonFramesForLevel(0),
                768,
                0,
                84,
                width
            );
        }
        const auto& level1Horizon = assets.horizonFramesForLevel(0);
        if (
            (!playableDetail || *playableDetail > nativeDetailOverlayThreshold)
            && level1Horizon.size() > 4
            && level1Horizon[4].width == 32
            && level1Horizon[4].height == 27
        ) {
            blitTransparent(backdrop, level1Horizon[4], 216, 0);
        }
    }

    if (backgroundClip) {
        for (auto y = backgroundClip->top; y <= backgroundClip->bottom; ++y) {
            for (auto x = backgroundClip->left; x <= backgroundClip->right; ++x) {
                frame.pixels[y * width + x] = backdrop.pixels[y * width + x];
            }
        }
    }
    const auto& view = model.failureView();
    std::size_t entityDrawOrdinal = 0;
    for (const auto& item : model.failureVisibleItems()) {
        const Level1Entity* entity = nullptr;
        switch (item.kind) {
        case FailureVisibleKind::mission:
            entity = (
                model.failureFromLevel2()
                ? &model.failureMissionEntities()[item.index]
                : &model.level1State().entities()[item.index]
            );
            break;
        case FailureVisibleKind::player:
            entity = &model.failurePlayer();
            break;
        case FailureVisibleKind::debris:
            entity = &model.failureDebris()[item.index];
            break;
        case FailureVisibleKind::burst:
            break;
        }
        if (entity != nullptr) {
            if (domain == FailureRenderDomain::playable && entity->type < 0) {
                const auto& effect = failureBitmap(assets, *entity);
                const auto local = transformToView(entity->position, view);
                constexpr std::int32_t bitmapNearPlane = 128;
                if (local.y < bitmapNearPlane) { continue; }
                const auto point = projectToViewport(local, view);
                if (point.x < view.viewport.left || point.x > view.viewport.right
                    || point.y < view.viewport.top || point.y > view.viewport.bottom) {
                    continue;
                }
                //0AC8 centers both axes with half the bitmap height.
                const auto halfHeight = effect.height / 2;
                blitTransparentClipped(frame, effect, point.x - halfHeight, point.y - halfHeight,
                    view.viewport.left, view.viewport.top, view.viewport.right, view.viewport.bottom);
                continue;
            }
            ++entityDrawOrdinal;
            drawRecoveredModel(
                frame,
                assets,
                model,
                *entity,
                view,
                traces,
                entityDrawOrdinal,
                domain == FailureRenderDomain::playable
                    ? RecoveredModelBank::playable : RecoveredModelBank::legacy
            );
            continue;
        }
        const auto& burst = model.failureBursts()[item.index];
        const auto local = transformToView(burst.position, view);
        if (local.y <= 0) {
            continue;
        }
        const auto point = projectToViewport(local, view);
        const IndexedBitmap* explosion = bitmap(
            assets,
            5,
            13 + model.presentationFrame() - burst.spawnFrame
        );
        if (explosion != nullptr) {
            if (backgroundClip) {
                blitTransparentClipped(frame, *explosion,
                    point.x - explosion->height / 2, point.y - explosion->height / 2,
                    view.viewport.left, view.viewport.top, view.viewport.right, view.viewport.bottom);
            } else {
                blitTransparent(frame, *explosion,
                    point.x - explosion->height / 2, point.y - explosion->height / 2);
            }
        }
    }
    // The destruction presenter alternates its two VGA pages. The odd page
    // exposes the untouched final byte of every 320-byte scanline; all 200
    // exact odd-frame captures therefore retain palette index 0 at x=319.
    if (!backgroundClip && (model.presentationFrame() & 1U) != 0) {
        fillRectangle(frame, 319, 0, 320, 200, 0);
    }
}

void drawRadarContacts(
    IndexedFrame& frame,
    const GameplayModel& model
) {
    // FUN_1000_698a programs the radar presenter bottom boundary as 0xb3.
    constexpr int radarVisibleBottom = 179;
    const auto& state = model.level1State();
    auto view = initialLevel1RadarView();
    view.origin.x = state.player().position.x;
    view.origin.y = state.player().position.y;
    const auto radarLocal = makeFixedMatrixQ14(
        0xC000,
        0,
        0
    );
    const auto playerYaw = makeFixedMatrixQ14(
        0,
        0,
        state.player().yaw
    );
    view.matrix = composeFixedMatricesQ14(
        radarLocal,
        playerYaw,
        0
    );
    transpose(view.matrix);
    const auto drawContact = [&frame, &view, radarVisibleBottom](
        const FixedPosition position,
        const std::uint8_t color
    ) {
        const auto local = transformToView(position, view);
        if (local.y <= 0) {
            return;
        }
        const auto point = projectToViewport(local, view);
        if (
            point.x < view.viewport.left
            || point.x > view.viewport.right
            || point.y < view.viewport.top
            || point.y > radarVisibleBottom
        ) {
            return;
        }
        // FUN_1000_698a passes the projected X coordinate minus four to the
        // radar point primitive.
        putPixel(frame, point.x - 4, point.y, color);
    };
    for (const auto& object : state.radarPresentationObjects()) {
        std::uint8_t color = 0;
        switch (object.category) {
        case -1:
            color = 228;
            break;
        case -2:
            color = object.type == 25 ? 234 : 138;
            break;
        case -3:
            color = 238;
            break;
        case -4:
            color = 78;
            break;
        default:
            continue;
        }
        drawContact(object.position, color);
    }
}

void drawArmorBar(
    IndexedFrame& frame,
    const GameplayModel& model,
    const bool full = false
) {
    constexpr int armorColumns = 43;
    constexpr std::uint8_t filledIndex = 0xEF;
    const bool level2Entry = isActiveCampaignPhase(model.phase());
    // The first Level 2 post-display boundary has reset DS:47f8 to 488. Its
    // presenter draws quotient 0 through quotient N inclusively, yielding
    // 31 columns for 488 even though the frozen Level 1 model ended at 408.
    const int presentedArmor = model.playerArmor();
    const int filledColumns = full
        ? armorColumns
        : std::clamp(
            std::max(0, presentedArmor) / 16 + (level2Entry ? 1 : 0),
            0,
            armorColumns
        );
    for (int column = 0; column < armorColumns; ++column) {
        const int x = (column + 0x38) * 2;
        const std::uint8_t index = (
            column < filledColumns ? filledIndex : transparentIndex
        );
        for (int y = 189; y <= 195; ++y) {
            putPixel(frame, x, y, index);
        }
    }
}

using Glyph = std::array<std::uint8_t, 5>;

Glyph glyph(const char character) {
    switch (character) {
    case 'C':
        return {0x3f, 0x40, 0x40, 0x40, 0x3f};
    case 'E':
        return {0x7f, 0x40, 0x7e, 0x40, 0x7f};
    case 'I':
        return {0x3e, 0x08, 0x08, 0x08, 0x3e};
    case 'N':
        return {0x61, 0x51, 0x49, 0x45, 0x43};
    case 'O':
        return {0x3e, 0x41, 0x41, 0x41, 0x3e};
    case 'S':
        return {0x3f, 0x40, 0x3e, 0x01, 0x7e};
    case 'T':
        return {0x7f, 0x08, 0x08, 0x08, 0x08};
    case 'U':
        return {0x41, 0x41, 0x41, 0x41, 0x3e};
    case 'Y':
        return {0x41, 0x22, 0x14, 0x08, 0x08};
    case '?':
        return {0x3e, 0x41, 0x0e, 0x00, 0x08};
    default:
        return {};
    }
}

void drawUiText(
    IndexedFrame& frame,
    const char* text,
    const int left,
    const int top,
    const bool beveled
) {
    constexpr std::uint8_t foreground = 0x80;
    constexpr std::uint8_t shadow = 0x9c;
    for (std::size_t characterIndex = 0; text[characterIndex] != '\0';
         ++characterIndex) {
        const Glyph rows = glyph(text[characterIndex]);
        const int characterLeft =
            left + static_cast<int>(characterIndex) * 8;
        for (int y = 0; y < static_cast<int>(rows.size()); ++y) {
            for (int x = 0; x < 7; ++x) {
                if ((rows[static_cast<std::size_t>(y)] & (0x40 >> x)) != 0) {
                    putPixel(frame, characterLeft + x, top + y, foreground);
                }
            }
        }
        if (beveled) {
            for (int y = 0; y < static_cast<int>(rows.size()); ++y) {
                for (int x = 0; x < 7; ++x) {
                    if (
                        (rows[static_cast<std::size_t>(y)] & (0x40 >> x))
                        != 0
                    ) {
                        putPixel(
                            frame,
                            characterLeft + x + 1,
                            top + y + 1,
                            shadow
                        );
                    }
                }
            }
        }
    }
}

void drawUiRectangle(
    IndexedFrame& frame,
    const int left,
    const int top,
    const int rectangleWidth,
    const int rectangleHeight
) {
    fillRectangle(
        frame,
        left,
        top,
        left + rectangleWidth,
        top + rectangleHeight,
        0x9c
    );
    const int right = left + rectangleWidth - 1;
    const int bottom = top + rectangleHeight - 1;
    drawLine(frame, left, top, right, top, 0x93);
    drawLine(frame, right, top, right, bottom, 0x93);
    drawLine(frame, left, top, left, bottom, 0x80);
    drawLine(frame, left, bottom, right, bottom, 0x80);
    drawLine(frame, left + 1, top + 1, right - 1, top + 1, 0x93);
    drawLine(frame, right - 1, top + 1, right - 1, bottom - 1, 0x93);
    drawLine(frame, left + 1, top + 1, left + 1, bottom - 1, 0x87);
    drawLine(frame, left + 1, bottom - 1, right - 1, bottom - 1, 0x87);
}

void drawDialogPanel(IndexedFrame& frame, const GameplayAssets& assets) {
    const auto* fill = bitmap(assets, 0, 12);
    if (fill == nullptr) {
        throw std::runtime_error("STELART dialog fill tile is unavailable");
    }
    for (const auto& effect : dialogPanelPageEffects(*fill, {64, 30, 193, 110}, 0xa000)) {
        const auto& pixel = std::get<RadarPixelWrite>(effect);
        putPixel(frame, pixel.point.x, pixel.point.y, pixel.color);
    }
}

void drawContinuePage(IndexedFrame& frame, const GameplayAssets& assets,
    const GameplayModel& model, const ContinuePagePart part) {
    if (part == ContinuePagePart::backdrop) {
        const auto* backdrop = bitmap(assets, 0, 20);
        if (backdrop == nullptr || backdrop->width != 264 || backdrop->height != 2) {
            throw std::runtime_error("STELART UI backdrop strip is unavailable");
        }
        int x = 0;
        for (int y = 0; y < height; y += 2, x += 3) {
            if (y > 174) {
                blitTransparent(frame, *backdrop, x - 524, y);
            }
            blitTransparent(frame, *backdrop, x - 262, y);
            if (x < width) {
                blitTransparent(frame, *backdrop, x, y);
            }
            if (x < 58) {
                blitTransparent(frame, *backdrop, x + 262, y);
            }
        }
        return;
    }
    if (part == ContinuePagePart::dialog) {
        drawDialogPanel(frame, assets);
        drawUiRectangle(frame, 78, 40, 165, 16);
        drawUiText(frame, "CONTINUE?", 125, 45, false);
        drawContinuePage(frame, assets, model, ContinuePagePart::yesWidget);
        drawContinuePage(frame, assets, model, ContinuePagePart::noWidget);
        return;
    }
    if (part == ContinuePagePart::pointer) {
        blitArt(frame, assets, 0, 0, 161,
            model.continueChoice() == ContinueChoice::yes ? 74 : 118);
        return;
    }
    if (part == ContinuePagePart::yesWidget) {
        blitArt(frame, assets, 0, 17, 100, 63);
        if (model.pressedContinueChoice() == ContinueChoice::yes) {
            blitArt(frame, assets, 0, 11, 114, 77);
            blitArt(frame, assets, 0, 11, 199, 77);
            blitArt(frame, assets, 0, 22, 120, 68);
        }
        drawUiText(frame, "YES", 148, 70, true);
        return;
    }
    blitArt(frame, assets, 0, 19, 100, 99);
    if (model.pressedContinueChoice() == ContinueChoice::no) {
        blitArt(frame, assets, 0, 11, 114, 108);
        blitArt(frame, assets, 0, 11, 199, 108);
        if (const auto* source = bitmap(assets, 0, 22)) {
            //583a's NO overlay uses blitter mode1: vertical reflection.
            for (int y = 0; y < source->height; ++y) {
                for (int x = 0; x < source->width; ++x) {
                    const auto pixel = source->pixels[
                        (source->height - 1 - y) * source->width + x];
                    if (pixel != transparentIndex) {
                        putPixel(frame, 120 + x, 111 + y, pixel);
                    }
                }
            }
        }
    }
    drawUiText(frame, "NO", 152, 114, true);
}

IndexedScreen renderContinuePromptIndexed(const GameplayAssets& assets, const GameplayModel& model) {
    if (assets.uiPalette.size() != 256) {
        throw std::runtime_error("STELART UI palette does not have 256 entries");
    }
    IndexedFrame frame{std::vector<std::uint8_t>(width * height, 0)};
    drawContinuePage(frame, assets, model, ContinuePagePart::backdrop);
    drawContinuePage(frame, assets, model, ContinuePagePart::dialog);
    drawContinuePage(frame, assets, model, ContinuePagePart::pointer);
    return {std::move(frame.pixels), assets.uiPalette};
}

std::vector<Rgb> gameplayPalette(
    const GameplayAssets& assets,
    const GameplayModel& model
) {
    if (
        !assets.emulateTargetPaletteTiming
        || assets.cockpitDacPalette.size() != 256 * 3
        || assets.cockpit.palette.size() != 256
    ) {
        return assets.cockpit.palette;
    }

    // The target writes the DAC from a 6-bit source palette.  Keep that
    // source alongside the expanded RGB palette so the transform remains
    // bit-exact instead of trying to invert the 8-bit expansion.
    std::size_t start = 0;
    std::size_t count = 0;
    int intensity = 63;
    std::size_t targetIndex = 238;
    if (
        isLevel3Phase(model.phase())
        || ((isLevel2Phase(model.phase()) || model.phase() == GameplayPhase::level5)
            && model.level2EntryPaletteActive())
    ) {
        // Each level restarts the same full-DAC fade. The first completed
        // ANTARES frame is the intensity-60 pass toward palette entry zero.
        start = 0;
        count = isLevel2Phase(model.phase()) && model.damageFlashActive()
            ? 128 : 256;
        intensity = 60;
        targetIndex = 0;
    } else if (model.ticks() >= 1 && model.ticks() <= 32) {
        // Startup fade owns the complete DAC through tick 32.
        start = 0;
        count = 256;
        intensity = 60;
        targetIndex = 0;
    } else if (model.ticks() == 33) {
        // The first hit callback interleaves with the still-running startup
        // fade: the lower half remains faded while the upper half receives
        // the first damage-flash write.
        start = 0;
        count = 128;
        intensity = 60;
        targetIndex = 0;
    } else if (model.damageFlashActive()) {
        if (model.damageFlashAge() == 0) {
            // The first DAC write of a hit updates only the upper palette
            // half; subsequent timer phases update all 256 entries.
            start = 128;
            count = 128;
            intensity = 15;
        } else if (model.damageFlashAge() < 8) {
            start = 0;
            count = 256;
            intensity = 31 + static_cast<int>(model.damageFlashAge()) * 4;
        }
    }
    if (count == 0 || intensity == 63) {
        return assets.cockpit.palette;
    }

    std::vector<Rgb> palette = assets.cockpit.palette;
    const auto applyTransform = [
        &palette,
        &assets
    ](
        const std::size_t transformStart,
        const std::size_t transformCount,
        const std::size_t transformTarget,
        const int transformIntensity
    ) {
        for (
            std::size_t index = transformStart;
            index < transformStart + transformCount;
            ++index
        ) {
            for (std::size_t component = 0; component < 3; ++component) {
                const int source = assets.cockpitDacPalette[
                    index * 3 + component
                ];
                const int target = assets.cockpitDacPalette[
                    transformTarget * 3 + component
                ];
                const int transformed =
                    source * transformIntensity / 63
                    + target * (63 - transformIntensity) / 63;
                const auto expanded = static_cast<std::uint8_t>(
                    (transformed << 2) | (transformed >> 4)
                );
                if (component == 0) {
                    palette[index].red = expanded;
                } else if (component == 1) {
                    palette[index].green = expanded;
                } else {
                    palette[index].blue = expanded;
                }
            }
        }
    };
    applyTransform(start, count, targetIndex, intensity);
    if (model.ticks() == 33 || (isLevel2Phase(model.phase())
        && model.level2EntryPaletteActive() && model.damageFlashActive())) {
        applyTransform(128, 128, 238, 15);
    }
    return palette;
}

std::vector<Rgb> level3ContactPalette(
    const std::vector<Rgb>& source,
    const unsigned presentationFrame
) {
    if (source.size() != 256) {
        throw std::runtime_error(
            "Level 3 contact palette does not have 256 entries"
        );
    }
    // FUN_1000:516b waits for VGA vertical retrace before each DAC write.
    // The portable frontend advances at 33 ms, while the native presenter
    // updates at 70 Hz.
    const unsigned phase = presentationFrame * 33U * 70U / 1000U;
    std::array<int, 48> values{};
    std::array<int, 48> directions{};
    for (std::size_t index = 0; index < values.size(); ++index) {
        values[index] = 63 - static_cast<int>(index);
        directions[index] = 1;
    }
    for (unsigned update = 0; update < phase; ++update) {
        for (std::size_t index = 0; index < values.size(); ++index) {
            if (values[index] <= 16) {
                directions[index] = 1;
            } else if (values[index] >= 63) {
                directions[index] = -1;
            }
            values[index] += directions[index];
        }
    }

    auto palette = source;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto component = static_cast<std::uint8_t>(
            (values[index] << 2) | (values[index] >> 4)
        );
        palette[16 + index] = {0, component, 0};
    }
    return palette;
}

std::shared_ptr<const WorldPresentation> captureChasePresentation(const GameplayAssets& assets,const GameplayModel& model) {
    const auto& actors=model.level2PresentationEntities();
    const auto palette=gameplayPalette(assets,model);
    const auto& player=model.level2Player();const auto view=chaseCamera(player,actors,WorldPresentationCapture::camera());
    IndexedFrame frame{std::vector<std::uint8_t>(width*height,campaignBackgroundColors.at(model.campaignLevel())[0])};
    auto heading=initialLevel1MainView();heading.matrix=makeFixedMatrixQ14(0,0,player.yaw);
    const auto forward=transformToView({0,100000,0},heading);
    auto direction=view;direction.origin={};
    const auto horizon=projectToViewport(transformToView(forward,direction),view).y;
    const auto& panorama=assets.horizonFramesForLevel(model.campaignLevel());
    const auto strip=std::find_if(panorama.begin(),panorama.end(),[](const auto& image) {
        return image.width==256&&image.height>0&&image.height<=32;
    });
    const auto stripHeight=strip==panorama.end()?16:strip->height;
    fillRectangle(frame,0,std::clamp<int>(horizon,0,height),width,height,
        campaignBackgroundColors.at(model.campaignLevel())[1]);
    drawHorizon(frame,panorama,wrap(-arithmeticShiftRightSix(signedAngle(player.yaw)),1024),
        0,horizon-stripHeight,width,stripHeight);
    frame.presentation=std::make_shared<WorldPresentationRecorder>(frame.pixels,model.campaignLevel(),
        static_cast<unsigned>(model.phase()),model.level2LoopTick(),view);
    frame.presentation->recordDestructions(model.destructions(),model.ticks());
    if(frame.presentation->capturesSpatial()) {
        SpatialWorld world;world.pilotCamera=spatialPilotCamera(player);world.anchor=player.position;
        world.tick=model.ticks();
        for(std::size_t slot=0;slot<world.playerShotAges.size();++slot) {
            world.playerShotAges[slot]=model.shotActive(slot)?model.shotAge(slot)+1:0;
        }
        world.translation=model.presentationTranslation();
        world.skyColor=campaignBackgroundColors.at(model.campaignLevel())[0];world.groundColor=campaignBackgroundColors.at(model.campaignLevel())[1];
        for(const auto& image:panorama) {if(image.width==256&&image.height==stripHeight) {world.panorama.push_back(image);}}
        frame.presentation->spatialWorld(std::move(world),spatialExplosionLights(model.destructions(),model.ticks(),model.campaignLevel(),view));
    }
    auto hull=player;hull.type=0;hull.flags=0;hull.renderProfile=WorldActorRenderProfile::noHandles;
    if(!assets.models.empty()) {hull.collisionRadius=assets.models[0].collisionRadius;}
    struct Item {const Level1Entity* actor;std::int32_t depth;std::optional<PresentationAllocation> identity;const DestructionCue* death=nullptr;};
    constexpr auto playerVisualSlot=std::numeric_limits<std::size_t>::max();
    std::vector<Item> order{{&hull,transformToView(hull.position,view).y,PresentationAllocation{playerVisualSlot,1}}};
    std::vector<bool> inherited(actors.size());
    for(const auto index:model.level2PresentationActiveListOrder()) {
        if(index<inherited.size()) {inherited[index]=true;}
    }
    for(std::size_t index=0;index<actors.size();++index) {
        const auto& actor=actors[index];
        if(!actor.active()&&!inherited[index]) {continue;}
        order.push_back({&actor,transformToView(actor.position,view).y,{}});
    }
    for(const auto& cue:model.destructions()) {
        if(cue.level==model.campaignLevel()) {order.push_back({nullptr,transformToView(cue.position,view).y,{},&cue});}
    }
    std::stable_sort(order.begin(),order.end(),[](const auto& a,const auto& b) {return a.depth>b.depth;});
    for(const auto& item:order) {
        if(!item.actor) {continue;}
        auto shadow=vehicleShadow(*item.actor,view,palette);
        if(!shadow) {continue;}
        const auto allocation=item.identity?item.identity:presentationAllocation(model,*item.actor);
        if(allocation) {
            shadow->identity=PresentationIdentity{allocation->slot,std::numeric_limits<std::size_t>::max()-1,
                0,0,item.actor->type,0,allocation->incarnation};
        }
        fillPolygon(frame,boundedPresentationPolygon(*shadow),view.viewport,shadow->color,true,&*shadow);
    }
    for(const auto& item:order) {
        if(item.death) {
            for(const auto& face:destructionFaces(*item.death,model.ticks(),view,palette)) {
                fillPolygon(frame,boundedPresentationPolygon(face),view.viewport,face.color,true,&face);
            }
            continue;
        }
        const auto& actor=*item.actor;
        if(actor.type==-1&&actor.stateBytes4dTo52[0]==0xfc) {
            if(actor.stateBytes4dTo52[1]>=15&&std::any_of(model.destructions().begin(),model.destructions().end(),[&](const auto& cue) {
                return cue.position==actor.position&&cue.level==model.campaignLevel();
            })) {continue;}
            const auto point=transformToView(actor.position,view);
            if(point.y<=128||actor.stateBytes4dTo52[1]==0) {continue;}
            const auto* image=bitmap(assets,5,actor.stateBytes4dTo52[1]-1);
            if(image) {
                frame.presentation->spatialBillboard({{float(point.x),float(point.y),float(point.z)},
                    float(image->width)*point.y/view.perspectiveScale,float(image->height)*point.y/view.perspectiveScale,*image});
                const auto projected=projectToViewport(point,view);
                blitTransparent(frame,*image,projected.x-image->height/2,projected.y-image->height/2);
            }
            continue;
        }
        drawRecoveredModel(frame,assets,model,actor,view,nullptr,0,RecoveredModelBank::playable,
            ModelRenderPass::enhancedObservation,item.identity);
        const auto* beam=actor.type==7?model.level2CompletionBeam(actor.allocationIndex):nullptr;
        if(!beam||!beam->projected) {continue;}
        // Reproject the source-owned stochastic billboard; never generate its colors again.
        const auto sourceView=initialLevel1MainView();
        const auto pixels=completionBeamPixels(*beam,sourceView.viewport);
        auto inverse=sourceView;inverse.matrix=makeFixedMatrixQ14(player.pitch,player.roll,player.yaw);
        const auto centerX=(sourceView.viewport.left+sourceView.viewport.right)/2;
        const auto centerY=(sourceView.viewport.top+sourceView.viewport.bottom)/2;
        const auto reproject=[&](const ProjectedPoint point) {
            const auto depth=beam->center.y;
            auto world=transformToView({static_cast<std::int32_t>(std::int64_t{point.x-centerX}*depth/sourceView.perspectiveScale),
                depth,static_cast<std::int32_t>(std::int64_t{centerY-point.y}*depth/sourceView.perspectiveScale)},inverse);
            world={world.x+player.position.x,world.y+player.position.y,world.z+player.position.z};
            return transformToView(world,view);
        };
        for(std::size_t begin=0;begin<pixels.size();) {
            auto end=begin+1;
            while(end<pixels.size()&&pixels[end].point.x==pixels[begin].point.x) {++end;}
            const auto a=reproject(pixels[begin].point),b=reproject(pixels[end-1].point);
            if(a.y>128&&b.y>128) {
                const std::vector<ProjectedPoint> line{projectToViewport(a,view),projectToViewport(b,view)};
                PresentationFace face{{},view.viewport,pixels[begin].color,false,{}};
                face.spatialPoints={{float(a.x),float(a.y),float(a.z)},{float(b.x),float(b.y),float(b.z)}};
                face.spatialStrokeRadius=float(beam->center.y)/(2*sourceView.perspectiveScale);
                face.rasterStrokeWidth=nativeEnergyStrokeWidth;
                const auto allocation=presentationAllocation(model,actor);
                face.identity=PresentationIdentity{allocation?allocation->slot:actor.allocationIndex,
                    std::numeric_limits<std::size_t>::max(),static_cast<std::size_t>(pixels[begin].point.x-pixels.front().point.x),
                    beam->timer,actor.type,0,allocation?std::optional<std::uint64_t>{allocation->incarnation}:std::nullopt};
                for(const auto point:line) {face.points.push_back({double(point.x),double(point.y)});}
                fillPolygon(frame,line,view.viewport,pixels[begin].color,false,&face);
            }
            begin=end;
        }
    }
    const auto beforeHud=frame.presentation->capturesSpatial()?frame.pixels:std::vector<std::uint8_t>{};
    drawChaseHud(frame.pixels,assets,model,view,palette);
    frame.presentation->spatialOverlay(beforeHud,frame.pixels,169*width);
    return frame.presentation->finish(frame.pixels);
}

} // namespace

WorldModelGeometryInput worldModelGeometryInput(const GameplayAssets& assets, const WorldModelSetupWork& setup) {
    if (setup.vertexPrecision != 0) {
        throw std::invalid_argument("wide world geometry requires its unshifted center");
    }
    return worldModelGeometryInput(assets, setup, setup.translation, PolygonClipMode::Disabled);
}

WorldModelGeometryInput worldModelGeometryInput(const GameplayAssets& assets, const WorldModelSetupWork& setup,
    const FixedPosition& fullCenter, PolygonClipMode clipping) {
    if (assets.modelColorTables.size() != 2) {
        throw std::invalid_argument("world geometry requires original remap and shade arrays");
    }
    if (setup.vertexPrecision > 1) {
        throw std::invalid_argument("unsupported world geometry vertex precision");
    }
    WorldModelGeometryInput input;
    input.vertexPrecision = setup.vertexPrecision;
    input.fullCenter = fullCenter;
    input.clipping = clipping;
    input.matrix = setup.matrix;
    input.translation = setup.translation;
    input.groupOrder = setup.groupOrder;
    input.colorRemap = assets.modelColorTables[0];
    input.shadeLevels = assets.modelColorTables[1];
    for (std::size_t index = 0; index < faceLightingAdjustments.size(); ++index) {
        input.lightingAdjustments[index] = static_cast<std::uint8_t>(faceLightingAdjustments[index]);
    }
    return input;
}

unsigned level1SuccessPhaseOrdinal(
    const unsigned presentationFrame
) noexcept {
    // The interactive frontend advances every 33 ms. The target evidence
    // measures the DAC loop at 70.054 Hz; the integer 70 Hz clock reproduces
    // the observed phase sequence and leaves frame 166 on phase 7, matching
    // the final palette before the 5.497-second sound actor releases it.
    return (
        presentationFrame * 33U * 70U / 1000U
    ) % 94U;
}

std::vector<Rgb> level1SuccessPalette(
    const std::vector<Rgb>& basePalette,
    unsigned loopOrdinal
) {
    if (basePalette.size() != 256) {
        throw std::runtime_error(
            "success palette base does not have 256 entries"
        );
    }
    std::array<int, 48> values{};
    std::array<int, 48> directions{};
    for (std::size_t index = 0; index < values.size(); ++index) {
        values[index] = 63 - static_cast<int>(index);
        directions[index] = 1;
    }
    loopOrdinal %= 94U;
    for (unsigned ordinal = 0; ordinal < loopOrdinal; ++ordinal) {
        for (std::size_t index = 0; index < values.size(); ++index) {
            if (values[index] < 17) {
                directions[index] = 1;
            } else if (values[index] > 62) {
                directions[index] = -1;
            }
            values[index] += directions[index];
        }
    }
    auto palette = basePalette;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto green = static_cast<std::uint8_t>(values[index]);
        palette[16 + index] = {
            0,
            static_cast<std::uint8_t>((green << 2U) | (green >> 4U)),
            0,
        };
    }
    return palette;
}

IndexedScreen renderLevel1SuccessIndexed(
    const IndexedScreen& frozenFrame,
    const unsigned presentationFrame
) {
    if (frozenFrame.pixels.size() != width * height) {
        throw std::runtime_error("frozen success frame is not 320x200");
    }
    return {
        frozenFrame.pixels,
        level1SuccessPalette(
            frozenFrame.palette,
            level1SuccessPhaseOrdinal(presentationFrame)
        ),
    };
}

IndexedScreen renderLevelEntryIndexed(
    const IndexedScreen& frozenFrame,
    const std::vector<std::uint8_t>& sourceDac,
    const unsigned step
) {
    constexpr std::size_t paletteEntries = 256;
    constexpr std::size_t components = 3;
    constexpr unsigned dacMaximum = 63;
    if (step >= levelEntryFadeSteps || sourceDac.size() != paletteEntries * components
        || frozenFrame.pixels.size() != width * height
        || std::any_of(sourceDac.begin(), sourceDac.end(), [dacMaximum](const auto value) {
            return value > dacMaximum;
        })) {
        throw std::runtime_error("invalid level-entry fade input");
    }
    const unsigned intensity = step * levelEntryFadeIntensityStep;
    auto result = frozenFrame;
    result.palette.resize(paletteEntries);
    for (std::size_t index = 0; index < paletteEntries; ++index) {
        std::array<std::uint8_t, components> color{};
        for (std::size_t component = 0; component < components; ++component) {
            // The original truncates these two products independently.
            const unsigned value = sourceDac[index * components + component] * intensity / dacMaximum
                + sourceDac[component] * (dacMaximum - intensity) / dacMaximum;
            color[component] = static_cast<std::uint8_t>((value << 2) | (value >> 4));
        }
        result.palette[index] = {color[0], color[1], color[2]};
    }
    return result;
}

std::string modelDrawTraceJson(
    const std::vector<ModelPolygonTrace>& traces,
    const char* frameField,
    const unsigned frameValue
) {
    std::ostringstream output;
    output
        << "{\"format_version\":1,\"" << frameField << "\":"
        << frameValue
        << ",\"draws\":[";
    for (std::size_t index = 0; index < traces.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        const auto& trace = traces[index];
        output
            << "{\"call\":" << (index + 1)
            << ",\"entity_draw\":" << trace.entityDrawOrdinal
            << ",\"entity_type\":" << trace.entityType
            << ",\"entity_position\":["
            << trace.entityPosition.x << ','
            << trace.entityPosition.y << ','
            << trace.entityPosition.z
            << "],\"entity_angles\":["
            << trace.entityPitch << ','
            << trace.entityRoll << ','
            << trace.entityYaw
            << "],\"entity_matrix\":{\"values\":[";
        for (
            std::size_t matrixIndex = 0;
            matrixIndex < trace.entityMatrix.values.size();
            ++matrixIndex
        ) {
            if (matrixIndex != 0) {
                output << ',';
            }
            output << trace.entityMatrix.values[matrixIndex];
        }
        output
            << "],\"mode\":" << trace.entityMatrix.mode
            << "},\"view_matrix\":{\"values\":[";
        for (
            std::size_t matrixIndex = 0;
            matrixIndex < trace.viewMatrix.values.size();
            ++matrixIndex
        ) {
            if (matrixIndex != 0) {
                output << ',';
            }
            output << trace.viewMatrix.values[matrixIndex];
        }
        output
            << "],\"mode\":" << trace.viewMatrix.mode
            << "},\"combined_matrix\":{\"values\":[";
        for (
            std::size_t matrixIndex = 0;
            matrixIndex < trace.combinedMatrix.values.size();
            ++matrixIndex
        ) {
            if (matrixIndex != 0) {
                output << ',';
            }
            output << trace.combinedMatrix.values[matrixIndex];
        }
        output
            << "],\"mode\":" << trace.combinedMatrix.mode
            << "},\"viewport\":["
            << trace.viewport.left << ','
            << trace.viewport.right << ','
            << trace.viewport.top << ','
            << trace.viewport.bottom
            << "],\"entity_view_origin\":["
            << trace.entityViewOrigin.x << ','
            << trace.entityViewOrigin.y << ','
            << trace.entityViewOrigin.z
            << "],\"apparent_size\":" << trace.apparentSize
            << ",\"lod_threshold\":" << trace.lodThreshold
            << ",\"group\":" << trace.groupIndex
            << ",\"shape\":" << trace.shapeIndex
            << ",\"flags\":" << static_cast<unsigned>(trace.flags)
            << ",\"color\":" << static_cast<unsigned>(trace.color)
            << ",\"camera\":[";
        for (
            std::size_t pointIndex = 0;
            pointIndex < trace.cameraPoints.size();
            ++pointIndex
        ) {
            if (pointIndex != 0) {
                output << ',';
            }
            const auto point = trace.cameraPoints[pointIndex];
            output
                << '[' << point.x << ',' << point.y << ',' << point.z << ']';
        }
        output << "],\"projected\":[";
        for (
            std::size_t pointIndex = 0;
            pointIndex < trace.projectedPoints.size();
            ++pointIndex
        ) {
            if (pointIndex != 0) {
                output << ',';
            }
            const auto point = trace.projectedPoints[pointIndex];
            output << '[' << point.x << ',' << point.y << ']';
        }
        output << "]}";
    }
    output << "]}";
    return output.str();
}

std::string level1FailureModelDrawTraceJson(
    const GameplayAssets& assets,
    const GameplayModel& model
) {
    if (model.phase() != GameplayPhase::failureExplosion) {
        throw std::runtime_error(
            "failure model draw trace requires the failure presenter"
        );
    }
    IndexedFrame frame{
        std::vector<std::uint8_t>(width * height, 0)
    };
    std::vector<ModelPolygonTrace> traces;
    drawFailurePresenter(frame, assets, model, &traces);
    return modelDrawTraceJson(
        traces,
        "presentation_frame",
        model.presentationFrame()
    );
}

std::string level1GameplayModelDrawTraceJson(
    const GameplayAssets& assets,
    const GameplayModel& model
) {
    if (model.phase() != GameplayPhase::level1) {
        throw std::runtime_error(
            "gameplay model draw trace requires active Level 1"
        );
    }
    IndexedFrame frame{
        std::vector<std::uint8_t>(width * height, 0)
    };
    std::vector<ModelPolygonTrace> traces;
    drawCombat(frame, assets, model, &traces);
    return modelDrawTraceJson(
        traces,
        "loop_tick",
        model.level1State().loopTick()
    );
}

std::string level2GameplayModelDrawTraceJson(
    const GameplayAssets& assets,
    const GameplayModel& model
) {
    if (
        model.phase() != GameplayPhase::level2Handoff
        && model.phase() != GameplayPhase::level2
        && model.phase() != GameplayPhase::level2SuccessPresentation
        && model.phase() != GameplayPhase::level3SuccessPresentation
        && !isLevel3Phase(model.phase())
    ) {
        throw std::runtime_error(
            "gameplay model draw trace requires Level 2 presentation state"
        );
    }
    IndexedFrame frame{
        std::vector<std::uint8_t>(width * height, 0)
    };
    std::vector<ModelPolygonTrace> traces;
    drawLevel2Entities(frame, assets, model, &traces);
    return modelDrawTraceJson(
        traces,
        "loop_tick",
        model.level2LoopTick()
    );
}

std::string level2RadarContactTraceJson(const GameplayModel& model) {
    if (
        model.phase() != GameplayPhase::level2Handoff
        && model.phase() != GameplayPhase::level2
        && model.phase() != GameplayPhase::level3Handoff
        && model.phase() != GameplayPhase::level3
    ) {
        throw std::runtime_error(
            "radar contact trace requires active Level 2/3 state"
        );
    }
    IndexedFrame frame{
        std::vector<std::uint8_t>(width * height, 0)
    };
    std::vector<RadarContactTrace> traces;
    drawLevel2Radar(frame, model, &traces);
    std::ostringstream output;
    output
        << "{\"format_version\":1,\"loop_tick\":"
        << model.level2LoopTick()
        << ",\"contacts\":[";
    for (std::size_t index = 0; index < traces.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        const auto& trace = traces[index];
        output
            << "{\"entity_index\":" << trace.entityIndex
            << ",\"active_ordinal\":" << trace.activeOrdinal
            << ",\"entity_type\":" << trace.entityType
            << ",\"category\":" << static_cast<int>(trace.category)
            << ",\"position\":["
            << trace.position.x << ',' << trace.position.y << ','
            << trace.position.z << "],\"local\":["
            << trace.local.x << ',' << trace.local.y << ','
            << trace.local.z << "],\"projected\":["
            << trace.projected.x << ',' << trace.projected.y
            << "],\"draw_x\":" << (trace.projected.x - 4)
            << ",\"color\":" << static_cast<unsigned>(trace.color)
            << ",\"visible\":" << (trace.visible ? "true" : "false")
            << '}';
    }
    output << "]}";
    return output.str();
}

IndexedScreen renderGameplayIndexedInternal(
    const GameplayAssets& assets,
    const GameplayModel& model,
    const IndexedScreen* previous,
    const bool redrawViewportFromPage
) {
    if (model.phase() == GameplayPhase::continuePrompt) {
        return renderContinuePromptIndexed(assets, model);
    }
    if (model.phase() == GameplayPhase::campaignEnded) {
        throw std::runtime_error("campaign results and return-to-menu presentation are not implemented");
    }
    if (
        model.phase() == GameplayPhase::failurePreExplosionBlackout
        || model.phase() == GameplayPhase::failurePostExplosionBlackout
    ) {
        auto palette = assets.uiPalette.size() == 256
            ? assets.uiPalette : assets.cockpit.palette;
        if (model.phase() == GameplayPhase::failurePostExplosionBlackout
            && assets.emulateTargetPaletteTiming) {
            palette = failureBlackoutPalette(assets);
        }
        return {
            std::vector<std::uint8_t>(width * height, 0),
            std::move(palette),
        };
    }
    if (
        model.phase() == GameplayPhase::level3SuccessPresentation
        && assets.useUnprovenLevel3SuccessSurfaces
        && (
            !model.level3CompletionByPickup()
                ? !assets.level3SuccessFrames.empty()
                : !assets.level3ContactSuccessFrames.empty()
        )
    ) {
        const auto& frames = model.level3CompletionByPickup()
            ? assets.level3ContactSuccessFrames
            : assets.level3SuccessFrames;
        auto frameIndex = std::min<std::size_t>(
            model.presentationFrame(),
            frames.size() - 1
        );
        if (model.level3CompletionByPickup()) {
            frameIndex = std::min<std::size_t>(
                frameIndex,
                GameplayModel::level3ContactPresenterSurfaceFrames - 1
            );
        }
        auto frame = frames[frameIndex];
        if (
            frame.pixels.size() != width * height
            || frame.palette.size() != 256
        ) {
            throw std::runtime_error(
                "Level 3 success capture frame is not 320x200/256-color"
            );
        }
        if (model.level3CompletionByPickup()) {
            frame.palette = level3ContactPalette(
                frame.palette,
                model.presentationFrame()
            );
        }
        return frame;
    }
    if (model.phase() == GameplayPhase::level4Handoff) {
        const auto& frames = assets.level3HandoffFrames;
        if (assets.useUnprovenLevel3SuccessSurfaces && !frames.empty()) {
            const auto& frame = frames.front();
            if (
                frame.pixels.size() != width * height
                || frame.palette.size() != 256
            ) {
                throw std::runtime_error(
                    "Level 3 handoff capture frame is not 320x200/256-color"
                );
            }
            return frame;
        }
        // Synthetic tests and direct callers may omit the optional handoff
        // capture. Preserve the terminal surface rather than inventing a
        // Level 4 viewport.
        const auto& fallbackFrames = model.level3CompletionByPickup()
            ? assets.level3ContactSuccessFrames
            : assets.level3SuccessFrames;
        if (
            assets.useUnprovenLevel3SuccessSurfaces
            && !fallbackFrames.empty()
        ) {
            return fallbackFrames.back();
        }
    }
    if (model.phase() == GameplayPhase::failureExplosion) {
        if (assets.cockpit.palette.size() != 256) {
            throw std::runtime_error(
                "gameplay palette does not have 256 entries"
            );
        }
        IndexedFrame indexed{
            std::vector<std::uint8_t>(width * height, 0)
        };
        drawFailurePresenter(indexed, assets, model);
        return {
            std::move(indexed.pixels),
            failurePalette(assets.cockpit.palette),
        };
    }
    if (assets.cockpit.pixels.size() != width * height) {
        throw std::runtime_error("cockpit frame is not 320x200");
    }
    if (assets.cockpit.palette.size() != 256) {
        throw std::runtime_error("gameplay palette does not have 256 entries");
    }

    const bool useHistory =
        previous != nullptr
        && isLevel3Phase(model.phase())
        && previous->pixels.size() == width * height;
    IndexedFrame indexed{
        useHistory ? previous->pixels : assets.cockpit.pixels,
    };
    const bool successSurface = (
        model.phase() == GameplayPhase::successPresentation
        || model.phase() == GameplayPhase::level2SuccessPresentation
        || model.phase() == GameplayPhase::level3SuccessPresentation
    );
    if (
        model.phase() == GameplayPhase::level2Handoff
        || model.phase() == GameplayPhase::level2
        || isLevel3Phase(model.phase())
    ) {
        // Whole-viewport history is a diagnostic negative control. Native
        // Mode-X page updates retain the destination page but redraw the
        // current viewport before the cockpit presenters run.
        if (!useHistory || redrawViewportFromPage) {
            drawLevel2HandoffViewport(indexed, assets, model);
        }
        drawLevel2Entities(indexed, assets, model);
        drawSight(indexed, model);
        drawCockpitComposition(indexed, assets, model);
        if (useHistory) {
            // The central six-row cockpit strip is part of the native cached
            // page. The ordinary presenter redraw can overwrite stale bytes
            // there, while the native partial-redraw path retains them.
            for (int y = 121; y < 127; ++y) {
                for (int x = 128; x < 184; ++x) {
                    indexed.pixels[static_cast<std::size_t>(y * width + x)] =
                        previous->pixels[
                            static_cast<std::size_t>(y * width + x)
                        ];
                }
            }
        }
        if (
            isLevel3Phase(model.phase())
            && assets.useUnprovenLevel3GuardianSurface
            && !assets.level3GuardianFrames.empty()
        ) {
            overlayLevel3GuardianStaticSurface(indexed, assets);
            if (!useHistory) {
                overlayLevel3IndicatorPageTransfer(indexed, assets, model);
            }
            clearLevel3GuardianStaticRadarContacts(indexed);
        }
        drawLevel2Radar(indexed, model);
        drawArmorBar(indexed, model);
    } else if (successSurface) {
        // FUN_1000_6f52 copies the pristine cockpit page into the render page.
        // Its viewport is the stored radial portal, not the final world frame.
        // The completion caller enters the shared presenter before the
        // ordinary Level 1 radar list is rebuilt. Cached HUD fields are
        // already current, so the pristine page keeps its blank radar,
        // header, zero score, default panels, and full armor strip. In
        // particular, do not project the model's initial Level 1 contact into
        // a Level 3 success frame.
        drawCockpitComposition(indexed, assets, model, false);
        // The sight is the final shared-presenter primitive; drawing it after
        // the cockpit page preserves the four outer brackets that the native
        // terminal frame leaves visible over the green viewport.
        drawSight(indexed, model);
        drawArmorBar(indexed, model, true);
        if (model.phase() == GameplayPhase::level2SuccessPresentation) {
            // 6f52 copies only the upper 146 rows. The lower page retains
            // the live score/lamp, and 698a redraws the existing radar list.
            // The blank campaign header belongs to the copied upper page.
            drawLevel1Score(indexed, model.score(), 240, &assets.scoreFont);
            if (model.weaponIndicatorVisible()) {
                blitArt(indexed, assets, 4,
                        model.weaponReadyIndicator() ? 40 : 39, 243, 170);
            }
            drawLevel2Radar(indexed, model);
        }
    } else {
        drawViewportBase(indexed, assets, model);
        drawCombat(indexed, assets, model);
        drawCockpitComposition(indexed, assets, model);
        drawRadarContacts(indexed, model);
        drawArmorBar(indexed, model);
    }
    auto palette = gameplayPalette(assets, model);
    if (isLevel3Phase(model.phase()) && !assets.level3GuardianFrames.empty()) {
        const auto& guardianPalette = assets.level3GuardianFrames.front().palette;
        if (guardianPalette.size() == 256) {
            palette = guardianPalette;
        }
    }
    if (successSurface) {
        palette = level1SuccessPalette(
            palette,
            level1SuccessPhaseOrdinal(model.presentationFrame())
        );
    }
    return {std::move(indexed.pixels), std::move(palette)};
}

IndexedScreen renderLevel1ViewportIndexed(const GameplayAssets& assets, const GameplayModel& model) {
    if (model.phase() != GameplayPhase::level1) {
        throw std::invalid_argument("Level1 viewport requires an ordinary frame");
    }
    if (assets.cockpit.pixels.size() != width * height || assets.cockpit.palette.size() != 256) {
        throw std::invalid_argument("Level1 viewport requires complete indexed assets");
    }
    IndexedFrame indexed{assets.cockpit.pixels};
    if(WorldPresentationCapture::enabled()) {
        indexed.presentation=std::make_shared<WorldPresentationRecorder>(indexed.pixels,model.campaignLevel(),
            static_cast<unsigned>(model.phase()),model.level1State().loopTick());
    }
    drawViewportBase(indexed, assets, model);
    drawCombat(indexed, assets, model);
    const auto presentation=indexed.presentation?indexed.presentation->finish(indexed.pixels):nullptr;
    return {std::move(indexed.pixels), gameplayPalette(assets, model),presentation};
}

static IndexedScreen renderLevel2Viewport(const GameplayAssets& assets, const GameplayModel& model,
    const EffectProjector& effectProjector, const BeamProjectionSink& beamProjection,
    const RecoveredModelBank bank, const std::optional<std::int16_t> playableDetail = std::nullopt) {
    if (model.phase() != GameplayPhase::level2Handoff && model.phase() != GameplayPhase::level2
        && model.phase() != GameplayPhase::level5 && model.phase() != GameplayPhase::level3Native
        && model.phase() != GameplayPhase::playable
        && !model.pendingLevel2FailureTail()) {
        throw std::invalid_argument("Level2 viewport requires an ordinary frame");
    }
    if (assets.cockpit.pixels.size() != width * height || assets.cockpit.palette.size() != 256) {
        throw std::invalid_argument("Level2 viewport requires complete indexed assets");
    }
    IndexedFrame indexed{assets.cockpit.pixels};
    if(WorldPresentationCapture::enabled()) {
        indexed.presentation=std::make_shared<WorldPresentationRecorder>(indexed.pixels,model.campaignLevel(),
            static_cast<unsigned>(model.phase()),model.level2LoopTick());
        indexed.presentation->recordDestructions(model.destructions(),model.ticks());
    }
    drawLevel2HandoffViewport(indexed, assets, model, playableDetail);
    drawLevel2Entities(indexed, assets, model, nullptr, effectProjector, beamProjection, bank);
    drawSight(indexed, model);
    const auto chase=indexed.presentation&&bank==RecoveredModelBank::playable&&!model.pendingLevel2FailureTail()
        ?captureChasePresentation(assets,model):nullptr;
    const auto presentation=indexed.presentation?indexed.presentation->finish(indexed.pixels,chase):nullptr;
    return {std::move(indexed.pixels), gameplayPalette(assets, model),presentation};
}

IndexedScreen renderLevel2ViewportIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const EffectProjector& effectProjector, const BeamProjectionSink& beamProjection) {
    return renderLevel2Viewport(assets, model, effectProjector, beamProjection, RecoveredModelBank::legacy);
}

IndexedScreen renderPlayableLevel2ViewportIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const EffectProjector& effectProjector, const BeamProjectionSink& beamProjection) {
    return renderLevel2Viewport(assets, model, effectProjector, beamProjection, RecoveredModelBank::playable);
}

IndexedScreen renderPlayableLevel2ViewportIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const EffectProjector& effectProjector, const BeamProjectionSink& beamProjection, const std::int16_t detail) {
    if (!nativeDetailSettingSupported(detail)) { throw std::invalid_argument("unsupported native render detail"); }
    return renderLevel2Viewport(assets, model, effectProjector, beamProjection, RecoveredModelBank::playable, detail);
}

IndexedScreen renderFullDetailPlayableLevel2ViewportIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const EffectProjector& effectProjector, const BeamProjectionSink& beamProjection, const std::int16_t detail) {
    if (!nativeFullDetailSettingSupported(detail)) { throw std::invalid_argument("unsupported native full render detail"); }
    return renderLevel2Viewport(assets, model, effectProjector, beamProjection, RecoveredModelBank::playable, detail);
}

IndexedScreen renderContinuePageIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const IndexedScreen& destination, const ContinuePagePart part) {
    if (destination.pixels.size() != width * height || destination.palette.size() != 256) {
        throw std::invalid_argument("retained Continue page is incomplete");
    }
    IndexedFrame frame{destination.pixels};
    drawContinuePage(frame, assets, model, part);
    return {std::move(frame.pixels), destination.palette};
}

static IndexedScreen renderFailurePage(const GameplayAssets& assets, const GameplayModel& model,
    const IndexedScreen& destination, const Viewport backgroundClip, const FailureRenderDomain domain,
    const std::optional<std::int16_t> playableDetail = std::nullopt) {
    if (model.phase() != GameplayPhase::failureExplosion
        || destination.pixels.size() != width * height || assets.cockpit.palette.size() != 256
        || backgroundClip.left != 0 || backgroundClip.top != 0 || backgroundClip.bottom != height - 1
        || (backgroundClip.right != width - 1 && backgroundClip.right != width - 2)
        || (domain == FailureRenderDomain::source
            && std::any_of(model.failureMissionEntities().begin(), model.failureMissionEntities().end(),
                [](const auto& entity) { return entity.type < 0; }))) {
        throw std::invalid_argument("unsupported retained destruction page");
    }
    if (domain == FailureRenderDomain::playable) {
        for (const auto& entity : model.failureMissionEntities()) {
            if (entity.type < 0) { static_cast<void>(failureBitmap(assets, entity)); }
        }
    }
    IndexedFrame indexed{destination.pixels};
    drawFailurePresenter(indexed, assets, model, nullptr, backgroundClip, domain, playableDetail);
    return {std::move(indexed.pixels), failurePalette(assets.cockpit.palette)};
}

IndexedScreen renderFailurePageIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const IndexedScreen& destination, const Viewport backgroundClip) {
    return renderFailurePage(assets, model, destination, backgroundClip, FailureRenderDomain::source);
}

IndexedScreen renderPlayableFailurePageIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const IndexedScreen& destination, const Viewport backgroundClip) {
    return renderFailurePage(assets, model, destination, backgroundClip, FailureRenderDomain::playable);
}

IndexedScreen renderPlayableFailurePageIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const IndexedScreen& destination, const Viewport backgroundClip, const std::int16_t detail) {
    if (!nativeDetailSettingSupported(detail)) { throw std::invalid_argument("unsupported native failure detail"); }
    return renderFailurePage(assets, model, destination, backgroundClip, FailureRenderDomain::playable, detail);
}

IndexedScreen renderFullDetailPlayableFailurePageIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const IndexedScreen& destination, const Viewport backgroundClip, const std::int16_t detail) {
    if (!nativeFullDetailSettingSupported(detail)) { throw std::invalid_argument("unsupported native full failure detail"); }
    return renderFailurePage(assets, model, destination, backgroundClip, FailureRenderDomain::playable, detail);
}

IndexedScreen renderGameplayIndexed(
    const GameplayAssets& assets,
    const GameplayModel& model
) {
    return renderGameplayIndexedInternal(assets, model, nullptr, false);
}

IndexedScreen renderGameplayIndexedWithHistory(
    const GameplayAssets& assets,
    const GameplayModel& model,
    const IndexedScreen& previous
) {
    return renderGameplayIndexedInternal(assets, model, &previous, false);
}

IndexedScreen renderGameplayIndexedWithPageHistory(
    const GameplayAssets& assets,
    const GameplayModel& model,
    const IndexedScreen& previous
) {
    return renderGameplayIndexedInternal(assets, model, &previous, true);
}

RgbFrame renderGameplay(
    const GameplayAssets& assets,
    const GameplayModel& model
) {
    const auto indexed = renderGameplayIndexed(assets, model);
    RgbFrame frame{width, height, std::vector<Rgb>(width * height)};
    for (std::size_t offset = 0; offset < indexed.pixels.size(); ++offset) {
        frame.pixels[offset] = indexed.palette[indexed.pixels[offset]];
    }
    return frame;
}

}  // namespace stellar7
