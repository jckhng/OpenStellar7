#include "stellar7/indexed_render.hpp"
#include "playable_system_pages.hpp"
#include "stellar7/scene_script.hpp"

#include <stdexcept>
#include <string>
#include <algorithm>
#include <limits>
#include <type_traits>

namespace stellar7 {
namespace {

constexpr Viewport display{0, 319, 0, 199};
constexpr Viewport horizonViewport{24, 295, 24, 145};
constexpr std::uint16_t planeCount = 4;
constexpr std::uint16_t panoramaWidth = 256;
constexpr std::int16_t cockpitOverlayStage = 70;
constexpr std::uint16_t compatibilityCockpitCachePage = 0xa800;
constexpr std::uint16_t recoveredCockpitCachePage = 0xa400;

std::uint16_t cockpitCachePage(const CockpitPageLayout layout) {
    switch (layout) {
    case CockpitPageLayout::compatibility:
        return compatibilityCockpitCachePage;
    case CockpitPageLayout::recoveredStartup:
        return recoveredCockpitCachePage;
    default:
        throw std::invalid_argument("unsupported cockpit page layout");
    }
}

void appendBitmap(std::vector<IndexedPageEffect>& effects, const IndexedBitmap& bitmap,
    const ProjectedPoint position, const PlanarBlitMode mode, const Viewport clip,
    const std::uint16_t page) {
    if (bitmap.width == 0 || bitmap.height == 0 || bitmap.width % planeCount != 0
        || bitmap.pixels.size() != static_cast<std::size_t>(bitmap.width) * bitmap.height) {
        throw std::invalid_argument("invalid indexed effect bitmap");
    }
    for (std::uint16_t plane = 0; plane < planeCount; ++plane) {
        for (std::uint16_t row = 0; row < bitmap.height; ++row) {
            for (std::uint16_t column = plane; column < bitmap.width; column += planeCount) {
                const auto x = static_cast<std::int32_t>(position.x) + column;
                const auto y = static_cast<std::int32_t>(position.y) + row;
                const auto color = bitmap.pixels[static_cast<std::size_t>(row) * bitmap.width + column];
                if (x < clip.left || x > clip.right || y < clip.top || y > clip.bottom
                    || (mode != PlanarBlitMode::opaque && color == 0)) {
                    continue;
                }
                effects.emplace_back(RadarPixelWrite{
                    {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)}, color, page});
            }
        }
    }
}

const IndexedBitmap& cockpitBitmap(const CockpitWorkBitmaps& bitmaps, const CockpitBitmapRole role) {
    if (role == CockpitBitmapRole::strip) {
        return bitmaps.strip;
    }
    if (role == CockpitBitmapRole::indicator) {
        return bitmaps.indicator;
    }
    if (role == CockpitBitmapRole::matchingPageBackground) {
        return bitmaps.matchingPageBackground;
    }
    const auto index = static_cast<std::size_t>(role) - static_cast<std::size_t>(CockpitBitmapRole::overlay0);
    return bitmaps.overlays.at(index);
}

void appendUiLine(std::vector<IndexedPageEffect>& effects, ProjectedPoint first,
    const ProjectedPoint last, const std::uint8_t color, const std::uint16_t page) {
    if (first.x != last.x && first.y != last.y) {
        throw std::invalid_argument("UI border must be axis aligned");
    }
    for (auto y = std::min(first.y, last.y); y <= std::max(first.y, last.y); ++y) {
        for (auto x = std::min(first.x, last.x); x <= std::max(first.x, last.x); ++x) {
            if (x >= display.left && x <= display.right && y >= display.top && y <= display.bottom) {
                effects.emplace_back(RadarPixelWrite{{x, y}, color, page});
            }
        }
    }
}

void appendUiText(std::vector<IndexedPageEffect>& effects, const BitmapFont& font,
    const std::string_view text, const ProjectedPoint center, const std::uint16_t page) {
    if (font.firstCharacter != 32 || font.height != 6 || font.glyphs.size() != 64) {
        throw std::invalid_argument("menu requires source font12ca");
    }
    auto width = 0;
    for (const unsigned char character : text) {
        if (character < font.firstCharacter || character - font.firstCharacter >= font.glyphs.size()) {
            throw std::invalid_argument("menu character outside source font");
        }
        const auto& glyph = font.glyphs.at(character - font.firstCharacter);
        if (glyph.width != 8 || glyph.height != font.height || glyph.pixels.size() != 8 * font.height) {
            throw std::invalid_argument("malformed source UI glyph");
        }
        width += glyph.width;
    }
    //2fa6 draws128 first, then156 at+1,+1 using the selected font's width.
    for (const auto offset : {0, 1}) {
        auto x = center.x - width / 2 + offset;
        for (const unsigned char character : text) {
            const auto& glyph = font.glyphs.at(character - font.firstCharacter);
            for (std::uint16_t y = 0; y < glyph.height; ++y) {
                for (std::uint16_t column = 0; column < glyph.width; ++column) {
                    const auto ink = glyph.pixels[y * glyph.width + column];
                    if (ink > 1) {
                        throw std::invalid_argument("UI glyph is not a binary mask");
                    }
                    if (ink != 0) {
                        effects.emplace_back(RadarPixelWrite{{static_cast<std::int16_t>(x + column),
                            static_cast<std::int16_t>(center.y + y + offset)},
                            static_cast<std::uint8_t>(offset == 0 ? 128 : 156), page});
                    }
                }
            }
            x += glyph.width;
        }
    }
}


}

