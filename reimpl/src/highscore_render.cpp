#include "stellar7/highscore_render.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <string_view>

namespace stellar7 {
namespace {

constexpr std::int16_t screenWidth = 320;
constexpr std::int16_t screenHeight = 200;
constexpr std::uint16_t planeCount = 4;
constexpr CockpitCopyRegion outerPanel{40, 40, 240, 120};
constexpr CockpitCopyRegion innerPanel{65, 56, 190, 39};
constexpr CockpitCopyRegion nameBox{94, 110, 134, 12};
constexpr CockpitCopyRegion okCopy{100, 128, 116, 30};
constexpr CockpitCopyRegion hallPanel{53, 9, 215, 188};
constexpr CockpitCopyRegion hallRows{67, 76, 186, 78};
constexpr CockpitCopyRegion hallWidgetCopy{103, 162, 116, 30};
constexpr ProjectedPoint nameOrigin{98, 113};
constexpr std::uint8_t titleForeground = 128;
constexpr std::uint8_t titleShadow = 156;
constexpr std::uint8_t editorColor = 239;
constexpr std::uint8_t hallNameColor = 230;
constexpr std::uint8_t completionMark = 0x80;

void appendBitmap(std::vector<IndexedPageEffect>& effects, const IndexedBitmap& bitmap,
    const ProjectedPoint position, const CockpitCopyRegion clip, const std::uint16_t page) {
    if (bitmap.width == 0 || bitmap.height == 0 || bitmap.width % planeCount != 0
        || bitmap.pixels.size() != static_cast<std::size_t>(bitmap.width) * bitmap.height) {
        throw std::invalid_argument("invalid highscore UI bitmap");
    }
    for (std::uint16_t plane = 0; plane < planeCount; ++plane) {
        for (std::uint16_t row = 0; row < bitmap.height; ++row) {
            for (std::uint16_t column = plane; column < bitmap.width; column += planeCount) {
                const auto x = static_cast<int>(position.x) + column;
                const auto y = static_cast<int>(position.y) + row;
                const auto color = bitmap.pixels[static_cast<std::size_t>(row) * bitmap.width + column];
                if (x < clip.x || x >= clip.x + clip.width || y < clip.y
                    || y >= clip.y + clip.height || color == 0) {
                    continue;
                }
                effects.emplace_back(RadarPixelWrite{{static_cast<std::int16_t>(x),
                    static_cast<std::int16_t>(y)}, color, page});
            }
        }
    }
}

void appendLine(std::vector<IndexedPageEffect>& effects, const int x0, const int y0,
    const int x1, const int y1, const std::uint8_t color, const std::uint16_t page) {
    if (x0 != x1 && y0 != y1) {
        throw std::invalid_argument("highscore UI line must be axis aligned");
    }
    for (int y = std::min(y0, y1); y <= std::max(y0, y1); ++y) {
        for (int x = std::min(x0, x1); x <= std::max(x0, x1); ++x) {
            if (x >= 0 && x < screenWidth && y >= 0 && y < screenHeight) {
                effects.emplace_back(RadarPixelWrite{{static_cast<std::int16_t>(x),
                    static_cast<std::int16_t>(y)}, color, page});
            }
        }
    }
}

void appendFill(std::vector<IndexedPageEffect>& effects, const CockpitCopyRegion rectangle,
    const std::uint8_t color, const std::uint16_t page) {
    for (int y = rectangle.y; y < rectangle.y + rectangle.height; ++y) {
        for (int x = rectangle.x; x < rectangle.x + rectangle.width; ++x) {
            effects.emplace_back(RadarPixelWrite{{static_cast<std::int16_t>(x),
                static_cast<std::int16_t>(y)}, color, page});
        }
    }
}

void validateFont(const BitmapFont& font) {
    if (font.firstCharacter != 32 || font.height != 6 || font.glyphs.size() != 64) {
        throw std::invalid_argument("highscore UI requires source font 12ca");
    }
}

void appendText(std::vector<IndexedPageEffect>& effects, const BitmapFont& font,
    const std::string_view label, const ProjectedPoint position, const std::uint8_t color,
    const std::uint16_t page) {
    validateFont(font);
    int x = position.x;
    for (const unsigned char character : label) {
        if (character < font.firstCharacter
            || character - font.firstCharacter >= font.glyphs.size()) {
            throw std::invalid_argument("highscore UI character outside source font");
        }
        const auto& glyph = font.glyphs[character - font.firstCharacter];
        if (glyph.width != 8 || glyph.height != font.height
            || glyph.pixels.size() != static_cast<std::size_t>(glyph.width) * glyph.height) {
            throw std::invalid_argument("malformed highscore UI glyph");
        }
        for (std::uint16_t row = 0; row < glyph.height; ++row) {
            for (std::uint16_t column = 0; column < glyph.width; ++column) {
                const auto ink = glyph.pixels[static_cast<std::size_t>(row) * glyph.width + column];
                if (ink > 1) {
                    throw std::invalid_argument("highscore UI glyph is not a binary mask");
                }
                if (ink != 0) {
                    effects.emplace_back(RadarPixelWrite{{static_cast<std::int16_t>(x + column),
                        static_cast<std::int16_t>(position.y + row)}, color, page});
                }
            }
        }
        x += glyph.width;
    }
}

void appendCenteredTitle(std::vector<IndexedPageEffect>& effects, const BitmapFont& font,
    const std::string_view label, const std::int16_t y, const std::uint16_t page) {
    const auto x = static_cast<std::int16_t>(160 - static_cast<int>(label.size()) * 4);
    appendText(effects, font, label, {x, y}, titleForeground, page);
    appendText(effects, font, label, {static_cast<std::int16_t>(x + 1),
        static_cast<std::int16_t>(y + 1)}, titleShadow, page);
}

void appendBackdrop(std::vector<IndexedPageEffect>& effects, const IndexedBitmap& tile,
    const std::uint16_t page) {
    const CockpitCopyRegion display{0, 0, screenWidth, screenHeight};
    int x = 0;
    for (std::int16_t y = 0; y < screenHeight; y += 2, x += 3) {
        if (y > 174) {
            appendBitmap(effects, tile, {static_cast<std::int16_t>(x - 524), y}, display, page);
        }
        appendBitmap(effects, tile, {static_cast<std::int16_t>(x - 262), y}, display, page);
        if (x < screenWidth) {
            appendBitmap(effects, tile, {static_cast<std::int16_t>(x), y}, display, page);
        }
        if (x < 58) {
            appendBitmap(effects, tile, {static_cast<std::int16_t>(x + 262), y}, display, page);
        }
    }
}

void appendInnerPanel(std::vector<IndexedPageEffect>& effects, const IndexedBitmap& tile,
    const std::uint16_t page) {
    if (tile.width != 40 || tile.height != 40) {
        throw std::invalid_argument("highscore panel requires source UI tile");
    }
    for (int y = innerPanel.y; y < innerPanel.y + innerPanel.height - 1; y += tile.height) {
        for (int x = innerPanel.x; x < innerPanel.x + innerPanel.width - 1; x += tile.width) {
            appendBitmap(effects, tile, {static_cast<std::int16_t>(x),
                static_cast<std::int16_t>(y)}, innerPanel, page);
        }
    }
    const auto right = innerPanel.x + innerPanel.width - 1;
    const auto bottom = innerPanel.y + innerPanel.height - 1;
    appendLine(effects, innerPanel.x, innerPanel.y, right, innerPanel.y, 128, page);
    appendLine(effects, right, innerPanel.y, right, bottom, 128, page);
    appendLine(effects, innerPanel.x, bottom, right, bottom, 135, page);
    appendLine(effects, innerPanel.x, innerPanel.y, innerPanel.x, bottom, 135, page);
}

void appendBevel(std::vector<IndexedPageEffect>& effects, const CockpitCopyRegion rectangle,
    const std::uint16_t page) {
    const auto x = rectangle.x;
    const auto y = rectangle.y;
    const auto right = x + rectangle.width - 1;
    const auto bottom = y + rectangle.height - 1;
    appendLine(effects, x, y, right, y, 147, page);
    appendLine(effects, right, y, right, bottom, 147, page);
    appendLine(effects, x, y, x, bottom, 128, page);
    appendLine(effects, x, bottom, right, bottom, 128, page);
    appendLine(effects, x + 1, y + 1, right - 1, y + 1, 147, page);
    appendLine(effects, right - 1, y + 1, right - 1, bottom - 1, 147, page);
    appendLine(effects, x + 1, y + 1, x + 1, bottom - 1, 135, page);
    appendLine(effects, x + 1, bottom - 1, right - 1, bottom - 1, 135, page);
}

void appendTemplate18Widget(std::vector<IndexedPageEffect>& effects,
    const std::vector<IndexedBitmap>& art, const BitmapFont& font, const std::string_view label,
    const ProjectedPoint origin, const MenuWidgetState state, const std::uint16_t page) {
    const CockpitCopyRegion display{0, 0, screenWidth, screenHeight};
    appendBitmap(effects, art[18], origin, display, page);
    if (state == MenuWidgetState::pressed) {
        appendBitmap(effects, art[11], {static_cast<std::int16_t>(origin.x + 10),
            static_cast<std::int16_t>(origin.y + 7)}, display, page);
        appendBitmap(effects, art[11], {static_cast<std::int16_t>(origin.x + 104),
            static_cast<std::int16_t>(origin.y + 7)}, display, page);
        appendBitmap(effects, art[21], {static_cast<std::int16_t>(origin.x + 20),
            static_cast<std::int16_t>(origin.y + 5)}, display, page);
    }
    const auto center = static_cast<std::int16_t>(origin.x + 60);
    const auto x = static_cast<std::int16_t>(center - static_cast<int>(label.size()) * 4);
    const auto y = static_cast<std::int16_t>(origin.y + 7);
    appendText(effects, font, label, {x, y}, titleForeground, page);
    appendText(effects, font, label, {static_cast<std::int16_t>(x + 1),
        static_cast<std::int16_t>(y + 1)}, titleShadow, page);
}

void validateArt(const std::vector<IndexedBitmap>& art) {
    if (art.size() <= 21 || art[20].width != 264 || art[20].height != 2
        || art[12].width != 40 || art[12].height != 40) {
        throw std::invalid_argument("highscore UI requires source STELART art group");
    }
}

void validateHallTable(const HighscoreTable& table) {
    for (const auto& entry : table.entries()) {
        for (std::size_t index = 0; index < entry.name.size(); ++index) {
            const auto stored = static_cast<std::uint8_t>(entry.name[index]);
            const auto displayed = static_cast<std::uint8_t>(
                index == 0 ? stored & static_cast<std::uint8_t>(~completionMark) : stored);
            if ((index != 0 && (stored & completionMark) != 0)
                || displayed < 32 || displayed > 95) {
                throw std::invalid_argument("highscore hall name outside source font");
            }
        }
    }
}

void validateHallAssets(const std::vector<IndexedBitmap>& art, const BitmapFont& font) {
    validateArt(art);
    validateFont(font);
    for (const auto index : {11U, 16U, 18U, 20U, 21U}) {
        const auto& bitmap = art[index];
        if (bitmap.width == 0 || bitmap.height == 0 || bitmap.width % planeCount != 0
            || bitmap.pixels.size() != static_cast<std::size_t>(bitmap.width) * bitmap.height) {
            throw std::invalid_argument("invalid highscore hall bitmap");
        }
    }
    for (const auto& glyph : font.glyphs) {
        if (glyph.width != 8 || glyph.height != font.height
            || glyph.pixels.size() != static_cast<std::size_t>(glyph.width) * glyph.height
            || std::any_of(glyph.pixels.begin(), glyph.pixels.end(),
                [](const std::uint8_t ink) { return ink > 1; })) {
            throw std::invalid_argument("invalid highscore hall font glyph");
        }
    }
}

std::string signedScore(const std::uint32_t score) {
    if ((score & 0x80000000U) != 0) {
        return "-" + std::to_string(0U - score);
    }
    return std::to_string(score);
}

void appendHallRows(std::vector<IndexedPageEffect>& effects, const BitmapFont& font,
    const HighscoreTable& table, const std::uint16_t page) {
    for (std::size_t index = 0; index < table.entries().size(); ++index) {
        const auto& entry = table.entries()[index];
        if (entry.name.empty() && entry.score == 0) {
            continue;
        }
        const auto y = static_cast<std::int16_t>(81 + index * 7);
        auto displayedName = entry.name;
        const auto flagged = !displayedName.empty()
            && (static_cast<std::uint8_t>(displayedName.front()) & completionMark) != 0;
        if (flagged) {
            displayedName.front() = static_cast<char>(
                static_cast<std::uint8_t>(displayedName.front()) & ~completionMark);
        }
        appendText(effects, font, displayedName, {71, y}, hallNameColor, page);
        if (flagged) {
            appendLine(effects, 243, y, 243, y + 4, hallNameColor, page);
            appendFill(effects, {244, y, 3, 3}, editorColor, page);
        }
        const auto score = signedScore(entry.score);
        const auto x = static_cast<std::int16_t>(248 - score.size() * 8);
        appendText(effects, font, score, {x, y}, editorColor, page);
    }
}

}

std::vector<IndexedPageEffect> highscoreEntryPageEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, const std::uint16_t backPage, const std::uint16_t frontPage) {
    validateArt(art);
    validateFont(titleFont);
    std::vector<IndexedPageEffect> effects;
    appendBackdrop(effects, art[20], backPage);
    const auto panel = dialogPanelPageEffects(art[12], outerPanel, backPage);
    effects.insert(effects.end(), panel.begin(), panel.end());
    appendInnerPanel(effects, art[12], backPage);
    constexpr std::array<std::string_view, 4> title{
        "CONGRATULATIONS!!", "YOU HAVE QUALIFIED", "TO THE HALL OF FAME",
        "PLEASE ENTER YOUR NAME"};
    for (std::size_t index = 0; index < title.size(); ++index) {
        appendCenteredTitle(effects, titleFont, title[index],
            static_cast<std::int16_t>(60 + index * 8), backPage);
    }
    appendFill(effects, nameBox, 0, backPage);
    appendBevel(effects, nameBox, backPage);
    appendTemplate18Widget(effects, art, titleFont, "OK", {100, 128},
        MenuWidgetState::released, backPage);
    effects.emplace_back(RadarPageCopy{backPage, frontPage, {0, 0}, screenWidth, screenHeight});
    return effects;
}

