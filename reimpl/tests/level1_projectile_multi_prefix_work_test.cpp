#include "stellar7/gameplay.hpp"
#include "stellar7/level1_primary_fire_work.hpp"
#include "stellar7/simulation_prefix_work.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;

template<class Function>
void rejects(Function function) {
    bool rejected = false;
    try { function(); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

struct Ground final : Level1FrameRenderer {
    void render(Level1RenderState state) override {
        state.ground.advance(state.player.yawVelocity,state.player.forwardVelocity,state.rng);
    }
};

Level1ProjectilePrefixInput initial() {
    auto model = std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    Ground ground;
    model->tickRetainedLevel1({},ground);
    model->finishLevel1Frame();
    auto state = std::make_unique<Level1State>(model->level1State());
    const auto fire = level1PrimaryFireOperations({state->player(),state->projectiles(),
        state->playerWeaponCooldown(),state->playerProjectileTimers(),0,Level1State::entityCount,
        {},false,{}, {256,0,134}});
    assert(fire.launch && state->firePrimary());
    return {state->player(),state->entities(),state->projectiles(),{}, {},{},false,state->rng(),
        0,false,0,true,{false,false}};
}

Level1ProjectilePrefixInput shots(const Level1ProjectilePrefixInput& base,const std::size_t count,
    const bool pending) {
    auto input = base;
    for (std::size_t index = 0; index < Level1State::entityCount; ++index) {
        input.allocationOrder.push_back({Level1PresentationKind::entity,index});
    }
    for (std::size_t slot = 0; slot < count; ++slot) {
        input.projectiles[slot] = base.projectiles[0];
        input.projectiles[slot].poolSlot = slot;
        input.allocationOrder.push_back({Level1PresentationKind::projectile,slot});
    }
    input.activeOrder = input.allocationOrder;
    if (pending) { input.activeOrder.pop_back(); }
    input.refreshRequested = pending;
    return input;
}

void operations(const Level1ProjectilePrefixWork& work,const std::uint32_t before,
    const std::uint32_t after = 0) {
    std::vector<NormalWorkOperation> canonical;
    for (const auto& operation : work.operations) {
        if (!canonical.empty() && operation.kind == NormalWorkKind::instructions
            && canonical.back().kind == operation.kind) {
            canonical.back().count += operation.count;
        } else { canonical.push_back(operation); }
    }
    assert(canonical.size() == (after ? 3 : 1));
    assert(canonical[0].kind == NormalWorkKind::instructions && canonical[0].count == before);
    if (after) {
        assert(canonical[1].kind == NormalWorkKind::repeatString && canonical[1].count == 6);
        assert(canonical[2].kind == NormalWorkKind::instructions && canonical[2].count == after);
    }
}

void updated(const Level1ProjectilePrefixInput& input,const Level1ProjectilePrefixWork& work) {
    assert(work.allocationOrder == input.allocationOrder && work.rng.calls() == 32);
    for (const auto reference : input.allocationOrder) {
        if (reference.kind != Level1PresentationKind::projectile) { continue; }
        const auto& old = input.projectiles[reference.index];
        const auto& shot = work.projectiles[reference.index];
        const auto listed = std::find(input.activeOrder.begin(),input.activeOrder.end(),reference)
            != input.activeOrder.end();
        assert(shot.poolSlot == reference.index && shot.ownerIsPlayer);
        assert(shot.stateBytes4dTo54[1] == old.stateBytes4dTo54[1] - (listed ? 1 : 0));
        assert(shot.position.x == old.position.x && shot.position.z == 0);
        assert(shot.position.y == old.position.y + (listed ? 500 : 0));
        assert(shot.presenterDistance == (listed ? static_cast<std::uint32_t>(shot.position.y) : old.presenterDistance));
        if (!listed) { assert(shot.targetHeading == old.targetHeading); }
    }
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    const auto base = initial();
    auto input = shots(base,2,true);
    input.projectiles[0].position.y = 4000;
    input.projectiles[0].stateBytes4dTo54[1] = 56;
    input.projectiles[0].presenterDistance = 4000;
    input.projectiles[0].targetHeading = 0x8000;
    const auto pending = level1ProjectilePrefixOperations(input);
    operations(pending,6091,458);
    updated(input,pending);
    assert(pending.activeOrder == input.allocationOrder && !pending.refreshRequested);

    input = shots(base,2,false);
    input.activeOrder = {{Level1PresentationKind::projectile,1},{Level1PresentationKind::entity,2},
        {Level1PresentationKind::projectile,0},{Level1PresentationKind::entity,3},
        {Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},
        {Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0}};
    const auto interleaved = level1ProjectilePrefixOperations(input);
    operations(interleaved,7462);
    updated(input,interleaved);
    std::reverse(input.activeOrder.begin(),input.activeOrder.end());
    operations(level1ProjectilePrefixOperations(input),7433);

    input = shots(base,Level1State::projectilePoolSize,false);
    const auto coincident = level1ProjectilePrefixOperations(input);
    operations(coincident,17695);
    updated(input,coincident);
    //FD filtering, rather than spatial separation, permits all eight coincident shots.
    for (const auto& shot : coincident.projectiles) {
        assert((shot.position == FixedPosition{0,500,0}) && shot.stateBytes4dTo54[1] == 63);
    }
    std::reverse(input.activeOrder.begin(),input.activeOrder.end());
    const auto reversed = level1ProjectilePrefixOperations(input);
    operations(reversed,17677);
    assert(reversed.activeOrder != coincident.activeOrder);
    updated(input,reversed);

    input = shots(base,Level1State::projectilePoolSize,true);
    input.activeOrder = {{Level1PresentationKind::projectile,6},{Level1PresentationKind::entity,2},
        {Level1PresentationKind::projectile,5},{Level1PresentationKind::entity,3},
        {Level1PresentationKind::projectile,4},{Level1PresentationKind::entity,4},
        {Level1PresentationKind::projectile,3},{Level1PresentationKind::entity,5},
        {Level1PresentationKind::projectile,2},{Level1PresentationKind::entity,1},
        {Level1PresentationKind::projectile,1},{Level1PresentationKind::entity,0},
        {Level1PresentationKind::projectile,0}};
    const auto lastAppend = level1ProjectilePrefixOperations(input);
    operations(lastAppend,15700,788);
    updated(input,lastAppend);
    assert(lastAppend.activeOrder == input.allocationOrder);

    auto changed = input;
    changed.activeOrder.pop_back(); //Two pending appends exceed the retained launch boundary.
    rejects([&]() { level1ProjectilePrefixOperations(changed); });
    changed = input;
    changed.projectiles.back().stateBytes4dTo54[1] = 63;
    rejects([&]() { level1ProjectilePrefixOperations(changed); });
    changed = input;
    changed.allocationOrder.back() = changed.allocationOrder[Level1State::entityCount];
    rejects([&]() { level1ProjectilePrefixOperations(changed); });
    changed = input;
    changed.activeOrder.back() = changed.activeOrder.front();
    rejects([&]() { level1ProjectilePrefixOperations(changed); });
    changed = input;
    changed.projectiles[3].stateBytes4dTo54[1] = 1;
    rejects([&]() { level1ProjectilePrefixOperations(changed); });
    changed = input;
    changed.projectiles[3].ownerIsPlayer = false;
    rejects([&]() { level1ProjectilePrefixOperations(changed); });
    changed = input;
    changed.projectiles[3].position.z = -301;
    rejects([&]() { level1ProjectilePrefixOperations(changed); });
    changed = input;
    changed.projectiles[3].position = changed.entities[0].position;
    changed.projectiles[3].position.y -= 500;
    rejects([&]() { level1ProjectilePrefixOperations(changed); });
}
