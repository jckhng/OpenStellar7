#include "stellar7/level2_periodic_spawn_work.hpp"
#include "stellar7/campaign_loader.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;

//Synthetic source-call controls; no campaign trajectory or clock qualification.
std::vector<Model3d> models() {
    std::vector<Model3d> result(44);
    for (const auto type : {1,4,8,13}) {
        auto& model = result[type];
        model.scaleShift = 0;
        model.collisionRadius = type == 1 ? 1342 : type == 4 ? 600 : type == 8 ? 812 : 732;
    }
    return result;
}

StellarRng rng(const std::uint16_t choice, const std::uint16_t placement = 0x0a07,
    const std::uint16_t spinOrHeading = 0x0311, const bool wrap = false) {
    std::array<std::uint16_t, StellarRng::wordCount> table{};
    if (wrap) {
        table[0] = choice; table[55] = placement; table[54] = spinOrHeading; table[53] = 0x4560;
        return StellarRng(table, 0, 60, 0, 17);
    }
    table[55] = choice; table[54] = placement; table[53] = spinOrHeading; table[52] = 0x4560;
    return StellarRng(table, 48, 110, 0, 17);
}

Level2PeriodicSpawnInput input(const std::size_t slot = 0) {
    Level2PeriodicSpawnInput result{};
    result.activeEnemies = 2;
    result.spawnedEnemies = 5;
    result.nextEntityIndex = slot + 2;
    //Two existing fixed logical records; ordinary physical/logical order differs.
    result.allocationOrder = {{Level1PresentationKind::entity,0},{Level1PresentationKind::entity,1}};
    for (std::size_t index = 0; index < result.ordinaryPool.size(); ++index) {
        auto& item = result.ordinaryPool[index];
        item.actor.stateBytes4dTo52[0] = 255;
        item.actor.animatedModelPresenterEnabled = false;
        item.actor.renderProfile = WorldActorRenderProfile::noHandles;
        if (index < slot) {
            item.entityIndex = index + 2;
            item.actor.allocationIndex = index + 2;
            item.actor.type = 8;
            item.actor.stateBytes4dTo52[1] = 2;
            result.allocationOrder.push_back({Level1PresentationKind::entity,index + 2});
        }
    }
    return result;
}

template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

void sameRng(StellarRng actual, StellarRng expected) {
    assert(actual.indexA() == expected.indexA() && actual.indexB() == expected.indexB());
    assert(actual.calls() == expected.calls() && actual.last() == expected.last());
    for (std::size_t index = 0; index < StellarRng::wordCount * 2; ++index) {
        assert(actual.next() == expected.next());
    }
}

std::uint64_t instructionCount(const Level2PeriodicSpawnWork& work) {
    std::uint64_t total = 0;
    for (const auto& operation : work.operations) {
        if (operation.kind == NormalWorkKind::instructions) { total += operation.count; }
    }
    return total;
}

std::vector<NormalWorkOperation> normalized(const std::vector<NormalWorkOperation>& operations) {
    std::vector<NormalWorkOperation> result;
    for (const auto operation : operations) {
        if (!result.empty() && result.back().kind == operation.kind && operation.kind == NormalWorkKind::instructions) {
            result.back().count += operation.count;
        } else { result.push_back(operation); }
    }
    return result;
}

