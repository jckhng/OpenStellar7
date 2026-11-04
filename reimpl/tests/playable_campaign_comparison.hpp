#pragma once
#include "stellar7/native_session.hpp"
#include <stdexcept>
#include <tuple>

namespace stellar7::test {
enum class CampaignComparisonStage { frame, boundary };
inline void requireCampaignComparison(const bool condition,const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
inline void compareCampaignRng(StellarRng left, StellarRng right, const CampaignComparisonStage stage) {
    requireCampaignComparison(left.calls() == right.calls() && left.indexA() == right.indexA() && left.indexB() == right.indexB(),
        "budget composition changed RNG cursors");
    if (stage==CampaignComparisonStage::boundary) {
        for (std::size_t index = 0; index < StellarRng::wordCount; ++index) {
            requireCampaignComparison(left.next() == right.next(), "budget composition changed RNG words at a boundary");
        }
    }
}

inline void compareCampaignFrame(const IndexedScreen& left, const IndexedScreen& right) {
    requireCampaignComparison(left.pixels == right.pixels && left.palette.size() == right.palette.size(),
        "budget composition changed indexed pixels/palette size");
    for (std::size_t index = 0; index < left.palette.size(); ++index) {
        requireCampaignComparison(left.palette[index].red == right.palette[index].red
            && left.palette[index].green == right.palette[index].green
            && left.palette[index].blue == right.palette[index].blue,
            "budget composition changed a palette color");
    }
}

inline auto campaignActorFields(const Level1Entity& value) {
            return std::tie(value.allocationIndex,value.flags,value.type,value.position.x,value.position.y,value.position.z,
                value.pitch,value.roll,value.yaw,value.pitchVelocity,value.rollVelocity,value.yawVelocity,
                value.xVelocity,value.yVelocity,value.zVelocity,value.forwardVelocity,value.stateBytes4dTo52,
                value.modelAnimationVariant,value.primaryType11AnimationSelector,value.behaviorTimer,value.type12MissGate,
                value.type6CloakTimer,value.weaponSlotCount,value.weaponCooldown,value.weaponSlotTimers,
                value.collisionRadius,value.presenterDistance,value.targetHeading,value.collisionContactAllocationIndex,
                value.collisionContactIsPlayer,value.presentationModelAnimationVariant,value.animatedModelPresenterEnabled,
                value.ownerIsPlayer,value.ownerAllocationIndex,value.renderProfile,value.modelAnimationTail,value.presentationModelAnimationTail);
}

struct CampaignBudgetComparison {
    std::size_t oplCursor = 0;
    std::vector<std::int16_t> leftPcm, rightPcm;

    void compare(const NativeGameplaySession& left, const NativeGameplaySession& right,
        const NativeSessionDelivery& a, const NativeSessionDelivery& b, const CampaignComparisonStage stage) {
        const bool boundary=stage==CampaignComparisonStage::boundary;
        requireCampaignComparison(a.firstSample == b.firstSample && a.samples == b.samples,
            "budget composition changed a PCM delivery");
        leftPcm.insert(leftPcm.end(),a.samples.begin(),a.samples.end());
        rightPcm.insert(rightPcm.end(),b.samples.begin(),b.samples.end());
        requireCampaignComparison(a.events.size() == b.events.size() && a.presentations.size() == b.presentations.size()
            && a.entry.size() == b.entry.size(), "budget composition changed delivery children");
        for (std::size_t index = 0; index < a.events.size(); ++index) {
            requireCampaignComparison(a.events[index].loopTick == b.events[index].loopTick
                && a.events[index].eventId == b.events[index].eventId, "budget composition changed ordered events");
        }
        for (std::size_t index = 0; index < a.entry.size(); ++index) {
            requireCampaignComparison(a.entry[index].elapsedNanoseconds == b.entry[index].elapsedNanoseconds
                && a.entry[index].samples == b.entry[index].samples, "budget composition changed entry delivery");
            compareCampaignFrame(a.entry[index].indexed,b.entry[index].indexed);
        }
        compareCampaignFrame(left.frame(),right.frame());
        for (std::size_t index = 0; index < a.presentations.size(); ++index) {
            requireCampaignComparison(a.presentations[index].sampleEnd == b.presentations[index].sampleEnd,
                "budget composition changed a presentation endpoint");
            compareCampaignFrame(a.presentations[index].indexed,b.presentations[index].indexed);
        }
        for (const auto role : {NativePageRole::cache,NativePageRole::back,NativePageRole::front}) {
            requireCampaignComparison(left.retainedPagePixels(role) == right.retainedPagePixels(role),
                "budget composition changed retained page pixels");
        }
        const auto pagesA = left.pageObservation(), pagesB = right.pageObservation();
        requireCampaignComparison(std::tie(pagesA.projection,pagesA.cache,pagesA.back,pagesA.front,pagesA.copy,pagesA.draw)
            == std::tie(pagesB.projection,pagesB.cache,pagesB.back,pagesB.front,pagesB.copy,pagesB.draw),
            "budget composition changed retained page roles");

        const auto& modelA = left.model();
        const auto& modelB = right.model();
        requireCampaignComparison(modelA.phase() == modelB.phase() && modelA.ticks() == modelB.ticks()
            && modelA.campaignLevel() == modelB.campaignLevel() && modelA.score() == modelB.score()
            && modelA.playerArmor() == modelB.playerArmor() && modelA.remainingContinues() == modelB.remainingContinues()
            && modelA.continueChoice() == modelB.continueChoice() && modelA.level2LoopTick() == modelB.level2LoopTick()
            && modelA.level2ActiveListOrder() == modelB.level2ActiveListOrder()
            && modelA.level2Ground().points() == modelB.level2Ground().points(),
            "budget composition changed retained campaign state");
        const auto& systemsA = modelA.level1State();
        const auto& systemsB = modelB.level1State();
        requireCampaignComparison(systemsA.systemTimers() == systemsB.systemTimers()
            && systemsA.systemStatusLevels() == systemsB.systemStatusLevels()
            && systemsA.selectedSystem() == systemsB.selectedSystem(), "budget composition changed system state");
        requireCampaignComparison(campaignActorFields(modelA.level2Player()) == campaignActorFields(modelB.level2Player()), "budget composition changed the player");
        requireCampaignComparison(modelA.presentationTranslation()==modelB.presentationTranslation(),"budget composition changed presentation world translation");
        const auto& actorsA = modelA.level2Entities();
        const auto& actorsB = modelB.level2Entities();
        requireCampaignComparison(actorsA.size() == actorsB.size(), "budget composition changed actor membership");
        for (std::size_t index = 0; index < actorsA.size(); ++index) {
            requireCampaignComparison(campaignActorFields(actorsA[index]) == campaignActorFields(actorsB[index]), "budget composition changed an actor record");
        }
        compareCampaignRng(modelA.level2Rng(),modelB.level2Rng(),stage);
        compareCampaignRng(modelA.failureRng(),modelB.failureRng(),stage);
        requireCampaignComparison(left.elapsedSamples() == right.elapsedSamples(), "budget composition changed the retained sample epoch");
        const auto driver = [](const SoundBlasterDriverState& value) {
            return std::tie(value.notes,value.velocities,value.channelVoiceCursors,value.directTimers,value.directLevelLatch,
                value.clockSource,value.normalClockCycles,value.clockPhase,value.clockPeriod,value.sequencePosition,
                value.sequenceWait,value.runningStatus,value.fade,value.fadeDivider,value.sequenceActive,
                value.carrierKsl,value.carrierFallback,value.operatorKsl,value.operatorFallback,value.operatorEnabled,
                value.driverEnabled,value.initializationOplWrites,value.sequenceAllocated,value.sequenceSelected,
                value.sequencePositions,value.sequenceAllocations,value.sequenceSelections,value.sequenceActivity);
        };
        requireCampaignComparison(driver(left.synth().driverState()) == driver(right.synth().driverState()),
            "budget composition changed the retained audio driver");
        const auto& oplA = left.synth().registerWrites();
        const auto& oplB = right.synth().registerWrites();
        requireCampaignComparison(oplA.size() == oplB.size() && oplA.size() >= oplCursor,
            "budget composition changed OPL write count/history");
        for (std::size_t index = boundary ? 0 : oplCursor; index < oplA.size(); ++index) {
            requireCampaignComparison(oplA[index].registerNumber == oplB[index].registerNumber && oplA[index].value == oplB[index].value,
                "budget composition changed ordered OPL writes");
        }
        oplCursor = oplA.size();
        if (boundary) {
            requireCampaignComparison(leftPcm == rightPcm, "budget composition changed whole PCM at a boundary");
        }
    }
};

} //namespace stellar7::test
