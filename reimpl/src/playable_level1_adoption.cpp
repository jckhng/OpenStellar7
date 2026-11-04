#include "stellar7/native_session.hpp"
#include "stellar7/enemy20_audio_work.hpp"
#include "level1_reward_model_bank.hpp"
#include "retained_level1_clock_domain.hpp"
#include "playable_startup_provider.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::size_t initialOrdinaryCount = 2;
constexpr std::uint32_t initialRngCalls = 16;
constexpr std::size_t fixedRadarBase = 16;
constexpr std::size_t projectileRadarBase = 32;
constexpr std::size_t effectRadarBase = projectileRadarBase + Level1State::projectilePoolSize;
constexpr std::size_t resetFixedCount = 8;

Level1Entity projectileRecord(const Level1Projectile& source) {
    Level1Entity actor{};
    actor.flags = source.flags;
    actor.type = source.type;
    actor.collisionRadius = source.collisionRadius;
    actor.position = source.position;
    actor.pitch = source.pitch;
    actor.roll = source.roll;
    actor.yaw = source.yaw;
    actor.pitchVelocity = source.pitchVelocity;
    actor.rollVelocity = source.rollVelocity;
    actor.yawVelocity = source.yawVelocity;
    actor.xVelocity = source.xVelocity;
    actor.yVelocity = source.yVelocity;
    actor.zVelocity = source.zVelocity;
    actor.forwardVelocity = source.forwardVelocity;
    std::copy_n(source.stateBytes4dTo54.begin(), actor.stateBytes4dTo52.size(), actor.stateBytes4dTo52.begin());
    actor.behaviorTimer = source.stateBytes4dTo54.back();
    actor.presenterDistance = source.presenterDistance;
    actor.targetHeading = source.targetHeading;
    actor.ownerIsPlayer = source.ownerIsPlayer;
    actor.ownerAllocationIndex = source.ownerAllocationIndex;
    actor.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
    actor.animatedModelPresenterEnabled = false;
    return actor;
}
}

