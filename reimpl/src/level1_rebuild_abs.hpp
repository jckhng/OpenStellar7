#pragma once
#include <cstdint>

namespace stellar7::detail {
struct Level1RebuildAbsolute {
    std::uint32_t magnitude;
    std::uint8_t branchDispatches;
    std::uint8_t storeDispatches;
};

//3768 X388A..38C2/Y38D4..390C; the caller owns the initial CMP/JL pair.
//The special branch writes the scratch magnitude itself and skips common stores.
constexpr Level1RebuildAbsolute level1RebuildAbsolute(const std::uint32_t difference) noexcept {
    if((difference&0x80000000U)==0) {
        return {difference,3,2};
    }
    if((difference&0xffffU)!=0x8000U) {
        return {0U-difference,7,2};
    }
    if(difference!=0xffff8000U) {
        return {0U-difference,9,2};
    }
    return {0x7fffU,7,0};
}
}
