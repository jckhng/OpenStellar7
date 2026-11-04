#include "briefing_strip.hpp"
#include "stellar7/failure_model_work.hpp"
#include "stellar7/raster_line_work.hpp"

#include <array>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr Viewport preview{50, 295, 35, 109}; //Copy width246; geometry view ends294.
constexpr Viewport ground{50, 295, 72, 109}; //2A33..2A45 direct clip writes.
constexpr Viewport rasterViewport{0, 318, 0, 199};
constexpr std::uint8_t skyColor = 159;
constexpr std::uint8_t groundColor = 147; //DS12F4.
constexpr std::uint8_t starColor = 87; //DS12FB, not a guessed white star.
constexpr std::uint8_t gridColor = 141; //DS12F5.
constexpr std::array<ProjectedPoint, 14> starOffsets{{
    {5,20}, {27,12}, {43,22}, {63,25}, {97,31}, {115,30}, {100,5},
    {125,21}, {235,35}, {147,11}, {183,27}, {220,9}, {163,19}, {117,32}
}};

void pixel(std::vector<IndexedPageEffect>& effects, const int x, const int y,
    const std::uint8_t color, const std::uint16_t page) {
    effects.emplace_back(RadarPixelWrite{{static_cast<std::int16_t>(x),
        static_cast<std::int16_t>(y)}, color, page});
}

void horizontal(std::vector<IndexedPageEffect>& effects, const int y,
    const std::uint8_t color, const std::uint16_t page) {
    if (y < ground.top || y > ground.bottom) {
        return;
    }
    for (int x = ground.left; x <= ground.right; ++x) {
        pixel(effects, x, y, color, page);
    }
}

void gridLine(std::vector<IndexedPageEffect>& effects, const ProjectedPoint end,
    const std::uint16_t page) {
    //Same established pixel-only full-screen kernel/crop as the preview candidate.
    //No instruction-work or clipped-endpoint fidelity claim escapes this helper.
    const auto raster = ownedPolygonOperations({{173,72}, end}, rasterViewport,
        PolygonClipMode::Enabled, FailurePolygonStyle::outline, gridColor, gridColor);
    for (const auto& value : raster.pixels) {
        if (value.point.x >= ground.left && value.point.x <= ground.right
            && value.point.y >= ground.top && value.point.y <= ground.bottom) {
            effects.emplace_back(RadarPixelWrite{value.point, value.color, page});
        }
    }
}
}

std::vector<IndexedPageEffect> briefingStripEffects(const std::int16_t index,
    const GameplayAssets& assets, const std::uint16_t page) {
    if (assets.uiPalette.size() != 256 || (page != 0xa000 && page != 0xa400 && page != 0xa800)) {
        throw std::invalid_argument("briefing strip requires the retained native palette/page");
    }
    std::vector<IndexedPageEffect> effects;
    for (int y = preview.top; y <= preview.bottom; ++y) {
        for (int x = preview.left; x <= preview.right; ++x) {
            pixel(effects, x, y, skyColor, page);
        }
    }
    //28CB..28FD: native VGA profile rows71..56 use unsigned144..159.
    for (int row = 0; row < 16; ++row) {
        for (int x = preview.left; x <= preview.right; ++x) {
            pixel(effects, x, 71 - row, static_cast<std::uint8_t>(144 + row), page);
        }
    }
    for (int y = 73; y <= preview.bottom; ++y) {
        horizontal(effects, y, groundColor, page);
    }
    for (const auto star : starOffsets) {
        pixel(effects, star.x + 50, star.y + 35, starColor, page);
    }
    //2A4B..2AB8: source fan lines, then the two horizon lines.
    for (int offset = 0; offset < 150; offset += 30) {
        gridLine(effects, {static_cast<std::int16_t>(173 - offset), 110}, page);
        gridLine(effects, {static_cast<std::int16_t>(173 + offset), 110}, page);
    }
    for (int y = 102; y > 70; y -= 8) {
        gridLine(effects, {50, static_cast<std::int16_t>(y)}, page);
        gridLine(effects, {296, static_cast<std::int16_t>(y)}, page);
    }
    horizontal(effects, 72, gridColor, page);
    horizontal(effects, 73, gridColor, page);
    //2AE7..2B09: wrapped16 numerator, CWD/IDIV truncates towardzero, divisors1..14.
    const auto numerator = static_cast<std::int16_t>(static_cast<std::uint16_t>(index) + 75U);
    for (int divisor = 1; divisor < 15; ++divisor) {
        const auto y = static_cast<std::int16_t>(numerator / divisor + 62);
        horizontal(effects, y, gridColor, page);
    }
    return effects;
}
} // namespace stellar7
