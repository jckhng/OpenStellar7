#include "stellar7/gameplay.hpp"
#include "stellar7/ending_scene.hpp"

#include <stdexcept>
#include <utility>

namespace stellar7 {

void GameplayModel::initializePlayableDifficulty(const CampaignDifficulty difficulty) {
    const auto charges=campaignInitialCharges(difficulty);
    const auto continues=campaignContinueBudget(difficulty);
    if (playableCampaignFrames_ || phase_!=GameplayPhase::level1 || ticks_!=0
        || level1_.loopTick_!=0 || level1_.hasAdvanced_) {
        throw std::invalid_argument("difficulty selection requires the actual fresh playable Begin owner");
    }
    runtimeDifficulty_=difficulty;
    remainingContinues_=continues;
    level1_.systemStatusLevels_=charges;
}

std::uint16_t GameplayModel::scaledCampaignAward(const std::uint16_t award) const {
    return playableCampaignFrames_ ? campaignScoreAward(award,runtimeDifficulty_) : award;
}

void GameplayModel::advancePlayableFailure() {
    if (!playableCampaignFrames_ || !failureFromLevel2_
        || (phase_!=GameplayPhase::failurePreExplosionBlackout
            && phase_!=GameplayPhase::failureExplosion
            && phase_!=GameplayPhase::failurePostExplosionBlackout)) {
        throw std::invalid_argument("playable destruction requires its retained lethal campaign frame");
    }
    tick({});
    if (phase_==GameplayPhase::continuePrompt) {
        recoveredContinueDecision_=ContinueDecision::pending;
    }
}

ContinueDecision GameplayModel::pollPlayableContinue(const GameplayInput& input,
    const std::optional<ContinuePointerInput>& pointer) {
    if (!playableCampaignFrames_ || !failureFromLevel2_ || phase_!=GameplayPhase::continuePrompt
        || !recoveredContinueDecision_) {
        throw std::invalid_argument("playable Continue requires its entered retained prompt");
    }
    if (*recoveredContinueDecision_!=ContinueDecision::pending) {
        return *recoveredContinueDecision_;
    }
    if(pointer&&pointer->button==ContinuePointerButton::cancel) {
        pressedContinueChoice_.reset();
        return *recoveredContinueDecision_;
    }
    if (pointer && pointer->button == ContinuePointerButton::released
        && !input.forward && !input.reverse && !input.left && !input.right) {
        continueNavigationArmed_ = true;
    }
    if(pointer&&!input.exit) {
        const bool held=pointer->button==ContinuePointerButton::held;
        if(!pressedContinueChoice_) {
            if(held) {pressedContinueChoice_=pointer->hit;}
        } else if(!held) {
            const auto activated=*pressedContinueChoice_;
            pressedContinueChoice_.reset();
            recoveredContinueDecision_=activated==ContinueChoice::yes ? ContinueDecision::yes : ContinueDecision::no;
        } else if(pointer->hit!=pressedContinueChoice_) {
            pressedContinueChoice_.reset();
        }
    } else {
        recoveredContinueDecision_=updateContinueChoice(input);
    }
    if (*recoveredContinueDecision_==ContinueDecision::no
        || *recoveredContinueDecision_==ContinueDecision::cancelled) {
        phase_=GameplayPhase::campaignEnded;
    }
    return *recoveredContinueDecision_;
}

void GameplayModel::completePlayableCampaignRetry(const std::vector<Model3d>& shared,
    const std::vector<Model3d>& local) {
    if (!playableCampaignFrames_ || !failureFromLevel2_ || phase_!=GameplayPhase::continuePrompt
        || recoveredContinueDecision_!=ContinueDecision::yes || remainingContinues_==0
        || activeCampaignLevel_>=7 || campaignModelRadius(shared,local,0)==0
        || (playableHighscoreContinuation_ && playableHighscoreContinuation_->caller!=PlayableHighscoreCaller::retry)) {
        throw std::invalid_argument("playable retry requires its affirmative same-level Continue return");
    }
    auto rng=failureRng_;
    const auto destination=static_cast<std::uint8_t>(activeCampaignLevel_);
    auto actors=constructCampaignLoaderActors(campaignLevelDefinition(destination,campaignDifficultyIndex(runtimeDifficulty_)).table,rng,shared,local);
    initializeRetainedCampaignLevel(destination,std::move(actors),rng,shared,local);
    constexpr std::int16_t beginnerRetryArmor=680;
    level2PlayerArmor_=beginnerRetryArmor;
    level2Score_=0;
    --remainingContinues_;
    level1_.resetCampaignTransientState();
    pressedContinueChoice_.reset();
    recoveredContinueDecision_.reset();
    playableHighscoreContinuation_.reset();
    travel_=0;
    weaponIndicatorVisible_=false;
    weaponReadyIndicator_=false;
    explosionTicksRemaining_=0;
    explosionAge_=0;
    failureAudioEvents_.clear();
    failureSourceLoopTick_.reset();
    //The explicit provider owns loader/preloop even when reloading index1/2.
    phase_=GameplayPhase::playableLoaderReturn;
}

std::optional<PlayableHighscoreCaller> GameplayModel::playableHighscoreCaller() const noexcept {
    return playableHighscoreContinuation_ ? std::optional{playableHighscoreContinuation_->caller} : std::nullopt;
}

void GameplayModel::beginPlayableHighscoreNameEntry() {
    if (!playableCampaignFrames_ || playableHighscoreContinuation_) {
        throw std::invalid_argument("playable score entry requires an unconsumed campaign caller");
    }
    PlayableHighscoreCaller caller;
    if (phase_==GameplayPhase::continuePrompt && recoveredContinueDecision_==ContinueDecision::yes) {
        caller=PlayableHighscoreCaller::retry;
    } else if (phase_==GameplayPhase::endingReturn) {
        validateTitleReturn();
        caller=PlayableHighscoreCaller::endingTitle;
    } else if (phase_==GameplayPhase::campaignEnded && failureFromLevel2_
        && (recoveredContinueDecision_==ContinueDecision::no || recoveredContinueDecision_==ContinueDecision::cancelled
            || (remainingContinues_==0 && !recoveredContinueDecision_))) {
        caller=PlayableHighscoreCaller::gameOverTitle;
    } else {
        throw std::invalid_argument("playable score entry lost its330A caller");
    }
    playableHighscoreContinuation_=PlayableHighscoreContinuation{caller,presentationFrame_};
    phase_=GameplayPhase::highscoreNameEntry;
    presentationFrame_=0;
}

void GameplayModel::beginPlayableHighscoreHall() {
    if (!playableCampaignFrames_ || phase_!=GameplayPhase::highscoreNameEntry || !playableHighscoreContinuation_) {
        throw std::invalid_argument("playable score hall requires its completed name entry");
    }
    phase_=GameplayPhase::highscoreHall;
    presentationFrame_=0;
}

void GameplayModel::finishPlayableHighscore() {
    if (!playableCampaignFrames_ || (phase_!=GameplayPhase::highscoreNameEntry
            && phase_!=GameplayPhase::highscoreHall) || !playableHighscoreContinuation_) {
        throw std::invalid_argument("playable score return requires its completed modal child");
    }
    const auto& parent=*playableHighscoreContinuation_;
    phase_=parent.caller==PlayableHighscoreCaller::retry ? GameplayPhase::continuePrompt
        : parent.caller==PlayableHighscoreCaller::endingTitle ? GameplayPhase::endingReturn : GameplayPhase::campaignEnded;
    presentationFrame_=parent.presentationFrame;
}

void GameplayModel::finishPlayableTitleReturn() {
    if (!playableCampaignFrames_) {
        throw std::invalid_argument("playable title requires its retained campaign owner");
    }
    if (phase_==GameplayPhase::endingReturn) {
        validateTitleReturn();
    } else if (phase_==GameplayPhase::campaignEnded && failureFromLevel2_) {
        //The actual parent publishes destruction RNG after330A returns.
    } else {
        throw std::invalid_argument("playable title requires completed ending or game over");
    }
    if (playableHighscoreContinuation_
        && ((phase_==GameplayPhase::endingReturn && playableHighscoreContinuation_->caller!=PlayableHighscoreCaller::endingTitle)
            || (phase_==GameplayPhase::campaignEnded && playableHighscoreContinuation_->caller!=PlayableHighscoreCaller::gameOverTitle))) {
        throw std::invalid_argument("playable title lost its completed330A caller");
    }
    if (phase_==GameplayPhase::campaignEnded) {level2Rng_=failureRng_;}
    playableHighscoreContinuation_.reset();
    phase_=GameplayPhase::nativeMenu;
    presentationFrame_=0;
    ++ticks_;
}

void GameplayModel::preparePlayableMenuBegin(const CampaignDifficulty difficulty) {
    if (!playableCampaignFrames_ || phase_!=GameplayPhase::nativeMenu) {
        throw std::invalid_argument("playable Begin requires its retained menu owner");
    }
    const auto charges=campaignInitialCharges(difficulty);
    const auto continues=campaignContinueBudget(difficulty);
    runtimeDifficulty_=difficulty;
    remainingContinues_=continues;
    level1_.systemStatusLevels_=charges;
    level1_.score_=0;
    level2Score_=0;
    phase_=GameplayPhase::nativeBeginWait;
    presentationFrame_=0;
}

void GameplayModel::completePlayableMenuBegin(const std::vector<Model3d>& shared,
    const std::vector<Model3d>& local) {
    if (!playableCampaignFrames_ || phase_!=GameplayPhase::nativeBeginWait
        || campaignModelRadius(shared,local,0)==0) {
        throw std::invalid_argument("playable Begin loader requires its completed menu wait and Level1 banks");
    }
    auto rng=level2Rng_;
    auto actors=constructCampaignLoaderActors(campaignLevelDefinition(0,campaignDifficultyIndex(runtimeDifficulty_)).table,rng,shared,local);
    initializeRetainedCampaignLevel(0,std::move(actors),rng,shared,local);
    level2PlayerArmor_=680;
    level1_.resetCampaignTransientState();
    failureAudioEvents_.clear();
    failureSourceLoopTick_.reset();
    recoveredContinueDecision_.reset();
    playableHighscoreContinuation_.reset();
    travel_=0;
    weaponIndicatorVisible_=false;
    weaponReadyIndicator_=false;
}

void GameplayModel::completePlayableCampaignEnding() {
    if (pendingPlayableCampaignDestination()<7 || level2PlayerArmor_<0) {
        throw std::invalid_argument("playable ending requires its completed final campaign exit");
    }
    level5Constructor_.reset();
    playableOrdinaryConstructor_.reset();
    campaignExitFadeTicks_=1;
    level2PlayerCollisionMomentum_=0;
    activeCampaignLevel_=7;
    campaignSuccess_=true;
    phase_=GameplayPhase::endingEntry;
    presentationFrame_=0;
    ++ticks_;
}

void GameplayModel::enterPlayableGuardianVictory() {
    if (!playableCampaignFrames_ || activeCampaignLevel_!=6 || level2PlayerArmor_<0) {
        throw std::invalid_argument("7512 requires the retained final guardian defeat");
    }
    playableOrdinaryConstructor_.reset();
    level2Complete_=true;
    level2TailConsumed_=true;
    campaignExitFadeTicks_=1;
    campaignSuccess_=true;
    activeCampaignLevel_=7;
    phase_=GameplayPhase::endingEntry;
    presentationFrame_=0;
}

void GameplayModel::validateCampaignSuccessCleanup() const {
    if (phase_ != GameplayPhase::level5SuccessReturn || activeCampaignLevel_ != 6
        || !level2TailConsumed_ || level5SuccessContinuation_ || !level5TransientStateKnownInactive_
        || !successAudioOwned_ || successAudioActive_ || level2PlayerArmor_ < 0) {
        throw std::invalid_argument("campaign success cleanup requires the retained neutral Level5 return");
    }
}

void GameplayModel::finishCampaignSuccessCleanup() {
    validateCampaignSuccessCleanup();
    level5Constructor_.reset();
    level5GuardianRadius_ = 0;
    level5GuardianAnimationLimit_ = 0;
    campaignExitFadeTicks_ = 1; //0469's132C persists beyond the released level resources.
    level2PlayerCollisionMomentum_ = 0;
    ++activeCampaignLevel_;
    campaignSuccess_ = true;
    phase_ = GameplayPhase::endingEntry;
    presentationFrame_ = 0;
    ++ticks_;
}

void GameplayModel::validateEndingFirstPage() const {
    if (phase_ != GameplayPhase::endingEntry || activeCampaignLevel_ != 7
        || !campaignSuccess_ || presentationFrame_ != 0) {
        throw std::invalid_argument("ending first page requires its retained campaign entry");
    }
}

void GameplayModel::finishEndingFirstPage() {
    validateEndingFirstPage();
    phase_ = GameplayPhase::endingPresentation;
    presentationFrame_ = 1;
    ++ticks_;
}

void GameplayModel::validateEndingPage() const {
    if (phase_ != GameplayPhase::endingPresentation || activeCampaignLevel_ != 7
        || !campaignSuccess_ || presentationFrame_ == 0 || presentationFrame_ > endingScenePageCount) {
        throw std::invalid_argument("ending page requires its retained successful scene and next-page cursor");
    }
}

void GameplayModel::finishEndingPage(const EndingPageResult result) {
    validateEndingPage();
    if ((result == EndingPageResult::present) != (presentationFrame_ < endingScenePageCount)) {
        throw std::invalid_argument("ending page result disagrees with its retained SCENED cursor");
    }
    if (result == EndingPageResult::exhausted) {
        phase_ = GameplayPhase::endingTeardown;
    }
    ++presentationFrame_;
    ++ticks_;
}

void GameplayModel::validateEndingTeardown() const {
    if (phase_ != GameplayPhase::endingTeardown || activeCampaignLevel_ != 7
        || !campaignSuccess_ || presentationFrame_ != endingScenePageCount + 1) {
        throw std::invalid_argument("ending teardown requires the retained normal SCENED return");
    }
}

void GameplayModel::finishEndingTeardown() {
    validateEndingTeardown();
    phase_ = GameplayPhase::endingReturn;
    ++ticks_;
}

void GameplayModel::validateTitleReturn() const {
    if (phase_ != GameplayPhase::endingReturn || activeCampaignLevel_ != 7
        || !campaignSuccess_ || presentationFrame_ != endingScenePageCount + 1) {
        throw std::invalid_argument("title return requires retained successful SCENED teardown");
    }
}

void GameplayModel::finishTitleReturn() {
    validateTitleReturn();
    phase_ = GameplayPhase::nativeMenu;
    presentationFrame_ = 0;
    ++ticks_;
}

void GameplayModel::finishMenuPoll() {
    if (phase_ != GameplayPhase::nativeMenu) {
        throw std::invalid_argument("menu poll requires its retained native owner");
    }
    ++presentationFrame_;
    ++ticks_;
}

void GameplayModel::validateMenuBegin() const {
    if (phase_ != GameplayPhase::nativeMenu || activeCampaignLevel_ != 7
        || !campaignSuccess_) {
        throw std::invalid_argument("Begin requires the retained successful native menu");
    }
}

void GameplayModel::prepareMenuBegin() {
    validateMenuBegin();
    // 0168/0175: the admitted difficulty-zero profile precedes the 480-IRQ wait.
    constexpr unsigned beginnerBudget = 2;
    constexpr std::int8_t beginnerStatus = 3;
    remainingContinues_ = beginnerBudget;
    level1_.systemStatusLevels_.fill(beginnerStatus);
    level1_.score_ = 0;
    level2Score_ = 0;
    phase_ = GameplayPhase::nativeBeginWait;
}

void GameplayModel::finishMenuBegin() {
    if (phase_ != GameplayPhase::nativeBeginWait || activeCampaignLevel_ != 7
        || !campaignSuccess_) {
        throw std::invalid_argument("Begin loader requires its retained menu wait");
    }

    // 48d4 clears the shared pools while retaining the upper fixed records.
    level2Entities_.clear();
    initializeCampaignPools(0, 0);
    level2Player_ = {};
    level2PlayerArmor_ = 680;
    level2ActiveEnemyCount_ = 2;
    level2SpawnedEnemyCount_ = 2;
    level2CompletionObjectIndex_ = static_cast<std::size_t>(-1);
    level2PostWaveObjectIndex_ = static_cast<std::size_t>(-1);
    level2PickupIndex_ = static_cast<std::size_t>(-1);
    level2Complete_ = false;
    level3CompletionByPickup_ = false;
    level2PrefixGuards_.reset();
    level2PendingInput_ = {};
    level2HasAdvanced_ = false;
    level2PlayerCollisionMomentum_ = 0;
    level2Type46ContactSlowdown_ = false;
    level5Constructor_.reset();
    level5GuardianRadius_ = 0;
    level5GuardianAnimationLimit_ = 0;

    // 4e06 constructs the new Level 1 actors and ground from the live shared RNG.
    level1_.reloadAfterFailure(level2Rng_);
    level2Rng_ = level1_.rng();
    travel_ = 0;
    weaponIndicatorVisible_ = false;
    weaponReadyIndicator_ = false;
    explosionTicksRemaining_ = 0;
    explosionAge_ = 0;
    damageFlashAge_ = 0;
    damageFlashActive_ = false;
    failureView_ = {};
    failurePlayer_ = {};
    failureMissionEntities_.clear();
    failureDebris_.clear();
    failureBursts_.clear();
    failureVisibleItems_.clear();
    failureAudioEvents_.clear();
    failureSourceLoopTick_.reset();
    failureFromLevel2_ = false;
    level2FailureTailFinished_ = false;
    successAudioOwned_ = false;
    successAudioActive_ = false;
    continueChoice_ = ContinueChoice::no;
    pressedContinueChoice_.reset();
    continueNavigationArmed_ = false;

    // 50c4 draws twice before the first ordinary display advances the oscillator.
    constexpr std::size_t loaderDraws = 2;
    lastCockpitInitialization_.levelIndex = 0;
    lastCockpitInitialization_.displaySteps = static_cast<std::uint8_t>(loaderDraws);
    lastCockpitInitialization_.states[0] = {cockpitIndicatorX_, cockpitIndicatorVelocity_};
    for (std::size_t index = 1; index <= loaderDraws; ++index) {
        advanceCockpitIndicator();
        lastCockpitInitialization_.states[index] = {cockpitIndicatorX_, cockpitIndicatorVelocity_};
    }
    lastCockpitInitialization_.states.back() = lastCockpitInitialization_.states[loaderDraws];
    phase_ = GameplayPhase::level1BeginHandoff;
    presentationFrame_ = 0;
}

} // namespace stellar7
