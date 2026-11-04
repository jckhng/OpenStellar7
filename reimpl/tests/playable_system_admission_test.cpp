#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"
#include "native_level2_frame_fixture.hpp"

#include <iostream>

namespace {
using namespace stellar7;
using test::requireCampaignComparison;

template<class Call> void rejects(Call call) {
    bool rejected = false;
    try { call(); } catch (const std::invalid_argument&) { rejected = true; }
    requireCampaignComparison(rejected,"source or malformed system action was admitted");
}

void sameLevel1(const GameplayModel& left, const GameplayModel& right) {
    const auto& a = left.level1State();
    const auto& b = right.level1State();
    requireCampaignComparison(a.loopTick() == b.loopTick() && a.activeListOrder() == b.activeListOrder()
        && a.semanticActiveListOrder() == b.semanticActiveListOrder() && a.ground().points() == b.ground().points()
        && a.playerWeaponCooldown() == b.playerWeaponCooldown() && a.playerProjectileTimers() == b.playerProjectileTimers()
        && a.audioEvents().size() == b.audioEvents().size() && a.entities().size() == b.entities().size()
        && a.systemStatusLevels() == b.systemStatusLevels() && a.systemTimers() == b.systemTimers()
        && a.selectedSystem() == b.selectedSystem() && a.missionActive() == b.missionActive()
        && a.failureState() == b.failureState() && a.successState() == b.successState()
        && a.activeEnemyCount() == b.activeEnemyCount() && a.spawnedEnemyCount() == b.spawnedEnemyCount()
        && test::campaignActorFields(a.player()) == test::campaignActorFields(b.player()),
        "rejected action changed its retained source Level1 owner");
    for (std::size_t index = 0; index < a.entities().size(); ++index) {
        requireCampaignComparison(test::campaignActorFields(a.entities()[index]) == test::campaignActorFields(b.entities()[index]),
            "rejected action changed a source Level1 actor");
    }
    test::compareCampaignRng(a.rng(),b.rng(),test::CampaignComparisonStage::boundary);
}

void sameSession(const NativeGameplaySession& left, const NativeGameplaySession& right) {
    test::CampaignBudgetComparison comparison;
    comparison.compare(left,right,{}, {},test::CampaignComparisonStage::boundary);
    sameLevel1(left.model(),right.model());
    requireCampaignComparison(left.playableCampaign() == right.playableCampaign()
        && left.recoveredClockPhase()->playableCampaign == right.recoveredClockPhase()->playableCampaign,
        "rejected action adopted the playable device domain");
}

std::unique_ptr<RecoveredFrontend> begin(const GameplayAssets& assets, const NativeStartupAssets& startup,
    const FrameCopyProfile copy, const CampaignExecution execution) {
    auto frontend = std::make_unique<RecoveredFrontend>(assets,startup,copy,10000,
        std::shared_ptr<HighscoreStore>{},execution);
    unsigned polls = 0;
    for (unsigned call = 0; !frontend->session() && call < 10000; ++call) {
        NativeMenuInput input{};
        if (frontend->startup().phase() == NativeStartupPhase::awaitingMenuDecision) { input.confirm = polls++ == 1; }
        frontend->advanceStartup(input);
    }
    requireCampaignComparison(frontend->session() != nullptr,"system admission startup did not reach actual Begin");
    return frontend;
}

std::unique_ptr<NativeGameplaySession> sourceLevel1(const GameplayAssets& assets, const NativeStartupAssets& resources) {
    NativeStartupSequence startup(assets,resources);
    startup.start(NativeStartupExecution::recoveredContext);
    startup.initializeSettings(StartupCapabilityProfile::noMouseOrJoystick);
    startup.initializeResources(NativeStartupDeviceProfile::portableOplNoDspProbe);
    startup.initializePresentation();
    startup.initializeFonts();
    while (startup.phase() == NativeStartupPhase::logoCountdownWork) { startup.pollLogoCountdown(10000); }
    startup.playLogoEvent(); startup.animateLogo(); startup.finishLogoSound(); startup.releaseLogo();
    startup.skipIntro(NativeIntroKey::escape); startup.beginMenuSong(); startup.initializeMenuResources();
    startup.skipCredits(NativeIntroKey::escape); startup.enterMenu(NativeMenuEntryProfile::keyboardOnly);
    startup.beginGame(NativeMenuBeginProfile::enterPressRelease);
    auto session = std::make_unique<NativeGameplaySession>(assets,startup.releaseGameplay());
    session->initializeRecoveredLevel1();
    return session;
}

void run(const std::filesystem::path& root) {
    const auto assets = naturalRoute::assets(root);
    const auto startup = decodeNativeStartupAssets(naturalRoute::read(root / "STELART.RES"));
    const auto drivers = decodeTaggedPayloads(naturalRoute::read(root / "STELLAR.RES"),{'V','G','A',':'});
    requireCampaignComparison(drivers.size() == 1,"missing actual VGA driver");
    const auto copy = decodeFrameCopyProfile(drivers.front());

    auto source = sourceLevel1(assets,startup);
    auto untouchedSource = sourceLevel1(assets,startup);
    auto frontend = begin(assets,startup,copy,CampaignExecution::sourceWork);
    auto untouchedFrontend = begin(assets,startup,copy,CampaignExecution::sourceWork);
    auto level2 = test::makeRecoveredLevel2PreloopSession(root);
    auto untouchedLevel2 = test::makeRecoveredLevel2PreloopSession(root);
    auto retained = std::make_unique<GameplayModel>();
    const auto untouchedRetained = std::make_unique<GameplayModel>(*retained);
    for (const auto action : {GameplaySystemAction::jump,GameplaySystemAction::activateSelected,
        GameplaySystemAction::shield,GameplaySystemAction::scanner,GameplaySystemAction::superPrimary,
        GameplaySystemAction::turbo,GameplaySystemAction::cycleSelected,GameplaySystemAction::decoy,
        static_cast<GameplaySystemAction>(255)}) {
        GameplayInput input{};
        input.systemAction = action;
        input.left = true; //Verify rejected pulses cannot carry supported arrow work through admission.
        rejects([&] { source->advanceRecoveredLevel1Frame(copy,10000,input); });
        sameSession(*source,*untouchedSource);
        rejects([&] { frontend->advanceGameplay(input); });
        sameSession(*frontend->session(),*untouchedFrontend->session());
        rejects([&] { level2->advanceRecoveredLevel2Frame(copy,10000,input); });
        sameSession(*level2,*untouchedLevel2);
        rejects([&] { retained->tickRetainedLevel1(input); });
        requireCampaignComparison(retained->ticks() == untouchedRetained->ticks(),"retained rejection advanced a tick");
        sameLevel1(*retained,*untouchedRetained);
    }
    //Level2's retained ordinary entry, separate from its neutral preloop admission.
    level2->advanceRecoveredLevel2Frame(copy,10000,{});
    untouchedLevel2->advanceRecoveredLevel2Frame(copy,10000,{});
    GameplayInput jump{};
    jump.systemAction = GameplaySystemAction::jump;
    jump.forward = true;
    rejects([&] { level2->advanceRecoveredLevel2Frame(copy,10000,jump); });
    sameSession(*level2,*untouchedLevel2);

    auto playableLevel2 = test::makePlayableLevel2Session(root);
    playableLevel2->advancePlayableCampaign(jump);
    requireCampaignComparison(playableLevel2->model().level1State().systemTimers()[2] == 220
        && playableLevel2->model().level1State().systemStatusLevels()[2] == -2,
        "actual retained Level2 native frame rejected its explicit Jump action");
    playableLevel2->advancePlayableCampaign({});
    requireCampaignComparison(playableLevel2->model().level2Player().position.z == 220,
        "actual retained Level2 lost its adjacent airborne frame");

    auto playable = begin(assets,startup,copy,CampaignExecution::playable);
    auto untouchedPlayable = begin(assets,startup,copy,CampaignExecution::playable);
    requireCampaignComparison(!playable->session()->playableCampaign(),"malformed-enum test already adopted");
    GameplayInput malformed{};
    malformed.systemAction = static_cast<GameplaySystemAction>(255);
    rejects([&] { playable->advanceGameplay(malformed); });
    sameSession(*playable->session(),*untouchedPlayable->session());
    std::cout << "Public source native/frontend and retained-model queued-action rejection; initial malformed playable action atomic PASS\n";
}
}

int main(int argc,char** argv) {
    if (argc != 2) { return 2; }
    try { run(argv[1]); return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
