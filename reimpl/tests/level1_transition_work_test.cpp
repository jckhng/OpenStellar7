#include "stellar7/level1_transition_work.hpp"
#include "stellar7/score_work.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace stellar7;
template<class F>void rejects(F action){bool bad=false;try{action();}catch(const std::invalid_argument&){bad=true;}assert(bad);}
int main(){try{
Level1TransitionProfile p{};p.score=3400;p.armor=650;p.systemLevels.fill(3);p.systemTimers={1,2,3,17,5,6,7};
p.frontPage=0xa000;p.backPage=0xa800;p.cachePage=0xa400;p.successReturned=true;
const auto work=level1SuccessTransitionOperations(p);
assert(work.observation.levelIndex==1&&work.observation.score==3400&&work.observation.armor==680);
assert((work.observation.systemTimers==std::array<std::int16_t,7>{0,0,0,17,0,0,0}));
assert(work.observation.loopTick==0&&work.observation.firstEntryDisplay&&work.observation.oldPresenterListEmpty&&work.observation.missionActive);
assert(work.poolResetSegment<work.constructorsSegment&&work.constructorsSegment<work.loaderPagesSegment&&work.loaderPagesSegment<work.groundSegment);
assert(work.segments.at(work.poolResetSegment).sourceEntry==0x48d4);
assert(work.segments.at(work.constructorsSegment).sourceEntry==0x4f72);
assert(work.segments.at(work.loaderPagesSegment).sourceEntry==0x50c4);
assert(work.segments.at(work.groundSegment).sourceEntry==0x4c16);
assert(std::count_if(work.segments.begin(),work.segments.end(),[](auto& s){return s.kind==RetryWorkKind::stopPendingSound;})==2);
assert(std::count_if(work.segments.begin(),work.segments.end(),[](auto& s){return s.kind==RetryWorkKind::musicActivation;})==1);
assert(work.segments.front().operations.front().count==5&&work.segments.back().kind==RetryWorkKind::controllerRebindProvider);
for(const auto& s:work.segments){assert(s.sourceEntry!=0x0784&&s.sourceEntry!=0x0a9a);}
auto other=p;other.levelIndex=1;rejects([&]{level1SuccessTransitionOperations(other);});other=p;other.introduction=1;rejects([&]{level1SuccessTransitionOperations(other);});
other=p;other.successReturned=false;rejects([&]{level1SuccessTransitionOperations(other);});other=p;other.exitRequested=true;rejects([&]{level1SuccessTransitionOperations(other);});
other=p;other.armor=0;assert(level1SuccessTransitionOperations(other).observation.armor==80);std::swap(other.frontPage,other.backPage);assert(level1SuccessTransitionOperations(other).observation.levelIndex==1);
BitmapFont font{32,8,0,std::vector<IndexedBitmap>(96,{4,8,std::vector<std::uint8_t>(32,1)})};
assert(scoreOperations(p.score,0,0,font,CockpitPageState::fromRecoveredMenu(),ScoreAwardDomain::transitionDisplay).score==p.score);
rejects([&]{scoreOperations(p.score,0,0,font,CockpitPageState::fromRecoveredMenu());});
std::vector<Model3d> models(44);models[0].flags=1;models[0].collisionRadius=723;models[8].flags=1;models[8].collisionRadius=860;models[13].collisionRadius=559;models[30].collisionRadius=868;
std::array<std::uint16_t,56> table{};for(unsigned i=0;i<table.size();++i){table[i]=static_cast<std::uint16_t>(i*1193U+83U);}StellarRng rng(table,48,110,0,100);
const auto actors=level2LoaderActors(rng,models,{});assert(rng.calls()==100&&actors.rng.calls()==112&&actors.actors.size()==7&&actors.activeEnemies==5&&actors.spawnedEnemies==5);
const std::array<unsigned,7> types{8,8,8,13,13,30,30};
for(unsigned i=0;i<types.size();++i){assert(actors.actors[i].actor.type==types[i]&&actors.actors[i].actor.allocationIndex==i);assert(actors.actors[i].slot==(i<5?i:i-5));}
assert((actors.actors[0].actor.position==FixedPosition{12288,24576,0}));
assert((actors.actors[5].actor.position==FixedPosition{-8192,20480,0}));assert(actors.actors[6].actor.yaw==0);
auto invalidModels=models;invalidModels[30].collisionRadius=0;rejects([&]{level2LoaderActors(rng,invalidModels,{});});assert(rng.calls()==100);
Level2PoolResetInput reset{};reset.armor=500;reset.systemTimers={1,2,3,4,5,6,7};
for(auto& a:reset.ordinary){a.renderProfile=WorldActorRenderProfile::noHandles;}for(auto& a:reset.fixed){a.renderProfile=WorldActorRenderProfile::noHandles;}for(auto& a:reset.effects){a.renderProfile=WorldActorRenderProfile::noHandles;}
reset.ordinary[1].primaryType11AnimationSelector=true;reset.fixed[0].renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
reset.fixed[9].position={123,456,789};reset.fixed[9].stateBytes4dTo52[1]=99;
reset.effects[2].renderProfile=WorldActorRenderProfile::heapSelectorNoCallback;
const auto cleared=level2PoolReset(reset,models[0]);assert(cleared.selectorReleases.size()==2&&cleared.selectorReleases[0].pool==Level2ResetPool::ordinary&&cleared.selectorReleases[0].slot==1&&cleared.selectorReleases[1].pool==Level2ResetPool::effect);
assert(cleared.state.fixed[9].position==reset.fixed[9].position&&cleared.state.fixed[9].stateBytes4dTo52[1]==99);
assert(cleared.state.armor==580&&cleared.state.systemTimers[3]==4&&cleared.state.systemTimers[2]==0);
assert(cleared.state.player.collisionRadius==723&&cleared.state.player.stateBytes4dTo52[1]==1);
assert(!cleared.state.ordinary[1].primaryType11AnimationSelector&&cleared.state.projectiles[0].renderProfile==WorldActorRenderProfile::inlineSelectorNoCallback);
const auto ground=level2RetainedGroundInitializationOperations(actors.rng);auto expectedRng=actors.rng;const Level1GroundState expected(expectedRng);
assert(ground.rng.calls()==124&&ground.ground.points()==expected.points());
std::uint64_t dispatches=0;for(auto op:ground.operations){assert(op.kind==NormalWorkKind::instructions);dispatches+=op.count;}assert(dispatches==469);
StellarRng wrap(table,0,0,0,0);const auto wrapped=level2RetainedGroundInitializationOperations(wrap);dispatches=0;for(auto op:wrapped.operations){dispatches+=op.count;}assert(dispatches==471&&wrapped.rng.calls()==12);
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
