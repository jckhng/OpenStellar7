#include "stellar7/bonus_prefix_work.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <stdexcept>
using namespace stellar7;
struct Op {int kind;std::uint64_t count;std::uint32_t value;};
struct HitCase {
 const char* label;std::int16_t type;std::uint8_t health,callback,fcSlot,fixedSlot;
 std::int16_t prior;std::uint16_t streak;std::vector<Level1ObjectRef> before,after;
 std::uint8_t healthAfter;std::int16_t priorAfter,activeAfter;std::uint16_t streakAfter;
 std::vector<Op> operations;std::vector<std::uint8_t> fixedAfter,effectAfter;
};
const std::vector<HitCase> hitCases{
 {"tank-third-fresh",1,1,1,0,4,1,2,{{Level1PresentationKind::entity,0},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::projectile,0}},{{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::projectile,0},{Level1PresentationKind::entity,6}},0,1,1,0,{{0,170,0},{2,6,0},{0,112,0},{2,6,0},{0,42,0},{-1,4,6},{0,8,0}},{0,0,42,0,74,1,0,0,65,1,0,0,114,253,255,255,100,0,0,0,0,0,0,0,0,0,0,0,79,162,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,0,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,254,1,7,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,14,165,165,17,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
 {"lift-third-fresh",11,1,3,0,4,11,2,{{Level1PresentationKind::entity,0},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::projectile,0}},{{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::projectile,0},{Level1PresentationKind::entity,6}},0,11,1,0,{{0,170,0},{2,6,0},{0,112,0},{2,6,0},{0,42,0},{-1,4,6},{0,8,0}},{0,0,42,0,74,1,0,0,65,1,0,0,114,253,255,255,100,0,0,0,0,0,0,0,0,0,0,0,79,162,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,0,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,254,1,3,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,14,165,165,17,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
 {"tank-third-fc-full",1,1,1,8,4,1,2,{{Level1PresentationKind::entity,0},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::projectile,0},{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::deathEffect,1},{Level1PresentationKind::deathEffect,2},{Level1PresentationKind::deathEffect,3},{Level1PresentationKind::deathEffect,4},{Level1PresentationKind::deathEffect,5},{Level1PresentationKind::deathEffect,6},{Level1PresentationKind::deathEffect,7}},{{Level1PresentationKind::entity,6},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::projectile,0},{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::deathEffect,1},{Level1PresentationKind::deathEffect,2},{Level1PresentationKind::deathEffect,3},{Level1PresentationKind::deathEffect,4},{Level1PresentationKind::deathEffect,5},{Level1PresentationKind::deathEffect,6},{Level1PresentationKind::deathEffect,7}},0,1,1,0,{{0,170,0},{2,6,0},{0,188,0},{-1,4,6},{0,8,0}},{0,0,42,0,74,1,0,0,65,1,0,0,114,253,255,255,100,0,0,0,0,0,0,0,0,0,0,0,79,162,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,0,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,254,1,7,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{}},
 {"tank-third-reuse-fixed0",1,1,1,0,0,1,2,{{Level1PresentationKind::entity,0},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::projectile,0}},{{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::projectile,0},{Level1PresentationKind::entity,2}},0,1,1,0,{{0,142,0},{2,6,0},{0,112,0},{2,6,0},{0,42,0},{-1,4,6},{0,8,0}},{0,0,42,0,74,1,0,0,65,1,0,0,114,253,255,255,100,0,0,0,0,0,0,0,0,0,0,0,79,160,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,0,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,254,1,7,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,14,165,165,17,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
 {"tank-second-no-birth",1,1,1,0,4,1,1,{{Level1PresentationKind::entity,0},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::projectile,0}},{{Level1PresentationKind::deathEffect,0},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::projectile,0}},0,1,1,2,{{0,123,0},{2,6,0},{0,42,0},{-1,4,6},{0,8,0}},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,254,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,14,165,165,17,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
 {"surviving-third-history",1,2,1,0,4,1,2,{{Level1PresentationKind::entity,0},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::projectile,0}},{{Level1PresentationKind::entity,0},{Level1PresentationKind::entity,1},{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},{Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5},{Level1PresentationKind::projectile,0},{Level1PresentationKind::deathEffect,0}},1,1,2,2,{{0,55,0},{2,6,0},{0,9,0},{-1,4,5},{0,6,0}},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,254,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{165,165,255,255,165,165,165,165,65,1,0,0,114,253,255,255,0,0,0,0,165,165,165,165,165,165,200,10,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,252,1,165,165,12,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165,165}},
};
Model3d bonusModel(){Model3d value{};value.scaleShift=0;value.collisionRadius=330;return value;}
Level1TargetHitInput hitInput(const HitCase& c){
 Level1TargetHitInput value{};value.target.type=c.type;value.target.position={321,-654,0};
 value.target.stateBytes4dTo52={255,c.health,0,0,c.callback,0};value.targetIndex=0;
 value.bonus={c.prior,c.streak};value.activeEnemies=2;value.allocationOrder=c.before;
 for(std::size_t slot=0;slot<value.effectAges.size();++slot){value.effectAges[slot]=slot<c.fcSlot?1:0;}
 return value;
}
auto fixedSlots(const HitCase& c){
 std::array<Level1BonusFixedSlot,16> slots{};
 for(std::size_t slot=0;slot<8;++slot){
  auto& item=slots[slot];item.provenance=Level1BonusFixedProvenance::reset;
  item.entity.stateBytes4dTo52[0]=254;
  if(slot<4){item.provenance=Level1BonusFixedProvenance::retained;item.entityIndex=slot+2;
   item.entity.allocationIndex=slot+2;item.entity.type=29;
   item.entity.stateBytes4dTo52[1]=slot==c.fixedSlot?0:100;}
 }
 return slots;
}
std::vector<Op> canonical(const Level1TargetHitWork& work){
 std::vector<Op> ops;
 for(const auto& step:work.steps){
  for(const auto op:step.operations){const auto k=static_cast<int>(op.kind);
   if(op.kind==NormalWorkKind::instructions&&!ops.empty()&&ops.back().kind==k){ops.back().count+=op.count;}
   else{ops.push_back({k,op.count,op.value});}}
  if(step.audio){ops.push_back({-1,step.audio->callerDispatches,static_cast<std::uint32_t>(step.audio->sound)});}
 }return ops;
}
void word(std::vector<std::uint8_t>& bytes,const std::size_t at,const std::uint16_t value){bytes[at]=static_cast<std::uint8_t>(value);bytes[at+1]=static_cast<std::uint8_t>(value>>8U);}
void dword(std::vector<std::uint8_t>& bytes,const std::size_t at,const std::uint32_t value){word(bytes,at,static_cast<std::uint16_t>(value));word(bytes,at+2,static_cast<std::uint16_t>(value>>16U));}
std::vector<std::uint8_t> fixedBytes(const HitCase& c,const Level1BonusTargetHitWork& work){
 std::vector<std::uint8_t> bytes(99);word(bytes,2,c.fixedSlot<4?29:0);bytes[0x4d]=254;
 if(!work.bonus||!work.bonus->birth){return bytes;}
 const auto& receipt=*work.bonus->birth;const auto& value=receipt.entity;
 bytes[0]=value.flags;word(bytes,2,value.type);dword(bytes,4,value.collisionRadius);
 dword(bytes,8,value.position.x);dword(bytes,12,value.position.y);dword(bytes,16,value.position.z);
 word(bytes,0x14,value.pitch);word(bytes,0x16,value.roll);word(bytes,0x18,value.yaw);
 word(bytes,0x1a,receipt.drawCallback);
 //Synthetic fixture address only, for the immutable +1C inline-alias journal.
 word(bytes,0x1c,static_cast<std::uint16_t>(0xa000+c.fixedSlot*0x80+receipt.inlineSelectorOffset));
 word(bytes,0x32,value.rollVelocity);word(bytes,0x34,value.yawVelocity);
 std::copy(value.stateBytes4dTo52.begin(),value.stateBytes4dTo52.end(),bytes.begin()+0x4d);return bytes;
}
std::vector<std::uint8_t> effectBytes(const Level1TargetHitEffect& value){
 std::vector<std::uint8_t> bytes(99,0xa5);word(bytes,2,value.type);
 dword(bytes,8,value.position.x);dword(bytes,12,value.position.y);dword(bytes,16,value.position.z);
 word(bytes,0x1a,value.callback);bytes[0x4d]=value.category;bytes[0x4e]=value.age;bytes[0x51]=value.endAge;return bytes;
}
auto allocationResult(const HitCase& c,const Level1BonusTargetHitWork& value){
 auto refs=c.before;
 if(value.bonus){refs=value.bonus->allocationOrder;}
 if(value.hit.effect){assert(value.hit.effect->appendedAllocationIndex==refs.size());refs.push_back({Level1PresentationKind::deathEffect,value.hit.effect->poolSlot});}
 if(value.hit.removal){const auto& r=*value.hit.removal;assert(r.lastIndex+1==refs.size()&&refs.back()==r.replacement);refs[r.removedIndex]=refs.back();refs.pop_back();}
 return refs;
}
struct UpdateCase {const char* label;std::uint16_t phase,after;std::uint64_t instructions;};
const std::vector<UpdateCase> updateCases{
 {"born-full",0,2048,260},
 {"adjacent-full",2048,4096,260},
 {"reflected-full",32768,34816,276},
 {"wrapped-flat",63488,0,46},
};
Level1Entity updateInput(const UpdateCase& c){
 Level1Entity actor{};actor.type=42;actor.collisionRadius=330;actor.position={321,-654,100};
 actor.roll=actor.yaw=c.phase;actor.rollVelocity=actor.yawVelocity=0x800;
 actor.stateBytes4dTo52={254,1,7,0,0,7};actor.modelAnimationVariant=7;
 actor.animatedModelPresenterEnabled=false;actor.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;return actor;
}
template<class Action>void rejects(Action action){bool caught=false;try{action();}catch(const std::invalid_argument&){caught=true;}assert(caught);}
int main(){
 const auto model=bonusModel();
 for(const auto& c:hitCases){const auto before=hitInput(c);const auto slots=fixedSlots(c);
  const auto result=level1BonusTargetHitOperations(before,{slots,6,44,model});const auto& work=result.hit;
  assert(work.targetHealth==c.healthAfter&&work.bonus.lastType==c.priorAfter&&work.bonus.consecutive==c.streakAfter);
  assert(work.activeEnemies==c.activeAfter&&allocationResult(c,result)==c.after&&work.allocationCount==c.after.size());
  assert(fixedBytes(c,result)==c.fixedAfter);
  assert(work.effect.has_value()==!c.effectAfter.empty());if(work.effect){assert(effectBytes(*work.effect)==c.effectAfter);}
  const auto ops=canonical(work);assert(ops.size()==c.operations.size());
  for(std::size_t i=0;i<ops.size();++i){assert(ops[i].kind==c.operations[i].kind&&ops[i].count==c.operations[i].count&&ops[i].value==c.operations[i].value);}
  if(result.bonus&&result.bonus->birth){assert(result.bonusOperationOffset==2&&result.bonus->birth->poolSlot==c.fixedSlot);}
  assert(before.target.stateBytes4dTo52[1]==c.health&&before.bonus.consecutive==c.streak);
 }
 auto input=hitInput(hitCases[0]);const auto slots=fixedSlots(hitCases[0]);
 rejects([&]{level1TargetHitOperations(input,Level1TargetHitDomain::initialOrdinary);});
 auto unknown=slots;unknown[4].provenance=Level1BonusFixedProvenance::unknown;
 rejects([&]{level1BonusTargetHitOperations(input,{unknown,6,44,model});});
 auto callback=slots;callback[4].drawCallback=0x130c;
 rejects([&]{level1BonusTargetHitOperations(input,{callback,6,44,model});});
 rejects([&]{level1BonusTargetHitOperations(input,{slots,6,43,model});});
 for(const auto& c:updateCases){const auto before=updateInput(c);const auto work=level1BonusFixedUpdateOperations(before);
  assert(work.actor.roll==c.after&&work.actor.yaw==c.after&&work.actor.pitch==0&&work.actor.position==before.position);
  std::uint64_t total=0;for(const auto op:work.operations){assert(op.kind==NormalWorkKind::instructions);total+=op.count;}
  assert(total==c.instructions&&before.roll==c.phase);
 }
 auto bad=updateInput(updateCases[0]);bad.stateBytes4dTo52[2]=8;
 rejects([&]{level1BonusFixedUpdateOperations(bad);});
 bad=updateInput(updateCases[0]);bad.rollVelocity=0;rejects([&]{level1BonusFixedUpdateOperations(bad);});
}