std::vector<IndexedPageEffect> highscoreOkWidgetEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, const MenuWidgetState state, const std::uint16_t backPage,
    const std::uint16_t frontPage) {
    validateArt(art);
    validateFont(titleFont);
    std::vector<IndexedPageEffect> effects;
    appendTemplate18Widget(effects, art, titleFont, "OK", {100, 128}, state, backPage);
    effects.emplace_back(RadarPageCopy{backPage, frontPage, {okCopy.x, okCopy.y},
        okCopy.width, okCopy.height});
    return effects;
}

std::vector<IndexedPageEffect> highscoreNameEditEffects(const BitmapFont& titleFont,
    const HighscoreNameEntry& entry, const NameEntryEdit edit, const std::uint16_t backPage,
    const std::uint16_t frontPage) {
    validateFont(titleFont);
    std::vector<IndexedPageEffect> effects;
    if (edit.change == NameEntryChange::none) {
        return effects;
    }
    const auto name = entry.name();
    if (name.size() > highscoreEnteredNameLimit
        || (edit.change == NameEntryChange::appended && name.size() != edit.previousLength + 1)
        || (edit.change == NameEntryChange::erased && name.size() + 1 != edit.previousLength)
        || (edit.change == NameEntryChange::caret && name.size() != edit.previousLength)) {
        throw std::invalid_argument("highscore edit result does not match current name");
    }
    if (edit.change == NameEntryChange::caret) {
        appendFill(effects, {static_cast<std::int16_t>(nameOrigin.x + name.size() * 8 + 1),
            nameOrigin.y, 7, 6}, edit.caretColor, backPage);
    } else {
        const auto clearX = nameOrigin.x + (edit.change == NameEntryChange::appended ? 1 : 0);
        appendFill(effects, {static_cast<std::int16_t>(clearX), nameOrigin.y,
            static_cast<std::int16_t>(edit.previousLength * 8 + 8), 6}, 0, backPage);
        appendText(effects, titleFont, name, nameOrigin, editorColor, backPage);
    }
    effects.emplace_back(RadarPageCopy{backPage, frontPage, {nameBox.x, nameBox.y},
        nameBox.width, nameBox.height});
    return effects;
}

