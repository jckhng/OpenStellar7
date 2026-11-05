#pragma once
#include "stellar7/destruction_presentation.hpp"
#include "stellar7/combat_presentation.hpp"

#include "stellar7/level1.hpp"
#include "stellar7/resource_codec.hpp"
#include "stellar7/frame_tail_work.hpp"
#include "stellar7/simulation_prefix_work.hpp"
#include "stellar7/visibility_state.hpp"
#include "stellar7/failure_control_work.hpp"
#include "stellar7/cockpit_state.hpp"
#include "stellar7/highscore.hpp"
#include "stellar7/campaign_loader.hpp"
#include "stellar7/deployment_prefix_work.hpp"
#include "stellar7/playable_system_effects.hpp"
#include "stellar7/native_pointer.hpp"
#include "stellar7/native_joystick.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace stellar7 {

class GameplayModel;
struct Level2PeriodicSpawnInput;
struct Level2PeriodicSpawnWork;
struct Level2PeriodicTailReceipt;
struct Level2OrdinaryTailReceipt;
struct Level1CombatSceneWork;
struct Level2HostileProjectileModel;
struct Level2PeriodicOrdinaryProfile;
struct RadarWork;
struct WeaponLampWork;

struct FailureGameplayStepWork;
struct RecoveredLevel1SuccessTailWork;
struct RecoveredLevel2PoolResetWork;
struct Level2PoolResetWork;
struct RecoveredLevel2PreloopWork;
struct Level1TransitionWork;
struct Level2LoaderActors;
struct Level2LoaderPageInput;
struct SuccessDisplayWork;
struct CockpitWorkBitmaps;
struct WorldModelGeometryInput;
struct WorldModelSetupWork;
struct GameplayAssets;
struct EndingSceneAssets;
struct StorySceneAssets;
enum class PolygonClipMode;

WorldModelGeometryInput worldModelGeometryInput(const GameplayAssets& assets, const WorldModelSetupWork& setup);
WorldModelGeometryInput worldModelGeometryInput(const GameplayAssets& assets, const WorldModelSetupWork& setup,
    const FixedPosition& fullCenter, PolygonClipMode clipping);

enum class GameplaySystemAction {
    none=0, jump=1, activateSelected=2,
    shield=3, scanner=4, superPrimary=5, turbo=6, cycleSelected=7, decoy=8, bomb=9, preferences=10, calibrateJoystick=11,
};

struct PlayableJumpState;
struct PlayableJumpResult;

struct GameplayInput {
    bool left = false;
    bool right = false;
    bool forward = false;
    bool reverse = false;
    bool fire = false;
    bool exit = false;
    bool confirm = false;
    GameplaySystemAction systemAction = GameplaySystemAction::none;
    std::optional<NativePointerInput> pointer;
    std::optional<NativeJoystickSample> joystick;
    std::optional<JoystickAxes> analog;
    GameplaySystemAction joystickAction=GameplaySystemAction::none;
    std::uint16_t sourceKey=0;
};

enum class Level2SightExecution { pending, rendered };
enum class Level2CockpitExecution { pending, rendered };
enum class Level2PrefixExecution { semantic, source };
enum class Level2SourceDomain { constructorControls, projectileCombat, projectileLifecycle, periodicLifecycle };

struct Level2RenderState {
    Level1GroundState& ground;
    StellarRng& rng;
    const Level1Entity& player;
    const std::vector<Level1Entity>& entities;
    const std::vector<std::size_t>& order;
    bool& alternateSightPhase;
    Level2SightExecution& sightExecution;
    std::int16_t& cockpitIndicatorX;
    std::int16_t& cockpitIndicatorVelocity;
    Level2CockpitExecution& cockpitExecution;
};

class Level2FrameRenderer {
public:
    virtual ~Level2FrameRenderer() = default;
    // Called after actor updates/sorting; owns the ground advance on this path.
    virtual void render(Level2RenderState state) = 0;
    virtual Level2PrefixExecution prefixExecution() const noexcept {
        return Level2PrefixExecution::semantic;
    }
    virtual Level2SourceDomain sourceDomain() const noexcept {
        return Level2SourceDomain::constructorControls;
    }
    // A returned plan replaces the next prefix after a consumed source tail.
    virtual std::optional<StationaryLevel2PrefixWork> prepare(
        const StationaryLevel2PrefixInput&) { return std::nullopt; }
    // Called after presentation snapshots; a returned state consumes this tail once.
    virtual std::optional<FrameTailState> finish(
        const FrameTailState&, const FrameTailGuards&) { return std::nullopt; }
    virtual const Level2LifecycleTailWork* lifecycleTail() const noexcept { return nullptr; }
    virtual const Level2PeriodicTailReceipt* periodicTail() const noexcept { return nullptr; }
    virtual const Level2OrdinaryTailReceipt* ordinaryTail() const noexcept { return nullptr; }
};

struct CampaignLevelAssets {
    std::string resourceName;
    std::vector<std::vector<IndexedBitmap>> bitmapGroups;
    std::vector<std::uint8_t> musicSequence;
    std::vector<Model3d> models;
};

