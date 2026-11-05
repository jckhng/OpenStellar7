#include "stellar7/gameplay.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
bool hasLevel2PrefixInput(const GameplayInput& input) {
    return input.left || input.right || input.forward || input.reverse
        || input.fire || input.exit;
}
} // namespace

void GameplayModel::prepareFirstLevel2SourceFrame(const GameplayInput& input, Level2FrameRenderer* renderer) {
    constexpr std::size_t level2CampaignIndex = 1;
    constexpr std::int16_t initialEnemies = 5;
    constexpr std::int16_t warningArmor = 200;
    if (renderer == nullptr || renderer->prefixExecution() != Level2PrefixExecution::source
        || activeCampaignLevel_ != level2CampaignIndex || level2HasAdvanced_ || level2TailConsumed_ || level2LoopTick_ != 0
        || hasLevel2PrefixInput(input) || hasLevel2PrefixInput(level2PendingInput_) || !level2PrefixGuards_
        || !level2ActiveListOrder_.empty() || !level2ObjectListOrder_.empty()
        || level2Visibility_.tracking() != VisibilityTracking::sourceInitialized
        || !level2Visibility_.refreshRequested() || level2Complete_ || level2PlayerArmor_ < warningArmor
        || level2ActiveEnemyCount_ != initialEnemies || level2SpawnedEnemyCount_ != initialEnemies
        || !level2PendingTerminalEffectPresentations_.empty()
        || !level2RetiredEffectSemanticEntities_.empty()
        || !level2RetiredEffectSemanticActiveOrder_.empty()
        || !level2RetiredEffectSemanticActivePositions_.empty()
        || !level2EffectSemanticActivePositionOverrides_.empty()) {
        throw std::invalid_argument("first source Level2 prefix requires its genuine preloop ownership");
    }
    const auto& guards = *level2PrefixGuards_;
    const auto prefix = renderer->prepare({level2Player_, level2Entities_, {},
        level1_.projectionListOrigin_, level2Visibility_.refreshRequested(), guards.fadeTicks,
        guards.specialActorPresent, level2PlayerCollisionMomentum_, true, guards.effectHandles});
    if (!prefix) {
        throw std::invalid_argument("first source Level2 renderer returned no prefix execution");
    }
    applyLevel2Prefix(*prefix);
    level2HasAdvanced_ = true;
    phase_ = GameplayPhase::level2;
    presentLevel2(input, renderer, true);
    if (!level2TailConsumed_) {
        throw std::invalid_argument("first source Level2 renderer did not consume its genuine first tail");
    }
    level2EntryPaletteActive_ = false;
}

void GameplayModel::qualifyLevel2SourcePrefix(const GameplayInput& input, Level2FrameRenderer* renderer) {
    constexpr std::int16_t sourceForwardLimit = 200;
    constexpr std::int16_t sourceYawLimit = 512;
    const auto sourceLifecycle=renderer!=nullptr && renderer->prefixExecution()==Level2PrefixExecution::source
        && (renderer->sourceDomain()==Level2SourceDomain::projectileLifecycle
            || renderer->sourceDomain()==Level2SourceDomain::periodicLifecycle);
    const auto sourceCombat=sourceLifecycle || (renderer!=nullptr && renderer->prefixExecution()==Level2PrefixExecution::source
        && renderer->sourceDomain()==Level2SourceDomain::projectileCombat);
    if(sourceCombat) {
        //Validate canonical pool/list/phase ancestry before preserving source guards.
        if(renderer->sourceDomain()==Level2SourceDomain::periodicLifecycle) {
            recoveredLevel2PeriodicProjectilePrefixInput(Level1PlayerCombatDomain::expiryAndLoss);
        } else {
            recoveredLevel2ProjectilePrefixInput(sourceLifecycle ? Level1PlayerCombatDomain::expiryAndLoss
                : Level1PlayerCombatDomain::survivingOnly);
        }
    }
    const auto sourceControls = renderer != nullptr && renderer->prefixExecution() == Level2PrefixExecution::source
        && phase_ == GameplayPhase::level2 && level2TailConsumed_ && level2PrefixGuards_
        && (sourceCombat || !input.fire) && !input.exit && !input.confirm
        && (sourceCombat || !level2PendingInput_.fire) && !level2PendingInput_.exit && !level2PendingInput_.confirm;
    const auto supportedPrefixPlayer = level2Player_.pitch == 0 && level2Player_.roll == 0
        && (sourceControls || (level2Player_.yaw == 0 && level2Player_.yawVelocity == 0))
        && level2Player_.yawVelocity >= -sourceYawLimit && level2Player_.yawVelocity <= sourceYawLimit
        && level2Player_.pitchVelocity == 0 && level2Player_.rollVelocity == 0
        && level2Player_.xVelocity == 0 && level2Player_.yVelocity == 0
        && level2Player_.zVelocity == 0
        && level2Player_.forwardVelocity >= -sourceForwardLimit
        && level2Player_.forwardVelocity <= sourceForwardLimit
        && (level2Player_.forwardVelocity == 0 || level2Player_.position.z == 0);
    const auto constructorActors = level2Entities_.size() == 7
        && level2ActiveListOrder_.size() == 7
        && std::all_of(level2Entities_.begin(), level2Entities_.end(), [](const auto& actor) {
            return actor.active() && (actor.type == 8 || actor.type == 13 || actor.type == 30)
                && actor.renderProfile == WorldActorRenderProfile::noHandles;
        });
    if ((!sourceControls && (hasLevel2PrefixInput(input) || hasLevel2PrefixInput(level2PendingInput_))) || !supportedPrefixPlayer
        || (!sourceCombat && !constructorActors)
        || (level2PlayerCollisionMomentum_!=0
            && !(sourceControls && renderer->sourceDomain()==Level2SourceDomain::periodicLifecycle))
        || level2Complete_ || level2PlayerArmor_ < 0) {
        level2PrefixGuards_.reset();
    }
}

