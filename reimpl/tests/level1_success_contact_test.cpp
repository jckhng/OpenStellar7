#include "stellar7/level1.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace stellar7 {

struct Level1ReloadTestAccess {
    static void staticPickupContact(Level1State& state, const bool retained) {
        state.retainSuccessContact_ = retained;
        state.hasAdvanced_ = false;
        state.loopTick_ = 3280;
        state.activeEnemyCount_ = 1;
        state.spawnedEnemyCount_ = state.maximumEnemyCount_;
        state.postWaveObjectAllocationIndex_ = 1; // Retained1292 makes source46C8 return.
        state.player_.position = {181, 84, 0};
        state.player_.forwardVelocity = 0;
        state.player_.yawVelocity = 0;
        state.playerWeaponCooldown_ = 3;
        state.playerProjectileTimers_ = {4, 2};
        state.selectedSystem_ = -1;
        state.systemStatusLevels_.fill(3);
        state.systemTimers_.fill(0);
        state.entities_[0].stateBytes4dTo52[1] = 0;
        state.entities_[1].stateBytes4dTo52[1] = 0;
        state.activeListOrder_ = {4, 2, 3, 5};
        auto& pickup = state.entities_[5];
        pickup.type = 25;
        pickup.collisionRadius = 897;
        pickup.position = state.player_.position;
        pickup.stateBytes4dTo52 = {254, 100, 0, 0, 0, 0xff};
        pickup.forwardVelocity = 0;
        pickup.xVelocity = 0;
        pickup.yVelocity = 0;
        pickup.zVelocity = 0;
        pickup.pitchVelocity = 0;
        pickup.rollVelocity = 0;
        pickup.yawVelocity = -2816;
        for (auto& projectile : state.projectiles_) {
            projectile.stateBytes4dTo54[1] = 0;
        }
        for (auto& effect : state.deathEffects_) {
            effect.listed = false;
        }
    }

    static bool pending(const Level1State& state) noexcept { return state.pendingSuccessContact(); }
    static void finish(Level1State& state, const Level1Input& input) { state.finishSuccessContact(input); }
    static void damageStatus(Level1State& state) noexcept { state.systemStatusLevels_[0] = 2; }
    static void restoreStatus(Level1State& state) noexcept { state.systemStatusLevels_[0] = 3; }
    static void clearGuardianReference(Level1State& state) noexcept {
        state.postWaveObjectAllocationIndex_ = static_cast<std::size_t>(-1);
    }
    static void addProjectile(Level1State& state) noexcept {
        state.projectiles_[0].stateBytes4dTo54[1] = 20;
        state.projectiles_[0].position = {10000, 10000, 0};
    }
    static void setPickupYawSpin(Level1State& state, const std::int16_t spin) noexcept {
        state.entities_[5].yawVelocity = spin;
    }
};

}

int main() {
    using stellar7::Level1Input;
    using stellar7::Level1ReloadTestAccess;
    using stellar7::Level1State;

    Level1State paused;
    Level1ReloadTestAccess::staticPickupContact(paused, true);
    const auto ground = paused.ground().points();
    const auto rngCalls = paused.rng().calls();
    const auto yaw = paused.entities()[5].yaw;
    paused.advanceIdleTick();
    assert(Level1ReloadTestAccess::pending(paused));
    assert(paused.successState() && !paused.missionActive());
    assert(paused.loopTick() == 3280);
    assert(paused.entities()[5].stateBytes4dTo52[2] == 0);
    assert(paused.entities()[5].stateBytes4dTo52[5] == 0xff);
    assert(paused.entities()[5].yaw == yaw);
    assert(paused.player().position.x == 181 && paused.player().position.y == 84);
    assert(paused.ground().points() == ground && paused.rng().calls() == rngCalls);
    assert((paused.activeListOrder() == std::vector<std::size_t>{4, 2, 3, 5}));
    paused.advanceIdleTick(); // The blocking presentation does not replay this frame.
    assert(paused.loopTick() == 3280 && paused.rng().calls() == rngCalls);
    assert(paused.entities()[5].stateBytes4dTo52[2] == 0);

    Level1ReloadTestAccess::damageStatus(paused);
    const auto rejectedRng = paused.rng().calls();
    const auto rejectedLoop = paused.loopTick();
    bool rejected = false;
    try {
        Level1ReloadTestAccess::finish(paused, {});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && !Level1ReloadTestAccess::pending(paused));
    assert(paused.loopTick() == rejectedLoop && paused.rng().calls() == rejectedRng);
    assert(paused.entities()[5].stateBytes4dTo52[2] == 0);
    Level1ReloadTestAccess::restoreStatus(paused);
    assert(Level1ReloadTestAccess::pending(paused));
    rejected = false;
    try {
        Level1ReloadTestAccess::finish(paused, {false, false, false, false, true, false});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && paused.loopTick() == 3280 && paused.rng().calls() == rngCalls);

    Level1ReloadTestAccess::finish(paused, {true, false, true, false, false, false});
    assert(!Level1ReloadTestAccess::pending(paused));
    assert(paused.loopTick() == 3281);
    assert(paused.entities()[5].stateBytes4dTo52[2] == 1);
    assert(paused.entities()[5].stateBytes4dTo52[5] == 1);
    assert(paused.entities()[5].yaw == yaw);
    assert(paused.player().position.x == 0 && paused.player().position.y == 0);
    assert(paused.player().yawVelocity == 0x50 && paused.player().forwardVelocity == 10);
    assert(paused.playerWeaponCooldown() == 2);
    assert(paused.playerProjectileTimers()[0] == 3 && paused.playerProjectileTimers()[1] == 1);
    assert(paused.ground().points() == ground && paused.rng().calls() == rngCalls);
    rejected = false;
    try {
        Level1ReloadTestAccess::finish(paused, {});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && paused.loopTick() == 3281 && paused.rng().calls() == rngCalls);

    Level1State legacy;
    Level1ReloadTestAccess::staticPickupContact(legacy, false);
    legacy.advanceIdleTick();
    assert(!Level1ReloadTestAccess::pending(legacy));
    assert(legacy.successState() && legacy.entities()[5].stateBytes4dTo52[2] == 1);

    Level1State unknownCompletion;
    Level1ReloadTestAccess::staticPickupContact(unknownCompletion, true);
    Level1ReloadTestAccess::clearGuardianReference(unknownCompletion);
    unknownCompletion.advanceIdleTick();
    assert(!Level1ReloadTestAccess::pending(unknownCompletion));

    Level1State unsupported;
    Level1ReloadTestAccess::staticPickupContact(unsupported, true);
    Level1ReloadTestAccess::addProjectile(unsupported);
    unsupported.advanceIdleTick();
    assert(!Level1ReloadTestAccess::pending(unsupported));

    Level1State unsupportedSpin;
    Level1ReloadTestAccess::staticPickupContact(unsupportedSpin, true);
    Level1ReloadTestAccess::setPickupYawSpin(unsupportedSpin, 1);
    unsupportedSpin.advanceIdleTick();
    assert(!Level1ReloadTestAccess::pending(unsupportedSpin));
}
