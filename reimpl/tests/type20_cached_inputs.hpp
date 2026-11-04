#pragma once
#include "stellar7/simulation_prefix_work.hpp"
#include <cassert>
#include <stdexcept>
namespace type20test {
using namespace stellar7;
template<class Actor> void common(Actor& a,const std::vector<std::int64_t>& v) {
 a.type=static_cast<std::int16_t>(v[0]);a.collisionRadius=static_cast<std::uint32_t>(v[1]);
 a.position={static_cast<std::int32_t>(v[2]),static_cast<std::int32_t>(v[3]),static_cast<std::int32_t>(v[4])};
 a.pitch=static_cast<std::uint16_t>(v[5]);a.roll=static_cast<std::uint16_t>(v[6]);a.yaw=static_cast<std::uint16_t>(v[7]);
 a.pitchVelocity=static_cast<std::int16_t>(v[8]);a.rollVelocity=static_cast<std::int16_t>(v[9]);a.yawVelocity=static_cast<std::int16_t>(v[10]);
 a.xVelocity=static_cast<std::int16_t>(v[11]);a.yVelocity=static_cast<std::int16_t>(v[12]);a.zVelocity=static_cast<std::int16_t>(v[13]);a.forwardVelocity=static_cast<std::int16_t>(v[14]);
}
inline Level1Entity actor(const std::vector<std::int64_t>& v,std::size_t index) {
 Level1Entity a{};common(a,v);a.allocationIndex=index;
 for(std::size_t i=0;i<6;++i){a.stateBytes4dTo52[i]=static_cast<std::uint8_t>(v[15+i]);}
 a.behaviorTimer=static_cast<std::uint8_t>(v[21]);a.weaponSlotCount=static_cast<std::uint8_t>(v[22]);a.weaponCooldown=static_cast<std::uint8_t>(v[23]);
 for(std::size_t i=0;i<3;++i){a.weaponSlotTimers[i]=static_cast<std::uint8_t>(v[24+i]);}
 a.presenterDistance=static_cast<std::uint32_t>(v[27]);a.targetHeading=static_cast<std::uint16_t>(v[28]);a.flags=static_cast<std::uint8_t>(v[29]);return a;
}
inline Level1Projectile shot(const std::vector<std::int64_t>& v) {
 Level1Projectile a{};common(a,v);
 for(std::size_t i=0;i<8;++i){a.stateBytes4dTo54[i]=static_cast<std::uint8_t>(v[15+i]);}
 a.presenterDistance=static_cast<std::uint32_t>(v[23]);a.targetHeading=static_cast<std::uint16_t>(v[24]);a.flags=static_cast<std::uint8_t>(v[25]);
 a.ownerIsPlayer=v[26]!=0;a.poolSlot=static_cast<std::size_t>(v[27]);a.ownerAllocationIndex=static_cast<std::size_t>(v[28]);return a;
}
inline Level1ProjectilePrefixInput carry(const Level1ProjectilePrefixWork& w) {
 return {w.player,w.entities,w.projectiles,w.allocationOrder,w.activeOrder,w.lastListCamera,w.refreshRequested,w.rng,0,false,0,true,{false,false}};
}
inline Level1EnemyPrefixProfile profile(std::uint16_t tick=1) {
 return {tick,256,{0,441},{0,134},Level1ProjectileAllocationDomain::retainedAirborne};
}
inline Level1ProjectilePrefixInput cachedInput(unsigned which) {
 Level1ProjectilePrefixInput value{};value.active=true;
 switch(which) {
 case 0: {
 value.player=actor({0,723,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0},0);
 value.entities.push_back(actor({1,1342,-1841,1961,0,0,0,41195,0,0,-143,0,0,0,0,255,1,0,0,1,2,0,2,0,0,0,0,2651,40608,0},0));
 value.entities.push_back(actor({11,748,1084,5811,1000,0,62642,22768,0,0,271,0,0,0,270,255,1,5,2,3,1,0,1,0,0,0,0,6217,30864,0},1));
 value.entities.push_back(actor({29,862,16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,24576,0},2));
 value.entities.push_back(actor({29,862,16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,8192,0},3));
 value.entities.push_back(actor({29,862,-16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,40960,0},4));
 value.entities.push_back(actor({29,862,-16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,16,0,0,243,0,0,0,0,0,0,0,22528,57344,0},5));
 value.projectiles[0]=shot({23,134,0,13500,0,0,0,0,0,0,0,0,0,0,500,253,37,0,0,0,0,0,0,13500,32768,0,1,0,-1});
 value.projectiles[1]=shot({23,134,0,9000,0,0,0,0,0,0,0,0,0,0,500,253,46,0,0,0,0,0,0,9000,32768,0,1,1,-1});
 value.projectiles[2]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,2,-1});
 value.projectiles[3]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,3,-1});
 value.projectiles[4]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,4,-1});
 value.projectiles[5]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,5,-1});
 value.projectiles[6]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,6,-1});
 value.projectiles[7]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,7,-1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(1),0});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(1),1});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(1),0});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(1),1});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.lastListCamera={0,0,0};value.refreshRequested=false;
 for(unsigned n=0;n<118;++n){value.rng.next();}
 assert(value.rng.indexA()==36 && value.rng.indexB()==98 && value.rng.last()==5593);
 break; }
 case 1: {
 value.player=actor({0,723,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0},0);
 value.entities.push_back(actor({1,1342,-1841,1961,0,0,0,41052,0,0,-143,0,0,0,0,255,1,0,0,1,0,1,2,8,54,0,0,2651,40608,0},0));
 value.entities.push_back(actor({11,748,866,5650,1000,0,62551,23039,0,0,271,0,0,0,270,255,1,5,2,3,1,0,1,0,0,0,0,5974,31200,0},1));
 value.entities.push_back(actor({29,862,16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,24576,0},2));
 value.entities.push_back(actor({29,862,16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,8192,0},3));
 value.entities.push_back(actor({29,862,-16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,40960,0},4));
 value.entities.push_back(actor({29,862,-16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,16,0,0,243,0,0,0,0,0,0,0,22528,57344,0},5));
 value.projectiles[0]=shot({23,134,0,14000,0,0,0,0,0,0,0,0,0,0,500,253,36,0,0,0,0,0,0,14000,32768,0,1,0,-1});
 value.projectiles[1]=shot({23,134,0,9500,0,0,0,0,0,0,0,0,0,0,500,253,45,0,0,0,0,0,0,9500,32768,0,1,1,-1});
 value.projectiles[2]=shot({17,441,-1841,1961,0,0,0,41052,0,0,0,0,0,0,500,253,54,0,0,0,0,0,0,0,0,0,0,2,0});
 value.projectiles[3]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,3,-1});
 value.projectiles[4]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,4,-1});
 value.projectiles[5]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,5,-1});
 value.projectiles[6]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,6,-1});
 value.projectiles[7]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,7,-1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(1),0});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(1),1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(1),2});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(1),0});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(1),1});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(1),2});
 value.lastListCamera={0,0,0};value.refreshRequested=false;
 for(unsigned n=0;n<120;++n){value.rng.next();}
 assert(value.rng.indexA()==32 && value.rng.indexB()==94 && value.rng.last()==30648);
 break; }
 case 2: {
 value.player=actor({0,723,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0},0);
 value.entities.push_back(actor({1,1342,-1841,1961,0,0,0,41195,0,0,-143,0,0,0,0,255,1,0,0,1,2,0,2,0,0,0,0,2651,40608,0},0));
 value.entities.push_back(actor({11,748,1084,5811,1000,0,62642,22768,0,0,271,0,0,0,270,255,1,5,2,3,1,0,1,0,0,0,0,6217,30864,0},1));
 value.entities.push_back(actor({29,862,16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,24576,0},2));
 value.entities.push_back(actor({29,862,16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,8192,0},3));
 value.entities.push_back(actor({29,862,-16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,40960,0},4));
 value.entities.push_back(actor({29,862,-16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,16,0,0,243,0,0,0,0,0,0,0,22528,57344,0},5));
 value.projectiles[0]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,0,-1});
 value.projectiles[1]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,1,-1});
 value.projectiles[2]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,2,-1});
 value.projectiles[3]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,3,-1});
 value.projectiles[4]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,4,-1});
 value.projectiles[5]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,5,-1});
 value.projectiles[6]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,6,-1});
 value.projectiles[7]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,7,-1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.lastListCamera={0,0,0};value.refreshRequested=false;
 for(unsigned n=0;n<118;++n){value.rng.next();}
 assert(value.rng.indexA()==36 && value.rng.indexB()==98 && value.rng.last()==5593);
 break; }
 case 3: {
 value.player=actor({0,723,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0},0);
 value.entities.push_back(actor({1,1342,-2048,2048,0,0,0,0,0,0,0,0,0,0,0,255,1,0,0,1,0,0,2,0,0,0,0,0,0,0},0));
 value.entities.push_back(actor({11,748,8192,8192,1000,0,0,24576,0,0,0,0,0,0,270,255,1,5,4,3,3,0,1,0,0,0,0,0,0,0},1));
 value.entities.push_back(actor({29,862,16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,0,0,0},2));
 value.entities.push_back(actor({29,862,16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,0,0,0},3));
 value.entities.push_back(actor({29,862,-16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,0,0,0},4));
 value.entities.push_back(actor({29,862,-16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,16,0,0,243,0,0,0,0,0,0,0,0,0,0},5));
 value.projectiles[0]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,0,-1});
 value.projectiles[1]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,1,-1});
 value.projectiles[2]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,2,-1});
 value.projectiles[3]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,3,-1});
 value.projectiles[4]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,4,-1});
 value.projectiles[5]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,5,-1});
 value.projectiles[6]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,6,-1});
 value.projectiles[7]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,7,-1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.lastListCamera={0,0,0};value.refreshRequested=true;
 for(unsigned n=0;n<29;++n){value.rng.next();}
 assert(value.rng.indexA()==102 && value.rng.indexB()==52 && value.rng.last()==38480);
 break; }
 case 4: {
 value.player=actor({0,723,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0},0);
 value.entities.push_back(actor({1,1342,-2048,2048,0,0,0,0,0,0,-143,0,0,0,0,255,1,0,0,1,2,0,2,0,0,0,0,2816,40960,0},0));
 value.entities.push_back(actor({11,748,8001,8001,1000,0,0,24576,0,0,0,0,0,0,270,255,1,5,4,3,4,63,1,0,59,0,0,11001,24576,0},1));
 value.entities.push_back(actor({29,862,16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,24576,0},2));
 value.entities.push_back(actor({29,862,16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,8192,0},3));
 value.entities.push_back(actor({29,862,-16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,40960,0},4));
 value.entities.push_back(actor({29,862,-16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,16,0,0,243,0,0,0,0,0,0,0,22528,57344,0},5));
 value.projectiles[0]=shot({20,134,8001,8001,1000,0,0,24576,0,0,0,0,0,0,500,253,60,0,0,0,0,0,0,0,0,0,0,0,1});
 value.projectiles[1]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,1,-1});
 value.projectiles[2]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,2,-1});
 value.projectiles[3]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,3,-1});
 value.projectiles[4]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,4,-1});
 value.projectiles[5]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,5,-1});
 value.projectiles[6]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,6,-1});
 value.projectiles[7]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,7,-1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(1),0});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(1),0});
 value.lastListCamera={0,0,0};value.refreshRequested=false;
 for(unsigned n=0;n<31;++n){value.rng.next();}
 assert(value.rng.indexA()==98 && value.rng.indexB()==48 && value.rng.last()==23347);
 break; }
 case 5: {
 value.player=actor({0,723,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0},0);
 value.entities.push_back(actor({1,1342,-2048,2048,0,0,0,40960,0,0,0,0,0,0,0,255,1,0,0,1,0,0,2,0,0,0,0,0,0,0},0));
 value.entities.push_back(actor({11,748,8192,8192,1000,0,0,24576,0,0,0,0,0,0,270,255,1,5,4,3,3,0,1,0,0,0,0,0,0,0},1));
 value.entities.push_back(actor({29,862,16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,0,0,0},2));
 value.entities.push_back(actor({29,862,16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,0,0,0},3));
 value.entities.push_back(actor({29,862,-16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,0,0,0},4));
 value.entities.push_back(actor({29,862,-16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,16,0,0,243,0,0,0,0,0,0,0,0,0,0},5));
 value.projectiles[0]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,0,-1});
 value.projectiles[1]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,1,-1});
 value.projectiles[2]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,2,-1});
 value.projectiles[3]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,3,-1});
 value.projectiles[4]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,4,-1});
 value.projectiles[5]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,5,-1});
 value.projectiles[6]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,6,-1});
 value.projectiles[7]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,7,-1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.lastListCamera={0,0,0};value.refreshRequested=true;
 for(unsigned n=0;n<29;++n){value.rng.next();}
 assert(value.rng.indexA()==102 && value.rng.indexB()==52 && value.rng.last()==38480);
 break; }
 case 6: {
 value.player=actor({0,723,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0},0);
 value.entities.push_back(actor({1,1342,-2048,2048,0,0,0,40960,0,0,0,0,0,0,0,255,1,0,0,1,0,0,2,0,0,0,0,0,0,0},0));
 value.entities.push_back(actor({11,748,8192,8192,1000,0,0,18432,0,0,0,0,0,0,270,255,1,5,4,3,0,0,1,0,0,0,0,0,0,0},1));
 value.entities.push_back(actor({29,862,16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,0,0,0},2));
 value.entities.push_back(actor({29,862,16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,0,0,0},3));
 value.entities.push_back(actor({29,862,-16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,0,0,0},4));
 value.entities.push_back(actor({29,862,-16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,16,0,0,243,0,0,0,0,0,0,0,0,0,0},5));
 value.projectiles[0]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,0,-1});
 value.projectiles[1]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,1,-1});
 value.projectiles[2]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,2,-1});
 value.projectiles[3]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,3,-1});
 value.projectiles[4]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,4,-1});
 value.projectiles[5]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,5,-1});
 value.projectiles[6]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,6,-1});
 value.projectiles[7]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,7,-1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.lastListCamera={0,0,0};value.refreshRequested=true;
 for(unsigned n=0;n<29;++n){value.rng.next();}
 assert(value.rng.indexA()==102 && value.rng.indexB()==52 && value.rng.last()==38480);
 break; }
 case 7: {
 value.player=actor({0,723,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0},0);
 value.entities.push_back(actor({1,1342,-2048,2048,0,0,0,40960,0,0,0,0,0,0,0,255,1,0,0,1,0,1,2,8,54,0,0,2816,40960,0},0));
 value.entities.push_back(actor({11,748,7927,8139,1000,0,65442,18432,0,0,94,0,0,0,270,255,1,5,4,3,0,26,1,0,0,0,0,11111,24736,0},1));
 value.entities.push_back(actor({29,862,16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,24576,0},2));
 value.entities.push_back(actor({29,862,16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,8192,0},3));
 value.entities.push_back(actor({29,862,-16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,40960,0},4));
 value.entities.push_back(actor({29,862,-16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,16,0,0,243,0,0,0,0,0,0,0,22528,57344,0},5));
 value.projectiles[0]=shot({17,441,-2048,2048,0,0,0,40960,0,0,0,0,0,0,500,253,54,0,0,0,0,0,0,0,0,0,0,0,0});
 value.projectiles[1]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,1,-1});
 value.projectiles[2]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,2,-1});
 value.projectiles[3]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,3,-1});
 value.projectiles[4]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,4,-1});
 value.projectiles[5]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,5,-1});
 value.projectiles[6]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,6,-1});
 value.projectiles[7]=shot({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,253,0,0,0,0,0,0,0,0,0,0,0,7,-1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(1),0});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(1),0});
 value.lastListCamera={0,0,0};value.refreshRequested=false;
 for(unsigned n=0;n<32;++n){value.rng.next();}
 assert(value.rng.indexA()==96 && value.rng.indexB()==46 && value.rng.last()==35351);
 break; }
 case 8: {
 value.player=actor({0,723,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0},0);
 value.entities.push_back(actor({1,1342,-1841,1961,0,0,0,41195,0,0,-143,0,0,0,0,255,1,0,0,1,2,0,2,0,0,0,0,2651,40608,0},0));
 value.entities.push_back(actor({11,748,1084,5811,1000,0,62642,22768,0,0,271,0,0,0,270,255,1,5,2,3,1,0,1,0,0,0,0,6217,30864,0},1));
 value.entities.push_back(actor({29,862,16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,24576,0},2));
 value.entities.push_back(actor({29,862,16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,8192,0},3));
 value.entities.push_back(actor({29,862,-16384,16384,0,0,0,0,0,0,0,0,0,0,0,254,100,0,0,0,0,0,0,0,0,0,0,22528,40960,0},4));
 value.entities.push_back(actor({29,862,-16384,-16384,0,0,0,0,0,0,0,0,0,0,0,254,16,0,0,243,0,0,0,0,0,0,0,22528,57344,0},5));
 value.projectiles[0]=shot({23,134,0,13500,0,0,0,0,0,0,0,0,0,0,500,253,37,0,0,0,0,0,0,13500,32768,0,1,0,-1});
 value.projectiles[1]=shot({23,134,0,9000,0,0,0,0,0,0,0,0,0,0,500,253,46,0,0,0,0,0,0,9000,32768,0,1,1,-1});
 value.projectiles[2]=shot({23,134,0,13500,0,0,0,0,0,0,0,0,0,0,500,253,37,0,0,0,0,0,0,13500,32768,0,1,2,-1});
 value.projectiles[3]=shot({23,134,0,13500,0,0,0,0,0,0,0,0,0,0,500,253,37,0,0,0,0,0,0,13500,32768,0,1,3,-1});
 value.projectiles[4]=shot({23,134,0,13500,0,0,0,0,0,0,0,0,0,0,500,253,37,0,0,0,0,0,0,13500,32768,0,1,4,-1});
 value.projectiles[5]=shot({23,134,0,13500,0,0,0,0,0,0,0,0,0,0,500,253,37,0,0,0,0,0,0,13500,32768,0,1,5,-1});
 value.projectiles[6]=shot({23,134,0,13500,0,0,0,0,0,0,0,0,0,0,500,253,37,0,0,0,0,0,0,13500,32768,0,1,6,-1});
 value.projectiles[7]=shot({23,134,0,13500,0,0,0,0,0,0,0,0,0,0,500,253,37,0,0,0,0,0,0,13500,32768,0,1,7,-1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(1),0});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(1),1});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(1),2});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(1),3});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(1),4});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(1),5});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(1),6});
 value.allocationOrder.push_back({static_cast<Level1PresentationKind>(1),7});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),2});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),3});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),4});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),5});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(1),0});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(1),1});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),1});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(0),0});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(1),2});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(1),3});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(1),4});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(1),5});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(1),6});
 value.activeOrder.push_back({static_cast<Level1PresentationKind>(1),7});
 value.lastListCamera={0,0,0};value.refreshRequested=false;
 for(unsigned n=0;n<118;++n){value.rng.next();}
 assert(value.rng.indexA()==36 && value.rng.indexB()==98 && value.rng.last()==5593);
 break; }
 default: throw std::invalid_argument("unknown cached prefix input");
 }
 return value;
}
}
