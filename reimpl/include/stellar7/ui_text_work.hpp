#pragma once

#include "stellar7/indexed_pages.hpp"

#include <string_view>

namespace stellar7 {

enum class UiFontLayout { fixed, proportional };

struct UiTextWidthWork {
    std::uint16_t width;
    std::vector<NormalWorkOperation> operations;
};

struct UiTextWork {
    std::uint16_t width;
    std::vector<NormalWorkOperation> operations;
    std::vector<IndexedPageEffect> effects;
    // One-based source dispatch of each pixel store, excluding IRQs/restarts.
    std::vector<std::uint64_t> pixelDispatches;
};

//49E4 through RETF; excludes caller. Explicit layout preserves the source branch.
UiTextWidthWork uiTextWidthOperations(const BitmapFont& font, UiFontLayout layout,
    std::string_view text);

//4C0E/4A4C and Mode-X0FEB through RETF; excludes outer CALLF and IRQs.
//Transparent style1, valid glyphs wholly inside the source's direct-pixel bounds.
UiTextWork uiTextOperations(const BitmapFont& font, UiFontLayout layout,
    std::string_view text, ProjectedPoint origin, std::uint8_t color,
    std::uint16_t page, Viewport viewport);

} // namespace stellar7
