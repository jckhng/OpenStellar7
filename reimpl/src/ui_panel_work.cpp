#include "stellar7/ui_panel_work.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace stellar7 {
namespace {
using Kind = NormalWorkKind;
constexpr Viewport display{0, 319, 0, 199};
constexpr int tileSize = 40;
constexpr int borderSteps = 7;
constexpr std::uint16_t firstPage = 0xa000;
constexpr std::uint16_t pageSpacing = 0x400;

void instructions(UiPanelWork& work, const std::uint64_t count) {
    if (!work.operations.empty() && work.operations.back().kind == Kind::instructions) {
        work.operations.back().count += count;
        return;
    }
    work.operations.push_back({Kind::instructions, count});
}

void port8(UiPanelWork& work) { work.operations.push_back({Kind::port8Write, 1}); }
void port16(UiPanelWork& work) { work.operations.push_back({Kind::port16Write, 1}); }
void string(UiPanelWork& work) { work.operations.push_back({Kind::string, 1}); }

void pixel(UiPanelWork& work, const int x, const int y, const std::uint8_t color,
    const std::uint16_t page) {
    work.effects.push_back(RadarPixelWrite{{static_cast<std::int16_t>(x),
        static_cast<std::int16_t>(y)}, color, page});
}

void requireRectangle(const CockpitCopyRegion rectangle, const std::uint16_t page) {
    if (rectangle.x < 0 || rectangle.y < 0 || rectangle.width < 14 || rectangle.height < 14
        || int{rectangle.x} + rectangle.width > 320 || int{rectangle.y} + rectangle.height > 200
        || page < firstPage || page > firstPage + 2 * pageSpacing
        || (page - firstPage) % pageSpacing != 0) {
        throw std::invalid_argument("unsupported source UI panel rectangle");
    }
}

//The shared ground line admission remains unchanged; this recovers only UI axes.
void axisLine(UiPanelWork& work, int x0, int y0, int x1, int y1,
    const std::uint8_t color, const std::uint16_t page) {
    if (x0 != x1 && y0 != y1) {
        throw std::invalid_argument("UI line is not axis aligned");
    }
    instructions(work, 14); //21B4..21D0, clipping enabled.
    const auto clip = [&](const bool horizontal, const int boundary, const bool high) {
        int& first = horizontal ? x0 : y0;
        int& second = horizontal ? x1 : y1;
        const auto outside = [&](const int coordinate) {
            return high ? static_cast<std::uint16_t>(coordinate) > boundary : coordinate < boundary;
        };
        instructions(work, 3);
        const bool firstOutside = outside(first);
        instructions(work, 2);
        if (firstOutside && outside(second)) {
            instructions(work, 1 + 5); //2218 JMP plus229C epilogue.
            return false;
        }
        if (!firstOutside && !outside(second)) {
            return true;
        }
        if (!firstOutside) {
            std::swap(x0, x1);
            std::swap(y0, y1);
            instructions(work, 3);
        }
        instructions(work, 11); //Signed IMUL/IDIV intersection; other axis is constant.
        first = boundary;
        return true;
    };
    if (!clip(false, display.top, false) || !clip(true, display.left, false)
        || !clip(false, display.bottom, true) || !clip(true, display.right, true)) {
        return;
    }
    instructions(work, 4);
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
        instructions(work, 2);
    }
    instructions(work, 1 + 18); //2298 CALLF;0283..02AF.
    if (x0 == x1 && y0 == y1) {
        instructions(work, 8); port16(work); instructions(work, 2);
        string(work); instructions(work, 3);
        pixel(work, x0, y0, color, page);
    } else if (y0 == y1) {
        instructions(work, 11 + 9); //0283 horizontal arm and0205..0217.
        if (((x0 ^ x1) & ~3) == 0) {
            instructions(work, 14); port16(work); instructions(work, 1);
            string(work); instructions(work, 1);
        } else {
            instructions(work, 11); port16(work); instructions(work, 2);
            string(work); instructions(work, 1); port8(work); instructions(work, 6);
            const int bulk = x1 - x0 + 1 - 4 + (x0 & 3);
            if ((bulk >> 2) & 1) {
                string(work);
            }
            work.operations.push_back({Kind::repeatString, static_cast<std::uint64_t>(bulk >> 3)});
            instructions(work, 2); port8(work); instructions(work, 1);
            string(work); instructions(work, 1);
        }
        instructions(work, 3);
        for (int x = x0; x <= x1; ++x) {
            pixel(work, x, y0, color, page);
        }
    } else {
        instructions(work, 9); port16(work); instructions(work, 4);
        if (y1 < y0) {
            instructions(work, 4);
        }
        instructions(work, 3);
        for (int y = std::min(y0, y1); y <= std::max(y0, y1); ++y) {
            instructions(work, 1); string(work); instructions(work, 2);
            pixel(work, x0, y, color, page);
        }
        instructions(work, 3);
    }
    instructions(work, 5); //229C..22A0.
}

