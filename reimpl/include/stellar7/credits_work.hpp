#pragma once

#include "stellar7/ui_panel_work.hpp"
#include "stellar7/ui_text_work.hpp"

namespace stellar7 {

struct CreditsColors {
    UiPanelColors panel;
    std::uint8_t fill;
};

struct CreditsDrawingWork {
    std::vector<NormalWorkOperation> operations;
    std::vector<IndexedPageEffect> effects;
    UiPanelSurface finalSurface;
    std::size_t leaderDots = 0;
};

//B12A zero19CA through B27E, before its first blackout CALL.
//Includes shared draw bodies; font-selection providers are native/untimed.
CreditsDrawingWork creditsDrawingOperations(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, const BitmapFont& scoreFont, const CreditsColors& colors,
    std::uint16_t backPage);

} // namespace stellar7
