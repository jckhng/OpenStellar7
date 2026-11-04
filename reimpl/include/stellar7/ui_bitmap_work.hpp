#pragma once

#include "stellar7/indexed_pages.hpp"

namespace stellar7 {

struct UiBitmapWork {
    std::vector<NormalWorkOperation> operations;
    std::vector<IndexedPageEffect> effects;
};

// Clipped Mode-X2C1B JMPF through1280 RETF; flags0, native scale, planar source.
// Includes the resident dispatch stub; excludes the caller's CALLF and IRQs.
UiBitmapWork uiBitmapOperations(const IndexedBitmap& bitmap, ProjectedPoint position,
    Viewport viewport, std::uint16_t page);

// Same clipped dispatch with flags2, bounded to the source24x22 title cap.
UiBitmapWork uiMirroredBitmapOperations(const IndexedBitmap& bitmap, ProjectedPoint position,
    Viewport viewport, std::uint16_t page);

//Group5 terminal index31: immutable DS0 width0/height534Dh alias, with the
//source world clip and a valid projected anchor. Includes header and clip work.
UiBitmapWork uiEffectSentinelOperations(ProjectedPoint anchor, Viewport viewport,
    std::uint16_t page);
}
