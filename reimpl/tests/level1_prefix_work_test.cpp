#include "stellar7/gameplay.hpp"
#include "stellar7/simulation_prefix_work.hpp"
#include "../src/level1_rebuild_abs.hpp"
#include <cstdio>

#include <algorithm>
#include <array>
#include <cassert>
#include <memory>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
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

stellar7::Level1PrefixInput input(const stellar7::Level1State& state, const bool first) {
    return {state.player(), state.entities(), {0, 1, 2, 3, 4, 5}, state.activeListOrder(),
        {}, first, state.rng(), 0, false, 0, true, {false, false}};
}

void sameActor(const stellar7::Level1Entity& actual, const stellar7::Level1Entity& expected) {
    assert(actual.position == expected.position);
    assert(actual.pitch == expected.pitch && actual.roll == expected.roll && actual.yaw == expected.yaw);
    assert(actual.pitchVelocity == expected.pitchVelocity && actual.rollVelocity == expected.rollVelocity);
    assert(actual.yawVelocity == expected.yawVelocity && actual.forwardVelocity == expected.forwardVelocity);
    assert(actual.xVelocity == expected.xVelocity && actual.yVelocity == expected.yVelocity);
    assert(actual.zVelocity == expected.zVelocity && actual.stateBytes4dTo52 == expected.stateBytes4dTo52);
    assert(actual.behaviorTimer == expected.behaviorTimer && actual.weaponCooldown == expected.weaponCooldown);
    assert(actual.weaponSlotTimers == expected.weaponSlotTimers);
    assert(actual.presenterDistance == expected.presenterDistance && actual.targetHeading == expected.targetHeading);
    assert(actual.collisionContactAllocationIndex == expected.collisionContactAllocationIndex);
    assert(actual.collisionContactIsPlayer == expected.collisionContactIsPlayer);
}

std::uint64_t ordinaryDispatches(const stellar7::Level1PrefixWork& work) {
    std::uint64_t result = 0;
    for (const auto& operation : work.operations) {
        assert(operation.kind == stellar7::NormalWorkKind::instructions);
        result += operation.count;
    }
    return result;
}

class Compare final : public stellar7::Level1FrameRenderer {
public:
    explicit Compare(const stellar7::Level1PrefixWork& expected) : expected_(expected) {}

