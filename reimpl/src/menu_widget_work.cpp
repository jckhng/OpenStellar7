#include "stellar7/menu_widget_work.hpp"
#include "stellar7/ui_text_work.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr Viewport display{0, 319, 0, 199};
constexpr std::size_t scratchBytes = 500;
constexpr std::size_t recordBytes = 50;
constexpr std::size_t sideBitmap = 11;
constexpr std::uint8_t darkText = 128;
constexpr std::uint8_t lightText = 156;
constexpr std::uint16_t planeCount = 4;
constexpr std::array<std::pair<std::uint16_t, std::string_view>, 5> sourceLabels{{
    {0x02c4, "BEGIN"}, {0x02ca, "BRIEFING"}, {0x02d3, "PREFS"}, {0x023c, "SCORES"}, {0x02d9, "EXIT"}
}};

enum class Field : std::size_t {
    bitmapX = 0x08, bitmapY = 0x0a, copyWidth = 0x0c, copyHeight = 0x0e,
    overlayX = 0x10, overlayY = 0x12, overlayBitmap = 0x14, overlayFlags = 0x16,
    textX = 0x18, textY = 0x1a, leftX = 0x1c, leftY = 0x1e,
    rightX = 0x20, rightY = 0x22, enabled = 0x24, baseBitmap = 0x26, label = 0x28
};

void requireRecord(const std::vector<std::uint8_t>& scratch, const std::size_t index) {
    if (scratch.size() != scratchBytes || index >= scratchBytes / recordBytes) {
        throw std::invalid_argument("menu widget requires a record in the retained500-byte allocation");
    }
}

bool physicalPage(const std::uint16_t page) {
    return page == 0xa000 || page == 0xa400 || page == 0xa800;
}

std::uint16_t field(const std::vector<std::uint8_t>& scratch, const std::size_t index, const Field value) {
    const auto offset = index * recordBytes + static_cast<std::size_t>(value);
    return static_cast<std::uint16_t>(scratch[offset] | (scratch[offset + 1] << 8));
}

ProjectedPoint position(const std::vector<std::uint8_t>& scratch, const std::size_t index,
    const Field x, const Field y) {
    return {static_cast<std::int16_t>(field(scratch, index, x)), static_cast<std::int16_t>(field(scratch, index, y))};
}

std::string_view label(const std::uint16_t pointer) {
    for (const auto& source : sourceLabels) {
        if (source.first == pointer) {
            return source.second;
        }
    }
    throw std::invalid_argument("menu widget label is outside the recovered immutable menu strings");
}
}

