#include "stellar7/gameplay.hpp"
#include "heap_animation.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {

void advanceHeapAnimation(Level1Entity& entity, const unsigned limit,const unsigned selectors) noexcept {
    auto& countdown = entity.stateBytes4dTo52[3];
    if (countdown == 0) {
        countdown = entity.stateBytes4dTo52[2];
        entity.modelAnimationVariant = static_cast<std::uint8_t>(
            (entity.modelAnimationVariant + 1U) % limit);
        for(std::size_t index=1;index<selectors&&index<retainedAnimationSelectorCapacity;++index) {
            auto& variant=entity.modelAnimationTail[index-1];variant=static_cast<std::uint8_t>((variant+1U)%limit);
        }
    } else {
        --countdown;
    }
}
void snapshotHeapAnimation(Level1Entity& entity) noexcept {
    entity.presentationModelAnimationVariant=entity.modelAnimationVariant;
    entity.presentationModelAnimationTail=entity.modelAnimationTail;
}

void GameplayModel::advanceLevel3Animation() noexcept {
    for (const auto index : level2ActiveListOrder_) {
        auto& actor = level2Entities_[index];
        if (actor.type == 24 && actor.animatedModelPresenterEnabled) {
            advanceHeapAnimation(actor, level3FixedAnimationLimit_);
        }
    }
}

void GameplayModel::finishLevel3Radar() {
    const auto playableLethalDisplay = playableCampaignFrames_
        && (activeCampaignLevel_ == 2 || activeCampaignLevel_ == 5)
        && pendingLevel2FailureTail();
    if ((phase_ != GameplayPhase::level3Native && phase_!=GameplayPhase::playable && !playableLethalDisplay)
        || level3RadarFinished_ || level2TailConsumed_) {
        throw std::invalid_argument("Level3 radar callback requires its pending completed main display");
    }
    advanceLevel3Animation(); //698A's B996 callback, even when main geometry was clipped.
    level3RadarFinished_ = true;
    if (playableCampaignFrames_) {
        syncPlayableCampaignPools();
        return;
    }
    for (std::size_t index = 0; index < level2Entities_.size(); ++index) {
        const auto slot = level2EntityFixedPoolSlots_[index];
        if (slot < level2FixedObjectPoolSize_) {
            level2FixedPool_[slot] = level2Entities_[index];
        }
    }
}

void GameplayModel::advanceLevel2GuardianAnimation() noexcept {
    // FUN_1000:130c advances the animation after each main/radar draw.
    //LEVEL2 44/45 use six/five variants; Level5 shared11 uses eight.
    //Keep the preceding radar callback on this side of the main boundary,
    //as in Level1State. Pixel presenters never execute this callback again.
    for (auto& entity : level2Entities_) {
        const auto limit = playableCampaignFrames_ && campaignModelsBound_ && entity.type>=0 && entity.type!=24
            && static_cast<std::size_t>(entity.type)<campaignModels_.size()
            ? campaignModels_[entity.type].animationLimit
            : activeCampaignLevel_ == 4 && entity.type == 11 ? type11AnimationLimit
            : activeCampaignLevel_ == 4 && entity.type == 44 ? level5GuardianAnimationLimit_
            : activeCampaignLevel_ == 1 && entity.type == 44 ? 6U
            : activeCampaignLevel_ == 1 && entity.type == 45 ? 5U : 0U;
        if (!entity.animatedModelPresenterEnabled || limit == 0) {
            continue;
        }
        if (playableCampaignFrames_ && entity.renderProfile != WorldActorRenderProfile::heapSelectorDeferredAnimation) {
            continue;
        }
        if (activeCampaignLevel_ == 4 && std::find(level2ActiveListOrder_.begin(),
            level2ActiveListOrder_.end(), entity.allocationIndex) == level2ActiveListOrder_.end()) {
            continue;
        }
        if (entity.renderProfile != WorldActorRenderProfile::heapSelectorDeferredAnimation) {
            entity.renderProfile = WorldActorRenderProfile::unknown;
        }
        const auto selectors=playableCampaignFrames_&&campaignModelsBound_&&entity.type>=0
            &&static_cast<std::size_t>(entity.type)<campaignModels_.size()?campaignModels_[entity.type].selectors:1;
        advanceHeapAnimation(entity, limit,selectors);
    }
}

const std::vector<Level1Entity>&
GameplayModel::level2PresentationEntities() const noexcept {
    return level2PresentationEntities_;
}

const std::vector<std::size_t>&
GameplayModel::level2PresentationActiveListOrder() const noexcept {
    return level2PresentationActiveListOrder_;
}

