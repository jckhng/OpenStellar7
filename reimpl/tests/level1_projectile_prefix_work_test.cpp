#include "stellar7/gameplay.hpp"
#include "stellar7/level1_primary_fire_work.hpp"
#include "stellar7/level1_world_work.hpp"
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
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

class Ground final : public Level1FrameRenderer {
public:
    void render(Level1RenderState state) override {
        state.ground.advance(state.player.yawVelocity, state.player.forwardVelocity, state.rng);
    }
};

Level1ProjectilePrefixInput nextInput(const Level1ProjectilePrefixWork& work) {
    return {work.player, work.entities, work.projectiles, work.allocationOrder, work.activeOrder,
        work.lastListCamera, work.refreshRequested, work.rng, 0, false, 0, true, {false, false}};
}

std::vector<NormalWorkOperation> canonical(const std::vector<NormalWorkOperation>& operations) {
    std::vector<NormalWorkOperation> result;
    for (const auto& operation : operations) {
        if (!result.empty() && operation.kind == NormalWorkKind::instructions
            && result.back().kind == operation.kind) {
            result.back().count += operation.count;
        } else {
            result.push_back(operation);
        }
    }
    return result;
}

void ordinaryWork(const Level1ProjectilePrefixWork& work, const std::uint64_t expected) {
    const auto operations = canonical(work.operations);
    assert(operations.size() == 1 && operations[0].kind == NormalWorkKind::instructions);
    assert(operations[0].count == expected);
}

