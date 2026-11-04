#include "stellar7/gameplay.hpp"
#include "stellar7/bonus_prefix_work.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected=false;
    try { action(); } catch (const std::invalid_argument&) { rejected=true; }
    assert(rejected);
}
struct Ground final : Level1FrameRenderer {
    void render(Level1RenderState state) override {
        state.ground.advance(state.player.yawVelocity,state.player.forwardVelocity,state.rng);
    }
};
BitmapFont font() {
    return {32,8,0,std::vector<IndexedBitmap>(96,{4,8,std::vector<std::uint8_t>(32,1)})};
}
Level1TargetPrefixProfile profile() {
    Level1EnemyPrefixProfile enemy{1,256,{0,441},{0,134},Level1ProjectileAllocationDomain::retained,
        Level1ActorContactDomain::ordinaryActors,Level1OrdinaryMembership::periodicSpawned};
    return {{enemy,680,0,0,0,Level1DamagePaletteMode::residentVga,Level1PlayerCombatDomain::expiryAndLoss},0,Level1TargetHitDomain::initialOrdinary};
}
Level1TargetPrefixInput initial(const std::uint8_t health,const bool shotFirst,const std::size_t sourceIndex=0) {
    auto model=std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    Ground ground;
    model->tickRetainedLevel1({},ground);
    model->finishLevel1Frame();
    const auto& state=model->level1State();
    Level1TargetPrefixInput result{{state.player(),state.entities(),state.projectiles(),{}, {},{},
        false,state.rng(),0,false,0,true,{false,false}},{},{},2,0};
    auto& actor=result.prefix.entities[sourceIndex];
    actor.position={12000,4000,0};
    actor.stateBytes4dTo52[1]=health;
    actor.stateBytes4dTo52[5]=sourceIndex==1?4:1;
    actor.behaviorTimer=10;
    actor.forwardVelocity=sourceIndex==1?270:0;
    actor.yawVelocity=0;
    actor.weaponCooldown=255;
    auto& shot=result.prefix.projectiles[0];
    shot.type=23; shot.collisionRadius=134;
    shot.position={12000,3500,0}; shot.yaw=0;
    shot.forwardVelocity=500; shot.stateBytes4dTo54={253,10,0,0,0,0,0,0};
    shot.ownerIsPlayer=true;
    for (std::size_t index=0;index<result.prefix.entities.size();++index) {
        result.prefix.allocationOrder.push_back({Level1PresentationKind::entity,index});
        result.prefix.activeOrder.push_back({Level1PresentationKind::entity,index});
    }
    result.prefix.allocationOrder.push_back({Level1PresentationKind::projectile,0});
    if (shotFirst) { result.prefix.activeOrder.insert(result.prefix.activeOrder.begin(),{Level1PresentationKind::projectile,0}); }
    else { result.prefix.activeOrder.push_back({Level1PresentationKind::projectile,0}); }
    //Source0BC8 partial writes must preserve unrelated reused FC fields.
    auto& effect=result.effects[0];
    effect.entity.flags=0x23;
    effect.entity.collisionRadius=655;
    effect.entity.pitch=19;
    effect.entity.presenterDistance=777;
    effect.entity.targetHeading=888;
    effect.projected={41,42};
    return result;
}
Level1TargetPrefixInput carry(const Level1TargetPrefixWork& value) {
    const auto& prefix=value.damage.combat.prefix;
    return {{prefix.player,prefix.entities,prefix.projectiles,prefix.allocationOrder,prefix.activeOrder,
        prefix.lastListCamera,prefix.refreshRequested,prefix.rng,value.damage.fadeTicks,false,0,true,{false,false}},
        value.effects,value.bonus,value.activeEnemies,value.score};
}
bool has(const std::vector<Level1ObjectRef>& order,const Level1ObjectRef ref) {
    return std::find(order.begin(),order.end(),ref)!=order.end();
}
}
Model3d bonusModel(){Model3d model{};model.scaleShift=0;model.collisionRadius=330;return model;}
int main(){
#ifdef _MSC_VER
 _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
 const auto scoreFont=font();const auto pages=CockpitPageState::fromRecoveredMenu();const auto model=bonusModel();
 for(const std::size_t sourceIndex:{0U,1U}){
  for(const bool shotFirst:{false,true}){
   auto input=initial(1,shotFirst,sourceIndex);const auto sourceType=input.prefix.entities[sourceIndex].type;
   input.bonus={sourceType,2};
   rejects([&]{level1TargetPrefixOperations(input,profile(),scoreFont,pages);});
   const auto result=level1BonusTargetPrefixOperations(input,profile(),scoreFont,pages,44,model);
   const auto& prefix=result.prefix;const auto& state=prefix.damage.combat.prefix;
   assert(result.bonusBirths.size()==1&&result.bonusBirths[0].receipt.poolSlot==4);
   const auto& birth=result.bonusBirths[0];assert(birth.receipt.materializeEntity&&birth.receipt.entityIndex==6);
   assert(birth.resetProjection&&birth.resetProjection->entityIndex==6&&birth.resetProjection->fixedPoolSlot==4);
   assert(birth.resetProjection->sourceResetEntry==0x49f4&&birth.resetProjection->firstResetSlotCount==8);
   assert(state.entities.size()==7&&!state.entities[sourceIndex].active());
   const auto& bonus=state.entities[6];assert(bonus.type==42&&bonus.active()&&bonus.position.z==100);
   assert(bonus.roll==0&&bonus.yaw==0&&bonus.stateBytes4dTo52[2]==(sourceType==1?7:3));
   assert(has(state.allocationOrder,{Level1PresentationKind::entity,6})&&has(state.activeOrder,{Level1PresentationKind::entity,6}));
   const auto& hit=prefix.targetHits.front().work;assert(hit.effect&&hit.effect->appendedAllocationIndex==birth.receipt.appendedAllocationIndex+1);
   assert(hit.removal&&hit.removal->replacement.kind==Level1PresentationKind::deathEffect);
   assert(prefix.bonus.lastType==sourceType&&prefix.bonus.consecutive==0&&prefix.activeEnemies==1);
   assert(prefix.score==(sourceType==1?20U:50U)&&prefix.scoreSteps.size()==1);
   assert(state.entities[sourceIndex].stateBytes4dTo52[0]==255&&state.entities[sourceIndex].behaviorTimer==9);
   const auto next=level1BonusTargetPrefixOperations(carry(prefix),profile(),scoreFont,pages,44,model);
   assert(next.bonusBirths.empty()&&next.prefix.targetHits.empty()&&next.prefix.score==prefix.score);
   const auto& spin=next.prefix.damage.combat.prefix.entities[6];
   assert(spin.roll==0x800&&spin.yaw==0x800&&spin.position==bonus.position);
   assert(next.prefix.effects[0].entity.stateBytes4dTo52[1]==prefix.effects[0].entity.stateBytes4dTo52[1]);
   assert(input.prefix.entities.size()==6&&input.prefix.entities[sourceIndex].active()&&input.bonus.consecutive==2);
  }
 }
 //Retired fixed0 keeps its identity and old45D2 position. If shot is first,
 //the same pointer is reused before its old-list visit and spins that tick.
 auto input=initial(1,true);input.bonus={1,2};input.prefix.entities[2].stateBytes4dTo52[1]=0;
 input.prefix.refreshRequested=true;
 input.prefix.allocationOrder.erase(std::find(input.prefix.allocationOrder.begin(),input.prefix.allocationOrder.end(),Level1ObjectRef{Level1PresentationKind::entity,2}));
 const auto reused=level1BonusTargetPrefixOperations(input,profile(),scoreFont,pages,44,model);
 assert(reused.bonusBirths.size()==1&&!reused.bonusBirths[0].receipt.materializeEntity&&!reused.bonusBirths[0].resetProjection);
 assert(reused.bonusBirths[0].receipt.entityIndex==2&&reused.bonusBirths[0].receipt.poolSlot==0);
 assert(reused.prefix.damage.combat.prefix.entities.size()==6&&reused.prefix.damage.combat.prefix.entities[2].roll==0x800);
 //Ordinary source slots and fixed source slots are independent even when their
 //canonical materialization chronology interleaves.
 input=initial(1,true);input.bonus={1,2};auto ordinary=input.prefix.entities[0];ordinary.allocationIndex=6;ordinary.stateBytes4dTo52[1]=0;
 input.prefix.entities.push_back(ordinary);
 const auto interleaved=level1BonusTargetPrefixOperations(input,profile(),scoreFont,pages,44,model);
 assert(interleaved.bonusBirths[0].receipt.poolSlot==4&&interleaved.bonusBirths[0].receipt.entityIndex==7);
 assert(interleaved.prefix.damage.combat.prefix.entities[6].type==1&&!interleaved.prefix.damage.combat.prefix.entities[6].active());
 assert(interleaved.bonusBirths[0].resetProjection->entityIndex==7);
 //A non-emitting prefix remains the old operation with no provider/cache event.
 input=initial(1,true);input.bonus={1,1};const auto old=level1TargetPrefixOperations(input,profile(),scoreFont,pages);
 const auto same=level1BonusTargetPrefixOperations(input,profile(),scoreFont,pages,44,model);
 const auto compact=[](const std::vector<NormalWorkOperation>& ops){std::vector<NormalWorkOperation> result;
  for(const auto op:ops){if(op.kind==NormalWorkKind::instructions&&!result.empty()&&result.back().kind==op.kind){result.back().count+=op.count;}else{result.push_back(op);}}return result;};
 const auto originalOps=compact(old.damage.combat.prefix.operations),sameOps=compact(same.prefix.damage.combat.prefix.operations);
 assert(same.bonusBirths.empty()&&same.prefix.score==old.score&&sameOps.size()==originalOps.size());
 for(std::size_t i=0;i<originalOps.size();++i){const auto a=originalOps[i],b=sameOps[i];assert(a.kind==b.kind&&a.count==b.count&&a.value==b.value);}
 input=initial(1,true);input.bonus={1,2};auto strict=profile();strict.targetDomain=Level1TargetHitDomain::ordinaryTank;
 rejects([&]{level1BonusTargetPrefixOperations(input,strict,scoreFont,pages,44,model);});
 auto bad=model;bad.collisionRadius=331;rejects([&]{level1BonusTargetPrefixOperations(input,profile(),scoreFont,pages,44,bad);});
 assert(input.prefix.entities.size()==6&&input.prefix.entities[0].active()&&input.bonus.consecutive==2);
}
