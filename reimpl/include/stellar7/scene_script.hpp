#pragma once

#include "stellar7/resource_codec.hpp"
#include "stellar7/indexed_pages.hpp"

#include <array>
#include <string>
#include <optional>
#include <variant>

namespace stellar7 {

// Recovered command families reached by campaign Story and SCENED resources.
enum class SceneCommandKind : std::uint16_t {
    cacheScreen = 0x0020,
    releaseBitmapBank = 0x0080,
    endPage = 0x0ff0,
    setDelay = 0x1021,
    selectFrame = 0x1031,
    selectBitmapBank = 0x1051,
    selectPaletteBank = 0x1061,
    selectFontBank = 0x1071,
    metadata1111 = 0x1111,
    metadata1201 = 0x1201,
    selectCaptureBank = 0x1121,
    captureRectangle = 0x4214,
    drawPoint = 0xa002,
    drawLine = 0xa0a4,
    fillRectangle = 0xa114,
    drawMirroredBitmap = 0xa524,
    restoreCapture = 0xa601,
    setColors = 0x2002,
    sound = 0x2012,
    cacheRectangle = 0x4204,
    dialogue = 0xa304,
    drawBitmap = 0xa504,
    loadBackground = 0xf01f,
    loadBitmaps = 0xf02f,
    bindFont = 0xf04f,
    loadPalette = 0xf05f,
    text0 = 0xf10f,
    text1 = 0xf11f,
    text4 = 0xf14f,
    text5 = 0xf15f,
    text6 = 0xf16f,
    text2 = 0xf12f,
    text3 = 0xf13f,
};

struct SceneCommand {
    SceneCommandKind kind;
    std::array<std::uint16_t, 4> arguments{};
    std::string text;
    std::size_t sourceOffset = 0;
};

struct SceneProgram {
    std::vector<std::vector<SceneCommand>> pages;
};

using SceneEmbeddedAsset = std::variant<std::vector<Rgb>,
    std::vector<IndexedBitmap>, std::vector<std::uint8_t>>;

struct SceneResource {
    SceneProgram program;
    // File order matters: FxxF names are skipped; loaders consume this stream.
    std::vector<SceneEmbeddedAsset> assets;
};

SceneProgram decodeSceneProgram(const std::vector<std::uint8_t>& payload,
    std::uint16_t declaredPageCount);
SceneResource decodeSceneResource(const std::vector<std::uint8_t>& resource);

enum class ScenePaletteOperation { selectBank, load };
struct ScenePaletteChange {
    ScenePaletteOperation operation;
    std::optional<std::size_t> asset;
};

enum class SceneSoundOperation { loadVoice, playSequence };
struct SceneSoundRequest {
    SceneSoundOperation operation;
    std::uint16_t argument;
};

struct SceneBitmapDraw {
    std::size_t asset;
    std::size_t frame;
    ProjectedPoint origin;
    bool mirrored = false;
};

struct SceneBackgroundDraw {
    std::size_t asset;
};

struct SceneDialogueDraw {
    CockpitCopyRegion rectangle;
    std::array<std::string, 9> text;
    std::uint8_t primary;
    std::uint8_t secondary;
};

struct ScenePlanarCopy { RadarPageCopy region; };

struct ScenePrimitiveDraw {
    SceneCommandKind kind;
    CockpitCopyRegion arguments;
    std::uint8_t color;
};
struct SceneCapture { std::size_t bank; CockpitCopyRegion rectangle; };
struct SceneRestoreCapture { std::size_t bank; CockpitCopyRegion rectangle; };
struct SceneCaptureState { std::array<std::optional<IndexedBitmap>, 6> banks; };

using ScenePageOperation = std::variant<RadarPageCopy, ScenePaletteChange,
    SceneSoundRequest, SceneBitmapDraw, SceneBackgroundDraw, SceneDialogueDraw,
    ScenePrimitiveDraw, SceneCapture, SceneRestoreCapture, ScenePlanarCopy>;

struct ScenePageWork {
    bool present;
    std::vector<ScenePageOperation> operations;
    // 10A8's requested IRQ countdown when source0678 is zero; the input owner
    // may interrupt it. Not a frame duration or CPU/device proof.
    std::uint16_t delayIrqCounts;
};

// Retained 0252 command/page operation. Rendering, sound and IRQ waits are lowered
// by their existing runtime owners; producing work does not execute those owners.
class ScenePageInterpreter {
public:
    ScenePageInterpreter(const SceneResource& resource, RadarPages pages,
        std::uint8_t primary, std::uint8_t secondary, const SceneResource* sharedAssets = nullptr);
    ScenePageWork execute(std::size_t pageIndex);
    ScenePageWork executeComposite(const std::vector<std::size_t>& pages);

private:
    ScenePageWork executePage(std::size_t pageIndex, bool begin = true, bool present = true);
    void enqueue(CockpitCopyRegion rectangle);
    void appendCopy(ScenePageWork& work, CockpitCopyRegion rectangle,
        std::uint16_t source, std::uint16_t destination) const;
    void commitCache(ScenePageWork& work, CockpitCopyRegion rectangle);
    std::size_t consumeAsset(std::size_t variantIndex);
    const SceneResource* resource_;
    const SceneResource* assets_;
    RadarPages pages_;
    std::size_t assetCursor_ = 0;
    std::optional<std::size_t> palette_;
    std::optional<std::size_t> bitmaps_;
    std::array<std::optional<std::size_t>, 6> bitmapBanks_{};
    std::size_t bitmapBank_ = 0;
    std::size_t captureBank_ = 0;
    std::array<std::optional<CockpitCopyRegion>, 6> captures_{};
    std::optional<std::size_t> selectedFrame_;
    std::vector<CockpitCopyRegion> current_;
    std::vector<CockpitCopyRegion> previous_;
    std::array<std::string, 9> text_;
    std::uint8_t primary_;
    std::uint8_t secondary_;
    std::uint16_t delay_ = 0;
    bool fullDirty_ = false;
};

// Mode-X pixel leaves only. Palette, audio and waits belong to their owners.
std::vector<IndexedPageEffect> scenePagePixelEffects(const ScenePageOperation& operation,
    const SceneResource& resource, const BitmapFont& scoreFont, std::uint16_t backPage);

void applyScenePageOperation(IndexedPages& pages, const ScenePageOperation& operation,
    const SceneResource& resource, const BitmapFont& font, std::uint16_t backPage, SceneCaptureState& captures);

} // namespace stellar7
