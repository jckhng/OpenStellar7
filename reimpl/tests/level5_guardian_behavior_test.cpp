#include "../src/level5_guardian.hpp"

#include <cassert>
#include <cstdint>

namespace {
using namespace stellar7;
Level1Entity guardian() {
    Level1Entity actor{};
    actor.type = 44;
    actor.position = {1000, 2000, 0};
    actor.presenterDistance = 0x2000;
    actor.renderProfile = WorldActorRenderProfile::heapSelectorDeferredAnimation;
    actor.animatedModelPresenterEnabled = true;
    actor.stateBytes4dTo52[2] = 2;
    return actor;
}
void callbackBranches() {
    unsigned shots = 0;
    const auto fire = [&shots](Level1Entity&) { ++shots; };
    auto actor = guardian();
    actor.presenterDistance = 0x7001;
    actor.yawVelocity = 99;
    actor.targetHeading = 0x1234;
    advanceLevel5GuardianBehavior(actor, 0, fire);
    assert(actor.yaw == 0x1234 && actor.yawVelocity == 99 && shots == 0);
    assert(!actor.animatedModelPresenterEnabled && actor.renderProfile == WorldActorRenderProfile::heapSelectorNoCallback);
    for (const auto difference : {0, 0x2ff, -0x2ff, -32768}) {
        actor = guardian();
        actor.targetHeading = static_cast<std::uint16_t>(difference);
        advanceLevel5GuardianBehavior(actor, 15, fire);
        assert(shots == 0 && !actor.animatedModelPresenterEnabled);
        advanceLevel5GuardianBehavior(actor, 16, fire);
        assert(shots == 1 && !actor.animatedModelPresenterEnabled);
        shots = 0;
    }
    actor = guardian();
    actor.presenterDistance = 0x7000;
    advanceLevel5GuardianBehavior(actor, 0, fire);
    assert(shots == 1);
}
void lateralBranches() {
    unsigned shots = 0;
    const auto fire = [&shots](Level1Entity&) { ++shots; };
    for (const auto difference : {0x300, -0x300, 0x2fff, -0x2fff}) {
        for (const bool contact : {false, true}) {
            auto actor = guardian();
            actor.targetHeading = static_cast<std::uint16_t>(difference);
            actor.collisionContactIsPlayer = contact;
            actor.yawVelocity = 99;
            advanceLevel5GuardianBehavior(actor, 0, fire);
            const auto amount = (difference > 0 ? 120 : -120) * (contact ? -1 : 1);
            assert(actor.position.x == 1000 + amount && actor.position.y == 2000);
            assert(actor.yawVelocity == 0 && actor.stateBytes4dTo52[2] == 2 && shots == 0);
            assert(!actor.collisionContactIsPlayer && actor.animatedModelPresenterEnabled);
            assert(actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation);
        }
    }
    for (const auto yaw : {0x1000, 0x4000, 0x8000, 0xffff}) {
        auto actor = guardian();
        actor.yaw = static_cast<std::uint16_t>(yaw);
        actor.targetHeading = static_cast<std::uint16_t>(yaw + 0x800);
        actor.collisionContactAllocationIndex = 3;
        Level1Entity expected{};
        expected.yaw = static_cast<std::uint16_t>(yaw + 0x4000);
        expected.forwardVelocity = -120;
        advanceEntityKinematics(expected);
        advanceLevel5GuardianBehavior(actor, 0, fire);
        assert(actor.position.x == 1000 + expected.position.x && actor.position.y == 2000 + expected.position.y);
        assert(actor.collisionContactAllocationIndex == static_cast<std::size_t>(-1));
    }
}
void turnBranches() {
    const auto noFire = [](Level1Entity&) { assert(false); };
    for (const auto difference : {0x3000, -0x3000, 0x7fff, -0x7fff}) {
        auto actor = guardian();
        actor.targetHeading = static_cast<std::uint16_t>(difference);
        advanceLevel5GuardianBehavior(actor, 0, noFire);
        assert(actor.yawVelocity == (difference > 0 ? 160 : -160));
        assert(actor.roll == 0 && actor.stateBytes4dTo52[2] == 1 && actor.animatedModelPresenterEnabled);
    }
    for (const auto distance : {0x1000, 0xfff}) {
        auto actor = guardian();
        actor.presenterDistance = distance;
        actor.targetHeading = 0x300;
        advanceLevel5GuardianBehavior(actor, 0, noFire);
        assert(actor.yawVelocity == 160 && actor.stateBytes4dTo52[2] == 1);
    }
    auto actor = guardian();
    actor.presenterDistance = 0x1000;
    actor.targetHeading = 0x1000;
    actor.position.z = 500;
    actor.roll = 1;
    advanceLevel5GuardianBehavior(actor, 0, noFire);
    assert(actor.roll == static_cast<std::uint16_t>(-10));
    actor = guardian();
    actor.presenterDistance = 0x1000;
    actor.targetHeading = static_cast<std::uint16_t>(-0x1000);
    actor.position.z = 499;
    actor.roll = 77;
    advanceLevel5GuardianBehavior(actor, 0, noFire);
    assert(actor.roll == 77 && actor.yawVelocity == -160);
}
}
int main() {
    callbackBranches();
    lateralBranches();
    turnBranches();
}