void branches() {
    auto state = input();
    auto seed = rng(0x0105);
    state.spawnedEnemies = 12;
    auto result = level2PeriodicSpawnOperations(state, seed, {});
    assert(result.outcome == Level1PeriodicSpawnOutcome::limit && instructionCount(result) == 13);
    assert(!result.selectedType && !result.effectiveType && !result.spawn);
    assert(result.allocationOrder == state.allocationOrder);
    sameRng(result.rng, seed);

    state = input();
    //Actual row's all16 indexed bytes, including source cross-row10..12.
    for (std::int16_t active = 0; active <= 12; ++active) {
        state.activeEnemies = active;
        state.spawnedEnemies = std::max<std::int16_t>(5,active);
        const auto threshold = campaignSpawnThreshold(1, static_cast<std::uint8_t>(active));
        seed = rng(static_cast<std::uint16_t>(0x0100 + threshold));
        result = level2PeriodicSpawnOperations(state, seed, {});
        assert(result.outcome == (state.spawnedEnemies == 12 ? Level1PeriodicSpawnOutcome::limit
            : Level1PeriodicSpawnOutcome::threshold));
        assert(instructionCount(result) == (state.spawnedEnemies == 12 ? 13 : 40));
        if (state.spawnedEnemies != 12) { seed.next(); }
        sameRng(result.rng, seed);
        assert(result.allocationOrder == state.allocationOrder && !result.spawn);
    }
    state = input();
    result = level2PeriodicSpawnOperations(state,rng(5),{});
    assert(result.outcome == Level1PeriodicSpawnOutcome::highZero && instructionCount(result) == 42);
    result = level2PeriodicSpawnOperations(state,rng(0xff05),{});
    assert(result.outcome == Level1PeriodicSpawnOutcome::noType && instructionCount(result) == 200);
    assert(!result.selectedType && !result.effectiveType && !result.spawn);
    auto expected = rng(0xff05); expected.next(); sameRng(result.rng,expected);
    //No selected type means4370's forced4 branch is not entered.
    state.spawnedEnemies = 8;
    assert(level2PeriodicSpawnOperations(state,rng(0xff05),{}).outcome == Level1PeriodicSpawnOutcome::noType);
}

