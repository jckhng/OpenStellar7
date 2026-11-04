#include "stellar7/visibility_state.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>

int main() {
    using namespace stellar7;
    VisibilityState state;
    assert(state.refreshRequested() && !state.lastCamera());
    std::vector<Level1Entity> actors(7);
    for (auto& actor : actors) {
        actor.flags = 0x8f;
        actor.stateBytes4dTo52[1] = 1;
    }
    actors[1].position.x = 64999;
    actors[2].position.x = 65000;
    actors[3].position.y = -65000;
    actors[4].position.x = -32768;
    actors[5].position.x = 65536;
    actors[6].position.x = std::numeric_limits<std::int32_t>::min();
    const std::vector<std::size_t> allocation{6, 3, 1, 4, 0, 5, 2};
    const auto initial = state.refresh({}, actors, allocation, {});
    assert(initial.refreshed);
    assert((initial.activeOrder == std::vector<std::size_t>{6, 1, 4, 0}));
    assert(!state.refreshRequested() && state.lastCamera() == FixedPosition{});
    assert(actors[1].flags == 0x8f && actors[2].flags == 0x8b);
    assert(actors[3].flags == 0x8b && actors[5].flags == 0x8b);
    const std::vector<std::size_t> sorted{0, 4, 1, 6};
    const auto retained = state.refresh({}, actors, allocation, sorted);
    assert(!retained.refreshed && retained.activeOrder == sorted);

    // Pool reset replaces records; the session-owned visibility state survives.
    actors.assign(7, Level1Entity{});
    for (std::size_t index = 0; index < actors.size(); ++index) {
        auto& actor = actors[index];
        actor.stateBytes4dTo52[1] = 1;
        actor.flags = static_cast<std::uint8_t>(0x80U | index);
        actor.position = {static_cast<std::int32_t>(index * 7),
            -static_cast<std::int32_t>(index * 5), static_cast<std::int32_t>(index)};
        state.allocationSucceeded();
    }
    assert(state.refreshRequested() && state.lastCamera() == FixedPosition{});
    const FixedPosition newCamera{123, -456, 789};
    const auto nextLevel = state.refresh(newCamera, actors, allocation, {});
    assert(nextLevel.activeOrder == allocation && state.lastCamera() == newCamera);
    state.removalSucceeded();
    assert(state.refreshRequested() && state.lastCamera() == newCamera);
    actors[2].stateBytes4dTo52[1] = 0;
    auto rejected = false;
    try {
        state.refresh(newCamera, actors, allocation, nextLevel.activeOrder);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && state.refreshRequested() && state.lastCamera() == newCamera);
    actors[2].stateBytes4dTo52[1] = 1;
    state.refresh(newCamera, actors, allocation, nextLevel.activeOrder);
    std::vector<std::uint8_t> flagsBeforeRetained;
    std::vector<FixedPosition> positionsBeforeRetained;
    for (const auto& actor : actors) {
        flagsBeforeRetained.push_back(actor.flags);
        positionsBeforeRetained.push_back(actor.position);
    }
    const auto assertRetained = [&](const FixedPosition movedCamera) {
        const auto retainedResult = state.refresh(movedCamera, actors, allocation,
            nextLevel.activeOrder);
        assert(!retainedResult.refreshed);
        assert(retainedResult.activeOrder == nextLevel.activeOrder);
        assert(!state.refreshRequested() && state.lastCamera() == newCamera);
        for (std::size_t index = 0; index < actors.size(); ++index) {
            assert(actors[index].flags == flagsBeforeRetained[index]);
            assert(actors[index].position == positionsBeforeRetained[index]);
        }
    };
    assertRetained({newCamera.x + 1023, newCamera.y + 1023, newCamera.z});
    assertRetained({newCamera.x + 1023, newCamera.y - 1023, newCamera.z});
    assertRetained({newCamera.x - 1023, newCamera.y + 1023, newCamera.z});
    assertRetained({newCamera.x - 1023, newCamera.y - 1023, newCamera.z});
    const auto assertRejectedCamera = [&](const FixedPosition rejectedCamera) {
        auto cameraRejected = false;
        try {
            state.refresh(rejectedCamera, actors, allocation, nextLevel.activeOrder);
        } catch (const std::invalid_argument&) {
            cameraRejected = true;
        }
        assert(cameraRejected && !state.refreshRequested() && state.lastCamera() == newCamera);
        for (std::size_t index = 0; index < actors.size(); ++index) {
            assert(actors[index].flags == flagsBeforeRetained[index]);
            assert(actors[index].position == positionsBeforeRetained[index]);
        }
    };
    assertRejectedCamera({newCamera.x + 1024, newCamera.y, newCamera.z});
    assertRejectedCamera({newCamera.x - 1024, newCamera.y, newCamera.z});
    assertRejectedCamera({newCamera.x, newCamera.y + 1024, newCamera.z});
    assertRejectedCamera({newCamera.x, newCamera.y - 1024, newCamera.z});
    assertRejectedCamera({newCamera.x, newCamera.y, newCamera.z + 1});
    rejected = false;
    try {
        state.refresh(newCamera, actors, {0, 0}, {});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    const auto savedCamera = state.lastCamera();
    const auto savedFlags = actors[0].flags;
    state.invalidate();
    assert(state.tracking() == VisibilityTracking::unknown);
    state.allocationSucceeded();
    assert(state.tracking() == VisibilityTracking::unknown);
    rejected = false;
    try {
        state.refresh({}, actors, allocation, {});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && state.lastCamera() == savedCamera && actors[0].flags == savedFlags);

    VisibilityState native;
    constexpr std::size_t pooledActors = 48;
    std::vector<Level1Entity> pooled(pooledActors);
    std::vector<std::size_t> poolOrder;
    for (std::size_t index = 0; index < pooled.size(); ++index) {
        pooled[index].stateBytes4dTo52[1] = 1;
        poolOrder.push_back(index);
    }
    pooled.back().position.x = 65000;
    const auto nativeInitial = native.refresh({}, pooled, poolOrder, {}, VisibilityDomain::nativeGameplay);
    assert(nativeInitial.activeOrder.size() == pooledActors - 1);
    const auto near = native.refresh({1023, 0, 0}, pooled, poolOrder, nativeInitial.activeOrder,
        VisibilityDomain::nativeGameplay);
    assert(!near.refreshed && near.activeOrder == nativeInitial.activeOrder);
    const auto moved = native.refresh({1024, 0, 0}, pooled, poolOrder, near.activeOrder,
        VisibilityDomain::nativeGameplay);
    assert(moved.refreshed && moved.activeOrder == poolOrder);
    assert(native.lastCamera() == FixedPosition({1024, 0, 0}));
    const auto reverse = native.refresh({}, pooled, poolOrder, moved.activeOrder, VisibilityDomain::nativeGameplay);
    assert(reverse.refreshed && reverse.activeOrder == nativeInitial.activeOrder);
    VisibilityState nativeY;
    auto yActors = pooled;
    yActors.back().position = {0, 65000, 0};
    const auto initialY = nativeY.refresh({}, yActors, poolOrder, {}, VisibilityDomain::nativeGameplay);
    const auto nearY = nativeY.refresh({0, 1023, 0}, yActors, poolOrder, initialY.activeOrder,
        VisibilityDomain::nativeGameplay);
    assert(!nearY.refreshed && nearY.activeOrder == initialY.activeOrder);
    const auto movedY = nativeY.refresh({0, 1024, 0}, yActors, poolOrder, nearY.activeOrder,
        VisibilityDomain::nativeGameplay);
    assert(movedY.refreshed && movedY.activeOrder == poolOrder);
    VisibilityState qualified;
    rejected = false;
    try { qualified.refresh({}, pooled, poolOrder, {}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    pooled.push_back(pooled.front());
    poolOrder.push_back(pooledActors);
    rejected = false;
    try { native.refresh({}, pooled, poolOrder, reverse.activeOrder, VisibilityDomain::nativeGameplay); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);

    //3768 has no Z trigger. A Jump must preserve its inherited list/cache until
    //dirty or either planar axis reaches1024; then the whole XYZ pose is copied.
    VisibilityState jumping;
    std::vector<Level1Entity> jumpActors(48);
    std::vector<std::size_t> jumpOrder;
    for (std::size_t index = 0; index < jumpActors.size(); ++index) {
        jumpActors[index].stateBytes4dTo52[1] = 1;
        jumpOrder.push_back(index);
    }
    jumpActors.back().position.x = 65000;
    const auto groundList = jumping.refresh({},jumpActors,jumpOrder,{},VisibilityDomain::playableCampaign);
    assert(groundList.refreshed && groundList.activeOrder.size() == 47);
    const auto assertJumpRejected = [&](const FixedPosition camera) {
        const auto oldCamera = jumping.lastCamera();
        const auto oldRequest = jumping.refreshRequested();
        std::vector<std::uint8_t> oldFlags;
        for (const auto& actor : jumpActors) { oldFlags.push_back(actor.flags); }
        bool failed = false;
        try { jumping.refresh(camera,jumpActors,jumpOrder,groundList.activeOrder,VisibilityDomain::playableCampaign); }
        catch (const std::invalid_argument&) { failed = true; }
        assert(failed && jumping.lastCamera() == oldCamera && jumping.refreshRequested() == oldRequest);
        for (std::size_t index = 0; index < jumpActors.size(); ++index) { assert(jumpActors[index].flags == oldFlags[index]); }
    };
    for (const auto height : {-1,2531}) { assertJumpRejected({0,0,height}); }
    for (const auto height : {220,2530,0}) {
        const auto retainedJump = jumping.refresh({1023,0,height},jumpActors,jumpOrder,groundList.activeOrder,
            VisibilityDomain::playableCampaign);
        assert(!retainedJump.refreshed && retainedJump.activeOrder == groundList.activeOrder);
        assert(jumping.lastCamera() == FixedPosition{});
    }
    const auto movedJump = jumping.refresh({1024,0,2530},jumpActors,jumpOrder,groundList.activeOrder,
        VisibilityDomain::playableCampaign);
    assert(movedJump.refreshed && movedJump.activeOrder == jumpOrder);
    assert(jumping.lastCamera() == FixedPosition({1024,0,2530}));
    const auto descendingJump = jumping.refresh({1024,0,220},jumpActors,jumpOrder,movedJump.activeOrder,
        VisibilityDomain::playableCampaign);
    assert(!descendingJump.refreshed && descendingJump.activeOrder == movedJump.activeOrder);
    assert(jumping.lastCamera() == FixedPosition({1024,0,2530}));
    jumping.allocationSucceeded();
    const auto dirtyJump = jumping.refresh({1024,0,220},jumpActors,jumpOrder,movedJump.activeOrder,
        VisibilityDomain::playableCampaign);
    assert(dirtyJump.refreshed && jumping.lastCamera() == FixedPosition({1024,0,220}));
    for (const auto oldDomain : {VisibilityDomain::qualifiedFrame,VisibilityDomain::nativeGameplay}) {
        VisibilityState old;
        std::vector<Level1Entity> oldActors(1);
        oldActors[0].stateBytes4dTo52[1] = 1;
        old.refresh({},oldActors,{0},{},oldDomain);
        bool failed = false;
        try { old.refresh({0,0,220},oldActors,{0},{0},oldDomain); }
        catch (const std::invalid_argument&) { failed = true; }
        assert(failed && old.lastCamera() == FixedPosition{} && !old.refreshRequested());
    }
}
