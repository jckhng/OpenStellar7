#include "stellar7/gameplay.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace stellar7 {

struct GameplayModelTestAccess {
    static void emptyCompletedDisplay(GameplayModel& model) {
        model.level2Entities_.clear();
        model.initializeCampaignPools(4, 0);
        model.phase_ = GameplayPhase::level5;
        model.level2Visibility_.resetLevel();
        model.level2Player_ = {};
        model.level2Player_.type = 0;
        model.level2Player_.collisionRadius = 330;
        model.level2Player_.stateBytes4dTo52[1] = 1;
        model.level2PlayerArmor_ = 680;
        model.level2ActiveEnemyCount_ = 0;
        model.level2SpawnedEnemyCount_ = 16;
        model.level2LoopTick_ = 17; // Completed main 16; tail still owns counter 16.
        model.level2HasAdvanced_ = true;
        model.level2TailConsumed_ = false;
        model.snapshotLevel2Presentation();
    }

    static void setSpawned(GameplayModel& model, std::int16_t count,
        std::int16_t active) {
        model.level2SpawnedEnemyCount_ = count;
        model.level2ActiveEnemyCount_ = active;
    }

    static void retainOrdinaryRadarPointer(GameplayModel& model) {
        Level1Entity presenter{};
        presenter.type = 11;
        presenter.stateBytes4dTo52[0] = 0xff;
        model.level2PresentationEntities_.push_back(presenter);
        model.level2PresentationActiveListOrder_.push_back(0);
    }

    static void suppressWithExistingCraftPointer(GameplayModel& model) {
        model.level2CompletionObjectIndex_ = 0;
    }

    static void fillActorPool(GameplayModel& model) {
        model.level2ActorPoolActive_.fill(true);
    }

    static std::size_t completionIndex(const GameplayModel& model) {
        return model.level2CompletionObjectIndex_;
    }

    static const auto& inheritedOrder(const GameplayModel& model) {
        return model.level2ActiveListOrder_;
    }

    static const auto& presentationOrder(const GameplayModel& model) {
        return model.level2PresentationActiveListOrder_;
    }

    static std::size_t actorSlot(const GameplayModel& model, std::size_t index) {
        return model.level2EntityActorPoolSlots_.at(index);
    }

    static bool tailConsumed(const GameplayModel& model) {
        return model.level2TailConsumed_;
    }
};

} // namespace stellar7

