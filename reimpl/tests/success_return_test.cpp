#include "stellar7/gameplay.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace stellar7 {

struct GameplayModelTestAccess {
    static void rewardContact(GameplayModel& model, std::uint16_t sourceTick = 975) {
        model.level2Entities_.clear();
        model.initializeCampaignPools(4, 0);
        model.phase_ = GameplayPhase::level5;
        model.level2HasAdvanced_ = true;
        model.level5TransientStateKnownInactive_ = true;
        model.level2TailConsumed_ = true;
        model.level2LoopTick_ = sourceTick;
        model.level2Player_ = {};
        model.level2Player_.collisionRadius = 723;
        model.level2Player_.forwardVelocity = 200;
        model.level2Player_.yaw = 2336;
        model.level2Player_.yawVelocity = 512;
        model.level2PlayerArmor_ = 136;
        model.level2Score_ = 1250;
        model.level2ActiveEnemyCount_ = 1;
        model.level2SpawnedEnemyCount_ = 18;
        Level1Entity reward{};
        reward.type = 25;
        reward.position = {0, 800, 0};
        reward.collisionRadius = 897;
        reward.yawVelocity = 3072;
        reward.stateBytes4dTo52 = {0xfe, 100, 1, 0, 0, 0xff};
        model.level2PickupIndex_ = model.allocateLevel2FixedObject(reward);
        // 4506 is partial; this fixture supplies the later 45C6 reward fields.
        model.level2Entities_.at(model.level2PickupIndex_) = reward;
        model.level2FixedPool_.at(model.level2EntityFixedPoolSlots_.at(model.level2PickupIndex_)) = reward;
        model.level2ActiveListOrder_ = {model.level2PickupIndex_};
        model.tick({});
        assert(model.phase_ == GameplayPhase::level2SuccessPresentation);
        model.setSuccessAudioActive(true);
    }

    static const Level1Entity& physicalReward(const GameplayModel& model) {
        return model.level2FixedPool_.at(model.level2EntityFixedPoolSlots_.at(0));
    }

    static void alterIdentity(GameplayModel& model) { ++model.level2AllocationRevision_; }
    static void expireWeapon(GameplayModel& model) { model.level2Player_.weaponCooldown = 1; }
    static void extraActor(GameplayModel& model) { model.level2ActiveListOrder_.push_back(0); }
    static void unknownTransientState(GameplayModel& model) { model.level5TransientStateKnownInactive_ = false; }
    static void retainedTimers(GameplayModel& model) {
        model.level2Player_.weaponCooldown = 2;
        model.level2Player_.weaponSlotTimers = {3, 3, 0};
    }
    static void missingPause(GameplayModel& model) {
        model.startLevel3();
        model.activeCampaignLevel_ = 4;
        model.phase_ = GameplayPhase::level2SuccessPresentation;
    }
};

} // namespace stellar7

namespace {
using namespace stellar7;

void assertRetainedReturn(std::uint16_t sourceTick) {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::rewardContact(*model, sourceTick);
    const auto player = model->level2Player();
    const auto reward = model->level2Entities().front();
    const auto rngCalls = model->level2Rng().calls();
    const auto ground = model->level2Ground().points();
    const auto indicator = model->level2CockpitIndicatorX();
    const auto loop = model->level2LoopTick();
    for (unsigned poll = 0; poll < 3; ++poll) {
        model->tick({});
        assert(model->level2LoopTick() == loop);
        assert(model->level2Player().position == player.position);
        assert(model->level2Entities().front().stateBytes4dTo52 == reward.stateBytes4dTo52);
    }
    model->setSuccessAudioActive(false);
    model->tick({});
    assert(model->campaignLevel() == 6);
    assert(model->score() == 1250 && model->playerArmor() == 136);
    assert(model->heading() == player.yaw);
    assert(model->turnVelocity() == player.yawVelocity - 80);
    assert(model->forwardVelocity() == player.forwardVelocity - 10);
    assert(model->level2LoopTick() == loop);
    assert(model->level2Player().position == FixedPosition({0, 0, 0}));
    assert(model->level2Player().yaw == player.yaw);
    assert(model->level2Player().forwardVelocity == player.forwardVelocity - 10);
    const auto& after = model->level2Entities().front();
    assert(after.position == FixedPosition({reward.position.x - player.position.x,
        reward.position.y - player.position.y, reward.position.z}));
    assert(after.yaw == reward.yaw);
    assert(after.stateBytes4dTo52[2] == 0);
    assert(after.stateBytes4dTo52[5] == 0xff);
    assert(GameplayModelTestAccess::physicalReward(*model).stateBytes4dTo52 == after.stateBytes4dTo52);
    assert(GameplayModelTestAccess::physicalReward(*model).position == after.position);
    assert(model->level2Rng().calls() == rngCalls);
    assert(model->level2Ground().points() == ground);
    assert(model->level2CockpitIndicatorX() == indicator);
    assert(model->level2AudioEvents().empty());
    const auto ticks = model->ticks();
    bool rejected = false;
    try { model->tick({}); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected && model->ticks() == ticks);
}

void assertWeaponTimerOwnership() {
    auto model = std::make_unique<GameplayModel>();
    GameplayInput fire{};
    fire.fire = true;
    model->tick(fire);
    model->tick({});
    assert(model->shotActive());
    GameplayModelTestAccess::rewardContact(*model);
    GameplayModelTestAccess::retainedTimers(*model);
    model->setSuccessAudioActive(false);
    model->tick({});
    assert(model->weaponCooldown() == 1 && !model->weaponReady());
    assert(model->level1State().playerProjectileTimers()[0] != 0);
    assert(!model->shotActive() && !model->shotActive(0) && !model->shotActive(1));
    assert(model->shotCount() == 0 && model->shotAge() == 0 && model->shotAge(0) == 0);
    assert(model->level2Player().weaponSlotTimers == (std::array<std::uint8_t, 3>{2, 2, 0}));
}

template<class Alter>
void assertRejected(Alter alter, std::uint16_t sourceTick = 975,
    const GameplayInput& input = {}) {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::rewardContact(*model, sourceTick);
    alter(*model);
    model->setSuccessAudioActive(false);
    const auto ticks = model->ticks();
    const auto loop = model->level2LoopTick();
    const auto player = model->level2Player();
    const auto reward = model->level2Entities();
    const auto calls = model->level2Rng().calls();
    bool rejected = false;
    try { model->tick(input); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    assert(model->ticks() == ticks && model->level2LoopTick() == loop);
    assert(model->level2Player().position == player.position);
    assert(model->level2Entities().front().stateBytes4dTo52 == reward.front().stateBytes4dTo52);
    assert(model->level2Rng().calls() == calls);
}
}

int main() {
    try {
        assertRetainedReturn(975);
        assertRetainedReturn(65535);
        assertWeaponTimerOwnership();
        assertRejected([](auto&) {}, 976);
        assertRejected(GameplayModelTestAccess::alterIdentity);
        assertRejected(GameplayModelTestAccess::expireWeapon);
        assertRejected(GameplayModelTestAccess::extraActor);
        assertRejected(GameplayModelTestAccess::unknownTransientState);
        assertRejected(GameplayModelTestAccess::missingPause);
        GameplayInput fire{};
        fire.fire = true;
        assertRejected([](auto&) {}, 975, fire);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
