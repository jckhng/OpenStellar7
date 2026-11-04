#pragma once

#include "stellar7/highscore.hpp"
#include "stellar7/indexed_render.hpp"

namespace stellar7 {

// 31d0 paints the retained back page once, then presents it without flipping.
// The caller owns page roles, palette restoration, and the temporary pointer.
std::vector<IndexedPageEffect> highscoreEntryPageEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, std::uint16_t backPage, std::uint16_t frontPage);

// Template 18's OK control. A state change repaints and copies only its extent.
std::vector<IndexedPageEffect> highscoreOkWidgetEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, MenuWidgetState state, std::uint16_t backPage,
    std::uint16_t frontPage);

// 33e6's accepted edit or caret toggle, followed by 3520's name-box copy.
std::vector<IndexedPageEffect> highscoreNameEditEffects(const BitmapFont& titleFont,
    const HighscoreNameEntry& entry, NameEntryEdit edit, std::uint16_t backPage,
    std::uint16_t frontPage);

// 5c14(0) centers the only widget's hit rectangle at (161,139).
// The caller owns the temporary back-page save/copy/restore.
std::vector<IndexedPageEffect> highscorePointerPageEffects(const IndexedBitmap& pointer,
    std::uint16_t backPage);

// 2b14 paints the resident table on back, then copies the whole page to front.
// The caller owns palette restoration, polling, and temporary pointer pixels.
std::vector<IndexedPageEffect> highscoreHallPageEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, const HighscoreTable& table, std::uint16_t backPage,
    std::uint16_t frontPage);

std::vector<IndexedPageEffect> highscoreHallWidgetEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, MenuWidgetState state, std::uint16_t backPage,
    std::uint16_t frontPage);

// 5c14(0) centers the hall widget hit box at (164,173).
std::vector<IndexedPageEffect> highscoreHallPointerPageEffects(const IndexedBitmap& pointer,
    std::uint16_t backPage);

}
