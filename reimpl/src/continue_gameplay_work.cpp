#include "stellar7/gameplay.hpp"

#include <stdexcept>

namespace stellar7 {

void GameplayModel::initializeContinuePrompt() noexcept {
    phase_ = remainingContinues_ == 0
        ? GameplayPhase::campaignEnded : GameplayPhase::continuePrompt;
    presentationFrame_ = 0;
    continueChoice_ = ContinueChoice::no;
    pressedContinueChoice_.reset();
    continueNavigationArmed_ = false;
    recoveredContinueDecision_.reset();
}

ContinueDecision GameplayModel::updateContinueChoice(const GameplayInput& input) {
    if (input.exit) {
        pressedContinueChoice_.reset();
        return ContinueDecision::cancelled;
    }
    const bool direction = input.forward || input.reverse || input.left || input.right;
    if (!direction) {
        continueNavigationArmed_ = true;
    } else if (continueNavigationArmed_) {
        //853A links both vertical directions to the other widget.
        if (input.forward || input.reverse) {
            continueChoice_ = continueChoice_ == ContinueChoice::yes
                ? ContinueChoice::no : ContinueChoice::yes;
        }
        continueNavigationArmed_ = false;
    }
    const bool activate = input.confirm || input.fire;
    if (pressedContinueChoice_) {
        if (!activate) {
            const auto decision = *pressedContinueChoice_;
            pressedContinueChoice_.reset();
            return decision == ContinueChoice::yes ? ContinueDecision::yes : ContinueDecision::no;
        }
        if (*pressedContinueChoice_ != continueChoice_) {
            pressedContinueChoice_.reset();
        }
        return ContinueDecision::pending;
    }
    if (activate) {
        pressedContinueChoice_ = continueChoice_;
    }
    return ContinueDecision::pending;
}

void GameplayModel::enterRecoveredContinue() {
    const bool retainedLevel = failureFromLevel2_ ? recoveredLevel2ContinueReady() : level1_.levelIndex_ == 0;
    if (phase_ != GameplayPhase::failurePostExplosionBlackout || !retainedLevel || remainingContinues_ == 0) {
        throw std::invalid_argument("recovered Continue requires its completed campaign destruction and a retry");
    }
    initializeContinuePrompt();
    recoveredContinueDecision_ = ContinueDecision::pending;
}

ContinueDecision GameplayModel::pollRecoveredContinue(const GameplayInput& input) {
    const bool retainedLevel = failureFromLevel2_
        ? recoveredLevel2ContinueReady() && remainingContinues_ != 0 : level1_.levelIndex_ == 0;
    if (phase_ != GameplayPhase::continuePrompt || !retainedLevel || !recoveredContinueDecision_) {
        throw std::invalid_argument("recovered Continue input requires its entered UI owner");
    }
    if (*recoveredContinueDecision_ == ContinueDecision::pending) {
        recoveredContinueDecision_ = updateContinueChoice(input);
    }
    return *recoveredContinueDecision_;
}

ContinueDecision GameplayModel::recoveredContinueDecision() const noexcept {
    return recoveredContinueDecision_.value_or(ContinueDecision::pending);
}

void GameplayModel::resumeRecoveredContinueRetry() {
    if (phase_ != GameplayPhase::continuePrompt || failureFromLevel2_
        || level1_.levelIndex_ != 0 || recoveredContinueDecision_ != ContinueDecision::yes) {
        throw std::invalid_argument("recovered Continue retry requires its retained affirmative decision");
    }
    const auto lampVisible = weaponIndicatorVisible_;
    const auto lampReady = weaponReadyIndicator_;
    retryLevel1();
    //13B6 survives040F; only the ordinary7BAE presenter replaces its dirty-state value.
    weaponIndicatorVisible_ = lampVisible;
    weaponReadyIndicator_ = lampReady;
    //50AC leaves45D2 empty;3768 builds it after the loader without another constructor/RNG pass.
    level1_.objectListOrder_.clear();
    level1_.activeListOrder_.clear();
    level1_.semanticActiveListOrder_.clear();
    level1_.nativeInitialListPending_ = true;
    recoveredLevel1TickBase_ = ticks_;
    recoveredContinueDecision_.reset();
}

void GameplayModel::completeRecoveredLevel1Retry() {
    if (phase_ != GameplayPhase::level1RetryHandoff || failureFromLevel2_ || level1_.levelIndex_ != 0
        || level1_.loopTick_ != 0 || level1_.hasAdvanced_ || !level1_.nativeInitialListPending_
        || !level1_.objectListOrder_.empty() || !level1_.activeListOrder_.empty()
        || !level1_.semanticActiveListOrder_.empty() || recoveredLevel1TickBase_ != ticks_
        || recoveredContinueDecision_) {
        throw std::invalid_argument("recovered Level1 retry return requires its completed loader boundary");
    }
    phase_ = GameplayPhase::level1;
}

} // namespace stellar7
