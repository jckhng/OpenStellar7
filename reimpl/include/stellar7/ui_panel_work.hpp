#pragma once

#include "stellar7/ui_bitmap_work.hpp"

namespace stellar7 {

struct UiPanelColors {
    std::array<std::uint8_t, 7> edges;
    std::uint8_t light;
    std::uint8_t middle;
    std::uint8_t dark;
};

struct UiPanelSurface {
    Viewport viewport;
    PolygonClipMode clipping;
    std::uint8_t rasterStyle;
    std::uint8_t lineColor;
};

struct UiPanelWork {
    std::vector<NormalWorkOperation> operations;
    std::vector<IndexedPageEffect> effects;
    UiPanelSurface finalSurface;
};

// Mode-X52AA through RET; includes54CE/545E/5BFE and their ordered children.
// Native40x40 tile, inclusive screen clipping; separate2E25 fill is excluded.
UiPanelWork uiPanelOperations(const IndexedBitmap& tile, CockpitCopyRegion rectangle,
    const UiPanelColors& colors, std::uint16_t page);

//5642 through RET, normal style1/fullscreen clipping; eight ordered axis lines.
UiPanelWork uiBevelOperations(CockpitCopyRegion rectangle, const UiPanelColors& colors,
    std::uint16_t page);

}