void constructors() {
    const auto shared = models();
    for (const auto high : {1,64,65,164,165,204,205,254}) {
        const auto type = high < 65 ? 1 : high < 165 ? 4 : high < 205 ? 8 : 13;
        const auto choice = static_cast<std::uint16_t>((high << 8U) + 5);
        for (const bool wrap : {false,true}) {
            for (const auto placement : {0x0a00,0x0a07}) {
                const auto seed = rng(choice,static_cast<std::uint16_t>(placement),0x0311,wrap);
                const auto state = input(3);
                const auto result = level2PeriodicSpawnOperations(state,seed,shared);
                assert(result.outcome == Level1PeriodicSpawnOutcome::spawned && result.spawn);
                assert(*result.selectedType == type && *result.effectiveType == type);
                const auto& birth = *result.spawn;
                assert(birth.physicalSlot == 3 && birth.logicalIndex == state.nextEntityIndex);
                assert(birth.actor.allocationIndex == birth.logicalIndex);
                assert(birth.actor.collisionRadius == shared[type].collisionRadius);
                assert(birth.actor.type == type && !birth.actor.animatedModelPresenterEnabled);
                assert(birth.actor.renderProfile == WorldActorRenderProfile::noHandles);
                const auto defaults = campaignOrdinaryDefaults(static_cast<std::uint8_t>(type));
                assert(birth.actor.stateBytes4dTo52[0] == 255 && birth.actor.stateBytes4dTo52[1] == defaults.health);
                assert(birth.actor.stateBytes4dTo52[2] == 0 && birth.actor.stateBytes4dTo52[3] == 0);
                assert(birth.actor.stateBytes4dTo52[4] == defaults.behavior && birth.actor.weaponSlotCount == defaults.weaponSlots);
                assert(birth.actor.forwardVelocity == defaults.speed.value_or(0));
                assert(birth.actor.yawVelocity == (type == 13 ? -2560 : 0));
                assert(birth.actor.yaw == (type == 13 ? 0 : 0x8000));
                const FixedPosition position = placement == 0x0a07 ? FixedPosition{-32769,-20481,0} : FixedPosition{20479,-32769,0};
                assert(birth.actor.position == position);
                assert(birth.appendedAllocationIndex == state.allocationOrder.size());
                assert(birth.activeEnemies == 3 && birth.spawnedEnemies == 6 && birth.refreshRequested);
                assert(result.allocationOrder.size() == state.allocationOrder.size() + 1);
                assert(std::equal(state.allocationOrder.begin(),state.allocationOrder.end(),result.allocationOrder.begin()));
                assert((result.allocationOrder.back() == Level1ObjectRef{Level1PresentationKind::entity,birth.logicalIndex}));
                const auto boundary = birth.cacheReset;
                assert(boundary.sourceResume == 0x43df && boundary.physicalKey == 3);
                assert(boundary.operationOffset >= 2 && boundary.operationOffset < result.operations.size());
                assert(result.operations[boundary.operationOffset-1].kind == NormalWorkKind::instructions);
                assert(result.operations[boundary.operationOffset-1].count == 4);
                assert(result.operations[boundary.operationOffset-2].kind == NormalWorkKind::repeatString);
                assert(result.operations[boundary.operationOffset-2].count == 99);
                auto expected = seed;
                for (int draw = 0; draw < (type == 13 ? 4 : 3); ++draw) { expected.next(); }
                sameRng(result.rng,expected);
            }
        }
    }
    //Static source dispatch controls: firstweight, no placement branches, no wrap.
    assert(instructionCount(level2PeriodicSpawnOperations(input(),rng(0x0105,0x0a00),shared)) == 422);
    assert(instructionCount(level2PeriodicSpawnOperations(input(),rng(0x4105,0x0a00),shared)) == 428);
    assert(instructionCount(level2PeriodicSpawnOperations(input(),rng(0xa505,0x0a00),shared)) == 436);
    assert(instructionCount(level2PeriodicSpawnOperations(input(),rng(0xcd05,0x0a00,0x0300),shared)) == 467);
    assert(instructionCount(level2PeriodicSpawnOperations(input(),rng(0xcd05,0x0a00,0x0301),shared)) == 468);

    const auto lastFree = level2PeriodicSpawnOperations(input(15),rng(0x0105,0x0a00),shared);
    assert(lastFree.spawn->physicalSlot == 15 && lastFree.spawn->logicalIndex == 17);
    assert(instructionCount(lastFree) == 422 + 7 * 15);

    for (const std::int16_t spawned : {6,7,8,9,10,11}) {
        auto state = input(); state.spawnedEnemies = spawned;
        const auto result = level2PeriodicSpawnOperations(state,rng(0xcd05),shared);
        assert(*result.selectedType == 13 && *result.effectiveType == (spawned >= 7 && spawned <= 9 ? 4 : 13));
        assert(result.spawn->actor.type == *result.effectiveType);
        assert(result.rng.calls() == 17U + (*result.effectiveType == 13 ? 4 : 3));
    }
    //The owned descriptor controls radius; synthetic nonstandard geometry metadata
    //here proves no hidden type->radius or inferred type->heap-selector default.
    auto alternate = shared; alternate[4].collisionRadius = 901; alternate[4].flags = 3;
    assert(level2PeriodicSpawnOperations(input(),rng(0x4105),alternate).spawn->actor.collisionRadius == 901);
}