namespace {

constexpr std::size_t noObject = static_cast<std::size_t>(-1);
constexpr std::int16_t level5SpawnLimit = 16;
constexpr std::uint16_t entrySourceCounter = 16;
constexpr std::uint16_t completionEvent = 17;

std::size_t eventCount(const stellar7::GameplayModel& model,
    std::uint16_t eventId) {
    return static_cast<std::size_t>(std::count_if(
        model.level2AudioEvents().begin(), model.level2AudioEvents().end(),
        [eventId](const auto& event) { return event.eventId == eventId; }));
}

void assertTailBirthAndAdjacentMains() {
    stellar7::GameplayModel model;
    stellar7::GameplayModel suppressed;
    stellar7::GameplayModelTestAccess::emptyCompletedDisplay(model);
    stellar7::GameplayModelTestAccess::emptyCompletedDisplay(suppressed);
    stellar7::GameplayModelTestAccess::suppressWithExistingCraftPointer(suppressed);

    assert(model.level2Entities().empty());
    assert(stellar7::GameplayModelTestAccess::completionIndex(model) == noObject);
    assert(eventCount(model, completionEvent) == 0);
    const auto beforeRng = model.level2Rng().calls();
    const auto suppressedBeforeRng = suppressed.level2Rng().calls();

    model.finishLevel5Frame();
    suppressed.finishLevel5Frame();

    assert(stellar7::GameplayModelTestAccess::tailConsumed(model));
    assert(model.level2Entities().size() == 1);
    assert(stellar7::GameplayModelTestAccess::completionIndex(model) == 0);
    assert(stellar7::GameplayModelTestAccess::actorSlot(model, 0) == 0);
    assert(stellar7::GameplayModelTestAccess::inheritedOrder(model).empty());
    assert(stellar7::GameplayModelTestAccess::presentationOrder(model).empty());
    assert(eventCount(model, completionEvent) == 1);
    assert(model.level2AudioEvents().back().loopTick == entrySourceCounter);
    assert(model.level2Rng().calls() - beforeRng
        == suppressed.level2Rng().calls() - suppressedBeforeRng + 2);

    const auto born = model.level2Entities().front();
    stellar7::Level1Entity expected{};
    expected.position = {0, 0, 5000};
    expected.forwardVelocity = static_cast<std::int16_t>(0x9000);
    stellar7::advanceEntityKinematics(expected);
    assert(born.position == expected.position);
    assert(born.type == 7);
    assert(born.collisionRadius == 13052);
    assert(born.forwardVelocity == 400);
    assert(born.pitch == 0 && born.roll == 0 && born.yaw == 0);
    assert(born.stateBytes4dTo52[0] == 0xff);
    assert(born.stateBytes4dTo52[1] == 100);
    assert(born.stateBytes4dTo52[2] == 0);
    assert(born.stateBytes4dTo52[4] == 5);
    assert(born.stateBytes4dTo52[5] == 0);
    assert(born.renderProfile
        == stellar7::WorldActorRenderProfile::inlineSelectorNoCallback);

    const auto afterBirthRng = model.level2Rng().calls();
    bool rejectedDoubleTail = false;
    try {
        model.finishLevel5Frame();
    } catch (const std::invalid_argument&) {
        rejectedDoubleTail = true;
    }
    assert(rejectedDoubleTail);
    assert(model.level2Rng().calls() == afterBirthRng);
    assert(eventCount(model, completionEvent) == 1);

    model.tick({});
    assert(model.level2Entities().size() == 1);
    assert(model.level2Entities().front().position == born.position);
    assert(model.level2Entities().front().stateBytes4dTo52[5] == 0);
    assert(eventCount(model, completionEvent) == 1);
    model.finishLevel5Frame();

    expected = born;
    stellar7::advanceEntityKinematics(expected);
    model.tick({});
    assert(model.level2Entities().front().position == expected.position);
    assert(model.level2Entities().front().stateBytes4dTo52[5] == 0);
    assert(eventCount(model, completionEvent) == 1);
}

void assertEntryGuards() {
    stellar7::GameplayModel belowLimit;
    stellar7::GameplayModelTestAccess::emptyCompletedDisplay(belowLimit);
    stellar7::GameplayModelTestAccess::setSpawned(
        belowLimit, level5SpawnLimit - 1, 15);
    belowLimit.finishLevel5Frame();
    assert(belowLimit.level2Entities().empty());
    assert(eventCount(belowLimit, completionEvent) == 0);

    stellar7::GameplayModel retainedPointer;
    stellar7::GameplayModelTestAccess::emptyCompletedDisplay(retainedPointer);
    stellar7::GameplayModelTestAccess::setSpawned(retainedPointer, level5SpawnLimit, 1);
    stellar7::GameplayModelTestAccess::retainOrdinaryRadarPointer(retainedPointer);
    retainedPointer.finishLevel5Frame();
    assert(retainedPointer.level2Entities().empty());
    assert(eventCount(retainedPointer, completionEvent) == 0);

    stellar7::GameplayModel counterOnly;
    stellar7::GameplayModelTestAccess::emptyCompletedDisplay(counterOnly);
    stellar7::GameplayModelTestAccess::setSpawned(counterOnly, level5SpawnLimit, 1);
    counterOnly.finishLevel5Frame();
    assert(counterOnly.level2Entities().size() == 1);
    assert(eventCount(counterOnly, completionEvent) == 1);

    stellar7::GameplayModel fullPool;
    stellar7::GameplayModelTestAccess::emptyCompletedDisplay(fullPool);
    stellar7::GameplayModelTestAccess::fillActorPool(fullPool);
    const auto beforeRng = fullPool.level2Rng().calls();
    bool rejectedFullPool = false;
    try {
        fullPool.finishLevel5Frame();
    } catch (const std::invalid_argument&) {
        rejectedFullPool = true;
    }
    assert(rejectedFullPool);
    assert(fullPool.level2Rng().calls() == beforeRng);
    assert(fullPool.level2Entities().empty());
    assert(eventCount(fullPool, completionEvent) == 0);
}

} // namespace

int main() {
    assertTailBirthAndAdjacentMains();
    assertEntryGuards();
}