struct GameplayAssets {
    IndexedScreen cockpit;
    std::vector<IndexedBitmap> horizonFrames;
    std::vector<std::vector<IndexedBitmap>> artGroups;
    std::vector<Model3d> models;
    std::vector<Rgb> uiPalette;
    std::vector<std::vector<std::uint8_t>> soundSequences;
    std::vector<std::uint8_t> soundBlasterInstrumentResource;
    std::vector<std::uint8_t> levelMusicSequence;
    std::vector<std::uint8_t> presentationMusicSequence;
    // Optional target-local source palette.  Synthetic/test assets leave this
    // empty; the loaded Stellar 7 assets use it to reproduce runtime DAC
    // transforms without changing the indexed framebuffer.
    std::vector<std::uint8_t> cockpitDacPalette;
    bool emulateTargetPaletteTiming = false;
    std::vector<IndexedBitmap> level2HorizonFrames;
    std::vector<std::uint8_t> level2MusicSequence;
    std::vector<CampaignLevelAssets> campaignLevels;
    // Optional target-local capture of the Level 3 terminal presenter.  The
    // VQT decoding exists, but scene selection and presenter history remain
    // unqualified. This diagnostic indexed/DAC sequence stays under .work.
    std::vector<IndexedScreen> level3SuccessFrames;
    // The Type-25/26 contact handler enters the shared presenter through a
    // parameter-dependent partial-redraw track.  Keep its controlled native
    // sequence separate from the canonical completion track.
    std::vector<IndexedScreen> level3ContactSuccessFrames;
    // First exact indexed/DAC frame after the native Level 3 presenter
    // returns to the campaign loop. This is a handoff boundary asset, not a
    // decoded Level 4 implementation.
    std::vector<IndexedScreen> level3HandoffFrames;
    // Optional captured Level 3 guardian page used as a static cockpit/HCUD
    // surface. The world viewport remains renderer-driven; pixels outside the
    // viewport preserve the native running-page surface when this evidence is
    // supplied by the target-local launcher.
    std::vector<IndexedScreen> level3GuardianFrames;
    // LEVEL2.RES replaces the level-local model-table region while Level 2
    // is active. Its five entries correspond to native entity types 44..48.
    std::vector<Model3d> level2Models;
    // LEVEL3.RES's local model table is distinct from the cockpit/global
    // table.  Its four entries are selected for native entity types 44..47
    // while the Level 3 campaign phase is active.
    std::vector<Model3d> level3Models;
    // The captured guardian framebuffer has unproven save-state ancestry.
    // Keep it available for explicit comparison commands without importing
    // its pixels into ordinary interactive rendering.
    bool useUnprovenLevel3GuardianSurface = false;
    // The terminal presenter sequences were captured from controlled
    // save-state/substitution routes and retain stale Mode-X page pixels.
    // Explicit comparison commands may opt in; ordinary interactive play
    // uses the clean model-driven presenter until a natural-run capture
    // proves a replacement surface.
    bool useUnprovenLevel3SuccessSurfaces = false;
    BitmapFont scoreFont;
    std::vector<std::uint8_t> logoMusicSequence;
    std::vector<std::array<std::uint8_t, 256>> modelColorTables;
    // Shared registry0 initialized by4a34/bfcc; local level loads retain it.
    std::array<std::uint16_t, 2> sharedModelRegistry{};
    BitmapFont titleFont;
    std::optional<CockpitSettings> cockpitSettings;
    std::optional<HallOfFameScores> hallOfFameScores;
    std::vector<std::uint8_t> soundBlasterVolumeTable;
    std::optional<HighscoreTable> highscoreTable;
    std::shared_ptr<const EndingSceneAssets> endingScene;
    std::vector<std::uint8_t> briefingText;
    std::shared_ptr<const StorySceneAssets> storyScenes;
    bool attractVoiceAvailable=false;

    const std::vector<IndexedBitmap>& horizonFramesForLevel(
        const std::size_t levelIndex
    ) const noexcept {
        if (
            levelIndex < campaignLevels.size()
            && !campaignLevels[levelIndex].bitmapGroups.empty()
        ) {
            return campaignLevels[levelIndex].bitmapGroups.front();
        }
        if (levelIndex == 0) {
            return horizonFrames;
        }
        if (levelIndex == 1) {
            return level2HorizonFrames;
        }
        static const std::vector<IndexedBitmap> empty;
        return empty;
    }

    const std::vector<std::uint8_t>& musicForLevel(
        const std::size_t levelIndex
    ) const noexcept {
        if (levelIndex < campaignLevels.size()) {
            return campaignLevels[levelIndex].musicSequence;
        }
        if (levelIndex == 0) {
            return levelMusicSequence;
        }
        if (levelIndex == 1) {
            return level2MusicSequence;
        }
        static const std::vector<std::uint8_t> empty;
        return empty;
    }
};

enum class GameplayPhase {
    level1,
    successPresentation,
    level2Handoff,
    level2,
    level2SuccessPresentation,
    level3Handoff,
    level3,
    level3SuccessPresentation,
    level4Handoff,
    failurePreExplosionBlackout,
    failureExplosion,
    failurePostExplosionBlackout,
    continuePrompt,
    level1RetryHandoff,
    campaignEnded,
    level5LoaderReturn,
    level5,
    highscoreNameEntry,
    highscoreHall,
    level5SuccessReturn,
    endingEntry,
    endingPresentation,
    endingTeardown,
    endingReturn,
    nativeMenu,
    nativeBeginWait,
    level1BeginHandoff,
    level2SuccessReturn,
    level3LoaderReturn,
    level3PreloopReturn,
    level3Native,
    level1SuccessTailReturn,
    level2LoaderReturn,
    level2MusicReturn,
    level2PreloopReturn,
    playableLoaderReturn,
    playablePreloopReturn,
    playable,
};

enum class ContinueChoice { yes, no };
enum class ContinuePointerButton { released, held, cancel };
struct ContinuePointerInput {
    std::optional<ContinueChoice> hit;
    ContinuePointerButton button;
};
enum class ContinueDecision { pending, yes, no, cancelled };

enum class PlayableHighscoreCaller { retry, endingTitle, gameOverTitle };
enum class EndingPageResult { present, exhausted };

struct FailureBurst {
    FixedPosition position{};
    unsigned spawnFrame = 0;
};

enum class FailureVisibleKind : std::uint8_t {
    mission,
    player,
    debris,
    burst,
};

struct FailureVisibleItem {
    FailureVisibleKind kind = FailureVisibleKind::mission;
    std::size_t index = 0;
};

struct CockpitIndicatorState {
    std::int16_t x;
    std::int16_t velocity;
};

struct PlayablePreferencesRadarActor {
    std::size_t allocation;
    Level1Entity actor;
    std::optional<std::uint64_t> incarnation;
};

struct PlayableModelDetail {
    std::uint16_t scale = 256;
    std::uint16_t cap = 256;
};

struct CockpitInitializationRecord {
    std::uint8_t levelIndex;
    std::array<CockpitIndicatorState, 4> states;
    std::uint8_t displaySteps = 3;
};

struct Level1GameplayFrameWork;
class RadarState;
class IndexedPages;

class GameplayModel {
public:
    static constexpr std::size_t maxShots = 2;
    // Legacy headless fallback; live success playback supplies completion.
    static constexpr unsigned successPresentationFrames = 167;
    // Sound Blaster success FC occurs at sample252645/44100Hz. Nominal
    // headless fallback at1455 samples per33ms display frame; live audio
    // supplies its actual handle state through the setter below.
    static constexpr unsigned level2SuccessPresentationFrames = 174;
    static constexpr unsigned level3SuccessPresentationFrames = 202;
    static constexpr unsigned level3ContactPresenterSurfaceFrames = 98;
    static constexpr unsigned failureExplosionFrames = 45;
    static constexpr std::size_t failureDebrisCount = 30;

    GameplayModel() noexcept;
    static GameplayModel fromNativeStartup() noexcept;
    static GameplayModel fromNativeStartup(std::int16_t detail);
    void synchronizePlayableDetail(std::int16_t detail);
    std::array<CockpitIndicatorState, 3> resumePlayablePreferences(std::int16_t detail);
    PlayableModelDetail playableModelDetail(std::int16_t type) const noexcept;
    std::vector<PlayablePreferencesRadarActor> playablePreferencesRadarActors() const;

