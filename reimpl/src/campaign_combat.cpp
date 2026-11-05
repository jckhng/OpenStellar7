#include "stellar7/gameplay.hpp"
#include "campaign_guardian_children.hpp"
#include <algorithm>

namespace stellar7 {

bool GameplayModel::campaignCollides(const Level1Entity& left, const Level1Entity& right) noexcept {
    if (left.type == 7 || right.type == 7) {
        return false;
    }
    if (
        static_cast<std::int32_t>(left.collisionRadius) < 0
        || static_cast<std::int32_t>(right.collisionRadius) < 0
    ) {
        return false;
    }
    const auto threshold = static_cast<std::int64_t>(
        (left.collisionRadius + right.collisionRadius) >> 1U
    );
    const auto magnitude = [](const std::int64_t value) noexcept {
        return value < 0 ? -value : value;
    };
    return (
        magnitude(
            static_cast<std::int64_t>(left.position.x)
            - right.position.x
        ) <= threshold
        && magnitude(
            static_cast<std::int64_t>(left.position.y)
            - right.position.y
        ) <= threshold
        && magnitude(
            static_cast<std::int64_t>(left.position.z)
            - right.position.z
        ) <= threshold
    );
}

std::size_t GameplayModel::campaignEntityIdentity(const std::size_t index) const {
    const auto ordinary = level2EntityActorPoolSlots_[index];
    const auto fixed = level2EntityFixedPoolSlots_[index];
    const auto projectile = level2EntityProjectilePoolSlots_[index];
    if (ordinary < level2ActorPoolSize_) { return ordinary; }
    if (fixed < level2FixedObjectPoolSize_) { return level2ActorPoolSize_ * 3 + fixed; }
    if (projectile < level2ProjectilePoolSize_) { return level2ActorPoolSize_ + projectile; }
    return level2ActorPoolSize_ * 2 + level2EntityDeathEffectPoolSlots_[index];
}

std::size_t GameplayModel::campaignEntityIndex(const std::size_t identity) const {
    const auto kind = identity / level2ActorPoolSize_;
    const auto slot = identity % level2ActorPoolSize_;
    const auto& slots = kind == 0 ? level2EntityActorPoolSlots_ : kind == 1 ? level2EntityProjectilePoolSlots_
        : kind == 2 ? level2EntityDeathEffectPoolSlots_ : level2EntityFixedPoolSlots_;
    const auto found = std::find(slots.begin(), slots.end(), slot);
    return found == slots.end() ? static_cast<std::size_t>(-1)
        : static_cast<std::size_t>(found - slots.begin());
}

bool GameplayModel::applyCampaignHit(Level1Entity& shadow, const std::uint8_t amount,
    const std::size_t identity, const CampaignCombatCallbacks& callbacks,
    const std::optional<FixedPosition> effectPosition) {
    const auto wasAlive=shadow.stateBytes4dTo52[1]!=0;
    const auto hit = campaignActorHit(shadow, amount, static_cast<std::uint8_t>(activeCampaignLevel_));
    if(hit.lethal&&wasAlive) {recordDestruction(shadow,effectPosition.value_or(shadow.position),hit.eventId);}
    const auto specialRewardAllowed = postWaveObjectIndex() == static_cast<std::size_t>(-1)
        || shadow.collisionContactAllocationIndex != postWaveObjectIndex();
    shadow.stateBytes4dTo52[1] = hit.health;
    const auto target = identity == static_cast<std::size_t>(-1) ? identity : campaignEntityIndex(identity);
    if (target < level2Entities_.size()) {
        level2Entities_[target].stateBytes4dTo52[1] = hit.health;
    }
    auto effect = shadow;
    if (effectPosition) { effect.position = *effectPosition; }
    for (const auto action : hit.actions) {
        switch (action) {
        case CampaignActorHitAction::registerBonusDeath:
            registerLevel5DestroyedTypeBonus(shadow);
            break;
        case CampaignActorHitAction::effect:
            callbacks.effect(effect, static_cast<std::uint8_t>(hit.effectFirstFrame + 2), hit.effectTerminalFrame);
            break;
        case CampaignActorHitAction::retire: {
            const auto live = identity == static_cast<std::size_t>(-1) ? identity : campaignEntityIndex(identity);
            if (live < level2Entities_.size()) {
                level2Entities_[live].stateBytes4dTo52[1] = 0;
                callbacks.retire(live);
            }
            break;
        }
        case CampaignActorHitAction::decrementEnemyCount:
            --level2ActiveEnemyCount_;
            break;
        case CampaignActorHitAction::audio:
            level2AudioEvents_.push_back({level2LoopTick_, hit.eventId});
            break;
        case CampaignActorHitAction::guardianScore:
            level2Score_ += scaledCampaignAward(1000);
            break;
        case CampaignActorHitAction::sortObjects:
            for (std::size_t destination = 0; destination < level2ActiveListOrder_.size(); ++destination) {
                auto selected = destination;
                for (auto candidate = destination + 1; candidate < level2ActiveListOrder_.size(); ++candidate) {
                    if (level2Entities_[level2ActiveListOrder_[selected]].presenterDistance
                        < level2Entities_[level2ActiveListOrder_[candidate]].presenterDistance) {
                        selected = candidate;
                    }
                }
                std::swap(level2ActiveListOrder_[destination], level2ActiveListOrder_[selected]);
            }
            break;
        case CampaignActorHitAction::guardianReward:
            callbacks.reward(shadow, CampaignExit::greenReward);
            break;
        case CampaignActorHitAction::victory:
            enterPlayableGuardianVictory();
            break;
        case CampaignActorHitAction::deactivateLevel:
            break; //The retained victory owner clears the active level after7512.
        case CampaignActorHitAction::specialRewardIfEligible:
            if (specialRewardAllowed) { callbacks.reward(shadow, CampaignExit::blueWarp); }
            break;
        }
    }
    return hit.lethal;
}

} // namespace stellar7