void boundedLine(UiPanelWork& work, int x0, int y0, int x1, int y1,
    const std::uint8_t color, const std::uint16_t page) {
    instructions(work, 6 + 2); //545E..546B.
    if (x0 == x1) {
        instructions(work, 2);
        if (x0 < 0) {
            instructions(work, 5);
            return;
        }
        instructions(work, 2);
        if (x0 > display.right) {
            instructions(work, 5);
            return;
        }
    } else {
        instructions(work, 2);
        if (x0 < 0) {
            x0 = 0;
            instructions(work, 1);
        }
        instructions(work, 2);
        if (x1 > display.right) {
            x1 = display.right;
            instructions(work, 1);
        }
    }
    instructions(work, 3);
    if (y0 == y1) {
        instructions(work, 2);
        if (y0 > display.bottom) {
            instructions(work, 5);
            return;
        }
        instructions(work, 2);
        if (y0 < 0) {
            instructions(work, 5);
            return;
        }
        instructions(work, 2);
    } else {
        instructions(work, 2);
        if (y0 < 0) {
            y0 = 0;
            instructions(work, 1);
        }
        instructions(work, 3);
        if (y1 > display.right) { //Original54B1 uses319 for the vertical high clamp.
            y1 = display.right;
            instructions(work, 1);
        }
    }
    instructions(work, 5);
    axisLine(work, x0, y0, x1, y1, color, page);
    instructions(work, 6);
}

void tiledPanel(UiPanelWork& work, const IndexedBitmap& tile,
    const CockpitCopyRegion rectangle, const UiPanelColors& colors, const std::uint16_t page) {
    const auto x = rectangle.x;
    const auto y = rectangle.y;
    const int right = x + rectangle.width - 1;
    const int bottom = y + rectangle.height - 1;
    instructions(work, 24 + 1); //54CE..550B; positive admitted dimensions.
    for (int row = y; row < bottom; row += tileSize) {
        instructions(work, 3);
        for (int column = x; column < right; column += tileSize) {
            instructions(work, 6);
            const auto bitmap = uiBitmapOperations(tile,
                {static_cast<std::int16_t>(column), static_cast<std::int16_t>(row)},
                {x, static_cast<std::int16_t>(right), y, static_cast<std::int16_t>(bottom)}, page);
            work.operations.insert(work.operations.end(), bitmap.operations.begin(), bitmap.operations.end());
            work.effects.insert(work.effects.end(), bitmap.effects.begin(), bitmap.effects.end());
            instructions(work, 4);
        }
        instructions(work, 5);
    }
    instructions(work, 2 + 6 + 12); //553D restore/CALL5BFE, full reset, first line setup.
    axisLine(work, x, y, right, y, colors.dark, page);
    instructions(work, 11);
    axisLine(work, right, y, right, bottom, colors.dark, page);
    instructions(work, 8);
    axisLine(work, x, bottom, right, bottom, colors.middle, page);
    instructions(work, 6);
    axisLine(work, x, y, x, bottom, colors.middle, page);
    instructions(work, 6);
}
}