    void render(stellar7::Level1RenderState state) override {
        sameActor(state.player, expected_.player);
        assert(state.order == expected_.activeOrder);
        assert(state.entities.size() == expected_.actors.size());
        for (std::size_t index = 0; index < state.entities.size(); ++index) {
            sameActor(state.entities[index], expected_.actors[index]);
        }
        auto actual = state.rng;
        auto expected = expected_.rng;
        assert(actual.calls() == expected.calls());
        for (std::size_t index = 0; index < stellar7::StellarRng::wordCount * 2; ++index) {
            assert(actual.next() == expected.next());
        }
        state.ground.advance(state.player.yawVelocity, state.player.forwardVelocity, state.rng);
    }

private:
    const stellar7::Level1PrefixWork& expected_;
};
void checkRebuildAbsoluteFamily(const stellar7::Level1PrefixInput& base) {
    using namespace stellar7;
    struct AbsoluteControl {std::uint32_t raw,magnitude;std::uint8_t branch,stores;};
    //PC paths388A..38C2/38D4..390C, including low8000/high!=FFFF and wrap.
    constexpr std::array<AbsoluteControl,10> controls{{
        {0,0,3,2},{0x8000,0x8000,3,2},{0x18000,0x18000,3,2},
        {0xffffffffU,1,7,2},{0xffff8001U,0x7fff,7,2},
        {0xffff8000U,0x7fff,7,0},{0xffff7fffU,0x8001,7,2},
        {0xfffe8000U,0x18000,9,2},{0x80008000U,0x7fff8000,9,2},
        {0x80000000U,0x80000000U,7,2}
    }};
    for(const auto c:controls) {
        const auto work=detail::level1RebuildAbsolute(c.raw);
        assert(work.magnitude==c.magnitude && work.branchDispatches==c.branch && work.storeDispatches==c.stores);
    }
    struct RebuildControl {FixedPosition position;std::uint8_t xBranch,xStores,yBranch,yStores;};
    constexpr std::array<RebuildControl,6> rebuild{{
        {{32768,16384,0},7,0,7,2},{{16384,32768,0},7,2,7,0},
        {{32768,32768,0},7,0,7,0},{{-32768,-32768,0},3,2,3,2},
        {{32767,32769,0},7,2,7,2},{{32769,32767,0},7,2,7,2}
    }};
    assert(base.activeOrder.empty() && base.refreshRequested && base.player.position==FixedPosition{});
    for(const auto c:rebuild) {
        auto controlled=base;controlled.actors[2].position=c.position;
        Level1PrefixWork actual;
        try {actual=level1PrefixOperations(controlled);}
        catch(const std::exception& error) {
            std::fprintf(stderr,"3768 rebuild (%d,%d): %s\n",c.position.x,c.position.y,error.what());throw;
        }
        assert(actual.activeOrder==controlled.allocationOrder && actual.actors[2].position==c.position);
        assert(!actual.refreshRequested && actual.lastListCamera==actual.player.position);
        const auto copy=std::find_if(actual.operations.begin(),actual.operations.end(),[](const auto& op) {
            return op.kind==NormalWorkKind::repeatString && op.count==6;
        });
        assert(copy!=actual.operations.end());
        const std::array<std::array<unsigned,4>,6> rows{{
            {{3,2,7,2}},{{7,2,7,2}},{{c.xBranch,c.xStores,c.yBranch,c.yStores}},
            {{7,2,3,2}},{{3,2,7,2}},{{3,2,3,2}}
        }};
        std::vector<NormalWorkOperation> expected{{NormalWorkKind::instructions,8}};
        for(std::size_t i=0;i<rows.size();++i) {
            for(const auto count:{19U,rows[i][0],rows[i][1]+7U,rows[i][2],rows[i][3]+10U,i+1==rows.size()?4U:5U}) {
                expected.push_back({NormalWorkKind::instructions,count});
            }
        }
        expected.push_back({NormalWorkKind::instructions,6});expected.push_back({NormalWorkKind::instructions,5});
        const std::vector<NormalWorkOperation> suffix(copy+1,actual.operations.end());
        assert(suffix.size()==expected.size());
        for(std::size_t i=0;i<expected.size();++i) {
            assert(suffix[i].kind==expected[i].kind && suffix[i].count==expected[i].count && suffix[i].value==expected[i].value);
        }
    }
    auto unsupported=base;unsupported.actors[2].position.x=65000;
    rejects([&]{level1PrefixOperations(unsupported);});
    unsupported=base;unsupported.refreshRequested=false;unsupported.lastListCamera.x=32768;
    rejects([&]{level1PrefixOperations(unsupported);});
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    auto model = std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    const auto firstInput = input(model->level1State(), true);
    const auto first = level1PrefixOperations(firstInput);
    checkRebuildAbsoluteFamily(firstInput);
    assert(first.rng.calls() == 16 && first.activeOrder == firstInput.allocationOrder);
    assert(!first.refreshRequested && first.lastListCamera == FixedPosition{});
    //The final FE actor retains the loader's special bytes; FF dispatch alone reads+51.
    assert(first.actors.back().stateBytes4dTo52[1] == 16);
    assert(first.actors.back().stateBytes4dTo52[4] == static_cast<std::uint8_t>(-13));
    bool cameraCopy = false;
    for (const auto& operation : first.operations) {
        if (operation.kind == NormalWorkKind::repeatString) {
            assert(!cameraCopy && operation.count == 6);
            cameraCopy = true;
        }
    }
    assert(cameraCopy);
    Compare compareFirst(first);
    model->tickRetainedLevel1({}, compareFirst);
    model->finishLevel1Frame();
    const auto nextInput = input(model->level1State(), false);
    const auto next = level1PrefixOperations(nextInput);
    constexpr std::uint64_t sourceAdjacentDispatches = 4875;
    std::uint64_t adjacentDispatches = 0;
    for (const auto& operation : next.operations) {
        assert(operation.kind == NormalWorkKind::instructions);
        adjacentDispatches += operation.count;
    }
    assert(adjacentDispatches == sourceAdjacentDispatches);
    assert(nextInput.rng.calls() == 29 && next.rng.calls() == 32);
    assert((next.actors[0].position == FixedPosition{-1841, 1961, 0}));
    assert(next.actors[0].forwardVelocity == 0 && next.actors[0].yawVelocity == -143);
    assert(next.actors[0].stateBytes4dTo52[5] == 2);
    assert(next.actors[1].yawVelocity == 0 && next.actors[1].behaviorTimer == 10);
    Compare compareNext(next);
    model->tickRetainedLevel1({}, compareNext);
    model->finishLevel1Frame();
    bool liftSteered = false;
    bool liftBanked = false;
    for (unsigned frame = 2; frame < 15; ++frame) {
        const auto continuing = level1PrefixOperations(input(model->level1State(), false));
        liftSteered = liftSteered || continuing.actors[1].yawVelocity != 0;
        liftBanked = liftBanked || continuing.actors[1].roll != 0;
        assert(continuing.actors.back().stateBytes4dTo52[1] == 16);
        assert(continuing.actors.back().stateBytes4dTo52[4] == static_cast<std::uint8_t>(-13));
        Compare compare(continuing);
        model->tickRetainedLevel1({}, compare);
        model->finishLevel1Frame();
    }
    assert(liftSteered && liftBanked);

    //Pinned01D0->02A3 controls mutate only behavior/timer/cooldown in the adjacent input.
    constexpr std::array<std::uint64_t, 6> sourceTankBehaviorDispatches{
        4870, 4772, 4822, 4824, 4763, 4778};
    constexpr std::array<std::uint64_t, 6> sourceLiftBehaviorDispatches{
        4870, 4884, 4830, 4886, 4826, 4841};
    for (std::size_t behavior = 0; behavior < sourceTankBehaviorDispatches.size(); ++behavior) {
        auto controlled = nextInput;
        controlled.actors[0].stateBytes4dTo52[5] = static_cast<std::uint8_t>(behavior);
        controlled.actors[0].weaponCooldown = 255;
        assert(ordinaryDispatches(level1PrefixOperations(controlled)) == sourceTankBehaviorDispatches[behavior]);
        controlled = nextInput;
        controlled.actors[0].weaponCooldown = 255;
        controlled.actors[1].stateBytes4dTo52[5] = static_cast<std::uint8_t>(behavior);
        controlled.actors[1].weaponCooldown = 255;
        assert(ordinaryDispatches(level1PrefixOperations(controlled)) == sourceLiftBehaviorDispatches[behavior]);
    }
    constexpr std::array<std::uint16_t, 6> sourceBankYaws{0, 0x4000, 0x8000, 0xc000, 0x2345, 0xabcd};
    constexpr std::array<std::uint64_t, 6> sourceBankDispatches{5033, 5031, 4981, 4975, 5033, 4981};
    for (std::size_t index = 0; index < sourceBankYaws.size(); ++index) {
        auto banked = nextInput;
        banked.actors[0].weaponCooldown = 255;
        banked.actors[1].roll = 0xff00;
        banked.actors[1].yaw = sourceBankYaws[index];
        banked.actors[1].stateBytes4dTo52[5] = 1;
        assert(ordinaryDispatches(level1PrefixOperations(banked)) == sourceBankDispatches[index]);
    }

    auto moving = nextInput;
    moving.player.forwardVelocity = 100;
    moving.player.yaw = 0x1200;
    moving.player.yawVelocity = 80;
    const auto moved = level1PrefixOperations(moving);
    sameActor(moved.player, flatKinematicsOperations(moving.player).actor);
    assert(moved.lastListCamera == moving.lastListCamera);
    moving.player.position.x = 2000;
    const auto rebuilt = level1PrefixOperations(moving);
    assert(rebuilt.lastListCamera == rebuilt.player.position);
    assert(rebuilt.activeOrder == moving.allocationOrder);
    assert(!rebuilt.refreshRequested);

    auto changed = firstInput;
    changed.refreshRequested = false;
    rejects([&]() { level1PrefixOperations(changed); });
    changed = nextInput;
    changed.refreshRequested = true;
    assert(level1PrefixOperations(changed).activeOrder == changed.allocationOrder);
    changed = nextInput;
    changed.activeOrder[1] = changed.activeOrder[0];
    rejects([&]() { level1PrefixOperations(changed); });
    changed = firstInput;
    changed.allocationOrder[0] = 1;
    rejects([&]() { level1PrefixOperations(changed); });
    changed = nextInput;
    changed.fadeTicks = 1;
    rejects([&]() { level1PrefixOperations(changed); });
    changed = nextInput;
    changed.projectileHandlePresent[0] = true;
    rejects([&]() { level1PrefixOperations(changed); });
    changed = nextInput;
    changed.player.forwardVelocity = 201;
    rejects([&]() { level1PrefixOperations(changed); });
    changed = nextInput;
    changed.actors[1].stateBytes4dTo52[4] = 4;
    rejects([&]() { level1PrefixOperations(changed); });
    changed = nextInput;
    changed.actors[1].position = changed.actors[0].position;
    changed.actors[1].position.z = 1000;
    rejects([&]() { level1PrefixOperations(changed); });
    changed = nextInput;
    changed.player.yawVelocity = 513;
    rejects([&]() { level1PrefixOperations(changed); });
    changed = nextInput;
    changed.actors[0].stateBytes4dTo52[5] = 6;
    rejects([&]() { level1PrefixOperations(changed); });
    changed = nextInput;
    changed.lastListCamera.x = -40000;
    rejects([&]() { level1PrefixOperations(changed); });
    changed = nextInput;
    changed.actors[0].stateBytes4dTo52[5] = 2;
    changed.actors[0].forwardVelocity = 0;
    changed.actors[0].yaw = presenterTargetHeading(changed.actors[0].position, changed.player.position);
    rejects([&]() { level1PrefixOperations(changed); });
}
