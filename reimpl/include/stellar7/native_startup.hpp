#pragma once

#include "stellar7/presentation_clock.hpp"
#include "stellar7/indexed_pages.hpp"
#include "stellar7/indexed_render.hpp"
#include "stellar7/menu_model.hpp"
#include "stellar7/startup_context_work.hpp"
#include "stellar7/startup_settings_work.hpp"
#include "stellar7/startup_resources_work.hpp"
#include "stellar7/startup_presentation_work.hpp"
#include "stellar7/driver_sequence_work.hpp"
#include "stellar7/startup_fonts_work.hpp"
#include "stellar7/startup_logo_work.hpp"
#include "stellar7/startup_intro_work.hpp"
#include "stellar7/credits_work.hpp"
#include "stellar7/menu_drawing_work.hpp"
#include "stellar7/native_delivery.hpp"
#include "stellar7/campaign_loader.hpp"
#include "stellar7/native_pointer.hpp"
#include "stellar7/native_joystick.hpp"
#include "stellar7/native_attract.hpp"

#include "stellar7/native_menu_features.hpp"
#include <memory>
#include <optional>

namespace stellar7 {
class PlayableStoryPresenter;
class PlayableJoystick;
class PlayableJoystickCalibration;
class NativeJoystickCalibrationRun;
class PlayableAttractRunner;
class PlayableAttractCredits;
enum class StoryScene;
struct NativeMenuInput;
struct GameplayInput;

struct NativeStartupAssets {
    IndexedScreen logoScreen;
    IndexedScreen menuScreen;
    std::vector<IndexedBitmap> logoFrames;
    std::vector<Rgb> reentryPalette;
};
NativeStartupAssets decodeNativeStartupAssets(const std::vector<std::uint8_t>& artResource);

struct NativeStartupContextRun {
    StartupContextWork context;
    ClockedPresentationWork installer;
    ClockedPresentationWork work;
};

struct NativeStartupSettingsRun {
    StartupSettingsWork settings;
    ClockedPresentationWork work;
};

struct NativeStartupResourcesRun {
    StartupResourcesWork resources;
    ClockedPresentationWork work;
    std::vector<std::uint8_t> transferBuffer;
    std::vector<std::uint8_t> scratchBuffer;
};

struct NativeStartupSongHandle {
    std::vector<std::uint8_t> payload;
    std::optional<std::size_t> next;
    std::uint16_t repeat = 0;
};

struct NativeStartupFontsRun {
    StartupFontsWork fonts;
    std::vector<ClockedPresentationWork> work;
    BitmapFont firstFont;
    BitmapFont secondFont;
    std::uint32_t memoryProbeBytes = 0;
    bool probeReleased = false;
    std::uint64_t firstSample = 0;
    std::vector<std::int16_t> samples;
};

enum class StartupMusicProfile { sourceSelected, playableUnpublished };

struct NativeStartupPresentationRun {
    StartupMusicProfile audioProfile = StartupMusicProfile::sourceSelected;
    StartupPresentationWork presentation;
    DriverSequenceWork directReset;
    DriverSequenceWork sequenceInitialization;
    SoundBlasterProgramState program;
    std::vector<NativeStartupSongHandle> songs;
    std::optional<std::size_t> loadedHead;
    std::optional<std::size_t> selectedHead;
    std::vector<ClockedPresentationWork> work;
    std::vector<ClockedPaletteWork> palettes;
    std::vector<StartupPresentationProvider> providers;
    PaletteWorkState palette;
    StartupPresentationPage drawPage = StartupPresentationPage::back;
    SoundBlasterClockPhase finalPhase;
    std::uint64_t firstSample = 0;
    std::vector<std::int16_t> samples;
};

struct NativeStartupLogoFrame {
    std::int16_t index;
    std::uint64_t sampleEnd;
    SoundBlasterClockPhase phase;
    std::array<std::vector<std::uint8_t>, 3> pages; //Front, back, cache physical storage.
};

struct NativeStartupLogoRun {
    BitmapCache cache;
    StartupLogoWork logo;
    std::vector<ClockedPresentationWork> work;
    std::vector<NativeStartupLogoFrame> frames;
    SoundBlasterClockPhase finalPhase;
    std::uint64_t firstSample = 0;
    std::vector<std::int16_t> samples;
};

struct NativeStartupLogoEndRun {
    std::vector<ClockedPresentationWork> work;
    SoundBlasterClockPhase finalPhase;
    std::uint64_t firstSample = 0;
    std::uint64_t guestPolls = 0;
    std::vector<std::int16_t> samples;
};

struct NativeStartupLogoReleaseRun {
    std::vector<ClockedPresentationWork> work;
    SoundBlasterClockPhase finalPhase;
    std::uint64_t firstSample = 0;
    std::vector<std::int16_t> samples;
    bool metadataReleased = false;
};

enum class NativeIntroPresentation { earlyEscape, storySkipped, storyPlayed };

struct NativeStartupIntroRun {
    NativeIntroPresentation mode = NativeIntroPresentation::earlyEscape;
    StartupIntroSkipWork input;
    ClockedPresentationWork query;
    ClockedBlackoutWork blackout;
    std::vector<ClockedPresentationWork> paletteSelection;
    PaletteWorkState palette;
    SoundBlasterClockPhase finalPhase;
    std::uint64_t firstSample = 0;
    std::vector<std::int16_t> samples;
};

struct NativeStartupMenuSongRun {
    StartupMusicProfile audioProfile = StartupMusicProfile::sourceSelected;
    NativeStartupSongHandle song;
    DriverSequenceWork activation;
    std::vector<ClockedPresentationWork> work;
    SoundBlasterDriverState before, after;
    SoundBlasterClockPhase finalPhase;
    std::uint64_t firstSample = 0;
    std::vector<std::int16_t> samples;
};

struct NativeStartupMenuResourcesRun {
    StartupCapabilityProfile pointerCapability = StartupCapabilityProfile::noMouseOrJoystick;
    BitmapCache cache;
    std::vector<ClockedPresentationWork> before;
    ClockedBlackoutWork blackout;
    std::vector<ClockedPresentationWork> after;
    std::array<std::size_t, 3> bitmapCounts{};
    std::array<std::uint16_t, 2> randomSeed{};
    CockpitSettings configuration{};
    PaletteWorkState palette;
    SoundBlasterDriverState driverBefore, driverAfter;
    SoundBlasterClockPhase finalPhase;
    std::uint64_t firstSample = 0;
    std::vector<std::int16_t> samples;
    bool streamsClosed = false;
};

struct NativeStartupCreditsRun {
    CreditsDrawingWork drawing;
    std::vector<ClockedPresentationWork> work;
    std::vector<PaletteWorkState> paletteUploads;
    std::vector<PaletteRequest> paletteRequests;
    std::vector<std::uint8_t> displayedPage;
    PaletteWorkState palette;
    UiPanelSurface surface;
    SoundBlasterDriverState driverBefore, driverAfter;
    SoundBlasterClockPhase finalPhase;
    std::uint16_t sourceKey = 0;
    std::uint16_t countdownRead = 0;
    std::uint16_t drawPage = 0;
    std::uint16_t copyPage = 0;
    std::uint8_t selectedFont = 1;
    std::uint8_t textColor = 0;
    std::uint64_t firstSample = 0;
    std::vector<std::int16_t> samples;
};

enum class NativeMenuEntryProfile { keyboardOnly };

struct NativeStartupMenuEntryRun {
    MenuDrawingWork drawing;
    std::vector<ClockedPresentationWork> work;
    std::vector<PaletteWorkState> paletteUploads;
    PaletteWorkState palette;
    SoundBlasterDriverState driverBefore, driverAfter;
    SoundBlasterClockPhase finalPhase;
    std::vector<std::uint8_t> scratch;
    std::vector<std::uint8_t> transfer;
    ProjectedPoint pointer{163,69};
    CockpitCopyRegion oldRectangle{163,69,16,21};
    std::uint64_t firstSample = 0;
    std::vector<std::int16_t> samples;
};

enum class NativeMenuBeginProfile { enterPressRelease, portablePointer, playableWidget };

struct NativeStartupBeginPoll {
    bool pressed = false;
    std::uint16_t activeWidget = 0;
    std::uint16_t activation = 0;
    std::uint16_t result = 0;
    std::uint64_t sampleEnd = 0;
    std::vector<std::uint8_t> front, back, transfer;
};

struct NativeStartupBeginRun {
    NativeMenuBeginProfile inputProfile = NativeMenuBeginProfile::enterPressRelease;
    bool playableAudio = false;
    std::vector<ClockedPresentationWork> work;
    std::vector<PaletteWorkState> paletteUploads;
    std::array<NativeStartupBeginPoll,2> polls;
    PaletteWorkState palette;
    SoundBlasterDriverState driverBefore, driverAfter;
    SoundBlasterClockPhase finalPhase;
    std::uint64_t guestPolls = 0;
    std::array<std::int8_t,7> difficulty{3,3,3,3,3,3,3};
    std::uint64_t firstSample = 0;
    std::vector<std::int16_t> samples;
};

struct NativeStartupMenuInputRun {
    std::uint64_t firstSample = 0;
    std::vector<std::int16_t> samples;
    std::vector<ClockedPresentationWork> work;
    NativeStartupBeginPoll poll;
    bool activated = false;
};

// Stable storage: the clock's synth reference survives transfer into gameplay.
class NativeAudioOwner {
public:
    explicit NativeAudioOwner(const GameplayAssets& assets,
        SoundBlasterInitialization initialization = SoundBlasterInitialization::compatibility);
    ~NativeAudioOwner();
    NativeAudioOwner(const NativeAudioOwner&) = delete;
    NativeAudioOwner& operator=(const NativeAudioOwner&) = delete;
    const SoundBlasterSynth& synth() const noexcept { return synth_; }
    std::uint64_t elapsedSamples() const noexcept { return clock_.elapsedSamples(); }

private:
    friend class NativeStartupSequence;
    friend class NativeGameplaySession;
    friend class NativeStartupGameplayHandoff;
    std::optional<SoundBlasterBomb7CallerRow> bomb7CallerRow() const;
    bool playableSoundsEnabled() const;
    bool playableMusicEnabled() const;
    PlayableMusicPublication playableMusicPublication() const;
    void synchronizePlayableMusic(SoundBlasterClockPhase& phase);
    bool playableSoundsProfile() const;
    bool playableMusicProfile() const;
    CampaignDifficulty playableDifficulty() const;
    bool playableDifficultyProfile() const;
    std::int16_t playableDetail() const;
    bool playableDetailProfile() const noexcept;
    bool playablePointerEnabled() const;
    bool playablePointerProfile() const;
    bool playableStoryEnabled() const;
    bool playableStoryProfile() const;
    bool playableJoystickProfile() const;
    bool takeJoystickCancellation() noexcept;
    void acceptJoystickSample(const std::optional<NativeJoystickSample>& sample);
    StartupSettingsWork joystickSettings(const CockpitSettings& configuration,const HighscoreTable& table,StartupCapabilityProfile profile);
    NativeMenuInput joystickMenuInput(NativeMenuInput input) const;
    void joystickGameplayInput(GameplayInput& input);
    bool joystickButtonHeld() const;
    std::shared_ptr<NativeJoystickCalibrationRun> joystickCalibrationRun_;
    std::unique_ptr<PlayableJoystick> joystick_;
    NativeJoystickSample joystickSample_{};
    bool pendingJoystickCancel_=false;
    std::shared_ptr<const NativeStartupAssets> startupAssets_;
    std::shared_ptr<PlayableAttractRunner> attract_;
    std::int16_t attractCreditsSkip_=0;
    bool attractPagesTransferred_=false;
    SoundBlasterSynth synth_;
    PresentationClock clock_{synth_};
    std::optional<NativeStartupContextRun> context_;
    std::optional<NativeStartupSettingsRun> settings_;
    std::optional<NativeStartupResourcesRun> resources_;
    std::optional<NativeStartupPresentationRun> presentation_;
    std::optional<NativeStartupFontsRun> fonts_;
    std::optional<NativeStartupLogoRun> logo_;
    std::optional<NativeStartupLogoEndRun> logoEnd_;
    std::optional<NativeStartupLogoReleaseRun> logoRelease_;
    std::optional<NativeStartupIntroRun> intro_;
    std::optional<NativeStartupMenuSongRun> menuSong_;
    std::optional<NativeStartupMenuResourcesRun> menuResources_;
    std::optional<NativeStartupCreditsRun> credits_;
    std::optional<NativeStartupMenuEntryRun> menuEntry_;
    std::optional<NativeStartupBeginRun> begin_;
    std::optional<NativeStartupMenuInputRun> menuInput_;
    std::optional<SoundBlasterClockPhase> workPhase_;
    std::optional<HighscoreTable> currentScores_;
    bool menuPreferencesApplied_ = false;
    bool playablePreferences_ = false;
    NativeMenuFeatures menuFeatures_ = NativeMenuFeatures::legacy;
    NativePointerCapability pointerCapability_ = NativePointerCapability::unavailable;
    StartupMusicProfile startupMusicProfile_ = StartupMusicProfile::sourceSelected;
    bool ready_ = false;
    bool adopted_ = false;
};

// Moves the completed0180 owners. It does not execute the Level1 loader or frame.
class NativeStartupGameplayHandoff {
public:
    NativeStartupGameplayHandoff(const NativeStartupGameplayHandoff&) = delete;
    NativeStartupGameplayHandoff& operator=(const NativeStartupGameplayHandoff&) = delete;
    NativeStartupGameplayHandoff(NativeStartupGameplayHandoff&&) noexcept = default;
    NativeStartupGameplayHandoff& operator=(NativeStartupGameplayHandoff&&) noexcept = default;
    const SoundBlasterSynth& synth() const;
    const SoundBlasterClockPhase& clockPhase() const;
    std::uint64_t elapsedSamples() const;

private:
    friend class NativeStartupSequence;
    friend class NativeGameplaySession;
    NativeStartupGameplayHandoff(const GameplayAssets& assets, std::unique_ptr<NativeAudioOwner> audio,
        IndexedPages pages, CockpitPageState roles, PaletteWorkState palette);
    const GameplayAssets* assets_;
    std::unique_ptr<NativeAudioOwner> audio_;
    IndexedPages pages_;
    CockpitPageState roles_;
    PaletteWorkState palette_;
};

enum class NativeStartupPhase { idle, logoDelay, logoFrames, logoEnd, menu, beginWait, ready, adopted, rejected, awaitingSettings, awaitingResourceInitialization, awaitingPresentationResources, awaitingFontResources, logoCountdownWork, awaitingLogoFrames, awaitingLogoAnimation, awaitingLogoSoundEnd, awaitingLogoRelease, awaitingIntroInput, awaitingMenuMusic, awaitingMenuResources, awaitingMenuConstruction, awaitingMenuEntry, awaitingMenuDecision, awaitingGameplayLoader };
enum class NativeStartupExecution { sampleCompatibility, recoveredContext, playableMusicOff, playableContext };
enum class NativeStartupDeviceProfile { portableOplNoDspProbe };
enum class StartupLogoEventProfile { sourceSound, playableSoundsDisabled };
enum class NativeMenuAction { none, begin, exit, unsupported };
enum class NativeMenuPage { cache, back, front };

struct NativeMenuInput {
    bool up = false;
    bool down = false;
    bool confirm = false;
    bool left = false;
    bool right = false;
    bool cancel = false;
    std::optional<NativePointerInput> pointer;
    std::optional<NativeJoystickSample> joystick;
    std::uint16_t sourceKey=0;
    bool calibrateJoystick=false;
};

// Sample compatibility admits accepted intro skip; recovered work retains menu
// resource setup. Original DOS loading/device ancestry is unqualified.
class NativeStartupSequence {
public:
    NativeStartupSequence(const GameplayAssets& game, const NativeStartupAssets& resources,
        LogoCompletion completion = LogoCompletion::sequenceEnd,
        std::shared_ptr<HighscoreStore> highscoreStore = {});
    void start(NativeStartupExecution execution = NativeStartupExecution::sampleCompatibility,
        NativePointerCapability pointerCapability = NativePointerCapability::unavailable,
        NativeMenuFeatures menuFeatures = NativeMenuFeatures::legacy,
        NativeJoystickSample joystick = {});
    void initializeSettings(StartupCapabilityProfile profile);
    void setJoystickSample(NativeJoystickSample sample);
    void initializeResources(NativeStartupDeviceProfile profile);
    void initializePresentation();
    void initializeFonts(StartupFontsProfile profile = StartupFontsProfile::normalMemory);
    ClockedPresentationWork pollLogoCountdown(std::size_t guestPolls);
    ClockedPresentationWork playLogoEvent(StartupLogoEventProfile profile = StartupLogoEventProfile::sourceSound);
    void animateLogo(std::size_t pollsPerPacket = 10000);
    void finishLogoSound(std::size_t pollsPerPacket = 10000, const NativeDeliverySink& sink = {});
    void releaseLogo();
    NativeSessionDelivery pollStoryIntro(NativeMenuInput input,std::size_t workPacketBudget,
        const NativeDeliverySink& sink = {});
    bool storyActive() const noexcept;
    std::optional<StoryScene> storyScene() const noexcept {return activeStory_;}
    std::size_t storyPage() const noexcept;
    SampledVoiceObservation storyVoice() const noexcept;
    bool titleActive() const noexcept;
    NativeSessionDelivery pollIntroTitle(NativeMenuInput input,std::size_t workPacketBudget,
        const NativeDeliverySink& sink = {});
    void skipIntro(NativeIntroKey key);
    void beginMenuSong();
    void initializeMenuResources();
    void skipCredits(NativeIntroKey key, std::size_t workPacketBudget = 10000);
    bool creditsActive() const noexcept;
    NativeSessionDelivery pollPlayableCredits(NativeMenuInput input,std::size_t workPacketBudget,
        const NativeDeliverySink& sink = {});
    void enterMenu(NativeMenuEntryProfile profile, std::size_t workPacketBudget = 10000);
    void beginGame(NativeMenuBeginProfile profile, std::size_t workPacketBudget = 10000);
    // Native Enter levels on the first widget; other menu controls reject.
    NativeMenuAction pollRecoveredMenu(NativeMenuInput input, std::size_t workPacketBudget = 10000,
        const NativeDeliverySink& sink = {});
    NativeSessionDelivery pollPlayableMenu(NativeMenuInput input, std::size_t workPacketBudget = 10000,
        const NativeDeliverySink& sink = {});
    bool menuBackAvailable() const noexcept;
    std::optional<NativeAttractProgress> attractProgress() const noexcept;
    bool attractAcceptsSourceKey() const noexcept;
    bool joystickCalibrationActive() const noexcept;
    bool joystickCalibrationRejected() const noexcept;
    JoystickCalibrationPoint joystickCalibrationPoint() const noexcept;
    const NativeStartupMenuInputRun* recoveredMenuInput() const noexcept;
    void acceptSkipInput();
    std::vector<std::int16_t> advance(std::size_t sampleBudget);
    void requestBegin();
    NativeMenuAction pollMenu(NativeMenuInput input);
    const MenuModel& menu() const noexcept { return menu_; }
    std::vector<std::uint8_t> menuPagePixels(NativeMenuPage page) const;
    std::uint16_t menuPageSegment(NativeMenuPage page) const noexcept;
    std::unique_ptr<NativeAudioOwner> releaseAudio();
    NativeStartupGameplayHandoff releaseGameplay();
    NativeStartupPhase phase() const noexcept { return phase_; }
    const IndexedScreen& frame() const noexcept { return frame_; }
    std::uint64_t elapsedSamples() const noexcept;
    std::uint16_t countdown() const noexcept;
    std::size_t logoSteps() const noexcept { return logoSteps_; }
    const std::array<std::int8_t, 7>& difficultyParameters() const noexcept { return difficulty_; }
    const NativeStartupContextRun* recoveredContext() const noexcept;
    const NativeStartupSettingsRun* recoveredSettings() const noexcept;
    const NativeStartupResourcesRun* recoveredResources() const noexcept;
    const NativeStartupPresentationRun* recoveredPresentation() const noexcept;
    const NativeStartupFontsRun* recoveredFonts() const noexcept;
    const NativeStartupLogoRun* recoveredLogo() const noexcept;
    const NativeStartupLogoEndRun* recoveredLogoEnd() const noexcept;
    const NativeStartupLogoReleaseRun* recoveredLogoRelease() const noexcept;
    const NativeStartupIntroRun* recoveredIntro() const noexcept;
    const NativeStartupMenuSongRun* recoveredMenuSong() const noexcept;
    const NativeStartupMenuResourcesRun* recoveredMenuResources() const noexcept;
    const NativeStartupCreditsRun* recoveredCredits() const noexcept;
    const NativeStartupMenuEntryRun* recoveredMenuEntry() const noexcept;
    const NativeStartupBeginRun* recoveredBegin() const noexcept;

private:
    enum class IntroTitleStage { inactive, overlays, returnInput };
    enum class MenuAttractProvider { unsupported, unavailable };
    NativeSessionDelivery advancePlayableAttract(NativeMenuInput input,std::size_t workPacketBudget);
    std::optional<NativeSessionDelivery> pollMenuJoystickCalibration(const NativeMenuInput& requested,std::size_t budget);
    void executeRecoveredMenu(const std::optional<NativeMenuInput>& input, std::size_t workPacketBudget,
        const NativeDeliverySink& sink = {}, NativeMenuBeginProfile profile = NativeMenuBeginProfile::enterPressRelease,
        MenuAttractProvider attractProvider = MenuAttractProvider::unsupported);
    void transition();
    void drawLogoStep();
    void drawMenuCursor();
    void drawMenuWidget(std::size_t selection, MenuWidgetState state);
    std::shared_ptr<PlayableStoryPresenter> story_;
    std::shared_ptr<PlayableAttractCredits> coldCredits_;
    std::optional<StoryScene> activeStory_;
    std::optional<CockpitPageState> storyRoles_;
    std::size_t introCharacter_ = 0;
    std::int16_t introX_ = 40;
    std::int16_t introY_ = 84;
    bool introStarted_ = false;
    bool introTypingFinished_ = false;
    IntroTitleStage introTitleStage_ = IntroTitleStage::inactive;
    std::uint64_t nativeTitleEndSample_ = 0;
    const GameplayAssets& game_;
    const NativeStartupAssets& resources_;
    std::unique_ptr<NativeAudioOwner> audio_;
    LogoCompletion completion_;
    NativeStartupPhase phase_ = NativeStartupPhase::idle;
    // Host page keys are relative storage only, not recovered Mode-X allocation order.
    IndexedPages pages_;
    IndexedScreen frame_;
    bool skipAccepted_ = false;
    std::size_t logoSteps_ = 0;
    std::array<std::int8_t, 7> difficulty_{};
    MenuModel menu_;
    std::optional<std::size_t> pressedMenu_;
    bool menuArmed_ = true;
    std::shared_ptr<PlayableMenuDialog> menuDialog_;
    std::shared_ptr<HighscoreStore> highscoreStore_;
    std::optional<ProjectedPoint> menuCursor_;
};

}  // namespace stellar7