void GameplayModel::adoptPlayableLevel1(const std::vector<Model3d>& shared, const std::vector<Model3d>& local) {
    const auto& source = level1_;
    const auto definition = campaignLevelDefinition(0, campaignDifficultyIndex(runtimeDifficulty_));
    const auto noObject = static_cast<std::size_t>(-1);
    if (playableCampaignFrames_ || phase_ != GameplayPhase::level1 || ticks_ != 0
        || source.levelIndex_ != 0 || source.loopTick_ != 0 || source.hasAdvanced_
        || !source.nativeInitialListPending_ || !source.missionActive_ || source.failureState_
        || source.failurePending_ || source.successState_ || source.successContactPending_ || source.exitState_
        || source.ordinaryTailPending_ || source.ordinaryTailConsumed_ || recoveredSuccessPause_
        || source.rng_.calls() != initialRngCalls || source.entities_.size() != definition.table.actors.size()
        || source.allocationList_.size() != source.entities_.size() || !source.objectListOrder_.empty()
        || !source.activeListOrder_.empty() || !source.semanticActiveListOrder_.empty()
        || source.activeEnemyCount_ != initialOrdinaryCount || source.spawnedEnemyCount_ != initialOrdinaryCount
        || source.completionObjectAllocationIndex_ != noObject || source.postWaveObjectAllocationIndex_ != noObject
        || source.playerWeaponCooldown_ != 0 || source.playerCollisionMomentum_ != 0
        || std::any_of(source.playerProjectileTimers_.begin(), source.playerProjectileTimers_.end(), [](auto value) { return value != 0; })
        || std::any_of(source.projectiles_.begin(), source.projectiles_.end(), [](const auto& actor) { return actor.active(); })
        || std::any_of(source.deathEffects_.begin(), source.deathEffects_.end(), [](const auto& actor) { return actor.listed || actor.entity.active(); })
        || !source.audioEvents_.empty() || source.selectedSystem_ != -1
        || std::any_of(source.systemTimers_.begin(), source.systemTimers_.end(), [](auto value) { return value != 0; })) {
        throw std::invalid_argument("playable Level1 adoption requires its untouched initial constructor/preloop owner");
    }
    validateCampaignLoaderActors(definition.table, shared, local);
    for (std::size_t index = 0; index < source.entities_.size(); ++index) {
        const auto& actor = source.entities_[index];
        const auto& entry = definition.table.actors[index];
        const bool ordinary = index < initialOrdinaryCount;
        const bool special = index == source.entities_.size() - 1;
        const auto expectedHealth = ordinary ? campaignOrdinaryDefaults(entry.type).health : special ? 16 : 100;
        const auto expectedBehavior = ordinary ? campaignOrdinaryDefaults(entry.type).behavior : special ? 0xf3 : 0;
        const auto& allocation = source.allocationList_[index];
        if (allocation.kind != Level1State::ObjectKind::entity || allocation.index != index
            || actor.allocationIndex != index || actor.type != entry.type || actor.flags != 0
            || actor.collisionRadius != campaignModelRadius(shared, local, entry.type)
            || actor.position.x != campaignCoordinate(entry.x) || actor.position.y != campaignCoordinate(entry.y)
            || actor.position.z != (entry.type == 11 ? 1000 : 0) || actor.yaw != entry.yaw
            || actor.stateBytes4dTo52[0] != (ordinary ? 0xff : 0xfe)
            || actor.stateBytes4dTo52[1] != expectedHealth || actor.stateBytes4dTo52[4] != expectedBehavior
            || actor.stateBytes4dTo52[5] != 0 || actor.behaviorTimer != 0 || actor.modelAnimationVariant != 0) {
            throw std::invalid_argument("playable Level1 adoption lost an initial table actor");
        }
    }
    if (source.player_.allocationIndex != source.entities_.size() || !(source.player_.position == FixedPosition{})
        || source.player_.collisionRadius != campaignModelRadius(shared, local, 0)
        || source.player_.pitch != 0 || source.player_.roll != 0 || source.player_.yaw != 0
        || source.player_.forwardVelocity != 0 || source.player_.yawVelocity != 0 || source.player_.stateBytes4dTo52[1] != 1) {
        throw std::invalid_argument("playable Level1 adoption lost its initial player");
    }
    auto staged = std::make_unique<GameplayModel>(*this);
    auto& old = staged->level1_;
    staged->bindCampaignModels(0, shared, local);
    staged->level2Entities_ = std::move(old.entities_);
    for (auto& actor : staged->level2Entities_) {
        //The Level1 owner encodes these source presenters implicitly.
        actor.renderProfile = actor.type == 11 ? WorldActorRenderProfile::heapSelectorDeferredAnimation
            : WorldActorRenderProfile::noHandles;
        actor.animatedModelPresenterEnabled = actor.type == 11;
    }
    staged->initializeCampaignPools(0, initialOrdinaryCount);
    for (std::size_t slot = initialOrdinaryCount; slot < staged->level2ActorPoolSize_; ++slot) {
        auto& actor = staged->level2ActorPool_[slot];
        actor.stateBytes4dTo52[0] = 0xff;
        actor.renderProfile = WorldActorRenderProfile::noHandles;
        actor.animatedModelPresenterEnabled = false;
    }
    for (std::size_t slot = 0; slot < old.projectiles_.size(); ++slot) {
        staged->level2ProjectilePool_[slot] = projectileRecord(old.projectiles_[slot]);
    }
    for (std::size_t slot = 0; slot < old.deathEffects_.size(); ++slot) {
        auto actor = old.deathEffects_[slot].entity;
        actor.renderProfile = WorldActorRenderProfile::noHandles;
        actor.animatedModelPresenterEnabled = false;
        staged->level2DeathEffectPool_[slot] = actor;
    }
    staged->level2Player_ = old.player_;
    staged->level2Player_.renderProfile = WorldActorRenderProfile::noHandles;
    staged->level2Player_.animatedModelPresenterEnabled = false;
    staged->level2Rng_ = old.rng_;
    staged->level2Ground_ = old.ground_;
    staged->level2Score_ = old.score_;
    staged->level2PlayerArmor_ = old.playerArmor_;
    staged->level2ActiveEnemyCount_ = old.activeEnemyCount_;
    staged->level2SpawnedEnemyCount_ = old.spawnedEnemyCount_;
    staged->level2CompletionObjectIndex_ = old.completionObjectAllocationIndex_;
    staged->level2PostWaveObjectIndex_ = old.postWaveObjectAllocationIndex_;
    staged->level2PickupIndex_ = noObject;
    staged->level2PrefixGuards_.reset();
    staged->level2PendingInput_ = {};
    staged->level2HasAdvanced_ = false;
    staged->level2Complete_ = false;
    staged->level3CompletionByPickup_ = false;
    staged->level2PlayerCollisionMomentum_ = 0;
    staged->level2Type46ContactSlowdown_ = false;
    staged->level2ActiveListOrder_.clear();
    staged->level2ObjectListOrder_.clear();
    staged->level2Visibility_.resetLevel();
    old.allocationList_.clear();
    old.nativeInitialListPending_ = false;
    old.missionActive_ = false;
    staged->playableCampaignFrames_ = true;
    staged->phase_ = GameplayPhase::playablePreloopReturn;
    staged->snapshotLevel2Presentation();
    *this = std::move(*staged);
}