void aliasesAndFullPool() {
    auto state = input(2);
    state.nextEntityIndex = 12;
    auto& retired = state.ordinaryPool[2];
    retired.entityIndex = 9;
    retired.actor.allocationIndex = 9;
    retired.actor.type = 13;
    retired.actor.flags = 0xff;
    retired.actor.position = {123,456,789};
    retired.actor.yawVelocity = -222;
    retired.actor.weaponCooldown = 25;
    retired.actor.presenterDistance = 88;
    retired.actor.collisionContactAllocationIndex = 1;
    //OLD is intentionally not supplied: its physical2 alias must remain untouched.
    const auto result = level2PeriodicSpawnOperations(state,rng(0x0105),models());
    assert(result.spawn->physicalSlot == 2 && result.spawn->logicalIndex == 9);
    assert(result.spawn->actor.allocationIndex == 9 && result.spawn->actor.type == 1);
    assert(result.spawn->actor.flags == 0 && result.spawn->actor.weaponCooldown == 0);
    assert(result.spawn->actor.yawVelocity == 0 && result.spawn->actor.presenterDistance == 0);
    assert(result.spawn->actor.collisionContactAllocationIndex == static_cast<std::size_t>(-1));
    assert(result.spawn->cacheReset.physicalKey == 2 && result.allocationOrder.back().index == 9);
    assert(retired.actor.type == 13 && retired.actor.weaponCooldown == 25); //Pure borrowed input unchanged.

    //Actual initial FF0 retirement reuses logical0; fixed5/6 remain separate.
    state = input(); state.nextEntityIndex = 7;
    state.allocationOrder = {{Level1PresentationKind::entity,5},{Level1PresentationKind::entity,6}};
    state.ordinaryPool[0].entityIndex = 0;
    state.ordinaryPool[0].actor.allocationIndex = 0;
    state.ordinaryPool[0].actor.type = 8;
    const auto initialReuse = level2PeriodicSpawnOperations(state,rng(0x4105),models());
    assert(initialReuse.spawn->physicalSlot == 0 && initialReuse.spawn->logicalIndex == 0);
    assert(initialReuse.spawn->actor.type == 4 && initialReuse.spawn->cacheReset.physicalKey == 0);
    assert(initialReuse.allocationOrder[0].index == 5 && initialReuse.allocationOrder[1].index == 6);
    assert(initialReuse.allocationOrder.back().index == 0);

    //Fullpool is a synthetic source-call control, not an asserted campaign invariant.
    for (const std::int16_t spawned : {6,8,10}) {
        state = input(16); state.spawnedEnemies = spawned;
        auto seed = rng(0x0105);
        const auto full = level2PeriodicSpawnOperations(state,seed,{});
        assert(full.outcome == Level1PeriodicSpawnOutcome::poolFull && !full.spawn);
        assert(*full.selectedType == 1 && *full.effectiveType == (spawned == 8 ? 4 : 1));
        assert(full.allocationOrder == state.allocationOrder);
        assert(instructionCount(full) == 177U + (spawned < 7 ? 128U : spawned <= 9 ? 129U : 130U) + 5);
        seed.next(); sameRng(full.rng,seed);
    }
}

