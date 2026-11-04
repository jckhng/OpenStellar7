#pragma once

#include "stellar7/indexed_render.hpp"

#include <array>
#include <optional>
#include <string>

namespace stellar7 {

constexpr std::size_t briefingPageCount = 19;
using BriefingTextPages = std::array<std::vector<std::string>, briefingPageCount>;

// Decoded COCKPIT.RES TXT:0; 2D7E/2E60/2ED0. No specimen text is embedded.
BriefingTextPages parseBriefingText(const std::vector<std::uint8_t>& decoded);

struct BriefingPreview {
    std::uint8_t model;
    std::string_view name;
};

std::optional<BriefingPreview> briefingPreview(std::size_t page);

// Pixel-only 2790 background. No front copy, clock work or model draw.
std::vector<IndexedPageEffect> briefingShellEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, std::uint16_t backPage);

// Pixel-only 2ED0 body: uses source12CC proportional font, raw case and color128.
std::vector<IndexedPageEffect> briefingTextEffects(const BriefingTextPages& pages,
    std::size_t page, const BitmapFont& scoreFont, std::uint16_t backPage);

//21E6's title/name and immutable per-model statistics; no generated descriptions.
std::vector<IndexedPageEffect> briefingDetailsEffects(std::size_t page,
    const BitmapFont& titleFont, const BitmapFont& scoreFont, std::uint16_t backPage);

// 25F4's six records in order: EXIT,CONTINUE,then four arrow controls.
std::vector<IndexedPageEffect> briefingWidgetEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, std::size_t widget, MenuWidgetState state,
    std::uint16_t backPage);

} // namespace stellar7
