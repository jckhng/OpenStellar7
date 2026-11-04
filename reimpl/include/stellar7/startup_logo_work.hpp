#pragma once

#include "stellar7/bitmap_cache.hpp"
#include "stellar7/render_work.hpp"

namespace stellar7 {

enum class StartupLogoKind { instructions, countdownStore, copyToCache, bitmap,
    copyToFront, wait, frameReturn };

struct StartupLogoSegment {
    StartupLogoKind kind;
    std::uint32_t sourcePc;
    std::vector<NormalWorkOperation> operations;
    std::uint8_t ordinal = 0;
    std::int16_t frameIndex = 0;
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint16_t value = 0;
};

struct StartupLogoWork {
    std::vector<StartupLogoSegment> segments;
    // Raster views of actual cached planes; no synthesized padding pixels.
    std::array<IndexedBitmap, 8> bitmaps;
};

//620B..6241: eight ascending and nine descending calls, ending with index-1.
// Root executes countdownStore/frameReturn as one typed dispatch each and owns
// dynamic663B polling plus its three exit instructions. No CRTC flip occurs.
StartupLogoWork startupLogoOperations(const BitmapCache& cache);

}  // namespace stellar7