void strictNegatives() {
    const auto shared = models(); const auto seed = rng(0x0105);
    auto state = input(); state.difficulty = 1;
    rejects([&] { level2PeriodicSpawnOperations(state,seed,shared); });
    state = input(); state.activeEnemies = -1;
    rejects([&] { level2PeriodicSpawnOperations(state,seed,shared); });
    state = input(); state.nextEntityIndex = 49;
    rejects([&] { level2PeriodicSpawnOperations(state,seed,shared); });
    state = input(); state.nextEntityIndex = 48;
    rejects([&] { level2PeriodicSpawnOperations(state,seed,shared); });
    state = input(); state.spawnedEnemies = 15;
    rejects([&] { level2PeriodicSpawnOperations(state,seed,shared); });
    for(const auto spawned:{13,14}) {
        state=input();state.spawnedEnemies=static_cast<std::int16_t>(spawned);
        const auto postWave=level2PeriodicSpawnOperations(state,seed,shared);
        assert(postWave.outcome==Level1PeriodicSpawnOutcome::limit && !postWave.spawn);
        assert(instructionCount(postWave)==13);sameRng(postWave.rng,seed);
    }
    state = input(2); state.ordinaryPool[1].entityIndex = state.ordinaryPool[0].entityIndex;
    rejects([&] { level2PeriodicSpawnOperations(state,seed,shared); });
    state = input(2); state.ordinaryPool[0].actor.allocationIndex = 3;
    rejects([&] { level2PeriodicSpawnOperations(state,seed,shared); });
    state = input(2); state.allocationOrder.pop_back();
    rejects([&] { level2PeriodicSpawnOperations(state,seed,shared); });
    state = input(); state.allocationOrder.push_back(state.allocationOrder.front());
    rejects([&] { level2PeriodicSpawnOperations(state,seed,shared); });
    state = input(); state.allocationOrder.push_back({Level1PresentationKind::projectile,8});
    rejects([&] { level2PeriodicSpawnOperations(state,seed,shared); });
    state = input(); state.ordinaryPool[0].actor.type = 11;
    rejects([&] { level2PeriodicSpawnOperations(state,seed,shared); });
    state = input(); state.ordinaryPool[0].actor.renderProfile = WorldActorRenderProfile::heapSelectorNoCallback;
    rejects([&] { level2PeriodicSpawnOperations(state,seed,shared); });
    state = input(); state.ordinaryPool[0].actor.primaryType11AnimationSelector = true;
    rejects([&] { level2PeriodicSpawnOperations(state,seed,shared); });
    state = input(); state.ordinaryPool[0].actor.animatedModelPresenterEnabled = true;
    rejects([&] { level2PeriodicSpawnOperations(state,seed,shared); });
    state = input(); state.ordinaryPool[0].actor.stateBytes4dTo52[0] = 254;
    rejects([&] { level2PeriodicSpawnOperations(state,seed,shared); });
    state = input();
    rejects([&] { level2PeriodicSpawnOperations(state,StellarRng(std::array<std::uint16_t,StellarRng::wordCount>{},1,110,0,0),shared); });
    rejects([&] { level2PeriodicSpawnOperations(state,StellarRng(std::array<std::uint16_t,StellarRng::wordCount>{},48,112,0,0),shared); });
    auto bad = shared; bad.pop_back(); state = input();
    rejects([&] { level2PeriodicSpawnOperations(state,seed,bad); });
    bad = shared; bad[1].scaleShift = 1;
    rejects([&] { level2PeriodicSpawnOperations(state,seed,bad); });
    bad = shared; bad[1].collisionRadius = 0;
    rejects([&] { level2PeriodicSpawnOperations(state,seed,bad); });
    auto fullRegistry = input(16);
    for (std::size_t index = 0; index < 7; ++index) {
        fullRegistry.allocationOrder.push_back({Level1PresentationKind::projectile,index});
        fullRegistry.allocationOrder.push_back({Level1PresentationKind::deathEffect,index});
    }
    //32 entries, but selected FF0 is retired/unlisted: preserve registry bound.
    fullRegistry.ordinaryPool[0].actor.stateBytes4dTo52[1] = 0;
    const auto retiredRef = Level1ObjectRef{Level1PresentationKind::entity,2};
    fullRegistry.allocationOrder.erase(std::find(fullRegistry.allocationOrder.begin(),fullRegistry.allocationOrder.end(),retiredRef));
    fullRegistry.allocationOrder.push_back({Level1PresentationKind::projectile,7});
    assert(fullRegistry.allocationOrder.size() == 32);
    rejects([&] { level2PeriodicSpawnOperations(fullRegistry,seed,shared); });
}

void reusedLevel1Kernel() {
    //EquivalentType1 paths: sourceLevel2 adds two branch instructions in4370.
    auto current = input(2);
    Level1PeriodicSpawnInput old{2,5,10,static_cast<std::uint16_t>(current.allocationOrder.size()),current.nextEntityIndex,{}};
    for (std::size_t slot = 0; slot < old.ordinaryPool.size(); ++slot) {
        old.ordinaryPool[slot] = {current.ordinaryPool[slot].actor.stateBytes4dTo52[1],false,current.ordinaryPool[slot].entityIndex};
    }
    auto shared = models(); shared[1].flags = 1;
    const auto seed = rng(0x0105);
    const auto a = level1PeriodicSpawnOperations(old,seed,shared);
    const auto b = level2PeriodicSpawnOperations(current,seed,shared);
    auto expected = normalized(a.beforeProvider);
    const auto actual = normalized(b.operations);
    assert(expected.size() == 3 && actual.size() == 3);
    expected.front().count += 2;
    for (std::size_t index = 0; index < actual.size(); ++index) {
        assert(actual[index].kind == expected[index].kind && actual[index].count == expected[index].count);
    }
    assert(a.spawn->actor.position == b.spawn->actor.position && a.spawn->actor.yaw == b.spawn->actor.yaw);
    assert(a.spawn->actor.forwardVelocity == b.spawn->actor.forwardVelocity);
    sameRng(a.rng,b.rng);
}
} // namespace

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    branches();
    constructors();
    aliasesAndFullPool();
    strictNegatives();
    reusedLevel1Kernel();
}