std::optional<std::size_t> GameplayModel::level2PresentationPoolSlot(const std::size_t index) const noexcept {
    if (level2PresentationAllocationRevision_ != level2NonProjectileAllocationRevision_) {
        return std::nullopt;
    }
    return index < level2PresentationPoolSlots_.size() ? level2PresentationPoolSlots_[index] : std::nullopt;
}

std::optional<std::uint64_t> GameplayModel::level2PresentationActorIncarnation(const std::size_t index) const noexcept {
    if (level2PresentationAllocationRevision_ != level2NonProjectileAllocationRevision_) {
        return std::nullopt;
    }
    return index < level2PresentationActorIncarnations_.size()
        ? level2PresentationActorIncarnations_[index] : std::nullopt;
}

std::vector<Level1Entity> GameplayModel::level2SemanticEntities() const {
    auto entities = level2Entities_;
    auto retired = level2RetiredEffectSemanticEntities_;
    std::sort(
        retired.begin(),
        retired.end(),
        [](const Level1Entity& left, const Level1Entity& right) {
            return left.allocationIndex < right.allocationIndex;
        }
    );
    for (const auto& item : retired) {
        const auto insertion = std::min(
            item.allocationIndex,
            entities.size()
        );
        entities.insert(
            entities.begin() + static_cast<std::ptrdiff_t>(insertion),
            item
        );
    }
    for (std::size_t index = 0; index < entities.size(); ++index) {
        entities[index].allocationIndex = index;
    }
    return entities;
}

std::vector<std::size_t> GameplayModel::level2SemanticActiveListOrder() const {
    if (!level2RetiredEffectSemanticEntities_.empty()) {
        const auto semanticEntities = level2SemanticEntities();
        const auto& boundaryStartOrder =
            level2RetiredEffectSemanticActiveOrder_;
        if (
            boundaryStartOrder.size() == semanticEntities.size()
            && level2EffectSemanticActivePositionOverrides_.empty()
            && std::all_of(
                boundaryStartOrder.begin(),
                boundaryStartOrder.end(),
                [&semanticEntities](const std::size_t index) {
                    return index < semanticEntities.size();
                }
            )
        ) {
            if (level2RetiredEffectSemanticOrderRebuilt_) {
                return boundaryStartOrder;
            }
            // FUN_1000:14b0 sorts the inherited presenter list before
            // FUN_1000:0ac8 retires a terminal effect. The post-callback
            // allocation list has already been rebuilt and is not the
            // source of this one-boundary semantic list. Natural Level
            // 3 loop 3294 exposes the distinction through three equal
            // distance Type-47 identities. Equal counts alone do not
            // prove unchanged identities: a nonlethal hit can replace
            // a shot with an effect while another effect retires (Level
            // 2 loop 9894). Those remaps use the fallback below.
            auto order = boundaryStartOrder;
            const auto semanticDistance = [&semanticEntities](
                const std::size_t index
            ) {
                return semanticEntities[index].type == 46
                    ? std::uint32_t{0x7000}
                    : semanticEntities[index].presenterDistance;
            };
            for (std::size_t destination = 0;
                 destination < order.size(); ++destination) {
                auto selected = destination;
                for (std::size_t candidate = destination + 1;
                     candidate < order.size(); ++candidate) {
                    if (
                        semanticDistance(order[selected])
                        < semanticDistance(order[candidate])
                    ) {
                        selected = candidate;
                    }
                }
                if (selected != destination) {
                    std::swap(order[destination], order[selected]);
                }
            }
            return order;
        }
        auto order = level2ActiveListOrder_;
        const auto rawDistance = [this](const std::size_t index) {
            return level2Entities_[index].type == 46
                ? std::uint32_t{0x7000}
                : level2Entities_[index].presenterDistance;
        };
        auto rawOrderIsDistanceSorted = true;
        for (std::size_t index = 1; index < order.size(); ++index) {
            rawOrderIsDistanceSorted = rawOrderIsDistanceSorted
                && rawDistance(order[index - 1])
                    >= rawDistance(order[index]);
        }
        std::vector<std::size_t> retiredOrder(
            level2RetiredEffectSemanticEntities_.size()
        );
        for (std::size_t index = 0; index < retiredOrder.size(); ++index) {
            retiredOrder[index] = index;
        }
        std::sort(
            retiredOrder.begin(),
            retiredOrder.end(),
            [this](const std::size_t left, const std::size_t right) {
                return level2RetiredEffectSemanticEntities_[left]
                        .allocationIndex
                    < level2RetiredEffectSemanticEntities_[right]
                        .allocationIndex;
            }
        );
        for (const auto retiredIndex : retiredOrder) {
            const auto allocationIndex =
                level2RetiredEffectSemanticEntities_[retiredIndex]
                    .allocationIndex;
            for (auto& index : order) {
                if (index >= allocationIndex) {
                    ++index;
                }
            }
            const auto requestedPosition = retiredIndex
                    < level2RetiredEffectSemanticActivePositions_.size()
                ? level2RetiredEffectSemanticActivePositions_[
                    retiredIndex
                ]
                : order.size();
            order.insert(
                order.begin() + static_cast<std::ptrdiff_t>(
                    std::min(requestedPosition, order.size())
                ),
                allocationIndex
            );
        }
        for (const auto& override :
             level2EffectSemanticActivePositionOverrides_) {
            const auto slot = std::find(
                level2EntityDeathEffectPoolSlots_.begin(),
                level2EntityDeathEffectPoolSlots_.end(),
                override.first
            );
            if (
                slot
                == level2EntityDeathEffectPoolSlots_.end()
            ) {
                continue;
            }
            auto semanticIndex = static_cast<std::size_t>(
                slot - level2EntityDeathEffectPoolSlots_.begin()
            );
            for (const auto retiredIndex : retiredOrder) {
                if (
                    level2RetiredEffectSemanticEntities_[
                        retiredIndex
                    ].allocationIndex <= semanticIndex
                ) {
                    ++semanticIndex;
                }
            }
            const auto current = std::find(
                order.begin(),
                order.end(),
                semanticIndex
            );
            if (current == order.end()) {
                continue;
            }
            order.erase(current);
            order.insert(
                order.begin() + static_cast<std::ptrdiff_t>(
                    std::min(override.second, order.size())
                ),
                semanticIndex
            );
        }
        if (rawOrderIsDistanceSorted) {
            // A presenter-retired effect does not suppress the native
            // FUN_1000:14b0 distance sort. Include its one-boundary
            // semantic pointer in that same stable selection sort.
            const auto entities = level2SemanticEntities();
            const auto semanticDistance = [&entities](
                const std::size_t index
            ) {
                return entities[index].type == 46
                    ? std::uint32_t{0x7000}
                    : entities[index].presenterDistance;
            };
            for (std::size_t destination = 0;
                 destination < order.size(); ++destination) {
                auto selected = destination;
                for (std::size_t candidate = destination + 1;
                     candidate < order.size(); ++candidate) {
                    if (
                        semanticDistance(order[selected])
                        < semanticDistance(order[candidate])
                    ) {
                        selected = candidate;
                    }
                }
                if (selected != destination) {
                    std::swap(order[destination], order[selected]);
                }
            }
        }
        return order;
    }
    if (level2RetiredEffectRebuildReported_) {
        return level2AllocationListOrder_;
    }
    return level2ActiveListOrder_;
}

