#pragma once
#include "stellar7/bonus_pickup_prefix_work.hpp"
namespace stellar7 {
struct Level1ArrivalPrefixContext {
    std::size_t craftEntityIndex;
    std::optional<std::size_t> guardianEntityIndex;
};
//Same whole mixed prefix and pickup ownership, plus one Type7 approach craft.
//Its fresh49F8 append is absent from inherited45D2 until late3768. Subsequent
//old-list visits execute3B3A/9298/A418 once; no draw-time behavior mutation.
Level1BonusPickupPrefixWork level1ArrivalPrefixOperations(const Level1TargetPrefixInput& input,
    const Level1TargetPrefixProfile& profile,const BitmapFont& scoreFont,
    const CockpitPageState& pages,std::uint16_t firstBlockCapacity,const Model3d& model42,
    const Level1BonusPickupContext& pickup,const Level1ArrivalPrefixContext& arrival);
} // namespace stellar7