std::vector<IndexedPageEffect> highscorePointerPageEffects(const IndexedBitmap& pointer,
    const std::uint16_t backPage) {
    if (pointer.width != 16 || pointer.height != 20) {
        throw std::invalid_argument("highscore pointer requires source UI art");
    }
    std::vector<IndexedPageEffect> effects;
    appendBitmap(effects, pointer, {161, 139}, {0, 0, screenWidth, screenHeight}, backPage);
    return effects;
}

std::vector<IndexedPageEffect> highscoreHallPageEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, const HighscoreTable& table, const std::uint16_t backPage,
    const std::uint16_t frontPage) {
    validateHallAssets(art, titleFont);
    validateHallTable(table);
    std::vector<IndexedPageEffect> effects;
    appendBackdrop(effects, art[20], backPage);
    const auto panel = dialogPanelPageEffects(art[12], hallPanel, backPage);
    effects.insert(effects.end(), panel.begin(), panel.end());
    appendTemplate18Widget(effects, art, titleFont, "CONTINUE", {103, 162},
        MenuWidgetState::released, backPage);
    appendFill(effects, hallRows, titleShadow, backPage);
    appendBevel(effects, hallRows, backPage);
    appendBitmap(effects, art[16], {104, 20}, {0, 0, screenWidth, screenHeight}, backPage);
    appendCenteredTitle(effects, titleFont, "HIGH SCORES", 52, backPage);
    appendHallRows(effects, titleFont, table, backPage);
    effects.emplace_back(RadarPageCopy{backPage, frontPage, {0, 0}, screenWidth, screenHeight});
    return effects;
}

std::vector<IndexedPageEffect> highscoreHallWidgetEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, const MenuWidgetState state, const std::uint16_t backPage,
    const std::uint16_t frontPage) {
    validateHallAssets(art, titleFont);
    std::vector<IndexedPageEffect> effects;
    appendTemplate18Widget(effects, art, titleFont, "CONTINUE", {103, 162}, state, backPage);
    effects.emplace_back(RadarPageCopy{backPage, frontPage,
        {hallWidgetCopy.x, hallWidgetCopy.y}, hallWidgetCopy.width, hallWidgetCopy.height});
    return effects;
}

std::vector<IndexedPageEffect> highscoreHallPointerPageEffects(const IndexedBitmap& pointer,
    const std::uint16_t backPage) {
    if (pointer.width != 16 || pointer.height != 20) {
        throw std::invalid_argument("highscore pointer requires source UI art");
    }
    std::vector<IndexedPageEffect> effects;
    appendBitmap(effects, pointer, {164, 173}, {0, 0, screenWidth, screenHeight}, backPage);
    return effects;
}

}
