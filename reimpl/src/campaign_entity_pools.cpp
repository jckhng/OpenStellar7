#include "stellar7/gameplay.hpp"
#include "campaign_pool_identity.hpp"

#include <algorithm>

namespace stellar7 {

void GameplayModel::initializeCampaignPools(const std::size_t level,
    const std::size_t actorPoolMembers) noexcept {
    fuelBay_.fixedSlot.reset(); //49B9 clears503C;0086/0088/008A retain their own state.
    damageFlashAge_ = 0;
    damageFlashActive_ = false;
    level2EntryPaletteActive_ = true;
    activeCampaignLevel_ = level;
    failureFromLevel2_ = false;
    level2FailureTailFinished_ = false;
    level1FailureTailFinished_ = false;
    level2LoopTick_ = 0;
    level2TailConsumed_ = false;
    level3FixedAnimationLimit_ = 0;
    level3RadarFinished_ = false;
    level5SuccessContinuation_.reset();
    level2SuccessContinuation_.reset();
    level2SecondaryPickupKnownNull_ = true; //48D4 clears source1290 independently of1292/1294.
    level5TransientStateKnownInactive_ = false;
    campaignExitFadeTicks_ = 0;
    campaignSuccess_ = false;
    level2ArmorWarningActive_ = false;
    level2AudioEvents_.clear();
    level2AllocationRevision_ = 0;
    level2NonProjectileAllocationRevision_ = 0;
    level2FixedAllocationRevision_ = 0;
    alternateSightPhase_ = true;
    level2ActiveListOrder_.resize(level2Entities_.size());
    for (std::size_t index = 0; index < level2Entities_.size(); ++index) {
        level2ActiveListOrder_[index] = index;
    }
    level2AllocationListOrder_ = level2ActiveListOrder_;
    level2Visibility_.allocationSucceeded();
    level2ObjectListOrder_ = level2ActiveListOrder_;

    level2PendingTerminalEffectPresentations_.clear();
    level2PendingTerminalEffectSlots_.clear();
    level2RetiredEffectSemanticEntities_.clear();
    level2RetiredEffectSemanticActiveOrder_.clear();
    level2RetiredEffectSemanticOrderRebuilt_ = false;
    level2RetiredEffectSemanticActivePositions_.clear();
    level2EffectSemanticActivePositionOverrides_.clear();
    level2RetiredEffectRebuildReported_ = false;

    level2ActorPool_ = {};
    level2ActorPoolActive_.fill(false);
    level2ActorIncarnations_.fill(0);
    level2EntityActorPoolSlots_.assign(
        level2Entities_.size(),
        level2ActorPoolSize_
    );
    level2EntityFixedPoolSlots_.assign(level2Entities_.size(), level2FixedObjectPoolSize_);
    for (std::size_t slot = 0; slot < level2ResetFixedObjectCount_; ++slot) {
        level2FixedPool_[slot] = {};
        level2FixedPool_[slot].stateBytes4dTo52[0] = 0xfe;
        level2FixedPool_[slot].renderProfile = WorldActorRenderProfile::noHandles;
        level2FixedPool_[slot].animatedModelPresenterEnabled = false;
        level2FixedPoolKnown_[slot] = true;
        level2FixedPoolEverUsed_[slot] = false;
    }
    for (std::size_t index = actorPoolMembers; index < level2Entities_.size(); ++index) {
        const auto slot = index - actorPoolMembers;
        if (slot < level2FixedObjectPoolSize_) {
            level2EntityFixedPoolSlots_[index] = slot;
            level2FixedPoolEverUsed_[slot] = true;
            level2FixedPool_[slot] = level2Entities_[index];
            level2FixedPoolKnown_[slot] = true;
        }
    }
    for (std::size_t index = 0; index < actorPoolMembers; ++index) {
        level2ActorPool_[index] = level2Entities_[index];
        level2ActorPoolActive_[index] = true;
        level2ActorIncarnations_[index] = 1;
        level2EntityActorPoolSlots_[index] = index;
    }
    level2ProjectilePool_ = {};
    level2ProjectilePoolActive_.fill(false);
    level2ProjectilePoolWasActiveAtBoundaryStart_.fill(false);
    level2EntityProjectilePoolSlots_.assign(
        level2Entities_.size(),
        level2ProjectilePoolSize_
    );
    level2DeathEffectPool_ = {};
    level2DeathEffectPoolActive_.fill(false);
    level2DeathEffectPoolPendingPresenterRetirement_.fill(false);
    level2EntityDeathEffectPoolSlots_.assign(
        level2Entities_.size(),
        level2DeathEffectPoolSize_
    );

    snapshotLevel2Presentation();

}

void GameplayModel::syncPlayableCampaignPools() {
    for (std::size_t index = 0; index < level2Entities_.size(); ++index) {
        const auto& actor = level2Entities_[index];
        const auto ordinary = level2EntityActorPoolSlots_[index];
        const auto fixed = level2EntityFixedPoolSlots_[index];
        const auto projectile = level2EntityProjectilePoolSlots_[index];
        const auto effect = level2EntityDeathEffectPoolSlots_[index];
        if (ordinary < level2ActorPoolSize_) { level2ActorPool_[ordinary] = actor; }
        if (fixed < level2FixedObjectPoolSize_) { level2FixedPool_[fixed] = actor; }
        if (projectile < level2ProjectilePoolSize_) { level2ProjectilePool_[projectile] = actor; }
        if (effect < level2DeathEffectPoolSize_) { level2DeathEffectPool_[effect] = actor; }
    }
}

std::size_t GameplayModel::allocateLevel2Actor(
    Level1Entity entity, const ActorInitialization initialization
) noexcept {
    if (playableCampaignFrames_ && campaignModelsBound_ && entity.type>=0
        && static_cast<std::size_t>(entity.type)<campaignModels_.size()) {
        const auto& metadata=campaignModels_[entity.type];
        //AE10 retains4370(13)'s collision radius when rebinding visual type47/48.
        entity.collisionRadius = entity.type == 47 || entity.type == 48 ? campaignModels_[13].radius : metadata.radius;
        //4370 ordinary constructors own their selector recipe independently
        //of the model's count. Local AE10/477A births attach their own selector.
        if (entity.type>=44 && metadata.selectors==0) {
            entity.renderProfile=WorldActorRenderProfile::noHandles;
            entity.animatedModelPresenterEnabled=false;
        } else if (entity.type>=44 && (metadata.selectors==1
            || (activeCampaignLevel_==0 && entity.type==44 && metadata.selectors==3 && metadata.animationLimit==8))) {
            const auto deferred=entity.type==44 || entity.type==45;
            entity.renderProfile=deferred ? WorldActorRenderProfile::heapSelectorDeferredAnimation
                : WorldActorRenderProfile::inlineSelectorNoCallback;
            entity.animatedModelPresenterEnabled=deferred;
        }
    }
    if (playableCampaignFrames_ && (entity.type == 47 || entity.type == 48)) {
        entity.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
        entity.animatedModelPresenterEnabled = false;
    }
    //4370/49f4 clears the record;1/4/8/13 install neither handle nor callback.
    if (initialization == ActorInitialization::legacy && (!playableCampaignFrames_ || !campaignModelsBound_)) {
        entity.renderProfile = activeCampaignLevel_ == 1
            && (entity.type == 1 || entity.type == 4 || entity.type == 8 || entity.type == 13)
            && entity.flags == 0 && entity.stateBytes4dTo52[0] == 0xff
            ? WorldActorRenderProfile::noHandles : WorldActorRenderProfile::unknown;
    }
    const auto actorSlot = std::find(
        level2ActorPoolActive_.begin(),
        level2ActorPoolActive_.end(),
        false
    );
    if (actorSlot == level2ActorPoolActive_.end()) {
        return static_cast<std::size_t>(-1);
    }
    if (entity.type == 26) {
        level2SecondaryPickupKnownNull_ = false;
    }
    level2PrefixGuards_.reset();
    const auto actorPoolIndex = static_cast<std::size_t>(
        actorSlot - level2ActorPoolActive_.begin()
    );
    auto insertionIndex = level2Entities_.size();
    for (std::size_t index = 0; index < level2Entities_.size(); ++index) {
        const auto occupiedActorSlot = level2EntityActorPoolSlots_[index];
        if (
            level2Entities_[index].type == 30
            || occupiedActorSlot >= level2ActorPoolSize_
            || occupiedActorSlot > actorPoolIndex
        ) {
            insertionIndex = index;
            break;
        }
    }
    for (auto& index : level2AllocationListOrder_) {
        if (index >= insertionIndex) {
            ++index;
        }
    }
    for (auto& index : level2ObjectListOrder_) {
        if (index >= insertionIndex) {
            ++index;
        }
    }
    if (initialization == ActorInitialization::validatedOrdinary) {
        for (auto& index : level2ActiveListOrder_) {
            if (index >= insertionIndex) {
                ++index;
            }
        }
    }
    entity.allocationIndex = insertionIndex;
    ++level2AllocationRevision_;
    level2RetiredEffectSemanticOrderRebuilt_ = false;
    ++level2NonProjectileAllocationRevision_;
    level2Entities_.insert(
        level2Entities_.begin()
            + static_cast<std::ptrdiff_t>(insertionIndex),
        entity
    );
    level2EntityActorPoolSlots_.insert(
        level2EntityActorPoolSlots_.begin()
            + static_cast<std::ptrdiff_t>(insertionIndex),
        actorPoolIndex
    );
    insertLevel2FixedPoolSlot(insertionIndex, level2FixedObjectPoolSize_);
    level2EntityProjectilePoolSlots_.insert(
        level2EntityProjectilePoolSlots_.begin()
            + static_cast<std::ptrdiff_t>(insertionIndex),
        level2ProjectilePoolSize_
    );
    level2EntityDeathEffectPoolSlots_.insert(
        level2EntityDeathEffectPoolSlots_.begin()
            + static_cast<std::ptrdiff_t>(insertionIndex),
        level2DeathEffectPoolSize_
    );
    for (std::size_t index = insertionIndex + 1;
         index < level2Entities_.size(); ++index) {
        level2Entities_[index].allocationIndex = index;
    }
    level2ActorPool_[actorPoolIndex] = entity;
    level2ActorPoolActive_[actorPoolIndex] = true;
    ++level2ActorIncarnations_[actorPoolIndex];
    level2AllocationListOrder_.push_back(insertionIndex);
    level2ObjectListOrder_ = level2AllocationListOrder_;
    if (initialization == ActorInitialization::legacy && !playableCampaignFrames_) {
        level2ActiveListOrder_ = level2AllocationListOrder_;
    } else {
        level2Visibility_.allocationSucceeded();
    }
    return insertionIndex;
}

bool GameplayModel::hasPhysicalPostWaveRole() const noexcept {
    return level2PostWaveObjectIndex_ >= postWaveActorKeyBase_
        && level2PostWaveObjectIndex_ < postWaveActorKeyBase_ + level2ActorPoolSize_;
}

void GameplayModel::retainPhysicalPostWaveRole() noexcept {
    if (!playableCampaignFrames_ || activeCampaignLevel_ != 1 || !campaignModelsBound_
        || level2PostWaveObjectIndex_ >= level2Entities_.size()) {
        return;
    }
    const auto slot = level2EntityActorPoolSlots_[level2PostWaveObjectIndex_];
    if (slot < level2ActorPoolSize_) {
        level2PostWaveObjectIndex_ = postWaveActorKeyBase_ + slot;
    }
}

std::size_t GameplayModel::postWaveObjectIndex() const noexcept {
    if (!hasPhysicalPostWaveRole()) {
        return level2PostWaveObjectIndex_;
    }
    const auto slot = level2PostWaveObjectIndex_ - postWaveActorKeyBase_;
    const auto found = std::find(level2EntityActorPoolSlots_.begin(), level2EntityActorPoolSlots_.end(), slot);
    return found == level2EntityActorPoolSlots_.end() ? retiredGuardianIndex
        : static_cast<std::size_t>(found - level2EntityActorPoolSlots_.begin());
}

void GameplayModel::insertLevel2FixedPoolSlot(const std::size_t index, const std::size_t slot) noexcept {
    if (playableCampaignFrames_) {
        //All physical pools share the compact view; a birth can precede a reward.
        for (auto* pointer:{&level2CompletionObjectIndex_,&level2PostWaveObjectIndex_,&level2PickupIndex_}) {
            if (*pointer>=index && *pointer<postWaveActorKeyBase_) {++*pointer;}
        }
    }
    level2EntityFixedPoolSlots_.resize(level2Entities_.size() - 1, level2FixedObjectPoolSize_);
    level2EntityFixedPoolSlots_.insert(level2EntityFixedPoolSlots_.begin() + index, slot);
}

std::size_t GameplayModel::allocateLevel2FixedObject(
    Level1Entity entity
) noexcept {
    const auto retainedFixed = activeCampaignLevel_ == 4 || (playableCampaignFrames_ && (entity.type==42 || entity.type==43))
        || (playableCampaignFrames_ && campaignModelsBound_ && activeCampaignLevel_ == 1 && entity.type == 46)
        || (playableCampaignFrames_ && (activeCampaignLevel_ == 0 || activeCampaignLevel_ == 3 || activeCampaignLevel_ == 5 || activeCampaignLevel_ == 6));
    entity.renderProfile = WorldActorRenderProfile::unknown;
    // FUN_1000:4506 owns a separate 16-record pool located after the actor
    // pool and before projectile/effect records.  Its allocation appends the
    // new pointer to 0x49f8, but the semantic allocation index follows the
    // pool record's physical position.
    if (!retainedFixed) {
        const auto activeFixedObjects = static_cast<std::size_t>(std::count_if(
            level2Entities_.begin(), level2Entities_.end(), [](const Level1Entity& candidate) noexcept {
                return candidate.type == 30 || candidate.type == 46;
            }));
        if (activeFixedObjects >= level2FixedObjectPoolSize_) {
            return static_cast<std::size_t>(-1);
        }
    }
    auto fixedSlot = level2FixedObjectPoolSize_;
    if (retainedFixed) {
        for (std::size_t slot = 0; slot < level2FixedObjectPoolSize_; ++slot) {
            if (!level2FixedPoolKnown_[slot]) {
                return static_cast<std::size_t>(-1);
            }
            if (!level2FixedPool_[slot].active()) {
                fixedSlot = slot;
                break;
            }
        }
    } else {
        std::array<bool, level2FixedObjectPoolSize_> occupied{};
        for (const auto slot : level2EntityFixedPoolSlots_) {
            if (slot < occupied.size()) {
                occupied[slot] = true;
            }
        }
        const auto available = std::find(occupied.begin(), occupied.end(), false);
        fixedSlot = static_cast<std::size_t>(available - occupied.begin());
    }
    if (fixedSlot == level2FixedObjectPoolSize_) {
        return static_cast<std::size_t>(-1);
    }
    if (retainedFixed && (entity.type == 42 || entity.type == 25 || entity.type == 26 || entity.type == 46)) {
        //4506 leaves all unspecified fields in the physical fixed record.
        const auto type = entity.type;
        const auto radius = entity.collisionRadius;
        entity = level2FixedPool_[fixedSlot];
        entity.type = type;
        entity.collisionRadius = radius;
        entity.flags = 0;
        entity.position.z = 0;
        entity.pitch = entity.roll = entity.yaw = 0;
        entity.stateBytes4dTo52[0] = 0xfe;
        entity.stateBytes4dTo52[1] = 100;
        entity.stateBytes4dTo52[4] = 0;
    }
    if (playableCampaignFrames_ && (entity.type == 25 || entity.type == 26)) {
        entity.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
        entity.animatedModelPresenterEnabled = false;
    }
    if (playableCampaignFrames_ && entity.type == 46 && level2FixedPoolKnown_[fixedSlot]) {
        //4506 retains the selected record's handle and callback on every reuse.
        entity.renderProfile = level2FixedPool_[fixedSlot].renderProfile;
        entity.animatedModelPresenterEnabled = level2FixedPool_[fixedSlot].animatedModelPresenterEnabled;
    }
    //48d4 clears eight fixed records;4506 scans sixteen and only partially initializes.
    constexpr std::size_t resetFixedCount = 8;
    if (activeCampaignLevel_ == 1 && (entity.type == 46 || (entity.type == 25 && !playableCampaignFrames_)) && fixedSlot < resetFixedCount
        && !level2FixedPoolEverUsed_[fixedSlot]) {
        entity.renderProfile = WorldActorRenderProfile::noHandles;
    }
    level2FixedPoolEverUsed_[fixedSlot] = true;
    if (entity.type == 26) {
        level2SecondaryPickupKnownNull_ = false;
    }
    level2PrefixGuards_.reset();
    auto insertionIndex = level2Entities_.size();
    for (std::size_t index = 0; index < level2Entities_.size(); ++index) {
        if (
            level2EntityProjectilePoolSlots_[index]
                < level2ProjectilePoolSize_
            || level2EntityDeathEffectPoolSlots_[index]
                < level2DeathEffectPoolSize_
        ) {
            insertionIndex = index;
            break;
        }
    }
    for (auto& index : level2AllocationListOrder_) {
        if (index >= insertionIndex) {
            ++index;
        }
    }
    for (auto& index : level2ObjectListOrder_) {
        if (index >= insertionIndex) {
            ++index;
        }
    }
    if (retainedFixed) {
        for (auto& index : level2ActiveListOrder_) {
            if (index >= insertionIndex) {
                ++index;
            }
        }
    }
    entity.allocationIndex = insertionIndex;
    ++level2AllocationRevision_;
    level2RetiredEffectSemanticOrderRebuilt_ = false;
    ++level2NonProjectileAllocationRevision_;
    ++level2FixedAllocationRevision_;
    level2FixedPool_[fixedSlot] = entity;
    level2FixedPoolKnown_[fixedSlot] = true;
    level2Entities_.insert(
        level2Entities_.begin()
            + static_cast<std::ptrdiff_t>(insertionIndex),
        entity
    );
    level2EntityActorPoolSlots_.insert(
        level2EntityActorPoolSlots_.begin()
            + static_cast<std::ptrdiff_t>(insertionIndex),
        level2ActorPoolSize_
    );
    insertLevel2FixedPoolSlot(insertionIndex, fixedSlot);
    level2EntityProjectilePoolSlots_.insert(
        level2EntityProjectilePoolSlots_.begin()
            + static_cast<std::ptrdiff_t>(insertionIndex),
        level2ProjectilePoolSize_
    );
    level2EntityDeathEffectPoolSlots_.insert(
        level2EntityDeathEffectPoolSlots_.begin()
            + static_cast<std::ptrdiff_t>(insertionIndex),
        level2DeathEffectPoolSize_
    );
    for (std::size_t index = insertionIndex + 1;
         index < level2Entities_.size(); ++index) {
        level2Entities_[index].allocationIndex = index;
    }
    level2AllocationListOrder_.push_back(insertionIndex);
    level2ObjectListOrder_ = level2AllocationListOrder_;
    if (retainedFixed) {
        level2Visibility_.allocationSucceeded();
    } else {
        level2ActiveListOrder_ = level2AllocationListOrder_;
    }
    return insertionIndex;
}

} // namespace stellar7
