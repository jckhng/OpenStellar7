#include "stellar7/failure_effect_work.hpp"
#include "failure_effect_age.hpp"
#include "stellar7/combat_effect_work.hpp"
#include <array>
#include <stdexcept>

namespace stellar7 {
namespace {
enum class EffectProfile { failure, ordinaryCombat, guardianCombat };
constexpr std::int32_t nearPlane = 128;
constexpr std::uint8_t firstFrame = 14;
constexpr std::uint8_t endFrame = 17;
constexpr std::uint16_t effectCallback = 0x0ac8;
void instructions(std::vector<NormalWorkOperation>& work,const std::uint64_t count) {
    work.push_back({NormalWorkKind::instructions,count});
}
void write(FailureEffectWork& work,const FailureEffectField field,const std::int32_t value) {
    work.stateWrites.emplace_back(FailureEffectScalarWrite{field,value});
}
enum class BoundSide { minimum, maximum };
bool bound(FailureEffectWork& work,const std::int32_t coordinate,const std::int16_t limit,const BoundSide side) {
    const bool minimum = side == BoundSide::minimum;
    const auto high = coordinate < 0 ? -1 : coordinate >> 16;
    if ((minimum && high < 0) || (!minimum && high > 0)) {
        instructions(work.operations,4); return false;
    }
    if (high != 0) { instructions(work.operations,5); return true; }
    instructions(work.operations,7);
    return minimum ? coordinate >= limit : coordinate <= limit;
}
std::int32_t divide(FailureEffectWork& work,const std::int32_t coordinate,const std::int32_t depth) {
    //5413/5392 simple signed64/32 branch, denominator128..32767 and numerator<2^23.
    instructions(work.operations,53+(coordinate < 0 ? 18U : 0U));
    return static_cast<std::int32_t>(static_cast<std::int64_t>(coordinate)*128/depth);
}
void remove(FailureEffectWork& work,const FailureEffectInput& input) {
    if (input.allocationCount == 0) { instructions(work.operations,10); return; }
    if (!input.matchingIndex) { instructions(work.operations,12+7*input.allocationCount); return; }
    const auto index = *input.matchingIndex;
    const auto last = input.allocationCount-1;
    instructions(work.operations,18+7*index+(index == last ? 0 : 5));
    work.removal = FailureEffectRemoval{index,last};
    write(work,FailureEffectField::registryRefresh,1);
}
}
static FailureEffectWork effectOperations(const FailureEffectInput& input,
    const std::vector<IndexedBitmap>& effectFrames, const EffectProfile profile) {
    if (input.collisionRadius != 0 || (input.state.flags != 0x88 && input.state.flags != 0x8c)
        || input.state.projectedRadius != 0 || input.state.callback != effectCallback
        || (input.page != 0xa000 && input.page != 0xa800)
        || input.center.y < -32768 || input.center.y > 32767
        || input.center.x < -65535 || input.center.x > 65535
        || input.center.z < -65535 || input.center.z > 65535
        || input.allocationCount > 64
        || (input.matchingIndex && *input.matchingIndex >= input.allocationCount)) {
        throw std::invalid_argument("unsupported failure bitmap effect");
    }
    FailureEffectWork result{{},{},input.state,input.center,false,false,{},std::nullopt};
    auto& work = result.operations;
    instructions(work,22+(input.center.y < 0 ? 4U : input.center.y < nearPlane ? 6U : 3U)+5);
    instructions(work,111); //4B57/5413/5392: zero radius, bounded positive divisor.
    instructions(work,16); //Clamp, flags8 and cache-copy setup.
    work.push_back({NormalWorkKind::repeatString,6});
    result.stateWrites.emplace_back(FailureEffectCacheWrite{input.center});
    result.state.flags &= 0xfb;
    write(result,FailureEffectField::flags,result.state.flags);
    if (input.center.y < nearPlane) {
        instructions(work,input.center.y < 0 ? 6 : 9); //Clearbit4, near test andBBCE jump.
    } else {
        instructions(work,13); //Clearbit4, near tests and4ABF CALL.
        instructions(work,52);
        const auto x = input.screenCenter.x+divide(result,input.center.x,input.center.y);
        instructions(work,50);
        const auto y = input.screenCenter.y-divide(result,input.center.z,input.center.y);
        instructions(work,6+11); //4ABF epilogue and screen-center offsets.
        if (bound(result,x,input.viewport.left,BoundSide::minimum) && bound(result,x,input.viewport.right,BoundSide::maximum)
            && bound(result,y,input.viewport.top,BoundSide::minimum) && bound(result,y,input.viewport.bottom,BoundSide::maximum)) {
            instructions(work,8);
            result.state.projectedRadius = 0;
            result.state.projected = {static_cast<std::int16_t>(x),static_cast<std::int16_t>(y)};
            result.state.flags |= 4;
            result.projected = true;
            write(result,FailureEffectField::projectedRadius,0);
            write(result,FailureEffectField::projectedX,x);
            write(result,FailureEffectField::projectedY,y);
            write(result,FailureEffectField::flags,result.state.flags);
        }
    }
    instructions(work,7+14+9); //Premodel gate/CALL, flags80 CB7A, deferred0AC8 CALL.
    write(result,FailureEffectField::rasterStyle,1);
    const auto age = detail::failureEffectAge(result.state.age, result.state.endAge);
    result.state.age = age.incremented;
    write(result,FailureEffectField::age,result.state.age);
    if (result.projected) {
        UiBitmapWork bitmapWork;
        constexpr std::uint8_t sentinelFrame=31;
        const auto sentinel=profile==EffectProfile::guardianCombat
            && input.state.age==sentinelFrame && effectFrames.size()==sentinelFrame;
        if (sentinel) {
            bitmapWork=uiEffectSentinelOperations(result.state.projected,input.viewport,input.page);
        } else {
            if (effectFrames.size() <= input.state.age) { throw std::invalid_argument("missing failure bitmap frame"); }
            const auto& bitmap = effectFrames[input.state.age];
            if (profile == EffectProfile::failure) {
                constexpr std::array<std::array<std::uint16_t,2>,3> dimensions{{{88,63},{96,68},{56,46}}};
                const auto& expected = dimensions[input.state.age-firstFrame];
                if (bitmap.width != expected[0] || bitmap.height != expected[1]) {
                    throw std::invalid_argument("unsupported failure bitmap dimensions");
                }
            }
            const auto half = static_cast<std::int16_t>(bitmap.height/2);
            bitmapWork = uiBitmapOperations(bitmap,
                {static_cast<std::int16_t>(result.state.projected.x-half),
                 static_cast<std::int16_t>(result.state.projected.y-half)},input.viewport,input.page);
        }
        instructions(work,35); //0AC8..0B99 bitmap CALLF.
        work.insert(work.end(),bitmapWork.operations.begin(),bitmapWork.operations.end());
        for (const auto& effect : bitmapWork.effects) { result.pixels.push_back(std::get<RadarPixelWrite>(effect)); }
        instructions(work,1); //0B9E cleanup.
    } else {
        instructions(work,20); //0AC8..0AFC, no bitmap/table access.
    }
    instructions(work,4); //0BA1..0BAC style reset and signed age/end test.
    write(result,FailureEffectField::rasterStyle,0);
    if (age.retired) {
        instructions(work,2); //0BAE PUSH/0BAF CALL481A.
        remove(result,input);
        instructions(work,5); //Cleanup and callback/active/end clears.
        result.state.callback = 0; result.state.age = 0; result.state.endAge = 0; result.retired = true;
        write(result,FailureEffectField::callback,0);
        write(result,FailureEffectField::age,0);
        write(result,FailureEffectField::endAge,0);
    }
    instructions(work,5+1); //0AC8 epilogue andBC06 callback-argument cleanup.
    return result;
}
FailureEffectWork failureEffectOperations(const FailureEffectInput& input,
    const std::vector<IndexedBitmap>& effectFrames) {
    if (input.state.age < firstFrame || input.state.age >= endFrame || input.state.endAge != endFrame
        || !(input.viewport == Viewport{0,318,0,199})
        || input.screenCenter.x != 159 || (input.screenCenter.y != 99 && input.screenCenter.y != 100)) {
        throw std::invalid_argument("unsupported failure bitmap effect");
    }
    return effectOperations(input,effectFrames,EffectProfile::failure);
}

FailureEffectWork level1CombatEffectOperations(const FailureEffectInput& input,
    const std::vector<IndexedBitmap>& effectFrames) {
    constexpr std::uint8_t nonlethalEnd = 12;
    constexpr std::uint8_t firstLethalEnd = 17;
    constexpr std::uint8_t lastLethalEnd = 29;
    constexpr std::uint8_t lethalStride = 3;
    const auto nonlethal = input.state.endAge == nonlethalEnd
        && input.state.age >= 1 && input.state.age < nonlethalEnd;
    const auto lethal = input.state.endAge >= firstLethalEnd && input.state.endAge <= lastLethalEnd
        && (input.state.endAge-firstLethalEnd) % lethalStride == 0
        && input.state.age >= input.state.endAge-lethalStride && input.state.age < input.state.endAge;
    if ((!nonlethal && !lethal) || !(input.viewport == Viewport{24,294,24,145})
        || !(input.screenCenter == ProjectedPoint{159,84})) {
        throw std::invalid_argument("unsupported ordinary combat bitmap effect");
    }
    return effectOperations(input,effectFrames,EffectProfile::ordinaryCombat);
}
FailureEffectWork level1GuardianEffectOperations(const FailureEffectInput& input,
    const std::vector<IndexedBitmap>& effectFrames) {
    constexpr std::uint8_t firstLethalEnd=17;
    constexpr std::uint8_t lastLethalEnd=32;
    constexpr std::uint8_t lethalStride=3;
    constexpr std::uint8_t lethalFrames=4;
    if (input.state.endAge==12) { return level1CombatEffectOperations(input,effectFrames); }
    if (input.state.endAge<firstLethalEnd || input.state.endAge>lastLethalEnd
        || (input.state.endAge-firstLethalEnd)%lethalStride!=0
        || input.state.age<input.state.endAge-lethalFrames || input.state.age>=input.state.endAge
        || !(input.viewport==Viewport{24,294,24,145}) || !(input.screenCenter==ProjectedPoint{159,84})) {
        throw std::invalid_argument("unsupported Level1 guardian bitmap effect");
    }
    return effectOperations(input,effectFrames,EffectProfile::guardianCombat);
}
} // namespace stellar7

namespace stellar7 {
FailureEffectWork level2FailureEffectOperations(const FailureEffectInput& input,
    const std::vector<IndexedBitmap>& frames) {
    constexpr std::uint8_t nonlethalEnd = 12;
    constexpr std::uint8_t firstLethalEnd = 17;
    constexpr std::uint8_t lastLethalEnd = 29;
    constexpr std::uint8_t lethalStride = 3;
    const auto nonlethal = input.state.endAge == nonlethalEnd
        && input.state.age >= 1 && input.state.age < nonlethalEnd;
    const auto lethal = input.state.endAge >= firstLethalEnd && input.state.endAge <= lastLethalEnd
        && (input.state.endAge-firstLethalEnd)%lethalStride == 0
        && input.state.age >= input.state.endAge-lethalStride && input.state.age < input.state.endAge;
    if ((!nonlethal && !lethal) || !(input.viewport == Viewport{0,318,0,199})
        || !(input.screenCenter == ProjectedPoint{159,99})) {
        throw std::invalid_argument("unsupported Level2 failure bitmap effect");
    }
    return effectOperations(input,frames,EffectProfile::ordinaryCombat);
}
} // namespace stellar7