std::vector<IndexedPageEffect> dialogPanelPageEffects(const IndexedBitmap& tile,
    const CockpitCopyRegion rectangle, const std::uint16_t page) {
    if (tile.width != 40 || tile.height != 40 || rectangle.width < 14 || rectangle.height < 14
        || rectangle.x < 1 || rectangle.y < 1 || rectangle.x + rectangle.width >= 320
        || rectangle.y + rectangle.height >= 200) {
        throw std::invalid_argument("unsupported source UI panel");
    }
    std::vector<IndexedPageEffect> effects;
    const auto x = rectangle.x;
    const auto y = rectangle.y;
    const auto right = static_cast<std::int16_t>(x + rectangle.width);
    const auto bottom = static_cast<std::int16_t>(y + rectangle.height);
    for (auto row = y; row < bottom - 1; row += tile.height) {
        for (auto column = x; column < right - 1; column += tile.width) {
            appendBitmap(effects, tile, {column, row}, PlanarBlitMode::generalTransparent,
                {x, static_cast<std::int16_t>(right - 1), y, static_cast<std::int16_t>(bottom - 1)}, page);
        }
    }
    appendUiLine(effects, {x, y}, {static_cast<std::int16_t>(right - 1), y}, 128, page);
    appendUiLine(effects, {static_cast<std::int16_t>(right - 1), y},
        {static_cast<std::int16_t>(right - 1), static_cast<std::int16_t>(bottom - 1)}, 128, page);
    appendUiLine(effects, {x, static_cast<std::int16_t>(bottom - 1)},
        {static_cast<std::int16_t>(right - 1), static_cast<std::int16_t>(bottom - 1)}, 135, page);
    appendUiLine(effects, {x, y}, {x, static_cast<std::int16_t>(bottom - 1)}, 135, page);
    constexpr std::array<std::uint8_t, 7> colors{147, 141, 135, 128, 135, 141, 147};
    const auto line = [&](const int x0, const int y0, const int x1, const int y1, const std::uint8_t color) {
        appendUiLine(effects, {static_cast<std::int16_t>(x0), static_cast<std::int16_t>(y0)},
            {static_cast<std::int16_t>(x1), static_cast<std::int16_t>(y1)}, color, page);
    };
    for (int inset = 0; inset < static_cast<int>(colors.size()); ++inset) {
        const auto color = colors[inset];
        line(x + inset, y + inset, right - inset, y + inset, color);
        line(x + inset, bottom - inset, right - inset, bottom - inset, color);
        line(x + inset, y + inset, x + inset, bottom - inset, color);
        line(right - inset, y + inset, right - inset, bottom - inset, color);
    }
    const auto quarterWidth = rectangle.width / 4;
    const auto quarterHeight = rectangle.height / 4;
    for (int inset = -1; inset <= 5; ++inset) {
        const auto color = colors[inset + 1];
        line(x + quarterWidth + inset, y + inset, right - quarterWidth - inset, y + inset, color);
        line(x + quarterWidth + inset, bottom - inset, right - quarterWidth - inset, bottom - inset, color);
        line(x + inset, y + quarterHeight + inset, x + inset, bottom - quarterHeight - inset, color);
        line(right - inset, y + quarterHeight + inset, right - inset, bottom - quarterHeight - inset, color);
    }
    return effects;
}

std::vector<IndexedPageEffect> mainMenuWidgetEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& font, const std::size_t selection, const MenuWidgetState state, const std::uint16_t page) {
    constexpr std::array<std::int16_t, 5> rows{58, 90, 115, 140, 165};
    constexpr std::array<std::string_view, 5> labels{"BEGIN", "BRIEFING", "PREFS", "SCORES", "EXIT"};
    if (selection >= rows.size() || art.size() <= 22) {
        throw std::invalid_argument("unsupported source menu widget");
    }
    std::vector<IndexedPageEffect> effects;
    const auto y = rows[selection];
    appendBitmap(effects, art[selection == 0 ? 17 : 18], {102, y}, PlanarBlitMode::generalTransparent, display, page);
    if (state == MenuWidgetState::pressed) {
        const auto sideY = static_cast<std::int16_t>(y + (selection == 0 ? 14 : 7));
        appendBitmap(effects, art[11], {static_cast<std::int16_t>(selection == 0 ? 116 : 112), sideY},
            PlanarBlitMode::generalTransparent, display, page);
        appendBitmap(effects, art[11], {static_cast<std::int16_t>(selection == 0 ? 201 : 206), sideY},
            PlanarBlitMode::generalTransparent, display, page);
        appendBitmap(effects, art[selection == 0 ? 22 : 21],
            {122, static_cast<std::int16_t>(y + 5)},
            PlanarBlitMode::generalTransparent, display, page);
    }
    appendUiText(effects, font, labels[selection], {162, static_cast<std::int16_t>(y + 7)}, page);
    return effects;
}

