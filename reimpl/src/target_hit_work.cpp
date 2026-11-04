#include "stellar7/bonus_target_hit_work.hpp"
#include "level1_effect_constructor.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::size_t maximumAllocations = 64;
constexpr std::uint32_t distanceBucketSize = 0x1964;
constexpr std::uint32_t maximumDistanceBucket = 5;
constexpr std::int16_t tankType = 1;
constexpr std::int16_t fixedType = 29;
constexpr std::int16_t liftType = 11;
constexpr std::int16_t level2TankType = 8;
constexpr std::int16_t level2LiftType = 13;
constexpr std::int16_t level2FixedType = 30;
constexpr std::int16_t level2ConstructorEnemyCount = 5;
constexpr std::int16_t level2PeriodicEnemyLimit = 12; //DS0995 difficulty0 total-spawn limit.
constexpr std::uint8_t ordinaryCategory = 255;
constexpr std::uint8_t fixedCategory = 254;
using Kind = NormalWorkKind;

void instructions(Level1TargetHitWork& work, const std::uint64_t count) {
    if (work.steps.empty() || work.steps.back().audio) { work.steps.push_back({{}, {}}); }
    work.steps.back().operations.push_back({Kind::instructions, count});
}

void audio(Level1TargetHitWork& work, const Level1TargetHitSound sound) {
    const auto impact = sound == Level1TargetHitSound::impact;
    work.steps.push_back({{}, Level1TargetHitAudio{sound,
        static_cast<std::uint16_t>(impact ? 0x127a : 0x1238),
        static_cast<std::uint16_t>(impact ? 0x1284 : 0x1242)}});
}

std::size_t validate(const Level1TargetHitInput& input, const Level1TargetHitDomain domain,
    const Level1TargetBonusContext* bonusContext) {
    const auto& target = input.target;
    const auto periodic = domain == Level1TargetHitDomain::level2PeriodicOrdinary;
    const auto level2 = domain == Level1TargetHitDomain::level2Constructor || periodic;
    const auto type3=domain==Level1TargetHitDomain::periodicClass1 && target.type==3;
    const auto tank = (!level2 || periodic) && target.type == tankType;
    const auto type4 = periodic && target.type == 4;
    const auto lift = (domain == Level1TargetHitDomain::initialOrdinary
        || domain==Level1TargetHitDomain::periodicClass1) && target.type == liftType;
    const auto guardian=domain==Level1TargetHitDomain::guardianNonlethal && target.type==44;
    const auto level2Tank = level2 && target.type == level2TankType;
    const auto level2Lift = level2 && target.type == level2LiftType;
    const auto ordinary = tank || type3 || type4 || lift || guardian || level2Tank || level2Lift;
    const auto fixed = target.type == (level2 ? level2FixedType : fixedType);
    const auto nextStreak = static_cast<std::uint16_t>(input.bonus.lastType == target.type
        ? input.bonus.consecutive + 1U : 1U);
    const auto lethal = target.stateBytes4dTo52[1] == input.damage;
    //118A bypasses all +51 reads; lethal fixed-target children remain strict.
    const auto fixedImpact = fixed && !level2 && !lethal;
    if ((domain != Level1TargetHitDomain::ordinaryTank && domain != Level1TargetHitDomain::initialOrdinary && domain != Level1TargetHitDomain::guardianNonlethal && domain!=Level1TargetHitDomain::periodicClass1 && !level2)
        || input.playerTarget || input.specialTarget || (guardian ? (input.damage!=1 && input.damage!=5) || target.stateBytes4dTo52[1]<=input.damage : input.damage != 1)
        || (!ordinary && !fixed)
        || target.allocationIndex != input.targetIndex
        || target.stateBytes4dTo52[0] != (ordinary ? ordinaryCategory : fixedCategory)
        || (!fixedImpact && target.stateBytes4dTo52[4] != (tank || type3 || type4 ? 1 : lift ? 3 : guardian ? 5 : level2Tank ? 2 : level2Lift ? 4 : 0))
        || target.stateBytes4dTo52[1] == 0 || target.stateBytes4dTo52[1] > 127
        || target.presenterDistance > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
        || input.activeEnemies < (ordinary ? 1 : 0)
        || input.activeEnemies > (periodic ? level2PeriodicEnemyLimit : level2 ? level2ConstructorEnemyCount : guardian ? 12 : 10)
        || input.allocationOrder.empty() || input.allocationOrder.size() > maximumAllocations
        || (lethal && ordinary && nextStreak == 3 && !bonusContext && !level2Tank && !type3)) {
        throw std::invalid_argument("unsupported Level1 target-hit source profile");
    }
    const Level1ObjectRef targetRef{Level1PresentationKind::entity, input.targetIndex};
    const auto found = std::find(input.allocationOrder.begin(), input.allocationOrder.end(), targetRef);
    if (found == input.allocationOrder.end()) {
        throw std::invalid_argument("target hit requires the live target registry identity");
    }
    for (std::size_t index = 0; index < input.allocationOrder.size(); ++index) {
        const auto ref = input.allocationOrder[index];
        if ((ref.kind != Level1PresentationKind::entity && ref.kind != Level1PresentationKind::projectile
                && ref.kind != Level1PresentationKind::deathEffect)
            || (ref.kind == Level1PresentationKind::projectile && ref.index >= Level1State::projectilePoolSize)
            || (ref.kind == Level1PresentationKind::deathEffect && ref.index >= input.effectAges.size())
            || std::find(input.allocationOrder.begin(), input.allocationOrder.begin() + index, ref)
                != input.allocationOrder.begin() + index) {
            throw std::invalid_argument("target hit requires unique bounded registry identities");
        }
    }
    const auto free = std::find(input.effectAges.begin(), input.effectAges.end(), 0);
    if (free != input.effectAges.end()) {
        const auto slot = static_cast<std::size_t>(free - input.effectAges.begin());
        if (input.allocationOrder.size() == maximumAllocations
            || std::find(input.allocationOrder.begin(), input.allocationOrder.end(),
                Level1ObjectRef{Level1PresentationKind::deathEffect, slot}) != input.allocationOrder.end()) {
            throw std::invalid_argument("free FC slot requires an available append identity");
        }
    }
    return static_cast<std::size_t>(found - input.allocationOrder.begin());
}

