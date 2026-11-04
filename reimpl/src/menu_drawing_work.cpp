#include "stellar7/menu_drawing_work.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr Viewport display{0, 319, 0, 199};
constexpr std::size_t scratchBytes = 500;
constexpr std::size_t recordWords = 25;
constexpr std::size_t recordBytes = recordWords * 2;
constexpr std::size_t menuItems = 5;
constexpr std::array<std::int16_t, menuItems> rows{58, 90, 115, 140, 165};
constexpr std::array<std::string_view, menuItems> labels{"BEGIN", "BRIEFING", "PREFS", "SCORES", "EXIT"};
constexpr std::array<std::uint16_t, menuItems> labelPointers{0x02c4, 0x02ca, 0x02d3, 0x023c, 0x02d9};
constexpr std::array<std::uint16_t, recordWords> firstTemplate{
    21, 6, 81, 11, 0, 0, 116, 30, 20, 5, 22, 0, 60, 7, 14, 14, 99, 14, 1, 0, 0, 0, 0, 0, 0};
constexpr std::array<std::uint16_t, recordWords> ordinaryTemplate{
    21, 6, 81, 11, 0, 0, 116, 30, 20, 5, 21, 0, 60, 7, 10, 7, 104, 7, 1, 0, 0, 0, 0, 0, 0};
constexpr UiPanelColors panelColors{{147, 141, 135, 128, 135, 141, 147}, 147, 135, 128};
constexpr std::uint8_t darkText = 128;
constexpr std::uint8_t lightText = 156;
constexpr std::int16_t widgetX = 102;
constexpr std::size_t firstWidgetBitmap = 17;
constexpr std::size_t ordinaryWidgetBitmap = 18;

void storeWord(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void constructRecord(std::vector<std::uint8_t>& scratch, const std::size_t index) {
    auto words = index == 0 ? firstTemplate : ordinaryTemplate;
    //5720 translates all four rectangles and both nonzero side origins.
    for (const auto offset : {0, 4, 8, 12, 14, 16}) {
        words[offset] += widgetX;
        words[offset + 1] += rows[index];
    }
    words[19] = static_cast<std::uint16_t>(index == 0 ? firstWidgetBitmap : ordinaryWidgetBitmap);
    words[20] = labelPointers[index];
    for (std::size_t word = 0; word < recordWords; ++word) {
        storeWord(scratch, index * recordBytes + word * 2, words[word]);
    }
}

void linkRecord(std::vector<std::uint8_t>& scratch, const std::size_t index) {
    const auto base = index * recordBytes;
    storeWord(scratch, base + 0x2a, static_cast<std::uint16_t>((index + menuItems - 1) % menuItems));
    storeWord(scratch, base + 0x2c, static_cast<std::uint16_t>((index + 1) % menuItems));
    storeWord(scratch, base + 0x2e, static_cast<std::uint16_t>(index));
    storeWord(scratch, base + 0x30, static_cast<std::uint16_t>(index));
}
}

