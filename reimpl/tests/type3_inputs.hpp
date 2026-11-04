#pragma once
#include "type20_cached_inputs.hpp"
namespace type3test {
using namespace stellar7;
inline Level1EnemyPrefixProfile profile(std::uint16_t tick=1) {
 return {tick,44,{0,441},{0,134},Level1ProjectileAllocationDomain::retainedAirborne,
  Level1ActorContactDomain::ordinaryActors,Level1OrdinaryMembership::periodicSpawned,
  Level1OrdinaryFamily::type1And3};
}
inline Level1ProjectilePrefixInput input(unsigned which) {
 Level1ProjectilePrefixInput value{};value.active=true;
 if(which==0) {
  value.player=type20test::actor({0,723,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0},0);
  value.entities.push_back(type20test::actor({1,1342,-20000,-20000,0,0,0,0,0,0,0,0,0,0,0,255,1,0,0,1,1,127,2,255,255,255,0,0,0,0},0));
  value.entities.push_back(type20test::actor({11,748,20000,-20000,1000,0,0,0,0,0,0,0,0,0,270,255,1,5,4,3,4,63,1,0,255,0,0,0,0,0},1));
  value.entities.push_back(type20test::actor({29,862,16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,0,0,0},2));
  value.entities.push_back(type20test::actor({29,862,16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,0,0,0},3));
  value.entities.push_back(type20test::actor({29,862,-16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,0,0,0},4));
  value.entities.push_back(type20test::actor({29,862,-16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,16,0,0,243,0,0,0,0,0,0,0,0,0,0},5));
  value.entities.push_back(type20test::actor({3,770,0,12000,0,0,0,32768,0,0,0,0,0,0,216,255,1,0,0,1,1,0,1,0,0,0,0,0,0,0},6));
  value.projectiles[0]=type20test::shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,0,-1});
  value.projectiles[1]=type20test::shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,1,-1});
  value.projectiles[2]=type20test::shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,2,-1});
  value.projectiles[3]=type20test::shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,3,-1});
  value.projectiles[4]=type20test::shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,4,-1});
  value.projectiles[5]=type20test::shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,5,-1});
  value.projectiles[6]=type20test::shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,6,-1});
  value.projectiles[7]=type20test::shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,7,-1});
  value.allocationOrder={{Level1PresentationKind::entity,0},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::entity,6}};
  value.activeOrder={{Level1PresentationKind::entity,6},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0}};
  value.rng=StellarRng({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,368},0,110,0,0);
 }
 else if(which==1) {
  value.player=type20test::actor({0,723,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0},0);
  value.entities.push_back(type20test::actor({1,1342,-20000,-20000,0,0,0,0,0,0,0,0,0,0,0,255,1,0,0,1,1,127,2,255,255,255,0,0,0,0},0));
  value.entities.push_back(type20test::actor({11,748,20000,-20000,1000,0,0,0,0,0,0,0,0,0,270,255,1,5,4,3,4,63,1,0,255,0,0,0,0,0},1));
  value.entities.push_back(type20test::actor({29,862,16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,0,0,0},2));
  value.entities.push_back(type20test::actor({29,862,16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,0,0,0},3));
  value.entities.push_back(type20test::actor({29,862,-16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,0,0,0},4));
  value.entities.push_back(type20test::actor({29,862,-16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,16,0,0,243,0,0,0,0,0,0,0,0,0,0},5));
  value.entities.push_back(type20test::actor({3,770,0,12000,0,0,0,32768,0,0,0,0,0,0,216,255,1,0,0,1,3,0,1,0,0,0,0,0,0,0},6));
  value.projectiles[0]=type20test::shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,0,-1});
  value.projectiles[1]=type20test::shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,1,-1});
  value.projectiles[2]=type20test::shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,2,-1});
  value.projectiles[3]=type20test::shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,3,-1});
  value.projectiles[4]=type20test::shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,4,-1});
  value.projectiles[5]=type20test::shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,5,-1});
  value.projectiles[6]=type20test::shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,6,-1});
  value.projectiles[7]=type20test::shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,7,-1});
  value.allocationOrder={{Level1PresentationKind::entity,0},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::entity,6}};
  value.activeOrder={{Level1PresentationKind::entity,6},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,0}};
  value.rng=StellarRng({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5},0,110,0,0);
 }
 else {throw std::invalid_argument("unknown Type3 control");}
 value.entities[6].animatedModelPresenterEnabled=false;
 value.entities[6].renderProfile=WorldActorRenderProfile::noHandles;
 return value;
}
}
