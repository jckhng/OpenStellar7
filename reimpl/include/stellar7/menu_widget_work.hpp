#pragma once

#include "stellar7/indexed_render.hpp"
#include "stellar7/ui_bitmap_work.hpp"

namespace stellar7 {

struct MenuWidgetWork {
    std::vector<NormalWorkOperation> operations;
    std::vector<IndexedPageEffect> effects;
    std::uint8_t textColor;
};

//583A through RET, enabled retained record with a source menu label/FNT0 text.
//Full viewport/style1; normal bitmap flags0. Excludes outer CALL and IRQs.
//Reads the existing500-byte allocation; leaves DRAW=BACK and textstyle1.
MenuWidgetWork menuWidgetOperations(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, const std::vector<std::uint8_t>& scratch,
    std::size_t index, MenuWidgetState state, std::uint16_t backPage);

//58FA through RET, including22A1 JMPF and106A; outer CALL/IRQs excluded.
//Uses the record copy rectangle, including source planar alignment/overcopy.
//Leaves COPY=BACK and DRAW=FRONT, even for a disabled record.
UiBitmapWork menuWidgetCopyOperations(const std::vector<std::uint8_t>& scratch,
    std::size_t index, std::uint16_t backPage, std::uint16_t frontPage);

} // namespace stellar7
