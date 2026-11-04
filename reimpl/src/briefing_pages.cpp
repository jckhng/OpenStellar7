#include "briefing_pages.hpp"
#include "stellar7/ui_text_work.hpp"
#include "stellar7/ui_panel_work.hpp"

#include <stdexcept>

namespace stellar7 {
namespace {
constexpr Viewport display{0, 319, 0, 199};
constexpr std::uint8_t dark = 128;
constexpr std::uint8_t shadow = 156;
enum class BitmapDirection { normal, mirrored };
constexpr std::array<BriefingPreview, 15> previews{{
    {1,"SANDSLED"}, {2,"LASER TANK"}, {3,"HOVERCRAFT"}, {4,"PROWLER"},
    {5,"ASSAULT TANK"}, {250,"STALKER"}, {8,"LASER BATTERY"}, {10,"PULSAR"},
    {11,"SKIMMER"}, {13,"SEEKER"}, {29,"OBSTACLES"}, {25,"WARPLINK"},
    {31,"FUELBAY"}, {44,"GUARDIAN"}, {0,"RAVEN"}
}};
struct PreviewDetails {
    std::string_view armament;
    std::string_view propulsion;
    std::string_view speed;
    char shots;
    char armor;
};
//Immutable EXE tables024E/0262/0276 and bytes0130/013B, printed by21E6.
constexpr std::array<PreviewDetails, 10> previewDetails{{
    {"Light Laser", "Rocket Thrusters", "200", '2', '1'},
    {"Medium Laser", "Nuclear Powered Treads", "128", '1', '2'},
    {"Light Cannon", "Jet Turbine/Air Cushion", "192", '1', '1'},
    {"Light Cannon", "Nuclear Powered Treads", "160", '1', '2'},
    {"Medium Cannon", "Nuclear Powered Treads", "120", '2', '3'},
    {"???", "Agrav Generator", "???", '?', '?'},
    {"Heavy Laser", "None", "0", '2', '2'},
    {"Laser?", "None", "0", '?', '?'},
    {"Light Cannon", "Agrav and Rocket Thrusters", "240", '1', '1'},
    {"Fission Bomb", "Agrav Generator", "???", '-', '1'}
}};

void append(std::vector<IndexedPageEffect>& target, const std::vector<IndexedPageEffect>& effects) {
    target.insert(target.end(), effects.begin(), effects.end());
}

void bitmap(std::vector<IndexedPageEffect>& effects, const IndexedBitmap& image,
    const int x, const int y, const std::uint16_t page,
    const BitmapDirection direction = BitmapDirection::normal) {
    if (image.width == 0 || image.height == 0 || image.width % 4 != 0
        || image.pixels.size() != static_cast<std::size_t>(image.width) * image.height) {
        throw std::invalid_argument("briefing requires complete source UI bitmaps");
    }
    for (int row = 0; row < image.height; ++row) {
        for (int column = 0; column < image.width; ++column) {
            const auto sourceX = direction == BitmapDirection::mirrored ? image.width - 1 - column : column;
            const auto color = image.pixels[static_cast<std::size_t>(row) * image.width + sourceX];
            if (color == 0 || x + column < 0 || x + column > display.right
                || y + row < 0 || y + row > display.bottom) {
                continue;
            }
            effects.emplace_back(RadarPixelWrite{{static_cast<std::int16_t>(x + column),
                static_cast<std::int16_t>(y + row)}, color, page});
        }
    }
}

void fill(std::vector<IndexedPageEffect>& effects, const CockpitCopyRegion box,
    const std::uint8_t color, const std::uint16_t page) {
    for (int y = box.y; y < box.y + box.height; ++y) {
        for (int x = box.x; x < box.x + box.width; ++x) {
            effects.emplace_back(RadarPixelWrite{{static_cast<std::int16_t>(x),
                static_cast<std::int16_t>(y)}, color, page});
        }
    }
}

void line(std::vector<IndexedPageEffect>& effects, const int x0, const int y0,
    const int x1, const int y1, const std::uint8_t color, const std::uint16_t page) {
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            effects.emplace_back(RadarPixelWrite{{static_cast<std::int16_t>(x),
                static_cast<std::int16_t>(y)}, color, page});
        }
    }
}

