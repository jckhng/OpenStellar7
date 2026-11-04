#pragma once
#include "stellar7/scene_script.hpp"
#include <memory>

namespace stellar7 {
enum class StoryScene { bridgeOpening, earthOpening, level2, level4, level7, draxon, defeat };
enum class StorySceneResult { playing, complete, skipped };
struct StorySceneAssets {
    SceneResource bridge;
    SceneResource earth;
    std::array<SceneResource,3> campaign;
    SceneResource draxon;
    std::array<std::vector<std::vector<std::uint8_t>>,4> voices;
    std::vector<std::uint8_t> backgroundSequence;
    std::optional<SceneResource> defeat;
};
StorySceneAssets decodeStorySceneAssets(const std::vector<std::uint8_t>& bridge,
    const std::vector<std::uint8_t>& earth, const std::array<std::vector<std::uint8_t>,3>& campaign,
    const std::vector<std::uint8_t>& draxon, const std::vector<std::uint8_t>& voice,
    const std::vector<std::uint8_t>& background = {}, const std::vector<std::uint8_t>& defeat = {});
const SceneResource& storySceneResource(const StorySceneAssets& assets, StoryScene scene);
const SceneResource& storySceneGraphics(const StorySceneAssets& assets, StoryScene scene);
std::optional<StoryScene> campaignStoryScene(std::size_t level);

struct StorySceneStep {
    ScenePageWork work{};
    std::uint16_t waitRawIrqs = 0;
    StorySceneResult result = StorySceneResult::playing;
    std::vector<std::size_t> pages;
};
// Source1110 composes eight ambient tracks before its main script page. It is
// distinct from sequential0252: one restore/swap and one presentation per update.
class StorySceneController {
public:
    StorySceneController(const StorySceneAssets& assets, StoryScene scene, RadarPages pages);
    StorySceneStep advance(std::uint16_t keyScan = 0);
    std::size_t mainPage() const noexcept { return mainPage_; }
    StorySceneResult result() const noexcept { return result_; }
    const SceneResource& graphics() const noexcept { return *graphics_; }
    bool composite() const noexcept { return composite_; }
private:
    ScenePageInterpreter interpreter_;
    const SceneResource* graphics_;
    bool composite_;
    bool entered_ = false;
    std::size_t mainPage_ = 88;
    std::array<std::size_t,8> tracks_{};
    std::int16_t repeats_ = 0;
    std::uint16_t delay_ = 0;
    StorySceneResult result_ = StorySceneResult::playing;
};
}
