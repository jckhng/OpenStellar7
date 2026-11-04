#include "stellar7/level1_frame.hpp"
#include "stellar7/world_traversal.hpp"
#include <algorithm>
#include <cassert>
#include <stdexcept>
using namespace stellar7;
template<class Action>void rejects(Action action){bool rejected=false;try{action();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
void sameWork(const std::vector<NormalWorkOperation>& a,const std::vector<NormalWorkOperation>& b){
    assert(a.size()==b.size());for(std::size_t i=0;i<a.size();++i){assert(a[i].kind==b[i].kind&&a[i].count==b[i].count&&a[i].value==b[i].value);}
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
    result.models.resize(30);
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
Level1CombatEffectView effect(std::size_t slot,std::uint8_t age){
    return {slot,{0x88,{0,-16384,0}},0,{0x88,{},0,age,12,0x0ac8},0x4321};
}
WorldSceneActor actor(const Level1CombatEffectView& effect){
    return {effect.actor,{},-1,effect.radius,effect.state.callback,0};
}
void sceneControls(){
    const auto data=assets();const auto view=initialLevel1MainView();
    const WorldSceneActor tank{{0,{0,-16384,0}},{},1,1342,0,0};
    const auto first=effect(0,11),last=effect(1,11);
    const std::vector<WorldSceneActor> actors{actor(first),tank,actor(last)};
    const std::vector<std::optional<Level1CombatEffectView>> effects{first,{},last};
    const std::vector<Level1ObjectRef> allocations{{Level1PresentationKind::deathEffect,0},
        {Level1PresentationKind::entity,0},{Level1PresentationKind::deathEffect,1}};
    const auto plan=level1CombatSceneOperations(data,view,actors,effects,{256,256,0,0},{256,-256,362},0xa800,allocations);
    assert(plan.effects.size()==2&&plan.scene.centers.size()==3&&plan.scene.animationStates.size()==3);
    assert(plan.effects[0].ordinal==0&&plan.effects[1].ordinal==2);
    const auto& a=plan.effects[0].work;const auto& b=plan.effects[1].work;
    assert(a.retired&&b.retired&&a.state.age==0&&b.state.age==0);
    assert(a.removal&&a.removal->removedIndex==0&&a.removal->lastIndex==2);
    assert(b.removal&&b.removal->removedIndex==0&&b.removal->lastIndex==1);
    assert(plan.allocationOrderAfterMain==(std::vector<Level1ObjectRef>{{Level1PresentationKind::entity,0}}));
    assert(allocations.size()==3&&effects[0]->state.age==11&&effects[2]->state.age==11);
    const auto center=worldFirstCenterOperations(view,{0,first.actor.position});
    assert(plan.scene.operations.front().kind==center.operations.front().kind
        &&plan.scene.operations.front().count==center.operations.front().count+2);
    rejects([&]{worldSceneOperations(data,view,actors,{256,256,0,0},{256,-256,362},WorldSceneMatrixProfile::level1Combat);});
    auto duplicate=effects;duplicate[2]=first;
    rejects([&]{level1CombatSceneOperations(data,view,actors,duplicate,{256,256,0,0},{256,-256,362},0xa800,allocations);});
    auto visibleData=data;
    visibleData.artGroups[5].assign(12,{8,5,std::vector<std::uint8_t>(40,7)});
    auto visible=effect(0,1);visible.actor.position={100,1800,200};
    const auto draw=level1CombatSceneOperations(visibleData,view,{actor(visible)},{visible},
        {256,256,0,0},{256,-256,362},0xa800,{{Level1PresentationKind::deathEffect,0}});
    assert(draw.scene.pixels.size()==40&&draw.effects.size()==1);
    assert(draw.effects[0].work.state.age==2&&draw.effects[0].work.projected);
    assert(draw.effects[0].work.state.projected==(ProjectedPoint{166,70}));
    assert(!draw.effects[0].work.retired&&!draw.effects[0].work.removal);
}
int main(){
    sceneControls();auto data=assets();Level1State level;level.reloadAfterFailure(StellarRng{});
    auto actors=level.entities();for(auto& value:actors){value.position.y=-16384;}
    auto ground=level.ground();auto rng=level.rng();
    std::array<Level1Projectile,Level1State::projectilePoolSize> pool{};
    for(std::size_t i=0;i<pool.size();++i){pool[i].poolSlot=i;}
    std::vector<Level1ObjectRef> refs;for(std::size_t i=0;i<actors.size();++i){refs.push_back({Level1PresentationKind::entity,i});}
    auto allocations=refs;Level1ProjectileRenderState frame{ground,rng,level.player(),actors,pool,refs,2};
    Level1CombatRenderContext context{allocations,{}};
    const CockpitSettings settings{90,{}};const Level1PresentationState presentation{{140,4},false};
    auto pages=CockpitPageState::fromRecoveredMenu();IndexedPages pixels;RadarState radar(data.artGroups[3]);
    for(std::size_t i=0;i<actors.size();++i){radar.initializeAllocation(i);}
    pixels.apply(radar.initializeLevel1(680,{},0,{pages.cachePage(),pages.backPage(),pages.frontPage(),pages.backPage(),pages.cachePage()}));
    const auto strict=planLevel1PeriodicCombatFrame(data,frame,presentation,680,settings,pages,radar,pixels);
    const auto same=planLevel1EffectCombatFrame(data,frame,context,presentation,680,settings,pages,radar,pixels);
    sameWork(strict.operations,same.frame.operations);
    assert(same.effects.empty()&&same.allocationOrderAfterMain==allocations);
    //The killed target remains in its stable storage, absent from the rebuilt list.
    actors[0].stateBytes4dTo52[1]=0;refs.erase(refs.begin());allocations=refs;
    refs.push_back({Level1PresentationKind::deathEffect,0});refs.push_back({Level1PresentationKind::deathEffect,1});
    allocations=refs;context.effects={effect(0,11),effect(1,1)};
    //A missing loader-owned cache must reject before any render state is committed.
    rejects([&]{planLevel1EffectCombatFrame(data,frame,context,presentation,680,settings,pages,radar,pixels);});
    radar.initializeAllocation(32);radar.initializeAllocation(33);
    const auto mixed=planLevel1EffectCombatFrame(data,frame,context,presentation,680,settings,pages,radar,pixels);
    assert(mixed.effects.size()==2&&mixed.frame.radarActors.size()==refs.size());
    const auto terminalOrdinal=refs.size()-2,liveOrdinal=refs.size()-1;
    assert(mixed.effects[0].work.retired&&mixed.effects[0].work.state.age==0);
    assert(!mixed.effects[1].work.retired&&mixed.effects[1].work.state.age==2);
    assert(mixed.frame.radarActors[terminalOrdinal].callback==0&&mixed.frame.radarActors[liveOrdinal].callback==0x0ac8);
    assert(mixed.frame.radar.actors.size()==refs.size());
    assert(mixed.allocationOrderAfterMain.size()+1==allocations.size());
    assert(std::find(mixed.allocationOrderAfterMain.begin(),mixed.allocationOrderAfterMain.end(),
        Level1ObjectRef{Level1PresentationKind::deathEffect,0})==mixed.allocationOrderAfterMain.end());
    assert(mixed.frame.animationAfterMain.countdown==5&&mixed.frame.animationAfterRadar.countdown==4);
    assert(mixed.frame.radarState.projection(32).cachedCenter.y!=0);
    assert(radar.projection(32).cachedCenter==(FixedPosition{}));
    assert(actors[0].stateBytes4dTo52[0]==255&&actors[0].stateBytes4dTo52[4]==1);
    assert(context.effects[1].state.age==1&&allocations.size()==7&&rng.calls()==16);
    rejects([&]{planLevel1PeriodicCombatFrame(data,frame,presentation,680,settings,pages,radar,pixels);});
    auto stale=context;stale.effects[0].state.projected.x=1;
    rejects([&]{planLevel1EffectCombatFrame(data,frame,stale,presentation,680,settings,pages,radar,pixels);});
    auto bad=context;bad.effects[0].poolSlot=8;
    rejects([&]{planLevel1EffectCombatFrame(data,frame,bad,presentation,680,settings,pages,radar,pixels);});
    auto badCallback=context;badCallback.effects[1].state.callback=0;
    rejects([&]{planLevel1EffectCombatFrame(data,frame,badCallback,presentation,680,settings,pages,radar,pixels);});
    //Owner identity survives target death; a hostile projectile does not require its owner live.
    auto& shot=pool[0];shot.type=17;shot.collisionRadius=441;shot.position={0,-16384,0};
    shot.forwardVelocity=500;shot.ownerAllocationIndex=0;shot.stateBytes4dTo54={253,54,0,0,0,0,0,0};
    refs.push_back({Level1PresentationKind::projectile,0});allocations=refs;radar.initializeAllocation(16);
    const auto retainedOwner=planLevel1EffectCombatFrame(data,frame,context,presentation,680,settings,pages,radar,pixels);
    assert(retainedOwner.frame.radarActors.size()==refs.size());
    shot.ownerAllocationIndex=2;
    rejects([&]{planLevel1EffectCombatFrame(data,frame,context,presentation,680,settings,pages,radar,pixels);});
}
