#pragma once

#include "stellar7/resource_codec.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace stellar7 {

struct BitmapCacheHeader {
    std::uint16_t segment;
    std::uint16_t offset;
    std::uint16_t width;
    std::uint16_t height;
};

struct BitmapCacheFrame {
    BitmapCacheHeader header;
    IndexedBitmap bitmap;
};

struct BitmapCache {
    std::uint16_t initialCursor;
    std::uint16_t nextCursor;
    // Each plane contains only this group's writes, beginning at initialCursor.
    std::array<std::vector<std::uint8_t>, 4> planes;
    std::vector<BitmapCacheFrame> frames;
};

// Resident1e71 copies source planes into ac00 and rewrites frame headers.
// Source-decoded nonempty group; excludes DOS source-block release and clock work.
BitmapCache cacheBitmapGroup(const std::vector<IndexedBitmap>& frames, std::uint16_t initialCursor);

}
