#pragma once

#include "stellar7/indexed_pages.hpp"

#include <array>
#include <string_view>

namespace stellar7 {

enum class MenuWidgetState { released, pressed };

std::vector<IndexedPageEffect> dialogPanelPageEffects(const IndexedBitmap& tile,
    CockpitCopyRegion rectangle, std::uint16_t page);
std::vector<IndexedPageEffect> mainMenuPageEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& font, std::uint16_t page);
std::vector<IndexedPageEffect> mainMenuWidgetEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& font, std::size_t selection, MenuWidgetState state, std::uint16_t page);

struct HorizonColors {
    std::uint8_t sky;
    std::uint8_t ground;
};

//Compatibility: cacheA800/backA000 orA400. Recovered startup: cacheA400/backA800.
enum class CockpitPageLayout { compatibility, recoveredStartup };
enum class CockpitCacheDomain { initialLoader, retainedGameplay };

//4e06's163e resource draw replaces every indexed cache pixel, including zero.
std::vector<IndexedPageEffect> cockpitResourcePageEffects(const IndexedScreen& screen,
    std::uint16_t cachePage, CockpitPageLayout layout = CockpitPageLayout::compatibility);

std::vector<IndexedPageEffect> horizonPageEffects(const HorizonWorkState& state,
    const std::vector<IndexedBitmap>& frames, HorizonColors colors, std::uint16_t page);

std::vector<IndexedPageEffect> cockpitPageEffects(const CockpitWork& work,
    const CockpitWorkBitmaps& bitmaps, std::uint16_t cachePage, std::uint16_t drawPage);

//50c4 through its second6d46; radar and final back-to-front copy follow separately.
// Pixel effects only; does not execute initialization CPU work or advance an owner.
std::vector<IndexedPageEffect> cockpitCachePageEffects(const CockpitWork& matching,
    const CockpitWork& ordinary, const CockpitWorkBitmaps& bitmaps,
    const BitmapFont& font, std::string_view title, std::uint8_t titleColor,
    std::uint16_t cachePage, std::uint16_t backPage,
    CockpitPageLayout layout = CockpitPageLayout::compatibility,
    CockpitCacheDomain domain = CockpitCacheDomain::initialLoader);

//7970 zero-increment redraw, score font12cc and ordinary text style1 retained from7936.
// Source signed32 formatting; no score or page-owner mutation.
std::vector<IndexedPageEffect> scorePageEffects(std::uint32_t score, const BitmapFont& font,
    std::uint16_t backPage, std::uint16_t frontPage,
    CockpitPageLayout layout = CockpitPageLayout::compatibility);

//8f1c/8ed2/8f40, no selected-panel redraw (selection=-1), native Mode-X.
// Negative status is normalized locally; supplied retained state is unchanged.
std::vector<IndexedPageEffect> statusPageEffects(const std::array<std::int8_t, 7>& status,
    const std::array<std::int16_t, 7>& timers, std::int16_t selection,
    const std::vector<IndexedBitmap>& artGroup4, std::uint16_t backPage, std::uint16_t frontPage,
    CockpitPageLayout layout = CockpitPageLayout::compatibility);

std::vector<IndexedPageEffect> statusRepairPageEffects(std::size_t system,
    const std::array<std::int8_t, 7>& status, const std::array<std::int16_t, 7>& timers,
    const std::vector<IndexedBitmap>& artGroup4, std::uint16_t backPage, std::uint16_t frontPage,
    CockpitPageLayout layout = CockpitPageLayout::compatibility);

}
