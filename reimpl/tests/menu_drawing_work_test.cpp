#include "stellar7/menu_drawing_work.hpp"
#include "stellar7/indexed_render.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace {
template<class Function>
void rejects(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

stellar7::IndexedBitmap bitmap(const std::uint16_t width, const std::uint16_t height) {
    stellar7::IndexedBitmap result{width, height, {}};
    for (std::size_t index = 0; index < static_cast<std::size_t>(width) * height; ++index) {
        result.pixels.push_back(static_cast<std::uint8_t>((index * 37 + index / width * 13) % 256));
    }
    return result;
}
}

int main() {
    using namespace stellar7;
    constexpr std::uint16_t page = 0xa800;
    std::vector<IndexedBitmap> art(23);
    art[12] = bitmap(40, 40);
    art[16] = bitmap(120, 43);
    art[17] = bitmap(120, 33);
    art[18] = bitmap(120, 25);
    art[20] = bitmap(264, 2);
    BitmapFont font{32, 6, 0, {}};
    for (int character = 0; character < 64; ++character) {
        auto glyph = bitmap(8, 6);
        for (std::size_t pixel = 0; pixel < glyph.pixels.size(); ++pixel) {
            glyph.pixels[pixel] = static_cast<std::uint8_t>((pixel + character) % 3 == 0);
        }
        font.glyphs.push_back(std::move(glyph));
    }
    std::vector<std::uint8_t> scratch(500);
    for (std::size_t index = 0; index < scratch.size(); ++index) {
        scratch[index] = static_cast<std::uint8_t>(index * 17 + 9);
    }
    const auto originalScratch = scratch;
    const auto work = menuDrawingOperations(art, font, scratch, page);
    assert(scratch == originalScratch);
    assert(work.scratch.size() == scratch.size());
    assert(std::equal(scratch.begin() + 250, scratch.end(), work.scratch.begin() + 250));
    constexpr std::array<std::array<std::uint16_t, 25>, 5> records{{
        {123, 64, 81, 11, 102, 58, 116, 30, 122, 63, 22, 0, 162, 65, 116, 72, 201, 72, 1, 17, 0x2c4, 4, 1, 0, 0},
        {123, 96, 81, 11, 102, 90, 116, 30, 122, 95, 21, 0, 162, 97, 112, 97, 206, 97, 1, 18, 0x2ca, 0, 2, 1, 1},
        {123, 121, 81, 11, 102, 115, 116, 30, 122, 120, 21, 0, 162, 122, 112, 122, 206, 122, 1, 18, 0x2d3, 1, 3, 2, 2},
        {123, 146, 81, 11, 102, 140, 116, 30, 122, 145, 21, 0, 162, 147, 112, 147, 206, 147, 1, 18, 0x23c, 2, 4, 3, 3},
        {123, 171, 81, 11, 102, 165, 116, 30, 122, 170, 21, 0, 162, 172, 112, 172, 206, 172, 1, 18, 0x2d9, 3, 0, 4, 4}
    }};
    for (std::size_t record = 0; record < records.size(); ++record) {
        for (std::size_t word = 0; word < records[record].size(); ++word) {
            const auto offset = record * 50 + word * 2;
            assert(work.scratch[offset] == static_cast<std::uint8_t>(records[record][word]));
            assert(work.scratch[offset + 1] == static_cast<std::uint8_t>(records[record][word] >> 8));
        }
    }
    const auto reference = mainMenuPageEffects(art, font, page);
    IndexedPages actual;
    IndexedPages expected;
    actual.apply(work.effects);
    expected.apply(reference);
    for (std::int16_t y = 0; y < 200; ++y) {
        for (std::int16_t x = 0; x < 320; ++x) {
            assert(actual.known(page, {x, y}) == expected.known(page, {x, y}));
            if (expected.known(page, {x, y})) {
                assert(actual.read(page, {x, y}) == expected.read(page, {x, y}));
            }
            assert(!actual.known(0xa000, {x, y}) && !actual.known(0xa400, {x, y}));
        }
    }
    assert(work.widgetCount == 5 && work.selectedFont == 0 && work.textColor == 156);
    assert(work.finalSurface.viewport.left == 0 && work.finalSurface.viewport.right == 319);
    assert(work.finalSurface.viewport.top == 0 && work.finalSurface.viewport.bottom == 199);
    assert(work.finalSurface.clipping == PolygonClipMode::Enabled);
    assert(work.finalSurface.rasterStyle == 1 && work.finalSurface.lineColor == 147);
    assert(std::count_if(work.operations.begin(), work.operations.end(), [](const auto& operation) {
        return operation.kind == NormalWorkKind::repeatString && operation.count == 25;
    }) == 5);
    rejects([&]() { menuDrawingOperations(art, font, std::vector<std::uint8_t>(499), page); });
    rejects([&]() { menuDrawingOperations(art, font, scratch, 0xa100); });
    art[17].height = 30;
    rejects([&]() { menuDrawingOperations(art, font, scratch, page); });
}