    // Synthetic loop-zero diagnostic entry; real campaign transitions retain state.
    void startLevel2() noexcept;
    void startLevel3() noexcept;
    void tick(const GameplayInput& input);
    //Retained native semantics; simulation/render CPU work is not certified.
    void initializePlayableDifficulty(CampaignDifficulty difficulty);
    CampaignDifficulty runtimeDifficulty() const noexcept { return runtimeDifficulty_; }
    void preparePlayableCampaignModels(const std::vector<Model3d>& shared, const std::vector<Model3d>& local);
    void advancePlayableCampaign(const GameplayInput& input);
    std::vector<PlayableSystemEffect> finishPlayableCampaignFrame();
    std::vector<PlayableSystemEffect> resumePlayableCampaignSuccess(const GameplayInput& input = {});
    std::uint8_t pendingPlayableCampaignDestination() const;
    void completePlayableCampaignLoad(std::uint8_t destination, const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local);
    std::vector<PlayableSystemEffect> completePlayableCampaignPreloop();
    void completePlayableCampaignEnding();
    void advancePlayableFailure();
    ContinueDecision pollPlayableContinue(const GameplayInput& input,
        const std::optional<ContinuePointerInput>& pointer=std::nullopt);
    void completePlayableCampaignRetry(const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local);
    std::optional<PlayableHighscoreCaller> playableHighscoreCaller() const noexcept;
    void beginPlayableHighscoreNameEntry();
    void beginPlayableHighscoreHall();
    void finishPlayableHighscore();
    void finishPlayableTitleReturn();
    void preparePlayableMenuBegin(CampaignDifficulty difficulty=CampaignDifficulty::easy);
    void completePlayableMenuBegin(const std::vector<Model3d>& shared,const std::vector<Model3d>& local);
    void adoptPlayableLevel1(const std::vector<Model3d>& shared,const std::vector<Model3d>& local);
    void tickRetainedLevel1(const GameplayInput& input);
    void tickRetainedLevel1(const GameplayInput& input, Level1FrameRenderer& renderer);
    void finishLevel1Frame();
    Level1GameplayFrameWork advanceRecoveredLevel1Frame(const GameplayAssets& assets,
        const CockpitSettings& settings, const CockpitPageState& pages, const RadarState& radar,
        const IndexedPages& pixels, FrameTailGuards guards);
    FailureGameplayStepWork advanceRecoveredFailureStep(const GameplayAssets& assets,
        std::uint16_t drawPage, Viewport incomingHardwareClip);
    void enterRecoveredContinue();
    ContinueDecision pollRecoveredContinue(const GameplayInput& input);
    ContinueDecision recoveredContinueDecision() const noexcept;
    void resumeRecoveredContinueRetry();
    void completeRecoveredLevel1Retry();
    void tickWithRenderer(const GameplayInput& input, Level2FrameRenderer& renderer);
    std::vector<std::size_t> recoveredLevel2RenderKeys() const;
    Level1TargetPrefixInput recoveredLevel2ProjectilePrefixInput(
        Level1PlayerCombatDomain domain = Level1PlayerCombatDomain::survivingOnly) const;
    StationaryLevel2PrefixWork recoveredLevel2ProjectilePrefix(const BitmapFont& scoreFont,
        const CockpitPageState& pages, const Level2HostileProjectileModel* hostile = nullptr,
        Level1PlayerCombatDomain domain = Level1PlayerCombatDomain::survivingOnly) const;
    Level1TargetPrefixInput recoveredLevel2PeriodicProjectilePrefixInput(
        Level1PlayerCombatDomain domain = Level1PlayerCombatDomain::survivingOnly) const;
    StationaryLevel2PrefixWork recoveredLevel2PeriodicProjectilePrefix(const BitmapFont& scoreFont,
        const CockpitPageState& pages, const Level2PeriodicOrdinaryProfile& periodic,
        const Level2HostileProjectileModel* hostile = nullptr,
        Level1PlayerCombatDomain domain = Level1PlayerCombatDomain::survivingOnly) const;
    void commitRecoveredLevel2ProjectilePrefix(const StationaryLevel2PrefixWork& prefix);
    void commitRecoveredLevel2CombatMain(const Level1CombatSceneWork& work);
    void commitRecoveredLevel2CombatRadar(const RadarWork& work);
    Level1PrimaryFireInput recoveredLevel2CombatPrimaryInput(const GameplayAssets& assets,
        Level1PlayerCombatDomain domain = Level1PlayerCombatDomain::survivingOnly) const;
    void commitRecoveredLevel2CombatPrimary(const GameplayAssets& assets, const Level1PrimaryFireWork& work,
        Level1PlayerCombatDomain domain = Level1PlayerCombatDomain::survivingOnly);
    Level1PrimaryFireInput recoveredLevel2PeriodicPrimaryInput(const GameplayAssets& assets,
        Level1PlayerCombatDomain domain = Level1PlayerCombatDomain::survivingOnly) const;
    void commitRecoveredLevel2PeriodicPrimary(const GameplayAssets& assets, const Level1PrimaryFireWork& work,
        Level1PlayerCombatDomain domain = Level1PlayerCombatDomain::survivingOnly);
    void commitRecoveredLevel2WeaponLamp(const WeaponLampWork& work);
    void commitRecoveredLevel2CombatTail(const FrameTailState& tail);
    void commitRecoveredLevel2LifecycleTail(const Level2LifecycleTailWork& tail);
    Level2PeriodicSpawnInput recoveredLevel2PeriodicInput(const FrameTailState& recentered) const;
    Level1ArrivalInput recoveredLevel2ArrivalInput(const FrameTailState& recentered,
        const Level2PeriodicSpawnWork& periodic, std::uint8_t cachedEnemyFlag) const;
    void commitRecoveredLevel2PeriodicTail(const Level2LifecycleTailWork& tail,
        const Level2PeriodicSpawnWork& periodic, const std::vector<Model3d>& ownedShared,
        const Level1ArrivalWork* arrival = nullptr, std::uint8_t cachedEnemyFlag = 0);
    void commitRecoveredLevel2OrdinaryTail(const Level2LifecycleTailWork& tail);
    RecoveredLevel1SuccessTailWork advanceRecoveredLevel1SuccessTail(const GameplayAssets& assets,
        const CockpitPageState& pages, FrameTailGuards guards, std::uint8_t enemyFlag);
    Level1TransitionWork recoveredLevel2TransitionOperations(const CockpitPageState& pages,
        std::uint16_t introduction) const;
    void beginRecoveredLevel2Load();
    void beginRecoveredLevel2Retry();
    RecoveredLevel2PoolResetWork resetRecoveredLevel2RetryPools(const std::vector<Model3d>& sharedModels,
        const std::vector<Model3d>& levelModels = {});
    Level2LoaderPageInput prepareRecoveredLevel2RetryLoaderPages(const CockpitSettings& settings);
    RecoveredLevel2PoolResetWork resetRecoveredLevel2Pools(const std::vector<Model3d>& sharedModels,
        const std::vector<Model3d>& levelModels = {});
    Level2LoaderActors constructRecoveredLevel2Actors(const std::vector<Model3d>& sharedModels,
        const std::vector<Model3d>& levelModels = {});
    Level2LoaderPageInput prepareRecoveredLevel2LoaderPages(const CockpitSettings& settings);
    void completeRecoveredLevel2MusicActivation();
    RecoveredLevel2PreloopWork completeRecoveredLevel2Preloop();
    Level1PrimaryFireInput recoveredLevel2PrimaryInput(const GameplayAssets& assets) const;
    void commitRecoveredLevel2Primary(const GameplayAssets& assets, const Level1PrimaryFireWork& work);
    std::optional<Level1RewardSuccessChild> recoveredSuccessContact() const;
    SuccessDisplayWork beginRecoveredSuccess(const GameplayAssets& assets,
        const CockpitPageState& pages, const RadarState& radar, const CockpitWorkBitmaps& bitmaps);
    void finishRecoveredSuccess();
    Level1GameplayFrameWork resumeRecoveredLevel1Success(const GameplayAssets& assets,
        const CockpitSettings& settings, const CockpitPageState& pages, const RadarState& radar,
        const IndexedPages& pixels);
    bool pendingLevel1SuccessContact() const noexcept;
    void validateLevel1SuccessReturn(const GameplayInput& input) const;
    bool pendingLevel1FailureTail() const noexcept;
    void finishLevel1FailureTail();
    bool pendingLevel2FailureTail() const noexcept;
    void finishLevel2FailureTail();
    void setSuccessAudioActive(bool active) noexcept;
    // Native source7204->4e06/50be. Ordinary Level5 execution is a later boundary.
    void completeLevel2Warp(const std::vector<Model3d>& sharedModels,
        const std::vector<Model3d>& levelModels = {});
    void finishLevel5Frame();
    void validateLevel5SuccessReturn(const GameplayInput& input) const;
    void validateLevel2SuccessReturn(const GameplayInput& input) const;
    void completeLevel3Load(const std::vector<Model3d>& sharedModels,
        const std::vector<Model3d>& levelModels);
    void validateLevel3Preloop() const;
    std::vector<PlayableSystemEffect> completeLevel3Preloop();
    void advanceRetainedLevel3(const GameplayInput& input);
    void finishLevel3Radar();
    void finishLevel3Frame();
    void validateCampaignSuccessCleanup() const;
    void finishCampaignSuccessCleanup();
    void validateEndingFirstPage() const;
    void finishEndingFirstPage();
    void validateEndingPage() const;
    void finishEndingPage(EndingPageResult result);
    void validateEndingTeardown() const;
    void finishEndingTeardown();
    void validateTitleReturn() const;
    void finishTitleReturn();
    void finishMenuPoll();
    void validateMenuBegin() const;
    void prepareMenuBegin();
    void finishMenuBegin();
    void beginHighscoreNameEntry();
    void validateHighscoreHall(const std::vector<Model3d>& sharedModels) const;
    void beginHighscoreHall();
    void validateLevel5Retry(const std::vector<Model3d>& sharedModels,
        HighscoreReturn result) const;
    void completeLevel5Retry(const std::vector<Model3d>& sharedModels,
        HighscoreReturn result, const std::vector<Model3d>& levelModels = {});
    std::size_t campaignLevel() const noexcept { return activeCampaignLevel_; }

