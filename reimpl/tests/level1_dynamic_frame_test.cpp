#include "stellar7/level1_frame.hpp"
#include <cassert>
#include <stdexcept>
using namespace stellar7;
template<class Action>void rejects(Action action){bool rejected=false;try{action();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
GameplayAssets assets() {
    GameplayAssets result;
    result.horizonFrames.assign(4, {256, 16, std::vector<std::uint8_t>(256 * 16, 7)});
    result.artGroups.resize(5);
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

int main(){
    auto data=assets();Level1State level;level.reloadAfterFailure(StellarRng{});
    auto actors=level.entities();for(auto& actor:actors){actor.position.y=-16384;}
    auto ground=level.ground();auto rng=level.rng();
    std::array<Level1Projectile,Level1State::projectilePoolSize> pool{};
    for(std::size_t i=0;i<pool.size();++i){pool[i].poolSlot=i;}
    auto& shot=pool[0];shot.type=17;shot.collisionRadius=441;shot.position={0,-16384,0};
    shot.forwardVelocity=500;shot.ownerAllocationIndex=0;shot.stateBytes4dTo54={253,54,0,0,0,0,0,0};
    std::vector<Level1ObjectRef> order;
    for(std::size_t i=0;i<actors.size();++i){order.push_back({Level1PresentationKind::entity,i});}
    order.push_back({Level1PresentationKind::projectile,0});
    Level1ProjectileRenderState frame{ground,rng,level.player(),actors,pool,order,2};
    const CockpitSettings settings{90,{}};const Level1PresentationState presentation{{140,4},false};
    auto pages=CockpitPageState::fromRecoveredMenu();IndexedPages pixels;RadarState radar(data.artGroups[3]);
    for(std::size_t i=0;i<actors.size();++i){radar.initializeAllocation(i);}radar.initializeAllocation(16);
    pixels.apply(radar.initializeLevel1(680,{},0,{pages.cachePage(),pages.backPage(),pages.frontPage(),pages.backPage(),pages.cachePage()}));
    //Same existing six plus one hostile record must retain the old composition work.
    const auto strict=planLevel1CombatFrame(data,frame,presentation,680,settings,pages,radar,pixels);
    const auto periodic=planLevel1PeriodicCombatFrame(data,frame,presentation,680,settings,pages,radar,pixels);
    assert(strict.operations.size()==periodic.operations.size());
    for(std::size_t i=0;i<strict.operations.size();++i){const auto a=strict.operations[i],b=periodic.operations[i];
        assert(a.kind==b.kind&&a.count==b.count&&a.value==b.value);}
    auto actor=actors[0];actor.allocationIndex=6;actors.push_back(actor);
    order.insert(order.end()-1,{Level1PresentationKind::entity,6});radar.initializeAllocation(6);
    shot.ownerAllocationIndex=6;
    const auto dynamic=planLevel1PeriodicCombatFrame(data,frame,presentation,680,settings,pages,radar,pixels);
    assert(dynamic.radarActors.size()==8);
    rejects([&]{planLevel1CombatFrame(data,frame,presentation,680,settings,pages,radar,pixels);});
    rejects([&]{planLevel1ProjectileFrame(data,frame,presentation,680,settings,pages,radar,pixels);});
    shot.ownerAllocationIndex=2;
    rejects([&]{planLevel1PeriodicCombatFrame(data,frame,presentation,680,settings,pages,radar,pixels);});
    shot.ownerAllocationIndex=6;shot.type=20;
    rejects([&]{planLevel1PeriodicCombatFrame(data,frame,presentation,680,settings,pages,radar,pixels);});
}