std::vector<IndexedPageEffect> mainMenuPageEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& font, const std::uint16_t page) {
    if (art.size() <= 22 || art[20].width != 264 || art[20].height != 2) {
        throw std::invalid_argument("menu requires original STELART UI bitmaps");
    }
    std::vector<IndexedPageEffect> effects;
    auto x = 0;
    for (std::int16_t y = 0; y < 200; y += 2, x += 3) {
        const auto blit = [&](const int column) {
            appendBitmap(effects, art[20], {static_cast<std::int16_t>(column), y},
                PlanarBlitMode::generalTransparent, display, page);
        };
        if (y > 174) { blit(x - 524); }
        blit(x - 262);
        if (x < 320) { blit(x); }
        if (x < 58) { blit(x + 262); }
    }
    const auto append = [&](const auto& work) { effects.insert(effects.end(), work.begin(), work.end()); };
    append(dialogPanelPageEffects(art[12], {87, 2, 146, 195}, page));
    for (std::size_t selection = 0; selection < 5; ++selection) {
        append(mainMenuWidgetEffects(art, font, selection, MenuWidgetState::released, page));
    }
    appendBitmap(effects, art[16], {104, 12}, PlanarBlitMode::generalTransparent, display, page);
    appendUiText(effects, font, "STELLAR 7", {160, 43}, page);
    return effects;
}

std::vector<IndexedPageEffect> cockpitResourcePageEffects(const IndexedScreen& screen,
    const std::uint16_t cachePage, const CockpitPageLayout layout) {
    constexpr std::uint16_t width = display.right + 1;
    constexpr std::uint16_t height = display.bottom + 1;
    if (cachePage != cockpitCachePage(layout) || screen.pixels.size() != width * height) {
        throw std::invalid_argument("unsupported cockpit resource page");
    }
    std::vector<IndexedPageEffect> effects;
    appendBitmap(effects, {width, height, screen.pixels}, {0, 0}, PlanarBlitMode::opaque,
        display, cachePage);
    return effects;
}

std::vector<IndexedPageEffect> horizonPageEffects(const HorizonWorkState& state,
    const std::vector<IndexedBitmap>& frames, const HorizonColors colors, const std::uint16_t page) {
    const auto middle = state.viewTop + (state.viewBottom - state.viewTop) / 2;
    if (state.viewTop != horizonViewport.top || state.viewBottom != horizonViewport.bottom
        || state.cameraYaw != 0 || state.stage <= cockpitOverlayStage || state.background != HorizonBackground::solid
        || (state.overlay != HorizonOverlay::disabled && state.overlay != HorizonOverlay::enabled)
        || state.bitmapWidth != panoramaWidth || state.bitmapHeight == 0
        || state.bitmapHeight >= middle - state.viewTop || frames.size() < 4) {
        throw std::invalid_argument("unsupported horizon indexed effects");
    }
    for (std::size_t index = 0; index < 4; ++index) {
        if (frames[index].width != state.bitmapWidth || frames[index].height != state.bitmapHeight) {
            throw std::invalid_argument("horizon payload differs from work profile");
        }
    }
    std::vector<IndexedPageEffect> effects;
    const auto fill = [&](const int top, const int bottom, const std::uint8_t color) {
        for (int y = top; y <= bottom; ++y) {
            for (int x = horizonViewport.left; x <= horizonViewport.right; ++x) {
                effects.emplace_back(RadarPixelWrite{
                    {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)}, color, page});
            }
        }
    };
    fill(state.viewTop, middle - state.bitmapHeight - 1, colors.sky);
    fill(middle, state.viewBottom, colors.ground);
    const auto panoramaTop = static_cast<std::int16_t>(middle - state.bitmapHeight);
    appendBitmap(effects, frames[0], {horizonViewport.left, panoramaTop},
        PlanarBlitMode::opaque, horizonViewport, page);
    appendBitmap(effects, frames[1], {horizonViewport.left + panoramaWidth, panoramaTop},
        PlanarBlitMode::opaque, horizonViewport, page);
    return effects;
}

