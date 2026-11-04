#include "stellar7/gameplay.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {

void GameplayModel::bindCampaignModels(const std::uint8_t level,
    const std::vector<Model3d>& shared,const std::vector<Model3d>& local,const CampaignModelBinding binding) {
    std::array<CampaignModelMetadata,49> metadata{};
    constexpr std::size_t localModelBase=44;
    for (std::size_t type=0;type<metadata.size();++type) {
        const Model3d* model=type>=localModelBase && type-localModelBase<local.size()
            ? &local[type-localModelBase] : type<shared.size() ? &shared[type] : nullptr;
        if (!model) {
            continue;
        }
        const auto equalSelectors = model->flags != 0 && model->flags<=retainedAnimationSelectorCapacity && model->animationLimits.size() == model->flags
            && std::all_of(model->animationLimits.begin(), model->animationLimits.end(),
                [&](const auto limit) { return limit == model->animationLimits.front(); });
        metadata[type]={campaignModelRadius(shared,local,static_cast<std::uint16_t>(type)),model->flags,
            equalSelectors ? model->animationLimits.front() : std::uint8_t{0}};
    }
    std::optional<CampaignOrdinaryConstructor> constructor;
    if (binding==CampaignModelBinding::fullPlayable && shared.size()>=localModelBase) {
        const auto definition=campaignLevelDefinition(level,campaignDifficultyIndex(runtimeDifficulty_));
        std::vector<std::uint8_t> types;
        for (const auto& actor:definition.table.actors) {
            if (actor.type<17) {
                types.push_back(actor.type);
            }
        }
        for (std::size_t index=0;index<definition.weights.size();++index) {
            if (definition.weights[index]!=0) {
                types.push_back(static_cast<std::uint8_t>(index+1));
            }
        }
        if (level==1 || level==3) {
            types.push_back(4);
        }
        types.push_back(13); //AE10 always requests the shared Type13 constructor.
        constructor.emplace(level,types,shared,local);
    }
    //39EE(0) owns this record once;48D4 does not reset it.
    if(campaignPlayerProxy_.collisionRadius==0 && metadata[0].radius!=0) {
        campaignPlayerProxy_.collisionRadius=metadata[0].radius;
        campaignPlayerProxy_.renderProfile=WorldActorRenderProfile::noHandles;
        campaignPlayerProxy_.animatedModelPresenterEnabled=false;
    }
    campaignModels_=metadata;
    campaignModelsBound_=!shared.empty();
    playableOrdinaryConstructor_=std::move(constructor);
}

void GameplayModel::preparePlayableCampaignModels(const std::vector<Model3d>& shared,
    const std::vector<Model3d>& local) {
    constexpr std::size_t sharedModelCount=44;
    const bool combat=phase_==GameplayPhase::level2PreloopReturn || phase_==GameplayPhase::level2
        || phase_==GameplayPhase::level3PreloopReturn || phase_==GameplayPhase::level3Native
        || phase_==GameplayPhase::level5;
    if (!combat || playableCampaignFrames_ || activeCampaignLevel_>=7 || shared.size()<sharedModelCount
        || level2PlayerArmor_<0 || level2Complete_ || (level2HasAdvanced_ && !level2TailConsumed_)) {
        throw std::invalid_argument("playable model preparation requires its completed source combat boundary");
    }
    bindCampaignModels(static_cast<std::uint8_t>(activeCampaignLevel_),shared,local);
}

void GameplayModel::completeLevel3Load(const std::vector<Model3d>& sharedModels,
    const std::vector<Model3d>& levelModels) {
    if (phase_ != GameplayPhase::level2SuccessReturn || activeCampaignLevel_ != 1
        || !level2TailConsumed_ || level2SuccessContinuation_ || !level2Complete_
        || !successAudioOwned_ || successAudioActive_ || !level2SecondaryPickupKnownNull_
        || level2PlayerArmor_ < 0 || sharedModels.empty()
        || campaignModelRadius(sharedModels, levelModels, 0) == 0) {
        throw std::invalid_argument("Level3 loader requires the retained completed Level2 success return");
    }
    auto rng = level2Rng_;
    auto actors = constructLevel3LoaderActors(rng, sharedModels, levelModels);
    initializeRetainedCampaignLevel(2,std::move(actors),rng,sharedModels,levelModels);
}