void NativeGameplaySession::adoptPlayableLevel1() {
    if (playableCampaign_ || initialized_ || !level1Pages_ || !recoveredLoader_ || recoveredFrame_
        || recoveredFailure_ || recoveredRetry_ || recoveredSuccess_ || !recoveredController_
        || !audio_->workPhase_ || audio_->workPhase_->playableCampaign
        || (!retainedLevel1Program(*audio_->workPhase_)
            && !(recoveredLoader_->playableAudio && playableStartupLevel1Program(*audio_->workPhase_)))
        || !recoveredStartupPalette_ || !pixels_ || !radar_ || !assets_.cockpitSettings
        || model_.phase() != GameplayPhase::level1 || model_.ticks() != 0
        || recoveredController_->expectedLoopTick() != 0 || pageProjection_ != NativePageProjection::retainedLevel1Loader
        || recoveredLoader_->firstSample + recoveredLoader_->samples.size() != clock_.elapsedSamples()
        || (!(pages_.frontPage()==0xa000 && pages_.backPage()==0xa800)
            && !((hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::storyPanels) || audio_->attractPagesTransferred_)
                && pages_.frontPage()==0xa800 && pages_.backPage()==0xa000)) || pages_.cachePage() != 0xa400
        || assets_.campaignLevels.empty() || assets_.campaignLevels[0].resourceName != "LEVEL1.RES") {
        throw std::invalid_argument("playable Level1 adoption requires its completed initial native loader/preloop");
    }
    const auto& resources = assets_.campaignLevels[0];
    if (campaignResources_ && (campaignResources_->level != 0 || campaignResources_->models != &resources.models
        || campaignResources_->bitmaps != &resources.bitmapGroups || campaignResources_->music != &resources.musicSequence)) {
        throw std::invalid_argument("playable Level1 adoption lost its current resource bank");
    }
    const auto sharedCount = level1RewardModelCapacity(assets_);
    const std::vector<Model3d> shared(assets_.models.begin(), assets_.models.begin() + sharedCount);
    std::optional<SoundBlasterEnemy20McbByte> playableMcb;
    const auto& callback=audio_->workPhase_->callback;
    if (!callback || !callback->program) {
        throw std::invalid_argument("playable adoption lost its retained audio program");
    }
    if (!callback->program->enemy20McbByte) {
        constexpr std::array<std::int16_t,6> loadedOptions{1,0,1,1,0,255};
        constexpr std::array<std::int16_t,6> effectiveOptions{0,0,1,1,0,255};
        constexpr std::uint16_t fullView=82;
        auto loaded = assets_.cockpitSettings->options;
        auto effective = audio_->menuResources_ ? audio_->menuResources_->configuration.options
            : std::array<std::int16_t,6>{};
        if (audio_->playableSoundsProfile() || audio_->playableMusicProfile()) {
            // An absent mouse skips the allocation-free device query on reload.
            if (loaded[0] == 0) { loaded[0] = 1; }
            loaded[3] = 1;
            effective[3] = 1;
            if (audio_->playableMusicProfile()) { loaded[2] = 1; effective[2] = 1; }
            if (audio_->playableDifficultyProfile()) { loaded[4] = 0; effective[4] = 0; }
            if (audio_->playableStoryProfile()) { loaded[5] = 255; effective[5] = 255; }
            if (audio_->playablePointerProfile()) { loaded[0] = 1; effective[0] = 0; }
            if (audio_->playableJoystickProfile()) { loaded[1] = 0; effective[1] = 0; }
        }
        if (!audio_->menuResources_ || loaded!=loadedOptions
            || effective!=effectiveOptions
            || (audio_->menuResources_->configuration.viewSize!=assets_.cockpitSettings->viewSize
                && audio_->menuResources_->configuration.viewSize!=fullView && !audio_->playableDetailProfile())) {
            throw std::invalid_argument("playable adoption requires its owned menu-cache audio profile");
        }
        //This native menu provider builds the same cache with introduction enabled.
        playableMcb=SoundBlasterEnemy20McbByte::fromCache(audio_->menuResources_->cache);
        if (!playableMcb) {
            throw std::invalid_argument("playable adoption lost its menu-cache audio byte");
        }
    }
    const auto bomb7Row = audio_->bomb7CallerRow();
    if (bomb7Row) { synth_.validateBomb7CallerRow(*callback, *bomb7Row); }
    auto radar = *radar_;
    //Translate fresh physical cache identities; retain the existing armor/view/pixels.
    for (std::size_t slot = 0; slot < resetFixedCount; ++slot) {
        radar.initializeAllocation(fixedRadarBase + slot);
    }
    for (std::size_t slot = 0; slot < Level1State::projectilePoolSize; ++slot) {
        radar.initializeAllocation(projectileRadarBase + slot);
    }
    for (std::size_t slot = 0; slot < Level1State::deathEffectPoolSize; ++slot) {
        radar.initializeAllocation(effectRadarBase + slot);
    }
    model_.adoptPlayableLevel1(shared, resources.models);
    if (playableMcb) {
        *audio_->workPhase_=bindLevel1Enemy20McbAudio(synth_,*audio_->workPhase_,*playableMcb);
    }
    // The supported DOS launch owns PSP/MCB filename bytes independently of the host EXE name.
    constexpr std::string_view supportedDosExecutable = "STELLAR7.EXE";
    constexpr SoundBlasterEnemy20McbByte::NativeDosInitialLoadProfile supportedInitialDosLoad{
        0x0824,SoundBlasterEnemy20McbByte::DosLoadPlacement::afterPsp};
    constexpr std::size_t preheaderBitmapGroup=4;
    constexpr std::size_t preheaderBitmapFrame=56;
    const auto processByte = audio_->workPhase_->callback->program->enemy20McbByte->withNativeProcessIdentity(
        supportedDosExecutable, assets_.soundSequences.at(0),
        assets_.artGroups.at(preheaderBitmapGroup).at(preheaderBitmapFrame),supportedInitialDosLoad);
    auto processPhase = *audio_->workPhase_;
    processPhase.callback->program->enemy20McbByte = processByte;
    synth_.configurePlayableEnemy20Process(*audio_->workPhase_->callback, processByte);
    *audio_->workPhase_ = std::move(processPhase);
    if (bomb7Row) {
        synth_.configureBomb7CallerRow(*audio_->workPhase_->callback, *bomb7Row);
        audio_->workPhase_->callback->program->bomb7CallerRow = *bomb7Row;
    }
    radar_ = std::move(radar);
    campaignResources_ = CampaignResourceBorrow{0, &resources.bitmapGroups, &resources.models, &resources.musicSequence};
    initialActorCount_ = model_.level2PresentationEntities().size();
    initialPoolSlots_.fill(static_cast<std::size_t>(-1));
    initialActorTypes_.fill(-1);
    actorIncarnations_.fill(0);
    actorTypes_.fill(0);
    for (std::size_t index = 0; index < initialActorCount_; ++index) {
        const auto slot = *model_.level2PresentationPoolSlot(index);
        initialPoolSlots_[index] = slot;
        initialActorTypes_[index] = model_.level2PresentationEntities()[index].type;
        if (slot < ordinaryPoolSize_) {
            actorIncarnations_[slot] = *model_.level2PresentationActorIncarnation(index);
            actorTypes_[slot] = initialActorTypes_[index];
        }
    }
    level1Pages_ = false;
    playableCampaign_ = true;
    audio_->workPhase_->playableCampaign = true;
    recoveredController_.reset();
    level2Cursor_ = model_.level2AudioEvents().size();
    pageProjection_ = NativePageProjection::retainedPlayablePreloop;
}

} //namespace stellar7
