#pragma once

#include "briefing_pages.hpp"
#include "stellar7/gameplay.hpp"

namespace stellar7 {

enum class BriefingViewerControl { stop, faster, zoomIn, zoomOut };

// Owns only1D9E's source-local actors/counters; never borrows gameplay actors/RNG.
class BriefingViewer {
public:
    explicit BriefingViewer(const GameplayAssets& assets, PlayableModelDetail sharedDetail = {});
    void select(std::size_t textPage, const GameplayAssets& assets);
    // Returns true only on20C4's strip-index branch; caller can redraw source strip.
    bool control(BriefingViewerControl control);
    std::int16_t stripIndex() const noexcept { return stripIndex_; }
    // One source1F0E..1F86 foreground delivery. No samples/cycles are manufactured.
    // Caller restores source cache preview50,35,246,75 before applying these pixels.
    std::vector<IndexedPageEffect> poll(const GameplayAssets& assets, std::uint16_t backPage);

private:
    PlayableModelDetail sharedDetail_{};
    std::optional<BriefingPreview> preview_;
    Level1Entity actor_{};
    Level1Entity reference_{};
    std::vector<std::uint8_t> referenceSelectors_;
    std::uint8_t referenceWait_ = 0;
    std::uint16_t obstaclePhase_ = 0;
    std::int16_t stripIndex_ = 0;
    std::int32_t baseline_ = 2000;
    std::int16_t zoomStep_ = 100;
};

} // namespace stellar7
