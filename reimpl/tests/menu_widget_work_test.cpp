#include "stellar7/menu_widget_work.hpp"
#include "stellar7/menu_drawing_work.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

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

void store(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    constexpr std::uint16_t back = 0xa800;
    constexpr std::uint16_t front = 0xa000;
    constexpr std::array<std::int16_t, 5> rows{58, 90, 115, 140, 165};
    std::vector<IndexedBitmap> art(23);
    art[11] = {8, 7, std::vector<std::uint8_t>(56)};
    art[17] = {120, 33, std::vector<std::uint8_t>(3960)};
    art[18] = {120, 25, std::vector<std::uint8_t>(3000)};
    art[21] = {88, 11, std::vector<std::uint8_t>(968)};
    art[22] = {88, 11, std::vector<std::uint8_t>(968)};
    art[21].pixels[0] = 213;
    art[22].pixels[0] = 213;
    BitmapFont font{32, 6, 0, std::vector<IndexedBitmap>(64, {8, 6, std::vector<std::uint8_t>(48)})};
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const auto effects = mainMenuWidgetEffects(art, font, index, MenuWidgetState::pressed, back);
        //Both source templates use overlay origin(20,5), translated by(102,y).
        assert(effects.size() == 1);
        const auto& overlay = std::get<RadarPixelWrite>(effects.front());
        assert(overlay.point.x == 122 && overlay.point.y == rows[index] + 5 && overlay.color == 213);
    }

    art[11] = bitmap(8, 7);
    art[12] = bitmap(40, 40);
    art[16] = bitmap(120, 43);
    art[17] = bitmap(120, 33);
    art[18] = bitmap(120, 25);
    art[20] = bitmap(264, 2);
    art[21] = bitmap(88, 11);
    art[22] = bitmap(88, 11);
    for (std::size_t character = 0; character < font.glyphs.size(); ++character) {
        auto& glyph = font.glyphs[character];
        for (std::size_t pixel = 0; pixel < glyph.pixels.size(); ++pixel) {
            glyph.pixels[pixel] = static_cast<std::uint8_t>((pixel + character) % 3 == 0);
        }
    }
    const auto scratch = menuDrawingOperations(art, font, std::vector<std::uint8_t>(500, 91), back).scratch;
    const auto before = scratch;
    for (std::size_t index = 0; index < rows.size(); ++index) {
        for (const auto state : {MenuWidgetState::released, MenuWidgetState::pressed}) {
            const auto work = menuWidgetOperations(art, font, scratch, index, state, back);
            const auto reference = mainMenuWidgetEffects(art, font, index, state, back);
            assert(work.textColor == 156 && work.effects.size() == reference.size());
            for (std::size_t pixel = 0; pixel < reference.size(); ++pixel) {
                const auto& actual = std::get<RadarPixelWrite>(work.effects[pixel]);
                const auto& expected = std::get<RadarPixelWrite>(reference[pixel]);
                assert(actual.point.x == expected.point.x && actual.point.y == expected.point.y);
                assert(actual.color == expected.color && actual.page == expected.page);
            }
        }
        const auto copy = menuWidgetCopyOperations(scratch, index, back, front);
        assert(copy.effects.size() == 1);
        const auto& region = std::get<RadarPageCopy>(copy.effects.front());
        assert(region.sourcePage == back && region.destinationPage == front);
        assert(region.position.x == 100 && region.position.y == rows[index]);
        assert(region.coveredWidth == 120 && region.height == 30);
        IndexedPages pages;
        for (std::int16_t y = rows[index]; y < rows[index] + 30; ++y) {
            for (std::int16_t x = 99; x <= 220; ++x) {
                pages.write(back, {x, y}, static_cast<std::uint8_t>(x + y));
                pages.write(front, {x, y}, 73);
            }
        }
        pages.apply(copy.effects);
        for (std::int16_t y = rows[index]; y < rows[index] + 30; ++y) {
            assert(pages.read(front, {99, y}) == 73 && pages.read(front, {220, y}) == 73);
            for (std::int16_t x = 100; x < 220; ++x) {
                assert(pages.read(front, {x, y}) == static_cast<std::uint8_t>(x + y));
            }
        }
    }
    assert(scratch == before);

    auto changed = scratch;
    art[17].pixels[0] = 213;
    store(changed, 0x08, 103);
    store(changed, 0x1c, 0);
    store(changed, 0x20, 0);
    store(changed, 0x14, 0);
    const auto pressed = menuWidgetOperations(art, font, changed, 0, MenuWidgetState::pressed, back);
    const auto released = menuWidgetOperations(art, font, changed, 0, MenuWidgetState::released, back);
    assert(pressed.effects.size() == released.effects.size());
    assert(std::get<RadarPixelWrite>(pressed.effects.front()).point.x == 103);
    const auto shifted = menuWidgetCopyOperations(changed, 0, back, front);
    assert(std::get<RadarPageCopy>(shifted.effects.front()).coveredWidth == 124);
    store(changed, 0x24, 0);
    const auto disabled = menuWidgetCopyOperations(changed, 0, back, front);
    assert(disabled.effects.empty());
    rejects([&]() { menuWidgetOperations(art, font, changed, 0, MenuWidgetState::released, back); });
    rejects([&]() { menuWidgetOperations(art, font, scratch, 10, MenuWidgetState::released, back); });
    rejects([&]() { menuWidgetOperations(art, font, std::vector<std::uint8_t>(499), 0, MenuWidgetState::released, back); });
    rejects([&]() { menuWidgetCopyOperations(scratch, 0, back, back); });
    store(changed, 0x24, 1);
    store(changed, 0x28, 0);
    rejects([&]() { menuWidgetOperations(art, font, changed, 0, MenuWidgetState::released, back); });
    changed = scratch;
    store(changed, 0x16, 2);
    rejects([&]() { menuWidgetOperations(art, font, changed, 0, MenuWidgetState::pressed, back); });
}
