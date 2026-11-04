#include "stellar7/level1_frame.hpp"
#include "stellar7/level1_primary_fire_work.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void prepare(Level1State& state, const std::uint16_t choice) {
        for (auto& actor : state.entities_) { actor.position.y = -16384; }
        //The first ground child consumes twelve words, then0DDC reads these three.
        std::array<std::uint16_t,StellarRng::wordCount> table{};
        table[43] = choice; table[42] = 2567; table[41] = 43981;
        state.rng_ = StellarRng(table,48,110,0,state.rng_.calls());
    }
    static void allocationOrder(const Level1State& state, const bool fire) {
        assert(state.allocationList_.size() == (fire ? 8 : 7));
        const auto& born = state.allocationList_.at(fire ? 7 : 6);
        assert(born.kind == Level1State::ObjectKind::entity && born.index == 6);
        if (fire) {
            const auto& launched = state.allocationList_.at(6);
            assert(launched.kind == Level1State::ObjectKind::projectile && launched.index == 0);
        }
    }
};
struct GameplayModelTestAccess {
    static void prepare(GameplayModel& model, const std::uint16_t choice) {
        Level1ReloadTestAccess::prepare(model.level1_,choice);
    }
};
}

namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
GameplayAssets assets() {
    GameplayAssets out;
    out.horizonFrames.assign(4,{256,16,std::vector<std::uint8_t>(256*16,7)});
    out.artGroups.resize(5);
    out.artGroups[3] = {{120,56,std::vector<std::uint8_t>(120*56,7)},
        {4,1,{0,0,0,0}},{120,18,std::vector<std::uint8_t>(120*18,3)}};
    out.artGroups[4].assign(41,{4,1,{0,0,0,0}});
    out.artGroups[4][6].pixels.back() = 9;
    for (std::size_t i = 7; i < 15; ++i) {
        out.artGroups[4][i] = {8,1,std::vector<std::uint8_t>(8,13)};
    }
    out.artGroups[4][39] = out.artGroups[4][40] = {24,5,std::vector<std::uint8_t>(120,7)};
    out.models.resize(44);
    out.models[1].collisionRadius = 1342; out.models[1].flags = 1;
    out.models[3].collisionRadius = 770; out.models[3].flags = 1;
    out.models[11].collisionRadius = 748; out.models[11].flags = 1;
    out.models[11].animationLimits = {8};
    out.models[17].collisionRadius = 441;
    out.models[20].collisionRadius = out.models[23].collisionRadius = 134;
    ModelGroup shotGroup{};
    shotGroup.animationOffset = 0;
    shotGroup.vertexBlock = 23;
    shotGroup.vertices = {{{0,-2048,0}},{{-50,-100,42}},{{50,-100,0}},{{0,110,-42}}};
    ModelPrimitive shotPrimitive{};
    shotPrimitive.shapes = {{0x81,{8,8,1,1},0,{1,2,3}}};
    shotGroup.primitives = {shotPrimitive,shotPrimitive};
    out.models[23].flags = 1;
    out.models[23].lods = {{30,128,{shotGroup}}};
    out.modelColorTables.resize(2);
    for (std::size_t index = 0; index < 256; ++index) {
        out.modelColorTables[0][index] = static_cast<std::uint8_t>(index);
    }
    out.models[29].collisionRadius = 862;
    out.sharedModelRegistry = {256,256};
    return out;
}
FrameTailGuards guards(const bool fire = false) {
    FrameTailGuards out{};
    out.keyboardHead = out.keyboardTail = 0x1e;
    out.heldKeys[8] = fire;
    return out;
}

void equalOperations(const std::vector<NormalWorkOperation>& first,
    const std::vector<NormalWorkOperation>& second) {
    assert(first.size() == second.size());
    for (std::size_t i = 0; i < first.size(); ++i) {
        assert(first[i].kind == second[i].kind && first[i].count == second[i].count
            && first[i].value == second[i].value);
    }
}