void GameplayModel::initializeRetainedCampaignLevel(const std::uint8_t destination,
    std::vector<Level1Entity> actors,StellarRng rng,
    const std::vector<Model3d>& sharedModels,const std::vector<Model3d>& levelModels) {
    bindCampaignModels(destination,sharedModels,levelModels);
    const auto ordinaryCount = static_cast<std::size_t>(std::count_if(actors.begin(), actors.end(),
        [](const auto& actor) { return actor.stateBytes4dTo52[0] == 0xff; }));
    constexpr std::int16_t maximumArmor = 680;
    constexpr std::int16_t transitionArmorGain = 80;
    const auto armor = std::min<std::int16_t>(maximumArmor,
        static_cast<std::int16_t>(level2PlayerArmor_ + transitionArmorGain));
    //0428..0475 retire the old level;019F clears132C before4E06/48D4.
    level2Entities_ = std::move(actors);
    initializeCampaignPools(destination, ordinaryCount);
    level3FixedAnimationLimit_ = sharedModels[24].animationLimits.front();
    level2Rng_ = rng;
    level2PlayerArmor_ = armor;
    level2Player_ = {};
    level2Player_.allocationIndex = level2Entities_.size();
    level2Player_.collisionRadius = campaignModelRadius(sharedModels, levelModels, 0);
    level2Player_.renderProfile = WorldActorRenderProfile::noHandles;
    level2Player_.animatedModelPresenterEnabled = false;
    level2Player_.stateBytes4dTo52[1] = 1;
    for (std::size_t slot = ordinaryCount; slot < level2ActorPoolSize_; ++slot) {
        level2ActorPool_[slot].stateBytes4dTo52[0] = 0xff;
    }
    for (auto& projectile : level2ProjectilePool_) {
        projectile.stateBytes4dTo52[0] = 0xfd;
        projectile.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
    }
    for (auto& effect : level2DeathEffectPool_) {
        effect.flags = 0x88;
        effect.stateBytes4dTo52[0] = 0xfc;
    }
    level2PrefixGuards_.reset();
    level2PendingInput_ = {};
    level2HasAdvanced_ = false;
    level2PlayerCollisionMomentum_ = 0;
    level2Type46ContactSlowdown_ = false;
    level2ActiveEnemyCount_ = static_cast<std::int16_t>(ordinaryCount);
    level2SpawnedEnemyCount_ = static_cast<std::int16_t>(ordinaryCount);
    level2CompletionObjectIndex_ = static_cast<std::size_t>(-1);
    level2PostWaveObjectIndex_ = static_cast<std::size_t>(-1);
    level2PickupIndex_ = static_cast<std::size_t>(-1);
    level2Complete_ = false;
    level3CompletionByPickup_ = false;
    successAudioOwned_ = false;
    successAudioActive_ = false;
    level5Constructor_.reset();
    level5GuardianRadius_ = 0;
    level5GuardianAnimationLimit_ = 0;
    level1_.resetCampaignTransientState();
    level2ActiveListOrder_.clear();
    level2Visibility_.invalidate();
    snapshotLevel2Presentation();
    //50C4 owns two cockpit draws.4C16 ground initialization is after50BE.
    lastCockpitInitialization_.levelIndex = destination;
    lastCockpitInitialization_.displaySteps = 2;
    lastCockpitInitialization_.states[0] = {cockpitIndicatorX_, cockpitIndicatorVelocity_};
    for (std::size_t index = 1; index <= lastCockpitInitialization_.displaySteps; ++index) {
        advanceCockpitIndicator();
        lastCockpitInitialization_.states[index] = {cockpitIndicatorX_, cockpitIndicatorVelocity_};
    }
    lastCockpitInitialization_.states.back() = lastCockpitInitialization_.states[2];
    phase_ = destination==2 ? GameplayPhase::level3LoaderReturn : GameplayPhase::playableLoaderReturn;
    presentationFrame_ = 0;
    ++ticks_;
}

std::uint8_t GameplayModel::pendingPlayableCampaignDestination() const {
    if (!playableCampaignFrames_ || phase_!=GameplayPhase::level2SuccessReturn
        || !level2TailConsumed_ || !level2Complete_ || successAudioActive_
        || !successAudioOwned_ || level2SuccessContinuation_) {
        throw std::invalid_argument("playable destination requires its completed retained success return");
    }
    return campaignExitDestination(static_cast<std::uint8_t>(activeCampaignLevel_),playableExit_);
}

void GameplayModel::completePlayableCampaignLoad(const std::uint8_t destination,
    const std::vector<Model3d>& shared,const std::vector<Model3d>& local) {
    if (destination!=pendingPlayableCampaignDestination() || destination>=7
        || level2PlayerArmor_<0 || campaignModelRadius(shared,local,0)==0) {
        throw std::invalid_argument("playable loader requires its actual campaign destination and model banks");
    }
    auto rng=level2Rng_;
    auto actors=constructCampaignLoaderActors(campaignLevelDefinition(destination,campaignDifficultyIndex(runtimeDifficulty_)).table,
        rng,shared,local);
    initializeRetainedCampaignLevel(destination,std::move(actors),rng,shared,local);
}

} // namespace stellar7
