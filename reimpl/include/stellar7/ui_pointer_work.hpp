#pragma once

#include "stellar7/indexed_pages.hpp"

namespace stellar7 {

struct UiPointerSaveWork {
    std::vector<NormalWorkOperation> operations;
    std::vector<std::uint8_t> transfer;
};

struct UiPointerRestoreWork {
    std::vector<NormalWorkOperation> operations;
    std::vector<IndexedPageEffect> effects;
};

//22A5 JMPF and loaded0ED5 through RETF; outer CALLF/IRQs excluded.
//Reads COPY into row-major packed bytes, preserving the allocation tail.
UiPointerSaveWork uiPointerSaveOperations(const IndexedPages& pages, CockpitCopyRegion rectangle,
    std::uint16_t copyPage, const std::vector<std::uint8_t>& transfer);

//22AD JMPF and loaded0F49 through RETF; outer CALLF/IRQs excluded.
//Writes DRAW from packed bytes; no clipping or aligned planar-copy expansion.
//Each port8Write.value is the original OUT byte, including the index selection.
UiPointerRestoreWork uiPointerRestoreOperations(CockpitCopyRegion rectangle, std::uint16_t drawPage,
    const std::vector<std::uint8_t>& transfer);

}