void sameShot(const Level1Projectile& first, const Level1Projectile& second) {
    assert(first.poolSlot == second.poolSlot && first.flags == second.flags);
    assert(first.type == second.type && first.collisionRadius == second.collisionRadius);
    assert(first.position == second.position && first.pitch == second.pitch);
    assert(first.roll == second.roll && first.yaw == second.yaw);
    assert(first.pitchVelocity == second.pitchVelocity && first.rollVelocity == second.rollVelocity);
    assert(first.yawVelocity == second.yawVelocity && first.xVelocity == second.xVelocity);
    assert(first.yVelocity == second.yVelocity && first.zVelocity == second.zVelocity);
    assert(first.forwardVelocity == second.forwardVelocity && first.stateBytes4dTo54 == second.stateBytes4dTo54);
    assert(first.ownerIsPlayer == second.ownerIsPlayer && first.ownerAllocationIndex == second.ownerAllocationIndex);
    assert(first.presenterDistance == second.presenterDistance && first.targetHeading == second.targetHeading);
}
} // namespace

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    auto model = std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    Ground renderer;
    model->tickRetainedLevel1({}, renderer);
    model->finishLevel1Frame();
    auto owned = std::make_unique<Level1State>(model->level1State());
    const auto fire = level1PrimaryFireOperations({owned->player(), owned->projectiles(),
        owned->playerWeaponCooldown(), owned->playerProjectileTimers(), 0, Level1State::entityCount,
        {}, false, {}, {256, 0, 134}});
    assert(fire.launch && owned->firePrimary());
    Level1ProjectilePrefixInput input{owned->player(), owned->entities(), owned->projectiles(), {}, {}, {},
        fire.refreshRequested, owned->rng(), 0, false, 0, true, {false, false}};
    for (std::size_t index = 0; index < Level1State::entityCount; ++index) {
        input.allocationOrder.push_back({Level1PresentationKind::entity, index});
    }
    input.allocationOrder.push_back({Level1PresentationKind::projectile, fire.launch->poolSlot});
    for (const auto index : owned->activeListOrder()) {
        input.activeOrder.push_back({Level1PresentationKind::entity, index});
    }
    const auto first = level1ProjectilePrefixOperations(input);
    assert(first.allocationOrder == input.allocationOrder && first.activeOrder == input.allocationOrder);
    assert(!first.refreshRequested && first.lastListCamera == first.player.position);
    sameShot(first.projectiles[0], input.projectiles[0]); //Not yet in the updater/AI list.
    assert(first.projectiles[0].stateBytes4dTo54[1] == 64);
    assert(first.projectiles[0].presenterDistance == 0 && first.projectiles[0].targetHeading == 0);
    assert(first.rng.calls() == 32);
    const auto firstOperations = canonical(first.operations);
    assert(firstOperations.size() == 3);
    assert(firstOperations[0].kind == NormalWorkKind::instructions && firstOperations[0].count == 4838);
    assert(firstOperations[1].kind == NormalWorkKind::repeatString && firstOperations[1].count == 6);
    assert(firstOperations[2].kind == NormalWorkKind::instructions && firstOperations[2].count == 403);
    assert(first.entities.back().stateBytes4dTo52[1] == 16);
    assert(first.entities.back().stateBytes4dTo52[4] == static_cast<std::uint8_t>(-13));

    //Carry the native ground/animation/tail results into the next prefix.
    auto secondInput = nextInput(first);
    auto ground = owned->ground();
    ground.advance(first.player.yawVelocity, first.player.forwardVelocity, secondInput.rng);
    Model3d type11{};
    type11.flags = 1;
    type11.animationLimits = {8};
    auto& lift = secondInput.entities[1];
    const auto mainAnimation = level1AnimationOperations({lift.stateBytes4dTo52[2],
        lift.stateBytes4dTo52[3], lift.modelAnimationVariant}, type11);
    const auto radarAnimation = level1AnimationOperations(mainAnimation.state, type11);
    lift.stateBytes4dTo52[3] = radarAnimation.state.countdown;
    lift.modelAnimationVariant = radarAnimation.state.variant;
    FrameTailState tailInput{1, first.player.position, first.player.yawVelocity, first.player.forwardVelocity,
        {owned->playerWeaponCooldown(), owned->playerProjectileTimers()[0], owned->playerProjectileTimers()[1]}, {}};
    for (const auto reference : first.activeOrder) {
        if (reference.kind == Level1PresentationKind::entity) {
            const auto& actor = first.entities[reference.index];
            tailInput.actors.push_back({actor.type, actor.position});
        } else {
            const auto& shot = first.projectiles[reference.index];
            tailInput.actors.push_back({shot.type, shot.position});
        }
    }
    const FrameTailGuards guards{680, {}, {}, 0x1e, 0x1e, false, false, false, false, false, true, {}};
    const auto tail = level1ProjectileFrameTailOperations(tailInput, guards, secondInput.rng);
    secondInput.player.position = tail.state.playerPosition;
    secondInput.player.yawVelocity = tail.state.yawVelocity;
    secondInput.player.forwardVelocity = tail.state.forwardVelocity;
    secondInput.rng = tail.rng;
    for (std::size_t index = 0; index < first.activeOrder.size(); ++index) {
        const auto reference = first.activeOrder[index];
        if (reference.kind == Level1PresentationKind::entity) {
            secondInput.entities[reference.index].position = tail.state.actors[index].position;
        } else {
            secondInput.projectiles[reference.index].position = tail.state.actors[index].position;
        }
    }
    const auto second = level1ProjectilePrefixOperations(secondInput);
    assert((second.projectiles[0].position == FixedPosition{0, 500, 0}));
    assert(second.projectiles[0].stateBytes4dTo54[1] == 63 && second.projectiles[0].presenterDistance == 500);
    assert(second.projectiles[0].targetHeading == presenterTargetHeading({0, 500, 0}, second.player.position));
    assert(second.rng.calls() == 43);
    ordinaryWork(second, 6019);
    for (std::size_t slot = 1; slot < input.projectiles.size(); ++slot) {
        sameShot(second.projectiles[slot], input.projectiles[slot]);
    }
    auto changed = secondInput;
    changed.activeOrder = {{Level1PresentationKind::entity, 2}, {Level1PresentationKind::projectile, 0},
        {Level1PresentationKind::entity, 3}, {Level1PresentationKind::entity, 4},
        {Level1PresentationKind::entity, 5}, {Level1PresentationKind::entity, 1},
        {Level1PresentationKind::entity, 0}};
    ordinaryWork(level1ProjectilePrefixOperations(changed), 6009);
    changed = secondInput;
    std::reverse(changed.activeOrder.begin(), changed.activeOrder.end());
    ordinaryWork(level1ProjectilePrefixOperations(changed), 5992);

    //The same shot sees the tank before or after integration according to45D2.
    changed = secondInput;
    changed.entities[0].position = {-900, 1400, 0};
    changed.entities[0].yaw = 0xb000;
    changed.entities[0].yawVelocity = 0;
    changed.entities[0].forwardVelocity = 225;
    changed.entities[0].stateBytes4dTo52[5] = 2;
    changed.entities[0].weaponCooldown = 255;
    ordinaryWork(level1ProjectilePrefixOperations(changed), 6066);
    std::rotate(changed.activeOrder.begin(), changed.activeOrder.end() - 1, changed.activeOrder.end());
    ordinaryWork(level1ProjectilePrefixOperations(changed), 6067);
    changed.entities[0].position.y = 1300;
    level1ProjectilePrefixOperations(changed); //Pre-update X misses.
    std::rotate(changed.activeOrder.begin(), changed.activeOrder.begin() + 1, changed.activeOrder.end());
    rejects([&]() { level1ProjectilePrefixOperations(changed); }); //Post-update X/Y hit.

    changed = secondInput;
    changed.projectiles[0].stateBytes4dTo54[1] = 1;
    rejects([&]() { level1ProjectilePrefixOperations(changed); });
    changed = secondInput;
    changed.projectiles[0].position.z = -301;
    rejects([&]() { level1ProjectilePrefixOperations(changed); });
    changed = secondInput;
    changed.projectiles[0].type = 22;
    rejects([&]() { level1ProjectilePrefixOperations(changed); });
    changed = secondInput;
    changed.activeOrder[1] = changed.activeOrder[0];
    rejects([&]() { level1ProjectilePrefixOperations(changed); });
    changed = input;
    changed.refreshRequested = false;
    rejects([&]() { level1ProjectilePrefixOperations(changed); });
    changed = secondInput;
    changed.projectileHandlePresent[0] = true;
    rejects([&]() { level1ProjectilePrefixOperations(changed); });
}
