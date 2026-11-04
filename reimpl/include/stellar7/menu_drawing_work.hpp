#pragma once

#include "stellar7/ui_panel_work.hpp"
#include "stellar7/ui_text_work.hpp"

namespace stellar7 {

struct MenuDrawingWork {
    std::vector<NormalWorkOperation> operations;
    std::vector<IndexedPageEffect> effects;
    std::vector<std::uint8_t> scratch;
    UiPanelSurface finalSurface;
    std::uint16_t selectedFont;
    std::uint8_t textColor;
    std::uint16_t widgetCount;
};

//16B0 through17DE; stops before17E1 selects FRONT and8484 copies it.
//Includes caller/child work, except the native, untimed4848 font selection body.
//Copies five complete source records into the retained500-byte allocation.
MenuDrawingWork menuDrawingOperations(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, const std::vector<std::uint8_t>& scratch,
    std::uint16_t backPage);

} // namespace stellar7
