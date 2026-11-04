#include "stellar7/gameplay.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace stellar7 {

struct Level1ReloadTestAccess {
    static void history(Level1State& state) {
        state.score_ = 31;
        state.systemStatusLevels_.fill(1);
        state.systemTimers_ = {1, 2, 3, 77, 4, 5, 6};
        state.lastDestroyedType_ = 11;
        state.consecutiveDestroyedTypeCount_ = 2;
    }

    static void select(Level1State& state, std::int16_t system) {
        state.selectedSystem_ = system;
    }
};

struct GameplayModelTestAccess {
    static void returnedMenu(GameplayModel& model, const StellarRng& liveRng) {
        model.phase_ = GameplayPhase::nativeMenu;
        model.activeCampaignLevel_ = 7;
        model.campaignSuccess_ = true;
        model.presentationFrame_ = 3;
        model.ticks_ = 120;
        model.remainingContinues_ = 0;
        model.level2Score_ = 1250;
        model.level2PlayerArmor_ = 136;
        model.level2Rng_ = liveRng;
        model.level2Entities_.push_back(Level1Entity{});
        model.level2FixedPool_[0].stateBytes4dTo52[1] = 7;
        model.level2FixedPool_[8].type = 42;
        model.level2FixedPool_[8].stateBytes4dTo52[1] = 9;
        model.level2FixedPool_[8].position = {1234, -5678, 90};
        model.level2FixedPoolKnown_[8] = true;
        model.level2FixedPoolEverUsed_[8] = true;
        model.cockpitIndicatorX_ = 160;
        model.cockpitIndicatorVelocity_ = 4;
        Level1ReloadTestAccess::history(model.level1_);
    }

    static std::uint8_t fixedHealth(const GameplayModel& model, std::size_t slot) {
        return model.level2FixedPool_[slot].stateBytes4dTo52[1];
    }

    static const Level1Entity& fixed(const GameplayModel& model, std::size_t slot) {
        return model.level2FixedPool_[slot];
    }

    static bool fixedKnown(const GameplayModel& model, std::size_t slot) {
        return model.level2FixedPoolKnown_[slot];
    }

    static bool fixedEverUsed(const GameplayModel& model, std::size_t slot) {
        return model.level2FixedPoolEverUsed_[slot];
    }

    static void selectSystem(GameplayModel& model, std::int16_t system) {
        Level1ReloadTestAccess::select(model.level1_, system);
    }
};

}  // namespace stellar7

namespace {

using namespace stellar7;

template <typename Action>
void rejects(const Action& action) {
    bool threw = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void assertSameRng(StellarRng first, StellarRng second) {
    assert(first.calls() == second.calls());
    assert(first.indexA() == second.indexA());
    assert(first.indexB() == second.indexB());
    assert(first.last() == second.last());
    for (unsigned index = 0; index < 4; ++index) {
        assert(first.next() == second.next());
    }
}

void returnedBeginRetainsSharedState() {
    StellarRng liveRng;
    for (unsigned index = 0; index < 73; ++index) {
        liveRng.next();
    }
    Level1State expected;
    expected.reloadAfterFailure(liveRng);

    GameplayModel model;
    GameplayModelTestAccess::returnedMenu(model, liveRng);
    const auto tickBefore = model.ticks();
    model.validateMenuBegin();
    model.prepareMenuBegin();
    assert(model.phase() == GameplayPhase::nativeBeginWait);
    assert(model.ticks() == tickBefore && model.score() == 0);
    assert(model.remainingContinues() == 2);
    assert((model.level1State().systemStatusLevels()
        == std::array<std::int8_t, 7>{3, 3, 3, 3, 3, 3, 3}));
    assert(model.level1State().systemTimers()[3] == 77);
    assertSameRng(model.level2Rng(), liveRng);

    model.finishMenuBegin();
    assert(model.phase() == GameplayPhase::level1BeginHandoff);
    assert(model.campaignLevel() == 0 && model.ticks() == tickBefore);
    assert(model.score() == 0 && model.playerArmor() == 680);
    assert(model.level1State().entities().size() == Level1State::entityCount);
    assert(model.level1State().lastDestroyedType() == 11);
    assert(model.level1State().consecutiveDestroyedTypeCount() == 0);
    assert((model.level1State().systemTimers()
        == std::array<std::int16_t, 7>{0, 0, 0, 77, 0, 0, 0}));
    assert(GameplayModelTestAccess::fixedHealth(model, 0) == 0);
    assert(GameplayModelTestAccess::fixedKnown(model, 0));
    assert(GameplayModelTestAccess::fixed(model, 8).type == 42);
    assert(GameplayModelTestAccess::fixedHealth(model, 8) == 9);
    assert((GameplayModelTestAccess::fixed(model, 8).position
        == FixedPosition{1234, -5678, 90}));
    assert(GameplayModelTestAccess::fixedKnown(model, 8));
    assert(GameplayModelTestAccess::fixedEverUsed(model, 8));
    assertSameRng(model.level1State().rng(), expected.rng());
    assertSameRng(model.level2Rng(), expected.rng());
    const auto& initialization = model.lastCockpitInitialization();
    assert(initialization.levelIndex == 0 && initialization.displaySteps == 2);
    assert(initialization.states[0].x == 160);
    assert(initialization.states[1].x == 164);
    assert(initialization.states[2].x == 168);
    assert(initialization.states[3].x == 168);

    model.tick({});
    assert(model.phase() == GameplayPhase::level1 && model.ticks() == tickBefore + 1);
    assert(model.lastCockpitInitialization().displaySteps == 3);
    assert(model.cockpitIndicatorX() == 172);
    assert(model.lastCockpitInitialization().states.back().x == 172);
    model.tick({});
    assert(model.phase() == GameplayPhase::level1 && model.ticks() == tickBefore + 2);
    assert(model.cockpitIndicatorX() == 176);
    assert(model.lastCockpitInitialization().states.back().x == 172);
}

void rejectedPhasesLeaveModelUnchanged() {
    GameplayModel model;
    const auto initialTick = model.ticks();
    rejects([&] { model.validateMenuBegin(); });
    rejects([&] { model.prepareMenuBegin(); });
    rejects([&] { model.finishMenuBegin(); });
    assert(model.phase() == GameplayPhase::level1 && model.ticks() == initialTick);

    StellarRng liveRng;
    GameplayModelTestAccess::returnedMenu(model, liveRng);
    const auto before = model.level2Rng();
    rejects([&] { model.finishMenuBegin(); });
    assert(model.phase() == GameplayPhase::nativeMenu && model.score() == 1250);
    model.prepareMenuBegin();
    rejects([&] { model.prepareMenuBegin(); });
    rejects([&] { model.tick({}); });
    assert(model.phase() == GameplayPhase::nativeBeginWait && model.ticks() == 120);
    assertSameRng(model.level2Rng(), before);
}

void loaderRetainsSelectedSystem() {
    GameplayModel model;
    GameplayModelTestAccess::returnedMenu(model, StellarRng{});
    GameplayModelTestAccess::selectSystem(model, 2);
    model.prepareMenuBegin();
    model.finishMenuBegin();
    assert(model.level1State().selectedSystem() == 2);
}

}  // namespace

int main() {
    returnedBeginRetainsSharedState();
    rejectedPhasesLeaveModelUnchanged();
    loaderRetainsSelectedSystem();
}