MenuDrawingWork menuDrawingOperations(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, const std::vector<std::uint8_t>& scratch,
    const std::uint16_t backPage) {
    if (scratch.size() != scratchBytes || art.size() <= 20
        || titleFont.firstCharacter != 32 || titleFont.height != 6 || titleFont.glyphs.size() != 64
        || std::any_of(titleFont.glyphs.begin(), titleFont.glyphs.end(), [](const auto& glyph) { return glyph.width != 8; })
        || art[12].width != 40 || art[12].height != 40 || art[16].width != 120 || art[16].height != 43
        || art[17].width != 120 || art[17].height != 33 || art[18].width != 120 || art[18].height != 25
        || art[20].width != 264 || art[20].height != 2) {
        throw std::invalid_argument("menu drawing requires the retained scratch allocation and source UI resources");
    }
    MenuDrawingWork result{{}, {}, scratch, {display, PolygonClipMode::Enabled, 1, panelColors.edges.back()},
        0, lightText, menuItems};
    const auto plain = [&](const std::uint64_t count) {
        result.operations.push_back({NormalWorkKind::instructions, count});
    };
    const auto append = [&](const auto& work) {
        result.operations.insert(result.operations.end(), work.operations.begin(), work.operations.end());
        result.effects.insert(result.effects.end(), work.effects.begin(), work.effects.end());
    };
    const auto bitmap = [&](const std::size_t index, const int x, const int y) {
        append(uiBitmapOperations(art[index], {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)},
            display, backPage));
    };
    const auto centeredText = [&](const std::string_view text, const int x, const int y) {
        plain(8); //2FA6..2FB4, including49E4 CALLF.
        const auto width = uiTextWidthOperations(titleFont, UiFontLayout::fixed, text);
        result.operations.insert(result.operations.end(), width.operations.begin(), width.operations.end());
        const auto left = static_cast<std::int16_t>(x - width.width / 2);
        plain(11); //2FB9..2FD0, including first4C0E CALLF.
        append(uiTextOperations(titleFont, UiFontLayout::fixed, text, {left, static_cast<std::int16_t>(y)},
            darkText, backPage, display));
        plain(10); //2FD5..2FE8, including second4C0E CALLF.
        append(uiTextOperations(titleFont, UiFontLayout::fixed, text,
            {static_cast<std::int16_t>(left + 1), static_cast<std::int16_t>(y + 1)}, lightText, backPage, display));
        plain(6); //2FED..2FF5.
    };

    plain(8); //16B0..16CA;4848 body is the native font-selection boundary.
    plain(12); //2CF4..2D00, including5BFE.
    for (int y = 0, x = 0; y < 200; y += 2, x += 3) {
        plain(2);
        if (y > 174) {
            plain(8); bitmap(20, x - 524, y); plain(1);
        }
        plain(8); bitmap(20, x - 262, y); plain(1);
        plain(2);
        if (x < 320) {
            plain(7); bitmap(20, x, y); plain(1);
        }
        plain(2);
        if (x < 58) {
            plain(8); bitmap(20, x + 262, y); plain(1);
        }
        plain(5);
    }
    plain(3); //2D7A..2D7C.
    plain(9); //16CD..16DD, including52AA CALL.
    append(uiPanelOperations(art[12], {87, 2, 146, 195}, panelColors, backPage));
    plain(2 + 2); //16E0/CALL5718 and5718 reset/RET.
    for (std::size_t index = 0; index < menuItems; ++index) {
        plain(9); //Nine argument setup/CALL5720 instructions for every widget.
        plain(19); //5720..574A.
        result.operations.push_back({NormalWorkKind::repeatString, recordWords}); //574D REP MOVSW.
        constructRecord(result.scratch, index);
        plain(57); //574F..5800; both nonzero side branches, including CALL583A.
        plain(22); //583A..5873, enabled branch, including bitmap CALLF.
        bitmap(index == 0 ? firstWidgetBitmap : ordinaryWidgetBitmap, widgetX, rows[index]);
        plain(3 + 6); //5878..587F released branch;58DF..58EE with text CALL.
        centeredText(labels[index], widgetX + 60, rows[index] + 7);
        plain(6); //58F1..58F9.
        plain(7); //5803..580F, including widget count increment.
        plain(10); //Caller cleanup, link arguments and5810 CALL.
        linkRecord(result.scratch, index);
        plain(16); //5810..5838.
        plain(1); //5810 caller stack cleanup.
    }
    plain(9); //17B5..17C7, including title bitmap CALLF.
    bitmap(16, 104, 12);
    plain(8); //17CC..17DB, including2FA6 CALL.
    centeredText("STELLAR 7", 160, 43);
    plain(1); //17DE cleanup;17E1/front copy belongs to the continuation.
    return result;
}

} // namespace stellar7
