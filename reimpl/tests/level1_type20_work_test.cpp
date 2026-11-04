#include "type20_cached_inputs.hpp"
#include "stellar7/level1_primary_fire_work.hpp"
#include "stellar7/frame_tail_work.hpp"
#include <algorithm>
#include <cassert>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif
namespace {
using namespace stellar7;
using namespace type20test;
template<class Action> void rejects(Action action) {
 bool rejected=false;try{action();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
}
Level1ProjectilePrefixInput lifetimeInput() {
 auto input=cachedInput(4); //Controlled source following input, already owns20/FD0.
 auto& tank=input.entities[0];tank.position={-20000,-20000,0};tank.forwardVelocity=0;
 tank.yaw=0;tank.yawVelocity=0;tank.behaviorTimer=127;tank.stateBytes4dTo52[5]=0;
 tank.weaponSlotTimers={255,255,0};
 auto& lift=input.entities[1];lift.position={10000,0,1000};lift.yaw=0;lift.roll=0;
 lift.yawVelocity=0;lift.stateBytes4dTo52[5]=4;lift.behaviorTimer=63;lift.weaponSlotTimers[0]=59;
 return input;
}
Level1DamagePrefixProfile damageProfile(std::uint16_t tick) {
 return {profile(tick),680,0,0,0,Level1DamagePaletteMode::residentVga,Level1PlayerCombatDomain::expiryAndLoss};
}
}
int main() {
#ifdef _MSC_VER
 _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
 using namespace stellar7;using namespace type20test;
 auto input=cachedInput(3);
 const auto birth=level1EnemyPrefixOperations(input,profile());
 assert(birth.audioEvents.size()==1 && birth.audioEvents[0].eventId==15);
 assert(birth.prefix.projectiles[0].type==20 && birth.prefix.projectiles[0].collisionRadius==134);
 assert(birth.prefix.projectiles[0].ownerAllocationIndex==1 && !birth.prefix.projectiles[0].ownerIsPlayer);
 assert(birth.prefix.projectiles[0].stateBytes4dTo54[1]==60 && birth.prefix.projectiles[0].forwardVelocity==500);
 assert(birth.prefix.entities[1].stateBytes4dTo52[5]==4 && birth.prefix.entities[1].behaviorTimer==63);
 assert(birth.prefix.entities[1].weaponSlotTimers[0]==59 && birth.prefix.entities[1].weaponCooldown==0);
 assert(birth.prefix.allocationOrder.size()==7 && birth.prefix.activeOrder.size()==7);
 rejects([&]{level1EnemyPrefixOperations(input,{1,256,{0,441},{0,134}});});
 auto legacy=profile();legacy.allocationDomain=Level1ProjectileAllocationDomain::retained;
 rejects([&]{level1EnemyPrefixOperations(input,legacy);});
 auto banked=input;banked.entities[1].roll=22;
 const auto bankBirth=level1EnemyPrefixOperations(banked,profile());
 assert(bankBirth.prefix.projectiles[0].roll==22);
 auto bankFollow=carry(bankBirth.prefix);
 const auto bankStep=level1EnemyPrefixOperations(bankFollow,profile(2));
 assert(bankStep.prefix.projectiles[0].position.x==bankFollow.projectiles[0].position.x-354);
 assert(bankStep.prefix.projectiles[0].position.y==bankFollow.projectiles[0].position.y-354);
 assert(bankStep.prefix.projectiles[0].position.z==1000 && bankStep.prefix.projectiles[0].roll==22);
 assert(bankStep.prefix.projectiles[0].stateBytes4dTo54[1]==59 && bankStep.audioEvents.empty());
 auto live=lifetimeInput();
 for(unsigned tick=0;tick<60;++tick) {
  const auto step=level1DamagePrefixOperations(live,damageProfile(static_cast<std::uint16_t>(tick+2)));
  assert(step.armor==680 && step.combat.audioEvents.empty());
  assert(step.combat.prefix.projectiles[0].stateBytes4dTo54[1]==59-tick);
  live=carry(step.combat.prefix);
 }
 assert(!live.projectiles[0].active());
 assert(std::find(live.allocationOrder.begin(),live.allocationOrder.end(),Level1ObjectRef{Level1PresentationKind::projectile,0})==live.allocationOrder.end());
 const auto retainedDistance=live.projectiles[0].presenterDistance;
 auto reuse=live;auto& owner=reuse.entities[1];owner.stateBytes4dTo52[5]=3;
 owner.weaponSlotTimers[0]=0;owner.yaw=owner.targetHeading;owner.yawVelocity=0;
 const auto reused=level1EnemyPrefixOperations(reuse,profile(62));
 assert(reused.prefix.projectiles[0].active() && reused.prefix.projectiles[0].type==20);
 assert(reused.prefix.projectiles[0].presenterDistance==retainedDistance);
 assert(reused.audioEvents.size()==1 && reused.audioEvents[0].eventId==15);
 Level1PrimaryFireInput fire{live.player,live.projectiles,0,{},0,6,{},false,{},
  {256,0,134},Level1ProjectileAllocationDomain::retainedAirborne};
 const auto primary=level1PrimaryFireOperations(fire);
 assert(primary.launch && primary.launch->poolSlot==0 && primary.launch->projectile.type==23);
 assert(primary.launch->projectile.presenterDistance==retainedDistance);
 fire.allocationDomain=Level1ProjectileAllocationDomain::retained;
 rejects([&]{level1PrimaryFireOperations(fire);});
 auto hit=lifetimeInput();hit.projectiles[0].position={0,-500,150};hit.projectiles[0].yaw=0;
 const auto damaged=level1DamagePrefixOperations(hit,damageProfile(2));
 assert(damaged.armor==624 && damaged.fadeTicks==8 && damaged.combat.audioEvents.size()==1);
 assert(damaged.combat.audioEvents[0].eventId==6 && !damaged.combat.prefix.projectiles[0].active());
 hit=lifetimeInput();hit.projectiles[0].position={16384,15884,0};hit.projectiles[0].yaw=0;
 rejects([&]{level1DamagePrefixOperations(hit,damageProfile(2));});
 Level1TargetPrefixInput obstacleInput{hit,{},{},2,0};
 Level1TargetPrefixProfile obstacleProfile{damageProfile(2),0,Level1TargetHitDomain::initialOrdinary};
 obstacleProfile.damage.enemy.contactDomain=Level1ActorContactDomain::ordinaryActors;
 obstacleProfile.damage.enemy.membership=Level1OrdinaryMembership::periodicSpawned;
 const BitmapFont scoreFont{32,8,0,std::vector<IndexedBitmap>(96,{4,8,std::vector<std::uint8_t>(32,1)})};
 const auto obstacle=level1TargetPrefixOperations(obstacleInput,obstacleProfile,
  scoreFont,CockpitPageState::fromRecoveredMenu());
 assert(obstacle.damage.armor==680 && obstacle.damage.combat.audioEvents.empty());
 assert(!obstacle.damage.combat.prefix.projectiles[0].active()
  && obstacle.damage.combat.prefix.entities[2].stateBytes4dTo52[1]==100);
 //Hostile05BD retires without1176, score, font or FC children.
 assert(obstacle.targetHits.empty() && obstacle.scoreSteps.empty() && obstacle.score==0 && obstacle.activeEnemies==2);
 assert(std::none_of(obstacle.effects.begin(),obstacle.effects.end(),[](const auto& effect){return effect.listed;}));
 for(const auto change:{0,1,2,3,4,5}) {
  auto invalid=lifetimeInput();auto& shot=invalid.projectiles[0];
  if(change==0){shot.ownerAllocationIndex=0;}else if(change==1){shot.collisionRadius=441;}
  else if(change==2){shot.position.z=1001;}else if(change==3){shot.pitch=1;}
  else if(change==4){shot.stateBytes4dTo54[1]=61;}else{shot.rollVelocity=1;}
  rejects([&]{level1DamagePrefixOperations(invalid,damageProfile(2));});
 }
 FrameTailState tail{1,birth.prefix.player.position,0,0,{}, {}};
 for(const auto ref:birth.prefix.activeOrder) {
  if(ref.kind==Level1PresentationKind::entity){const auto& a=birth.prefix.entities[ref.index];tail.actors.push_back({a.type,a.position});}
  else{const auto& a=birth.prefix.projectiles[ref.index];tail.actors.push_back({a.type,a.position});}
 }
 FrameTailGuards guards{};guards.armor=680;guards.missionActive=true;guards.keyboardHead=guards.keyboardTail=0x1e;
 Level1PeriodicSpawnInput periodic{2,2,10,7,6,{}};
 Level1CombatEffectMembership membership{7,6,0};
 rejects([&]{level1CombatEffectFrameTailOperations(tail,guards,birth.prefix.rng,periodic,{},membership);});
 membership.projectileAllocationDomain=Level1ProjectileAllocationDomain::retainedAirborne;
 const auto returned=level1CombatEffectFrameTailOperations(tail,guards,birth.prefix.rng,periodic,{},membership);
 assert(returned.tail.state.loopTick==2 && returned.tail.state.actors.size()==7);
}
