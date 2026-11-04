#include "stellar7/type42_bonus_work.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::int16_t tankType = 1;
constexpr std::int16_t liftType = 11;
constexpr std::int16_t bonusType = 42;
constexpr std::uint8_t tankPayload = 7; //ImmutableDS1622[1].
constexpr std::uint8_t liftPayload = 3; //ImmutableDS1622[11].
constexpr std::size_t registryCapacity = 64;
constexpr std::uint16_t sharedModelCapacity = 44;
constexpr std::uint16_t nativeModelCapacity = 256;
constexpr std::uint16_t bonusRadius = 330;
constexpr std::int16_t bonusSpin = 0x0800;
constexpr std::int32_t bonusAltitude = 100;

void instructions(Level1Type42BonusWork& work, const std::uint64_t count) {
    work.operations.push_back({NormalWorkKind::instructions, count});
}
void validateRegistry(const Level1Type42BonusInput& input) {
    const auto tank=input.source.type==tankType;
    const auto lift=input.source.type==liftType;
    if ((!tank && !lift) || input.source.stateBytes4dTo52[0]!=255
        || input.source.stateBytes4dTo52[4]!=(tank ? 1 : 3) || input.allocationOrder.empty()
        || input.allocationOrder.size() > registryCapacity
        || input.source.allocationIndex >= input.nextEntityIndex) {
        throw std::invalid_argument("unsupported Type42 bonus source/registry");
    }
    for (std::size_t index=0;index<input.allocationOrder.size();++index) {
        const auto ref=input.allocationOrder[index];
        if ((ref.kind != Level1PresentationKind::entity && ref.kind != Level1PresentationKind::projectile
                && ref.kind != Level1PresentationKind::deathEffect)
            || (ref.kind == Level1PresentationKind::entity && ref.index >= input.nextEntityIndex)
            || (ref.kind == Level1PresentationKind::projectile && ref.index >= Level1State::projectilePoolSize)
            || (ref.kind == Level1PresentationKind::deathEffect && ref.index >= Level1State::deathEffectPoolSize)
            || std::find(input.allocationOrder.begin(),input.allocationOrder.begin()+index,ref)
                != input.allocationOrder.begin()+index) {
            throw std::invalid_argument("Type42 bonus requires unique owned registry refs");
        }
    }
    if (std::find(input.allocationOrder.begin(),input.allocationOrder.end(),
            Level1ObjectRef{Level1PresentationKind::entity,input.source.allocationIndex}) == input.allocationOrder.end()) {
        throw std::invalid_argument("Type42 bonus source must remain live until1176 retirement");
    }
}
}

Level1Type42BonusWork level1Type42BonusOperations(const Level1Type42BonusInput& input,
    const std::uint16_t firstBlockCapacity, const Model3d& model42) {
    validateRegistry(input);
    Level1Type42BonusWork work{input.history,{},input.allocationOrder,input.refreshRequested,{}};
    const auto sourceType=input.source.type;
    const auto payload=sourceType==tankType ? tankPayload : liftPayload;
    const auto sameType=input.history.lastType==sourceType;
    work.history.lastType=sourceType;
    work.history.consecutive=static_cast<std::uint16_t>(sameType ? input.history.consecutive+1U : 1U);
    if (work.history.consecutive!=3) {
        instructions(work,sameType ? 20 : 19); //90A4..90F1 excluding904C.
        return work;
    }
    work.history.consecutive=0;
    instructions(work,23); //90A4..90E2, includes history reset and CALL904C.
    instructions(work,8); //904C..9057, includes CALL4506.
    instructions(work,6); //4506..4510 pool scan setup.
    std::size_t slot=0;
    for (;slot<input.fixedSlots.size();++slot) {
        const auto& fixed=input.fixedSlots[slot];
        if (fixed.provenance!=Level1BonusFixedProvenance::reset
            && fixed.provenance!=Level1BonusFixedProvenance::retained) {
            throw std::invalid_argument("Type42 bonus reached unknown fixed-record ancestry");
        }
        instructions(work,3); //4513..4519 health test.
        if (!fixed.entity.active()) { break; }
        instructions(work,4); //451B..4521 next pointer and bound.
    }
    if (slot==input.fixedSlots.size()) {
        instructions(work,1+5); //4523 jump;4578..457E returnNULL.
        instructions(work,4+5); //905A..9062 failed allocation;904C epilogue.
        instructions(work,7); //90E5..90F1 restores lastType even on failure.
        return work;
    }
    const auto& fixed=input.fixedSlots[slot];
    const auto entityIndex=fixed.entityIndex.value_or(input.nextEntityIndex);
    if ((firstBlockCapacity!=sharedModelCapacity && firstBlockCapacity!=nativeModelCapacity)
        || model42.scaleShift!=0
        || model42.collisionRadius!=bonusRadius || fixed.drawCallback!=0
        || (fixed.entityIndex && *fixed.entityIndex>=input.nextEntityIndex)
        || input.allocationOrder.size()==registryCapacity
        || std::find(input.allocationOrder.begin(),input.allocationOrder.end(),
            Level1ObjectRef{Level1PresentationKind::entity,entityIndex})!=input.allocationOrder.end()) {
        throw std::invalid_argument("unsupported Type42 selected fixed record/model/append");
    }
    for (std::size_t prior=0;prior<input.fixedSlots.size();++prior) {
        if (prior!=slot && input.fixedSlots[prior].entityIndex
            && *input.fixedSlots[prior].entityIndex==entityIndex) {
            throw std::invalid_argument("Type42 fixed-record identity is duplicated");
        }
    }
    instructions(work,14); //4526..454B partial reset and radius CALLF.
    instructions(work,6+27+15); //D03C/C05C first nonnull model block, scale0.
    instructions(work,14); //4550..4576 radius/class/refresh/append and return.
    instructions(work,13); //905A..9078 successful alias/position copy setup.
    work.operations.push_back({NormalWorkKind::repeatString,6}); //907B REPMOVSW.
    instructions(work,15); //907D..90A3 altitude/health/payload/spin and return.
    instructions(work,7); //90E5..90F1 history restore and return.

    auto entity=fixed.entity;
    entity.allocationIndex=entityIndex;
    entity.flags=0;
    entity.type=bonusType;
    entity.collisionRadius=bonusRadius;
    entity.position={input.source.position.x,input.source.position.y,bonusAltitude};
    entity.pitch=entity.roll=entity.yaw=0;
    entity.rollVelocity=entity.yawVelocity=bonusSpin;
    entity.stateBytes4dTo52[0]=254;
    entity.stateBytes4dTo52[1]=1;
    entity.stateBytes4dTo52[2]=payload;
    entity.stateBytes4dTo52[4]=0;
    entity.stateBytes4dTo52[5]=payload;
    entity.modelAnimationVariant=payload;
    entity.animatedModelPresenterEnabled=false;
    entity.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
    work.birth=Level1Type42BonusReceipt{slot,entityIndex,work.allocationOrder.size(),
        !fixed.entityIndex,entity,fixed.drawCallback};
    work.allocationOrder.push_back({Level1PresentationKind::entity,entityIndex});
    work.refreshRequested=true;
    return work;
}

} // namespace stellar7
