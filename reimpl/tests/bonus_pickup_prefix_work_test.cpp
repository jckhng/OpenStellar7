#include "stellar7/bonus_pickup_prefix_work.hpp"
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
std::vector<IndexedBitmap> art(){std::vector<IndexedBitmap> result(57,{4,1,{1,0,2,0}});
 for(const auto item:std::array<std::array<std::uint16_t,3>,3>{{{43,16,20},{47,40,12},{49,8,3}}}){
  auto& frame=result[item[0]];frame={item[1],item[2],std::vector<std::uint8_t>(item[1]*item[2])};
  for(std::size_t j=0;j<frame.pixels.size();++j){frame.pixels[j]=j%3?static_cast<std::uint8_t>(j%13+1):0;}}
 return result;}
int main(){
#ifdef _MSC_VER
 _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
 const auto scoreFont=font();const auto group=art();const auto model=bonusModel();
 std::array<std::int8_t,Level1State::systemCount> systems{};systems.fill(3);
 std::array<std::int16_t,Level1State::systemCount> timers{};
 const Level1BonusPickupContext context{systems,timers,-1,group};
 auto pages=CockpitPageState::fromRecoveredMenu();
 for(unsigned parity=0;parity<2;++parity){
  for(const std::size_t sourceIndex:{0U,1U}){
   auto seed=initial(1,true,sourceIndex);seed.bonus={seed.prefix.entities[sourceIndex].type,2};
   const auto born=level1BonusTargetPrefixOperations(seed,profile(),scoreFont,pages,44,model);
   auto incoming=carry(born.prefix);const auto index=born.bonusBirths[0].receipt.entityIndex;
   const auto neutral=level1BonusPickupPrefixOperations(incoming,profile(),scoreFont,pages,44,model,context);
   assert(neutral.pickups.empty()&&neutral.drawingSteps.empty()&&!neutral.presentation&&neutral.systems==systems&&neutral.timers==timers&&neutral.selectedSystem==-1);
   const auto position=incoming.prefix.entities[index].position;
   incoming.prefix.player.position={position.x,position.y,0};
   rejects([&]{level1BonusTargetPrefixOperations(incoming,profile(),scoreFont,pages,44,model);});
   const auto result=level1BonusPickupPrefixOperations(incoming,profile(),scoreFont,pages,44,model,context);
   const auto& prefix=result.bonus.prefix;const auto& state=prefix.damage.combat.prefix;
   assert(result.pickups.size()==1&&result.pickups[0].entityIndex==index&&result.pickups[0].payload==(sourceIndex==0?7:3));
   assert(result.bonus.bonusBirths.empty()&&prefix.score==incoming.score+100&&prefix.activeEnemies==incoming.activeEnemies);
   assert(!state.entities[index].active()&&state.entities[index].collisionContactIsPlayer);
   assert(state.entities[index].collisionContactAllocationIndex==static_cast<std::size_t>(-1));
   assert(state.entities[index].roll==0x800&&state.entities[index].yaw==0x800&&state.entities[index].position==position);
   assert(!has(state.allocationOrder,{Level1PresentationKind::entity,index})&&!has(state.activeOrder,{Level1PresentationKind::entity,index}));
   assert(result.systems==systems&&result.timers==timers&&result.selectedSystem==-1);
   assert(std::count_if(prefix.damage.combat.audioEvents.begin(),prefix.damage.combat.audioEvents.end(),
       [](const auto& event){return event.eventId==24;})==1);
   unsigned markers=0;for(const auto op:state.operations){if(op.kind==NormalWorkKind::gameplayBonusPickupEvent){assert(op.count==1&&op.value==24);++markers;}}
   assert(markers==1&&prefix.scoreSteps.size()==1&&prefix.scoreSteps[0].work.fontSelections.size()==2);
   assert(result.drawingSteps.size()==1&&result.drawingSteps[0].operationOffset==result.pickups[0].operationEnd);
   assert(prefix.scoreSteps[0].operationOffset<result.drawingSteps[0].operationOffset);
   assert(result.presentation&&result.presentation->pages.drawPage()==pages.backPage()&&result.presentation->pages.copyPage()==pages.frontPage());
   assert(result.presentation->pages.frontPage()==pages.frontPage()&&result.presentation->pages.backPage()==pages.backPage()&&result.presentation->pages.cachePage()==pages.cachePage());
   assert(result.presentation->fillStyle==1&&result.presentation->fillColor==137&&result.presentation->selectedFont==0);
   auto picked=incoming.prefix.entities[index];picked.collisionContactIsPlayer=true;
   picked.collisionContactAllocationIndex=static_cast<std::size_t>(-1);
   const auto child=level1Type42PickupOperations({picked,0,incoming.score,systems,timers,-1,0,incoming.prefix.allocationOrder},scoreFont,group,pages);
   std::vector<NormalWorkOperation> expectedOps;auto expected=std::make_unique<IndexedPages>();
   auto actual=std::make_unique<IndexedPages>();
   for(const auto page:{pages.frontPage(),pages.backPage(),pages.cachePage()}){
    for(std::int16_t y=0;y<200;++y){for(std::int16_t x=0;x<320;++x){
     const auto color=static_cast<std::uint8_t>(x*7+y*13+(page>>4));
     expected->write(page,{x,y},color);actual->write(page,{x,y},color);
    }}
   }
   for(const auto& step:child.steps){
    if(const auto* drawing=std::get_if<Level1Type42PickupDrawing>(&step)){expectedOps.insert(expectedOps.end(),drawing->operations.begin(),drawing->operations.end());expected->apply(drawing->effects);}
    else if(const auto* score=std::get_if<ScoreWork>(&step)){expected->apply(score->effects);}
    else{expectedOps.push_back({NormalWorkKind::gameplayBonusPickupEvent,1,24});}}
   const auto& observation=result.pickups[0];assert(observation.operationEnd-observation.operationBegin==expectedOps.size());
   for(std::size_t i=0;i<expectedOps.size();++i){const auto a=expectedOps[i],b=state.operations[observation.operationBegin+i];assert(a.kind==b.kind&&a.count==b.count&&a.value==b.value);}
   actual->apply(prefix.scoreSteps[0].work.effects);actual->apply(result.drawingSteps[0].effects);
   for(const auto page:{pages.frontPage(),pages.backPage(),pages.cachePage()}){
    for(std::int16_t y=0;y<200;++y){for(std::int16_t x=0;x<320;++x){assert(expected->read(page,{x,y})==actual->read(page,{x,y}));}}}
   assert(incoming.prefix.entities[index].active()&&!incoming.prefix.entities[index].collisionContactIsPlayer&&incoming.score+100==prefix.score);
   const auto next=level1BonusPickupPrefixOperations(carry(prefix),profile(),scoreFont,pages,44,model,context);
   assert(next.pickups.empty()&&next.drawingSteps.empty()&&!next.presentation&&next.bonus.prefix.score==prefix.score);
   assert(next.bonus.prefix.damage.combat.prefix.entities[index].roll==0x800);
   auto wrong=systems;wrong[0]=2;const Level1BonusPickupContext badStatus{wrong,timers,-1,group};
   rejects([&]{level1BonusPickupPrefixOperations(incoming,profile(),scoreFont,pages,44,model,badStatus);});
   auto busy=timers;busy[0]=1;const Level1BonusPickupContext badTimer{systems,busy,-1,group};
   rejects([&]{level1BonusPickupPrefixOperations(incoming,profile(),scoreFont,pages,44,model,badTimer);});
   const Level1BonusPickupContext badSelection{systems,timers,0,group};
   rejects([&]{level1BonusPickupPrefixOperations(incoming,profile(),scoreFont,pages,44,model,badSelection);});
  }
  pages.flip();
 }
}