    std::int32_t heading() const noexcept;
    std::int32_t travel() const noexcept;
    std::int16_t turnVelocity() const noexcept;
    std::int16_t forwardVelocity() const noexcept;
    int panoramaOffset() const noexcept;
    std::uint64_t ticks() const noexcept;
    bool shotActive() const noexcept;
    bool shotActive(std::size_t slot) const noexcept;
    unsigned shotAge() const noexcept;
    unsigned shotAge(std::size_t slot) const noexcept;
    unsigned shotCount() const noexcept;
    unsigned weaponCooldown() const noexcept;
    bool weaponReady() const noexcept;
    bool weaponIndicatorVisible() const noexcept;
    bool weaponReadyIndicator() const noexcept;
    bool alternateSightPhase() const noexcept;
    int cockpitIndicatorX() const noexcept;
    const CockpitInitializationRecord& lastCockpitInitialization() const noexcept;
    bool enemyActive() const noexcept;
    bool explosionActive() const noexcept;
    std::int32_t enemyBearing() const noexcept;
    int enemyRange() const noexcept;
    unsigned explosionAge() const noexcept;
    FixedPosition explosionPosition() const noexcept;
    unsigned score() const noexcept;
    std::int16_t playerArmor() const noexcept;
    unsigned damageFlashAge() const noexcept;
    bool damageFlashActive() const noexcept;
    bool level2EntryPaletteActive() const noexcept;
    GameplayPhase phase() const noexcept;
    ContinueChoice continueChoice() const noexcept;
    std::optional<ContinueChoice> pressedContinueChoice() const noexcept;
    unsigned remainingContinues() const noexcept;
    unsigned presentationFrame() const noexcept;
    bool level3CompletionByPickup() const noexcept;
    const ViewTransform& failureView() const noexcept;
    const Level1Entity& failurePlayer() const noexcept;
    bool failurePlayerVisible() const noexcept;
    bool failureFromLevel2() const noexcept;
    const std::vector<Level1Entity>& failureMissionEntities() const noexcept;
    const std::vector<Level1Entity>& failureDebris() const noexcept;
    const std::vector<FailureBurst>& failureBursts() const noexcept;
    const std::vector<FailureVisibleItem>&
        failureVisibleItems() const noexcept;
    const StellarRng& failureRng() const noexcept;
    const std::vector<Level1AudioEvent>& failureAudioEvents() const noexcept;
    const Level1State& level1State() const noexcept;
    const std::vector<Level1AudioEvent>& level2AudioEvents() const noexcept;
    const std::vector<DestructionCue>& destructions() const noexcept {return destructions_;}
    const CombatFeedback& combatFeedback() const noexcept {return combatFeedback_;}
    const std::array<std::int64_t,2>& presentationTranslation() const noexcept {return presentationTranslation_;}
    std::uint16_t level2LoopTick() const noexcept;
    const Level1Entity& level2Player() const noexcept;
    const std::vector<Level1Entity>& level2Entities() const noexcept;
    const std::vector<std::size_t>& level2ActiveListOrder() const noexcept;
    const std::vector<Level1Entity>&
        level2PresentationEntities() const noexcept;
    const CompletionBeamPresentation* level2CompletionBeam(std::size_t index) const noexcept;
    const std::vector<std::size_t>&
        level2PresentationActiveListOrder() const noexcept;
    // Stable source-pool key for live presentation records; terminal snapshots have no key.
    std::optional<std::size_t> level2PresentationPoolSlot(std::size_t index) const noexcept;
    std::optional<std::uint64_t> level2PresentationActorIncarnation(std::size_t index) const noexcept;
    std::uint64_t level2AllocationRevision() const noexcept { return level2AllocationRevision_; }
    const Level1GroundState& level2Ground() const noexcept;
    const StellarRng& level2Rng() const noexcept;
    int level2CockpitIndicatorX() const noexcept;
    int level2CockpitIndicatorVelocity() const noexcept;

private:
    void recordDestruction(const Level1Entity& source,const FixedPosition& position,std::uint16_t event);
    std::vector<DestructionCue> destructions_;
    std::uint64_t destructionSerial_=0;
    CombatFeedback combatFeedback_;
    std::array<std::int64_t,2> presentationTranslation_{};
    friend struct GameplayModelTestAccess;
    friend struct GameplayAnalysisAccess;
    friend struct GameplayNativeCheckpointAccess;