void GameplayModel::advanceLevel2SourcePrefix(const GameplayInput& input, Level2FrameRenderer& renderer) {
    if (!level2PrefixGuards_
        || level2Visibility_.tracking() != VisibilityTracking::sourceInitialized
        || !level2Visibility_.lastCamera()
        || !level2PendingTerminalEffectPresentations_.empty()
        || !level2RetiredEffectSemanticEntities_.empty()
        || !level2RetiredEffectSemanticActiveOrder_.empty()
        || !level2RetiredEffectSemanticActivePositions_.empty()
        || !level2EffectSemanticActivePositionOverrides_.empty()) {
        throw std::invalid_argument("source prefix requires retained constructor/session ownership");
    }
    const auto& guards = *level2PrefixGuards_;
    const auto prefix = renderer.prepare({level2Player_, level2Entities_,
        level2ActiveListOrder_, *level2Visibility_.lastCamera(),
        level2Visibility_.refreshRequested(), guards.fadeTicks,
        guards.specialActorPresent, level2PlayerCollisionMomentum_,
        phase_ == GameplayPhase::level2 && !level2Complete_ && level2PlayerArmor_ >= 0,
        guards.effectHandles});
    if (!prefix) {
        throw std::invalid_argument("source prefix renderer returned no execution");
    }
    applyLevel2Prefix(*prefix);
    level2TailConsumed_ = false;
    presentLevel2(input, &renderer, false);
}

void GameplayModel::expireLevel2EffectPresentation() {
    // Terminal effect records retained for the preceding post-display page
    // expire when the next logical boundary begins.
    level2PendingTerminalEffectPresentations_.clear();
    level2PendingTerminalEffectSlots_.clear();
    level2RetiredEffectRebuildReported_ =
        !level2RetiredEffectSemanticEntities_.empty();
    if (level2RetiredEffectRebuildReported_) {
        // FUN_1000:3768's allocation-order rebuild is persistent input to
        // the following callback walk, not only the preceding presenter's
        // observable order.
        level2ActiveListOrder_ = level2AllocationListOrder_;
    }
    level2RetiredEffectSemanticEntities_.clear();
    level2RetiredEffectSemanticActiveOrder_.clear();
    level2RetiredEffectSemanticOrderRebuilt_ = false;
    level2RetiredEffectSemanticActivePositions_.clear();
    level2EffectSemanticActivePositionOverrides_.clear();
}