std::vector<IndexedPageEffect> cockpitPageEffects(const CockpitWork& work,
    const CockpitWorkBitmaps& bitmaps, const std::uint16_t cachePage, const std::uint16_t drawPage) {
    std::vector<IndexedPageEffect> effects;
    for (const auto& effect : work.effects) {
        if (const auto* copy = std::get_if<CockpitCopyRegion>(&effect)) {
            effects.emplace_back(RadarPageCopy{cachePage, drawPage, {copy->x, copy->y},
                static_cast<std::uint16_t>(copy->width), static_cast<std::uint16_t>(copy->height)});
            continue;
        }
        const auto& placement = std::get<CockpitBitmapPlacement>(effect);
        const auto& bitmap = cockpitBitmap(bitmaps, placement.bitmap);
        if (placement.x < display.left || placement.y < display.top
            || static_cast<std::int32_t>(placement.x) + bitmap.width - 1 > display.right
            || static_cast<std::int32_t>(placement.y) + bitmap.height - 1 > display.bottom) {
            throw std::invalid_argument("cockpit indexed bitmap crosses display boundary");
        }
        appendBitmap(effects, bitmap, {placement.x, placement.y},
            placement.mode, display, drawPage);
    }
    return effects;
}

std::vector<IndexedPageEffect> cockpitCachePageEffects(const CockpitWork& matching,
    const CockpitWork& ordinary, const CockpitWorkBitmaps& bitmaps,
    const BitmapFont& font, const std::string_view title, const std::uint8_t titleColor,
    const std::uint16_t cachePage, const std::uint16_t backPage, const CockpitPageLayout layout,
    const CockpitCacheDomain domain) {
    constexpr std::uint16_t maximumGlyphWidth = 22;
    constexpr std::uint16_t maximumGlyphHeight = 8;
    constexpr std::uint16_t titleWindowWidth = 120;
    constexpr std::int16_t titleY = 30;
    const auto expectedCachePage = cockpitCachePage(layout);
    const auto supportedBackPage = layout == CockpitPageLayout::compatibility
        ? backPage == 0xa000 || backPage == recoveredCockpitCachePage
        : backPage == compatibilityCockpitCachePage
            || (domain == CockpitCacheDomain::retainedGameplay && backPage == 0xa000);
    if (matching.state.pages != CockpitPageRelation::matching
        || ordinary.state.pages != CockpitPageRelation::differing
        || (domain != CockpitCacheDomain::initialLoader && domain != CockpitCacheDomain::retainedGameplay)
        || cachePage != expectedCachePage || !supportedBackPage
        || font.height == 0 || font.height > maximumGlyphHeight || font.glyphs.empty()
        || title.empty() || title.size() > titleWindowWidth) {
        throw std::invalid_argument("unsupported cockpit cache initialization");
    }
    std::uint16_t titleWidth = 0;
    for (const unsigned char character : title) {
        if (character < font.firstCharacter || character - font.firstCharacter >= font.glyphs.size()) {
            throw std::invalid_argument("cockpit title character outside original font");
        }
        const auto& glyph = font.glyphs[character - font.firstCharacter];
        if (glyph.width == 0 || glyph.width > maximumGlyphWidth || glyph.height != font.height
            || glyph.pixels.size() != glyph.width * glyph.height) {
            throw std::invalid_argument("unsupported cockpit title glyph");
        }
        titleWidth += glyph.width;
    }
    if (titleWidth > titleWindowWidth) {
        throw std::invalid_argument("cockpit title crosses cached window");
    }
    auto effects = cockpitPageEffects(matching, bitmaps, cachePage, cachePage);
    //7936 halves the width first, then subtracts from screen center.
    auto x = static_cast<std::int16_t>((display.right + 1) / 2 - titleWidth / 2);
    for (const unsigned char character : title) {
        const auto& glyph = font.glyphs[character - font.firstCharacter];
        for (std::uint16_t row = 0; row < glyph.height; ++row) {
            for (std::uint16_t column = 0; column < glyph.width; ++column) {
                const auto ink = glyph.pixels[row * glyph.width + column];
                if (ink > 1) {
                    throw std::invalid_argument("cockpit title glyph is not a binary mask");
                }
                if (ink != 0) {
                    effects.emplace_back(RadarPixelWrite{{static_cast<std::int16_t>(x + column),
                        static_cast<std::int16_t>(titleY + row)}, titleColor, cachePage});
                }
            }
        }
        x += glyph.width;
    }
    effects.emplace_back(RadarPageCopy{cachePage, backPage, {0, 0}, 320, 200});
    for (std::int16_t y = horizonViewport.top; y <= horizonViewport.bottom; ++y) {
        for (std::int16_t column = horizonViewport.left; column <= horizonViewport.right; ++column) {
            effects.emplace_back(RadarPixelWrite{{column, y}, 0, backPage});
        }
    }
    const auto cockpit = cockpitPageEffects(ordinary, bitmaps, cachePage, backPage);
    effects.insert(effects.end(), cockpit.begin(), cockpit.end());
    return effects;
}

