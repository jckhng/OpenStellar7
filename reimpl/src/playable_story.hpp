#pragma once
#include "stellar7/story_scene.hpp"
#include "stellar7/native_session.hpp"

namespace stellar7 {
enum class PlayableStoryCaller { normal, menuAttract };
struct PlayableStoryOwners {
    IndexedPages& pixels;
    CockpitPageState& pages;
    PaletteWorkState& palette;
    SoundBlasterSynth& synth;
    PresentationClock& clock;
    SoundBlasterClockPhase& phase;
    const BitmapFont& font;
    const std::vector<std::uint8_t>& cockpitPalette;
    bool musicEnabled;
    bool soundsEnabled;
    PlayableStoryCaller caller=PlayableStoryCaller::normal;
};
struct PlayableStoryInput { std::uint16_t keyScan = 0; bool buttonHeld = false; };
// Scene-owned metadata/captures only. The actual pages, palette, synth and clock
// are borrowed from the startup or gameplay caller on every delivery.
class PlayableStoryPresenter {
public:
    PlayableStoryPresenter(const StorySceneAssets& assets,StoryScene scene,RadarPages pages);
    NativeSessionDelivery advance(PlayableStoryOwners owners,PlayableStoryInput input,std::size_t workPacketBudget,
        const NativeDeliverySink& sink = {});
    StorySceneResult result() const noexcept {return result_;}
    std::size_t page() const noexcept {return controller_.mainPage();}
private:
    const StorySceneAssets* assets_;
    StorySceneController controller_;
    SceneCaptureState captures_;
    std::optional<StorySceneStep> step_;
    std::size_t operation_ = 0;
    std::optional<std::size_t> voiceGroup_;
    std::optional<std::size_t> loadedSegment_;
    std::int16_t currentSegment_ = initialSceneVoiceSegment;
    std::optional<std::vector<Rgb>> palette_;
    StorySceneResult pendingResult_ = StorySceneResult::playing;
    StorySceneResult result_ = StorySceneResult::playing;
    bool entered_ = false;
    bool buttonHeld_ = true;
    bool voiceAdvance_ = false;
    bool backgroundStarted_ = false;
};
}
