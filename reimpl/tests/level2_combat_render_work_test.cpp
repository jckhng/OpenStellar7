#include "stellar7/combat_scene_work.hpp"
#include "stellar7/radar_state.hpp"
#include "stellar7/world_traversal.hpp"
#include "stellar7/level1_projectile_render_work.hpp"
#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <iostream>
using namespace stellar7;
namespace {
template<class Function> void rejects(Function function) {
    bool rejected=false;try { function(); } catch(const std::invalid_argument&) { rejected=true; }assert(rejected);
}
void same(const std::vector<NormalWorkOperation>& a,const std::vector<NormalWorkOperation>& b) {
    assert(a.size()==b.size());
    for(std::size_t i=0;i<a.size();++i) {assert(a[i].kind==b[i].kind && a[i].count==b[i].count && a[i].value==b[i].value);}
}
GameplayAssets assets() {
    GameplayAssets result;
    result.horizonFrames.assign(4, {256, 16, std::vector<std::uint8_t>(256 * 16, 7)});
    result.artGroups.resize(6);
    result.artGroups[3] = {{120, 56, std::vector<std::uint8_t>(120 * 56, 7)},
        {4, 1, {0, 0, 0, 0}}, {120, 18, std::vector<std::uint8_t>(120 * 18, 3)}};
    result.artGroups[4].assign(15, {4, 1, {0, 0, 0, 0}});
    result.artGroups[4][6].pixels.back() = 9;
    for (std::size_t index = 7; index < 15; ++index) {
        result.artGroups[4][index] = {8, 1, std::vector<std::uint8_t>(8, 13)};
    }
    result.models.resize(31);
    result.models[8].collisionRadius=860;
    result.models[13].collisionRadius=559;
    result.models[30].collisionRadius=868;
    result.models[1].collisionRadius = 1342;
    result.models[11].collisionRadius = 748;
    result.models[11].flags = 1;
    result.models[11].animationLimits = {8};
    result.models[29].collisionRadius = 862;
    ModelGroup shotGroup{};
    shotGroup.animationOffset = 0;
    shotGroup.vertexBlock = 23;
    shotGroup.vertices = {{{0, -2048, 0}}, {{-50, -100, 42}}, {{50, -100, 0}}, {{0, 110, -42}}};
    ModelPrimitive shotPrimitive{};
    shotPrimitive.shapes = {{0x81, {8, 8, 1, 1}, 0, {1, 2, 3}}};
    shotGroup.primitives = {shotPrimitive, shotPrimitive};
    result.models[23].collisionRadius = 134;
    result.models[23].flags = 1;
    result.models[23].lods = {{30, 128, {shotGroup}}};
    result.modelColorTables.resize(2);
    for (std::size_t index = 0; index < 256; ++index) {
        result.modelColorTables[0][index] = static_cast<std::uint8_t>(index);
    }
    ModelGroup enemyGroup{};
    enemyGroup.animationOffset = -1;
    enemyGroup.vertexBlock = 17;
    enemyGroup.vertices = {{{0, 0, 0}}, {{0, -240, 30}}, {{0, 440, 30}},
        {{0, -240, 0}}, {{0, 440, 0}}};
    ModelPrimitive enemyPrimitive{};
    enemyPrimitive.shapes = {{0x80, {95, 95, 14, 14}, 255, {1, 2}},
        {0x80, {95, 95, 14, 14}, 255, {3, 4}}};
    enemyGroup.primitives = {enemyPrimitive};
    result.models[17].collisionRadius = 441;
    result.models[17].lods = {{0, 128, {enemyGroup}}};
    result.sharedModelRegistry[0] = 256;
    result.sharedModelRegistry[1] = 256;
    return result;
}

} // namespace
int checks() {
    const auto view=worldViewSetupOperations({{}, {}, {},0x88,7,{24,294,24,145},{256,-256,362}}).view;
    const WorldActorWorkInput flags4{4,{100,6000,0}};
    rejects([&] {worldFirstCenterOperations(view,flags4);});
    rejects([&] {worldNextCenterOperations(view,flags4);});
    rejects([&] {worldModelCallerOperations(flags4.position,134,4,0);});
    const auto center=worldFirstCenterOperations(view,flags4,WorldSourceFlagsDomain::flagsWithoutCallback);
    same(center.operations,worldFirstCenterOperations(view,{0,flags4.position}).operations);
    same(worldNextCenterOperations(view,flags4,WorldSourceFlagsDomain::flagsWithoutCallback).operations,
        worldNextCenterOperations(view,{0,flags4.position}).operations);
    same(worldModelCallerOperations(center.center,134,4,0,WorldSourceFlagsDomain::flagsWithoutCallback).operations,
        worldModelCallerOperations(center.center,134,0,0).operations);
    rejects([&] {worldFirstCenterOperations(view,{8,{}},WorldSourceFlagsDomain::flagsWithoutCallback);});
    rejects([&] {worldModelCallerOperations(center.center,134,4,0x130c,WorldSourceFlagsDomain::flagsWithoutCallback);});
    auto data=assets();data.artGroups[5].assign(31,{8,5,std::vector<std::uint8_t>(40,7)});
    WorldSceneActor shot{{4,{100,6000,0}},{},23,134,0,0};shot.inlineSelector=0;
    const auto draw=level2CombatSceneOperations(data,view,{shot},{std::nullopt},{256,256,0,0},
        {256,-256,362},0xa800,{{Level1PresentationKind::projectile,0}});
    assert(!draw.scene.operations.empty() && draw.effects.empty());
    assert(draw.scene.centers.front()==shot.actor.position);
    auto shot0=shot;shot0.actor.flags=0;
    const auto zero=level2CombatSceneOperations(data,view,{shot0},{std::nullopt},{256,256,0,0},
        {256,-256,362},0xa800,{{Level1PresentationKind::projectile,0}});
    same(draw.scene.operations,zero.scene.operations);
    data.models[19]=data.models[17];data.models[19].boundingRadius=6;
    auto hostile=shot;hostile.type=19;hostile.collisionRadius=441;
    const auto enemy=level2CombatSceneOperations(data,view,{hostile},{std::nullopt},{256,256,0,0},
        {256,-256,362},0xa800,{{Level1PresentationKind::projectile,0}});
    assert(!enemy.scene.culled.front() && enemy.scene.polygonCounts.front()==2 && !enemy.scene.pixels.empty());
    auto nearHostile=hostile;nearHostile.actor={0,{-500,128,0}};
    const auto nearEnemy=level2CombatSceneOperations(data,view,{nearHostile},{std::nullopt},{256,256,0,0},
        {256,-256,362},0xa800,{{Level1PresentationKind::projectile,0}});
    assert(!nearEnemy.scene.culled.front() && !nearEnemy.scene.pixels.empty());
    for(const std::uint16_t cameraYaw:{0,80}) {
        const ViewTransform rotated{{},makeFixedMatrixQ14(0,0,cameraYaw),{24,294,24,145},128};
        for(const std::uint16_t actorYaw:{0,50378}) {
            auto actual=hostile;actual.actor.flags=0;actual.angles={0,0,actorYaw};
            const auto admitted=level2CombatSceneOperations(data,rotated,{actual},{std::nullopt},{256,256,0,0},
                {256,-256,362},0xa800,{{Level1PresentationKind::projectile,0}});
            auto shared=actual;shared.type=17;shared.inlineSelector.reset();shared.inlineAnimation=std::array<std::uint8_t,6>{};
            const auto reference=worldSceneOperations(data,rotated,{shared},{256,256,0,0},
                {256,-256,362},WorldSceneMatrixProfile::level1Combat);
            same(admitted.scene.operations,reference.operations);
            assert(admitted.scene.pixels.size()==reference.pixels.size());
            for(std::size_t i=0;i<reference.pixels.size();++i) {
                assert(admitted.scene.pixels[i].point==reference.pixels[i].point);
                assert(admitted.scene.pixels[i].color==reference.pixels[i].color);
            }
        }
    }
    rejects([&] {worldSceneOperations(data,view,{hostile},{256,256,0,0});});
    data.models[19].boundingRadius=0;
    rejects([&] {level2CombatSceneOperations(data,view,{hostile},{std::nullopt},{256,256,0,0},
        {256,-256,362},0xa800,{{Level1PresentationKind::projectile,0}});});
    data.models[19].boundingRadius=6;
    for(const auto x:{-1000,1000}) {
        auto offset=shot;offset.actor.position.x=x;
        rejects([&]{level1ProjectileModelCullOperations(offset.actor.position);});
        const auto shared=level2CombatSceneOperations(data,view,{offset},{std::nullopt},{256,256,0,0},
            {256,-256,362},0xa800,{{Level1PresentationKind::projectile,0}});
        assert(!shared.scene.culled.front() && shared.scene.polygonCounts.front()>0);
    }
    rejects([&] {worldSceneOperations(data,view,{shot},{256,256,0,0});});
    auto wrong=shot;wrong.inlineSelector=1;
    rejects([&] {level2CombatSceneOperations(data,view,{wrong},{std::nullopt},{256,256,0,0},
        {256,-256,362},0xa800,{{Level1PresentationKind::projectile,0}});});
    //Whole0AC8 family first13/nonterminal14/terminal32; the same OLD survives main retirement.
    for(const auto age: {std::uint8_t{13},std::uint8_t{14},std::uint8_t{31}}) {
        const std::uint8_t end=age==31?32:17;
        Level1CombatEffectView effect{0,{0x88,{100,6000,0}},0,{0x88,{},0,age,end,0x0ac8},0};
        WorldSceneActor actor{effect.actor,{},-1,0,0x0ac8,0};
        const auto work=level2CombatSceneOperations(data,view,{actor},{effect},{256,256,0,0},
            {256,-256,362},0xa000,{{Level1PresentationKind::deathEffect,0}});
        assert(work.effects.size()==1);
        const auto& returned=work.effects.front().work;
        assert(returned.retired==(age==31));
        assert(returned.state.age==(age==31?0:age+1));
        assert(work.allocationOrderAfterMain.size()==(age==31?0:1));
        const IndexedBitmap panel{120,56,std::vector<std::uint8_t>(120*56,7)};
        RadarState radar({panel});
        const RadarPages pages{0xa400,0xa000,0xa800,0xa000,0xa400};
        radar.initializeLevel1(680,{},0,pages);radar.initializeAllocation(40);
        radar.retainLevel1CombatEffectProjection(40,returned,Level1EffectProjectionDomain::level2Combat);
        const auto radarWork=radar.drawLevel2Combat(680,{},0,{{40,{returned.state.flags,effect.actor.position},0,-1,-4,
            returned.state.callback,WorldActorRenderProfile::bitmapEffectCallback}},pages,{});
        assert(radarWork.cpuWorkQualified && radarWork.actors.size()==1);
        assert(radarWork.actors.front().flags==returned.state.flags && radarWork.actors.front().projectedY==0);
        rejects([&] {radar.drawLevel2Constructor(680,{},0,{{40,{returned.state.flags,effect.actor.position},0,-1,-4,
            returned.state.callback,WorldActorRenderProfile::bitmapEffectCallback}},pages,{});});
    }
    for(const auto back:{std::uint16_t{0xa000},std::uint16_t{0xa800}}) {
        const auto front=static_cast<std::uint16_t>(back==0xa000?0xa800:0xa000);
        const IndexedBitmap panel{120,56,std::vector<std::uint8_t>(120*56,7)};
        const WorldViewSetupInput radarView{{0,0,200000},{0xc000,0,0},{},0x88,7,{112,205,146,180},{256,-256,362}};
        const RadarActorInput fd{{4,{100,6000,0}},134,23,-3,0,WorldActorRenderProfile::inlineSelectorNoCallback,0,0,0};
        const auto radar=level2CombatRadarOperations({680,42,{0xa400,back,front,back,0xa400}},radarView,{},0,
            {fd},{},panel,{228,138});
        assert(radar.cpuWorkQualified && radar.actors[0].flags==4 && radar.actors[0].projectedY==0);
        auto hostileRadar=fd;hostileRadar.type=19;hostileRadar.radius=441;
        const auto enemyRadar=level2CombatRadarOperations({680,42,{0xa400,back,front,back,0xa400}},radarView,{},0,
            {hostileRadar},{},panel,{228,138});
        assert(enemyRadar.cpuWorkQualified && enemyRadar.actors[0].flags==4 && enemyRadar.actors[0].projectedY==0);
        const RadarPages pages{0xa400,back,front,back,0xa400};
        RadarState retained({panel});retained.initializeLevel1(680,{},0,pages);retained.initializeAllocation(32);
        const RadarStateActor hostileState{32,hostileRadar.actor,441,19,-3,0,
            WorldActorRenderProfile::inlineSelectorNoCallback};
        const auto retainedEnemy=retained.drawLevel2Combat(680,{},0,{hostileState},pages,{});
        assert(retainedEnemy.cpuWorkQualified && retained.projection(32).flags==4);
        auto wrongKey=hostileState;wrongKey.allocation=0;
        rejects([&] {retained.drawLevel2Combat(680,{},0,{wrongKey},pages,{});});
        rejects([&] {level2ConstructorRadarOperations({680,42,{0xa400,back,front,back,0xa400}},radarView,{},0,
            {fd},{},panel,{228,138});});
    }
    return 0;
}

int main() {
    try {return checks();} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