const CompletionBeamPresentation* GameplayModel::level2CompletionBeam(const std::size_t index) const noexcept {
    return index < level2BeamPresentations_.size() && level2BeamPresentations_[index]
        ? &*level2BeamPresentations_[index] : nullptr;
}

void GameplayModel::snapshotLevel2Presentation(std::vector<std::optional<CompletionBeamPresentation>> beams) {
    //Live membership writes back to its physical fixed record at the frame boundary.
    for (std::size_t index = 0; index < level2Entities_.size(); ++index) {
        if (index < level2EntityFixedPoolSlots_.size()) {
            const auto slot = level2EntityFixedPoolSlots_[index];
            if (slot < level2FixedObjectPoolSize_) {
                level2FixedPool_[slot] = level2Entities_[index];
                level2FixedPoolKnown_[slot] = true;
            }
        }
    }
    level2PresentationAllocationRevision_ = level2NonProjectileAllocationRevision_;
    level2BeamPresentations_ = std::move(beams);
    level2BeamPresentations_.resize(level2Entities_.size());
    level2PresentationEntities_ = level2Entities_;
    level2PresentationActiveListOrder_ = level2ActiveListOrder_;
    if(playableCampaignFrames_) {
        playableSystemOldActorSlots_.clear();
        for(const auto index:level2ActiveListOrder_) {
            const auto slot=level2EntityActorPoolSlots_[index];
            if(slot<level2ActorPoolSize_) {playableSystemOldActorSlots_.push_back(slot);}
        }
    }
    level2PresentationPoolSlots_.clear();
    level2PresentationActorIncarnations_.clear();
    for (std::size_t index = 0; index < level2Entities_.size(); ++index) {
        const auto slot = liveLevel2PoolSlot(index);
        level2PresentationPoolSlots_.push_back(slot);
        level2PresentationActorIncarnations_.push_back(slot && *slot < level2ActorPoolSize_
            ? std::optional<std::uint64_t>{level2ActorIncarnations_[*slot]} : std::nullopt);
    }
    std::vector<std::size_t> retiredOrder;
    for (std::size_t index = 0; index < level2RetiredEffectSemanticEntities_.size(); ++index) {
        retiredOrder.push_back(index);
    }
    std::sort(retiredOrder.begin(), retiredOrder.end(), [this](const auto left, const auto right) {
        return level2RetiredEffectSemanticEntities_[left].allocationIndex
            < level2RetiredEffectSemanticEntities_[right].allocationIndex;
    });
    for (const auto index : retiredOrder) {
        const auto insertion = std::min(level2RetiredEffectSemanticEntities_[index].allocationIndex,
            level2PresentationEntities_.size());
        level2PresentationEntities_.insert(level2PresentationEntities_.begin() + insertion,
            level2PendingTerminalEffectPresentations_.at(index));
        level2BeamPresentations_.insert(level2BeamPresentations_.begin() + insertion, std::nullopt);
        level2PresentationActorIncarnations_.insert(level2PresentationActorIncarnations_.begin() + insertion,
            std::nullopt);
        level2PresentationPoolSlots_.insert(level2PresentationPoolSlots_.begin() + insertion,
            level2ActorPoolSize_ + level2FixedObjectPoolSize_ + level2ProjectilePoolSize_
                + level2PendingTerminalEffectSlots_.at(index));
    }
    if (!retiredOrder.empty()) {
        for (std::size_t index = 0; index < level2PresentationEntities_.size(); ++index) {
            level2PresentationEntities_[index].allocationIndex = index;
        }
        level2PresentationActiveListOrder_ = level2SemanticActiveListOrder();
    }
}