    void advanceCockpitIndicator() noexcept;
    enum class InitialDisplay { firstOrdinary, loaderOnly };
    explicit GameplayModel(InitialDisplay display, std::optional<std::int16_t> nativeDetail = std::nullopt) noexcept;
    void prepareCockpitInitialDisplay(InitialDisplay display = InitialDisplay::firstOrdinary) noexcept;
    void initializeSyntheticCampaignIndicator() noexcept;
    void initializeFailurePresentation() noexcept;
    void advanceFailureMissionAnimation() noexcept;
    void initializeContinuePrompt() noexcept;
    bool recoveredLevel2ContinueReady() const noexcept;
    RecoveredLevel2PoolResetWork commitRecoveredLevel2PoolReset(Level2PoolResetWork work);
    Level2LoaderPageInput recordRecoveredLevel2LoaderPages(const CockpitSettings& settings);
    ContinueDecision updateContinueChoice(const GameplayInput& input);
    void advanceContinuePrompt(const GameplayInput& input);
    void retryLevel1();
    void retryLevel2(const GameplayInput& input);
    void advanceFailurePresentation(unsigned frame) noexcept;
    void finishFailurePresentation() noexcept;
    void spawnFailureBurst(unsigned frame) noexcept;
    void rebuildFailureVisibleItems(unsigned frame) noexcept;
    void initializeCampaignPools(std::size_t level, std::size_t actorPoolMembers) noexcept;
    enum class CampaignModelBinding { sourceMetadata, fullPlayable };
    void bindCampaignModels(std::uint8_t level, const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local, CampaignModelBinding binding = CampaignModelBinding::fullPlayable);
    void initializeRetainedCampaignLevel(std::uint8_t destination, std::vector<Level1Entity> actors,
        StellarRng rng, const std::vector<Model3d>& shared,const std::vector<Model3d>& local);
    void initializeLevel5(const std::vector<Model3d>& sharedModels, StellarRng rng,
        std::int16_t armor, const std::vector<Model3d>& levelModels = {});
    void validateLevel5Reload(const std::vector<Model3d>& sharedModels) const;
    enum class Level2EntryOrigin { campaign, diagnostic, retry };
    void initializeLevel2(const GameplayInput& input, Level2EntryOrigin origin) noexcept;
    void initializeLevel3(std::uint32_t score = 2740) noexcept;
    void initializeLevel3FromCampaign() noexcept;
    enum class Level2Continuation { frameEntry, successReturn };
    void advanceLevel2(const GameplayInput& input, Level2FrameRenderer* renderer = nullptr,
        Level2Continuation continuation = Level2Continuation::frameEntry);
    void prepareFirstLevel2SourceFrame(const GameplayInput& input, Level2FrameRenderer* renderer);
    void qualifyLevel2SourcePrefix(const GameplayInput& input, Level2FrameRenderer* renderer);
    void advanceLevel2SourcePrefix(const GameplayInput& input, Level2FrameRenderer& renderer);
    static bool campaignCollides(const Level1Entity& left, const Level1Entity& right) noexcept;
    std::size_t campaignEntityIdentity(std::size_t index) const;
    std::size_t campaignEntityIndex(std::size_t identity) const;
    // Synchronous effects borrow frame-local allocation/contact owners. Never retain them.
    struct CampaignCombatCallbacks {
        std::function<void(std::size_t)> retire;
        std::function<bool(const Level1Entity&, std::uint8_t, std::uint8_t)> effect;
        std::function<void(const Level1Entity&, CampaignExit)> reward;
    };
    bool applyCampaignHit(Level1Entity& shadow, std::uint8_t amount, std::size_t identity,
        const CampaignCombatCallbacks& callbacks, std::optional<FixedPosition> effectPosition = {});
    enum class CampaignContactFlow { continueFrame, suspendFrame };
    struct CampaignContactStart;
    struct CampaignContactMotion;
    struct CampaignPlayerContactResult;
    struct CampaignProjectileContactResult;
    bool campaignActorExistedAtStart(std::size_t entityIndex, const CampaignContactStart& start) const noexcept;
    CampaignPlayerContactResult resolveCampaignPlayerContact(const CampaignContactStart& start,
        Level2Continuation continuation, Level2FrameRenderer* renderer, const CampaignCombatCallbacks& combat);
    CampaignProjectileContactResult resolveCampaignPlayerProjectileHits(const CampaignContactStart& start,
        const CampaignContactMotion& motion, const CampaignCombatCallbacks& combat);
    CampaignContactFlow resolveCampaignActorContacts(const CampaignContactStart& start,
        const CampaignContactMotion& motion, const CampaignCombatCallbacks& combat);
    void expireLevel2EffectPresentation();
    void sortLevel2ActiveByDistance();
    struct Level2EntityIdentity {
        std::uint8_t pool;
        std::size_t slot;
        std::size_t activePosition;
    };
    struct CampaignRetirement;
    enum class CampaignRewardDomain { shared, physicalLevel2Guardian };
    void retireCampaignEntity(std::size_t removed, CampaignRetirement& retirement) noexcept;
    bool spawnCampaignTransientEffect(const Level1Entity& source,
        std::uint8_t firstFrame, std::uint8_t terminalFrame) noexcept;
    void deployCampaignReward(const Level1Entity& source, CampaignExit exit,
        const std::vector<Level2EntityIdentity>& identities, CampaignRewardDomain domain);
    static void applyCampaignGroundRidingVelocity(Level1Entity& entity, const Level1Entity& behaviorTarget) noexcept;
    void advanceCampaignType48(Level1Entity& entity) noexcept;
    static constexpr std::size_t ordinaryBehaviorStateByte = 5;
    static std::int32_t arithmeticShiftRightFour(std::int32_t value);
    void advanceCampaignType10(std::size_t index);
    void advanceCampaignType12(std::size_t index);
    void advanceCampaignType13(Level1Entity& entity, const Level1Entity& behaviorTarget);
    void advanceCampaignOrdinaryWithFire(Level1Entity& actor, std::size_t ownerSlot, const Level1Entity& behaviorTarget);
    void advanceLiveCampaignOrdinary(std::size_t index, const Level1Entity& behaviorTarget);
    void advanceCampaignType6(std::size_t index);
    void advanceCampaignType9(std::size_t index);
    void advancePhysicalLevel2Guardian(std::size_t slot, const Level1Entity& behaviorTarget,
        const CampaignProjectileContactResult& projectileContact, bool& guardianAllocatedThisBoundary);
    void advanceLevel5Guardian(Level1Entity& actor, std::size_t ownerSlot);
    std::optional<Level1Entity> advanceLevel3Guardian(Level1Entity& entity);
    void deployLevel3GuardianBurst(const Level1Entity& source);
    void advanceCampaignGuardian(Level1Entity& actor, std::size_t ownerSlot,
        const std::vector<Level2EntityIdentity>& identities, const CampaignCombatCallbacks& combat);
    std::size_t level2FixedPoolSlot(std::size_t entityIndex) const noexcept;
    std::vector<Level2EntityIdentity> captureLevel2ActiveIdentities(const std::vector<std::size_t>& order) const;
    std::vector<std::size_t> remapLevel2BoundaryOrder(const std::vector<Level2EntityIdentity>& identities) const;
    bool consumeLevel2DeferredTail();
    PlayableJumpState playableJumpState() const;
    void commitPlayableJump(const PlayableJumpResult& result);
    std::vector<PlayableSystemEffect> playableSystemEffects_;
    //925E visits the retained OLD physical pointers, including inactive return tails.
    std::vector<std::size_t> playableSystemOldActorSlots_;
    void advanceLevel2GuardianAnimation() noexcept;
    void advanceLevel3Animation() noexcept;
    void advanceLevel2Pickup() noexcept;
    void resumeLevel5Success(const GameplayInput& input);
    void resumeLevel2Success(const GameplayInput& input);
    void presentLevel2(const GameplayInput& input, Level2FrameRenderer* renderer,
        bool firstFrame);
    void applyLevel2Tail(const FrameTailState& tail);
    Level1PrimaryFireInput level2PrimaryInput(const GameplayAssets& assets, Level1PrimaryFireMembership membership,
        Level1PlayerCombatDomain domain = Level1PlayerCombatDomain::survivingOnly) const;
    void commitLevel2Primary(const GameplayAssets& assets, const Level1PrimaryFireWork& work,
        Level1PrimaryFireMembership membership,
        Level1PlayerCombatDomain domain = Level1PlayerCombatDomain::survivingOnly);
    void commitLevel2CombatTail(const FrameTailState& tail, Level1PlayerCombatDomain domain);
    void validateLevel2CombatTail(const FrameTailState& tail, Level1PlayerCombatDomain domain) const;
    void applyLevel2CombatTailState(const FrameTailState& tail);
    void validateLevel2LifecycleTail(const Level2LifecycleTailWork& tail,
        const Level1ArrivalWork* arrival = nullptr) const;
    void finishLevel2LifecycleTail(const Level2LifecycleTailWork& tail);
    Level2PeriodicSpawnInput level2OrdinaryTailInput(const FrameTailState& recentered) const;
    Level1TargetPrefixInput level2ProjectilePrefixInput(Level1PlayerCombatDomain domain,
        const Level2PeriodicOrdinaryProfile* periodic) const;
    void applyLevel2Prefix(const StationaryLevel2PrefixWork& prefix);
    void refreshLevel2Visibility();
    void tickRetainedLevel1Impl(const GameplayInput& input, Level1FrameRenderer* renderer);
    enum class GameplayInputDomain { source, playable };
    void tickImpl(const GameplayInput& input, Level2FrameRenderer* renderer,
        Level1FrameRenderer* level1Renderer = nullptr, GameplayInputDomain domain = GameplayInputDomain::source);
    void refreshLevel2SightPhase() noexcept;
    void runLevel2PeriodicSpawnCheck();
    void runLevel2CompletionCheck();
    std::uint8_t nextLevel2VisualRandomByte() noexcept;
    bool fireLevel2EnemyProjectile(
        std::size_t entityIndex,
        int explicitWeaponSlot = -1
    );
    bool fireLevel2EnemyProjectile(Level1Entity& owner, std::size_t ownerSlot,
        int explicitWeaponSlot);
    void enterPlayableGuardianVictory();
    void syncPlayableCampaignPools();
    enum class ActorInitialization { legacy, validatedOrdinary };
    std::size_t allocateLevel2Actor(Level1Entity entity,
        ActorInitialization initialization = ActorInitialization::legacy) noexcept;
    std::size_t allocateLevel2FixedObject(Level1Entity entity) noexcept;
    void validatePlayableBomb() const;
    void deployPlayableBomb();
    std::optional<std::size_t> registerLevel5DestroyedTypeBonus(const Level1Entity& source);
    void insertLevel2FixedPoolSlot(std::size_t index, std::size_t slot) noexcept;
    bool hasPhysicalPostWaveRole() const noexcept;
    void retainPhysicalPostWaveRole() noexcept;
    std::size_t postWaveObjectIndex() const noexcept;