namespace {

void checkHudPages(const std::uint16_t back, const std::uint16_t front, const CockpitPageLayout layout) {
    std::uint16_t secondPage;
    switch (layout) {
    case CockpitPageLayout::compatibility:
        secondPage = 0xa400;
        break;
    case CockpitPageLayout::recoveredStartup:
        secondPage = 0xa800;
        break;
    default:
        throw std::invalid_argument("unsupported cockpit page layout");
    }
    if ((back != 0xa000 && back != secondPage) || (front != 0xa000 && front != secondPage) || back == front) {
        throw std::invalid_argument("unsupported HUD page roles");
    }
}

void appendHudFill(std::vector<IndexedPageEffect>& effects, const CockpitCopyRegion rectangle,
    const std::uint8_t color, const std::uint16_t page) {
    if (rectangle.width <= 0 || rectangle.height <= 0 || rectangle.x < 0 || rectangle.y < 0
        || rectangle.x + rectangle.width > 320 || rectangle.y + rectangle.height > 200) {
        throw std::invalid_argument("HUD rectangle outside display");
    }
    for (std::int16_t y = rectangle.y; y < rectangle.y + rectangle.height; ++y) {
        for (std::int16_t x = rectangle.x; x < rectangle.x + rectangle.width; ++x) {
            effects.emplace_back(RadarPixelWrite{{x, y}, color, page});
        }
    }
}

void appendHudCopy(std::vector<IndexedPageEffect>& effects, const CockpitCopyRegion rectangle,
    const std::uint16_t back, const std::uint16_t front) {
    const auto work = planarCopyWork(rectangle.x, rectangle.width, rectangle.height);
    const auto coveredWidth = static_cast<std::uint16_t>(4 * work.stringIterations / rectangle.height);
    const auto left = static_cast<std::int16_t>(rectangle.x - rectangle.x % 4);
    effects.emplace_back(RadarPageCopy{back, front, {left, rectangle.y}, coveredWidth,
        static_cast<std::uint16_t>(rectangle.height)});
}

void appendHudBitmap(std::vector<IndexedPageEffect>& effects, const IndexedBitmap& bitmap,
    const ProjectedPoint position, const std::uint16_t back) {
    if (position.x < 0 || position.y < 0 || position.x + bitmap.width > 320
        || position.y + bitmap.height > 200) {
        throw std::invalid_argument("HUD bitmap outside display");
    }
    appendBitmap(effects, bitmap, position, PlanarBlitMode::generalTransparent, display, back);
}

}

std::vector<IndexedPageEffect> scorePageEffects(const std::uint32_t score, const BitmapFont& font,
    const std::uint16_t backPage, const std::uint16_t frontPage, const CockpitPageLayout layout) {
    checkHudPages(backPage, frontPage, layout);
    constexpr CockpitCopyRegion scoreRectangle{240, 155, 30, 6};
    constexpr std::uint8_t scoreColor = 239; // Source12e9.
    if (font.firstCharacter != 32 || font.height != 8 || font.glyphs.size() != 96) {
        throw std::invalid_argument("unsupported source score font profile");
    }
    const auto signedScore = score <= 0x7fffffffU ? static_cast<std::int64_t>(score)
        : static_cast<std::int64_t>(score) - 0x100000000LL;
    const auto text = std::to_string(signedScore) + ' '; //7a1c terminates with space/NUL.
    std::vector<IndexedPageEffect> effects;
    appendHudFill(effects, scoreRectangle, 0, backPage);
    std::int32_t x = scoreRectangle.x;
    for (const unsigned char character : text) {
        const auto& glyph = font.glyphs.at(character - font.firstCharacter);
        if (glyph.width == 0 || glyph.width > 22 || glyph.height != font.height
            || glyph.pixels.size() != static_cast<std::size_t>(glyph.width) * glyph.height
            || x + glyph.width > 320) {
            throw std::invalid_argument("malformed score glyph");
        }
        for (std::uint16_t y = 0; y < glyph.height; ++y) {
            for (std::uint16_t column = 0; column < glyph.width; ++column) {
                const auto ink = glyph.pixels[static_cast<std::size_t>(y) * glyph.width + column];
                if (ink > 1) {
                    throw std::invalid_argument("score glyph is not a binary mask");
                }
                if (ink != 0) {
                    effects.emplace_back(RadarPixelWrite{{static_cast<std::int16_t>(x + column),
                        static_cast<std::int16_t>(scoreRectangle.y + y)}, scoreColor, backPage});
                }
            }
        }
        x += glyph.width;
    }
    appendHudCopy(effects, scoreRectangle, backPage, frontPage);
    return effects;
}