MenuWidgetWork menuWidgetOperations(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, const std::vector<std::uint8_t>& scratch,
    const std::size_t index, const MenuWidgetState state, const std::uint16_t backPage) {
    requireRecord(scratch, index);
    if ((state != MenuWidgetState::released && state != MenuWidgetState::pressed) || !physicalPage(backPage)
        || field(scratch, index, Field::enabled) == 0 || field(scratch, index, Field::textX) == 0
        || titleFont.firstCharacter != 32 || titleFont.height != 6 || titleFont.glyphs.size() != 64
        || std::any_of(titleFont.glyphs.begin(), titleFont.glyphs.end(), [](const auto& glyph) { return glyph.width != 8; })) {
        throw std::invalid_argument("menu widget requires an enabled source text record, FNT0 and a physical back page");
    }
    const auto text = label(field(scratch, index, Field::label));
    MenuWidgetWork result{{}, {}, lightText};
    const auto plain = [&](const std::uint64_t count) {
        result.operations.push_back({NormalWorkKind::instructions, count});
    };
    const auto append = [&](const auto& work) {
        result.operations.insert(result.operations.end(), work.operations.begin(), work.operations.end());
        result.effects.insert(result.effects.end(), work.effects.begin(), work.effects.end());
    };
    const auto bitmap = [&](const std::size_t bitmapIndex, const Field x, const Field y) {
        if (bitmapIndex >= art.size()) {
            throw std::invalid_argument("menu widget bitmap is outside the retained art group");
        }
        append(uiBitmapOperations(art[bitmapIndex], position(scratch, index, x, y), display, backPage));
    };
    plain(22); //583A..5873 enabled branch, including base CALLF.
    bitmap(field(scratch, index, Field::baseBitmap), Field::bitmapX, Field::bitmapY);
    plain(3); //5878..587F released test.
    if (state == MenuWidgetState::pressed) {
        plain(2); //5881..5885 left-side test.
        if (field(scratch, index, Field::leftX) != 0) {
            plain(7);
            bitmap(sideBitmap, Field::leftX, Field::leftY);
            plain(1);
        }
        plain(2); //589F..58A3 right-side test.
        if (field(scratch, index, Field::rightX) != 0) {
            plain(7);
            bitmap(sideBitmap, Field::rightX, Field::rightY);
            plain(1);
        }
        plain(2); //58BD..58C1 overlay test.
        const auto overlay = field(scratch, index, Field::overlayBitmap);
        if (overlay != 0) {
            if (field(scratch, index, Field::overlayFlags) != 0) {
                throw std::invalid_argument("menu widget overlay requires normal source bitmap flags");
            }
            plain(8);
            bitmap(overlay, Field::overlayX, Field::overlayY);
            plain(1);
        }
    }
    plain(6); //58DF..58EE nonzero text center, including2FA6 CALL.
    plain(8); //2FA6..2FB4, including49E4 CALLF.
    const auto width = uiTextWidthOperations(titleFont, UiFontLayout::fixed, text);
    result.operations.insert(result.operations.end(), width.operations.begin(), width.operations.end());
    const auto center = position(scratch, index, Field::textX, Field::textY);
    const auto left = static_cast<std::int16_t>(center.x - width.width / 2);
    plain(11); //2FB9..2FD0, including first4C0E CALLF.
    append(uiTextOperations(titleFont, UiFontLayout::fixed, text, {left, center.y}, darkText, backPage, display));
    plain(10); //2FD5..2FE8, including second4C0E CALLF.
    append(uiTextOperations(titleFont, UiFontLayout::fixed, text,
        {static_cast<std::int16_t>(left + 1), static_cast<std::int16_t>(center.y + 1)}, lightText, backPage, display));
    plain(6); //2FED..2FF5.
    plain(6); //58F1..58F9 caller cleanup/RET.
    return result;
}

UiBitmapWork menuWidgetCopyOperations(const std::vector<std::uint8_t>& scratch,
    const std::size_t index, const std::uint16_t backPage, const std::uint16_t frontPage) {
    requireRecord(scratch, index);
    if (!physicalPage(backPage) || !physicalPage(frontPage) || backPage == frontPage) {
        throw std::invalid_argument("menu widget copy requires distinct physical back/front pages");
    }
    UiBitmapWork result{};
    result.operations.push_back({NormalWorkKind::instructions, 13}); //58FA..591A.
    if (field(scratch, index, Field::enabled) == 0) {
        result.operations.push_back({NormalWorkKind::instructions, 4}); //5930..5934.
        return result;
    }
    const auto origin = position(scratch, index, Field::bitmapX, Field::bitmapY);
    const auto width = field(scratch, index, Field::copyWidth);
    const auto height = field(scratch, index, Field::copyHeight);
    if (origin.x < 0 || origin.y < 0 || int{origin.y} + height > display.bottom + 1) {
        throw std::invalid_argument("menu widget copy rectangle is outside the source display");
    }
    const auto body = planarCopyOperations(origin.x, width, height);
    const auto groups = (width + 2 * (origin.x % planeCount) + planeCount - 1) / planeCount;
    result.operations.push_back({NormalWorkKind::instructions, 5 + 1}); //591C..5928 and22A1 JMPF.
    result.operations.insert(result.operations.end(), body.begin(), body.end());
    result.effects.push_back(RadarPageCopy{backPage, frontPage,
        {static_cast<std::int16_t>(origin.x / planeCount * planeCount), origin.y},
        static_cast<std::uint16_t>(groups * planeCount), height});
    result.operations.push_back({NormalWorkKind::instructions, 5}); //592D..5934.
    return result;
}

} // namespace stellar7
