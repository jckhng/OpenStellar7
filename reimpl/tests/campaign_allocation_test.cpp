#include "campaign_allocation.hpp"
#include "campaign_pool_identity.hpp"
#include <memory>
#include <iostream>
#include <stdexcept>

namespace stellar7 {
namespace {
void require(const bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
}

// Declared synthetic pool boundaries; no preceding gameplay or DOS ancestry.
struct GameplayModelTestAccess {
    static void retirement() {
        constexpr auto noActor = GameplayModel::level2ActorPoolSize_;
        constexpr auto noProjectile = GameplayModel::level2ProjectilePoolSize_;
        constexpr auto noFixed = GameplayModel::level2FixedObjectPoolSize_;
        constexpr std::uint32_t cachedDistance = 111;
        constexpr std::uint16_t cachedHeading = 222;
        for (const auto continuation : {GameplayModel::Level2Continuation::frameEntry,
            GameplayModel::Level2Continuation::successReturn}) {
            auto model = std::make_unique<GameplayModel>();
            model->playableCampaignFrames_ = true;
            model->level2Entities_.assign(4, {});
            for (std::size_t index = 0; index < model->level2Entities_.size(); ++index) {
                model->level2Entities_[index].allocationIndex = index;
                model->level2Entities_[index].stateBytes4dTo52[1] = 1;
            }
            model->level2EntityActorPoolSlots_ = {0, noActor, 1, noActor};
            model->level2EntityProjectilePoolSlots_ = {noProjectile, 0, noProjectile, noProjectile};
            model->level2EntityFixedPoolSlots_ = {noFixed, noFixed, noFixed, 0};
            model->level2EntityDeathEffectPoolSlots_.assign(4, GameplayModel::level2DeathEffectPoolSize_);
            model->level2ActorPoolActive_.fill(false);
            model->level2ActorPoolActive_[0] = model->level2ActorPoolActive_[1] = true;
            model->level2ProjectilePoolActive_.fill(false);
            model->level2ProjectilePoolActive_[0] = true;
            model->level2AllocationListOrder_ = {2, 0, 3, 1};
            model->level2ObjectListOrder_ = model->level2ActiveListOrder_ = model->level2AllocationListOrder_;
            model->level2PostWaveObjectIndex_ = 0;
            model->level2CompletionObjectIndex_ = 2;
            model->level2PickupIndex_ = 3;
            auto& shot = model->level2Entities_[1];
            shot.type = 17;
            shot.position = {400, -700, 200};
            shot.presenterDistance = cachedDistance;
            shot.targetHeading = cachedHeading;
            shot.collisionContactAllocationIndex = 0;
            model->level2Entities_[2].collisionContactAllocationIndex = 3;
            constexpr auto retiredContact = GameplayModel::retiredFixedContactBase_ + 5;
            model->level2Entities_[3].collisionContactAllocationIndex = retiredContact;
            Level1Entity semantic{};
            semantic.allocationIndex = 3;
            model->level2RetiredEffectSemanticEntities_ = {semantic};
            Level1Entity target{};
            target.position = {-300, 900, 0};
            GameplayModel::CampaignRetirement retired{target, continuation};
            model->retireCampaignEntity(0, retired);
            require(model->level2AllocationListOrder_ == std::vector<std::size_t>({1, 0, 2}),
                "retirement lost swap-back allocation order");
            require(model->level2ObjectListOrder_ == model->level2AllocationListOrder_
                && model->level2ActiveListOrder_ == model->level2AllocationListOrder_, "retirement lists disagree");
            require(model->level2PostWaveObjectIndex_ == retiredGuardianIndex
                && model->level2CompletionObjectIndex_ == 1 && model->level2PickupIndex_ == 2,
                "retirement confused physical guardian and compact indices");
            require(!model->level2ActorPoolActive_[0] && model->level2ActorPool_[0].stateBytes4dTo52[1] == 1,
                "retirement replaced the caller-owned actor state");
            require(model->level2Entities_[0].collisionContactAllocationIndex == static_cast<std::size_t>(-1)
                && model->level2Entities_[1].collisionContactAllocationIndex == 2
                && model->level2Entities_[2].collisionContactAllocationIndex == retiredContact,
                "retirement remapped a contact in the wrong identity domain");
            require(model->level2RetiredEffectSemanticEntities_.front().allocationIndex == 2,
                "ordinary removal failed to shift a retired semantic index");
            const auto before = model->level2Entities_.front();
            model->retireCampaignEntity(0, retired);
            const auto& physical = model->level2ProjectilePool_[0];
            const bool refresh = continuation == GameplayModel::Level2Continuation::frameEntry;
            require(!model->level2ProjectilePoolActive_[0] && physical.stateBytes4dTo52[1] == 0
                && physical.position == before.position, "projectile retirement lost physical record state");
            require(physical.presenterDistance == (refresh ? approximatePlanarDistance(target.position, before.position) : cachedDistance)
                && physical.targetHeading == (refresh ? presenterTargetHeading(before.position, target.position) : cachedHeading),
                "projectile retirement refreshed presenter fields in the wrong continuation");
            require(retired.lastNonEffectRemoval == 0 && retired.effectCount == 0,
                "non-effect retirement changed effect-only rebuild bookkeeping");
        }
    }

