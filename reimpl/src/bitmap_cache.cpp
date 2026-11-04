#include "stellar7/bitmap_cache.hpp"

#include <limits>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::uint16_t cacheSegment = 0xac00;
constexpr std::size_t cachePlaneBytes = 0x4000;
constexpr std::size_t planeCount = 4;
}

BitmapCache cacheBitmapGroup(const std::vector<IndexedBitmap>& frames, const std::uint16_t initialCursor) {
    if (frames.empty() || initialCursor >= cachePlaneBytes) {
        throw std::invalid_argument("unsupported bitmap cache group or cursor");
    }
    std::size_t next = initialCursor;
    for (const auto& frame : frames) {
        const auto pixels = static_cast<std::size_t>(frame.width) * frame.height;
        if (frame.width == 0 || frame.width % planeCount != 0 || frame.height == 0
            || pixels != frame.pixels.size() || pixels > std::numeric_limits<std::uint16_t>::max()
            || pixels / planeCount > cachePlaneBytes - next) {
            throw std::invalid_argument("bitmap cache source or capacity is unsupported");
        }
        next += pixels / planeCount;
    }
    BitmapCache cache{initialCursor, static_cast<std::uint16_t>(next), {}, {}};
    cache.frames.reserve(frames.size());
    for (auto& plane : cache.planes) {
        plane.reserve(next - initialCursor);
    }
    auto offset = initialCursor;
    for (const auto& frame : frames) {
        cache.frames.push_back({{cacheSegment, offset, frame.width, frame.height}, frame});
        for (std::size_t plane = 0; plane < planeCount; ++plane) {
            for (std::size_t pixel = plane; pixel < frame.pixels.size(); pixel += planeCount) {
                cache.planes[plane].push_back(frame.pixels[pixel]);
            }
        }
        offset = static_cast<std::uint16_t>(offset + frame.pixels.size() / planeCount);
    }
    return cache;
}

}
