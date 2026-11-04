#include "stellar7/gameplay.hpp"

#include <array>
#include <cassert>
#include <memory>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void assertAllocationRegistry(const Level1State& state) {
        assert(state.allocationList_.size() == 6);
        for (std::size_t index = 0; index < state.allocationList_.size(); ++index) {
            assert(state.allocationList_[index].kind == Level1State::ObjectKind::entity);
            assert(state.allocationList_[index].index == index);
        }
    }
    static void assertRecoveredLists(const Level1State& state) {
        assertAllocationRegistry(state);
        assert(state.objectListOrder_.empty() && state.activeListOrder_.empty());
        assert(state.semanticActiveListOrder_.empty() && state.nativeInitialListPending_);
    }
    static void assertLegacyLists(const Level1State& state) {
        assertAllocationRegistry(state);
        assert(state.objectListOrder_.size() == 6 && state.activeListOrder_.size() == 6);
        assert(state.semanticActiveListOrder_.size() == 6 && !state.nativeInitialListPending_);
    }
};
struct GameplayModelTestAccess {
    static void postBlackout(GameplayModel& model) {
        model.phase_ = GameplayPhase::failurePostExplosionBlackout;
        model.failureFromLevel2_ = false;
        model.presentationFrame_ = 17;
        model.ticks_ = 37;
        model.continueChoice_ = ContinueChoice::yes;
        model.pressedContinueChoice_ = ContinueChoice::yes;
        model.continueNavigationArmed_ = true;
        model.failureRng_ = StellarRng{};
        for (unsigned index = 0; index < 32; ++index) { model.failureRng_.next(); }
    }
    static void level2(GameplayModel& model) { model.failureFromLevel2_ = true; }
    static void exhaust(GameplayModel& model) { model.remainingContinues_ = 0; }
};
}

namespace {
using namespace stellar7;

template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    assert(rejected);
}

std::unique_ptr<GameplayModel> postBlackout() {
    auto model = std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    GameplayModelTestAccess::postBlackout(*model);
    return model;
}

GameplayInput vertical() { GameplayInput input; input.forward = true; return input; }
GameplayInput confirm() { GameplayInput input; input.confirm = true; return input; }

void assertUiEqual(const GameplayModel& first, const GameplayModel& second) {
    assert(first.continueChoice() == second.continueChoice());
    assert(first.pressedContinueChoice() == second.pressedContinueChoice());
}