//5642: high edges147, outer low128, inner low135.
void bevel(std::vector<IndexedPageEffect>& effects, const CockpitCopyRegion box,
    const std::uint16_t page) {
    const int right = box.x + box.width - 1;
    const int bottom = box.y + box.height - 1;
    line(effects, box.x, box.y, right, box.y, 147, page);
    line(effects, right, box.y, right, bottom, 147, page);
    line(effects, box.x, box.y, box.x, bottom, 128, page);
    line(effects, box.x, bottom, right, bottom, 128, page);
    line(effects, box.x + 1, box.y + 1, right - 1, box.y + 1, 147, page);
    line(effects, right - 1, box.y + 1, right - 1, bottom - 1, 147, page);
    line(effects, box.x + 1, box.y + 1, box.x + 1, bottom - 1, 135, page);
    line(effects, box.x + 1, bottom - 1, right - 1, bottom - 1, 135, page);
}

void title(std::vector<IndexedPageEffect>& effects, const BitmapFont& font,
    const std::string_view text, const int center, const int y, const std::uint16_t page) {
    const auto width = uiTextWidthOperations(font, UiFontLayout::fixed, text).width;
    const auto x = static_cast<std::int16_t>(center - width / 2);
    append(effects, uiTextOperations(font, UiFontLayout::fixed, text,
        {x, static_cast<std::int16_t>(y)}, dark, page, display).effects);
    append(effects, uiTextOperations(font, UiFontLayout::fixed, text,
        {static_cast<std::int16_t>(x + 1), static_cast<std::int16_t>(y + 1)},
        shadow, page, display).effects);
}
}

BriefingTextPages parseBriefingText(const std::vector<std::uint8_t>& decoded) {
    std::size_t position = 0;
    const auto readLine = [&]() {
        const auto start = position;
        while (position < decoded.size() && decoded[position] != '\r') {
            if (decoded[position] < 32 || decoded[position] > 126) {
                throw std::invalid_argument("briefing text requires source ASCII CRLF lines");
            }
            ++position;
        }
        if (position + 1 >= decoded.size() || decoded[position + 1] != '\n') {
            throw std::invalid_argument("truncated briefing text line");
        }
        const std::string line(decoded.begin() + start, decoded.begin() + position);
        position += 2;
        return line;
    };
    BriefingTextPages pages;
    for (auto& page : pages) {
        const auto count = readLine();
        if (count.size() != 1 || count.front() < '1' || count.front() > '9') {
            throw std::invalid_argument("briefing page requires its source one-digit line count");
        }
        for (int index = 0; index < count.front() - '0'; ++index) {
            page.push_back(readLine());
        }
    }
    if (readLine() != "0" || position != decoded.size()) {
        throw std::invalid_argument("briefing text requires nineteen pages and its terminal zero");
    }
    return pages;
}

std::optional<BriefingPreview> briefingPreview(const std::size_t page) {
    if (page >= briefingPageCount) {
        throw std::invalid_argument("briefing page outside source range");
    }
    if (page == 0 || page > previews.size()) {
        return std::nullopt;
    }
    return previews[page - 1];
}

std::vector<IndexedPageEffect> briefingShellEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& font, const std::uint16_t page) {
    if (art.size() <= 23) {
        throw std::invalid_argument("briefing requires source STELART group0");
    }
    constexpr UiPanelColors colors{{147,141,135,128,135,141,147},147,135,128};
    auto effects = uiPanelOperations(art[12], {0,0,320,200}, colors, page).effects;
    bitmap(effects, art[14], 10, 9, page);
    for (int index = 0; index < 12; ++index) {
        bitmap(effects, art[23], 34 + index * 21, 9, page);
    }
    bitmap(effects, art[14], 286, 9, page, BitmapDirection::mirrored);
    title(effects, font, "B R I E F I N G", 160, 16, page);
    fill(effects, {18, 114, 280, 47}, shadow, page);
    bevel(effects, {18, 114, 280, 47}, page);
    fill(effects, {50, 35, 246, 75}, shadow, page);
    bevel(effects, {48, 33, 250, 79}, page);
    bevel(effects, {19, 34, 26, 78}, page);
    fill(effects, {21, 36, 22, 74}, shadow, page);
    bitmap(effects, art[13], 122, 161, page);
    return effects;
}