void birth(const std::uint16_t choice, const std::int16_t type, const bool fire) {
    const auto data = assets();
    auto model = std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    GameplayModelTestAccess::prepare(*model,choice);
    auto pages = CockpitPageState::fromRecoveredMenu();
    IndexedPages pixels;
    for (const std::uint16_t page : {0xa000,0xa400,0xa800}) {
        for (std::int16_t y = 0; y < 200; ++y) {
            for (std::int16_t x = 0; x < 320; ++x) { pixels.write(page,{x,y},93); }
        }
    }
    RadarState radar(data.artGroups[3]);
    for (std::size_t i = 0; i < 6; ++i) { radar.initializeAllocation(i); }
    pixels.apply(radar.initializeLevel1(680,{},0,
        {pages.cachePage(),pages.backPage(),pages.frontPage(),pages.backPage(),pages.cachePage()}));
    const auto first = model->advanceRecoveredLevel1Frame(data,{90,{}},pages,radar,pixels,guards(fire));
    assert(first.periodic && first.periodic->spawn && first.periodic->selectedType == type);
    const auto& receipt = *first.periodic->spawn;
    const auto& state = model->level1State();
    assert(receipt.poolSlot == 2 && receipt.actor.allocationIndex == 6);
    assert(receipt.appendedAllocationIndex == (fire ? 7 : 6));
    assert(state.entities().size() == 7 && state.entities()[6].type == type);
    Level1ReloadTestAccess::allocationOrder(state,fire);
    assert(state.entities()[6].position == receipt.actor.position);
    assert(state.activeEnemyCount() == 3 && state.spawnedEnemyCount() == 3);
    assert(state.lastSpawnCheckpoint() && state.lastSpawnTick() == 0);
    assert(state.lastSpawnCheckpoint()->position == receipt.actor.position);
    assert(state.lastSpawnRng().calls() == first.periodic->rng.calls());
    assert(state.rng().calls() == first.render.ground.rng.calls() + 3);
    assert(first.render.radarActors.size() == 6 && first.tail.state.actors.size() == 6);
    assert(state.activeListOrder().size() == 6 && state.loopTick() == 1 && model->ticks() == 1);
    const auto& cached = first.render.radarState.projection(6);
    assert(cached.projectedX == 0 && cached.projectedY == 0 && cached.projectedRadius == 0);
    assert(first.periodic->provider.has_value() == (type == 11));
    assert(first.beforePeriodicProvider.empty() == (type != 11));
    if (type == 11) {
        assert(first.periodic->provider->entityIndex == 6 && !first.periodic->provider->guestElapsedQualified);
        assert(!first.tail.afterEntryFade.empty());
    }
    if (fire) {
        assert(state.projectiles()[0].active());
        assert(first.tail.state.weaponTimers[0] == 8);
        assert(state.audioEvents().size() == 1 && state.audioEvents().back().eventId == 14);
    }
    auto nextPages = first.render.pages; nextPages.flip();
    if (first.lamp) { nextPages = first.lamp->pages; }
    auto nextPixels = first.render.pixels;
    if (first.lamp) { nextPixels.apply(first.lamp->effects); }
    const auto next = model->advanceRecoveredLevel1Frame(data,{90,{}},nextPages,
        first.render.radarState,nextPixels,guards());
    assert(next.projectilePrefix && !next.periodic && next.beforePeriodicProvider.empty());
    assert(next.render.radarActors.size() == (fire ? 8 : 7));
    assert(model->ticks() == 2 && state.loopTick() == 2 && state.entities().size() == 7);
    if (type == 1) { assert(state.entities()[6].position == receipt.actor.position); }
    else if (type == 3) {
        assert(state.entities()[6].stateBytes4dTo52[4] == 1 && state.entities()[6].weaponSlotCount == 1);
        assert(state.entities()[6].renderProfile == WorldActorRenderProfile::noHandles);
    } else if (type == 11) {
        assert(next.render.animationsAfterRadar.size() == next.render.radarActors.size());
        for (std::size_t ordinal = 0; ordinal < next.render.radarActors.size(); ++ordinal) {
            const auto& actor = next.render.radarActors[ordinal];
            if (actor.type != 11) { continue; }
            assert(next.render.animationsAfterRadar[ordinal]);
            const auto& animation = *next.render.animationsAfterRadar[ordinal];
            const auto& retained = state.entities().at(actor.allocation);
            assert(retained.modelAnimationVariant == animation.variant
                && retained.stateBytes4dTo52[3] == animation.countdown);
        }
    }
}

void primaryMembership() {
    Level1PrimaryFireInput input{};
    for (std::size_t i = 0; i < input.pool.size(); ++i) {
        input.pool[i].poolSlot = i; input.pool[i].stateBytes4dTo54[0] = 0xfd;
    }
    input.allocationCount = 6; input.model = {256,0,134};
    const auto old = level1PrimaryFireOperations(input);
    input.allocationCount = 7;
    rejects([&] { level1PrimaryFireOperations(input); });
    input.membership = Level1PrimaryFireMembership::periodicSpawned;
    const auto dynamic = level1PrimaryFireOperations(input);
    assert(dynamic.launch && dynamic.launch->allocationIndex == 7 && dynamic.allocationCount == 8);
    equalOperations(old.beforeEvent14,dynamic.beforeEvent14);
    equalOperations(old.afterEvent14BeforeLamp,dynamic.afterEvent14BeforeLamp);
    equalOperations(old.afterLamp,dynamic.afterLamp);
    input.allocationCount = 15; rejects([&] { level1PrimaryFireOperations(input); });
    input.allocationCount = 7; input.membership = static_cast<Level1PrimaryFireMembership>(99);
    rejects([&] { level1PrimaryFireOperations(input); });
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    primaryMembership();
    birth(261,1,false);
    birth(261,1,true);
    birth(25605,3,false);
    birth(43525,11,false);
    return 0;
}
