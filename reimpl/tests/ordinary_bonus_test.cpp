#include "stellar7/gameplay.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace stellar7 {

struct Level1ReloadTestAccess {
    static void streak(Level1State& state, std::int16_t type, std::uint16_t count) {
        state.lastDestroyedType_ = type;
        state.consecutiveDestroyedTypeCount_ = count;
    }

    static void repairDomain(Level1State& state, std::size_t system,
        std::int8_t status, std::int16_t selected, std::int16_t timer) {
        state.systemStatusLevels_.fill(3);
        state.systemStatusLevels_[system] = status;
        state.systemTimers_.fill(0);
        state.systemTimers_[system] = timer;
        state.selectedSystem_ = selected;
    }
};

struct GameplayModelTestAccess {
    static Level1State& systems(GameplayModel& model) {
        return model.level1_;
    }

    static void level5(GameplayModel& model) {
        model.initializeCampaignPools(4, 0);
        model.phase_ = GameplayPhase::level5;
        model.level2ActiveEnemyCount_ = 5;
        model.level2SpawnedEnemyCount_ = 5;
    }

    static void occupyFixed(GameplayModel& model, std::size_t count) {
        model.level2Entities_.assign(count, Level1Entity{});
        model.level2EntityActorPoolSlots_.assign(count, model.level2ActorPoolSize_);
        model.level2EntityProjectilePoolSlots_.assign(count, model.level2ProjectilePoolSize_);
        model.level2EntityDeathEffectPoolSlots_.assign(count, model.level2DeathEffectPoolSize_);
        model.level2EntityFixedPoolSlots_.clear();
        model.level2AllocationListOrder_.clear();
        model.level2ObjectListOrder_.clear();
        model.level2ActiveListOrder_.clear();
        for (std::size_t slot = 0; slot < count; ++slot) {
            auto& actor = model.level2Entities_[slot];
            actor.allocationIndex = slot;
            actor.type = 30;
            actor.stateBytes4dTo52 = {0xfe, 100, 0, 0, 0, 0};
            actor.position = {30000, 30000, 0};
            model.level2FixedPool_[slot] = actor;
            model.level2FixedPoolKnown_[slot] = true;
            model.level2EntityFixedPoolSlots_.push_back(slot);
            model.level2AllocationListOrder_.push_back(slot);
            model.level2ObjectListOrder_.push_back(slot);
        }
    }

    static void markFixedUnknown(GameplayModel& model, std::size_t slot) {
        model.level2FixedPoolKnown_[slot] = false;
    }

    static void registerBonus(GameplayModel& model, const Level1Entity& source) {
        model.registerLevel5DestroyedTypeBonus(source);
    }

    static std::size_t entityCount(const GameplayModel& model) {
        return model.level2Entities_.size();
    }

    static const Level1Entity& entity(const GameplayModel& model, std::size_t index) {
        return model.level2Entities_.at(index);
    }

    static std::size_t fixedSlot(const GameplayModel& model, std::size_t index) {
        return model.level2EntityFixedPoolSlots_.at(index);
    }

    static const std::vector<std::size_t>& active(const GameplayModel& model) {
        return model.level2ActiveListOrder_;
    }

    static const std::vector<std::size_t>& allocation(const GameplayModel& model) {
        return model.level2AllocationListOrder_;
    }

    static void presentPickup(GameplayModel& model, std::size_t index) {
        model.level2ActiveListOrder_ = {index};
        model.level2Player_ = {};
        model.level2Player_.type = 0;
        model.level2Player_.collisionRadius = 330;
        model.level2Player_.stateBytes4dTo52[1] = 1;
        model.level2Player_.position = model.level2Entities_[index].position;
        model.level2PlayerArmor_ = 680;
        model.level2HasAdvanced_ = false;
        model.level2LoopTick_ = 1;
    }

    static void advance(GameplayModel& model) {
        model.advanceLevel2({}, nullptr);
    }
};

}  // namespace stellar7

