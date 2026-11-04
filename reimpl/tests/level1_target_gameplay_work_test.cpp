#include "stellar7/level1_frame.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace stellar7 {
struct GameplayModelTestAccess {
    static Level1State& level(GameplayModel& model) { return model.level1_; }
};
struct Level1ReloadTestAccess {
    static void prepare(Level1State& state) {
        for (auto& actor : state.entities_) { actor.position.y=-16384; }
        state.entities_[0].forwardVelocity=0;
        state.entities_[0].weaponCooldown=255;
        state.entities_[1].behaviorTimer=20;
    }
    static void hit(Level1State& state,const std::uint8_t health) {
        auto& target=state.entities_[0];
        constexpr std::int32_t targetX=2000;
        target.position={targetX,8000,0};
        target.stateBytes4dTo52[1]=health;
        target.stateBytes4dTo52[5]=1;
        target.behaviorTimer=10;
        target.yawVelocity=0;target.forwardVelocity=0;
        target.presenterDistance=0;
        auto& shot=state.projectiles_[0];
        shot.type=23;shot.collisionRadius=134;shot.position={targetX,7500,0};
        shot.forwardVelocity=500;shot.stateBytes4dTo54={253,10,0,0,0,0,0,0};
        shot.ownerIsPlayer=true;shot.ownerAllocationIndex=static_cast<std::size_t>(-1);
        state.allocationList_.push_back({Level1State::ObjectKind::projectile,0});
        state.objectListOrder_.insert(state.objectListOrder_.begin(),{Level1State::ObjectKind::projectile,0});
        state.nativeInitialListPending_=false;
    }
    static bool allocatedEffect(const Level1State& state) {
        return std::any_of(state.allocationList_.begin(),state.allocationList_.end(),[](const auto ref) {
            return ref.kind==Level1State::ObjectKind::deathEffect;
        });
    }
    static bool oldEffect(const Level1State& state) {
        return std::any_of(state.objectListOrder_.begin(),state.objectListOrder_.end(),[](const auto ref) {
            return ref.kind==Level1State::ObjectKind::deathEffect;
        });
    }
};
}
namespace {
using namespace stellar7;
GameplayAssets assets() {
    GameplayAssets result;
    result.horizonFrames.assign(4,{256,16,std::vector<std::uint8_t>(256*16,7)});
    result.artGroups.resize(6);
    result.artGroups[5].assign(40,{8,5,std::vector<std::uint8_t>(40,7)});
    result.artGroups[3] = {{120,56,std::vector<std::uint8_t>(120*56,7)},
        {4,1,{0,0,0,0}},{120,18,std::vector<std::uint8_t>(120*18,3)}};
    result.artGroups[4].assign(15,{4,1,{0,0,0,0}});
    for (std::size_t index = 7; index < 15; ++index) {
        result.artGroups[4][index] = {8,1,std::vector<std::uint8_t>(8,13)};
    }
    result.models.resize(30);
    result.models[1].collisionRadius = 1342;
    //Reuse the world-model geometry and two-group Type1 LOD fixtures.
    ModelGroup tankGroup{};
    tankGroup.vertexBlock = 10;
    tankGroup.animationOffset = -1;
    tankGroup.vertices = {{{0, -256, 0}}, {{-10, 1000, 0}}, {{10, 1000, 0}}, {{0, 1000, 10}}};
    ModelPrimitive tankPrimitive{};
    tankPrimitive.shapes = {{0x81, {8, 8, 1, 1}, 0, {1, 2, 3}}};
    auto tankVariant = tankPrimitive;
    tankVariant.shapes[0].colors = {9, 9, 1, 1};
    tankGroup.primitives = {tankPrimitive, tankVariant};
    result.models[1].lods = {{10, 128, {tankGroup, tankGroup}},
        {30, 128, {tankGroup, tankGroup}}, {255, 128, {tankGroup, tankGroup}}};
    result.modelColorTables.resize(2);
    for (std::size_t index=0;index<256;++index) {
        result.modelColorTables[0][index]=static_cast<std::uint8_t>(index);
    }
    result.models[11].collisionRadius = 748;
    result.models[11].flags = 1;
    result.models[11].animationLimits = {8};
    result.models[17].collisionRadius = 441;
    result.models[20].collisionRadius = 134;
    result.models[23].collisionRadius = 134;
    result.models[29].collisionRadius = 862;
    result.sharedModelRegistry[0] = result.sharedModelRegistry[1] = 256;
    result.scoreFont = {32,8,0,std::vector<IndexedBitmap>(96,{4,8,std::vector<std::uint8_t>(32,1)})};
    return result;
}


void control(const std::uint8_t health) {
    const auto data=assets();
    const CockpitSettings settings{90,{}};
    auto model=std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    auto& state=GameplayModelTestAccess::level(*model);
    Level1ReloadTestAccess::prepare(state);
    auto pages=CockpitPageState::fromRecoveredMenu();
    auto pixels=std::make_unique<IndexedPages>();
    for (const std::uint16_t page : {0xa000,0xa400,0xa800}) {
        for (std::int16_t y=0;y<200;++y) {
            for (std::int16_t x=0;x<320;++x) { pixels->write(page,{x,y},93); }
        }
    }
    RadarState radar(data.artGroups[3]);
    for (std::size_t index=0;index<state.entities().size();++index) { radar.initializeAllocation(index); }
    //Loader source47D8 cache allocation; birth must never reinitialize it.
    for (std::size_t slot=0;slot<Level1State::deathEffectPoolSize;++slot) { radar.initializeAllocation(32+slot); }
    pixels->apply(radar.initializeLevel1(680,{},0,
        {pages.cachePage(),pages.backPage(),pages.frontPage(),pages.backPage(),pages.cachePage()}));
    FrameTailGuards guards{};
    guards.keyboardHead=guards.keyboardTail=0x1e;
    auto first=model->advanceRecoveredLevel1Frame(data,settings,pages,radar,*pixels,guards);
    assert(first.prefixScores.empty());
    pages=first.render.pages;pages.flip();
    *pixels=first.render.pixels;radar=first.render.radarState;
    const auto incomingEvents=state.audioEvents().size();
    //Controlled arrival on the model's own state; no original-state/RNG reseed.
    Level1ReloadTestAccess::hit(state,health);
    radar.initializeAllocation(16);
    auto hit=model->advanceRecoveredLevel1Frame(data,settings,pages,radar,*pixels,guards);
    assert(model->ticks()==2 && state.loopTick()==2 && !state.projectiles()[0].active());
    assert(state.entities()[0].stateBytes4dTo52[1]==health-1 && state.entities()[0].stateBytes4dTo52[0]==255);
    assert(state.entities()[0].stateBytes4dTo52[4]==1 && state.entities()[0].behaviorTimer==9);
    assert(state.score()==(health==1 ? 20U : 0U) && state.activeEnemyCount()==(health==1 ? 1 : 2));
    assert(state.audioEvents().size()==incomingEvents+1 && state.audioEvents().back().eventId==(health==1 ? 6 : 5));
    assert(hit.prefixScores.size()==(health==1 ? 1U : 0U));
    assert(state.deathEffects()[0].listed && Level1ReloadTestAccess::allocatedEffect(state));
    const auto age=state.deathEffects()[0].entity.stateBytes4dTo52[1];
    assert(age==(health==1 ? 15 : 2)); //0BC8 first+1, then one main0AC8 increment.
    const auto& finalProjection=hit.render.radarState.projection(32);
    assert((state.deathEffects()[0].projected==ProjectedPoint{finalProjection.projectedX,finalProjection.projectedY}));
    bool tailEffect=false;
    for (const auto& actor : hit.tail.state.actors) { tailEffect=tailEffect || actor.type==-1; }
    assert(tailEffect && hit.tail.state.actors.size()==hit.render.radarActors.size());
    if (health==1) {
        assert(state.lastDestroyedType()==1 && state.consecutiveDestroyedTypeCount()==1);
        assert(hit.render.pixels.read(pages.frontPage(),{240,155})==239);
        assert(hit.render.pixels.read(pages.backPage(),{240,155})==239);
    }
    pages=hit.render.pages;pages.flip();
    auto adjacent=model->advanceRecoveredLevel1Frame(data,settings,pages,hit.render.radarState,hit.render.pixels,guards);
    assert(model->ticks()==3 && state.loopTick()==3 && adjacent.prefixScores.empty());
    assert(state.audioEvents().size()==incomingEvents+1 && state.deathEffects()[0].entity.stateBytes4dTo52[1]==age+1);
    assert(state.score()==(health==1 ? 20U : 0U));
    if (health!=1) { return; }
    pages=adjacent.render.pages;pages.flip();
    auto terminal=model->advanceRecoveredLevel1Frame(data,settings,pages,adjacent.render.radarState,adjacent.render.pixels,guards);
    assert(!state.deathEffects()[0].listed && !Level1ReloadTestAccess::allocatedEffect(state));
    assert(Level1ReloadTestAccess::oldEffect(state));
    assert(std::any_of(terminal.tail.state.actors.begin(),terminal.tail.state.actors.end(),[](const auto& actor) { return actor.type==-1; }));
    pages=terminal.render.pages;pages.flip();
    auto after=model->advanceRecoveredLevel1Frame(data,settings,pages,terminal.render.radarState,terminal.render.pixels,guards);
    assert(!Level1ReloadTestAccess::oldEffect(state) && after.prefixScores.empty());
    assert(state.score()==20 && state.audioEvents().size()==incomingEvents+1);
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    for (const std::uint8_t health : {1,2}) {
        try { control(health); }
        catch (const std::exception& error) {
            std::cerr << "target gameplay work (health " << static_cast<unsigned>(health)
                << "): " << error.what() << '\n';
            return 1;
        }
    }
    return 0;
}
