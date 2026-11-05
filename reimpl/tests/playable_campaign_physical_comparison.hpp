#pragma once
#include "playable_campaign_comparison.hpp"
#include <type_traits>
// Read-only physical owners, including inactive records and aliases.
namespace stellar7 {
struct GameplayModelTestAccess {
    template<class Proxy> static void compareProxy(const Proxy& a,const Proxy& b) {
        if constexpr (std::is_same_v<Proxy,FixedPosition>) {
            test::requireCampaignComparison(a==b,"budget composition changed canonical proxy XYZ");
        } else {
            static_assert(std::is_same_v<Proxy,Level1Entity>,"update full canonical proxy comparison");
            test::requireCampaignComparison(test::campaignActorFields(a)==test::campaignActorFields(b),
                "budget composition changed full canonical proxy actor");
        }
    }
    static auto effectFields(const PlayableSystemEffect& effect) {
        std::array<std::uint64_t,4> result{{effect.index(),0,0,0}};
        std::visit([&](const auto& value) {
            using Effect=std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Effect,SystemAudio>) {
                result[1]=value.event;
            } else if constexpr (std::is_same_v<Effect,SystemDecoyStrip>
                || std::is_same_v<Effect,SystemDecoyPalette>) {
                result[1]=static_cast<unsigned>(value.mode);
            } else {
                result[1]=value.system;
                if constexpr (std::is_same_v<Effect,SystemBorder>) {result[2]=value.selected;}
                else if constexpr (std::is_same_v<Effect,SystemPanel>) {
                    result[2]=static_cast<unsigned>(value.style);result[3]=value.remainingCharges;
                } else {static_assert(std::is_same_v<Effect,SystemCopy>,"update pending system-effect comparison");}
            }
        },effect);
        return result;
    }
    static auto projectileStorage(const GameplayModel& model) {
        return std::tie(model.level2ProjectilePool_, model.level2ProjectilePoolActive_,
            model.level2ProjectilePoolWasActiveAtBoundaryStart_, model.level2EntityProjectilePoolSlots_);
    }

