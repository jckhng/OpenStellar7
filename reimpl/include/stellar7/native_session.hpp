#pragma once
#include "stellar7/story_scene.hpp"

#include "stellar7/native_startup.hpp"
#include "stellar7/native_delivery.hpp"
#include "stellar7/indexed_render.hpp"
#include "stellar7/radar_state.hpp"
#include "stellar7/ending_scene.hpp"
#include "stellar7/frame_controller.hpp"
#include "stellar7/frame_copy_work.hpp"
#include "stellar7/failure_heap_work.hpp"
#include "stellar7/continue_work.hpp"
#include "stellar7/retry_work.hpp"
#include "stellar7/level1_periodic_spawn_work.hpp"
#include "stellar7/score_work.hpp"
#include "stellar7/success_session.hpp"
#include "stellar7/native_level2_transition.hpp"
#include "stellar7/native_level2_frame.hpp"
#include "stellar7/native_level2_retry.hpp"

namespace stellar7 {
class PlayableStoryPresenter;

struct Level1GameplayFrameWork;

enum class NativeSessionStart { level1, level2, level3, level5, level4, level6, level7 };
enum class NativePageProjection { cleanUnqualified, retainedNativeLevel1, retainedStationaryLevel2, retainedSteeringLevel2, retainedMovingLevel2, retainedLevel2FailureRestore, retainedLevel2Destruction, retainedLevel2DestructionClear, retainedLevel2Continue, retainedLevel2Success, retainedLevel5Loader, retainedLevel5, retainedHighscoreEntry, retainedHighscoreHall, retainedEndingEntry, retainedEndingPage, retainedMenu, retainedLevel1FailureRestore, retainedLevel1Destruction, retainedLevel1DestructionClear, retainedLevel1Continue, retainedLevel1Loader, retainedLevel1Success, retainedLevel2SuccessReturn, retainedLevel3Loader, retainedLevel3Preloop, retainedLevel3, retainedRecoveredLevel2Preloop, retainedRecoveredLevel2Frame, retainedPlayableLoader, retainedPlayablePreloop, retainedPlayableCampaign };
enum class NativePageRole { cache, back, front };

struct NativePageObservation {
    NativePageProjection projection;
    std::uint16_t cache, back, front;
    std::uint16_t copy, draw;
};

struct NativeLevel1LoaderRun {
    bool playableAudio = false;
    std::uint64_t firstSample = 0;
    std::vector<ClockedPresentationWork> work;
    std::optional<ClockedPaletteWork> blackPalette;
    std::vector<std::int16_t> samples;
};

struct NativeLevel1FrameRun {
    std::uint64_t firstSample = 0;
    std::uint16_t loopTick = 0;
    std::uint32_t prefixRngCalls = 0;
    std::uint32_t groundRngCalls = 0;
    std::vector<ClockedPresentationWork> work;
    std::optional<ClockedPageFlipWork> flip;
    std::vector<ClockedPaletteWork> palettes;
    std::vector<std::int16_t> samples;
    std::optional<Level1SpawnProviderCall> periodicProvider;
    std::vector<Level1SpawnProviderCall> deploymentProviders;
    std::vector<ScoreFontProviderRead> scoreFontProviders;
    bool prefixOnly = false;
    std::vector<Level1SelectorReleaseCall> selectorReleases;
};

struct NativeFailureFrameRun {
    std::uint64_t firstSample = 0;
    std::uint16_t sourceLoopTick = 0;
    std::uint16_t completedDisplays = 0;
    bool entered = false;
    bool finished = false;
    std::vector<ClockedPresentationWork> work;
    std::vector<ClockedPaletteWork> palettes;
    std::optional<ClockedPageFlipWork> flip;
    std::vector<FailureHeapProviderCall> providers;
    std::vector<std::int16_t> samples;
};

struct NativeContinueRun {
    std::uint64_t firstSample = 0;
    bool entered = false;
    ContinueState state;
    ContinueOutcome outcome = ContinueOutcome::pending;
    std::vector<ContinueProviderRead> providers;
    std::vector<ClockedPresentationWork> work;
    std::vector<ClockedPaletteWork> palettes;
    std::vector<std::int16_t> samples;
};

struct NativeRetryRun {
    std::uint64_t firstSample = 0;
    Level1RetryWork control;
    std::vector<ClockedPresentationWork> work;
    std::vector<RetryWorkSegment> providers;
    std::optional<ClockedPaletteWork> blackPalette;
    std::vector<std::int16_t> samples;
};

// Retains runtime owners for recovered work-clock and legacy sample-driven routes.
class NativeGameplaySession {
public:
    NativeGameplaySession(const GameplayAssets& assets, GameplayModel initialModel,
        NativeSessionStart start = NativeSessionStart::level1,
        std::unique_ptr<NativeAudioOwner> audio = {}, std::shared_ptr<HighscoreStore> highscoreStore = {});
    NativeGameplaySession(const GameplayAssets& assets, NativeStartupGameplayHandoff&& handoff,
        std::shared_ptr<HighscoreStore> highscoreStore = {});
    NativeGameplaySession(const NativeGameplaySession&) = delete;
    NativeGameplaySession& operator=(const NativeGameplaySession&) = delete;
    NativeGameplaySession(NativeGameplaySession&&) = delete;
    NativeGameplaySession& operator=(NativeGameplaySession&&) = delete;

