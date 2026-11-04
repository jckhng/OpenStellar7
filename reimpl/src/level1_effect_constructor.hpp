#pragma once
#include "stellar7/target_hit_work.hpp"
#include <algorithm>
#include <stdexcept>
namespace stellar7::detail {
struct EffectConstructorWork {std::vector<NormalWorkOperation> operations;std::optional<Level1TargetHitEffect> effect;};
//Shared complete0BC8/4580 child. No unlisted FC fields are cleared.
inline EffectConstructorWork effectConstructorOperations(const std::array<std::uint8_t,8>& ages,
    FixedPosition position,std::uint8_t first,std::uint8_t end,std::size_t allocationCount) {
    if(first>=end || end>32) {throw std::invalid_argument("unsupported FC constructor age range");}
    EffectConstructorWork out;const auto free=std::find(ages.begin(),ages.end(),0);
    if(free==ages.end()) {out.operations.push_back({NormalWorkKind::instructions,8+68+5});return out;}
    if(allocationCount>=64) {throw std::invalid_argument("FC append exceeds source work registry domain");}
    const auto slot=static_cast<std::size_t>(free-ages.begin());
    out.operations={{NormalWorkKind::instructions,8+20+7*slot+11},{NormalWorkKind::repeatString,6},{NormalWorkKind::instructions,8}};
    out.effect=Level1TargetHitEffect{slot,allocationCount,-1,252,static_cast<std::uint8_t>(first+1),position,end,0x0ac8};return out;
}
}
