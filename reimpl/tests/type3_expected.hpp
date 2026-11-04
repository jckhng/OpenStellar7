#pragma once
#include "type3_inputs.hpp"
#include <cassert>
namespace type3test {
template<class A> std::vector<std::int64_t> values(const A& a) {
 return {a.type,a.collisionRadius,a.position.x,a.position.y,a.position.z,a.pitch,a.roll,a.yaw,
  a.pitchVelocity,a.rollVelocity,a.yawVelocity,a.xVelocity,a.yVelocity,a.zVelocity,a.forwardVelocity};
}
inline std::vector<std::int64_t> values(const Level1Entity& a) {
 auto v=values<Level1Entity>(a);for(const auto x:a.stateBytes4dTo52){v.push_back(x);}
 v.insert(v.end(),{a.behaviorTimer,a.weaponSlotCount,a.weaponCooldown});
 for(const auto x:a.weaponSlotTimers){v.push_back(x);}
 v.insert(v.end(),{a.presenterDistance,a.targetHeading,a.flags});return v;
}
inline std::vector<std::int64_t> values(const Level1Projectile& a) {
 auto v=values<Level1Projectile>(a);for(const auto x:a.stateBytes4dTo54){v.push_back(x);}
 v.insert(v.end(),{a.presenterDistance,a.targetHeading,a.flags,a.ownerIsPlayer,
  static_cast<std::int64_t>(a.poolSlot),a.ownerAllocationIndex==static_cast<std::size_t>(-1)
  ? -1:static_cast<std::int64_t>(a.ownerAllocationIndex)});return v;
}
inline void sourceEqual(const Level1EnemyPrefixWork& planned,unsigned which) {
 const auto& w=planned.prefix;
 std::vector<std::vector<std::int64_t>> entities,projectiles;
 std::vector<std::int64_t> player,rng;
 std::vector<NormalWorkOperation> expected;
 std::vector<Level1ObjectRef> allocations,order;
 std::vector<Level1AudioEvent> events;
 FixedPosition camera{};bool refresh=false;
 if(which==0) {
 player={0,723,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0};
 entities={{1,1342,-20000,-20000,0,0,0,0,0,0,0,0,0,0,0,255,1,0,0,1,1,126,2,255,255,255,0,27500,57344,0},{11,748,20000,-19730,1000,0,0,0,0,0,0,0,0,0,270,255,1,5,4,3,4,62,1,0,254,0,0,27398,8272,0},{29,862,16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,24576,0},{29,862,16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,8192,0},{29,862,-16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,40960,0},{29,862,-16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,16,0,0,243,0,0,0,0,0,0,0,22528,57344,0},{3,770,0,11784,0,0,0,32768,0,0,-624,0,0,0,216,255,1,0,0,1,1,1,1,0,0,0,0,11784,32768,0}};
 projectiles={{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,0,-1},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,1,-1},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,2,-1},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,3,-1},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,4,-1},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,5,-1},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,6,-1},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,7,-1}};
 rng={110,108,368,1,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,368,736,1104,1472,1840,2208,2576,2944,3312,3680,4048,4416,4784,5152,5520,5888,6256,6624,6992,7360,7728,8096,8464,8832,9200,9568,9936,10304,10672,11040,11408,11776,12144,12512,12880,13248,13616,13984,14352,14720,15088,15456,15824,16192,16560,16928,17296,17664,18032,18400,18768,19136,19504,19872,20240,20608,20976,21712};
 expected={{NormalWorkKind::instructions,5954,0}};
allocations={{Level1PresentationKind::entity,0},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::entity,6}};
order={{Level1PresentationKind::entity,0},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,6}};
 events={};
 camera={0,0,0};refresh=false;
 }
 else if(which==1) {
 player={0,723,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0};
 entities={{1,1342,-20000,-20000,0,0,0,0,0,0,0,0,0,0,0,255,1,0,0,1,1,126,2,255,255,255,0,27500,57344,0},{11,748,20000,-19730,1000,0,0,0,0,0,0,0,0,0,270,255,1,5,4,3,4,62,1,0,254,0,0,27398,8272,0},{29,862,16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,24576,0},{29,862,16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,8192,0},{29,862,-16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,40960,0},{29,862,-16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,16,0,0,243,0,0,0,0,0,0,0,22528,57344,0},{3,770,0,11784,0,0,0,32768,0,0,0,0,0,0,216,255,1,0,0,1,0,1,1,0,60,0,0,11784,32768,0}};
 projectiles={{20,134,0,11784,0,0,0,32768,0,0,0,0,0,0,500,253,60,0,0,0,0,0,0,0,0,0,0,0,6},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,1,-1},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,2,-1},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,3,-1},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,4,-1},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,5,-1},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,6,-1},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,7,-1}};
 rng={110,108,5,1,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,80,85,90,95,100,105,110,115,120,125,130,135,140,145,150,155,160,165,170,175,180,185,190,195,200,205,210,215,220,225,230,235,240,245,250,255,260,265,270,275,280,285,295};
 expected={{NormalWorkKind::instructions,2515,0},{NormalWorkKind::repeatString,6,0},{NormalWorkKind::instructions,4,0},{NormalWorkKind::string,1,0},{NormalWorkKind::string,1,0},{NormalWorkKind::string,1,0},{NormalWorkKind::instructions,79,0},{NormalWorkKind::gameplayEnemyFireEvent,1,15},{NormalWorkKind::instructions,3532,0},{NormalWorkKind::repeatString,6,0},{NormalWorkKind::instructions,454,0}};
allocations={{Level1PresentationKind::entity,0},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::entity,6},{Level1PresentationKind::projectile,0}};
order={{Level1PresentationKind::entity,0},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::entity,6},{Level1PresentationKind::projectile,0}};
 events={{1,15}};
 camera={0,0,0};refresh=false;
 }
 else {throw std::invalid_argument("unknown Type3 source oracle");}
 assert(values(w.player)==player && w.entities.size()==entities.size());
 for(std::size_t i=0;i<entities.size();++i){assert(values(w.entities[i])==entities[i]);}
 for(std::size_t i=0;i<projectiles.size();++i){assert(values(w.projectiles[i])==projectiles[i]);}
 assert(w.allocationOrder==allocations && w.activeOrder==order && w.lastListCamera==camera && w.refreshRequested==refresh);
 assert(w.rng.indexA()==rng[0] && w.rng.indexB()==rng[1] && w.rng.last()==rng[2] && w.rng.calls()==rng[3]);
 auto future=w.rng;for(std::size_t i=4;i<rng.size();++i){assert(future.next()==rng[i]);}
 assert(planned.audioEvents.size()==events.size());
 for(std::size_t i=0;i<events.size();++i){assert(planned.audioEvents[i].loopTick==events[i].loopTick && planned.audioEvents[i].eventId==events[i].eventId);}
 std::vector<NormalWorkOperation> canonical;
 for(const auto op:w.operations){
  if(op.kind==NormalWorkKind::instructions && !canonical.empty() && canonical.back().kind==op.kind){canonical.back().count+=op.count;}
  else {canonical.push_back(op);}
 }
 assert(canonical.size()==expected.size());
 for(std::size_t i=0;i<expected.size();++i){assert(canonical[i].kind==expected[i].kind && canonical[i].count==expected[i].count && canonical[i].value==expected[i].value);}
}
}
