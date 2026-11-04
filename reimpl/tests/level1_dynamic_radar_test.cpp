#include "stellar7/radar_work.hpp"
#include "stellar7/radar_state.hpp"
#include <cassert>
#include <stdexcept>
using namespace stellar7;
template<class Action>void rejects(Action action){bool rejected=false;try{action();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
std::vector<RadarActorInput> actors(bool secondLift){
    std::vector<RadarActorInput> out{
        {{0,{-2048,2048,0}},1342,1,-1,0,WorldActorRenderProfile::noHandles,17,18,19},
        {{0,{8192,8192,1000}},748,11,-1,0x130c,WorldActorRenderProfile::heapSelectorDeferredAnimation,17,18,19},
        {{0,{16384,16384,0}},862,29,-2,0,WorldActorRenderProfile::noHandles,17,18,19},
        {{0,{16384,-16384,0}},862,29,-2,0,WorldActorRenderProfile::noHandles,17,18,19},
        {{0,{-16384,16384,0}},862,29,-2,0,WorldActorRenderProfile::noHandles,17,18,19},
        {{0,{-16384,-16384,0}},862,29,-2,0,WorldActorRenderProfile::noHandles,17,18,19}};
    out.push_back(secondLift?RadarActorInput{{0,{12000,-18000,1000}},748,11,-1,0x130c,WorldActorRenderProfile::heapSelectorDeferredAnimation,0,0,0}
        :RadarActorInput{{0,{-30000,30000,0}},1342,1,-1,0,WorldActorRenderProfile::noHandles,0,0,0});return out;
}
std::vector<std::optional<Level1AnimationState>> animations(bool secondLift){
    std::vector<std::optional<Level1AnimationState>> out(7);out[1]=Level1AnimationState{5,3,1};
    if(secondLift){out[6]=Level1AnimationState{5,5,1};}return out;
}
WorldViewSetupInput view(){return {{0,0,200000},{0xc000,0,0},{0,0,0},0x88,7,{112,205,146,180},{256,-256,362}};}
Model3d model(){Model3d out{};out.flags=1;out.animationLimits={8};return out;}
RadarRetainedState state(){return {600,37,{0xa400,0xa800,0xa000,0xa800,0xa400}};}
IndexedBitmap panel(){return {120,56,std::vector<std::uint8_t>(120*56,7)};}
int main(){
    for(const auto second:{false,true}){
        const auto work=level1DynamicCombatRadarOperations(state(),view(),{},0,actors(second),{},panel(),{228,138},model(),animations(second));
        assert(work.radar.cpuWorkQualified&&work.radar.actors.size()==7);
        assert(work.animations[1]->countdown==2&&work.animations[1]->variant==1);
        if(second){assert(work.animations[6]->countdown==4&&work.animations[6]->variant==1);}else{assert(!work.animations[6]);}
        rejects([&]{level1CombatRadarOperations(state(),view(),{},0,actors(second),{},panel(),{228,138},model(),{5,3,1});});
    }
    auto values=animations(false);values[0]=Level1AnimationState{5,3,1};
    rejects([&]{level1DynamicCombatRadarOperations(state(),view(),{},0,actors(false),{},panel(),{228,138},model(),values);});
    values=animations(true);values[6].reset();rejects([&]{level1DynamicCombatRadarOperations(state(),view(),{},0,actors(true),{},panel(),{228,138},model(),values);});
    auto input=actors(false);input[6].type=3;input[6].radius=770;
    rejects([&]{level1DynamicCombatRadarOperations(state(),view(),{},0,input,{},panel(),{228,138},model(),animations(false));});
    RadarState owner({panel()});
    for(std::size_t i=0;i<6;++i){owner.initializeAllocation(i);}
    owner.initializeLevel1(600,{},0,state().pages);
    std::vector<RadarStateActor> owned;
    for(std::size_t i=0;i<7;++i){const auto a=actors(false)[i];owned.push_back({i,a.actor,a.radius,a.type,a.category,a.callback,a.renderProfile});}
    rejects([&]{owner.drawLevel1DynamicCombat(600,{},0,owned,state().pages,{},model(),animations(false));});
    owner.initializeAllocation(6);
    const auto committed=owner.drawLevel1DynamicCombat(600,{},0,owned,state().pages,{},model(),animations(false));
    assert(committed.animations[1]->countdown==2&&owner.projection(6).cachedCenter.x==-30000);
}
