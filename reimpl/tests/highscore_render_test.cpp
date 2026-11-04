#include "stellar7/highscore_render.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

using namespace stellar7;

namespace {

template<class Action>
void rejects(Action action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

IndexedBitmap bitmap(const std::uint16_t width, const std::uint16_t height,
    const std::uint8_t color) {
    return {width, height, std::vector<std::uint8_t>(width * height, color)};
}

}

int main() {
    constexpr std::uint16_t back = 0xa400;
    constexpr std::uint16_t front = 0xa000;
    constexpr std::uint16_t cache = 0xa800;
    std::vector<IndexedBitmap> art(22, bitmap(4, 4, 0));
    art[0] = bitmap(16, 20, 1);
    art[11] = bitmap(4, 4, 11);
    art[12] = bitmap(40, 40, 7);
    art[18] = bitmap(116, 30, 18);
    art[20] = bitmap(264, 2, 42);
    art[21] = bitmap(4, 4, 21);
    BitmapFont font{32, 6, 0, std::vector<IndexedBitmap>(64, bitmap(8, 6, 0))};
    for (auto& glyph : font.glyphs) {
        glyph.pixels[0] = 1;
    }

    const auto initial = highscoreEntryPageEffects(art, font, back, front);
    const auto& fullCopy = std::get<RadarPageCopy>(initial.back());
    assert(fullCopy.sourcePage == back && fullCopy.destinationPage == front);
    assert(fullCopy.position.x == 0 && fullCopy.position.y == 0);
    assert(fullCopy.coveredWidth == 320 && fullCopy.height == 200);
    for (const auto& effect : initial) {
        if (const auto* write = std::get_if<RadarPixelWrite>(&effect)) {
            assert(write->page == back);
        }
    }
    IndexedPages pages;
    pages.write(cache, {65, 56}, 99);
    pages.apply(initial);
    assert(pages.read(back, {0, 0}) == 42); // 2cf4 backdrop.
    assert(pages.read(back, {40, 40}) == 147); // 52aa outer edge.
    assert(pages.read(back, {65, 56}) == 135); // 54ce left overwrites top corner.
    assert(pages.read(back, {100, 56}) == 128);
    assert(pages.read(back, {254, 70}) == 128);
    assert(pages.read(back, {100, 94}) == 135);
    assert(pages.read(back, {100, 90}) == 7); // Inner tile is clipped.
    assert(pages.read(back, {100, 115}) == 0); // Name box fill follows title.
    assert(pages.read(back, {100, 111}) == 147);
    assert(pages.read(back, {120, 110}) == 147);
    assert(pages.read(back, {94, 116}) == 128);
    assert(pages.read(back, {95, 116}) == 135);
    assert(pages.read(back, {227, 116}) == 147);
    assert(pages.read(back, {120, 121}) == 128);
    assert(pages.read(back, {100, 128}) == 18);
    assert(pages.read(back, {152, 135}) == 128); // OK text after art.
    assert(pages.read(back, {153, 136}) == 156);
    assert(pages.read(front, {152, 135}) == 128);
    assert(pages.read(cache, {65, 56}) == 99);

    const auto pressed = highscoreOkWidgetEffects(art, font, MenuWidgetState::pressed, back, front);
    const auto& widgetCopy = std::get<RadarPageCopy>(pressed.back());
    assert(widgetCopy.position.x == 100 && widgetCopy.position.y == 128);
    assert(widgetCopy.coveredWidth == 116 && widgetCopy.height == 30);
    pages.apply(pressed);
    assert(pages.read(front, {110, 135}) == 11);
    assert(pages.read(front, {204, 135}) == 11);
    assert(pages.read(front, {120, 133}) == 21);
    assert(pages.read(front, {152, 135}) == 128);
    pages.apply(highscoreOkWidgetEffects(art, font, MenuWidgetState::released, back, front));
    assert(pages.read(front, {110, 135}) == 18);
    assert(pages.read(front, {120, 133}) == 18);

    HighscoreNameEntry entry({0, 239});
    auto edit = entry.edit('a');
    assert(edit.change == NameEntryChange::appended);
    auto effects = highscoreNameEditEffects(font, entry, edit, back, front);
    assert(std::get<RadarPageCopy>(effects.back()).position.x == 94);
    assert(std::get<RadarPageCopy>(effects.back()).coveredWidth == 134);
    pages.apply(effects);
    assert(pages.read(front, {98, 113}) == 239);
    assert(pages.read(front, {106, 113}) == 0);
    assert(pages.read(front, {94, 116}) == 128);
    edit = entry.edit(0);
    assert(edit.change == NameEntryChange::caret && edit.caretColor == 239);
    pages.apply(highscoreNameEditEffects(font, entry, edit, back, front));
    assert(pages.read(front, {107, 113}) == 239);
    edit = entry.edit(8);
    assert(edit.change == NameEntryChange::erased);
    pages.apply(highscoreNameEditEffects(font, entry, edit, back, front));
    assert(pages.read(front, {98, 113}) == 0);
    assert(pages.read(front, {107, 113}) == 0);
    assert(pages.read(front, {94, 116}) == 128);
    assert(highscoreNameEditEffects(font, entry, entry.edit(27), back, front).empty());

    const auto pointer = highscorePointerPageEffects(art[0], back);
    pages.apply(pointer);
    assert(pages.read(back, {161, 139}) == 1);
    assert(pages.read(front, {161, 139}) != 1); // Pointer requires caller's transient copy.
    rejects([&] { highscorePointerPageEffects(art[11], back); });

    art[16] = bitmap(4, 4, 0);
    art[16].pixels[0] = 16;
    HighscoreTable::Entries rows{};
    rows[0] = {5155, "GORLENKO"};
    rows[1] = {4815, std::string(1, static_cast<char>('A' | 0x80)) + "LEXQ"};
    rows[3] = {0xffffffffU, "NEG"};
    rows[5] = {2540, "ACE"};
    HighscoreTable table({}, rows);
    IndexedPages hallPages;
    hallPages.write(cache, {67, 76}, 99);
    const auto hall = highscoreHallPageEffects(art, font, table, back, front);
    const auto& hallCopy = std::get<RadarPageCopy>(hall.back());
    assert(hallCopy.sourcePage == back && hallCopy.destinationPage == front);
    assert(hallCopy.position.x == 0 && hallCopy.position.y == 0);
    assert(hallCopy.coveredWidth == 320 && hallCopy.height == 200);
    for (const auto& effect : hall) {
        if (const auto* write = std::get_if<RadarPixelWrite>(&effect)) {
            assert(write->page == back);
        }
    }
    hallPages.apply(hall);
    assert(hallPages.read(back, {0, 0}) == 42);
    assert(hallPages.read(back, {53, 9}) == 147);
    assert(hallPages.read(back, {104, 20}) == 16);
    assert(hallPages.read(back, {105, 20}) == 7); // Transparent art retains the panel tile.
    assert(hallPages.read(back, {67, 100}) == 128);
    assert(hallPages.read(back, {68, 100}) == 135);
    assert(hallPages.read(back, {80, 100}) == 156);
    assert(hallPages.read(back, {116, 52}) == 128); // Centered HIGH SCORES.
    assert(hallPages.read(back, {117, 53}) == 156);
    assert(hallPages.read(back, {71, 81}) == 230);
    assert(hallPages.read(back, {216, 81}) == 239); // 5155 right-aligned at x=248-32.
    assert(hallPages.read(back, {71, 88}) == 230); // Flagged A displays without bit 7.
    assert(hallPages.read(back, {243, 88}) == 230);
    assert(hallPages.read(back, {244, 88}) == 239);
    assert(static_cast<std::uint8_t>(table.entries()[1].name.front()) == ('A' | 0x80));
    assert(hallPages.read(back, {71, 95}) == 156); // Blank zero row is skipped.
    assert(hallPages.read(back, {232, 102}) == 239); // Signed -1, two glyphs.
    assert(hallPages.read(back, {71, 116}) == 230); // Rank 5 row.
    assert(hallPages.read(back, {216, 116}) == 239); // 2540.
    assert(hallPages.read(back, {103, 162}) == 18);
    assert(hallPages.read(back, {131, 169}) == 128); // CONTINUE at x=163-32.
    assert(hallPages.read(back, {132, 170}) == 156);
    assert(hallPages.read(front, {216, 116}) == 239);
    assert(hallPages.read(cache, {67, 76}) == 99);

    const auto hallPressed = highscoreHallWidgetEffects(art, font, MenuWidgetState::pressed,
        back, front);
    const auto& hallWidgetCopy = std::get<RadarPageCopy>(hallPressed.back());
    assert(hallWidgetCopy.position.x == 103 && hallWidgetCopy.position.y == 162);
    assert(hallWidgetCopy.coveredWidth == 116 && hallWidgetCopy.height == 30);
    hallPages.apply(hallPressed);
    assert(hallPages.read(front, {113, 169}) == 11);
    assert(hallPages.read(front, {207, 169}) == 11);
    assert(hallPages.read(front, {123, 167}) == 21);
    assert(hallPages.read(front, {131, 169}) == 128);
    hallPages.apply(highscoreHallWidgetEffects(art, font, MenuWidgetState::released,
        back, front));
    assert(hallPages.read(front, {113, 169}) == 18);
    assert(hallPages.read(front, {123, 167}) == 18);
    hallPages.apply(highscoreHallPointerPageEffects(art[0], back));
    assert(hallPages.read(back, {164, 173}) == 1);
    assert(hallPages.read(front, {164, 173}) != 1);
    assert(hallPages.read(cache, {67, 76}) == 99);
    rejects([&] { highscoreHallPointerPageEffects(art[11], back); });
    auto invalidRows = rows;
    invalidRows[0].name = "lowercase";
    rejects([&] { highscoreHallPageEffects(art, font,
        HighscoreTable({}, invalidRows), back, front); });
    invalidRows = rows;
    invalidRows[0].name = std::string("A") + static_cast<char>('B' | 0x80);
    rejects([&] { highscoreHallPageEffects(art, font,
        HighscoreTable({}, invalidRows), back, front); });
    auto invalidFont = font;
    invalidFont.firstCharacter = 33;
    rejects([&] { highscoreHallPageEffects(art, invalidFont, table, back, front); });
    invalidFont = font;
    invalidFont.glyphs['A' - 32].pixels[0] = 2;
    rejects([&] { highscoreHallPageEffects(art, invalidFont, table, back, front); });
    art[16].pixels.clear();
    rejects([&] { highscoreHallPageEffects(art, font, table, back, front); });
    art[16] = bitmap(4, 4, 0);
    art[20].width = 260;
    rejects([&] { highscoreEntryPageEffects(art, font, back, front); });
    rejects([&] { highscoreHallPageEffects(art, font, table, back, front); });
}
