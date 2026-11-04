#pragma once
#include "stellar7/arrival_prefix_work.hpp"
#include "arrival_work_fixtures.hpp"
namespace arrival_prefix_fixture {
using namespace stellar7;
inline Level1TargetPrefixProfile profile(){return {{{1,256,{0,441},{0,134},Level1ProjectileAllocationDomain::retained,Level1ActorContactDomain::ordinaryActors,Level1OrdinaryMembership::periodicSpawned},680,0,0,0,Level1DamagePaletteMode::residentVga,Level1PlayerCombatDomain::expiryAndLoss},0,Level1TargetHitDomain::initialOrdinary};}
inline BitmapFont font(){return {32,8,0,std::vector<IndexedBitmap>(96,{4,8,std::vector<std::uint8_t>(32,1)})};}
inline Model3d bonusModel(){Model3d model{};model.collisionRadius=330;return model;}
inline Level1TargetPrefixInput input(){
 Level1Entity player{};player.type=0;player.collisionRadius=723;player.stateBytes4dTo52[1]=1;
 Level1PeriodicSpawnInput pool{0,10,10,4,6,{}};pool.ordinaryPool[0]={0,false,0};pool.ordinaryPool[1]={0,false,1};
 const auto birth=level1ArrivalOperations({1,{},{},player,pool},StellarRng{},arrival_fixture::models());
 Level1TargetPrefixInput in{{player,{}, {},{}, {},{},true,birth.rng,0,true,0,true,{false,false}}, {},{},1,0};
 in.prefix.entities.resize(6);in.prefix.entities[0]=birth.birth->actor;
 auto& dead=in.prefix.entities[1];dead.allocationIndex=1;dead.type=11;dead.collisionRadius=748;dead.position={14000,14000,1000};dead.forwardVelocity=270;dead.stateBytes4dTo52={255,0,5,0,3,0};dead.weaponSlotCount=1;dead.primaryType11AnimationSelector=true;
 for(std::size_t i=0;i<4;++i){auto& a=in.prefix.entities[i+2];a.allocationIndex=i+2;a.type=29;a.collisionRadius=862;a.position={static_cast<std::int32_t>(2000+i*2000),8000,0};a.stateBytes4dTo52={254,100,0,0,0,0};in.prefix.allocationOrder.push_back({Level1PresentationKind::entity,i+2});in.prefix.activeOrder.push_back({Level1PresentationKind::entity,i+2});}
 in.prefix.allocationOrder.push_back({Level1PresentationKind::entity,0});
 for(std::size_t i=0;i<in.prefix.projectiles.size();++i){in.prefix.projectiles[i].poolSlot=i;in.prefix.projectiles[i].stateBytes4dTo54[0]=253;}
 return in;
}
inline Level1BonusPickupPrefixWork run(const Level1TargetPrefixInput& in){std::array<std::int8_t,7> systems{};systems.fill(3);std::array<std::int16_t,7> timers{};const std::vector<IndexedBitmap> art;return level1ArrivalPrefixOperations(in,profile(),font(),CockpitPageState::fromRecoveredMenu(),256,bonusModel(),{systems,timers,-1,art},{0,{}});}
inline Level1TargetPrefixInput carry(const Level1BonusPickupPrefixWork& out){const auto& target=out.bonus.prefix;const auto& p=target.damage.combat.prefix;return {{p.player,p.entities,p.projectiles,p.allocationOrder,p.activeOrder,p.lastListCamera,p.refreshRequested,p.rng,target.damage.fadeTicks,true,0,true,{false,false}},target.effects,target.bonus,target.activeEnemies,target.score};}
inline Level1TargetPrefixInput withReusedOwnerShot(Level1TargetPrefixInput in){auto& shot=in.prefix.projectiles[0];shot.type=17;shot.collisionRadius=441;shot.position={25000,25000,0};shot.forwardVelocity=500;shot.stateBytes4dTo54={253,10,0,0,0,0,0,0};shot.ownerAllocationIndex=0;in.prefix.activeOrder.push_back({Level1PresentationKind::projectile,0});in.prefix.allocationOrder.insert(in.prefix.allocationOrder.end()-1,{Level1PresentationKind::projectile,0});return in;}

}