    static void effects() {
        auto model = std::make_unique<GameplayModel>();
        model->level2Entities_.clear();
        model->level2EntityActorPoolSlots_.clear();
        model->level2EntityProjectilePoolSlots_.clear();
        model->level2EntityFixedPoolSlots_.clear();
        model->level2EntityDeathEffectPoolSlots_.clear();
        model->level2AllocationListOrder_.clear();
        model->level2DeathEffectPoolActive_.fill(true);
        model->level2DeathEffectPoolPendingPresenterRetirement_.fill(false);
        model->level2DeathEffectPoolActive_[0] = model->level2DeathEffectPoolActive_[1] = false;
        model->level2DeathEffectPoolPendingPresenterRetirement_[0] = true;
        model->level2DeathEffectPool_[1].yaw = 77;
        model->level2DeathEffectPool_[1].forwardVelocity = 13;
        Level1Entity source{};
        source.position = {100, 200, 300};
        require(model->spawnCampaignTransientEffect(source, 2, 12), "free effect record was not allocated");
        require(model->level2EntityDeathEffectPoolSlots_ == std::vector<std::size_t>({1})
            && model->level2DeathEffectPoolPendingPresenterRetirement_[0], "allocation reused a pending presenter record");
        const auto& effect = model->level2Entities_.front();
        require(effect.position == source.position && effect.yaw == 77 && effect.forwardVelocity == 13
            && effect.stateBytes4dTo52[1] == 2 && effect.stateBytes4dTo52[4] == 12,
            "effect allocation cleared inherited motion or changed its interval");
        const auto revision = model->level2AllocationRevision_;
        require(!model->spawnCampaignTransientEffect(source, 2, 12)
            && model->level2AllocationRevision_ == revision && model->level2Entities_.size() == 1,
            "full effect pool mutated allocation state");
        model->level2DeathEffectPoolPendingPresenterRetirement_[0] = false;
        require(model->spawnCampaignTransientEffect(source, 3, 13), "released presenter record stayed unavailable");
        Level1Entity semantic{};
        semantic.allocationIndex = 7;
        model->level2RetiredEffectSemanticEntities_ = {semantic};
        GameplayModel::CampaignRetirement retired{source, GameplayModel::Level2Continuation::frameEntry};
        model->retireCampaignEntity(0, retired);
        require(retired.effectCount == 1 && retired.lastNonEffectRemoval == static_cast<std::size_t>(-1)
            && model->level2RetiredEffectSemanticEntities_.front().allocationIndex == 7,
            "effect retirement collapsed an inherited presenter identity");
        require(!model->level2DeathEffectPoolActive_[1]
            && model->level2EntityDeathEffectPoolSlots_ == std::vector<std::size_t>({0}),
            "effect retirement changed the surviving physical record");
    }
};
} // namespace stellar7

int main() {
    try {
        stellar7::GameplayModelTestAccess::retirement();
        stellar7::GameplayModelTestAccess::effects();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
