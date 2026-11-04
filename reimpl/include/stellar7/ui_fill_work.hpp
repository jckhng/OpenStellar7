#pragma once

#include "stellar7/indexed_pages.hpp"

namespace stellar7 {

struct UiFillWork {
    std::vector<NormalWorkOperation> operations;
    std::vector<IndexedPageEffect> effects;
};

//2E25 through RETF, equal colors/style1/native scale, no page/color remapping.
//Full-display clipping enabled; rectangle wholly inside320x200. Caller excluded.
UiFillWork uiFillOperations(CockpitCopyRegion rectangle, std::uint8_t color, std::uint16_t page);

} // namespace stellar7