    NativeSessionDelivery initialize();
    NativeSessionDelivery initializeRecoveredLevel1(std::size_t workPacketBudget = 10000);
    const NativeLevel1LoaderRun* recoveredLevel1Loader() const noexcept {
        return recoveredLoader_ ? &*recoveredLoader_ : nullptr;
    }
    NativeSessionDelivery advance(const GameplayInput& input, const NameEntryInput& text = {});
    NativeSessionDelivery advanceLevel1Frame();
    NativeSessionDelivery advanceRecoveredLevel1Frame(FrameCopyProfile copyProfile,
        std::size_t workPacketBudget = 10000, const GameplayInput& input = {});
    NativeSessionDelivery advanceRecoveredFailure(FrameCopyProfile copyProfile,
        std::size_t workPacketBudget = 10000);
    const NativeFailureFrameRun* recoveredFailureFrame() const noexcept {
        return recoveredFailure_ ? &*recoveredFailure_ : nullptr;
    }
    NativeSessionDelivery advanceRecoveredContinue(const ContinueKeyboardPacket& input = {},
        std::size_t workPacketBudget = 10000);
    const NativeContinueRun* recoveredContinue() const noexcept {
        return recoveredContinue_ ? &*recoveredContinue_ : nullptr;
    }
    NativeSessionDelivery resumeRecoveredLevel1Retry(std::size_t workPacketBudget = 10000);
    const NativeRetryRun* recoveredRetry() const noexcept {
        return recoveredRetry_ ? &*recoveredRetry_ : nullptr;
    }
    const NativeLevel1FrameRun* recoveredLevel1Frame() const noexcept {
        return recoveredFrame_ ? &*recoveredFrame_ : nullptr;
    }
    NativeSessionDelivery advanceRecoveredLevel2Transition(std::size_t workPacketBudget = 10000,
        const GameplayInput& input = {});
    const NativeLevel2TransitionRun* recoveredLevel2Transition() const noexcept {
        return recoveredLevel2Transition_ ? &*recoveredLevel2Transition_ : nullptr;
    }
    NativeSessionDelivery resumeRecoveredLevel2Retry(std::size_t workPacketBudget = 10000);
    const NativeLevel2RetryRun* recoveredLevel2Retry() const noexcept {
        return recoveredLevel2Retry_ ? &*recoveredLevel2Retry_ : nullptr;
    }
    NativeSessionDelivery advanceRecoveredLevel2Frame(FrameCopyProfile copyProfile,
        std::size_t workPacketBudget = 10000, const GameplayInput& input = {});
    NativeSessionDelivery advancePlayableCampaign(const GameplayInput& input = {},
        std::size_t workPacketBudget = 10000, const NameEntryInput& text = {},const NativeDeliverySink& sink = {});
    bool playableCampaign() const noexcept { return playableCampaign_; }
    NativeSessionDelivery pacePlayable(std::uint64_t minimumSampleEnd);
    const NativeLevel2FrameRun* recoveredLevel2Frame() const noexcept {
        return recoveredLevel2Frame_ ? &*recoveredLevel2Frame_ : nullptr;
    }
    NativeSessionDelivery advanceRecoveredSuccess(std::size_t workPacketBudget = 10000);
    NativeSessionDelivery resumeRecoveredLevel1Success(FrameCopyProfile copyProfile,
        std::size_t workPacketBudget = 10000);
    const NativeSuccessRun* recoveredSuccess() const noexcept {
        return recoveredSuccess_ ? &*recoveredSuccess_ : nullptr;
    }
    NativeSessionDelivery advanceMenu(NativeMenuInput input, std::size_t sampleBudget);
    const MenuModel* menu() const noexcept { return menu_ ? &*menu_ : nullptr; }
    const HighscoreNameEntry* highscoreNameEntry() const noexcept {
        return nameEntry_ ? &*nameEntry_ : nullptr;
    }
    const HighscoreTable* highscoreTable() const noexcept {
        return highscoreTable_ ? &*highscoreTable_ : nullptr;
    }
    const GameplayModel& model() const noexcept { return model_; }
    bool soundsEnabled() const;
    const SoundBlasterSynth& synth() const noexcept { return synth_; }
    const IndexedScreen& frame() const noexcept { return frame_; }
    std::optional<std::vector<std::uint8_t>> level2BackPagePixels() const;
    std::optional<std::vector<std::uint8_t>> level2CachePagePixels() const;
    std::optional<std::vector<std::uint8_t>> retainedPagePixels(NativePageRole role) const;
    std::uint64_t elapsedSamples() const noexcept { return clock_.elapsedSamples(); }
    const SoundBlasterClockPhase* recoveredClockPhase() const noexcept;
    std::optional<RadarActorOutput> radarObservation(std::size_t index) const {
        if (!radar_ || level1Pages_) {
            return std::nullopt;
        }
        const auto slot = model_.level2PresentationPoolSlot(index);
        return slot ? std::optional<RadarActorOutput>{radar_->projection(*slot)} : std::nullopt;
    }
    NativePageObservation pageObservation() const noexcept {
        return {pageProjection_, pages_.cachePage(), pages_.backPage(), pages_.frontPage(),
            pages_.copyPage(), pages_.drawPage()};
    }
    bool menuBackAvailable() const noexcept;
    std::optional<NativeAttractProgress> attractProgress() const noexcept;
    bool attractAcceptsSourceKey() const noexcept;
    bool joystickCalibrationActive() const noexcept;
    bool joystickCalibrationRejected() const noexcept;
    JoystickCalibrationPoint joystickCalibrationPoint() const noexcept;
    bool storyActive() const noexcept { return story_ || (storyDestination_ && storyRetry_); }
    std::optional<StoryScene> storyScene() const noexcept { return activeStory_; }
    std::size_t storyPage() const noexcept;

private:
    friend struct NativeSessionTestAccess;
    CockpitSettings renderSettings() const;
    void restorePlayablePreferencesPages();
    enum class NameEntryPoll { editing, accepted };
    enum class Level1Boundary { display, completedTail };
    bool playableCampaign_ = false;
    static std::unique_ptr<NativeAudioOwner> adoptAudio(const GameplayAssets& assets,
        NativeStartupGameplayHandoff& handoff);
    NativeSessionDelivery advanceImpl(const GameplayInput& input, const NameEntryInput& text,
        Level1Boundary boundary);
    NativeSessionDelivery executeRecoveredLevel1Work(std::unique_ptr<GameplayModel> staged,
        const Level1GameplayFrameWork& plan, FrameCopyProfile copyProfile,
        std::size_t workPacketBudget, const std::array<bool,9>& keys);
    void enter(NativeSessionDelivery& delivery);
    void refreshFrame();
    void finishCampaignLoad(NativeSessionStart level);
    NativeSessionDelivery advanceLevel3(const GameplayInput& input, const NameEntryInput& text);
    NativeSessionDelivery advancePlayableLifecycle(const GameplayInput& input, const NameEntryInput& text,
        std::size_t workPacketBudget);
    NativeSessionDelivery advancePlayableEnding(const GameplayInput& input,std::size_t workPacketBudget);
    NativeSessionDelivery advancePlayableTitle(const GameplayInput& input,std::size_t workPacketBudget);
    NativeSessionDelivery advancePlayableAttract(const GameplayInput& input,std::size_t workPacketBudget);
    NativeSessionDelivery advancePlayablePreferences(const GameplayInput& input,std::size_t workPacketBudget);
    bool livePreferences_=false;
    NativeSessionDelivery advanceJoystickCalibration(const GameplayInput& input,std::size_t workPacketBudget);
    std::optional<NativeSessionDelivery> pollMenuJoystickCalibration(const NativeMenuInput& requested,std::size_t budget);
    void adoptPlayableLevel1();
    bool schedulePlayableStory(std::size_t destination,bool retry);
    NativeSessionDelivery advancePlayableStory(const GameplayInput& input,std::size_t workPacketBudget,const NativeDeliverySink& sink = {});
    NativeSessionDelivery reloadPlayableRetry(std::size_t workPacketBudget);
    void validatePlayableBombEvent() const;
    void waitPlayableIrqs(NativeSessionDelivery& delivery,SoundBlasterClockPhase& phase,
        std::uint16_t rawCount,std::size_t workPacketBudget);
    void enterEnding(const GameplayInput& input, const NameEntryInput& text);
    void presentEndingFirstPage(const GameplayInput& input, const NameEntryInput& text);
    std::vector<std::int16_t> presentEndingPage(const GameplayInput& input, const NameEntryInput& text);
    void finishEnding(NativeSessionDelivery& delivery, const GameplayInput& input, const NameEntryInput& text);
    void returnToMenu(NativeSessionDelivery& delivery, const GameplayInput& input, const NameEntryInput& text);
    void renderMenuWidget(std::size_t selection, MenuWidgetState state);
    void renderMenuCursor();
    void publishMenuPalette(const PaletteWorkState& state);
    void beginMenuGame(NativeSessionDelivery& delivery);
    bool supportsRetainedPages() const noexcept;
    bool supportsLevel1Pages() const noexcept;
    bool supportsLevel1Pages(const GameplayModel& model) const noexcept;
    bool supportsFailurePages() const noexcept;
    bool supportsContinuePages() const noexcept;
    bool supportsNameEntryPages() const noexcept;
    bool supportsHallPages() const noexcept;
    bool supportsSuccessPages() const noexcept;
    bool supportsLevel2Retry(const GameplayInput& input) const noexcept;
    void initializePages(NativeSessionStart level);
    void renderLevel1Pages();
    void renderLevel1Radar();
    bool renderLevel2Pages(std::vector<NormalWorkOperation>* cockpitWork = nullptr);
    void renderWeaponLamp();
    void finishLevel2FailureDisplay();
    void finishLevel1FailureDisplay();
    void renderFailurePages();
    void renderContinuePages();
    void renderNameEntryPages(NameEntryPoll poll = NameEntryPoll::editing);
    void renderHallPages();
    void renderHighscorePointer(ProjectedPoint origin, const std::vector<IndexedPageEffect>& effects);
    void renderSuccessPages();
    std::vector<RadarStateActor> level2RadarActors() const;
    void setSourcePaletteIntensity(std::uint8_t intensity);
    std::optional<std::vector<std::uint8_t>> level2PagePixels(std::uint16_t page) const;
    void dispatch(const std::vector<Level1AudioEvent>& events, std::size_t& cursor,
        NativeSessionDelivery& delivery);
    const GameplayAssets& assets_;
    GameplayModel model_;
    std::unique_ptr<NativeAudioOwner> audio_;
    std::optional<PaletteWorkState> recoveredStartupPalette_;
    std::optional<NativeLevel1LoaderRun> recoveredLoader_;
    std::optional<NativeLevel1FrameRun> recoveredFrame_;
    std::optional<NativeFailureFrameRun> recoveredFailure_;
    std::optional<NativeContinueRun> recoveredContinue_;
    std::optional<NativeRetryRun> recoveredRetry_;
    std::optional<NativeLevel2TransitionRun> recoveredLevel2Transition_;
    std::optional<NativeLevel2FrameRun> recoveredLevel2Frame_;
    std::optional<NativeLevel2RetryRun> recoveredLevel2Retry_;
    std::optional<NativeSuccessState> recoveredSuccessState_;
    std::optional<NativeSuccessRun> recoveredSuccess_;
    std::optional<std::uint16_t> recoveredIntroduction_;
    //Only modal metadata persists here; scratch/transfer remain resources-owned.
    std::optional<ContinueState> recoveredContinueState_;
    std::uint64_t failureHeapGeneration_ = 0;
    std::unique_ptr<FrameController> recoveredController_;
    SoundBlasterSynth& synth_;
    PresentationClock& clock_;
    IndexedScreen frame_;
    CockpitPageState pages_;
    std::optional<IndexedPages> pixels_;
    std::optional<RadarState> radar_;
    CockpitWorkBitmaps cockpitBitmaps_;
    NativePageProjection pageProjection_ = NativePageProjection::cleanUnqualified;
    CockpitIndicatorState incomingIndicator_{};
    std::size_t initialActorCount_ = 0;
    static constexpr std::size_t initialCampaignActorCapacity = 8;
    std::array<std::size_t, initialCampaignActorCapacity> initialPoolSlots_{};
    std::array<std::int16_t, initialCampaignActorCapacity> initialActorTypes_{};
    static constexpr std::size_t ordinaryPoolSize_ = 16;
    std::array<std::uint64_t, ordinaryPoolSize_> actorIncarnations_{};
    std::array<std::int16_t, ordinaryPoolSize_> actorTypes_{};
    bool level1Pages_ = false;
    bool failurePages_ = false;
    bool continuePages_ = false;
    std::optional<HighscoreTable>& highscoreTable_;
    std::shared_ptr<HighscoreStore> highscoreStore_;
    std::optional<HighscoreNameEntry> nameEntry_;
    NameCaretState nameCaret_ = initialNameCaretState;
    NameEntryInput pendingNameInput_;
    bool nameEntryPages_ = false;
    bool nameOkPressed_ = false;
    bool incomingNameOkPressed_ = false;
    bool hallPages_ = false;
    bool hallContinuePressed_ = false;
    bool incomingHallContinuePressed_ = false;
    std::optional<SuccessPaletteState> successPalette_;
    struct CampaignResourceBorrow {
        std::size_t level;
        const std::vector<std::vector<IndexedBitmap>>* bitmaps;
        const std::vector<Model3d>* models;
        const std::vector<std::uint8_t>* music;
    };
    std::optional<CampaignResourceBorrow> campaignResources_;
    std::shared_ptr<PlayableStoryPresenter> story_;
    std::optional<StoryScene> activeStory_;
    std::optional<std::size_t> storyDestination_;
    bool storyRetry_ = false;
    bool gameOverScenePlayed_ = false;
    std::shared_ptr<const EndingSceneAssets> endingAssets_;
    std::optional<ScenePageInterpreter> ending_;
    std::optional<PaletteWorkState> endingPalette_;
    PaletteRequest endingPaletteRequest_{0, vgaPaletteColors, 0, 0};
    std::int16_t sceneVoiceSegment_ = initialSceneVoiceSegment;
    NativeIrqWait endingWait_{0};
    std::optional<MenuModel> menu_;
    std::optional<std::size_t> menuPressed_;
    std::optional<ProjectedPoint> menuCursor_;
    std::optional<ProjectedPoint> modalPointer_;
    bool menuArmed_ = true;
    std::shared_ptr<PlayableMenuDialog> menuDialog_;
    ContinueChoice continuePointerChoice_ = ContinueChoice::no;
    std::optional<ContinueChoice> continuePressedChoice_;
    Viewport failureBackgroundClip_{0, 319, 0, 199};
    bool pageLampVisible_ = false;
    bool pageLampReady_ = false;
    std::uint32_t pageScore_ = 0;
    std::array<std::int8_t, 7> pageStatus_{};
    IndexedScreen frozenSuccess_;
    bool hasFrozenSuccess_ = false;
    bool initialized_ = false;
    bool completeLevel1Frames_ = false;
    std::size_t level1Cursor_ = 0;
    std::size_t level2Cursor_ = 0;
    std::size_t failureCursor_ = 0;
};

}  // namespace stellar7