    std::uint16_t scaledCampaignAward(std::uint16_t award) const;
    Level1State level1_;
    CampaignDifficulty runtimeDifficulty_=CampaignDifficulty::easy;
    bool playableCampaignFrames_ = false;
    bool playableCockpitIndicatorEnabled_ = true;
    bool playableDetailBound_ = false;
    bool playableRadarAnimationSettled_ = false;
    PlayableModelDetail playableSharedModelDetail_{};
    struct CampaignModelMetadata {
        std::uint32_t radius = 0;
        std::uint8_t selectors = 0;
        std::uint8_t animationLimit = 0;
    };
    std::array<CampaignModelMetadata,49> campaignModels_{};
    bool campaignModelsBound_ = false;
    CampaignExit playableExit_ = CampaignExit::greenReward;
    std::optional<CampaignOrdinaryConstructor> playableOrdinaryConstructor_;
    std::int16_t level6GuardianTurn_ = 0;
    Level1Entity campaignPlayerProxy_{};
    std::int32_t travel_ = 0;
    std::uint64_t ticks_ = 0;
    std::uint64_t recoveredLevel1TickBase_ = 0;
    bool weaponIndicatorVisible_ = false;
    bool weaponReadyIndicator_ = false;
    FixedPosition level1ShotOrigin_{}; //Source4858, retained across primary launches.
    bool alternateSightPhase_ = false;
    std::int16_t cockpitIndicatorX_ = 130;
    std::int16_t cockpitIndicatorVelocity_ = 4;
    CockpitInitializationRecord lastCockpitInitialization_{};
    unsigned damageFlashAge_ = 0;
    bool damageFlashActive_ = false;
    bool level2EntryPaletteActive_ = true;
    unsigned explosionTicksRemaining_ = 0;
    unsigned explosionAge_ = 0;
    FixedPosition explosionPosition_{};
    GameplayPhase phase_ = GameplayPhase::level1;
    unsigned presentationFrame_ = 0;
    bool successAudioOwned_ = false;
    bool successAudioActive_ = false;
    std::optional<Level1RewardSuccessPause> recoveredSuccessPause_;
    bool recoveredSuccessReturned_ = false;
    std::optional<FailureControlState> recoveredFailureControl_;
    Level1Entity recoveredFailureCamera_{};
    std::array<std::optional<std::size_t>, Level1State::deathEffectPoolSize> failureBurstHistory_{};
    ViewTransform failureView_{};
    Level1Entity failurePlayer_{};
    std::vector<Level1Entity> failureMissionEntities_;
    std::vector<Level1Entity> failureDebris_;
    std::vector<FailureBurst> failureBursts_;
    std::vector<FailureVisibleItem> failureVisibleItems_;
    StellarRng failureRng_;
    // Source12c8 during admitted Level2 destruction; ordinary ordinal stays frozen.
    std::optional<std::uint16_t> failureSourceLoopTick_;
    std::vector<Level1AudioEvent> failureAudioEvents_;
    bool failureFromLevel2_ = false;
    bool level2FailureTailFinished_ = false;
    bool level1FailureTailFinished_ = false;
    static constexpr unsigned beginnerContinues = 2;
    unsigned remainingContinues_ = beginnerContinues;
    ContinueChoice continueChoice_ = ContinueChoice::no;
    std::optional<ContinueChoice> pressedContinueChoice_;
    bool continueNavigationArmed_ = false;
    std::optional<ContinueDecision> recoveredContinueDecision_;
    struct PlayableHighscoreContinuation {
        PlayableHighscoreCaller caller;
        unsigned presentationFrame;
    };
    std::optional<PlayableHighscoreContinuation> playableHighscoreContinuation_;
    std::uint16_t level2LoopTick_ = 0;
    std::vector<Level1AudioEvent> level2AudioEvents_;
    std::int16_t level2PlayerArmor_ = 488;
    struct FuelBayRecovery {
        static constexpr std::int16_t stationType=31, maximumArmor=680, initialLift=10, liftStep=5;
        static constexpr std::uint16_t entryRange=700, duration=30, initialYaw=256, completedYaw=20;
        static constexpr std::uint16_t award=100, repairPayload=1, entryEvent=1, completeEvent=24;
        std::optional<std::size_t> fixedSlot;
        std::uint16_t frame = 0;
        std::uint16_t yawIncrement = initialYaw;
        std::int16_t liftIncrement = initialLift;
    } fuelBay_;
    Level1Entity level2Player_{};
    std::vector<Level1Entity> level2Entities_;
    std::optional<CampaignOrdinaryConstructor> level5Constructor_;
    std::uint32_t level5GuardianRadius_ = 0;
    std::uint8_t level5GuardianAnimationLimit_ = 0;
    std::uint8_t level3FixedAnimationLimit_ = 0;
    bool level3RadarFinished_ = false;
    std::vector<std::size_t> level2ActiveListOrder_;
    std::vector<Level1Entity> level2PresentationEntities_;
    std::vector<std::optional<CompletionBeamPresentation>> level2BeamPresentations_;
    std::vector<std::size_t> level2PresentationActiveListOrder_;
    std::vector<std::optional<std::size_t>> level2PresentationPoolSlots_;
    std::vector<std::optional<std::uint64_t>> level2PresentationActorIncarnations_;
    std::uint64_t level2PresentationAllocationRevision_ = 0;
    std::vector<std::size_t> level2PendingTerminalEffectSlots_;
    std::optional<std::size_t> liveLevel2PoolSlot(std::size_t index) const noexcept;
    void snapshotLevel2Presentation(std::vector<std::optional<CompletionBeamPresentation>> beams = {});
    std::vector<Level1Entity> level2SemanticEntities() const;
    std::vector<std::size_t> level2SemanticActiveListOrder() const;
    std::vector<Level1Entity>
        level2PendingTerminalEffectPresentations_;
    std::vector<Level1Entity> level2RetiredEffectSemanticEntities_;
    std::vector<std::size_t> level2RetiredEffectSemanticActiveOrder_;
    std::vector<std::size_t> level2RetiredEffectSemanticActivePositions_;
    std::vector<std::pair<std::size_t, std::size_t>>
        level2EffectSemanticActivePositionOverrides_;
    bool level2RetiredEffectRebuildReported_ = false;
    bool level2RetiredEffectSemanticOrderRebuilt_ = false;
    std::vector<std::size_t> level2AllocationListOrder_;
    VisibilityState level2Visibility_;
    struct StationaryPrefixGuards {
        std::uint16_t fadeTicks;
        bool specialActorPresent;
        std::array<bool, 2> effectHandles;
    };
    std::optional<StationaryPrefixGuards> level2PrefixGuards_;
    std::vector<std::size_t> level2ObjectListOrder_;
    static constexpr std::size_t level2ActorPoolSize_ = 16;
    //Disjoint from compact indices and the legacy null/retired sentinels.
    static constexpr std::size_t postWaveActorKeyBase_ = static_cast<std::size_t>(-1) - 2 * level2ActorPoolSize_;
    static constexpr std::size_t level2FixedObjectPoolSize_ = 16;
    static constexpr std::size_t level2ResetFixedObjectCount_ = 8;
    std::array<Level1Entity, level2ActorPoolSize_> level2ActorPool_{};
    std::array<bool, level2ActorPoolSize_> level2ActorPoolActive_{};
    std::array<std::uint64_t, level2ActorPoolSize_> level2ActorIncarnations_{};
    std::vector<std::size_t> level2EntityActorPoolSlots_;
    std::vector<std::size_t> level2EntityFixedPoolSlots_;
    std::array<bool, level2FixedObjectPoolSize_> level2FixedPoolEverUsed_{};
    std::array<Level1Entity, level2FixedObjectPoolSize_> level2FixedPool_{};
    std::array<bool, level2FixedObjectPoolSize_> level2FixedPoolKnown_{};
    std::uint64_t level2AllocationRevision_ = 0;
    std::uint64_t level2NonProjectileAllocationRevision_ = 0;
    std::uint64_t level2FixedAllocationRevision_ = 0;
    static constexpr std::size_t level2ProjectilePoolSize_ = 8;
    static constexpr std::size_t playerCannonSlotCount_ = 2;
    std::array<Level1Entity, level2ProjectilePoolSize_>
        level2ProjectilePool_{};
    std::array<bool, level2ProjectilePoolSize_>
        level2ProjectilePoolActive_{};
    std::array<bool, level2ProjectilePoolSize_>
        level2ProjectilePoolWasActiveAtBoundaryStart_{};
    std::vector<std::size_t> level2EntityProjectilePoolSlots_;
    static constexpr std::size_t level2DeathEffectPoolSize_ = 8;
    // Encoded retired FE contacts are disjoint from every compact live index.
    static constexpr auto retiredFixedContactBase_ = level2ActorPoolSize_ + level2FixedObjectPoolSize_
        + level2ProjectilePoolSize_ + level2DeathEffectPoolSize_;
    std::array<Level1Entity, level2DeathEffectPoolSize_>
        level2DeathEffectPool_{};
    std::array<bool, level2DeathEffectPoolSize_>
        level2DeathEffectPoolActive_{};
    std::array<bool, level2DeathEffectPoolSize_>
        level2DeathEffectPoolPendingPresenterRetirement_{};
    std::vector<std::size_t> level2EntityDeathEffectPoolSlots_;
    Level1GroundState level2Ground_;
    StellarRng level2Rng_;
    std::uint32_t level2CRandState_ = 1;
    // DS:3988/398a state used by FUN_1000:e078 for guardian presentation
    // timers. The exact Level 2/3 entry captures share this seed.
    std::uint32_t level2VisualRandomState_ = 0x187ebcaaU;
    GameplayInput level2PendingInput_{};
    bool level2HasAdvanced_ = false;
    bool level2TailConsumed_ = false;
    struct Level5SuccessContinuation {
        std::uint16_t sourceTick;
        std::size_t fixedSlot;
        std::uint64_t allocationRevision;
    };
    std::optional<Level5SuccessContinuation> level5SuccessContinuation_;
    struct Level2SuccessContinuation {
        std::uint16_t sourceTick;
        std::size_t pickupIndex;
        std::size_t fixedSlot;
        std::uint64_t allocationRevision;
        std::vector<std::size_t> order;
        std::vector<std::size_t> fixedSlots;
        std::vector<std::int16_t> types;
    };
    std::optional<Level2SuccessContinuation> level2SuccessContinuation_;
    bool level2SecondaryPickupKnownNull_ = false;
    // 49DB reset provenance; the admitted native controls cannot grant abilities/help.
    bool level5TransientStateKnownInactive_ = false;
    std::uint16_t campaignExitFadeTicks_ = 0;
    bool campaignSuccess_ = false;
    bool level2ArmorWarningActive_ = false;
    std::int16_t level2PlayerCollisionMomentum_ = 0;
    bool level2Type46ContactSlowdown_ = false;
    std::int16_t level2ActiveEnemyCount_ = 5;
    std::int16_t level2SpawnedEnemyCount_ = 5;
    std::uint32_t level2Score_ = 0;
    std::size_t level2CompletionObjectIndex_ =
        static_cast<std::size_t>(-1);
    std::size_t level2PostWaveObjectIndex_ =
        static_cast<std::size_t>(-1);
    std::size_t level2PickupIndex_ = static_cast<std::size_t>(-1);
    bool level2Complete_ = false;
    bool level3CompletionByPickup_ = false;
    std::size_t activeCampaignLevel_ = 1;
};

