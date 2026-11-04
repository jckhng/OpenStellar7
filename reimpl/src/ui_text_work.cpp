#include "stellar7/ui_text_work.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace stellar7 {
namespace {
using Kind = NormalWorkKind;
constexpr int screenWidth = 320;
constexpr int screenHeight = 200;
constexpr std::size_t maskBits = 8;

std::uint16_t validate(const BitmapFont& font, const UiFontLayout layout,
    const std::string_view text) {
    if ((layout != UiFontLayout::fixed && layout != UiFontLayout::proportional)
        || font.height == 0 || font.glyphs.empty() || font.glyphs.size() > 255
        || font.firstCharacter + font.glyphs.size() > 256 || text.size() >= 65535) {
        throw std::invalid_argument("UI text requires a complete byte-indexed font and bounded string");
    }
    const auto fixedWidth = font.glyphs.front().width;
    for (const auto& glyph : font.glyphs) {
        if (glyph.width > 255 || glyph.height != font.height
            || glyph.pixels.size() != static_cast<std::size_t>(glyph.width) * font.height
            || (layout == UiFontLayout::fixed && (fixedWidth == 0 || glyph.width != fixedWidth))
            || std::any_of(glyph.pixels.begin(), glyph.pixels.end(), [](const auto pixel) { return pixel > 1; })) {
            throw std::invalid_argument("UI text requires complete binary glyphs with the declared layout");
        }
    }
    std::size_t width = 0;
    for (const auto character : text) {
        const auto value = static_cast<unsigned char>(character);
        if (value == 0 || value < font.firstCharacter
            || static_cast<std::size_t>(value - font.firstCharacter) >= font.glyphs.size()) {
            throw std::invalid_argument("UI text character is outside the admitted font range");
        }
        width += font.glyphs[value - font.firstCharacter].width;
    }
    if (width > static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max())) {
        throw std::invalid_argument("UI text width exceeds the admitted signed coordinate domain");
    }
    return static_cast<std::uint16_t>(width);
}

void instructions(std::vector<NormalWorkOperation>& operations, const std::uint64_t count) {
    if (count == 0) {
        return;
    }
    if (!operations.empty() && operations.back().kind == Kind::instructions) {
        operations.back().count += count;
        return;
    }
    operations.push_back({Kind::instructions, count});
}
}

UiTextWidthWork uiTextWidthOperations(const BitmapFont& font, const UiFontLayout layout,
    const std::string_view text) {
    UiTextWidthWork result{validate(font, layout, text), {}};
    if (layout == UiFontLayout::fixed) {
        instructions(result.operations, 13); //49E4..4A30.
        result.operations.push_back({Kind::repeatString, text.size() + 1}); //4A32 scans the terminator.
        instructions(result.operations, 13); //4A34..4A4A.
        return result;
    }
    instructions(result.operations, 20 + 15 * text.size()); //49F5 valid-character loop and RETF.
    return result;
}

UiTextWork uiTextOperations(const BitmapFont& font, const UiFontLayout layout,
    const std::string_view text, const ProjectedPoint origin, const std::uint8_t color,
    const std::uint16_t page, const Viewport viewport) {
    UiTextWork result{validate(font, layout, text), {}, {}, {}};
    if ((page != 0xa000 && page != 0xa400 && page != 0xa800)
        || viewport.left < 0 || viewport.right >= screenWidth || viewport.left > viewport.right
        || viewport.top < 0 || viewport.bottom >= screenHeight || viewport.top > viewport.bottom) {
        throw std::invalid_argument("UI text requires a physical page and bounded viewport");
    }
    std::uint64_t dispatches = 0;
    const auto emit = [&](const std::uint64_t count) {
        instructions(result.operations, count);
        dispatches += count;
    };
    emit(text.empty() ? 12 : 8); //4C0E empty return or first character.
    int x = origin.x;
    for (const auto character : text) {
        const auto& glyph = font.glyphs[static_cast<unsigned char>(character) - font.firstCharacter];
        //4ADB/4AE6 test the exclusive end against inclusive viewport bounds.
        if (x < viewport.left || origin.y < viewport.top
            || x + glyph.width > viewport.right || origin.y + font.height > viewport.bottom) {
            throw std::invalid_argument("UI text clipped-pixel fallback is not admitted");
        }
        emit(5); //4C1E..4C25, including near CALL4A4C.
        emit(layout == UiFontLayout::fixed ? 58 : 59); //4A4C..4B23.
        for (std::size_t row = 0; row < font.height; ++row) {
            emit(8); //4B26..4B59; width0 takes the equal-length 4B56 jump.
            for (std::size_t column = 0; column < glyph.width; ++column) {
                emit(8 + (column != 0 && column % maskBits == 0 ? 2 : 0)); //Mask rollover and glyph test.
                if (glyph.pixels[row * glyph.width + column] == 0) {
                    emit(2); //4BAE style8 disabled.
                } else {
                    emit(8); //4B7A..4B95, including CALLF0FEB.
                    emit(12); //0FEB..1009.
                    result.operations.push_back({Kind::port16Write, 1}); //100B map mask.
                    ++dispatches;
                    emit(8); //100C..101D, including the indexed byte store.
                    result.effects.push_back(RadarPixelWrite{
                        {static_cast<std::int16_t>(x + column), static_cast<std::int16_t>(origin.y + row)}, color, page});
                    result.pixelDispatches.push_back(dispatches);
                    emit(3 + 3); //1020..1022;4B98 and disabled style2.
                }
                emit(5); //4BCC..4BD8.
            }
            emit(8 + (row + 1 < font.height ? 1 : 0)); //4BDA..4BF9.
        }
        emit(6); //4BFC..4C04 near RET.
        emit(7); //4C28..4C39.
        x += glyph.width;
    }
    if (!text.empty()) {
        emit(5); //4C3B..4C40 far RET.
    }
    return result;
}

} // namespace stellar7