    static void compare(const GameplayModel& a, const GameplayModel& b) {
        test::compareCampaignRng(a.level2Rng(),b.level2Rng(),test::CampaignComparisonStage::boundary);
        test::compareCampaignRng(a.failureRng(),b.failureRng(),test::CampaignComparisonStage::boundary);
        const auto pool = [](const auto& x, const auto& y) {
            test::requireCampaignComparison(x.size()==y.size(),"budget composition changed pending actor count");
            for (std::size_t i = 0; i < x.size(); ++i) {
                test::requireCampaignComparison(test::campaignActorFields(x[i]) == test::campaignActorFields(y[i]),
                    "replay budget changed a physical pool record");
            }
        };
        pool(a.level2ActorPool_, b.level2ActorPool_); pool(a.level2FixedPool_, b.level2FixedPool_);
        pool(a.level2ProjectilePool_, b.level2ProjectilePool_); pool(a.level2DeathEffectPool_, b.level2DeathEffectPool_);
        pool(a.level2PendingTerminalEffectPresentations_,b.level2PendingTerminalEffectPresentations_);
        pool(a.level2PresentationEntities_,b.level2PresentationEntities_);
        pool(a.level2RetiredEffectSemanticEntities_,b.level2RetiredEffectSemanticEntities_);
        pool(a.failureDebris_,b.failureDebris_);
        compareProxy(a.campaignPlayerProxy_,b.campaignPlayerProxy_);
        test::requireCampaignComparison(a.playableSystemEffects_.size()==b.playableSystemEffects_.size(),
            "budget composition changed pending system-effect count");
        for (std::size_t i=0;i<a.playableSystemEffects_.size();++i) {
            test::requireCampaignComparison(effectFields(a.playableSystemEffects_[i])==effectFields(b.playableSystemEffects_[i]),
                "budget composition changed pending system effect");
        }
        const auto pending=[](const GameplayInput& value) {
            return std::tie(value.left,value.right,value.forward,value.reverse,value.fire,value.exit,value.confirm,value.systemAction);
        };
        test::requireCampaignComparison(pending(a.level2PendingInput_)==pending(b.level2PendingInput_)
            &&a.level2PendingInput_.pointer.has_value()==b.level2PendingInput_.pointer.has_value(),
            "budget composition changed deferred public input");
        if(a.level2PendingInput_.pointer) {
            const auto pointer=[](const NativePointerInput& value) {
                return std::tie(value.position.x,value.position.y,value.primary,value.secondary,value.phase);
            };
            test::requireCampaignComparison(pointer(*a.level2PendingInput_.pointer)==pointer(*b.level2PendingInput_.pointer),
                "budget composition changed deferred pointer input");
        }
        test::requireCampaignComparison(std::tie(a.runtimeDifficulty_,a.playableExit_,a.playableCampaignFrames_,
            a.level2ActorIncarnations_,a.level2FixedPoolEverUsed_,a.level2AllocationRevision_,
            a.level2NonProjectileAllocationRevision_,a.level2FixedAllocationRevision_,
            a.level2PresentationActiveListOrder_,a.level2PresentationPoolSlots_,a.level2PresentationActorIncarnations_,
            a.level2PresentationAllocationRevision_,a.level2RetiredEffectSemanticActiveOrder_,
            a.level2RetiredEffectSemanticActivePositions_,a.level2EffectSemanticActivePositionOverrides_,
            a.level2RetiredEffectRebuildReported_,a.level2RetiredEffectSemanticOrderRebuilt_,
            a.level2AllocationListOrder_,a.level2ObjectListOrder_,a.level2ProjectilePoolWasActiveAtBoundaryStart_,
            a.level2CRandState_,a.level2VisualRandomState_,a.level2HasAdvanced_,a.level2TailConsumed_,
            a.level2PlayerCollisionMomentum_,a.level2Type46ContactSlowdown_,a.level2Complete_,
            a.level2CompletionObjectIndex_,a.level2PostWaveObjectIndex_,a.level2PickupIndex_)
            ==std::tie(b.runtimeDifficulty_,b.playableExit_,b.playableCampaignFrames_,
            b.level2ActorIncarnations_,b.level2FixedPoolEverUsed_,b.level2AllocationRevision_,
            b.level2NonProjectileAllocationRevision_,b.level2FixedAllocationRevision_,
            b.level2PresentationActiveListOrder_,b.level2PresentationPoolSlots_,b.level2PresentationActorIncarnations_,
            b.level2PresentationAllocationRevision_,b.level2RetiredEffectSemanticActiveOrder_,
            b.level2RetiredEffectSemanticActivePositions_,b.level2EffectSemanticActivePositionOverrides_,
            b.level2RetiredEffectRebuildReported_,b.level2RetiredEffectSemanticOrderRebuilt_,
            b.level2AllocationListOrder_,b.level2ObjectListOrder_,b.level2ProjectilePoolWasActiveAtBoundaryStart_,
            b.level2CRandState_,b.level2VisualRandomState_,b.level2HasAdvanced_,b.level2TailConsumed_,
            b.level2PlayerCollisionMomentum_,b.level2Type46ContactSlowdown_,b.level2Complete_,
            b.level2CompletionObjectIndex_,b.level2PostWaveObjectIndex_,b.level2PickupIndex_),
            "replay budget changed retained identities/presentation aliases/deferred control state");
        test::requireCampaignComparison(std::tie(a.level2ActorPoolActive_,a.level2FixedPoolKnown_,
            a.level2ProjectilePoolActive_,a.level2DeathEffectPoolActive_,a.level2EntityActorPoolSlots_,
            a.level2EntityFixedPoolSlots_,a.level2EntityProjectilePoolSlots_,a.level2EntityDeathEffectPoolSlots_,
            a.level2ActiveEnemyCount_,a.level2SpawnedEnemyCount_,a.playableSystemOldActorSlots_,a.level6GuardianTurn_,
            a.level2PendingTerminalEffectSlots_,a.level2DeathEffectPoolPendingPresenterRetirement_) == std::tie(b.level2ActorPoolActive_,b.level2FixedPoolKnown_,
            b.level2ProjectilePoolActive_,b.level2DeathEffectPoolActive_,b.level2EntityActorPoolSlots_,
            b.level2EntityFixedPoolSlots_,b.level2EntityProjectilePoolSlots_,b.level2EntityDeathEffectPoolSlots_,
            b.level2ActiveEnemyCount_,b.level2SpawnedEnemyCount_,b.playableSystemOldActorSlots_,b.level6GuardianTurn_,
            b.level2PendingTerminalEffectSlots_,b.level2DeathEffectPoolPendingPresenterRetirement_), "replay budget changed pool ownership/quota");
    }
};
}
