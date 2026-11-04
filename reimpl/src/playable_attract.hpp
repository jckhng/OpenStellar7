#pragma once

#include "playable_attract_controller.hpp"
#include "briefing_preview.hpp"
#include "playable_story.hpp"
#include "stellar7/native_startup.hpp"
#include "stellar7/native_attract.hpp"
#include "story_audio_delivery.hpp"

namespace stellar7 {
struct PlayableAttractInput {
    NativeMenuInput menu;
    std::uint16_t sourceKey=0; //Actual03EE BIOS word; zero means no dequeued key.
    bool buttonHeld=false;
};
struct PlayableAttractOwners {
    IndexedPages& pixels;
    CockpitPageState& pages;
    PaletteWorkState& palette;
    SoundBlasterSynth& synth;
    PresentationClock& clock;
    SoundBlasterClockPhase& phase;
    const GameplayAssets& assets;
    const NativeStartupAssets& startup;
    const CockpitSettings& configuration;
    std::int16_t& creditsSkip; //Retained DS19CA, initialized by1578 once.
    PlayableModelDetail sharedDetail{};
};

class AttractPresentation {
public:
    AttractPresentation(PlayableAttractOwners owners,NativeSessionDelivery& delivery,std::size_t budget,
        const NativeDeliverySink& sink = {});
    void append(ClockedPresentationWork work);
    void append(NativeSessionDelivery work);
    void normal(const std::vector<NormalWorkOperation>& operations);
    void store(std::uint16_t raw);
    std::uint16_t read(NormalWorkKind kind=NormalWorkKind::playableAttractCountdownRead);
    bool wait();
    void publish();
    void upload(std::uint8_t intensity);
    void palette(const std::vector<Rgb>& colors);
    void cockpitPalette();
    void blackout();
    void fadeIn();
    void copy(std::uint16_t from,std::uint16_t to,ProjectedPoint point={0,0},std::uint16_t width=320,std::uint16_t height=200);
    void screen(const IndexedScreen& screen,std::uint16_t page);
    void titleScreen();
    void titleOverlays();
    PlayableAttractOwners o;
    std::size_t budget;
private:
    NativeSessionDelivery& delivery_;
    NativeDeliverySink sink_;
    StoryDeliveryStream stream_;
};

class PlayableAttractBriefing {
public:
    PlayableAttractBriefing(const GameplayAssets& assets,PlayableModelDetail sharedDetail);
    AttractBriefingAction input(const PlayableAttractInput& input,const CockpitSettings& configuration);
    void paint(AttractPresentation& presentation,std::size_t page);
    void control(AttractBriefingAction action);
    void cancelInteraction() noexcept {pressed_.reset();pointer_.reset();armed_=true;}
private:
    BriefingTextPages text_;
    BriefingViewer viewer_;
    std::optional<std::size_t> painted_;
    std::size_t selection_=1;
    std::optional<std::size_t> pressed_;
    std::optional<ProjectedPoint> pointer_;
    bool armed_=true;
    bool retainedEscape_=false;
};

class PlayableAttractIntro {
public:
    void advance(AttractPresentation& presentation,const PlayableAttractInput& input);
    bool complete() const noexcept {return stage_==Stage::complete;}
    bool acceptsKey() const noexcept;
private:
    enum class Stage { entry, logoDelay, logoFrames, logoEnd, logoRelease, logoHold,
        typewriter, bridge, earth, menuSong, menuHold, complete };
    void finishStory(AttractPresentation& presentation);
    void blink(AttractPresentation& presentation,std::size_t cycles);
    Stage stage_=Stage::entry;
    std::optional<StartupLogoWork> logo_;
    std::size_t segment_=0;
    bool logoWaitEntered_=false;
    bool logoOnce_=false;
    bool storyStarted_=false;
    bool introSkipped_=false;
    std::size_t character_=0;
    std::int16_t x_=40,y_=84;
    std::shared_ptr<PlayableStoryPresenter> story_;
};

enum class CreditsPresentationCaller { menuAttract, coldStartup };
class PlayableAttractCredits {
public:
    explicit PlayableAttractCredits(CreditsPresentationCaller caller=CreditsPresentationCaller::menuAttract):caller_(caller) {}
    void advance(AttractPresentation& presentation,const PlayableAttractInput& input);
    bool complete() const noexcept {return complete_;}
    bool acceptsKey() const noexcept {return entered_&&!complete_;}
private:
    CreditsPresentationCaller caller_;
    bool entered_=false;
    bool complete_=false;
};

//The runner owns only source-local control/presenters. All persistent owners are borrowed.
class PlayableAttractRunner {
public:
    PlayableAttractRunner(std::uint16_t capturedCountdown,bool voiceResourceAvailable);
    NativeSessionDelivery advance(PlayableAttractOwners owners,const PlayableAttractInput& input,std::size_t budget);
    bool returnedToMenu() const noexcept {return controller_.stage()==AttractStage::menu;}
    AttractStage stage() const noexcept {return controller_.stage();}
    std::size_t page() const noexcept {return controller_.page();}
    std::size_t tours() const noexcept {return controller_.tours();}
    NativeAttractProgress progress() const noexcept {
        return {static_cast<NativeAttractStage>(controller_.stage()),controller_.page(),controller_.tours()};
    }
    bool acceptsSourceKey() const noexcept;
    void cancelInteraction() noexcept {if(briefing_) {briefing_->cancelInteraction();}}
private:
    PlayableAttractController controller_;
    std::optional<PlayableAttractBriefing> briefing_;
    std::optional<PlayableAttractIntro> intro_;
    std::optional<PlayableAttractCredits> credits_;
    bool postCreditsEntered_=false;
    std::optional<bool> callerCampaign_;
};
}
