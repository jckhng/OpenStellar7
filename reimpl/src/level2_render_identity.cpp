#include "stellar7/gameplay.hpp"

#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::size_t constructorCount = 7;
constexpr std::size_t fixedKeyBase = 16;
constexpr std::size_t projectileKeyBase = 32;
constexpr std::size_t effectKeyBase = 40;
}

std::vector<std::size_t> GameplayModel::recoveredLevel2RenderKeys() const {
    const auto count=level2Entities_.size();
    if(activeCampaignLevel_!=1 || level1_.levelIndex_!=1 || count<constructorCount
        || level2EntityActorPoolSlots_.size()!=count || level2EntityFixedPoolSlots_.size()!=count
        || level2EntityProjectilePoolSlots_.size()!=count || level2EntityDeathEffectPoolSlots_.size()!=count) {
        throw std::invalid_argument("Level2 render keys require actual physical pool mappings");
    }
    std::vector<std::size_t> result;std::array<bool,48> seen{};
    for(std::size_t index=0;index<count;++index) {
        const auto ordinary=level2EntityActorPoolSlots_[index];const auto fixed=level2EntityFixedPoolSlots_[index];
        const auto projectile=level2EntityProjectilePoolSlots_[index];const auto effect=level2EntityDeathEffectPoolSlots_[index];
        const auto owners=static_cast<unsigned>(ordinary<level2ActorPoolSize_)+static_cast<unsigned>(fixed<level2FixedObjectPoolSize_)
            +static_cast<unsigned>(projectile<level2ProjectilePoolSize_)+static_cast<unsigned>(effect<level2DeathEffectPoolSize_);
        if(owners!=1) {throw std::invalid_argument("Level2 view has ambiguous physical identity");}
        const auto key=ordinary<level2ActorPoolSize_ ? ordinary : fixed<level2FixedObjectPoolSize_ ? fixedKeyBase+fixed
            : projectile<level2ProjectilePoolSize_ ? projectileKeyBase+projectile : effectKeyBase+effect;
        if(seen[key]) {throw std::invalid_argument("Level2 view aliases a physical pool slot");}
        seen[key]=true;result.push_back(key);
    }
    return result;
}

} // namespace stellar7