void effect(Level1TargetHitWork& work,const Level1TargetHitInput& input,
    const std::uint8_t first,const std::uint8_t end) {
    auto child=detail::effectConstructorOperations(input.effectAges,input.target.position,first,end,work.allocationCount);
    if(work.steps.empty() || work.steps.back().audio) {work.steps.push_back({{}, {}});}
    auto& out=work.steps.back().operations;out.insert(out.end(),child.operations.begin(),child.operations.end());
    if(child.effect) {work.effect=child.effect;++work.allocationCount;work.refreshRequested=true;}
}
}

namespace {
Level1BonusTargetHitWork targetHitPlan(const Level1TargetHitInput& input,
    const Level1TargetHitDomain domain, const Level1TargetBonusContext* bonusContext) {
    const auto targetIndex = validate(input,domain,bonusContext); //Reject every unsupported child before returning work.
    const auto& target = input.target;
    const auto ordinary = target.type == tankType || target.type == liftType
        || (domain==Level1TargetHitDomain::periodicClass1 && target.type==3)
        || ((domain == Level1TargetHitDomain::level2Constructor || domain == Level1TargetHitDomain::level2PeriodicOrdinary)
            && (target.type == level2TankType || target.type == level2LiftType))
        || (domain == Level1TargetHitDomain::level2PeriodicOrdinary && target.type == 4);
    const auto health = static_cast<std::uint8_t>(target.stateBytes4dTo52[1] - input.damage);
    Level1BonusTargetHitWork result{{health, input.bonus, input.activeEnemies, {}, {},
        input.allocationOrder.size(), input.refreshRequested, health == 0, {}},{}};
    auto& work = result.hit;
    auto allocations = input.allocationOrder;
    instructions(work, 9); //1176..1188:byte subtraction and signed positive guard.
    if (!work.destroyed) {
        instructions(work, 1 + 6); //118A and126C..1274, including0BC8 CALL.
        effect(work, input, 0, 12);
        instructions(work, 1); //1277 argument cleanup after the complete child.
        audio(work, Level1TargetHitSound::impact);
        instructions(work, 6); //1284..128B, AX0.
        return result;
    }

    const auto bonusChild = ordinary && bonusContext;
    instructions(work, (ordinary ? 2 : 4) + (bonusChild ? 2 : 3)); //Guards;11A0 PUSH/11A1 CALL.
    if (ordinary && bonusContext) {
        auto source = target;
        source.stateBytes4dTo52[1] = health;
        auto child = level1Type42BonusOperations({source,input.bonus,bonusContext->fixedSlots,
            bonusContext->nextEntityIndex,allocations,input.refreshRequested},
            bonusContext->firstBlockCapacity,bonusContext->model42);
        result.bonusOperationOffset = work.steps.front().operations.size();
        work.steps.front().operations.insert(work.steps.front().operations.end(),
            child.operations.begin(),child.operations.end());
        work.bonus = child.history;
        work.allocationCount = child.allocationOrder.size();
        work.refreshRequested = child.refreshRequested;
        allocations = child.allocationOrder;
        result.bonus = std::move(child);
        instructions(work,1); //11A4 argument cleanup after the whole90A4 RET.
    } else if (ordinary) {
        const auto sameType = input.bonus.lastType == target.type;
        work.bonus.consecutive = static_cast<std::uint16_t>(sameType ? input.bonus.consecutive + 1U : 1U);
        instructions(work, sameType ? 20 : 19); //90A4 no-bonus ordinary branch, including RET.
        if ((((domain == Level1TargetHitDomain::level2Constructor || domain == Level1TargetHitDomain::level2PeriodicOrdinary)
                && target.type == level2TankType)
                || (domain==Level1TargetHitDomain::periodicClass1 && target.type==3))
            && work.bonus.consecutive == 3) {
            instructions(work,2); //90CD/90D2: actual1622[Type3/8]==0, no904C child.
        }
    } else {
        instructions(work, 14); //90A4 type>13 bypasses streak writes.
    }
    work.bonus.lastType = target.type;
    instructions(work, 8 + 30); //11A7..11B7 andE0D0 positive32/16 complete RET8.
    const auto rawBucket = target.presenterDistance / distanceBucketSize;
    const auto bucket = std::min(rawBucket, maximumDistanceBucket);
    instructions(work, 3 + (rawBucket > maximumDistanceBucket ? 1 : 0) + 9);
    effect(work, input, static_cast<std::uint8_t>(3 * bucket + 13), static_cast<std::uint8_t>(3 * bucket + 17));
    instructions(work, 1 + 3); //11D7 effect cleanup;11DA PUSH/11DB CALL481A/cleanup.
    const auto last = work.allocationCount - 1;
    const auto replacement = work.effect
        ? Level1ObjectRef{Level1PresentationKind::deathEffect, work.effect->poolSlot}
        : allocations.back();
    work.removal = Level1TargetHitRemoval{targetIndex, last, replacement};
    instructions(work, 18 + 7 * targetIndex + (targetIndex == last ? 0 : 5));
    --work.allocationCount;
    work.refreshRequested = true;
    instructions(work, 2 + (ordinary ? 1 : 0) + 2 + 2); //11E1..1230, ordinary class/damage1.
    if (ordinary) { --work.activeEnemies; }
    audio(work, Level1TargetHitSound::destroyed);
    instructions(work, 8); //1242/1246,1262..126A AX1.
    return result;
}

} // namespace

Level1TargetHitWork level1TargetHitOperations(const Level1TargetHitInput& input,
    const Level1TargetHitDomain domain) {
    return targetHitPlan(input,domain,nullptr).hit;
}

Level1BonusTargetHitWork level1BonusTargetHitOperations(const Level1TargetHitInput& input,
    const Level1TargetBonusContext& context) {
    return targetHitPlan(input,Level1TargetHitDomain::initialOrdinary,&context);
}

} // namespace stellar7