struct RgbFrame {
    int width;
    int height;
    std::vector<Rgb> pixels;
};

IndexedScreen renderGameplayIndexed(
    const GameplayAssets& assets,
    const GameplayModel& model
);
// World and sight only; retained cockpit/radar owners compose the other layers.
IndexedScreen renderLevel1ViewportIndexed(const GameplayAssets& assets, const GameplayModel& model);

using EffectProjector = std::function<std::optional<ProjectedPoint>(std::size_t, const ViewTransform&)>;
using BeamProjectionSink = std::function<void(std::size_t, const CompletionBeamPresentation&)>;
IndexedScreen renderLevel2ViewportIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const EffectProjector& effectProjector = {}, const BeamProjectionSink& beamProjection = {});
// Render a Level 3 frame over a previously displayed indexed page. The native
// guardian presenter performs partial redraws and retains stale world pixels;
// this entry point makes that framebuffer history explicit for interactive
// callers while the stateless function remains the analysis default.
IndexedScreen renderGameplayIndexedWithHistory(
    const GameplayAssets& assets,
    const GameplayModel& model,
    const IndexedScreen& previous
);

// Reconstruct one native Mode-X page update. The previous page is retained
// as the destination buffer, while the current viewport and presenters are
// redrawn into it. This is distinct from whole-viewport history preservation
// and exists for the verified double-buffer/page-copy investigation.
IndexedScreen renderGameplayIndexedWithPageHistory(
    const GameplayAssets& assets,
    const GameplayModel& model,
    const IndexedScreen& previous
);
unsigned level1SuccessPhaseOrdinal(unsigned presentationFrame) noexcept;
std::vector<Rgb> level1SuccessPalette(
    const std::vector<Rgb>& basePalette,
    unsigned loopOrdinal
);
IndexedScreen renderLevel1SuccessIndexed(
    const IndexedScreen& frozenFrame,
    unsigned presentationFrame
);
inline constexpr unsigned levelEntryFadeSteps = 16;
inline constexpr unsigned levelEntryFadeIntensityStep = 4;
IndexedScreen renderLevelEntryIndexed(
    const IndexedScreen& frozenFrame,
    const std::vector<std::uint8_t>& sourceDac,
    unsigned step
);
std::string level1FailureModelDrawTraceJson(
    const GameplayAssets& assets,
    const GameplayModel& model
);
std::string level1GameplayModelDrawTraceJson(
    const GameplayAssets& assets,
    const GameplayModel& model
);
std::string level2GameplayModelDrawTraceJson(
    const GameplayAssets& assets,
    const GameplayModel& model
);
// Target-local diagnostic for the Level 2/3 radar presenter. It reports the
// entity state and projected contact pixel without changing the renderer.
std::string level2RadarContactTraceJson(
    const GameplayModel& model
);
IndexedScreen renderFailurePageIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const IndexedScreen& destination, Viewport backgroundClip);
enum class ContinuePagePart { backdrop, dialog, yesWidget, noWidget, pointer };
IndexedScreen renderContinuePageIndexed(const GameplayAssets& assets, const GameplayModel& model,
    const IndexedScreen& destination, ContinuePagePart part);
RgbFrame renderGameplay(
    const GameplayAssets& assets,
    const GameplayModel& model
);

}  // namespace stellar7