std::vector<IndexedPageEffect> briefingTextEffects(const BriefingTextPages& pages,
    const std::size_t page, const BitmapFont& font, const std::uint16_t backPage) {
    if (page >= pages.size()) {
        throw std::invalid_argument("briefing page outside source range");
    }
    const bool preview = page > 0 && page <= previews.size();
    const int center = preview ? 158 : 173;
    int y = preview ? (page <= 10 ? 139 : 129) : 53;
    std::vector<IndexedPageEffect> effects;
    for (const auto& text : pages[page]) {
        const auto width = uiTextWidthOperations(font, UiFontLayout::proportional, text).width;
        append(effects, uiTextOperations(font, UiFontLayout::proportional, text,
            {static_cast<std::int16_t>(center - width / 2), static_cast<std::int16_t>(y)},
            dark, backPage, display).effects);
        y += 7;
    }
    return effects;
}

std::vector<IndexedPageEffect> briefingWidgetEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& font, const std::size_t widget, const MenuWidgetState state,
    const std::uint16_t page) {
    if (widget >= 6 || art.size() <= 15) {
        throw std::invalid_argument("briefing requires one of six source widgets and its art");
    }
    const bool pressed = state == MenuWidgetState::pressed;
    std::vector<IndexedPageEffect> effects;
    if (widget >= 2) {
        const int y = 39 + static_cast<int>(widget - 2) * 18;
        bitmap(effects, art[widget + 3], 24, y, page);
        if (pressed) {
            bitmap(effects, art[widget - 1], 24, y, page);
        }
        return effects;
    }
    const int x = widget == 0 ? 11 : 202;
    bitmap(effects, art[widget + 9], x, 162, page);
    if (pressed) {
        bitmap(effects, art[11], x + (widget == 0 ? 13 : 83), 177, page);
        bitmap(effects, art[15], x + (widget == 0 ? 20 : 5), 167, page,
            widget == 1 ? BitmapDirection::mirrored : BitmapDirection::normal);
    }
    title(effects, font, widget == 0 ? "EXIT" : "CONTINUE",
        x + (widget == 0 ? 60 : 44), 169, page);
    return effects;
}

std::vector<IndexedPageEffect> briefingDetailsEffects(const std::size_t page,
    const BitmapFont& titleFont, const BitmapFont& scoreFont, const std::uint16_t backPage) {
    const auto preview = briefingPreview(page);
    if (!preview) {
        return {};
    }
    std::vector<IndexedPageEffect> effects;
    if (page <= previewDetails.size()) {
        const auto& details = previewDetails[page - 1];
        const auto text = [&](const std::string& value, const ProjectedPoint point) {
            append(effects, uiTextOperations(scoreFont, UiFontLayout::proportional,
                value, point, 160, backPage, display).effects);
        };
        text("Armament: " + std::string(details.armament), {34, 117});
        text(std::string(1, details.shots) + (details.shots == '1' ? " shot" : " shots")
            + " per round", {198, 117});
        text("Propulsion: " + std::string(details.propulsion), {34, 124});
        text("Speed: " + std::string(details.speed) + " kph", {34, 131});
        auto armor = std::string("Armor: .  meters");
        armor[8] = details.armor;
        text(armor, {198, 131});
    }
    append(effects, uiTextOperations(titleFont, UiFontLayout::fixed, preview->name,
        {51, 36}, dark, backPage, display).effects);
    return effects;
}
} // namespace stellar7