UiPanelWork uiPanelOperations(const IndexedBitmap& tile, const CockpitCopyRegion rectangle,
    const UiPanelColors& colors, const std::uint16_t page) {
    requireRectangle(rectangle, page);
    if (tile.width != tileSize || tile.height != tileSize) {
        throw std::invalid_argument("source UI panel requires40x40 tile");
    }
    UiPanelWork work{{}, {}, {display, PolygonClipMode::Enabled, 1, colors.edges.back()}};
    instructions(work, 8 + 7); //52AA native palette branch and54CE CALL setup.
    tiledPanel(work, tile, rectangle, colors, page);
    instructions(work, 3 + 6 + 5); //52EB..5306 including5BFE.
    const auto x = rectangle.x;
    const auto y = rectangle.y;
    const int right = x + rectangle.width;
    const int bottom = y + rectangle.height;
    for (int inset = 0; inset < borderSteps; ++inset) {
        const auto color = colors.edges[inset];
        instructions(work, 19);
        axisLine(work, x + inset, y + inset, right - inset, y + inset, color, page);
        instructions(work, 10);
        axisLine(work, x + inset, bottom - inset, right - inset, bottom - inset, color, page);
        instructions(work, 6);
        axisLine(work, x + inset, y + inset, x + inset, bottom - inset, color, page);
        instructions(work, 6);
        axisLine(work, right - inset, y + inset, right - inset, bottom - inset, color, page);
        instructions(work, inset + 1 < borderSteps ? 6 : 5);
    }
    instructions(work, 12);
    const int quarterWidth = rectangle.width / 4;
    const int quarterHeight = rectangle.height / 4;
    for (int inset = -1; inset < borderSteps - 1; ++inset) {
        const auto color = colors.edges[inset + 1];
        instructions(work, 30);
        boundedLine(work, x + quarterWidth + inset, y + inset,
            right - quarterWidth - inset, y + inset, color, page);
        instructions(work, 9);
        boundedLine(work, x + quarterWidth + inset, bottom - inset,
            right - quarterWidth - inset, bottom - inset, color, page);
        instructions(work, 17);
        boundedLine(work, x + inset, y + quarterHeight + inset,
            x + inset, bottom - quarterHeight - inset, color, page);
        instructions(work, 9);
        boundedLine(work, right - inset, y + quarterHeight + inset,
            right - inset, bottom - quarterHeight - inset, color, page);
        instructions(work, inset + 1 < borderSteps - 1 ? 6 : 5);
    }
    instructions(work, 6);
    return work;
}

UiPanelWork uiBevelOperations(const CockpitCopyRegion rectangle, const UiPanelColors& colors,
    const std::uint16_t page) {
    requireRectangle(rectangle, page);
    UiPanelWork work{{}, {}, {display, PolygonClipMode::Enabled, 1, colors.middle}};
    const auto x = rectangle.x;
    const auto y = rectangle.y;
    const int right = x + rectangle.width - 1;
    const int bottom = y + rectangle.height - 1;
    instructions(work, 18);
    axisLine(work, x, y, right, y, colors.light, page);
    instructions(work, 10);
    axisLine(work, right, y, right, bottom, colors.light, page);
    instructions(work, 8);
    axisLine(work, x, y, x, bottom, colors.dark, page);
    instructions(work, 6);
    axisLine(work, x, bottom, right, bottom, colors.dark, page);
    instructions(work, 17);
    axisLine(work, x + 1, y + 1, right - 1, y + 1, colors.light, page);
    instructions(work, 13);
    axisLine(work, right - 1, y + 1, right - 1, bottom - 1, colors.light, page);
    instructions(work, 8);
    axisLine(work, x + 1, y + 1, x + 1, bottom - 1, colors.middle, page);
    instructions(work, 6);
    axisLine(work, x + 1, bottom - 1, right - 1, bottom - 1, colors.middle, page);
    instructions(work, 6);
    return work;
}

}
