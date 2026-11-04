#include "stellar7/frame_tail_work.hpp"
#include "stellar7/campaign_loader.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
using namespace stellar7;
int main(){try{
for(unsigned tick:{16U,17U,65535U}){
FrameTailState state{static_cast<std::uint16_t>(tick),{100,-250,0},20,30,{},{{29,{2000,3000,0}},{29,{4000,3000,0}},{29,{6000,3000,0}},{29,{8000,3000,0}},{25,{12000,4000,0}}}};
FrameTailGuards guards{600,{},{},0x1e,0x1e,false,false,false,false,true,false,{}};
StellarRng rng;Level1PeriodicSpawnInput pool{1,12,10,5,7,{}};pool.ordinaryPool[0].entityIndex=1;
Level1ArrivalInput arrival{0,{},1,{},pool};arrival.player.position=state.playerPosition;arrival.player.yawVelocity=20;arrival.player.forwardVelocity=30;
if((tick&15U)==0){guards.periodic=FrameTailPeriodicInput{0,1,12,10,campaignSpawnThreshold(0,1),rng.indexA(),rng.indexB(),0};}
std::vector<Model3d> models(45);Level1CombatEffectMembership membership{5,5,0};
bool oldRejected=false;try{level1DeploymentFrameTailOperations(state,guards,rng,arrival,models,membership);}catch(const std::invalid_argument&){oldRejected=true;}assert(oldRejected);
const auto result=level1SuccessFrameTailOperations(state,guards,rng,arrival,models,membership);
const auto& tail=result.periodic.tail;
assert(!tail.missionActive&&!tail.failureRequested&&!tail.entryFade&&tail.state.loopTick==static_cast<std::uint16_t>(tick+1));
assert(tail.rng.calls()==rng.calls()&&tail.state.yawVelocity==0&&tail.state.forwardVelocity==20);
assert((tail.state.playerPosition==FixedPosition{0,0,0}));assert((tail.state.actors.back().position==FixedPosition{11900,4250,0}));
assert(tail.afterEntryFade.back().kind==NormalWorkKind::instructions&&tail.afterEntryFade.back().count==5);
if((tick&15U)==0){assert(result.periodic.periodic&&!result.periodic.periodic->spawn);}
}
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