void assertRngEqual(const StellarRng& first, const StellarRng& second) {
    assert(first.indexA() == second.indexA() && first.indexB() == second.indexB());
    assert(first.last() == second.last() && first.calls() == second.calls());
    auto firstCopy = first;
    auto secondCopy = second;
    for (std::size_t index = 0; index < StellarRng::wordCount; ++index) {
        assert(firstCopy.next() == secondCopy.next());
    }
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    auto ordinary = std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    rejects([&] { ordinary->enterRecoveredContinue(); });
    rejects([&] { ordinary->pollRecoveredContinue({}); });
    rejects([&] { ordinary->resumeRecoveredContinueRetry(); });
    assert(ordinary->phase() == GameplayPhase::level1 && ordinary->ticks() == 0);

    auto exhausted = postBlackout();
    GameplayModelTestAccess::exhaust(*exhausted);
    rejects([&] { exhausted->enterRecoveredContinue(); });
    assert(exhausted->phase() == GameplayPhase::failurePostExplosionBlackout && exhausted->ticks() == 37);
    exhausted->tick({});
    assert(exhausted->phase() == GameplayPhase::campaignEnded); //Legacy zero-budget policy is unchanged.
    auto level2 = postBlackout();
    GameplayModelTestAccess::level2(*level2);
    rejects([&] { level2->enterRecoveredContinue(); });
    assert(level2->phase() == GameplayPhase::failurePostExplosionBlackout);

    auto model = postBlackout();
    const auto ticks = model->ticks();
    const auto failureRng = model->failureRng();
    const auto ordinaryRng = model->level1State().rng();
    const auto loop = model->level1State().loopTick();
    model->enterRecoveredContinue();
    assert(model->phase() == GameplayPhase::continuePrompt && model->presentationFrame() == 0);
    assert(model->continueChoice() == ContinueChoice::no && !model->pressedContinueChoice());
    assert(model->recoveredContinueDecision() == ContinueDecision::pending);
    rejects([&] { model->enterRecoveredContinue(); });
    rejects([&] { model->resumeRecoveredContinueRetry(); });
    assert(model->pollRecoveredContinue(vertical()) == ContinueDecision::pending);
    assert(model->continueChoice() == ContinueChoice::no); //Inherited held direction must release first.
    assert(model->pollRecoveredContinue(vertical()) == ContinueDecision::pending);
    assert(model->pollRecoveredContinue({}) == ContinueDecision::pending);
    GameplayInput horizontal; horizontal.right = true;
    assert(model->pollRecoveredContinue(horizontal) == ContinueDecision::pending);
    assert(model->pollRecoveredContinue(vertical()) == ContinueDecision::pending);
    assert(model->continueChoice() == ContinueChoice::no); //Horizontal consumes the same navigation latch.
    assert(model->pollRecoveredContinue({}) == ContinueDecision::pending);
    assert(model->pollRecoveredContinue(vertical()) == ContinueDecision::pending);
    assert(model->continueChoice() == ContinueChoice::yes);
    assert(model->pollRecoveredContinue(vertical()) == ContinueDecision::pending);
    assert(model->continueChoice() == ContinueChoice::yes);
    assert(model->pollRecoveredContinue(confirm()) == ContinueDecision::pending);
    assert(model->pressedContinueChoice() == ContinueChoice::yes);
    auto changedWhilePressed = confirm(); changedWhilePressed.reverse = true;
    assert(model->pollRecoveredContinue(changedWhilePressed) == ContinueDecision::pending);
    assert(model->continueChoice() == ContinueChoice::no && !model->pressedContinueChoice());
    assert(model->pollRecoveredContinue(confirm()) == ContinueDecision::pending);
    assert(model->pressedContinueChoice() == ContinueChoice::no);
    assert(model->pollRecoveredContinue({}) == ContinueDecision::no);
    assert(model->phase() == GameplayPhase::continuePrompt && !model->pressedContinueChoice());
    assert(model->pollRecoveredContinue(vertical()) == ContinueDecision::no);
    assert(model->continueChoice() == ContinueChoice::no);
    rejects([&] { model->resumeRecoveredContinueRetry(); });
    assert(model->ticks() == ticks && model->remainingContinues() == 2);
    assert(model->level1State().loopTick() == loop);
    assertRngEqual(model->failureRng(), failureRng);
    assertRngEqual(model->level1State().rng(), ordinaryRng);

    auto cancelled = postBlackout();
    cancelled->enterRecoveredContinue();
    cancelled->pollRecoveredContinue(confirm());
    GameplayInput escape; escape.exit = true; escape.confirm = true;
    assert(cancelled->pollRecoveredContinue(escape) == ContinueDecision::cancelled);
    assert(cancelled->phase() == GameplayPhase::continuePrompt && !cancelled->pressedContinueChoice());
    assert(cancelled->pollRecoveredContinue({}) == ContinueDecision::cancelled);
    rejects([&] { cancelled->resumeRecoveredContinueRetry(); });
    assert(cancelled->ticks() == 37 && cancelled->remainingContinues() == 2);

    //The same UI owner drives the legacy eager retry and recovered deferred retry.
    auto legacy = postBlackout();
    auto recovered = std::make_unique<GameplayModel>(*legacy);
    legacy->tick({});
    recovered->enterRecoveredContinue();
    std::array<GameplayInput, 6> inputs{vertical(), {}, vertical(), confirm(), confirm(), vertical()};
    for (std::size_t index = 0; index < inputs.size(); ++index) {
        legacy->tick(inputs[index]);
        const auto decision = recovered->pollRecoveredContinue(inputs[index]);
        assertUiEqual(*legacy, *recovered);
        assert(decision == (index + 1 == inputs.size() ? ContinueDecision::yes : ContinueDecision::pending));
    }
    //Navigation is processed before release: the old pressed Yes wins even though selection is now No.
    assert(recovered->continueChoice() == ContinueChoice::no);
    assert(recovered->recoveredContinueDecision() == ContinueDecision::yes);
    assert(recovered->phase() == GameplayPhase::continuePrompt && recovered->remainingContinues() == 2);
    assertRngEqual(recovered->failureRng(), failureRng);
    assertRngEqual(recovered->level1State().rng(), ordinaryRng);
    assert(legacy->phase() == GameplayPhase::level1RetryHandoff && legacy->remainingContinues() == 1);
    recovered->resumeRecoveredContinueRetry();
    assert(recovered->phase() == GameplayPhase::level1RetryHandoff && recovered->remainingContinues() == 1);
    assert(recovered->ticks() == 37 && legacy->ticks() == 38 + inputs.size());
    assert(recovered->recoveredContinueDecision() == ContinueDecision::pending);
    assert(recovered->level1State().rng().calls() == failureRng.calls() + 16);
    assertRngEqual(recovered->level1State().rng(), legacy->level1State().rng());
    assert(recovered->level1State().loopTick() == 0 && recovered->playerArmor() == 680 && recovered->score() == 0);
    Level1ReloadTestAccess::assertRecoveredLists(recovered->level1State());
    Level1ReloadTestAccess::assertLegacyLists(legacy->level1State());
    assert(recovered->lastCockpitInitialization().displaySteps == 2);
    assert(recovered->cockpitIndicatorX() == legacy->cockpitIndicatorX());
    assert(recovered->failureAudioEvents().empty());
    rejects([&] { recovered->resumeRecoveredContinueRetry(); });
    rejects([&] { recovered->pollRecoveredContinue({}); });
    assert(recovered->remainingContinues() == 1);

    auto fire = postBlackout();
    fire->enterRecoveredContinue();
    fire->pollRecoveredContinue({}); fire->pollRecoveredContinue(vertical());
    GameplayInput firing; firing.fire = true;
    assert(fire->pollRecoveredContinue(firing) == ContinueDecision::pending);
    assert(fire->pollRecoveredContinue(confirm()) == ContinueDecision::pending);
    assert(fire->pressedContinueChoice() == ContinueChoice::yes);
    assert(fire->pollRecoveredContinue({}) == ContinueDecision::yes);
    assert(fire->remainingContinues() == 2 && fire->ticks() == 37);
    GameplayModelTestAccess::exhaust(*fire);
    rejects([&] { fire->resumeRecoveredContinueRetry(); });
    assert(fire->recoveredContinueDecision() == ContinueDecision::yes); //Failed retry cannot consume the decision.
    return 0;
}