namespace {
    constexpr std::array<std::uint8_t, 3> colors{146, 137, 176}; // Source12ec.
    constexpr std::array<CockpitCopyRegion, 7> rectangles{{{7, 151, 29, 17}, {7, 167, 29, 17},
        {35, 151, 26, 33}, {60, 151, 30, 17}, {60, 167, 30, 17}, {7, 183, 41, 17}, {47, 183, 43, 17}}};
    constexpr std::array<ProjectedPoint, 7> icons{{{14, 156}, {17, 170}, {42, 155}, {64, 153},
        {66, 172}, {14, 187}, {50, 186}}};
    constexpr std::size_t pipCount = 3;
    constexpr std::array<ProjectedPoint, 7 * pipCount> pips{{{10, 154}, {10, 158}, {10, 162},
        {10, 170}, {10, 174}, {10, 178}, {42, 178}, {46, 178}, {50, 178},
        {84, 154}, {84, 158}, {84, 162}, {84, 170}, {84, 174}, {84, 178},
        {10, 186}, {10, 190}, {10, 194}, {84, 186}, {84, 190}, {84, 194}}};
std::vector<IndexedPageEffect> statusEffects(const std::array<std::int8_t, 7>& status,
    const std::array<std::int16_t, 7>& timers, const std::int16_t selection,
    const std::vector<IndexedBitmap>& artGroup4, const std::uint16_t backPage, const std::uint16_t frontPage,
    const std::size_t firstSystem, const std::size_t endSystem, const CockpitPageLayout layout) {
    checkHudPages(backPage, frontPage, layout);
    if (selection != -1 || artGroup4.size() < 57 || firstSystem >= endSystem || endSystem > status.size()) {
        throw std::invalid_argument("unsupported HUD selection or resources");
    }
    std::vector<IndexedPageEffect> effects;
    for (std::size_t index = firstSystem; index < endSystem; ++index) {
        if (status[index] == -128) {
            throw std::invalid_argument("unsupported wrapped HUD status negation");
        }
        const auto level = status[index] < 0 ? -static_cast<int>(status[index]) : status[index];
        const auto state = timers[index] != 0 ? 2U : level != 0 ? 1U : 0U;
        const auto rectangle = rectangles[index];
        appendHudFill(effects, {static_cast<std::int16_t>(rectangle.x + 2),
            static_cast<std::int16_t>(rectangle.y + 2), static_cast<std::int16_t>(rectangle.width - 4),
            static_cast<std::int16_t>(rectangle.height - 4)}, colors[state], backPage);
        appendHudBitmap(effects, artGroup4[(level == 0 && state == 0 ? 50 : 41) + index], icons[index], backPage);
        for (std::size_t pip = 0; pip < pipCount; ++pip) {
            appendHudBitmap(effects, artGroup4[level > static_cast<int>(pip) ? 49 : 48],
                pips[index * pipCount + pip], backPage);
        }
        appendHudCopy(effects, rectangle, backPage, frontPage);
    }
    return effects;
}
}

std::vector<IndexedPageEffect> statusPageEffects(const std::array<std::int8_t, 7>& status,
    const std::array<std::int16_t, 7>& timers, const std::int16_t selection,
    const std::vector<IndexedBitmap>& artGroup4, const std::uint16_t backPage, const std::uint16_t frontPage,
    const CockpitPageLayout layout) {
    return statusEffects(status, timers, selection, artGroup4, backPage, frontPage, 0, status.size(), layout);
}

std::vector<IndexedPageEffect> statusRepairPageEffects(const std::size_t system,
    const std::array<std::int8_t, 7>& status, const std::array<std::int16_t, 7>& timers,
    const std::vector<IndexedBitmap>& artGroup4, const std::uint16_t backPage, const std::uint16_t frontPage,
    const CockpitPageLayout layout) {
    return statusEffects(status, timers, -1, artGroup4, backPage, frontPage, system, system + 1, layout);
}

std::vector<IndexedPageEffect> playableSystemPageEffects(const PlayableSystemEffect& effect,
    const std::vector<IndexedBitmap>& artGroup4, const std::uint16_t backPage, const std::uint16_t frontPage,
    const CockpitPageLayout layout) {
    checkHudPages(backPage, frontPage, layout);
    std::vector<IndexedPageEffect> effects;
    if (std::holds_alternative<SystemAudio>(effect)) { return effects; }
    const auto system = std::visit([](const auto& item) -> std::size_t {
        using Item = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<Item, SystemAudio>) { return 0; }
        else if constexpr (std::is_same_v<Item,SystemDecoyStrip>
            || std::is_same_v<Item,SystemDecoyPalette>) {
            throw std::invalid_argument("decoy effects require their native ordered provider");
        } else { return item.system; }
    }, effect);
    if (system >= rectangles.size()) { throw std::invalid_argument("invalid playable system panel"); }
    const auto rectangle = rectangles[system];
    if (const auto* border = std::get_if<SystemBorder>(&effect)) {
        constexpr std::uint8_t selectedColor = 234; //DS12EA.
        constexpr std::uint8_t unselectedColor = 148; //DS12EB.
        const auto color = border->selected ? selectedColor : unselectedColor;
        appendHudFill(effects, {rectangle.x, rectangle.y, rectangle.width, 1}, color, backPage);
        appendHudFill(effects, {rectangle.x, static_cast<std::int16_t>(rectangle.y + rectangle.height - 1), rectangle.width, 1}, color, backPage);
        appendHudFill(effects, {rectangle.x, rectangle.y, 1, rectangle.height}, color, backPage);
        appendHudFill(effects, {static_cast<std::int16_t>(rectangle.x + rectangle.width - 1), rectangle.y, 1, rectangle.height}, color, backPage);
        return effects;
    }
    if (std::holds_alternative<SystemCopy>(effect)) {
        appendHudCopy(effects, rectangle, backPage, frontPage);
        return effects;
    }
    const auto& panel = std::get<SystemPanel>(effect);
    const auto style = static_cast<std::size_t>(panel.style);
    if (artGroup4.size() < 57 || style >= colors.size() || panel.remainingCharges > pipCount) {
        throw std::invalid_argument("invalid playable system panel resources or state");
    }
    appendHudFill(effects, {static_cast<std::int16_t>(rectangle.x + 2), static_cast<std::int16_t>(rectangle.y + 2),
        static_cast<std::int16_t>(rectangle.width - 4), static_cast<std::int16_t>(rectangle.height - 4)}, colors[style], backPage);
    appendHudBitmap(effects, artGroup4[(panel.remainingCharges == 0 && panel.style == SystemPanelStyle::unavailable ? 50 : 41) + system],
        icons[system], backPage);
    for (std::size_t pip = 0; pip < pipCount; ++pip) {
        appendHudBitmap(effects, artGroup4[panel.remainingCharges > pip ? 49 : 48], pips[system * pipCount + pip], backPage);
    }
    appendHudCopy(effects, rectangle, backPage, frontPage);
    return effects;
}

