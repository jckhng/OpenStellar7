#include "stellar7/level1.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <stdexcept>

namespace stellar7 {

struct Level1ReloadTestAccess {
    static void lethalDisplay(Level1State& state) {
        state.hasAdvanced_ = true;
        state.missionActive_ = true;
        state.failurePending_ = true;
        state.failureTailConsumed_ = false;
        state.playerArmor_ = -32;
        state.loopTick_ = 17;
        state.player_.position = {320, -640, 0};
        state.player_.yawVelocity = 0;
        state.player_.forwardVelocity = 0;
        state.playerWeaponCooldown_ = 3;
        state.playerProjectileTimers_ = {4, 2};
        state.pendingPostBoundaryInput_ = {true, false, true, false, false, false};
        state.selectedSystem_ = -1;
        state.systemTimers_.fill(0);
        state.failureTailProjectileActiveAtStart_.fill(false);
        state.failureTailProjectileActiveAtStart_[0] = true;
        for (const auto slot : {0U, 1U}) {
            state.projectiles_[slot].position = {1000, 2000, 0};
            state.projectiles_[slot].stateBytes4dTo54[1] = 5;
        }
        auto type11 = std::find_if(state.entities_.begin(), state.entities_.end(),
            [](const auto& entity) { return entity.type == 11; });
        assert(type11 != state.entities_.end());
        type11->animatedModelPresenterEnabled = true;
        type11->stateBytes4dTo52[3] = 0;
        type11->stateBytes4dTo52[2] = 2;
        type11->modelAnimationVariant = 1;
    }

    static bool pending(const Level1State& state) noexcept { return state.pendingFailureTail(); }
    static void finish(Level1State& state) { state.finishFailureTail(); }
    static void periodic(Level1State& state) noexcept { state.loopTick_ = 16; }
    static void selected(Level1State& state) noexcept { state.selectedSystem_ = 0; }
    static void timed(Level1State& state) noexcept { state.systemTimers_[0] = 1; }
    static void exiting(Level1State& state) noexcept { state.pendingPostBoundaryInput_.exit = true; }
};

}

int main() {
    using stellar7::Level1ReloadTestAccess;
    using stellar7::Level1State;

    Level1State level;
    Level1ReloadTestAccess::lethalDisplay(level);
    assert(Level1ReloadTestAccess::pending(level));
    const auto rngCalls = level.rng().calls();
    const auto audioCount = level.audioEvents().size();
    const auto& type11Before = *std::find_if(level.entities().begin(), level.entities().end(),
        [](const auto& entity) { return entity.type == 11; });
    const auto variantBefore = type11Before.modelAnimationVariant;
    Level1ReloadTestAccess::finish(level);
    assert(!Level1ReloadTestAccess::pending(level));
    assert(level.loopTick() == 18);
    assert(level.player().position.x == 0 && level.player().position.y == 0);
    assert(level.player().yawVelocity == 0x50 && level.player().forwardVelocity == 10);
    assert(level.playerWeaponCooldown() == 2);
    assert(level.playerProjectileTimers()[0] == 3 && level.playerProjectileTimers()[1] == 1);
    assert(level.projectiles()[0].position.x == 680 && level.projectiles()[0].position.y == 2640);
    assert(level.projectiles()[1].position.x == 1000 && level.projectiles()[1].position.y == 2000);
    const auto& type11After = *std::find_if(level.entities().begin(), level.entities().end(),
        [](const auto& entity) { return entity.type == 11; });
    assert(type11After.modelAnimationVariant == ((variantBefore + 1U) & 7U));
    assert(type11After.stateBytes4dTo52[3] == 2);
    assert(level.rng().calls() == rngCalls && level.audioEvents().size() == audioCount);
    bool rejected = false;
    try {
        Level1ReloadTestAccess::finish(level);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && level.loopTick() == 18 && level.playerWeaponCooldown() == 2);
    level.advanceIdleTick();
    assert(level.failureState() && !level.missionActive());
    assert(level.loopTick() == 18 && level.playerWeaponCooldown() == 2);
    assert(level.rng().calls() == rngCalls && level.audioEvents().size() == audioCount);

    for (const auto reject : {&Level1ReloadTestAccess::periodic, &Level1ReloadTestAccess::selected,
            &Level1ReloadTestAccess::timed, &Level1ReloadTestAccess::exiting}) {
        Level1State unsupported;
        Level1ReloadTestAccess::lethalDisplay(unsupported);
        reject(unsupported);
        const auto before = unsupported;
        assert(!Level1ReloadTestAccess::pending(unsupported));
        rejected = false;
        try {
            Level1ReloadTestAccess::finish(unsupported);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
        assert(unsupported.loopTick() == before.loopTick());
        assert(unsupported.player().position.x == before.player().position.x);
        assert(unsupported.playerWeaponCooldown() == before.playerWeaponCooldown());
        assert(unsupported.playerProjectileTimers() == before.playerProjectileTimers());
        assert(unsupported.rng().calls() == before.rng().calls());
        assert(unsupported.audioEvents().size() == before.audioEvents().size());
    }
}