namespace {

using stellar7::GameplayModel;
using stellar7::GameplayModelTestAccess;
using stellar7::Level1Entity;
using stellar7::Level1ReloadTestAccess;

Level1Entity killed(std::int16_t type) {
    Level1Entity source{};
    source.type = type;
    source.position = {1234, -5678, 250};
    return source;
}

void thirdKillPayloadsAndIdentity() {
    constexpr std::array<std::pair<std::int16_t, std::uint8_t>, 3> cases{{
        {5, 6}, {11, 3}, {13, 4},
    }};
    for (const auto [type, payload] : cases) {
        auto model = std::make_unique<GameplayModel>();
        GameplayModelTestAccess::level5(*model);
        Level1ReloadTestAccess::streak(GameplayModelTestAccess::systems(*model), type, 2);
        const auto calls = model->level2Rng().calls();
        GameplayModelTestAccess::registerBonus(*model, killed(type));
        assert(model->level2Rng().calls() == calls);
        assert(model->level1State().lastDestroyedType() == type);
        assert(model->level1State().consecutiveDestroyedTypeCount() == 0);
        assert(GameplayModelTestAccess::entityCount(*model) == 1);
        const auto& bonus = GameplayModelTestAccess::entity(*model, 0);
        assert(bonus.type == 42 && bonus.collisionRadius == 330);
        assert((bonus.position == stellar7::FixedPosition{1234, -5678, 100}));
        assert((bonus.stateBytes4dTo52 == std::array<std::uint8_t, 6>{0xfe, 1, payload, 0, 0, payload}));
        assert(bonus.yawVelocity == 0x0800 && bonus.rollVelocity == 0x0800);
        assert(bonus.renderProfile == stellar7::WorldActorRenderProfile::inlineSelectorNoCallback);
        assert(GameplayModelTestAccess::fixedSlot(*model, 0) == 0);
        assert(GameplayModelTestAccess::allocation(*model) == std::vector<std::size_t>{0});
        assert(GameplayModelTestAccess::active(*model).empty());
    }
}

void fixedCapacityAndHistory() {
    auto occupied = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::level5(*occupied);
    GameplayModelTestAccess::occupyFixed(*occupied, 1);
    Level1ReloadTestAccess::streak(GameplayModelTestAccess::systems(*occupied), 11, 2);
    GameplayModelTestAccess::registerBonus(*occupied, killed(11));
    assert(GameplayModelTestAccess::entityCount(*occupied) == 2);
    assert(GameplayModelTestAccess::fixedSlot(*occupied, 1) == 1);
    assert(GameplayModelTestAccess::allocation(*occupied).back() == 1);
    assert(GameplayModelTestAccess::active(*occupied).empty());

    auto full = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::level5(*full);
    GameplayModelTestAccess::occupyFixed(*full, 16);
    Level1ReloadTestAccess::streak(GameplayModelTestAccess::systems(*full), 5, 2);
    const auto calls = full->level2Rng().calls();
    GameplayModelTestAccess::registerBonus(*full, killed(5));
    assert(GameplayModelTestAccess::entityCount(*full) == 16);
    assert(full->level1State().lastDestroyedType() == 5);
    assert(full->level1State().consecutiveDestroyedTypeCount() == 0);
    assert(full->level2Rng().calls() == calls);

    for (const auto occupiedCount : {std::size_t{0}, std::size_t{8}}) {
        auto history = std::make_unique<GameplayModel>();
        GameplayModelTestAccess::level5(*history);
        GameplayModelTestAccess::occupyFixed(*history, occupiedCount);
        if (occupiedCount == 0) {
            GameplayModelTestAccess::markFixedUnknown(*history, 0);
        }
        Level1ReloadTestAccess::streak(GameplayModelTestAccess::systems(*history), 13, 2);
        const auto callsBefore = history->level2Rng().calls();
        bool rejected = false;
        try {
            GameplayModelTestAccess::registerBonus(*history, killed(13));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected && GameplayModelTestAccess::entityCount(*history) == occupiedCount);
        assert(history->level1State().lastDestroyedType() == 13);
        assert(history->level1State().consecutiveDestroyedTypeCount() == 2);
        assert(history->level2Rng().calls() == callsBefore);
    }
}

void zeroSelectorKeepsCounting() {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::level5(*model);
    Level1ReloadTestAccess::streak(GameplayModelTestAccess::systems(*model), 8, 2);
    for (const auto expected : {3, 4}) {
        GameplayModelTestAccess::registerBonus(*model, killed(8));
        assert(model->level1State().lastDestroyedType() == 8);
        assert(model->level1State().consecutiveDestroyedTypeCount() == expected);
        assert(GameplayModelTestAccess::entityCount(*model) == 0);
    }
}

void pickupRepairAndGuards() {
    for (const auto restriction : {0, 1, 2, 3}) {
        auto model = std::make_unique<GameplayModel>();
        GameplayModelTestAccess::level5(*model);
        Level1ReloadTestAccess::streak(GameplayModelTestAccess::systems(*model), 11, 2);
        GameplayModelTestAccess::registerBonus(*model, killed(11));
        const auto pickup = std::size_t{0};
        GameplayModelTestAccess::presentPickup(*model, pickup);
        auto& systems = GameplayModelTestAccess::systems(*model);
        Level1ReloadTestAccess::repairDomain(systems, 2,
            restriction == 3 ? -1 : 1,
            restriction == 1 ? 0 : -1,
            restriction == 2 ? 1 : 0);
        const auto score = model->score();
        const auto armor = model->playerArmor();
        const auto events = model->level2AudioEvents().size();
        bool rejected = false;
        try {
            GameplayModelTestAccess::advance(*model);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        if (restriction != 0) {
            assert(rejected);
            assert(GameplayModelTestAccess::entityCount(*model) == 1);
            assert(model->score() == score && model->playerArmor() == armor);
            assert(model->level2AudioEvents().size() == events);
            assert(systems.systemStatusLevels()[2] == (restriction == 3 ? -1 : 1));
            continue;
        }
        assert(!rejected && GameplayModelTestAccess::entityCount(*model) == 0);
        assert(model->score() == score + 100 && model->playerArmor() == armor);
        assert(model->level2AudioEvents().size() == events + 1);
        assert(model->level2AudioEvents().back().eventId == 24);
        assert(systems.systemStatusLevels()[2] == 3);
        assert(std::all_of(systems.systemTimers().begin(), systems.systemTimers().end(),
            [](const auto timer) { return timer == 0; }));
        assert(systems.selectedSystem() == -1);
    }
}

}  // namespace

int main() {
    thirdKillPayloadsAndIdentity();
    fixedCapacityAndHistory();
    zeroSelectorKeepsCounting();
    pickupRepairAndGuards();
}