std::vector<IndexedPageEffect> scenePagePixelEffects(const ScenePageOperation& operation,
    const SceneResource& resource, const BitmapFont& scoreFont, const std::uint16_t backPage) {
    if (const auto* copy = std::get_if<RadarPageCopy>(&operation)) {
        return {*copy};
    }
    std::vector<IndexedPageEffect> effects;
    if (const auto* draw = std::get_if<SceneBitmapDraw>(&operation)) {
        const auto& bitmap = std::get<std::vector<IndexedBitmap>>(resource.assets.at(draw->asset)).at(draw->frame);
        if (draw->mirrored) {
            auto mirrored = bitmap;
            for (std::size_t row=0; row<bitmap.height; ++row) {
                std::reverse(mirrored.pixels.begin()+row*bitmap.width,mirrored.pixels.begin()+(row+1)*bitmap.width);
            }
            appendBitmap(effects,mirrored,draw->origin,PlanarBlitMode::generalTransparent,display,backPage);
        } else {
            appendBitmap(effects,bitmap,draw->origin,PlanarBlitMode::generalTransparent,display,backPage);
        }
        return effects;
    }
    if (const auto* background = std::get_if<SceneBackgroundDraw>(&operation)) {
        appendBitmap(effects, {320, 200, std::get<std::vector<std::uint8_t>>(
            resource.assets.at(background->asset))}, {0, 0}, PlanarBlitMode::opaque, display, backPage);
        return effects;
    }
    if (const auto* primitive = std::get_if<ScenePrimitiveDraw>(&operation)) {
        const auto r=primitive->arguments;
        if (primitive->kind==SceneCommandKind::drawPoint) {
            appendUiLine(effects,{r.x,r.y},{r.x,r.y},primitive->color,backPage);
        } else if (primitive->kind==SceneCommandKind::drawLine) {
            appendUiLine(effects,{r.x,r.y},{r.width,r.height},primitive->color,backPage);
        } else if (primitive->kind==SceneCommandKind::fillRectangle) {
            appendHudFill(effects,r,primitive->color,backPage);
        } else { throw std::invalid_argument("unknown scene primitive"); }
        return effects;
    }
    const auto* dialogue = std::get_if<SceneDialogueDraw>(&operation);
    if (dialogue == nullptr) {
        throw std::invalid_argument("scene non-pixel operation requires its runtime owner");
    }
    const auto rectangle = dialogue->rectangle;
    if (rectangle.x < 0 || rectangle.y < 0 || rectangle.width < 14 || rectangle.height < 24
        || rectangle.x + rectangle.width >= 320 || rectangle.y + rectangle.height >= 200
        || scoreFont.firstCharacter != 32 || scoreFont.height != 8 || scoreFont.glyphs.size() != 96) {
        throw std::invalid_argument("scene dialogue requires bounded geometry and inherited source font12CC");
    }
    constexpr std::array<std::uint8_t, 7> borderColors{4, 3, 2, 1, 2, 3, 4};
    for (std::int16_t inset = 0; inset < borderColors.size(); ++inset) {
        const auto left = static_cast<std::int16_t>(rectangle.x + inset);
        const auto right = static_cast<std::int16_t>(rectangle.x + rectangle.width - inset);
        const auto top = static_cast<std::int16_t>(rectangle.y + inset);
        const auto bottom = static_cast<std::int16_t>(rectangle.y + rectangle.height - inset);
        const auto color = borderColors[inset];
        appendUiLine(effects, {left, top}, {right, top}, color, backPage);
        appendUiLine(effects, {left, bottom}, {right, bottom}, color, backPage);
        appendUiLine(effects, {left, top}, {left, bottom}, color, backPage);
        appendUiLine(effects, {right, top}, {right, bottom}, color, backPage);
    }
    const auto x = static_cast<std::int16_t>(rectangle.x + 7);
    const auto y = static_cast<std::int16_t>(rectangle.y + 7);
    const auto width = static_cast<std::int16_t>(rectangle.width - 10);
    appendHudFill(effects, {x, y, static_cast<std::int16_t>(width - 3), 10}, dialogue->secondary, backPage);
    appendHudFill(effects, {x, static_cast<std::int16_t>(y + 10),
        static_cast<std::int16_t>(width - 3), static_cast<std::int16_t>(rectangle.height - 23)},
        dialogue->primary, backPage);
    const auto measure = [&scoreFont](const std::string& text) {
        auto result = 0;
        for (const unsigned char character : text) {
            if (character < scoreFont.firstCharacter
                || character - scoreFont.firstCharacter >= scoreFont.glyphs.size()) {
                throw std::invalid_argument("scene text outside inherited font range");
            }
            const auto& glyph = scoreFont.glyphs.at(character - scoreFont.firstCharacter);
            if (glyph.width == 0 || glyph.width > 22 || glyph.height != scoreFont.height
                || glyph.pixels.size() != static_cast<std::size_t>(glyph.width) * glyph.height) {
                throw std::invalid_argument("malformed scene font glyph");
            }
            result += glyph.width;
            if (result > std::numeric_limits<std::int16_t>::max()) {
                throw std::invalid_argument("scene text requires non-wrapped width");
            }
        }
        return result;
    };
    const auto titleWidth = measure(dialogue->text[0]);
    if (titleWidth >= width) {
        return effects;
    }
    for (std::size_t slot = 0; slot < dialogue->text.size(); ++slot) {
        const auto& text = dialogue->text[slot];
        const auto textWidth = measure(text);
        auto textX = slot == 0 ? x + width / 2 - titleWidth / 2 : x + 6;
        const auto textY = slot == 0 ? y + 2 : y + 3 + static_cast<int>(slot) * 7;
        if (textX + textWidth > std::numeric_limits<std::int16_t>::max()) {
            throw std::invalid_argument("scene text requires non-wrapped placement");
        }
        for (const unsigned char character : text) {
            const auto& glyph = scoreFont.glyphs.at(character - scoreFont.firstCharacter);
            for (std::uint16_t row = 0; row < glyph.height; ++row) {
                for (std::uint16_t column = 0; column < glyph.width; ++column) {
                    const auto ink = glyph.pixels[static_cast<std::size_t>(row) * glyph.width + column];
                    if (ink > 1) {
                        throw std::invalid_argument("scene glyph is not a binary mask");
                    }
                    if (ink != 0 && textX + column >= display.left && textX + column <= display.right
                        && textY + row >= display.top && textY + row <= display.bottom) {
                        effects.emplace_back(RadarPixelWrite{{static_cast<std::int16_t>(textX + column),
                            static_cast<std::int16_t>(textY + row)}, 0, backPage});
                    }
                }
            }
            textX += glyph.width;
        }
    }
    return effects;
}

}