std::optional<std::size_t> GameplayModel::liveLevel2PoolSlot(const std::size_t index) const noexcept {
    if (index >= level2Entities_.size()
        || index >= level2EntityActorPoolSlots_.size() || index >= level2EntityProjectilePoolSlots_.size()
        || index >= level2EntityDeathEffectPoolSlots_.size()) {
        return std::nullopt;
    }
    const auto actor = level2EntityActorPoolSlots_[index];
    if (actor < level2ActorPoolSize_) {
        return actor;
    }
    constexpr auto projectileBase = level2ActorPoolSize_ + level2FixedObjectPoolSize_;
    const auto projectile = level2EntityProjectilePoolSlots_[index];
    if (projectile < level2ProjectilePoolSize_) {
        return projectileBase + projectile;
    }
    const auto effect = level2EntityDeathEffectPoolSlots_[index];
    if (effect < level2DeathEffectPoolSize_) {
        return projectileBase + level2ProjectilePoolSize_ + effect;
    }
    if (playableCampaignFrames_ && index < level2EntityFixedPoolSlots_.size()) {
        const auto fixed = level2EntityFixedPoolSlots_[index];
        const auto& record = level2Entities_[index];
        const bool fixedCategory = record.stateBytes4dTo52[0] == 0xfe
            || (record.type == 24 && record.stateBytes4dTo52[0] == 0xfc);
        if (fixedCategory && fixed < level2FixedObjectPoolSize_ && level2FixedPoolKnown_[fixed]) {
            return level2ActorPoolSize_ + fixed;
        }
    }
    if (activeCampaignLevel_ == 2 && index < level2EntityFixedPoolSlots_.size()) {
        const auto& record = level2Entities_[index];
        const auto fixed = level2EntityFixedPoolSlots_[index];
        const bool animated = record.type == 24 && record.stateBytes4dTo52[0] == 0xfc
            && record.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation;
        const bool companion = record.type == 31 && record.stateBytes4dTo52[0] == 0xfe
            && record.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback;
        if ((animated || companion) && fixed < level2FixedObjectPoolSize_ && level2FixedPoolKnown_[fixed]) {
            return level2ActorPoolSize_ + fixed;
        }
    }
    if ((activeCampaignLevel_ != 1 && !(activeCampaignLevel_ == 4
            && (level2Entities_[index].type == 42 || level2Entities_[index].type == 25)))
        || index >= level2EntityFixedPoolSlots_.size()
        || level2Entities_[index].stateBytes4dTo52[0] != 0xfe) {
        return std::nullopt;
    }
    const auto fixed = level2EntityFixedPoolSlots_[index];
    if (fixed >= level2FixedObjectPoolSize_) {
        return std::nullopt;
    }
    return level2ActorPoolSize_ + fixed;
}

} // namespace stellar7