void GameplayModel::sortLevel2ActiveByDistance() {
    for (std::size_t destination = 0;
         destination < level2ActiveListOrder_.size();
         ++destination) {
        auto selected = destination;
        for (std::size_t candidate = destination + 1;
             candidate < level2ActiveListOrder_.size();
             ++candidate) {
            const auto sortDistance = [this](
                const std::size_t index
            ) noexcept {
                return level2Entities_[index].type == 46
                    ? std::uint32_t{0x7000}
                    : approximatePlanarDistance(
                        level2Player_.position,
                        level2Entities_[index].position
                    );
            };
            if (
                sortDistance(level2ActiveListOrder_[selected])
                < sortDistance(level2ActiveListOrder_[candidate])
            ) {
                selected = candidate;
            }
        }
        if (selected != destination) {
            std::swap(
                level2ActiveListOrder_[destination],
                level2ActiveListOrder_[selected]
            );
        }
    }
}

std::size_t GameplayModel::level2FixedPoolSlot(const std::size_t entityIndex) const noexcept {
    if (entityIndex < level2EntityFixedPoolSlots_.size()
        && level2EntityFixedPoolSlots_[entityIndex] < level2FixedObjectPoolSize_) {
        return level2EntityFixedPoolSlots_[entityIndex];
    }
    auto slot = std::size_t{0};
    for (std::size_t index = 0; index < entityIndex; ++index) {
        if (
            level2EntityActorPoolSlots_[index] >= level2ActorPoolSize_
            && level2EntityProjectilePoolSlots_[index]
                >= level2ProjectilePoolSize_
            && level2EntityDeathEffectPoolSlots_[index]
                >= level2DeathEffectPoolSize_
        ) {
            ++slot;
        }
    }
    return slot;
}

std::vector<GameplayModel::Level2EntityIdentity> GameplayModel::captureLevel2ActiveIdentities(
    const std::vector<std::size_t>& level2ActiveListOrderAtStart) const {
    std::vector<Level2EntityIdentity> level2ActiveIdentitiesAtStart;
    level2ActiveIdentitiesAtStart.reserve(level2ActiveListOrderAtStart.size());
    for (std::size_t activePosition = 0;
         activePosition < level2ActiveListOrderAtStart.size();
         ++activePosition) {
        const auto index = level2ActiveListOrderAtStart[activePosition];
        if (index >= level2Entities_.size()) {
            continue;
        }
        if (level2EntityActorPoolSlots_[index] < level2ActorPoolSize_) {
            level2ActiveIdentitiesAtStart.push_back({
                0, level2EntityActorPoolSlots_[index], activePosition,
            });
        } else if (
            level2EntityProjectilePoolSlots_[index]
                < level2ProjectilePoolSize_
        ) {
            level2ActiveIdentitiesAtStart.push_back({
                1, level2EntityProjectilePoolSlots_[index], activePosition,
            });
        } else if (
            level2EntityDeathEffectPoolSlots_[index]
                < level2DeathEffectPoolSize_
        ) {
            level2ActiveIdentitiesAtStart.push_back({
                2, level2EntityDeathEffectPoolSlots_[index], activePosition,
            });
        } else {
            level2ActiveIdentitiesAtStart.push_back({
                3, level2FixedPoolSlot(index), activePosition,
            });
        }
    }
    return level2ActiveIdentitiesAtStart;
}

std::vector<std::size_t> GameplayModel::remapLevel2BoundaryOrder(
    const std::vector<Level2EntityIdentity>& level2ActiveIdentitiesAtStart) const {
    std::vector<std::size_t> order;
    order.reserve(level2ActiveIdentitiesAtStart.size());
    for (const auto identity : level2ActiveIdentitiesAtStart) {
        for (std::size_t index = 0;
             index < level2Entities_.size();
             ++index) {
            const auto fixed =
                level2EntityActorPoolSlots_[index]
                    >= level2ActorPoolSize_
                && level2EntityProjectilePoolSlots_[index]
                    >= level2ProjectilePoolSize_
                && level2EntityDeathEffectPoolSlots_[index]
                    >= level2DeathEffectPoolSize_;
            const auto matches = identity.pool == 0
                ? level2EntityActorPoolSlots_[index] == identity.slot
                : identity.pool == 1
                    ? level2EntityProjectilePoolSlots_[index]
                        == identity.slot
                    : identity.pool == 2
                        ? level2EntityDeathEffectPoolSlots_[index]
                            == identity.slot
                        : fixed && level2FixedPoolSlot(index) == identity.slot;
            if (matches) {
                order.push_back(index);
                break;
            }
        }
    }
    return order;
}

} // namespace stellar7
