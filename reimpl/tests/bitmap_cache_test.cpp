#include "stellar7/bitmap_cache.hpp"
#include <cassert>
#include <stdexcept>

int main() {
    using namespace stellar7;
    const IndexedBitmap first{8, 2, {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15}};
    const IndexedBitmap second{4, 1, {16,17,18,19}};
    const auto cache = cacheBitmapGroup({first, second}, 7);
    assert(cache.initialCursor == 7 && cache.nextCursor == 12);
    assert(cache.frames.size() == 2);
    assert(cache.frames[0].header.segment == 0xac00 && cache.frames[0].header.offset == 7);
    assert(cache.frames[0].header.width == 8 && cache.frames[0].header.height == 2);
    assert(cache.frames[1].header.offset == 11);
    assert(cache.frames[0].bitmap.pixels == first.pixels);
    for (std::size_t plane = 0; plane < 4; ++plane) {
        assert(cache.planes[plane] == std::vector<std::uint8_t>({
            static_cast<std::uint8_t>(plane), static_cast<std::uint8_t>(plane + 4),
            static_cast<std::uint8_t>(plane + 8), static_cast<std::uint8_t>(plane + 12),
            static_cast<std::uint8_t>(plane + 16)}));
    }
    const auto reject = [](const std::vector<IndexedBitmap>& frames, std::uint16_t cursor) {
        try {
            cacheBitmapGroup(frames, cursor);
        } catch (const std::invalid_argument&) {
            return;
        }
        assert(false);
    };
    reject({}, 0);
    reject({{0,1,{}}}, 0);
    reject({{5,1,{0,1,2,3,4}}}, 0);
    reject({{4,1,{0,1,2}}}, 0);
    reject({first}, 0x4000);
    reject({first}, 0x3ffd);
    reject({{256,256,std::vector<std::uint8_t>(65536)}}, 0);
    const auto edge = cacheBitmapGroup({second}, 0x3fff);
    assert(edge.nextCursor == 0x4000);
    assert(edge.frames[0].header.offset == 0x3fff);
}
