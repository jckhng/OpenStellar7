#pragma once

#include "stellar7/menu_widget_work.hpp"
#include "stellar7/palette_clock.hpp"
#include "stellar7/ui_panel_work.hpp"

namespace stellar7 {

enum class ContinueStage { inactive, polling, cancelCleanup, finished };
enum class ContinueOutcome { pending, yes, no, cancelled, unavailable };

struct ContinueState {
    ContinueStage stage = ContinueStage::inactive;
    std::vector<std::uint8_t> scratch;
    std::vector<std::uint8_t> transfer;
    ProjectedPoint pointer{};
    CockpitCopyRegion oldRectangle{};
    std::uint16_t backPage = 0;
    std::uint16_t frontPage = 0;
    std::uint16_t fadeTicks = 0;
    std::uint16_t ability = 0;
    std::uint16_t selected = 0;       //418A navigation index.
    std::uint16_t centered = 0;       //418C5C14 index.
    std::uint16_t pressed = 0;        //10EC one-based widget.
    std::uint16_t button = 0;         //51E4/522C held level.
    std::uint16_t keyboardScan = 0;   //12E4/518A native peek result.
    std::uint16_t activeWidget = 0;   //4C36 drawing state.
    std::uint16_t countdown = 0;      //1DC1 preserved by modal5E9C.
    bool navigationArmed = false;    //BC6.
    bool tabArmed = false;           //BC4; non-Tab64A0 sets it.
    bool spaceEnabled = true;        //BC2.
    bool mouseInstalled = false;     //3752; quiescent native provider only.
    bool joystickEnabled = false;    //1DA8; unsupported.
    bool overlayEnabled = false;     //12E7; unsupported.
    std::uint16_t widgetCallback = 0x5ac8;
};

struct ContinueKeyboardPacket {
    std::uint8_t peekScan = 0;
    bool enter = false;
    bool space = false;
    //8BD0 source scan order: End,Down,PgDn,Home,Up,PgUp,Left,Right.
    std::array<bool, 8> directions{};
};

enum class ContinueProviderKind { keyboardPeek, heldKey, mousePosition, mouseButton, setMousePosition, fontSelection };
struct ContinueProviderRead {
    ContinueProviderKind kind;
    std::uint16_t selector;
    std::uint16_t value;
    ProjectedPoint point{};
    std::size_t stepOffset;
    bool guestElapsedQualified = false;
};

struct ContinueWorkStep {
    std::vector<NormalWorkOperation> operations;
    std::vector<IndexedPageEffect> effects;
    std::optional<PaletteRequest> palette;
};

struct ContinueWork {
    ContinueState state;
    IndexedPages pages;
    std::vector<ContinueWorkStep> steps;
    std::vector<ContinueProviderRead> providers;
    ContinueOutcome outcome = ContinueOutcome::pending;
};

//0401 through entry8754; stops before8781. No budget stops before0428.
//Palette steps own loaded077A only; resident1B38 remains in ordinary work.
ContinueWork continueEntryOperations(const ContinueState& state, const IndexedPages& pages,
    const std::vector<IndexedBitmap>& art, const BitmapFont& titleFont, std::uint16_t remainingContinues);

//One supplied native packet:8781/87BF CALL5936 through the next poll or040A.
//Escape exposes cancelCleanup, requiring another real packet for source87BF.
//Finished Yes/No stops before040C retry or0428 cleanup; neither is performed here.
ContinueWork continuePollOperations(const ContinueState& state, const IndexedPages& pages,
    const std::vector<IndexedBitmap>& art, const BitmapFont& titleFont, const ContinueKeyboardPacket& packet);

//Target-local leaves preserve the existing public menu/bitmap domains.
UiBitmapWork continueVerticalBitmapOperations(const IndexedBitmap& bitmap, ProjectedPoint position,
    Viewport viewport, std::uint16_t page);
MenuWidgetWork continueWidgetOperations(const std::vector<IndexedBitmap>& art, const BitmapFont& titleFont,
    const std::vector<std::uint8_t>& scratch, std::size_t index, MenuWidgetState state, std::uint16_t backPage);

} // namespace stellar7