namespace stellar7 {
void applyScenePageOperation(IndexedPages& pages, const ScenePageOperation& operation,
    const SceneResource& resource, const BitmapFont& font, const std::uint16_t backPage, SceneCaptureState& captures) {
    if (const auto* copy=std::get_if<ScenePlanarCopy>(&operation)) {
        pages.copyPlanar(copy->region);
        return;
    }
    if (const auto* capture=std::get_if<SceneCapture>(&operation)) {
        const auto r=capture->rectangle;
        IndexedBitmap bitmap{static_cast<std::uint16_t>(r.width),static_cast<std::uint16_t>(r.height),{}};
        for (int y=0;y<r.height;++y) {
            for (int x=0;x<r.width;++x) {
                bitmap.pixels.push_back(pages.read(backPage,{static_cast<std::int16_t>(r.x+x),static_cast<std::int16_t>(r.y+y)}));
            }
        }
        captures.banks.at(capture->bank)=std::move(bitmap);
        return;
    }
    if (const auto* restore=std::get_if<SceneRestoreCapture>(&operation)) {
        const auto& bitmap=captures.banks.at(restore->bank);
        if (!bitmap) { throw std::invalid_argument("scene pixel restore lacks its captured pixels"); }
        const auto r=restore->rectangle;
        if (bitmap->width!=r.width || bitmap->height!=r.height) { throw std::invalid_argument("scene capture shape changed"); }
        for (int y=0;y<r.height;++y) {
            for (int x=0;x<r.width;++x) {
                pages.write(backPage,{static_cast<std::int16_t>(r.x+x),static_cast<std::int16_t>(r.y+y)},bitmap->pixels[y*r.width+x]);
            }
        }
        return;
    }
    pages.apply(scenePagePixelEffects(operation,resource,font,backPage));
}
}
