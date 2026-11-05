#include "stellar7/level2_periodic_tail_work.hpp"
#include "stellar7/gameplay.hpp"
#include "gameplay_render_support.hpp"
#include "heap_animation.hpp"
#include "campaign_contact.hpp"
#include "campaign_allocation.hpp"
#include "playable_jump.hpp"
#include "playable_decoy.hpp"
#include "playable_bomb.hpp"
#include "failure_page.hpp"
#include "playable_detail.hpp"
#include "failure_effect_age.hpp"
#include "stellar7/level2_preloop_prefix_work.hpp"
#include "stellar7/completion_beam.hpp"
#include "stellar7/guardian_deployment_work.hpp"
#include "stellar7/campaign_loader.hpp"
#include "stellar7/render_work.hpp"
#include "stellar7/world_model_work.hpp"
#include "stellar7/palette_clock.hpp"
#include "stellar7/indexed_render.hpp"
#include "ordinary_behavior.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace stellar7 {
namespace {
// Observe a completed update without adding simulation work or audio events.
struct ArmorLossObservation {
    CombatFeedback& feedback;
    const std::int16_t& armor;
    const std::uint64_t& tick;
    std::size_t level;
    std::int16_t before;
    ~ArmorLossObservation() {
        if(armor<before) {feedback.damage=CombatCue{tick,level,before-armor};}
    }
};

constexpr std::uint8_t supportedCampaignDifficulty = 0;
constexpr std::array<std::size_t,6> abilityTimerIndices{0,1,2,4,5,6};

bool nativeInputRequested(const GameplayInput& input) {
    return input.systemAction!=GameplaySystemAction::none || input.joystick || input.analog
        || input.joystickAction!=GameplaySystemAction::none || input.sourceKey!=0;
}

bool hasActiveAbilityTimer(const std::array<std::int16_t,Level1State::systemCount>& timers) {
    return std::any_of(abilityTimerIndices.begin(),abilityTimerIndices.end(),
        [&](const auto index) { return timers[index]!=0; });
}

constexpr std::uint8_t level5GuardianAnimationLimit = 6;
constexpr std::uint32_t level5GuardianRadius = 1508;
constexpr std::uint8_t level5GuardianHealth = 50;

bool sameFirstLevel2Actor(const Level1Entity& left, const Level1Entity& right) {
    const auto fields = [](const Level1Entity& actor) {
        return std::tie(actor.allocationIndex, actor.flags, actor.type, actor.collisionRadius,
            actor.position.x, actor.position.y, actor.position.z, actor.pitch, actor.roll, actor.yaw,
            actor.pitchVelocity, actor.rollVelocity, actor.yawVelocity, actor.xVelocity, actor.yVelocity,
            actor.zVelocity, actor.forwardVelocity, actor.stateBytes4dTo52, actor.modelAnimationVariant,
            actor.primaryType11AnimationSelector, actor.behaviorTimer, actor.type12MissGate,actor.type6CloakTimer,
            actor.presenterDistance, actor.targetHeading, actor.weaponSlotCount, actor.weaponCooldown,
            actor.weaponSlotTimers, actor.collisionContactAllocationIndex, actor.collisionContactIsPlayer,
            actor.presentationModelAnimationVariant, actor.animatedModelPresenterEnabled, actor.ownerIsPlayer,
            actor.ownerAllocationIndex, actor.renderProfile,actor.modelAnimationTail,actor.presentationModelAnimationTail);
    };
    return fields(left) == fields(right);
}


void validateLevel5GuardianModel(const std::vector<Model3d>& models) {
    if (models.empty()) {
        return; // Ordinary-only component fixtures cannot deploy a guardian.
    }
    const auto& model = models.front();
    if (models.size() != 1 || model.collisionRadius != level5GuardianRadius
        || model.flags != 1 || model.animationLimits != std::vector<std::uint8_t>{level5GuardianAnimationLimit}) {
        throw std::invalid_argument("Level5 guardian requires actual local model/CFEC metadata");
    }
}

CampaignOrdinaryConstructor level5OrdinaryConstructor(const std::vector<Model3d>& models) {
    constexpr std::uint8_t levelIndex = 4;
    const auto definition = campaignLevelDefinition(levelIndex, supportedCampaignDifficulty);
    std::vector<std::uint8_t> types;
    for (std::size_t index = 0; index < definition.weights.size(); ++index) {
        if (definition.weights[index] != 0) {
            types.push_back(static_cast<std::uint8_t>(index + 1));
        }
    }
    return CampaignOrdinaryConstructor(levelIndex, types, models, {});
}

bool isCampaignReturnPhase(const GameplayPhase phase) noexcept {
    return phase == GameplayPhase::level2LoaderReturn
        || phase == GameplayPhase::level2MusicReturn || phase == GameplayPhase::level2PreloopReturn
        || phase == GameplayPhase::level2SuccessReturn
        || phase == GameplayPhase::level3LoaderReturn
        || phase == GameplayPhase::level3PreloopReturn
        || phase == GameplayPhase::playableLoaderReturn || phase == GameplayPhase::playablePreloopReturn
        || phase == GameplayPhase::level5SuccessReturn || phase == GameplayPhase::endingEntry
        || phase == GameplayPhase::endingPresentation || phase == GameplayPhase::endingTeardown
        || phase == GameplayPhase::endingReturn || phase == GameplayPhase::nativeMenu
        || phase == GameplayPhase::nativeBeginWait;
}

void advanceSourcePickupSelector(Level1Entity& pickup) noexcept {
    auto& selector = pickup.stateBytes4dTo52[2];
    auto& direction = pickup.stateBytes4dTo52[5];
    if (static_cast<std::int8_t>(selector) <= 0) {
        direction = 1;
    } else if (static_cast<std::int8_t>(selector) > 2) {
        direction = 0xff;
    }
    selector = static_cast<std::uint8_t>(selector + static_cast<std::int8_t>(direction));
}

std::int16_t arithmeticShiftRightOne(const std::int16_t value) {
    if (value >= 0) {
        return static_cast<std::int16_t>(value / 2);
    }
    return static_cast<std::int16_t>(-((-value + 1) / 2));
}

std::uint16_t absoluteSignedWord(const std::uint16_t value) {
    const auto signedValue = static_cast<std::int16_t>(value);
    if (signedValue >= 0) {
        return static_cast<std::uint16_t>(signedValue);
    }
    return static_cast<std::uint16_t>(
        -static_cast<std::int32_t>(signedValue)
    );
}

std::uint32_t failureDebrisRadius(const std::int16_t type) {
    constexpr std::array<std::uint32_t, 5> radii{
        171, 107, 88, 107, 157,
    };
    return radii[static_cast<std::size_t>(type - 36)];
}

template <std::size_t count>
void initializeCampaignPlacement(std::array<Level1Entity, count>& entities, const std::uint8_t level) {
    constexpr std::uint8_t animatedFixedType = 24;
    constexpr std::uint8_t levelFixedType = 29;
    constexpr std::uint8_t companionType = 31;
    const auto definition = campaignLevelDefinition(level, supportedCampaignDifficulty);
    std::size_t index = 0;
    const auto append = [&entities, &index](const CampaignInitialActor& entry, const std::uint8_t type,
        const std::uint16_t yaw) {
        if (index >= entities.size()) {
            throw std::logic_error("campaign fixture allocation count mismatch");
        }
        auto& entity = entities[index];
        entity.allocationIndex = index++;
        entity.type = type;
        entity.position = {campaignCoordinate(entry.x), campaignCoordinate(entry.y), 0};
        entity.yaw = yaw;
    };
    for (const auto& entry : definition.table.actors) {
        if (entry.type == animatedFixedType) {
            append(entry, animatedFixedType, 0);
            append(entry, companionType, 0);
        } else if (entry.type == levelFixedType) {
            append(entry, static_cast<std::uint8_t>(levelFixedType + level), 0);
        } else {
            append(entry, entry.type, entry.yaw);
        }
    }
    if (index != entities.size()) {
        throw std::logic_error("campaign fixture allocation count mismatch");
    }
}

const std::array<Level1Entity, 7>& level2InitialEntities() {
    // Recovered at the first Level 2 post-display boundary. These are the
    // seven actors installed by the level loader before loop tick zero.
    static const auto entities = [] {
        std::array<Level1Entity, 7> result{};
        initializeCampaignPlacement(result, 1);
        const auto initialize = [&result](
            const std::size_t index,
            const std::uint32_t collisionRadius,
            const std::uint8_t category,
            const std::uint8_t activeState
        ) {
            auto& entity = result[index];
            entity.collisionRadius = collisionRadius;
            // Ordinary/static pool reset clears both presenter handles.
            entity.renderProfile = WorldActorRenderProfile::noHandles;
            entity.stateBytes4dTo52 = {
                category, activeState, 0, 0,
                static_cast<std::uint8_t>(entity.type == 8 ? 2 : entity.type == 13 ? 4 : 0),
                0,
            };
        };
        initialize(0, 860, 0xff, 2);
        initialize(1, 860, 0xff, 2);
        initialize(2, 860, 0xff, 2);
        initialize(3, 559, 0xff, 1);
        initialize(4, 559, 0xff, 1);
        initialize(5, 868, 0xfe, 100);
        initialize(6, 868, 0xfe, 100);
        result[0].weaponSlotCount = 2;
        result[1].weaponSlotCount = 2;
        result[2].weaponSlotCount = 2;
        result[3].yawVelocity = 2048;
        result[4].yawVelocity = -2048;
        return result;
    }();
    return entities;
}

const std::array<Level1Entity, 6>& level3InitialEntities() {
    // Exact SANCTUARY loop-zero allocation order from the natural Level 2
    // success route. Types 24 and 31 are overlapping fixed presenter records;
    // the first four entries occupy actor-pool slots.
    static const auto entities = [] {
        std::array<Level1Entity, 6> result{};
        initializeCampaignPlacement(result, 2);
        const auto initialize = [&result](
            const std::size_t index,
            const std::uint32_t collisionRadius,
            const std::int16_t yawVelocity,
            const std::int16_t forwardVelocity,
            const std::array<std::uint8_t, 6> state,
            const std::uint8_t weaponSlots
        ) {
            auto& entity = result[index];
            entity.collisionRadius = collisionRadius;
            entity.yawVelocity = yawVelocity;
            entity.forwardVelocity = forwardVelocity;
            entity.stateBytes4dTo52 = state;
            entity.weaponSlotCount = weaponSlots;
        };
        initialize(
            0, 0xfffff000U,
            -1792, 0, {255, 1, 0, 0, 2, 0}, 3
        );
        initialize(
            1, 0xfffff000U,
            -2304, 0, {255, 1, 0, 0, 2, 0}, 3
        );
        initialize(
            2, 0xfffff000U,
            2048, 0, {255, 1, 0, 0, 2, 0}, 3
        );
        initialize(
            3, 0,
            0, 144, {255, 2, 0, 0, 1, 0}, 1
        );
        initialize(
            4, 0,
            0, 0, {252, 100, 2, 0, 0, 0}, 0
        );
        initialize(
            5, 0,
            0, 0, {254, 100, 0, 0, 0, 0}, 0
        );
        return result;
    }();
    return entities;
}

}  // namespace

void GameplayModel::spawnFailureBurst(const unsigned frame) noexcept {
    auto boundedRandom = [this](const std::uint16_t mask) {
        return static_cast<std::int32_t>(failureRng_.next() & mask)
            - static_cast<std::int32_t>(mask >> 1);
    };
    failureBursts_.push_back({
        {
            boundedRandom(0x03ff),
            boundedRandom(0x03ff),
            boundedRandom(0x01ff),
        },
        frame,
    });
}

void GameplayModel::rebuildFailureVisibleItems(
    const unsigned frame
) noexcept {
    failureVisibleItems_.clear();
    if (failureFromLevel2_) {
        for (std::size_t index = 0;
             index < failureMissionEntities_.size(); ++index) {
            const auto owner = failureMissionEntities_[index].allocationIndex;
            if (playableCampaignFrames_ && (owner >= level2Entities_.size()
                || !level2Entities_[owner].active())) {
                continue;
            }
            failureVisibleItems_.push_back({
                FailureVisibleKind::mission,
                index,
            });
        }
    } else {
        const auto& entities = level1_.entities();
        for (std::size_t index = 0; index < entities.size(); ++index) {
            if (entities[index].active()) {
                failureVisibleItems_.push_back({
                    FailureVisibleKind::mission,
                    index,
                });
            }
        }
    }
    if (frame <= 2) {
        failureVisibleItems_.push_back({FailureVisibleKind::player, 0});
    }
    if (frame != 0) {
        for (std::size_t index = 0; index < failureDebris_.size(); ++index) {
            failureVisibleItems_.push_back({
                FailureVisibleKind::debris,
                index,
            });
        }
    }
    for (std::size_t index = 0; index < failureBursts_.size(); ++index) {
        const auto& burst = failureBursts_[index];
        if (frame > burst.spawnFrame && frame - burst.spawnFrame <= 3) {
            failureVisibleItems_.push_back({
                FailureVisibleKind::burst,
                index,
            });
        }
    }

    const auto position = [this](const FailureVisibleItem& item) {
        switch (item.kind) {
        case FailureVisibleKind::mission:
            return (
                failureFromLevel2_
                ? failureMissionEntities_[item.index].position
                : level1_.entities()[item.index].position
            );
        case FailureVisibleKind::player:
            return failurePlayer_.position;
        case FailureVisibleKind::debris:
            return failureDebris_[item.index].position;
        case FailureVisibleKind::burst:
            return failureBursts_[item.index].position;
        }
        return FixedPosition{};
    };
    // FUN_1000_14b0 selection-sorts the pre-update visible list by the
    // FUN_1000_13de planar-distance approximation, farthest first.
    for (std::size_t first = 0;
         first < failureVisibleItems_.size(); ++first) {
        auto farthest = first;
        auto farthestDistance = approximatePlanarDistance(
            failureView_.origin,
            position(failureVisibleItems_[first])
        );
        for (std::size_t candidate = first + 1;
             candidate < failureVisibleItems_.size(); ++candidate) {
            const auto candidateDistance = approximatePlanarDistance(
                failureView_.origin,
                position(failureVisibleItems_[candidate])
            );
            if (candidateDistance > farthestDistance) {
                farthest = candidate;
                farthestDistance = candidateDistance;
            }
        }
        if (farthest != first) {
            std::swap(
                failureVisibleItems_[first],
                failureVisibleItems_[farthest]
            );
        }
    }
}

GameplayModel::GameplayModel() noexcept : GameplayModel(InitialDisplay::firstOrdinary) {}

GameplayModel::GameplayModel(const InitialDisplay display, const std::optional<std::int16_t> nativeDetail) noexcept
    : playableCockpitIndicatorEnabled_(!nativeDetail || *nativeDetail > nativeDetailIndicatorThreshold),
      playableDetailBound_(nativeDetail.has_value()) {
    if (display == InitialDisplay::loaderOnly) {
        // Reuse the constructor provider, not the legacy post-display literals.
        level1_.reloadAfterFailure(StellarRng{});
        level1_.objectListOrder_.clear();
        level1_.activeListOrder_.clear();
        level1_.semanticActiveListOrder_.clear();
        level1_.nativeInitialListPending_ = true;
    }
    prepareCockpitInitialDisplay(display);
}

GameplayModel GameplayModel::fromNativeStartup() noexcept {
    return GameplayModel(InitialDisplay::loaderOnly);
}

GameplayModel GameplayModel::fromNativeStartup(const std::int16_t detail) {
    if (!nativeFullDetailSettingSupported(detail)) {
        throw std::invalid_argument("unsupported native startup detail");
    }
    return GameplayModel(InitialDisplay::loaderOnly, detail);
}

void GameplayModel::synchronizePlayableDetail(const std::int16_t detail) {
    if (!playableDetailBound_ || !playableCampaignFrames_ || !nativeFullDetailSettingSupported(detail)) {
        throw std::invalid_argument("graphic detail requires the retained playable owner");
    }
    playableCockpitIndicatorEnabled_ = detail > nativeDetailIndicatorThreshold;
}

PlayableModelDetail GameplayModel::playableModelDetail(const std::int16_t type) const noexcept {
    constexpr std::int16_t sharedModelCount = 44;
    return type >= 0 && type < sharedModelCount ? playableSharedModelDetail_ : PlayableModelDetail{};
}

std::array<CockpitIndicatorState, 3> GameplayModel::resumePlayablePreferences(const std::int16_t detail) {
    if (!playableDetailBound_ || !playableCampaignFrames_ || (phase_ != GameplayPhase::playable && phase_ != GameplayPhase::level2
        && phase_ != GameplayPhase::level3Native && phase_ != GameplayPhase::level5)
        || !nativeFullDetailSettingSupported(detail)
        || std::any_of(level1_.systemStatusLevels_.begin(), level1_.systemStatusLevels_.end(),
            [](const auto status) { return status == -128; })) {
        throw std::invalid_argument("preferences resume requires its ordinary retained playable owner");
    }
    synchronizePlayableDetail(detail);
    //7CF1/8AFC updates descriptor0; local-bank descriptor1 remains at its own256/256.
    playableSharedModelDetail_ = nativeModelDetail(detail);
    std::array<CockpitIndicatorState, 3> states{};
    states[0] = {cockpitIndicatorX_, cockpitIndicatorVelocity_};
    for (std::size_t index = 1; index < states.size(); ++index) {
        advanceCockpitIndicator(); //50C4's two source6D46 calls.
        states[index] = {cockpitIndicatorX_, cockpitIndicatorVelocity_};
    }
    if (level2HasAdvanced_ && !playableRadarAnimationSettled_) {
        advanceLevel2GuardianAnimation();
        playableRadarAnimationSettled_ = true;
    }
    for (const auto index : level2ActiveListOrder_) {
        auto& actor = level2Entities_.at(index);
        if (!actor.animatedModelPresenterEnabled || actor.renderProfile != WorldActorRenderProfile::heapSelectorDeferredAnimation
            || !campaignModelsBound_ || actor.type < 0 || static_cast<std::size_t>(actor.type) >= campaignModels_.size()) {
            continue;
        }
        const auto limit = campaignModels_[actor.type].animationLimit;
        if (limit != 0) { advanceHeapAnimation(actor, limit,campaignModels_[actor.type].selectors); }
    }
    syncPlayableCampaignPools();
    //8F1C/8ED2 normalizes charge signs without resetting timers or abilities.
    constexpr std::uint16_t selectionEvent = 16;
    for (std::size_t system = 0; system < level1_.systemStatusLevels_.size(); ++system) {
        auto& status = level1_.systemStatusLevels_[system];
        if (status < 0) { status = static_cast<std::int8_t>(-status); }
        if (level1_.selectedSystem_ != static_cast<std::int16_t>(system) || status != 0) {
            continue;
        }
        //8ED2 calls8D14(-1): select the next currently positive charge, in source order.
        level2AudioEvents_.push_back({level2LoopTick_, selectionEvent});
        level1_.selectedSystem_ = -1;
        auto candidate = system;
        for (std::size_t count = 0; count < level1_.systemStatusLevels_.size(); ++count) {
            candidate = (candidate + 1) % level1_.systemStatusLevels_.size();
            if (level1_.systemStatusLevels_[candidate] > 0) {
                level1_.selectedSystem_ = static_cast<std::int16_t>(candidate);
                break;
            }
        }
    }
    return states;
}

std::vector<PlayablePreferencesRadarActor> GameplayModel::playablePreferencesRadarActors() const {
    if (!playableDetailBound_ || !playableCampaignFrames_) {
        throw std::invalid_argument("preferences radar requires its retained native playable OLD list");
    }
    std::vector<PlayablePreferencesRadarActor> result;
    for (const auto index : level2ActiveListOrder_) {
        const auto slot = liveLevel2PoolSlot(index);
        if (!slot) { throw std::logic_error("preferences radar lost an OLD physical identity"); }
        result.push_back({*slot, level2Entities_.at(index), *slot < level2ActorPoolSize_
            ? std::optional<std::uint64_t>{level2ActorIncarnations_[*slot]} : std::nullopt});
    }
    return result;
}

void GameplayModel::advanceCockpitIndicator() noexcept {
    if (!playableCockpitIndicatorEnabled_) {
        return;
    }
    constexpr std::int16_t lowerBound = 132;
    constexpr std::int16_t upperBound = 176;
    constexpr std::int16_t speed = 4;
    if (cockpitIndicatorX_ < lowerBound) {
        cockpitIndicatorVelocity_ = speed;
    } else if (cockpitIndicatorX_ > upperBound) {
        cockpitIndicatorVelocity_ = -speed;
    }
    cockpitIndicatorX_ = static_cast<std::int16_t>(cockpitIndicatorX_ + cockpitIndicatorVelocity_);
}

void GameplayModel::prepareCockpitInitialDisplay(const InitialDisplay display) noexcept {
    // Record the existing50c4 calls and first ordinary draw for pixel replay.
    lastCockpitInitialization_.displaySteps = display == InitialDisplay::loaderOnly ? 2 : 3;
    lastCockpitInitialization_.levelIndex = phase_ == GameplayPhase::level1
        ? 0 : static_cast<std::uint8_t>(activeCampaignLevel_);
    lastCockpitInitialization_.states[0] = {cockpitIndicatorX_, cockpitIndicatorVelocity_};
    for (std::size_t index = 1; index <= lastCockpitInitialization_.displaySteps; ++index) {
        if (index + 1 == lastCockpitInitialization_.states.size()
            && phase_ == GameplayPhase::successPresentation && activeCampaignLevel_ == 1) {
            // Source50c4's two cache draws precede4c16 construction and first4c46.
            level2Ground_.advance(0, 0, level2Rng_);
            level2Ground_.advance(0, 0, level2Rng_);
        }
        advanceCockpitIndicator();
        lastCockpitInitialization_.states[index] = {cockpitIndicatorX_, cockpitIndicatorVelocity_};
    }
    lastCockpitInitialization_.states.back() = lastCockpitInitialization_.states[lastCockpitInitialization_.displaySteps];
    if (activeCampaignLevel_ == 1 && !level2Entities_.empty()) {
        refreshLevel2Visibility();
    }
}

void GameplayModel::initializeSyntheticCampaignIndicator() noexcept {
    // Direct-entry diagnostics bypass the preceding campaign call history.
    cockpitIndicatorX_ = 142;
    cockpitIndicatorVelocity_ = -4;
    if (activeCampaignLevel_ == 1) {
        refreshLevel2Visibility();
    }
}

void GameplayModel::initializeFailurePresentation() noexcept {
    if (!level2FailureTailFinished_ && !level1FailureTailFinished_) {
        failureSourceLoopTick_.reset();
        if (failureFromLevel2_ && (activeCampaignLevel_ == 1 || activeCampaignLevel_ == 4)
            && level2HasAdvanced_) {
            //03d1 belongs to the lethal ordinary tail; an owned tail already applied it.
            failureSourceLoopTick_ = static_cast<std::uint16_t>(
                level2LoopTick_ + (activeCampaignLevel_ == 4 || level2TailConsumed_ ? 0 : 1));
        }
    }
    if (failureSourceLoopTick_) {
        ++*failureSourceLoopTick_; //7307 enters destruction display0.
    }
    if (failureFromLevel2_ && (playableCampaignFrames_ || activeCampaignLevel_ == 4)) {
        //The last ordinary698a ran before03d1; settle its deferred130c once.
        if (!playableRadarAnimationSettled_) { advanceLevel2GuardianAnimation(); }
        playableRadarAnimationSettled_ = false;
    }
    // Shared presenter 1000:72ec calls the common sound dispatcher with
    // event 6 on local frame zero and on each of the next seven frames.
    failureAudioEvents_.clear();
    failureAudioEvents_.push_back({
        static_cast<std::uint16_t>(ticks_),
        6,
    });
    failurePlayer_ = failureFromLevel2_ ? level2Player_ : level1_.player();
    failurePlayer_.allocationIndex = 0;
    failurePlayer_.flags = 0;
    failurePlayer_.type = 0;
    failurePlayer_.collisionRadius = 723;
    failurePlayer_.position = {};
    if (!failureFromLevel2_ && !level1FailureTailFinished_) {
        failurePlayer_.pitch = 0;
        failurePlayer_.roll = 0;
        failurePlayer_.yaw = 0;
    }
    failurePlayer_.pitchVelocity = 0;
    failurePlayer_.rollVelocity = 0;
    failurePlayer_.yawVelocity = 0;
    failurePlayer_.xVelocity = 0;
    failurePlayer_.yVelocity = 0;
    failurePlayer_.zVelocity = 0;
    failurePlayer_.forwardVelocity = 0;
    failurePlayer_.stateBytes4dTo52 = {0, 1, 0, 0, 0, 0};

    auto failureCamera = failurePlayer_;
    failureCamera.yaw = static_cast<std::uint16_t>(
        failureCamera.yaw + 0x4000U
    );
    failureCamera.forwardVelocity = -1800;
    advanceEntityKinematics(failureCamera);
    failureView_ = {
        failureCamera.position,
        makeFixedMatrixQ14(
            failureCamera.pitch,
            failureCamera.roll,
            failureCamera.yaw
        ),
        {0, 318, 0, 199},
        128,
    };
    transpose(failureView_.matrix);

    failureMissionEntities_.clear();
    if (failureFromLevel2_) {
        failureMissionEntities_.reserve(level2Entities_.size());
        for (std::size_t index = 0; index < level2Entities_.size(); ++index) {
            auto& source = level2Entities_[index];
            //78c8 retires every FD record in the current presenter list.
            if (source.stateBytes4dTo52[0] == 0xfd) {
                source.stateBytes4dTo52[1] = 0;
                const auto slot = level2EntityProjectilePoolSlots_[index];
                if (slot < level2ProjectilePoolSize_) {
                    level2ProjectilePool_[slot] = source;
                    level2ProjectilePoolActive_[slot] = false;
                }
                continue;
            }
            if (playableCampaignFrames_ && !source.active()) {
                continue;
            }
            auto entity = source;
            entity.position.x -= level2Player_.position.x;
            entity.position.y -= level2Player_.position.y;
            entity.position.z -= level2Player_.position.z;
            failureMissionEntities_.push_back(entity);
        }
        std::stable_sort(
            failureMissionEntities_.begin(),
            failureMissionEntities_.end(),
            [this](const Level1Entity& left, const Level1Entity& right) {
                return approximatePlanarDistance(
                    failureView_.origin,
                    left.position
                ) > approximatePlanarDistance(
                    failureView_.origin,
                    right.position
                );
            }
        );
    }

    failureRng_ = failureFromLevel2_ ? level2Rng_ : level1_.rng();
    failureDebris_.clear();
    failureDebris_.reserve(failureDebrisCount);
    failureBursts_.clear();
    failureBursts_.reserve(8);
    for (
        std::size_t index = 0;
        index < failureDebrisCount;
        ++index
    ) {
        const auto typeChoice = static_cast<std::int16_t>(
            static_cast<std::int16_t>(failureRng_.next()) % 5
        );
        Level1Entity debris{};
        debris.allocationIndex = index;
        debris.type = static_cast<std::int16_t>(
            36 + std::abs(typeChoice)
        );
        debris.collisionRadius = failureDebrisRadius(debris.type);
        debris.position = {};
        debris.pitch = absoluteSignedWord(failureRng_.next());
        debris.roll = failureRng_.next();
        debris.yaw = failureRng_.next();
        debris.forwardVelocity = static_cast<std::int16_t>(
            (failureRng_.next() & 0x3f) + 0x14
        );
        debris.stateBytes4dTo52 = {0, 1, 0, 0, 0, 0};
        advanceEntityKinematics(debris);
        debris.xVelocity = static_cast<std::int16_t>(
            debris.position.x
        );
        debris.yVelocity = static_cast<std::int16_t>(
            debris.position.y
        );
        debris.zVelocity = static_cast<std::int16_t>(
            debris.position.z
        );
        debris.pitchVelocity = static_cast<std::int16_t>(
            failureRng_.next()
        );
        debris.rollVelocity = static_cast<std::int16_t>(
            failureRng_.next()
        );
        debris.yawVelocity = static_cast<std::int16_t>(
            failureRng_.next()
        );
        debris.forwardVelocity = 0;
        failureDebris_.push_back(debris);
    }
    spawnFailureBurst(0);
    rebuildFailureVisibleItems(0);
    advanceFailureMissionAnimation();
}

void GameplayModel::advanceFailureMissionAnimation() noexcept {
    if (level1FailureTailFinished_ && !failureFromLevel2_) {
        for (const auto& item : failureVisibleItems_) {
            if (item.kind != FailureVisibleKind::mission) {
                continue;
            }
            auto& entity = level1_.entities_[item.index];
            if (entity.type != 11 || !entity.animatedModelPresenterEnabled) {
                continue;
            }
            entity.presentationModelAnimationVariant = entity.modelAnimationVariant;
            advanceHeapAnimation(entity, type11AnimationLimit);
            if (entity.primaryType11AnimationSelector) {
                level1_.type11AnimationVariant_ = entity.modelAnimationVariant;
            }
        }
        return;
    }
    if (playableCampaignFrames_ && failureFromLevel2_) {
        for (const auto& item : failureVisibleItems_) {
            if (item.kind != FailureVisibleKind::mission) {
                continue;
            }
            auto& presentation = failureMissionEntities_[item.index];
            auto& source = level2Entities_[presentation.allocationIndex];
            if (source.type == -1 && source.stateBytes4dTo52[0] == 0xfc
                && source.renderProfile == WorldActorRenderProfile::bitmapEffectCallback) {
                const auto age = detail::failureEffectAge(source.stateBytes4dTo52[1],
                    source.stateBytes4dTo52[4]);
                //0AEE saves the incoming frame; 0AF4 advances even when clipped.
                presentation.stateBytes4dTo52[1] = source.stateBytes4dTo52[1];
                presentation.stateBytes4dTo52[4] = source.stateBytes4dTo52[4];
                source.stateBytes4dTo52[1] = age.retired ? 0 : age.incremented;
                if (age.retired) {
                    source.stateBytes4dTo52[4] = 0;
                    source.renderProfile = WorldActorRenderProfile::noHandles;
                    const auto allocation = std::find(level2AllocationListOrder_.begin(),
                        level2AllocationListOrder_.end(), source.allocationIndex);
                    if (allocation != level2AllocationListOrder_.end()) {
                        *allocation = level2AllocationListOrder_.back();
                        level2AllocationListOrder_.pop_back();
                        level2ObjectListOrder_ = level2AllocationListOrder_;
                        level2Visibility_.removalSucceeded();
                    }
                    const auto slot = level2EntityDeathEffectPoolSlots_[source.allocationIndex];
                    if (slot < level2DeathEffectPoolSize_) {
                        level2DeathEffectPoolActive_[slot] = false;
                    }
                }
                continue;
            }
            if (!source.animatedModelPresenterEnabled
                || source.renderProfile != WorldActorRenderProfile::heapSelectorDeferredAnimation
                || source.type < 0 || static_cast<std::size_t>(source.type) >= campaignModels_.size()) {
                continue;
            }
            const auto limit = campaignModels_[source.type].animationLimit;
            if (limit == 0) {
                continue;
            }
            //72EC has MAIN only. Geometry uses the selector before130C.
            presentation.presentationModelAnimationVariant = source.modelAnimationVariant;
            presentation.presentationModelAnimationTail = source.modelAnimationTail;
            snapshotHeapAnimation(source);
            advanceHeapAnimation(source, limit,campaignModels_[source.type].selectors);
            presentation.modelAnimationVariant = source.modelAnimationVariant;
            presentation.modelAnimationTail = source.modelAnimationTail;
            presentation.stateBytes4dTo52[3] = source.stateBytes4dTo52[3];
        }
        syncPlayableCampaignPools();
        return;
    }
    if (!failureFromLevel2_ || activeCampaignLevel_ != 4) {
        return;
    }
    for (const auto& item : failureVisibleItems_) {
        if (item.kind != FailureVisibleKind::mission) {
            continue;
        }
        auto& entity = failureMissionEntities_[item.index];
        if (entity.type != 11 || !entity.animatedModelPresenterEnabled) {
            continue;
        }
        //B996 selects geometry before130c;72ec has no radar callback.
        entity.presentationModelAnimationVariant = entity.modelAnimationVariant;
        advanceHeapAnimation(entity, type11AnimationLimit);
        auto& source = level2Entities_[entity.allocationIndex];
        source.presentationModelAnimationVariant = entity.presentationModelAnimationVariant;
        source.modelAnimationVariant = entity.modelAnimationVariant;
        source.stateBytes4dTo52[3] = entity.stateBytes4dTo52[3];
    }
}

void GameplayModel::advanceFailurePresentation(
    const unsigned frame
) noexcept {
    if (failureSourceLoopTick_) {
        ++*failureSourceLoopTick_; //7307 enters the next destruction display.
    }
    rebuildFailureVisibleItems(frame);
    advanceFailureMissionAnimation();
    for (auto& debris : failureDebris_) {
        advanceEntityKinematics(debris);
        debris.zVelocity = static_cast<std::int16_t>(
            debris.zVelocity - 5
        );
        if (debris.position.z < -400) {
            debris.position.z = -400;
            debris.zVelocity = static_cast<std::int16_t>(
                -arithmeticShiftRightOne(debris.zVelocity)
            );
            debris.xVelocity = arithmeticShiftRightOne(debris.xVelocity);
            debris.yVelocity = arithmeticShiftRightOne(debris.yVelocity);
            debris.pitchVelocity = arithmeticShiftRightOne(
                debris.pitchVelocity
            );
            debris.rollVelocity = arithmeticShiftRightOne(
                debris.rollVelocity
            );
            debris.yawVelocity = arithmeticShiftRightOne(
                debris.yawVelocity
            );
        }
    }
    if (frame < 8) {
        failureAudioEvents_.push_back({
            static_cast<std::uint16_t>(ticks_),
            6,
        });
        spawnFailureBurst(frame);
    }
}

void GameplayModel::finishFailurePresentation() noexcept {
    //74fc/398a free only the heap debris. Source lists retain stale pointers.
    failureDebris_.clear();
}

void GameplayModel::initializeLevel2(const GameplayInput& input, const Level2EntryOrigin origin) noexcept {
    // Source019f/496e/4971/49b9: fade, effect handles and special actor clear.
    level2PrefixGuards_ = StationaryPrefixGuards{0, false, {false, false}};
    constexpr std::int16_t maximumArmor = 680;
    constexpr std::int16_t transitionArmorGain = 80;
    level2PlayerArmor_ = origin == Level2EntryOrigin::retry ? maximumArmor
        : origin == Level2EntryOrigin::campaign
        ? std::min<std::int16_t>(maximumArmor, static_cast<std::int16_t>(
            level1_.playerArmor() + transitionArmorGain)) : 488;
    level2Player_ = {};
    level2Player_.allocationIndex = 7;
    level2Player_.type = 0;
    level2Player_.collisionRadius = 723;
    level2Player_.stateBytes4dTo52 = {0, 1, 0, 0, 0, 0};
    //49f4 clears the player record;05d4 scans two fixed timer bytes independently.
    level2Player_.weaponSlotCount = 0;
    if (input.left || input.right) {
        level2Player_.yawVelocity = input.left ? 0x50 : -0x50;
    }
    if (input.forward || input.reverse) {
        level2Player_.forwardVelocity = input.reverse ? -10 : 10;
    }
    level2Entities_.assign(
        level2InitialEntities().begin(),
        level2InitialEntities().end()
    );
    constexpr std::size_t initialOrdinaryCount = 5;
    constexpr std::uint8_t ordinaryActorCategory = 0xff;
    constexpr std::uint8_t projectileCategory = 0xfd;
    constexpr std::uint8_t deathEffectCategory = 0xfc;
    constexpr std::uint8_t deathEffectFlags = 0x88;
    initializeCampaignPools(1, origin != Level2EntryOrigin::diagnostic
        ? initialOrdinaryCount : level2Entities_.size());
    if (origin != Level2EntryOrigin::diagnostic) {
        // Source48d4 clears records, then restores their pool categories.
        for (std::size_t slot = initialOrdinaryCount; slot < level2ActorPoolSize_; ++slot) {
            level2ActorPool_[slot].stateBytes4dTo52[0] = ordinaryActorCategory;
        }
        for (auto& projectile : level2ProjectilePool_) {
            projectile.stateBytes4dTo52[0] = projectileCategory;
            projectile.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
        }
        for (auto& effect : level2DeathEffectPool_) {
            effect.flags = deathEffectFlags;
            effect.stateBytes4dTo52[0] = deathEffectCategory;
        }
    } else {
        // Direct diagnostics retain their historical inactive-projectile fields.
        const auto& carriedProjectiles = level1_.projectiles();
        for (std::size_t slot = 0;
             slot < level2ProjectilePool_.size(); ++slot) {
            const auto& source = carriedProjectiles[slot];
            auto& destination = level2ProjectilePool_[slot];
            destination.flags = source.flags;
            destination.type = source.type;
            destination.collisionRadius = source.collisionRadius;
            destination.position = source.position;
            destination.pitch = source.pitch;
            destination.roll = source.roll;
            destination.yaw = source.yaw;
            destination.pitchVelocity = source.pitchVelocity;
            destination.rollVelocity = source.rollVelocity;
            destination.yawVelocity = source.yawVelocity;
            destination.xVelocity = source.xVelocity;
            destination.yVelocity = source.yVelocity;
            destination.zVelocity = source.zVelocity;
            destination.forwardVelocity = source.forwardVelocity;
            std::copy_n(
                source.stateBytes4dTo54.begin(),
                destination.stateBytes4dTo52.size(),
                destination.stateBytes4dTo52.begin()
            );
            destination.presenterDistance = 0;
            destination.targetHeading = 0;
        }
    }
    if (origin != Level2EntryOrigin::diagnostic) {
        level2Rng_ = origin == Level2EntryOrigin::retry ? failureRng_ : level1_.rng();
        constexpr std::int16_t randomYawActorType = 13;
        for (std::size_t index = 0; index < level2Entities_.size(); ++index) {
            auto& entity = level2Entities_[index];
            if (entity.stateBytes4dTo52[0] != ordinaryActorCategory) {
                continue;
            }
            // Source4370 consumes4862 placement even when4e06 replaces XY.
            level2Rng_.next();
            if (entity.type == randomYawActorType) {
                const auto word = level2Rng_.next();
                entity.yawVelocity = static_cast<std::int16_t>(
                    (((word >> 8U) & 3U) + 7U) << 8U);
                if ((word & 1U) != 0U) {
                    entity.yawVelocity = static_cast<std::int16_t>(-entity.yawVelocity);
                }
            }
            // Source4466's heading is likewise replaced by the level table.
            level2Rng_.next();
            level2ActorPool_[level2EntityActorPoolSlots_[index]] = entity;
        }
        level2Ground_ = Level1GroundState(
            std::array<GroundPoint, Level1GroundState::pointCount>{});
    } else {
        level2Ground_ = Level1GroundState({{
            {1344, 4000},
            {-6528, 2336},
            {8320, 4128},
            {5696, 1408},
            {6080, 4000},
            {-3456, 3232},
        }});
        constexpr std::array<std::uint16_t, StellarRng::wordCount>
            level2EntryRngTable{{
                42786, 8292, 49301, 19848, 28390, 39606, 35739, 47117,
                21851, 35043, 25291, 7860, 58490, 12863, 5079, 16780,
                35279, 63562, 28147, 14202, 49937, 31177, 29919, 55132,
                22022, 41649, 35042, 5684, 63591, 12157, 37462, 12795,
                3120, 4437, 13280, 54318, 3378, 3361, 41417, 45540,
                15527, 34184, 9491, 47134, 15669, 10284, 14731, 31650,
                43315, 7697, 55988, 36049, 16419, 53801, 1937, 62833,
            }};
        level2Rng_ = StellarRng(
            level2EntryRngTable,
            62,
            12,
            47117,
            49
        );
    }
    if (origin != Level2EntryOrigin::retry) {
        level2CRandState_ = 1;
        // Native ANTARES checkpoints through loop 1800 retain DS:3988 == 1.
        level2VisualRandomState_ = 1;
    }
    level2PendingInput_ = {};
    level2HasAdvanced_ = false;
    // The exact first ANTARES display carries the phase-one sight. The
    // shared presenter retains this byte until its next eight-tick refresh.
    alternateSightPhase_ = true;
    level2PlayerCollisionMomentum_ = 0;
    level2Type46ContactSlowdown_ = false;
    level2ActiveEnemyCount_ = initialOrdinaryCount;
    level2SpawnedEnemyCount_ = initialOrdinaryCount;
    // Campaign score is cumulative across the level-success presenter.
    level2Score_ = origin == Level2EntryOrigin::retry ? 0 : level1_.score();
    level2CompletionObjectIndex_ = static_cast<std::size_t>(-1);
    level2PostWaveObjectIndex_ = static_cast<std::size_t>(-1);
    level2PickupIndex_ = static_cast<std::size_t>(-1);
    level2Complete_ = false;
    level3CompletionByPickup_ = false;
}

void GameplayModel::initializeLevel3(const std::uint32_t score) noexcept {
    level2PrefixGuards_.reset();
    // Direct-start state remains a diagnostic fixture; campaign RNG is restored by the caller.
    level2PlayerArmor_ = 680;
    level2Player_ = {};
    level2Player_.allocationIndex = 6;
    level2Player_.type = 0;
    level2Player_.collisionRadius = 0;
    level2Player_.stateBytes4dTo52 = {0, 1, 0, 0, 0, 0};
    // The native Level 3 player record leaves its reported weapon-slot count
    // clear.  FUN_1000:05d4 nevertheless scans the two fixed timer bytes at
    // offsets 5b and 5c when the player fires.
    level2Player_.weaponSlotCount = 0;

    level2Entities_.assign(
        level3InitialEntities().begin(),
        level3InitialEntities().end()
    );
    initializeCampaignPools(2, 4);
    level2Visibility_.invalidate();

    level2Ground_ = Level1GroundState({{
        {6016, 1632},
        {2144, 4096},
        {7584, 3872},
        {-2432, 2496},
        {-2624, 3040},
        {-2432, 3904},
    }});
    constexpr std::array<std::uint16_t, StellarRng::wordCount>
        level3EntryRngTable{{
            24375, 38272, 35272, 57974, 30387, 48845, 28255, 2302,
            14486, 3902, 53379, 5786, 40215, 59214, 8031, 25020,
            8460, 53301, 3609, 22661, 57325, 51377, 46053, 40667,
            15239, 30698, 62941, 25852, 24510, 59203, 51809, 58706,
            4336, 32351, 16590, 44153, 46676, 3899, 835, 6172,
            61423, 55519, 23354, 37972, 7303, 52875, 64401, 40909,
            27136, 38025, 25597, 55129, 52140, 35960, 56649, 24968,
        }};
    level2Rng_ = StellarRng(
        level3EntryRngTable,
        32,
        94,
        27136,
        8
    );
    level2CRandState_ = 1;
    level2VisualRandomState_ = 0x187ebcaaU;
    level2PendingInput_ = {};
    level2HasAdvanced_ = true;
    level2PlayerCollisionMomentum_ = 0;
    level2Type46ContactSlowdown_ = false;
    level2ActiveEnemyCount_ = 4;
    level2SpawnedEnemyCount_ = 4;
    level2Score_ = score;
    level2CompletionObjectIndex_ = static_cast<std::size_t>(-1);
    level2PostWaveObjectIndex_ = static_cast<std::size_t>(-1);
    level2PickupIndex_ = static_cast<std::size_t>(-1);
    level2Complete_ = false;
    level3CompletionByPickup_ = false;
}

void GameplayModel::initializeLevel3FromCampaign() noexcept {
    const auto incomingRng = level2Rng_;
    const auto incomingVisualRandom = level2VisualRandomState_;
    const auto incomingCRand = level2CRandState_;
    initializeLevel3(level2Score_);
    level2Rng_ = incomingRng;
    level2VisualRandomState_ = incomingVisualRandom;
    level2CRandState_ = incomingCRand;

    constexpr std::int16_t randomYawActorType = 10;
    // COCKPIT.RES header +4, confirmed by the intact live d03c registry.
    constexpr std::array<std::uint32_t, 6> level3CollisionRadii{
        600, 600, 600, 1043, 2984, 1420
    };
    for (std::size_t index = 0; index < level2Entities_.size(); ++index) {
        auto& entity = level2Entities_[index];
        entity.collisionRadius = level3CollisionRadii[index];
        const auto slot = level2EntityActorPoolSlots_[index];
        if (slot >= level2ActorPoolSize_) {
            continue;
        }
        // 4862 consumes spawn-position randomness even when the level table
        // subsequently replaces that position. 4370 then selects yaw speed.
        level2Rng_.next();
        if (entity.type == randomYawActorType) {
            const auto word = level2Rng_.next();
            entity.yawVelocity = static_cast<std::int16_t>(
                (((word >> 8U) & 3U) + 7U) << 8U
            );
            if ((word & 1U) != 0U) {
                entity.yawVelocity = static_cast<std::int16_t>(-entity.yawVelocity);
            }
        }
        // The random heading at 4466 is likewise replaced by the level table.
        level2Rng_.next();
        level2ActorPool_[slot] = entity;
    }
    // The first 130c callback reloads Type 24's zero animation countdown.
    constexpr std::size_t animatedFixedPresenterIndex = 4;
    auto& fixedPresenter = level2Entities_[animatedFixedPresenterIndex];
    fixedPresenter.stateBytes4dTo52[3] = fixedPresenter.stateBytes4dTo52[2];
    fixedPresenter.modelAnimationVariant = 1;
    fixedPresenter.animatedModelPresenterEnabled = true;
    level2PresentationEntities_ = level2Entities_;

    // 4c16 initializes all six points. The pre-loop display then culls and
    // regenerates invalid points once, using the same continuing RNG stream.
    level2Ground_ = Level1GroundState(
        std::array<GroundPoint, Level1GroundState::pointCount>{}
    );
    level2Ground_.advance(0, 0, level2Rng_);
    level2Ground_.advance(0, 0, level2Rng_);
}

void GameplayModel::completeLevel2Warp(const std::vector<Model3d>& sharedModels,
    const std::vector<Model3d>& levelModels) {
    const auto pickup = std::find_if(level2Entities_.begin(), level2Entities_.end(), [](const auto& actor) {
        return actor.type == 25 && actor.collisionContactIsPlayer;
    });
    if (phase_ != GameplayPhase::level2SuccessPresentation || activeCampaignLevel_ != 1
        || !successAudioOwned_ || successAudioActive_ || pickup == level2Entities_.end()
        || sharedModels.empty() || campaignModelRadius(sharedModels, {}, 0) == 0) {
        throw std::invalid_argument("Level5 warp requires completed native Type25 success");
    }
    const auto incomingArmor = static_cast<std::int16_t>(level2PlayerArmor_ + 80);
    initializeLevel5(sharedModels, level2Rng_, std::min<std::int16_t>(680, incomingArmor), levelModels);
    ++ticks_;
}

void GameplayModel::initializeLevel5(const std::vector<Model3d>& sharedModels,
    StellarRng rng, const std::int16_t armor, const std::vector<Model3d>& levelModels) {
    constexpr std::size_t warpDestination = 4;
    validateLevel5GuardianModel(levelModels);
    auto constructor = level5OrdinaryConstructor(sharedModels);
    // Validate/construct on the continuing RNG before committing the loader state.
    auto actors = constructCampaignOrdinaryActors(campaignLevelDefinition(warpDestination, campaignDifficultyIndex(runtimeDifficulty_)).table,
        rng, sharedModels, {});
    bindCampaignModels(warpDestination,sharedModels,levelModels);
    level2Entities_ = std::move(actors);
    initializeCampaignPools(warpDestination, level2Entities_.size());
    level5TransientStateKnownInactive_ = true;
    level5Constructor_ = std::move(constructor);
    level5GuardianRadius_ = levelModels.empty() ? 0 : levelModels.front().collisionRadius;
    level5GuardianAnimationLimit_ = levelModels.empty() ? 0 : levelModels.front().animationLimits.front();
    level2Rng_ = rng;
    level2PlayerArmor_ = armor;
    level2Player_ = {};
    level2Player_.allocationIndex = level2Entities_.size();
    level2Player_.type = 0;
    level2Player_.collisionRadius = campaignModelRadius(sharedModels, {}, 0);
    level2Player_.renderProfile = WorldActorRenderProfile::noHandles;
    level2Player_.animatedModelPresenterEnabled = false;
    level2Player_.stateBytes4dTo52[1] = 1;
    for (std::size_t slot = level2Entities_.size(); slot < level2ActorPoolSize_; ++slot) {
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
    level2ActiveEnemyCount_ = level2Entities_.size();
    level2SpawnedEnemyCount_ = level2Entities_.size();
    level2CompletionObjectIndex_ = static_cast<std::size_t>(-1);
    level2PostWaveObjectIndex_ = static_cast<std::size_t>(-1);
    level2PickupIndex_ = static_cast<std::size_t>(-1);
    level2Complete_ = false;
    level3CompletionByPickup_ = false;
    successAudioOwned_ = false;
    successAudioActive_ = false;
    level1_.consecutiveDestroyedTypeCount_ = 0; //48d4/49d5 retains the last type.
    level2ActiveListOrder_.clear();
    level2Visibility_.invalidate();
    snapshotLevel2Presentation();
    //50c4 owns exactly two draws. Ground/score/AI are after50be, not loader work.
    lastCockpitInitialization_.levelIndex = warpDestination;
    lastCockpitInitialization_.displaySteps = 2;
    lastCockpitInitialization_.states[0] = {cockpitIndicatorX_, cockpitIndicatorVelocity_};
    for (std::size_t index = 1; index <= lastCockpitInitialization_.displaySteps; ++index) {
        advanceCockpitIndicator();
        lastCockpitInitialization_.states[index] = {cockpitIndicatorX_, cockpitIndicatorVelocity_};
    }
    lastCockpitInitialization_.states.back() = lastCockpitInitialization_.states[2];
    phase_ = GameplayPhase::level5LoaderReturn;
    presentationFrame_ = 0;
}

void GameplayModel::finishLevel5Frame() {
    if (phase_ != GameplayPhase::level5 || activeCampaignLevel_ != 4
        || !level2HasAdvanced_ || level2TailConsumed_ || level2LoopTick_ == 0) {
        throw std::invalid_argument("Level5 tail requires its own ordinary display");
    }
    //The public post-main ordinal is already one ahead. Execute the source tail
    //at its incoming12c8, including the first tick-zero periodic scheduler call.
    --level2LoopTick_;
    consumeLevel2DeferredTail();
    ++level2LoopTick_;
    level2TailConsumed_ = true;
}

void GameplayModel::validateLevel5SuccessReturn(const GameplayInput& input) const {
    if (phase_ != GameplayPhase::level2SuccessPresentation || activeCampaignLevel_ != 4
        || !successAudioOwned_ || !level2Complete_ || !level2HasAdvanced_ || level2TailConsumed_
        || !level5SuccessContinuation_ || !level5TransientStateKnownInactive_
        || input.left || input.right || input.forward || input.reverse || input.fire || input.exit || input.confirm
        || level2Player_.position.z != 0 || level2PlayerArmor_ < 0
        || level2Type46ContactSlowdown_ || level2PlayerCollisionMomentum_ != 0
        || level1_.selectedSystem() != -1
        || std::any_of(level1_.systemTimers().begin(), level1_.systemTimers().end(),
            [](const auto timer) { return timer != 0; })
        || level2Player_.weaponSlotTimers[2] != 0
        || level2Player_.weaponCooldown == 1
        || level2Player_.weaponSlotTimers[0] == 1 || level2Player_.weaponSlotTimers[1] == 1
        || level2Entities_.size() != 1 || level2PickupIndex_ != 0
        || level2ActiveListOrder_ != std::vector<std::size_t>{0}
        || level2AllocationListOrder_ != level2ActiveListOrder_
        || !level2PendingTerminalEffectPresentations_.empty()
        || std::any_of(level2ActorPoolActive_.begin(), level2ActorPoolActive_.end(), [](bool active) { return active; })
        || std::any_of(level2ProjectilePoolActive_.begin(), level2ProjectilePoolActive_.end(), [](bool active) { return active; })
        || std::any_of(level2DeathEffectPoolActive_.begin(), level2DeathEffectPoolActive_.end(), [](bool active) { return active; })
        || std::any_of(level2DeathEffectPoolPendingPresenterRetirement_.begin(),
            level2DeathEffectPoolPendingPresenterRetirement_.end(), [](bool active) { return active; })) {
        throw std::invalid_argument("Level5 success return requires its retained single reward and neutral inactive tail");
    }
    const auto& pause = *level5SuccessContinuation_;
    const auto& reward = level2Entities_.front();
    if (pause.sourceTick != static_cast<std::uint16_t>(level2LoopTick_ - 1)
        || (pause.sourceTick & 15U) == 0 || pause.allocationRevision != level2AllocationRevision_
        || pause.fixedSlot >= level2FixedObjectPoolSize_ || !level2FixedPoolKnown_[pause.fixedSlot]
        || level2EntityFixedPoolSlots_.size() != 1 || level2EntityFixedPoolSlots_.front() != pause.fixedSlot
        || reward.type != 25 || reward.stateBytes4dTo52[0] != 0xfe || !reward.collisionContactIsPlayer) {
        throw std::invalid_argument("Level5 success return changed its paused identity or requires periodic work");
    }
}

void GameplayModel::validateLevel2SuccessReturn(const GameplayInput& input) const {
    if (phase_ != GameplayPhase::level2SuccessPresentation || activeCampaignLevel_ != 1
        || !successAudioOwned_ || successAudioActive_ || !level2Complete_ || !level2HasAdvanced_
        || level2TailConsumed_ || !level2SuccessContinuation_ || !level2SecondaryPickupKnownNull_
        || input.left || input.right || input.forward || input.reverse || input.fire || input.exit || input.confirm
        || level2PlayerArmor_ < 0 || level2PlayerArmor_ > 680 || level2Player_.position.z != 0
        || level2Player_.pitch != 0 || level2Player_.roll != 0 || level2Type46ContactSlowdown_
        || level2PlayerCollisionMomentum_ != 0 || level1_.selectedSystem() != -1
        || level2Player_.weaponCooldown != 0
        || std::any_of(level2Player_.weaponSlotTimers.begin(), level2Player_.weaponSlotTimers.end(),
            [](const auto timer) { return timer != 0; })
        || std::any_of(level1_.systemStatusLevels().begin(), level1_.systemStatusLevels().end(),
            [](const auto status) { return status != 3; })
        || std::any_of(level1_.systemTimers().begin(), level1_.systemTimers().end(),
            [](const auto timer) { return timer != 0; })
        || std::any_of(level2ActorPoolActive_.begin(), level2ActorPoolActive_.end(), [](const auto active) { return active; })
        || std::any_of(level2ProjectilePoolActive_.begin(), level2ProjectilePoolActive_.end(), [](const auto active) { return active; })
        || std::any_of(level2DeathEffectPoolActive_.begin(), level2DeathEffectPoolActive_.end(), [](const auto active) { return active; })
        || std::any_of(level2DeathEffectPoolPendingPresenterRetirement_.begin(),
            level2DeathEffectPoolPendingPresenterRetirement_.end(), [](const auto pending) { return pending; })
        || !level2PendingTerminalEffectPresentations_.empty() || !level2PendingTerminalEffectSlots_.empty()
        || !level2RetiredEffectSemanticEntities_.empty() || !level2RetiredEffectSemanticActiveOrder_.empty()
        || !level2RetiredEffectSemanticActivePositions_.empty() || !level2EffectSemanticActivePositionOverrides_.empty()) {
        throw std::invalid_argument("Level2 success return requires neutral EOF and retained callback-free fixed records");
    }
    const auto& pause = *level2SuccessContinuation_;
    const auto count = level2Entities_.size();
    if ((pause.sourceTick & 15U) == 0 || pause.allocationRevision != level2AllocationRevision_
        || level2LoopTick_ != static_cast<std::uint16_t>(pause.sourceTick + 1)
        || pause.order != level2ActiveListOrder_ || pause.fixedSlots != level2EntityFixedPoolSlots_
        || pause.types.size() != count || pause.order.size() != count
        || level2AllocationListOrder_.size() != count || level2ObjectListOrder_ != level2AllocationListOrder_
        || level2EntityActorPoolSlots_.size() != count || level2EntityProjectilePoolSlots_.size() != count
        || level2EntityDeathEffectPoolSlots_.size() != count || pause.pickupIndex >= count
        || level2PickupIndex_ != pause.pickupIndex || pause.fixedSlot >= level2FixedObjectPoolSize_
        || pause.fixedSlots.size() != count || pause.fixedSlots[pause.pickupIndex] != pause.fixedSlot) {
        throw std::invalid_argument("Level2 success return changed its paused identity or requires periodic work");
    }
    std::array<bool, level2FixedObjectPoolSize_> fixedSeen{};
    std::vector<bool> activeSeen(count), allocationSeen(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto active = pause.order[index];
        const auto allocated = level2AllocationListOrder_[index];
        if (active >= count || allocated >= count || activeSeen[active] || allocationSeen[allocated]) {
            throw std::invalid_argument("Level2 success return requires complete nonduplicate inherited lists");
        }
        activeSeen[active] = allocationSeen[allocated] = true;
        const auto& actor = level2Entities_[index];
        const auto slot = pause.fixedSlots[index];
        if (slot >= level2FixedObjectPoolSize_ || fixedSeen[slot] || !level2FixedPoolKnown_[slot]
            || level2EntityActorPoolSlots_[index] != level2ActorPoolSize_
            || level2EntityProjectilePoolSlots_[index] != level2ProjectilePoolSize_
            || level2EntityDeathEffectPoolSlots_[index] != level2DeathEffectPoolSize_
            || actor.allocationIndex != index || actor.type != pause.types[index] || !actor.active()
            || actor.stateBytes4dTo52[0] != 0xfe
            || (actor.type != 30 && actor.type != 46 && (actor.type != 25 || index != pause.pickupIndex))) {
            throw std::invalid_argument("Level2 success return requires the source FE skip domain and known physical slots");
        }
        fixedSeen[slot] = true;
    }
    const auto& pickup = level2Entities_[pause.pickupIndex];
    if (pickup.type != 25 || pickup.collisionRadius != 897 || !pickup.collisionContactIsPlayer
        || pickup.forwardVelocity != 0 || pickup.xVelocity != 0 || pickup.yVelocity != 0 || pickup.zVelocity != 0
        || pickup.pitch != 0 || pickup.roll != 0 || pickup.pitchVelocity != 0 || pickup.rollVelocity != 0) {
        throw std::invalid_argument("Level2 success return lost its primary source128E pickup");
    }
}

void GameplayModel::resumeLevel2Success(const GameplayInput& input) {
    if(nativeInputRequested(input)) {
        throw std::invalid_argument("source/default continuation rejects queued system actions");
    }
    validateLevel2SuccessReturn(input);
    const auto sourceTick = level2SuccessContinuation_->sourceTick;
    //094F skips every admitted FE record;0A9A alone mutates source128E.
    auto& pickup = level2Entities_[level2SuccessContinuation_->pickupIndex];
    advanceSourcePickupSelector(pickup);
    level2PendingInput_ = input;
    level2LoopTick_ = sourceTick;
    consumeLevel2DeferredTail();
    level2LoopTick_ = static_cast<std::uint16_t>(sourceTick + 1);
    for (std::size_t index = 0; index < level2Entities_.size(); ++index) {
        level2FixedPool_[level2EntityFixedPoolSlots_[index]] = level2Entities_[index];
    }
    level2TailConsumed_ = true;
    level2SuccessContinuation_.reset();
    phase_ = GameplayPhase::level2SuccessReturn;
    presentationFrame_ = 0;
}

std::vector<PlayableSystemEffect> GameplayModel::resumePlayableCampaignSuccess(const GameplayInput& input) {
    if (!playableCampaignFrames_ || phase_ != GameplayPhase::level2SuccessPresentation
        || !level2Complete_ || !level2SuccessContinuation_
        || !successAudioOwned_ || successAudioActive_ || input.exit || input.confirm
        || input.systemAction!=GameplaySystemAction::none) {
        throw std::invalid_argument("playable success requires its retained completed sound/contact child");
    }
    const auto pause = *level2SuccessContinuation_;
    if (pause.allocationRevision != level2AllocationRevision_ || pause.order != level2ActiveListOrder_
        || pause.fixedSlots != level2EntityFixedPoolSlots_ || pause.types.size() != level2Entities_.size()
        || pause.pickupIndex >= level2Entities_.size() || level2PickupIndex_ != pause.pickupIndex
        || level2LoopTick_ != static_cast<std::uint16_t>(pause.sourceTick + 1)) {
        throw std::invalid_argument("playable success lost its suspended parent identities");
    }
    for (std::size_t index = 0; index < pause.types.size(); ++index) {
        if (pause.types[index] != level2Entities_[index].type) {
            throw std::invalid_argument("playable success changed an actor during its modal child");
        }
    }
    if (level2Entities_[pause.pickupIndex].type != static_cast<std::int16_t>(playableExit_)) {
        throw std::invalid_argument("playable return lost its actual campaign exit");
    }
    playableSystemEffects_.clear();
    const auto resumedAudio=level2AudioEvents_.size();
    phase_ = GameplayPhase::level2;
    advanceLevel2(input,nullptr,Level2Continuation::successReturn);
    for(auto index=resumedAudio;index<level2AudioEvents_.size();++index) {
        playableSystemEffects_.push_back(SystemAudio{level2AudioEvents_[index].eventId});
    }
    level2PendingInput_ = input;
    level2LoopTick_ = pause.sourceTick;
    consumeLevel2DeferredTail();
    ++level2LoopTick_;
    syncPlayableCampaignPools();
    level2TailConsumed_ = true;
    level2SuccessContinuation_.reset();
    phase_ = GameplayPhase::level2SuccessReturn;
    presentationFrame_ = 0;
    return std::move(playableSystemEffects_);
}

std::vector<PlayableSystemEffect> GameplayModel::completePlayableCampaignPreloop() {
    if (!playableCampaignFrames_ || phase_!=GameplayPhase::playableLoaderReturn
        || level2LoopTick_!=0 || level2HasAdvanced_ || level2TailConsumed_
        || !level2ActiveListOrder_.empty() || level2Complete_ || level2PlayerArmor_<0) {
        throw std::invalid_argument("playable preloop requires its retained reset loader");
    }
    const auto reset=normalizePlayableSystemsAfterReset(playableJumpState(),PlayableSystemDomain::fullBundle);
    playableSystemEffects_.clear();commitPlayableJump(reset);
    level2Ground_=Level1GroundState(level2Rng_);
    phase_=GameplayPhase::playablePreloopReturn;
    ++ticks_;
    return std::move(playableSystemEffects_);
}

void GameplayModel::validateLevel3Preloop() const {
    if(playableCampaignFrames_) {normalizePlayableSystemsAfterReset(playableJumpState(),PlayableSystemDomain::fullBundle);}
    const auto timed = playableCampaignFrames_ ? hasActiveAbilityTimer(level1_.systemTimers())
        : std::any_of(level1_.systemTimers().begin(),level1_.systemTimers().end(),
            [](const auto timer) { return timer!=0; });
    if (phase_ != GameplayPhase::level3LoaderReturn || activeCampaignLevel_ != 2
        || level2LoopTick_ != 0 || level2HasAdvanced_ || level2TailConsumed_
        || !level2ActiveListOrder_.empty() || level2Complete_
        || level2PlayerArmor_ < 0 || !(level2Player_.position == FixedPosition{})
        || level2Player_.pitch != 0 || level2Player_.roll != 0 || level2Player_.yaw != 0
        || level2Player_.forwardVelocity != 0 || level2Player_.yawVelocity != 0
        || level2Player_.collisionRadius == 0 || (!playableCampaignFrames_ && level1_.selectedSystem() != -1)
        || (!playableCampaignFrames_ && std::any_of(level1_.systemStatusLevels().begin(), level1_.systemStatusLevels().end(),
            [](const auto status) { return status < 0 || status > 3; }))
        || timed) {
        throw std::invalid_argument("Level3 preloop requires its active reset loader and untimed unselected systems");
    }
}

std::vector<PlayableSystemEffect> GameplayModel::completeLevel3Preloop() {
    validateLevel3Preloop();
    playableSystemEffects_.clear();
    if(playableCampaignFrames_) {commitPlayableJump(normalizePlayableSystemsAfterReset(playableJumpState(),PlayableSystemDomain::fullBundle));}
    //01BD/4C16 initializes six points. Culling first occurs at02B7/4C46.
    level2Ground_ = Level1GroundState(level2Rng_);
    phase_ = GameplayPhase::level3PreloopReturn;
    ++ticks_;
    return std::move(playableSystemEffects_);
}

void GameplayModel::advanceRetainedLevel3(const GameplayInput& input) {
    if(nativeInputRequested(input)) {
        throw std::invalid_argument("source/default continuation rejects queued system actions");
    }
    const bool first = phase_ == GameplayPhase::level3PreloopReturn;
    if ((!first && (phase_ != GameplayPhase::level3Native || !level2TailConsumed_ || !level3RadarFinished_))
        || activeCampaignLevel_ != 2 || level3FixedAnimationLimit_ == 0
        || level2Entities_.size() != 6 || level2PlayerArmor_ < 0 || level2Complete_
        || input.left || input.right || input.forward || input.reverse || input.fire || input.exit || input.confirm
        || level1_.selectedSystem() != -1
        || std::any_of(level1_.systemTimers().begin(), level1_.systemTimers().end(),
            [](const auto timer) { return timer != 0; })) {
        throw std::invalid_argument("Level3 frame requires its retained six-actor neutral continuation");
    }
    if (first) {
        level2Visibility_.resetLevel(); //3768 must rebuild the loader's empty inherited list.
    }
    phase_ = GameplayPhase::level3Native;
    level3RadarFinished_ = false;
    ++ticks_;
    advanceLevel2(input);
}

PlayableJumpState GameplayModel::playableJumpState() const {
    return {level1_.systemStatusLevels_,level1_.systemTimers_,level1_.selectedSystem_,level2Player_.position.z};
}

void GameplayModel::commitPlayableJump(const PlayableJumpResult& result) {
    level1_.systemStatusLevels_=result.state.statuses;
    level1_.systemTimers_=result.state.timers;
    level1_.selectedSystem_=result.state.selectedSystem;
    level2Player_.position.z=result.state.playerZ;
    if(result.proxyRefresh==PlayableProxyRefresh::copyPlayerRecord) {
        copyPlayableDecoy(campaignPlayerProxy_,level2Player_);
    }
    if(result.scannerRefresh!=PlayableScannerRefresh::none) {
        const auto scanner=result.scannerRefresh==PlayableScannerRefresh::active
            ? Type6ScannerState::active : Type6ScannerState::inactive;
        for(const auto slot:playableSystemOldActorSlots_) {
            const auto live=std::find(level2EntityActorPoolSlots_.begin(),level2EntityActorPoolSlots_.end(),slot);
            if(live!=level2EntityActorPoolSlots_.end()) {
                auto& actor=level2Entities_[static_cast<std::size_t>(live-level2EntityActorPoolSlots_.begin())];
                applyType6Scanner(actor,scanner);
                level2ActorPool_[slot]=actor;
            } else {
                applyType6Scanner(level2ActorPool_[slot],scanner);
            }
        }
    }
    for(const auto& effect:result.effects) {
        if(const auto* audio=std::get_if<SystemAudio>(&effect)) {
            level2AudioEvents_.push_back({level2LoopTick_,audio->event});
            if(audio->event==7 && result.bombRefresh==PlayableBombRefresh::deploy) {deployPlayableBomb();}
        }
        playableSystemEffects_.push_back(effect);
    }
}

void GameplayModel::advancePlayableCampaign(const GameplayInput& input) {
    if(input.joystick||input.sourceKey!=0) {throw std::invalid_argument("raw host input requires its native controller");}
    const auto level3 = phase_ == GameplayPhase::level3PreloopReturn || phase_ == GameplayPhase::level3Native;
    const auto later = phase_ == GameplayPhase::playablePreloopReturn || phase_ == GameplayPhase::playable;
    if ((!level3 && !later && phase_ != GameplayPhase::level2 && phase_ != GameplayPhase::level5)
        || input.exit || input.confirm || level2PlayerArmor_ < 0 || level2Complete_
        || (level2HasAdvanced_ && !level2TailConsumed_)) {
        throw std::invalid_argument("playable campaign requires its completed retained combat frame");
    }
    const auto nextJump=advancePlayableJump(playableJumpState(),PlayableSystemDomain::fullBundle);
    const auto joystick=applyPlayableJumpAction(nextJump.state,input.joystickAction,PlayableSystemDomain::fullBundle);
    const auto action=applyPlayableJumpAction(joystick.state,input.systemAction,PlayableSystemDomain::fullBundle);
    if(joystick.bombRefresh==PlayableBombRefresh::deploy||action.bombRefresh==PlayableBombRefresh::deploy) {validatePlayableBomb();}
    //The unbound startLevel2 diagnostic remains a component fixture. Bound
    //source metadata must pass the full playable constructor preflight.
    if (campaignModelsBound_ && !playableOrdinaryConstructor_) {
        throw std::invalid_argument("playable campaign requires its prepared typed constructors");
    }
    playableCampaignFrames_ = true;
    level2PrefixGuards_.reset();
    if (!level3 && !later) {
        //The legacy native presenter exposes the ordinal after main, while the
        //source executor enters with the ordinal before main. Convert once.
        if (activeCampaignLevel_ == 1 && level2HasAdvanced_ && level2TailConsumed_) {
            ++level2LoopTick_;
        }
        tickImpl(input,nullptr,nullptr,GameplayInputDomain::playable);
        syncPlayableCampaignPools();
        return;
    }
    if (phase_ == GameplayPhase::level3PreloopReturn || phase_ == GameplayPhase::playablePreloopReturn) {
        level2Visibility_.resetLevel();
    }
    phase_ = later ? GameplayPhase::playable : GameplayPhase::level3Native;
    level3RadarFinished_ = false;
    ++ticks_;
    advanceLevel2(input);
    syncPlayableCampaignPools();
    if (phase_ == GameplayPhase::endingEntry) { return; }
    if (level2PlayerArmor_ < 0) {
        failureFromLevel2_ = true;
        phase_ = GameplayPhase::failurePreExplosionBlackout;
    } else if (level2Complete_) {
        phase_ = GameplayPhase::level2SuccessPresentation;
    }
}

std::vector<PlayableSystemEffect> GameplayModel::finishPlayableCampaignFrame() {
    if (!playableCampaignFrames_ || level2TailConsumed_ || !level2HasAdvanced_
        || (phase_ != GameplayPhase::level2 && phase_ != GameplayPhase::level3Native
            && phase_ != GameplayPhase::playable && phase_ != GameplayPhase::level5
            && phase_ != GameplayPhase::failurePreExplosionBlackout)) {
        throw std::invalid_argument("playable tail requires its pending native display");
    }
    playableSystemEffects_.clear();
    const ArmorLossObservation armorObservation{combatFeedback_,level2PlayerArmor_,ticks_,activeCampaignLevel_,level2PlayerArmor_};
    --level2LoopTick_;
    const auto launched = consumeLevel2DeferredTail();
    ++level2LoopTick_;
    syncPlayableCampaignPools();
    destructions_.erase(std::remove_if(destructions_.begin(),destructions_.end(),[&](const auto& cue) {
        return cue.level!=activeCampaignLevel_||cue.born>ticks_||ticks_-cue.born>=destructionLifetime;
    }),destructions_.end());
    level2TailConsumed_ = true;
    if (launched) {
        weaponIndicatorVisible_ = true;
        weaponReadyIndicator_ = false;
    } else if (weaponReady()) {
        weaponReadyIndicator_ = true;
    }
    if (phase_ == GameplayPhase::failurePreExplosionBlackout) {
        failureSourceLoopTick_ = level2LoopTick_;
        level2FailureTailFinished_ = true;
    }
    return std::move(playableSystemEffects_);
}

void GameplayModel::finishLevel3Frame() {
    if (phase_ != GameplayPhase::level3Native || !level3RadarFinished_ || level2TailConsumed_
        || !level2HasAdvanced_ || level2LoopTick_ == 0) {
        throw std::invalid_argument("Level3 tail requires its completed main and radar display");
    }
    --level2LoopTick_; //The public post-main ordinal is one ahead of source12C8.
    consumeLevel2DeferredTail();
    ++level2LoopTick_;
    level2TailConsumed_ = true;
}

void GameplayModel::resumeLevel5Success(const GameplayInput& input) {
    if(nativeInputRequested(input)) {
        throw std::invalid_argument("source/default continuation rejects queued system actions");
    }
    validateLevel5SuccessReturn(input);
    // 094F skips the sole FE Type25. Only 128E's 0A9A remains before the tail.
    advanceLevel2Pickup();
    level2PendingInput_ = input;
    --level2LoopTick_;
    consumeLevel2DeferredTail();
    ++level2LoopTick_;
    level2FixedPool_[level5SuccessContinuation_->fixedSlot] = level2Entities_.front();
    level2TailConsumed_ = true;
    level5SuccessContinuation_.reset();
    // No admitted tail operation observes 51F4; loaded LEVEL5 ownership remains.
    activeCampaignLevel_ += 2;
    phase_ = GameplayPhase::level5SuccessReturn;
    presentationFrame_ = 0;
}

void GameplayModel::refreshLevel2SightPhase() noexcept {
    const auto sourceTick = activeCampaignLevel_ == 4
        ? static_cast<std::uint16_t>(level2LoopTick_ - 1) : level2LoopTick_;
    if ((sourceTick & 7U) != 0U) {
        return;
    }
    alternateSightPhase_ = false;
    const auto isCandidate = [this](const Level1Entity& entity) {
        // 9cd4 is a signed type comparison, with no health gate. The final
        // Type -1 effect is still listed when 674e asks for sight state.
        if (entity.type >= 14) {
            return false;
        }
        // The campaign loader clears DS:12ce, selecting cached +57 heading.
        const auto difference = static_cast<std::int16_t>(
            static_cast<std::uint16_t>(
                entity.targetHeading - level2Player_.yaw + 0x8000U
            )
        );
        const auto magnitude = static_cast<std::int16_t>(
            difference < 0 ? -static_cast<int>(difference) : difference
        );
        return magnitude < 0x400;
    };
    for (const auto index : level2ActiveListOrder_) {
        if (index < level2Entities_.size() && isCandidate(level2Entities_[index])) {
            alternateSightPhase_ = true;
            return;
        }
    }
    for (const auto& effect : level2PendingTerminalEffectPresentations_) {
        if (isCandidate(effect)) {
            alternateSightPhase_ = true;
            return;
        }
    }
}

void GameplayModel::validatePlayableBomb() const {
    constexpr std::size_t bombModel=43;
    if(!campaignModelsBound_ || campaignModels_[bombModel].radius==0 || campaignModels_[bombModel].selectors!=0) {
        throw std::invalid_argument("Bomb requires the actual shared model43");
    }
    if(const auto slot=playableBombSlot(level2FixedPool_,level2FixedPoolKnown_)) {
        if(level2FixedPool_[*slot].renderProfile==WorldActorRenderProfile::unknown) {
            throw std::invalid_argument("Bomb requires known retained FE callback ownership");
        }
    }
}

void GameplayModel::deployPlayableBomb() {
    validatePlayableBomb();
    const auto slot=playableBombSlot(level2FixedPool_,level2FixedPoolKnown_);
    if(!slot) {return;}
    auto actor=preparePlayableBombActor(level2FixedPool_[*slot],level2Player_,campaignModels_[43].radius);
    const auto index=allocateLevel2FixedObject(actor);
    if(index>=level2Entities_.size() || level2EntityFixedPoolSlots_[index]!=*slot) {
        throw std::logic_error("Bomb lost its qualified physical FE slot");
    }
    actor.allocationIndex=index;
    level2Entities_[index]=actor;
    level2FixedPool_[*slot]=actor;
}

std::optional<std::size_t> GameplayModel::registerLevel5DestroyedTypeBonus(const Level1Entity& source) {
    constexpr std::int16_t bonusType = 42;
    constexpr std::uint32_t bonusRadius = 330;
    constexpr std::int32_t bonusAltitude = 100;
    constexpr std::int16_t bonusSpin = 0x0800;
    const auto needsBonus = (source.type == 5 || source.type == 11 || source.type == 13)
        && level1_.lastDestroyedType_ == source.type && level1_.consecutiveDestroyedTypeCount_ == 2;
    if (needsBonus) {
        for (std::size_t slot = 0; slot < level2FixedObjectPoolSize_; ++slot) {
            if (!level2FixedPoolKnown_[slot]) {
                throw std::invalid_argument("Level5 bonus requires known fixed-record history");
            }
            if (!level2FixedPool_[slot].active()) {
                break;
            }
        }
    }
    const auto payload = level1_.beginDestroyedTypeBonus(source.type);
    std::optional<std::size_t> allocatedSlot;
    if (payload != 0) {
        Level1Entity request{};
        request.type = bonusType;
        request.collisionRadius = bonusRadius;
        const auto index = allocateLevel2FixedObject(request);
        if (index < level2Entities_.size()) {
            auto& pickup = level2Entities_[index];
            pickup.position = {source.position.x, source.position.y, bonusAltitude};
            pickup.yawVelocity = pickup.rollVelocity = bonusSpin;
            pickup.stateBytes4dTo52[1] = 1;
            pickup.stateBytes4dTo52[2] = payload;
            pickup.stateBytes4dTo52[5] = payload;
            if (pickup.renderProfile == WorldActorRenderProfile::noHandles
                || pickup.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback) {
                pickup.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
            }
            allocatedSlot = level2EntityFixedPoolSlots_[index];
            level2FixedPool_[*allocatedSlot] = pickup;
        }
    }
    //90a4 restores last type even when4506 finds no free fixed record.
    level1_.lastDestroyedType_ = source.type;
    return allocatedSlot;
}

void GameplayModel::runLevel2PeriodicSpawnCheck() {
    constexpr std::int16_t maximumThresholdIndex = 15;
    const auto definition = campaignLevelDefinition(
        static_cast<std::uint8_t>(activeCampaignLevel_), campaignDifficultyIndex(runtimeDifficulty_));
    if (level2SpawnedEnemyCount_ >= definition.table.enemyLimit) {
        return;
    }
    const auto choice = level2Rng_.next();
    const auto activeIndex = static_cast<std::uint8_t>(std::clamp<std::int16_t>(
        level2ActiveEnemyCount_, 0, maximumThresholdIndex));
    const auto selection = campaignSpawnType(definition, activeIndex, level2SpawnedEnemyCount_, choice);
    if (!selection) {
        return;
    }
    const auto selectedType = *selection;

    const auto actorSlot = std::find(
        level2ActorPoolActive_.begin(),
        level2ActorPoolActive_.end(),
        false
    );
    if (actorSlot == level2ActorPoolActive_.end()) {
        return;
    }
    if (playableCampaignFrames_ && playableOrdinaryConstructor_) {
        auto spawned=playableOrdinaryConstructor_->construct(selectedType,level2Rng_);
        allocateLevel2Actor(std::move(spawned),ActorInitialization::validatedOrdinary);
        ++level2ActiveEnemyCount_;
        ++level2SpawnedEnemyCount_;
        return;
    }
    if (activeCampaignLevel_ == 4) {
        if (!level5Constructor_) {
            throw std::logic_error("Level5 periodic spawn requires its validated ordinary constructor");
        }
        auto spawned = level5Constructor_->construct(selectedType, level2Rng_);
        allocateLevel2Actor(std::move(spawned), ActorInitialization::validatedOrdinary);
        ++level2ActiveEnemyCount_;
        ++level2SpawnedEnemyCount_;
        return;
    }
    const auto positionChoice = level2Rng_.next();
    auto firstCode = static_cast<std::int16_t>(
        static_cast<std::uint8_t>(positionChoice >> 8U) & 0x0FU
    );
    auto secondCode = static_cast<std::int16_t>(0x10);
    if ((positionChoice & 1U) != 0) {
        std::swap(firstCode, secondCode);
    }
    if ((positionChoice & 2U) != 0) {
        firstCode = static_cast<std::int16_t>(-firstCode);
    }
    if ((positionChoice & 4U) != 0) {
        secondCode = static_cast<std::int16_t>(-secondCode);
    }
    const auto coordinate = [](const std::int16_t code) noexcept {
        const auto lowByte = static_cast<std::uint8_t>(code);
        return static_cast<std::int32_t>(
            static_cast<std::int16_t>(
                static_cast<std::uint16_t>(lowByte) << 11U
            )
        ) - 1;
    };

    Level1Entity spawned{};
    spawned.flags = 0;
    spawned.type = selectedType;
    spawned.position = {
        coordinate(firstCode),
        coordinate(secondCode),
        selectedType == 12 ? 1000 : 0,
    };
    spawned.stateBytes4dTo52[0] = 0xff;
    if (activeCampaignLevel_ == 2) {
        // FUN_1000:4370's generic initializer fills these fields from the
        // Level 3 data tables.  Type 11/12 are born above the ground and
        // enter callback state five before their first behavior dispatch.
        switch (selectedType) {
        case 2:
            spawned.forwardVelocity = 144;
            spawned.stateBytes4dTo52[1] = 1;
            spawned.stateBytes4dTo52[4] = 1;
            spawned.weaponSlotCount = 1;
            break;
        case 3:
            spawned.forwardVelocity = 216;
            spawned.stateBytes4dTo52[1] = 1;
            spawned.stateBytes4dTo52[4] = 1;
            spawned.weaponSlotCount = 1;
            break;
        case 10:
            spawned.stateBytes4dTo52[1] = 1;
            spawned.stateBytes4dTo52[4] = 2;
            spawned.weaponSlotCount = 3;
            break;
        case 12:
            spawned.forwardVelocity = 360;
            spawned.stateBytes4dTo52[1] = 1;
            spawned.stateBytes4dTo52[2] = 5;
            spawned.stateBytes4dTo52[4] = 3;
            spawned.weaponSlotCount = 1;
            break;
        default:
            return;
        }
        if (selectedType == 10) {
            const auto turnChoice = level2Rng_.next();
            spawned.yawVelocity = static_cast<std::int16_t>(
                static_cast<std::uint16_t>(
                    static_cast<std::uint8_t>(
                        ((turnChoice >> 8U) & 3U) + 7U
                    )
                ) << 8U
            );
            if ((turnChoice & 1U) != 0) {
                spawned.yawVelocity = static_cast<std::int16_t>(
                    -spawned.yawVelocity
                );
            }
        }
    } else if (selectedType == 1) {
        spawned.collisionRadius = 1342;
        spawned.forwardVelocity = 225;
        spawned.stateBytes4dTo52[1] = 1;
        spawned.stateBytes4dTo52[4] = 1;
        spawned.weaponSlotCount = 2;
    } else if (selectedType == 4) {
        spawned.collisionRadius = 928;
        spawned.forwardVelocity = 180;
        spawned.stateBytes4dTo52[1] = 2;
        spawned.stateBytes4dTo52[4] = 1;
        spawned.weaponSlotCount = 1;
    } else if (selectedType == 8) {
        spawned.collisionRadius = 860;
        spawned.stateBytes4dTo52[1] = 2;
        spawned.stateBytes4dTo52[4] = 2;
        spawned.weaponSlotCount = 2;
    } else if (selectedType == 13) {
        spawned.collisionRadius = 559;
        spawned.stateBytes4dTo52[1] = 1;
        spawned.stateBytes4dTo52[4] = 4;
        const auto turnChoice = level2Rng_.next();
        spawned.yawVelocity = static_cast<std::int16_t>(
            static_cast<std::uint16_t>(
                static_cast<std::uint8_t>(
                    ((turnChoice >> 8U) & 3U) + 7U
                )
            ) << 8U
        );
        if ((turnChoice & 1U) != 0) {
            spawned.yawVelocity = static_cast<std::int16_t>(
                -spawned.yawVelocity
            );
        }
    } else {
        return;
    }
    const auto yawChoice = level2Rng_.next();
    spawned.yaw = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(
            static_cast<std::uint8_t>(yawChoice) & 0xF0U
        ) << 11U
    );

    allocateLevel2Actor(spawned);
    ++level2ActiveEnemyCount_;
    ++level2SpawnedEnemyCount_;
}

std::uint8_t GameplayModel::nextLevel2VisualRandomByte() noexcept {
    // FUN_1000:e078 calls the 16x16 -> 32-bit helper at e16a, adds the
    // target's fixed increment, stores the resulting low/high words at
    // DS:3988/398a, and returns the low byte of the 15-bit high word.
    const auto low = static_cast<std::uint32_t>(
        level2VisualRandomState_ & 0xffffU
    );
    const auto high = static_cast<std::int32_t>(
        (level2VisualRandomState_ >> 16U) & 0xffffU
    );
    const auto product = static_cast<std::uint64_t>(low) * 0x43fdU;
    const auto mixedHigh = static_cast<std::int64_t>(product >> 16U)
        + static_cast<std::int64_t>(high) * 0x43fd
        + static_cast<std::int64_t>(low) * 3;
    const auto mixed = static_cast<std::uint32_t>(
        (static_cast<std::uint32_t>(mixedHigh) & 0xffffU) << 16U
    ) | static_cast<std::uint32_t>(product & 0xffffU);
    level2VisualRandomState_ = mixed + 0x269ec3U;
    return static_cast<std::uint8_t>(
        (level2VisualRandomState_ >> 16U) & 0xffU
    );
}

void GameplayModel::tickRetainedLevel1(const GameplayInput& input) {
    tickRetainedLevel1Impl(input,nullptr);
}

void GameplayModel::tickRetainedLevel1(const GameplayInput& input, Level1FrameRenderer& renderer) {
    tickRetainedLevel1Impl(input,&renderer);
}

void GameplayModel::tickRetainedLevel1Impl(const GameplayInput& input, Level1FrameRenderer* renderer) {
    if (nativeInputRequested(input)) {
        throw std::invalid_argument("source retained Level1 rejects queued system actions");
    }
    if (phase_ != GameplayPhase::level1) {
        throw std::invalid_argument("retained Level1 tick requires its ordinary phase");
    }
    level1_.retainSuccessContact_ = true;
    try {
        tickImpl(input,nullptr,renderer);
    } catch (...) {
        level1_.retainSuccessContact_ = false;
        throw;
    }
    level1_.retainSuccessContact_ = false;
}

bool GameplayModel::pendingLevel1SuccessContact() const noexcept {
    return phase_ == GameplayPhase::successPresentation && level1_.pendingSuccessContact();
}

void GameplayModel::finishLevel1Frame() {
    if (phase_ != GameplayPhase::level1 || level1_.levelIndex() != 0) {
        throw std::invalid_argument("Level1 frame completion requires its ordinary phase");
    }
    level1_.finishFrame();
}

void GameplayModel::validateLevel1SuccessReturn(const GameplayInput& input) const {
    if (!pendingLevel1SuccessContact() || !successAudioOwned_ || successAudioActive_
        || input.left || input.right || input.forward || input.reverse || input.fire || input.exit || input.confirm) {
        throw std::invalid_argument("Level1 success return requires its paused contact, live EOF and neutral input");
    }
}

bool GameplayModel::pendingLevel1FailureTail() const noexcept {
    if (phase_ != GameplayPhase::level1 || level1_.levelIndex() != 0 || level1FailureTailFinished_
        || !level1_.pendingFailureTail() || level1_.pendingPostBoundaryInput_.fire
        || level1_.player().pitch != 0 || level1_.player().roll != 0 || level1_.player().yaw != 0
        || level1_.player().position.z != 0
        || std::any_of(level1_.projectiles().begin(), level1_.projectiles().end(),
            [](const auto& shot) { return shot.active(); })
        || std::any_of(level1_.deathEffects().begin(), level1_.deathEffects().end(),
            [](const auto& effect) { return effect.listed; })) {
        return false;
    }
    std::size_t active = 0;
    for (const auto& actor : level1_.entities()) {
        if (!actor.active()) {
            continue;
        }
        ++active;
        const auto category = static_cast<std::int8_t>(actor.stateBytes4dTo52[0]);
        if ((actor.type != 1 && actor.type != 11 && actor.type != 29)
            || (category != -1 && category != -2)
            || (actor.type == 11 && (!actor.primaryType11AnimationSelector
                || !actor.animatedModelPresenterEnabled))) {
            return false;
        }
    }
    const auto objects = level1_.radarPresentationObjects();
    std::size_t previous = 0;
    bool first = true;
    for (const auto& object : objects) {
        if (object.kind != Level1PresentationKind::entity || object.slot >= level1_.entities().size()
            || !level1_.entities()[object.slot].active() || (!first && object.slot <= previous)) {
            return false;
        }
        first = false;
        previous = object.slot;
    }
    return active != 0 && objects.size() == active;
}

void GameplayModel::finishLevel1FailureTail() {
    if (!pendingLevel1FailureTail()) {
        throw std::invalid_argument("Level1 failure tail requires its neutral retained final display");
    }
    const auto cooldown = level1_.playerWeaponCooldown();
    const auto timers = level1_.playerProjectileTimers();
    level1_.finishFailureTail();
    if (weaponIndicatorVisible_ && weaponReady()
        && ((cooldown != 0 && level1_.playerWeaponCooldown() == 0)
            || (timers[0] != 0 && level1_.playerProjectileTimers()[0] == 0)
            || (timers[1] != 0 && level1_.playerProjectileTimers()[1] == 0))) {
        weaponReadyIndicator_ = true;
    }
    failureSourceLoopTick_ = level1_.loopTick();
    level1FailureTailFinished_ = true;
}

bool GameplayModel::pendingLevel2FailureTail() const noexcept {
    if (playableCampaignFrames_) {
        return phase_ == GameplayPhase::failurePreExplosionBlackout && failureFromLevel2_
            && level2HasAdvanced_ && !level2TailConsumed_ && !level2FailureTailFinished_
            && !level2PendingInput_.exit;
    }
    const auto sourceTick = activeCampaignLevel_ == 4
        ? static_cast<std::uint16_t>(level2LoopTick_ - 1) : level2LoopTick_;
    return phase_ == GameplayPhase::failurePreExplosionBlackout && failureFromLevel2_
        && (activeCampaignLevel_ == 1 || activeCampaignLevel_ == 4)
        && level2HasAdvanced_ && !level2TailConsumed_
        && !level2FailureTailFinished_ && (sourceTick & 0x000fU) != 0
        && !level2PendingInput_.exit;
}

void GameplayModel::finishLevel2FailureTail() {
    if (!pendingLevel2FailureTail()) {
        throw std::invalid_argument("Level2 failure tail requires an unconsumed final display");
    }
    const auto cooldown = level2Player_.weaponCooldown;
    const auto timers = level2Player_.weaponSlotTimers;
    if (activeCampaignLevel_ == 4) {
        --level2LoopTick_;
    }
    const auto allocated = consumeLevel2DeferredTail();
    if (activeCampaignLevel_ == 4) {
        ++level2LoopTick_;
    }
    if (allocated) {
        weaponIndicatorVisible_ = true;
        weaponReadyIndicator_ = false;
    }
    const bool ready = level2Player_.weaponCooldown == 0
        && (level2Player_.weaponSlotTimers[0] == 0 || level2Player_.weaponSlotTimers[1] == 0);
    if (weaponIndicatorVisible_ && ready && ((cooldown != 0 && level2Player_.weaponCooldown == 0)
        || (timers[0] != 0 && level2Player_.weaponSlotTimers[0] == 0)
        || (timers[1] != 0 && level2Player_.weaponSlotTimers[1] == 0))) {
        weaponReadyIndicator_ = true;
    }
    //Keep the public ordinary ordinal; this existing owner records source03d1.
    failureSourceLoopTick_ = static_cast<std::uint16_t>(level2LoopTick_ + (activeCampaignLevel_ == 4 ? 0 : 1));
    if (activeCampaignLevel_ == 4) {
        level2TailConsumed_ = true;
    }
    level2FailureTailFinished_ = true;
}

bool GameplayModel::consumeLevel2DeferredTail() {
    bool level2PlayerProjectileAllocated = false;
    const auto level2ProjectileActiveAtStart = level2ProjectilePoolActive_;
    const auto cooldownBefore = level2Player_.weaponCooldown;
    const auto timersBefore = level2Player_.weaponSlotTimers;
    const auto& deferred = level2PendingInput_;
    const bool directional = (
        deferred.left
        || deferred.right
        || deferred.forward
        || deferred.reverse
    );
    std::size_t capturedAudio=level2AudioEvents_.size();
    const auto captureAudio=[&]() {
        if(!playableCampaignFrames_) {return;}
        while(capturedAudio<level2AudioEvents_.size()) {
            playableSystemEffects_.push_back(SystemAudio{level2AudioEvents_[capturedAudio++].eventId});
        }
    };
    if(playableCampaignFrames_) {
        commitPlayableJump(advancePlayableJump(playableJumpState(),PlayableSystemDomain::fullBundle));
        commitPlayableJump(applyPlayableJumpAction(playableJumpState(),deferred.joystickAction,PlayableSystemDomain::fullBundle));
        capturedAudio=level2AudioEvents_.size();
    }
    const auto applyControls=[&]() {
        if(playableCampaignFrames_ && deferred.analog && !directional) {
            //9F48..9F5C tests signedY against16, then stores the shifted axis.
            const auto x=deferred.analog->y<16 ? -static_cast<int>(deferred.analog->x) : static_cast<int>(deferred.analog->x);
            level2Player_.yawVelocity=static_cast<std::int16_t>(std::clamp(x*8,-512,512));
        } else if (deferred.left || deferred.right) {
            const auto adjusted = static_cast<std::int32_t>(
                level2Player_.yawVelocity
            ) + (deferred.left ? 0x50 : -0x50);
            level2Player_.yawVelocity = static_cast<std::int16_t>(
                std::clamp(adjusted, -0x200, 0x200)
            );
        } else if (level2Player_.yawVelocity > 0) {
            level2Player_.yawVelocity = static_cast<std::int16_t>(
                std::max(0, level2Player_.yawVelocity - 0x50)
            );
        } else if (level2Player_.yawVelocity < 0) {
            level2Player_.yawVelocity = static_cast<std::int16_t>(
                std::min(0, level2Player_.yawVelocity + 0x50)
            );
        }
        if(playableCampaignFrames_) {
            level2Player_.forwardVelocity=advancePlayableForwardVelocity(level2Player_.forwardVelocity,deferred,
                level1_.systemTimers_[6]!=0 ? PlayablePropulsion::turbo : PlayablePropulsion::ordinary);
        } else {
            if (deferred.forward || deferred.reverse) {
                const auto adjusted = static_cast<std::int32_t>(
                    level2Player_.forwardVelocity
                ) + (deferred.reverse ? -10 : 10);
                level2Player_.forwardVelocity = static_cast<std::int16_t>(
                    std::clamp(adjusted, -200, 200)
                );
            } else if (!directional && level2Player_.forwardVelocity > 0) {
                level2Player_.forwardVelocity = static_cast<std::int16_t>(
                    std::max(0, level2Player_.forwardVelocity - 10)
                );
            } else if (!directional && level2Player_.forwardVelocity < 0) {
                level2Player_.forwardVelocity = static_cast<std::int16_t>(
                    std::min(0, level2Player_.forwardVelocity + 10)
                );
            }
        }
        if (level2Type46ContactSlowdown_) {
            level2Player_.yawVelocity = arithmeticShiftRightOne(
                arithmeticShiftRightOne(level2Player_.yawVelocity)
            );
            level2Player_.forwardVelocity = arithmeticShiftRightOne(
                arithmeticShiftRightOne(level2Player_.forwardVelocity)
            );
            level2Type46ContactSlowdown_ = false;
        }
    };
    if(!playableCampaignFrames_) {applyControls();}
    if (deferred.fire && level2Player_.weaponCooldown == 0) {
        const auto profile=playablePrimaryProfile(playableCampaignFrames_ ? level1_.systemTimers_[5] : 0);
        auto timer = level2Player_.weaponSlotTimers.end();
        for (auto item = level2Player_.weaponSlotTimers.begin();
             item != level2Player_.weaponSlotTimers.begin()
                 + playerCannonSlotCount_;
             ++item) {
            if (*item == 0) {
                timer = item;
                break;
            }
        }
        const auto poolSlot = std::find(
            level2ProjectilePoolActive_.begin(),
            level2ProjectilePoolActive_.end(),
            false
        );
        if (
            (!profile.requiresCannonSlot || timer != level2Player_.weaponSlotTimers.end())
            && poolSlot != level2ProjectilePoolActive_.end()
        ) {
            const auto poolIndex = static_cast<std::size_t>(
                poolSlot - level2ProjectilePoolActive_.begin()
            );
            auto projectile = level2ProjectilePool_[poolIndex];
            if (playableCampaignFrames_) {
                if (projectile.renderProfile != WorldActorRenderProfile::inlineSelectorNoCallback) {
                    throw std::invalid_argument("player fire lost reset-owned FD selector ancestry");
                }
                projectile.animatedModelPresenterEnabled = false;
            }
            projectile.type = 23;
            if (playableCampaignFrames_ || activeCampaignLevel_ != 2) {
                projectile.collisionRadius = playableCampaignFrames_ && campaignModelsBound_ ? campaignModels_[23].radius : 134;
            }
            projectile.position = level2Player_.position;
            projectile.pitch = level2Player_.pitch;
            projectile.roll = level2Player_.roll;
            projectile.yaw = level2Player_.yaw;
            projectile.forwardVelocity = 500;
            projectile.zVelocity = 0;
            if (activeCampaignLevel_ == 4) {
                //44ba clears+3a/+51;05d4 writes life/selector. Other freed
                //record velocities, state and presenter cache survive reuse.
                projectile.stateBytes4dTo52[1] = profile.lifetime;
                projectile.stateBytes4dTo52[2] = profile.selector;
                projectile.stateBytes4dTo52[4] = 0;
            } else {
                projectile.pitchVelocity = 0;
                projectile.rollVelocity = 0;
                projectile.yawVelocity = 0;
                projectile.xVelocity = 0;
                projectile.yVelocity = 0;
                projectile.stateBytes4dTo52 = {0xfd, profile.lifetime, profile.selector, 0, 0, 0};
                projectile.weaponSlotCount = 0;
                projectile.weaponCooldown = 0;
                projectile.weaponSlotTimers = {};
            }
            projectile.ownerIsPlayer = true;
            projectile.ownerAllocationIndex =
                static_cast<std::size_t>(-1);
            projectile.collisionContactAllocationIndex =
                static_cast<std::size_t>(-1);
            projectile.collisionContactIsPlayer = false;
            level2ProjectilePoolActive_[poolIndex] = true;
            auto insertionIndex = level2Entities_.size();
            for (std::size_t index = 0;
                 index < level2EntityProjectilePoolSlots_.size();
                 ++index) {
                const auto occupiedPoolIndex =
                    level2EntityProjectilePoolSlots_[index];
                if (
                    (occupiedPoolIndex < level2ProjectilePoolSize_
                        && occupiedPoolIndex > poolIndex)
                    || (activeCampaignLevel_ == 1
                        && level2EntityDeathEffectPoolSlots_[index]
                            < level2DeathEffectPoolSize_)
                ) {
                    // Native projectile records precede the effect pool
                    // in memory. Preserve that semantic index order while
                    // appending the new pointer to the allocation list.
                    // Intact Level 2 ticks 2145/2146 exercise both pools.
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
            if (activeCampaignLevel_ == 4) {
                for (auto& index : level2ActiveListOrder_) {
                    if (index >= insertionIndex) {
                        ++index;
                    }
                }
            }
            projectile.allocationIndex = insertionIndex;
            ++level2AllocationRevision_;
            level2RetiredEffectSemanticOrderRebuilt_ = false;
            level2Entities_.insert(
                level2Entities_.begin()
                    + static_cast<std::ptrdiff_t>(insertionIndex),
                projectile
            );
            level2EntityProjectilePoolSlots_.insert(
                level2EntityProjectilePoolSlots_.begin()
                    + static_cast<std::ptrdiff_t>(insertionIndex),
                poolIndex
            );
            level2EntityActorPoolSlots_.insert(
                level2EntityActorPoolSlots_.begin()
                    + static_cast<std::ptrdiff_t>(insertionIndex),
                level2ActorPoolSize_
            );
            insertLevel2FixedPoolSlot(insertionIndex, level2FixedObjectPoolSize_);
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
            if (activeCampaignLevel_ == 4) {
                //44ba changes allocation ownership after main/radar.3768 admits
                //the pointer on the next display, preserving the preceding walk.
                level2Visibility_.allocationSucceeded();
            } else {
                level2ActiveListOrder_ = level2AllocationListOrder_;
            }
            if(profile.requiresCannonSlot) {*timer=32;}
            level2Player_.weaponCooldown=profile.cooldown;
            if(playableCampaignFrames_) {refreshPlayableDecoyPosition(campaignPlayerProxy_,level2Player_);}
            level2AudioEvents_.push_back({level2LoopTick_,profile.event});
            level2PlayerProjectileAllocated = true;
        }
    }
    if(playableCampaignFrames_) {
        captureAudio();
        commitPlayableJump(applyPlayableJumpAction(playableJumpState(),deferred.systemAction,PlayableSystemDomain::fullBundle));
        capturedAudio=level2AudioEvents_.size();
        if(level1_.systemTimers_[2]==0) {applyControls();}
    }
    const auto deltaX = level2Player_.position.x;
    const auto deltaY = level2Player_.position.y;
    presentationTranslation_[0]+=deltaX;presentationTranslation_[1]+=deltaY;
    //0C6A walks completed45D2, not the allocation array. Off-list actors
    //must retain their coordinates until3768 admits them again.
    const auto receivesTail = [this](const std::size_t index) {
        return !playableCampaignFrames_ || std::find(level2ActiveListOrder_.begin(),
            level2ActiveListOrder_.end(),index)!=level2ActiveListOrder_.end();
    };
    for (std::size_t index = 0;
         index < level2Entities_.size(); ++index) {
        if (!receivesTail(index)) {
            continue;
        }
        const auto poolSlot = level2EntityProjectilePoolSlots_[index];
        if (
            poolSlot < level2ProjectilePoolSize_
            && !level2ProjectileActiveAtStart[poolSlot]
        ) {
            continue;
        }
        if (level2Entities_[index].type == 7) {
            continue;
        }
        level2Entities_[index].position.x -= deltaX;
        level2Entities_[index].position.y -= deltaY;
    }
    // FUN_1000:9298 still carries a terminal effect pointer for this
    // boundary even though FUN_1000:3768 removed it from the allocation
    // array.  The stale pointer therefore receives the same camera-space
    // translation as the live presenter list before its final refresh.
    for (std::size_t slot = 0;
         slot < level2DeathEffectPoolSize_; ++slot) {
        if (
            level2DeathEffectPoolPendingPresenterRetirement_[slot]
            && !level2DeathEffectPoolActive_[slot]
        ) {
            level2DeathEffectPool_[slot].position.x -= deltaX;
            level2DeathEffectPool_[slot].position.y -= deltaY;
        }
    }
    if(playableCampaignFrames_ && level1_.systemTimers_[0]!=0) {
        recenterPlayableDecoy(campaignPlayerProxy_,level2Player_.position);
    }
    level2Player_.position.x = 0;
    level2Player_.position.y = 0;
    if ((level2LoopTick_ & 0x000FU) == 0) {
        for (std::size_t index = 0; index < level2Entities_.size(); ++index) {
            if (!receivesTail(index)) {
                continue;
            }
            auto& entity = level2Entities_[index];
            if (entity.type != 7) {
                normalizeWorldPosition(entity.position);
            }
        }
        // The shared campaign scheduler runs its periodic spawn check
        // after loop tick 16. ANTARES begins with five active/spawned
        // enemies against a maximum of twelve. Active-count bucket five
        // has a zero threshold, so this check rejects after exactly one
        // RNG call and does not otherwise mutate the decoded mission
        // state. The dense original series first exposes that call at
        // the following main-view boundary, loop tick 17.
        runLevel2PeriodicSpawnCheck();
        if (activeCampaignLevel_ == 4 || playableCampaignFrames_) {
            runLevel2CompletionCheck();
        }
    }
    constexpr std::int16_t armorWarningThreshold = 200;
    constexpr std::uint16_t armorWarningPeriodMask = 63;
    constexpr std::uint16_t armorWarningEvent = 3;
    if ((activeCampaignLevel_ == 4 || phase_ == GameplayPhase::level3Native)
        && level2PlayerArmor_ < armorWarningThreshold
        && (level2LoopTick_ & armorWarningPeriodMask) == 0) {
        level2ArmorWarningActive_ = true; //source129c, before cooldown/slot timers.
        level2AudioEvents_.push_back({level2LoopTick_, armorWarningEvent});
    }
    if (level2Player_.weaponCooldown != 0) {
        --level2Player_.weaponCooldown;
    }
    for (auto& timer : level2Player_.weaponSlotTimers) {
        if (timer != 0) {
            --timer;
        }
    }
    if (activeCampaignLevel_ == 4) {
        if (level2PlayerProjectileAllocated) {
            weaponIndicatorVisible_ = true;
            weaponReadyIndicator_ = false;
        }
        const auto ready = level2Player_.weaponCooldown == 0
            && (level2Player_.weaponSlotTimers[0] == 0 || level2Player_.weaponSlotTimers[1] == 0);
        if (weaponIndicatorVisible_ && ready
            && ((cooldownBefore != 0 && level2Player_.weaponCooldown == 0)
                || (timersBefore[0] != 0 && level2Player_.weaponSlotTimers[0] == 0)
                || (timersBefore[1] != 0 && level2Player_.weaponSlotTimers[1] == 0))) {
            weaponReadyIndicator_ = true;
        }
    }
    captureAudio();
    return level2PlayerProjectileAllocated;
}

void GameplayModel::runLevel2CompletionCheck() {
    if (level2Complete_
        || level2SpawnedEnemyCount_ < campaignLevelDefinition(
            static_cast<std::uint8_t>(activeCampaignLevel_),
            campaignDifficultyIndex(runtimeDifficulty_)).table.enemyLimit
        || level2CompletionObjectIndex_ != static_cast<std::size_t>(-1)
        || level2PostWaveObjectIndex_ != static_cast<std::size_t>(-1)) {
        return;
    }
    if (activeCampaignLevel_ == 4 || playableCampaignFrames_) {
        //698a's132e observes the completed3768 list, including clipped FF actors.
        for (const auto index : level2PresentationActiveListOrder_) {
            if (index >= level2PresentationEntities_.size()) {
                throw std::invalid_argument("Level5 completion requires its completed radar list");
            }
            if (level2PresentationEntities_[index].stateBytes4dTo52[0] == 0xff) {
                return;
            }
        }
        if (std::all_of(level2ActorPoolActive_.begin(), level2ActorPoolActive_.end(),
            [](const bool active) { return active; })) {
            throw std::invalid_argument("Level5 completion requires a free ordinary record");
        }
    } else if (level2ActiveEnemyCount_ != 0) {
        return;
    }
    level2Rng_.next();
    level2Rng_.next();
    Level1Entity completion{};
    completion.type = 7;
    completion.collisionRadius = 13052;
    completion.position = level2Player_.position;
    completion.position.z = 5000;
    completion.pitch = level2Player_.pitch;
    completion.roll = level2Player_.roll;
    completion.yaw = level2Player_.yaw;
    completion.forwardVelocity = static_cast<std::int16_t>(0x9000);
    completion.stateBytes4dTo52 = {255, 100, 0, 0, 5, 0};
    if (playableCampaignFrames_) {
        completion.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
        completion.animatedModelPresenterEnabled = false;
    }
    advanceEntityKinematics(completion);
    completion.forwardVelocity = 400;
    completion.weaponSlotCount = 3;
    level2CompletionObjectIndex_ = allocateLevel2Actor(completion,
        activeCampaignLevel_ == 4 ? ActorInitialization::validatedOrdinary : ActorInitialization::legacy);
    if ((playableCampaignFrames_ || activeCampaignLevel_ == 1 || activeCampaignLevel_ == 4)
        && level2CompletionObjectIndex_ < level2Entities_.size()) {
        //4712 attaches the reset record's inline +4f selector after4370.
        level2Entities_[level2CompletionObjectIndex_].renderProfile =
            WorldActorRenderProfile::inlineSelectorNoCallback;
    }
    level2AudioEvents_.push_back({level2LoopTick_, 17});
    ++level2ActiveEnemyCount_;
    ++level2SpawnedEnemyCount_;
}

void GameplayModel::recordDestruction(const Level1Entity& source,const FixedPosition& position,const std::uint16_t event) {
    const bool vehicle=(source.type>=1&&source.type<=16)||source.type==44||source.type==45;
    if(!playableCampaignFrames_||source.stateBytes4dTo52[0]!=0xff||!vehicle) {return;}
    constexpr std::size_t maximumCues=64;
    if(destructions_.size()==maximumCues) {destructions_.erase(destructions_.begin());}
    destructions_.push_back({++destructionSerial_,ticks_,activeCampaignLevel_,position,source.type,
        static_cast<std::uint16_t>(std::clamp<int>(source.collisionRadius,256,4096)),event});
}

void GameplayModel::advanceLevel2(const GameplayInput& input, Level2FrameRenderer* renderer,
    const Level2Continuation continuation) {
    const ArmorLossObservation armorObservation{combatFeedback_,level2PlayerArmor_,ticks_,activeCampaignLevel_,level2PlayerArmor_};
    const auto resuming = continuation == Level2Continuation::successReturn;
    const bool retainedLevel3 = phase_ == GameplayPhase::level3Native || phase_==GameplayPhase::playable;
    const bool playableLevel1 = playableCampaignFrames_ && activeCampaignLevel_ == 0;
    const bool physicalLevel2Guardian = playableCampaignFrames_ && activeCampaignLevel_ == 1
        && campaignModelsBound_ && playableOrdinaryConstructor_.has_value();
    retainPhysicalPostWaveRole();
    if (phase_ == GameplayPhase::level2PreloopReturn) {
        prepareFirstLevel2SourceFrame(input, renderer);
        return;
    }
    qualifyLevel2SourcePrefix(input, renderer);
    const auto campaignGuardians = playableCampaignFrames_ && (activeCampaignLevel_ == 0 || activeCampaignLevel_ == 3
        || activeCampaignLevel_ == 5 || activeCampaignLevel_ == 6);
    const auto firstFrame = !level2HasAdvanced_;
    const auto tailConsumed = level2TailConsumed_;
    if (tailConsumed && renderer != nullptr
        && renderer->prefixExecution() == Level2PrefixExecution::source) {
        advanceLevel2SourcePrefix(input, *renderer);
        return;
    }
    level2TailConsumed_ = false;
    if (level2HasAdvanced_ && !resuming) {
        if (!playableRadarAnimationSettled_) { advanceLevel2GuardianAnimation(); }
        playableRadarAnimationSettled_ = false;
    }
    expireLevel2EffectPresentation();
    const auto level2ActiveListOrderAtStart = level2ActiveListOrder_;
    const auto level2EntityCountAtStart = level2Entities_.size();
    auto level2ActiveIdentitiesAtStart = captureLevel2ActiveIdentities(level2ActiveListOrderAtStart);
    if(playableCampaignFrames_) {
        playableSystemOldActorSlots_.clear();
        for(const auto identity:level2ActiveIdentitiesAtStart) {
            if(identity.pool==0) {playableSystemOldActorSlots_.push_back(identity.slot);}
        }
    }
    auto level2ProjectileActiveAtStart =
        level2ProjectilePoolActive_;
    level2ProjectilePoolWasActiveAtBoundaryStart_ =
        level2ProjectileActiveAtStart;
    if (activeCampaignLevel_ == 4) {
        //A tail-born FD record is allocated but has not entered0784/9298.
        //Keep physical occupancy above for reuse; updater participation follows
        //the inherited pointer list until3768 rebuilds it for main.
        level2ProjectileActiveAtStart.fill(false);
        for (const auto identity : level2ActiveIdentitiesAtStart) {
            if (identity.pool == 1 && level2ProjectilePoolActive_[identity.slot]) {
                level2ProjectileActiveAtStart[identity.slot] = true;
            }
        }
    }
    const auto level2ActorActiveAtStart = level2ActorPoolActive_;
    const CampaignContactStart contactStart{level2ActiveIdentitiesAtStart, level2ActorActiveAtStart,
        level2ProjectileActiveAtStart, retainedLevel3, campaignGuardians};
    const auto actorExistedAtStart = [this, &contactStart](const std::size_t entityIndex) noexcept {
        return campaignActorExistedAtStart(entityIndex, contactStart);
    };
    bool level2PlayerProjectileAllocated = false;
    if (level2HasAdvanced_ && !tailConsumed && !resuming) {
        level2PlayerProjectileAllocated = consumeLevel2DeferredTail();
    } else {
        level2HasAdvanced_ = true;
    }

    const auto& behaviorTarget=playableCampaignFrames_
        ? playableBehaviorTarget(level2Player_,campaignPlayerProxy_,level1_.systemTimers_[0]) : level2Player_;
    if (level2PlayerCollisionMomentum_ != 0 && !resuming) {
        level2Player_.forwardVelocity = level2PlayerCollisionMomentum_;
        level2PlayerCollisionMomentum_ =
            level2PlayerCollisionMomentum_ < 0
            ? static_cast<std::int16_t>(
                -((-level2PlayerCollisionMomentum_) >> 1)
            )
            : static_cast<std::int16_t>(
                level2PlayerCollisionMomentum_ >> 1
            );
    }
    if (!resuming && (!tailConsumed || activeCampaignLevel_ == 4 || retainedLevel3)) {
        ++level2LoopTick_;
    }
    if (!resuming) { advanceEntityKinematics(level2Player_); }
    // FUN_1000_6d46 advances this cockpit light independently of player
    // steering. It reverses only after crossing the inclusive 132..176
    // range, so the endpoints visible in state are 130 and 178.
    if (renderer == nullptr && !resuming) {
        advanceCockpitIndicator();
    }
    CampaignRetirement retirement{behaviorTarget, continuation};
    const auto removeLevel2Entity = [this, &retirement](const std::size_t removed) noexcept {
        retireCampaignEntity(removed, retirement);
    };
    if (playableCampaignFrames_ && !resuming && fuelBay_.fixedSlot) {
        level2ArmorWarningActive_=false; //0EF8 clears source129C before either branch.
        const auto slot = *fuelBay_.fixedSlot;
        const auto owner = std::find(level2EntityFixedPoolSlots_.begin(),level2EntityFixedPoolSlots_.end(),slot);
        if (owner == level2EntityFixedPoolSlots_.end() || level2FixedPool_[slot].type != FuelBayRecovery::stationType) {
            throw std::logic_error("fuel bay lost its retained physical station");
        }
        const auto index = static_cast<std::size_t>(owner-level2EntityFixedPoolSlots_.begin());
        if (++fuelBay_.frame == FuelBayRecovery::duration) {
            removeLevel2Entity(index);
            level2FixedPool_[slot].stateBytes4dTo52[1] = 0;
            fuelBay_.fixedSlot.reset();fuelBay_.frame=0;
            fuelBay_.yawIncrement=FuelBayRecovery::completedYaw;fuelBay_.liftIncrement=FuelBayRecovery::initialLift;
            level2Score_ += scaledCampaignAward(FuelBayRecovery::award);
            level2AudioEvents_.push_back({level2LoopTick_,FuelBayRecovery::completeEvent});
            level1_.repairBonusSystem(FuelBayRecovery::repairPayload);
            //0F40's B452(1) dispatches the existing SBL op14 no-op.
        } else {
            level2PlayerArmor_ = FuelBayRecovery::maximumArmor;
            auto& station=level2Entities_[index];
            station.position.z += fuelBay_.liftIncrement;
            station.yaw = static_cast<std::uint16_t>(station.yaw+fuelBay_.yawIncrement);
            fuelBay_.yawIncrement = static_cast<std::uint16_t>(fuelBay_.yawIncrement+FuelBayRecovery::initialYaw);
            fuelBay_.liftIncrement = static_cast<std::int16_t>(fuelBay_.liftIncrement+FuelBayRecovery::liftStep);
            level2FixedPool_[slot] = station;
        }
    }

    const auto spawnLevel2TransientEffect = [this](const Level1Entity& source,
        const std::uint8_t firstFrame, const std::uint8_t terminalFrame) noexcept {
        return spawnCampaignTransientEffect(source, firstFrame, terminalFrame);
    };
    const auto rewardDomain = physicalLevel2Guardian ? CampaignRewardDomain::physicalLevel2Guardian : CampaignRewardDomain::shared;
    const auto deployGuardianReward = [this, &level2ActiveIdentitiesAtStart, rewardDomain](
        const Level1Entity& source, const CampaignExit exit) {
        deployCampaignReward(source, exit, level2ActiveIdentitiesAtStart, rewardDomain);
    };
    const CampaignCombatCallbacks combat{removeLevel2Entity, spawnLevel2TransientEffect, deployGuardianReward};
    const auto playerContact = resolveCampaignPlayerContact(contactStart, continuation, renderer, combat);
    if (playerContact.flow == CampaignContactFlow::suspendFrame) {
        return;
    }

    const auto terminalPlayerProjectiles = advanceCampaignProjectileLifetimes(level2ProjectileActiveAtStart, retirement);

    // A terminal effect has already left the allocation list, but its pointer
    // remains in FUN_1000:9298's presenter list for one more boundary.  That
    // final refresh survives in the freed record and is observable when the
    // pool slot is reused.
    for (std::size_t slot = 0;
         slot < level2DeathEffectPoolSize_; ++slot) {
        if (
            resuming || !level2DeathEffectPoolPendingPresenterRetirement_[slot]
            || level2DeathEffectPoolActive_[slot]
        ) {
            continue;
        }
        auto& effect = level2DeathEffectPool_[slot];
        effect.presenterDistance = approximatePlanarDistance(
            level2Player_.position,
            effect.position
        );
        effect.targetHeading = presenterTargetHeading(
            effect.position,
            level2Player_.position
        );
        level2DeathEffectPoolPendingPresenterRetirement_[slot] = false;
    }

    // 0AC8 is a main-presenter callback, after 3768's membership rebuild.
    // Retain that list and its compact identities before terminal effects leave it.
    const auto level2EffectPresenterOrder = level2ActiveListOrder_;
    const auto level2EffectPresenterOrderRebuilt = retirement.lastNonEffectRemoval != static_cast<std::size_t>(-1);
    std::array<std::size_t, level2DeathEffectPoolSize_> level2DeathEffectIndexAtPresenter{};
    level2DeathEffectIndexAtPresenter.fill(static_cast<std::size_t>(-1));
    for (std::size_t index = 0; index < level2Entities_.size(); ++index) {
        const auto effectSlot = level2EntityDeathEffectPoolSlots_[index];
        if (effectSlot < level2DeathEffectPoolSize_) {
            level2DeathEffectIndexAtPresenter[effectSlot] = index;
        }
    }
    for (std::size_t index = 0; index < level2Entities_.size();) {
        const auto effectSlot = level2EntityDeathEffectPoolSlots_[index];
        if (
            resuming || effectSlot >= level2DeathEffectPoolSize_
            || !level2DeathEffectPoolActive_[effectSlot]
        ) {
            ++index;
            continue;
        }
        auto& effect = level2Entities_[index];
        effect.presenterDistance = approximatePlanarDistance(
            level2Player_.position,
            effect.position
        );
        effect.targetHeading = presenterTargetHeading(
            effect.position,
            level2Player_.position
        );
        const auto nextFrame = static_cast<std::uint8_t>(
            effect.stateBytes4dTo52[1] + 1U
        );
        if (nextFrame >= effect.stateBytes4dTo52[4]) {
            auto terminalPresentation = effect;
            terminalPresentation.stateBytes4dTo52[1] = nextFrame;
            level2PendingTerminalEffectPresentations_.push_back(
                terminalPresentation
            );
            level2PendingTerminalEffectSlots_.push_back(effectSlot);
            auto retiredSemantic = effect;
            retiredSemantic.stateBytes4dTo52[1] = 0;
            retiredSemantic.stateBytes4dTo52[4] = 0;
            if (
                level2DeathEffectIndexAtPresenter[effectSlot]
                    < static_cast<std::size_t>(-1)
            ) {
                retiredSemantic.allocationIndex =
                    level2DeathEffectIndexAtPresenter[effectSlot];
            }
            level2RetiredEffectSemanticEntities_.push_back(retiredSemantic);
            level2RetiredEffectSemanticActiveOrder_ = level2EffectPresenterOrder;
            level2RetiredEffectSemanticOrderRebuilt_ = level2EffectPresenterOrderRebuilt;
            const auto activePosition = std::find(
                level2EffectPresenterOrder.begin(),
                level2EffectPresenterOrder.end(),
                level2DeathEffectIndexAtPresenter[effectSlot]
            );
            level2RetiredEffectSemanticActivePositions_.push_back(
                activePosition == level2EffectPresenterOrder.end()
                    ? level2EffectPresenterOrder.size()
                    : static_cast<std::size_t>(
                        activePosition - level2EffectPresenterOrder.begin()
                    )
            );
            level2DeathEffectPoolPendingPresenterRetirement_[effectSlot] =
                true;
            removeLevel2Entity(index);
            level2DeathEffectPool_[effectSlot].stateBytes4dTo52[1] = 0;
            level2DeathEffectPool_[effectSlot].stateBytes4dTo52[4] = 0;
            continue;
        }
        effect.stateBytes4dTo52[1] = nextFrame;
        ++index;
    }

    std::array<FixedPosition, level2ActorPoolSize_>
        level2ActorPositionsAtUpdateStart{};
    std::array<bool, level2ActorPoolSize_>
        level2ActorPositionAtUpdateStartValid{};
    std::array<std::size_t, level2ActorPoolSize_>
        level2ActorUpdateOrder{};
    level2ActorUpdateOrder.fill(static_cast<std::size_t>(-1));
    for (std::size_t index = 0; index < level2Entities_.size(); ++index) {
        const auto actorSlot = level2EntityActorPoolSlots_[index];
        if (actorSlot < level2ActorPoolSize_) {
            level2ActorPositionsAtUpdateStart[actorSlot] =
                level2Entities_[index].position;
            level2ActorPositionAtUpdateStartValid[actorSlot] = true;
        }
    }
    for (const auto identity : level2ActiveIdentitiesAtStart) {
        if (identity.pool == 0 && identity.slot < level2ActorPoolSize_) {
            // FUN_1000:0784 runs before the presenter/update walk retires a
            // terminal Type -1 record. A portable early retirement rebuild
            // must not alter whether a collision target has already received
            // its common integration. Route-v32 loop 2134->2135 observes the
            // returning Type 44 against the later guardian's start position.
            level2ActorUpdateOrder[identity.slot] = identity.activePosition;
        }
    }

    std::vector<std::size_t> kinematicOrder;
    if (retainedLevel3) {
        //094F traverses inherited pointers; 0982 integrates FE42, but not FC24/FE31.
        for (const auto index : remapLevel2BoundaryOrder(level2ActiveIdentitiesAtStart)) {
            if (level2Entities_[index].stateBytes4dTo52[0] == 0xff
                || (playableCampaignFrames_ && (level2Entities_[index].stateBytes4dTo52[0] == 0xfd
                    || (level2Entities_[index].stateBytes4dTo52[0] == 0xfe && level2Entities_[index].type == 42)))) {
                kinematicOrder.push_back(index);
            }
        }
    } else if (activeCampaignLevel_ == 4) {
        for (const auto identity : level2ActiveIdentitiesAtStart) {
            if (identity.pool > 3) {
                throw std::invalid_argument("Level5 movement requires a supported pool identity");
            }
            if (identity.pool == 3 && identity.slot >= level2FixedObjectPoolSize_) {
                throw std::invalid_argument("Level5 fixed movement requires a physical pool slot");
            }
            const auto& slots = identity.pool == 0 ? level2EntityActorPoolSlots_
                : identity.pool == 1 ? level2EntityProjectilePoolSlots_
                : identity.pool == 2 ? level2EntityDeathEffectPoolSlots_ : level2EntityFixedPoolSlots_;
            const auto slot = std::find(slots.begin(), slots.end(), identity.slot);
            if (slot != slots.end()) {
                const auto index = static_cast<std::size_t>(slot - slots.begin());
                const auto type = level2Entities_[index].type;
                if (identity.pool == 1 && type != 19 && type != 20 && type != 22 && type != 23) {
                    throw std::invalid_argument("Level5 movement requires its supported projectile");
                }
                if (identity.pool == 2 && type != -1) {
                    throw std::invalid_argument("Level5 movement requires its bitmap effect record");
                }
                if (identity.pool == 3 && ((type != 42 && type != 25)
                    || level2Entities_[index].renderProfile != WorldActorRenderProfile::inlineSelectorNoCallback)) {
                    throw std::invalid_argument("Level5 fixed traversal requires its source bonus or reward");
                }
                kinematicOrder.push_back(index);
            } else if (identity.pool == 3) {
                auto& retired = level2FixedPool_[identity.slot];
                if (!level2FixedPoolKnown_[identity.slot] || retired.type != 42
                    || retired.renderProfile != WorldActorRenderProfile::inlineSelectorNoCallback) {
                    throw std::invalid_argument("Level5 removed fixed movement requires retained bonus history");
                }
                advanceEntityKinematics(retired);
            }
        }
    } else {
        for (std::size_t index = 0; index < level2Entities_.size(); ++index) {
            kinematicOrder.push_back(index);
        }
    }
    for (const auto index : kinematicOrder) {
        const auto projectileSlot =
            level2EntityProjectilePoolSlots_[index];
        if (
            (
                projectileSlot < level2ProjectilePoolSize_
                && !level2ProjectileActiveAtStart[projectileSlot]
            )
            || !actorExistedAtStart(index)
            || level2Entities_[index].type == 25
            || level2Entities_[index].type == 26
        ) {
            continue;
        }
        const auto category=level2Entities_[index].stateBytes4dTo52[0];
        if (resuming && (category==252 || (category==254 && level2Entities_[index].type!=42))) {
            continue;
        }
        const auto actorSlot = level2EntityActorPoolSlots_[index];
        if (activeCampaignLevel_ == 4 && actorSlot < level2ActorPoolSize_
            && level2ActorUpdateOrder[actorSlot] == static_cast<std::size_t>(-1)) {
            continue;
        }
        advanceEntityKinematics(level2Entities_[index]);
    }
    const CampaignContactMotion contactMotion{level2ActorPositionsAtUpdateStart,
        level2ActorPositionAtUpdateStartValid, level2ActorUpdateOrder};
    const auto projectileContact = resolveCampaignPlayerProjectileHits(contactStart, contactMotion, combat);
    if (projectileContact.flow == CampaignContactFlow::suspendFrame) {
        return;
    }
    const auto retiredType48ActorSlot = projectileContact.retiredType48ActorSlot;
    if (activeCampaignLevel_ == 4) {
        retireCampaignTerminalPlayerProjectiles(terminalPlayerProjectiles, retirement);
    }
    if (playerContact.retiredType48ActorSlot < level2ActorPoolSize_) {
        const auto pendingType48 = std::find(
            level2EntityActorPoolSlots_.begin(),
            level2EntityActorPoolSlots_.end(),
            playerContact.retiredType48ActorSlot
        );
        if (pendingType48 != level2EntityActorPoolSlots_.end()) {
            const auto pendingIndex = static_cast<std::size_t>(
                pendingType48 - level2EntityActorPoolSlots_.begin()
            );
            if (
                pendingIndex < level2Entities_.size()
                && level2Entities_[pendingIndex].type == 48
                && level2Entities_[pendingIndex]
                    .stateBytes4dTo52[1] == 0
            ) {
                removeLevel2Entity(pendingIndex);
            }
        }
    }
    if (resolveCampaignActorContacts(contactStart, contactMotion, combat) == CampaignContactFlow::suspendFrame) {
        return;
    }
    if (resuming) {
        //Inactive025A skips9298 and every presenter callback after0784.
        //Its0A9A epilogue still addresses128E after earlier pointer removals.
        const auto fixedSlot=level2SuccessContinuation_->fixedSlot;
        const auto reward=std::find(level2EntityFixedPoolSlots_.begin(),level2EntityFixedPoolSlots_.end(),fixedSlot);
        if (reward==level2EntityFixedPoolSlots_.end()) {
            throw std::logic_error("playable success lost its physical reward during the remaining actor walk");
        }
        level2PickupIndex_=static_cast<std::size_t>(reward-level2EntityFixedPoolSlots_.begin());
        advanceSourcePickupSelector(level2Entities_[level2PickupIndex_]);
        return;
    }
    for (std::size_t index = 0;
         index < level2Entities_.size(); ++index) {
        if (!actorExistedAtStart(index)) {
            continue;
        }
        auto& entity = level2Entities_[index];
        const auto projectileSlot =
            level2EntityProjectilePoolSlots_[index];
        const auto deathEffectSlot =
            level2EntityDeathEffectPoolSlots_[index];
        const auto fixedRecord =
            level2EntityActorPoolSlots_[index] >= level2ActorPoolSize_
            && projectileSlot >= level2ProjectilePoolSize_
            && deathEffectSlot >= level2DeathEffectPoolSize_;
        if (fixedRecord) {
            const auto fixedSlot = level2FixedPoolSlot(index);
            const auto existedAtStart = std::any_of(
                level2ActiveIdentitiesAtStart.begin(),
                level2ActiveIdentitiesAtStart.end(),
                [fixedSlot](const Level2EntityIdentity& identity) noexcept {
                    return identity.pool == 3
                        && identity.slot == fixedSlot;
                }
            );
            if (!existedAtStart) {
                continue;
            }
        }
        if (
            (
                projectileSlot < level2ProjectilePoolSize_
                && !level2ProjectileActiveAtStart[projectileSlot]
            )
            || deathEffectSlot < level2DeathEffectPoolSize_
        ) {
            continue;
        }
        const auto actorSlot = level2EntityActorPoolSlots_[index];
        if ((activeCampaignLevel_ == 4 || retainedLevel3) && actorSlot < level2ActorPoolSize_
            && level2ActorUpdateOrder[actorSlot] == static_cast<std::size_t>(-1)) {
            continue;
        }
        entity.presenterDistance = approximatePlanarDistance(
            behaviorTarget.position,
            entity.position
        );
        entity.targetHeading = presenterTargetHeading(
            entity.position,
            behaviorTarget.position
        );
    }
    if (activeCampaignLevel_ == 4) {
        for (const auto identity : level2ActiveIdentitiesAtStart) {
            if (identity.pool != 3 || std::find(level2EntityFixedPoolSlots_.begin(),
                level2EntityFixedPoolSlots_.end(), identity.slot) != level2EntityFixedPoolSlots_.end()) {
                continue;
            }
            auto& retired = level2FixedPool_[identity.slot];
            retired.presenterDistance = approximatePlanarDistance(behaviorTarget.position, retired.position);
            retired.targetHeading = presenterTargetHeading(retired.position, behaviorTarget.position);
        }
    }
    //0784's0A7F epilogue oscillates128E independently of the inherited list.
    //A reward born inside a projectile callback goes2->3 before its first main.
    advanceLevel2Pickup();
    const auto actorWalk = advanceCampaignActorWalk(contactStart, behaviorTarget,
        projectileContact, combat, physicalLevel2Guardian);
    // FUN_1000:b996 owns bit 2 for every transformable object. FUN_1000:9298
    // invokes behavior first, then refreshes this main-view center flag on
    // the inherited presenter list. Level 3 loop 1558 exposes the clear
    // transition on a live Type -1 effect behind the player.
    auto mainView = initialLevel1MainView();
    mainView.origin = level2Player_.position;
    mainView.matrix = makeFixedMatrixQ14(
        level2Player_.pitch,
        level2Player_.roll,
        level2Player_.yaw
    );
    transpose(mainView.matrix);
    for (auto& entity : level2Entities_) {
        if (!entity.active() || (entity.flags & 0x08U) == 0) {
            continue;
        }
        entity.flags = static_cast<std::uint8_t>(
            entity.flags & static_cast<std::uint8_t>(~0x04U)
        );
        const auto local = transformToView(entity.position, mainView);
        if (local.y <= 0) {
            continue;
        }
        const auto projected = projectToViewport(local, mainView);
        if (
            projected.x >= mainView.viewport.left
            && projected.x <= mainView.viewport.right
            && projected.y >= mainView.viewport.top
            && projected.y <= mainView.viewport.bottom
        ) {
            entity.flags = static_cast<std::uint8_t>(
                entity.flags | 0x04U
            );
        }
    }
    if (actorWalk.retireCompletionObject < level2Entities_.size()) {
        removeLevel2Entity(actorWalk.retireCompletionObject);
        level2CompletionObjectIndex_ = static_cast<std::size_t>(-1);
        if (
            !playableCampaignFrames_ && level2PostWaveObjectIndex_ > actorWalk.retireCompletionObject
            && level2PostWaveObjectIndex_ < static_cast<std::size_t>(-1)
        ) {
            --level2PostWaveObjectIndex_;
        }
        if (
            !playableCampaignFrames_ && level2PickupIndex_ > actorWalk.retireCompletionObject
            && level2PickupIndex_ < static_cast<std::size_t>(-1)
        ) {
            --level2PickupIndex_;
        }
    }
    if (actorWalk.spawnGuardianType48 || actorWalk.spawnGuardianType47) {
        // FUN_1000:ae10 allocates through the ordinary actor initializer;
        // its overwritten defaults and model setup consume three Stellar
        // RNG words before the ground updater runs. The campaign index
        // selects Type 48 for Level 2 and Type 47 for Level 3.
        level2Rng_.next();
        level2Rng_.next();
        level2Rng_.next();
        Level1Entity emitted{};
        emitted.type = actorWalk.spawnGuardianType47 ? 47 : 48;
        emitted.collisionRadius = playableCampaignFrames_ && campaignModelsBound_ ? campaignModels_[emitted.type].radius
            : actorWalk.spawnGuardianType47 ? 0 : 559;
        emitted.position = actorWalk.guardianType48Source.position;
        emitted.position.z += 500;
        emitted.pitch = actorWalk.guardianType48Source.pitch;
        emitted.roll = 0;
        emitted.yaw = actorWalk.guardianType48Source.yaw;
        emitted.stateBytes4dTo52 = {255, 1, 0, 0, 4, 0};
        emitted.behaviorTimer = nextLevel2VisualRandomByte();
        const auto emittedIndex = allocateLevel2Actor(emitted);
        if (emittedIndex != static_cast<std::size_t>(-1)) {
            if (activeCampaignLevel_ == 1 && !actorWalk.spawnGuardianType47) {
                level2Entities_[emittedIndex].renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
            }
            const auto emittedActorSlot =
                level2EntityActorPoolSlots_[emittedIndex];
            if (
                !actorWalk.spawnGuardianType47
                && emittedActorSlot == retiredType48ActorSlot
            ) {
                // The replacement occupies a Type-48 record retired earlier
                // in the native pointer walk. Its pending callback therefore
                // advances the replacement on this boundary (v13 tick 10138).
                auto& replacement = level2Entities_[emittedIndex];
                replacement.presenterDistance =
                    actorWalk.guardianType48Source.presenterDistance;
                replacement.targetHeading =
                    actorWalk.guardianType48Source.targetHeading;
                advanceCampaignType48(replacement);
            }
            ++level2ActiveEnemyCount_;
            ++level2SpawnedEnemyCount_;
        }
    }
    if (actorWalk.level3Burst) {
        deployLevel3GuardianBurst(*actorWalk.level3Burst);
    }
    if (actorWalk.postWaveSource && level2PostWaveObjectIndex_ == static_cast<std::size_t>(-1)) {
        const bool guardianMetadata = playableCampaignFrames_
            ? campaignModelsBound_ && campaignModels_[44].radius != 0 && campaignModels_[44].animationLimit != 0
            : level5GuardianRadius_ != 0 && level5GuardianAnimationLimit_ != 0;
        if (activeCampaignLevel_ == 4 && (!guardianMetadata
            || std::find(level2ActorPoolActive_.begin(), level2ActorPoolActive_.end(), false) == level2ActorPoolActive_.end())) {
            throw std::invalid_argument("Level5 guardian requires local model metadata and a free actor record");
        }
        level2Rng_.next();
        level2Rng_.next();
        Level1Entity deployed{};
        deployed.type = 44;
        // FUN_1000:477a -> 4370 -> d03c uses the live model header.
        // Intact LEVEL2 local model 0 and the native guardian record both
        // give 1147. The old v13 zero came from a transplanted model heap.
        // Level 3's separate initialization remains an evidence frontier.
        deployed.collisionRadius = playableCampaignFrames_ && campaignModelsBound_ ? campaignModels_[44].radius
            : activeCampaignLevel_ == 4 ? level5GuardianRadius_
            : activeCampaignLevel_ == 1 ? 1147 : 0;
        deployed.position = {
            actorWalk.postWaveSource->position.x,
            actorWalk.postWaveSource->position.y,
            0,
        };
        deployed.yaw = actorWalk.postWaveSource->targetHeading;
        deployed.stateBytes4dTo52 = {255, playableCampaignFrames_ ? campaignGuardianInitialHealth(static_cast<std::uint8_t>(activeCampaignLevel_),runtimeDifficulty_)
            : activeCampaignLevel_ == 4 ? level5GuardianHealth : std::uint8_t{20}, 2, 0, 5, 0};
        deployed.animatedModelPresenterEnabled = playableCampaignFrames_ || activeCampaignLevel_ == 1 || activeCampaignLevel_ == 4;
        deployed.renderProfile = WorldActorRenderProfile::heapSelectorDeferredAnimation;
        level2PostWaveObjectIndex_ = allocateLevel2Actor(deployed, activeCampaignLevel_ == 4
            ? ActorInitialization::validatedOrdinary : ActorInitialization::legacy);
        if (activeCampaignLevel_ == 1 && level2PostWaveObjectIndex_ < level2Entities_.size()) {
            level2Entities_[level2PostWaveObjectIndex_].renderProfile =
                WorldActorRenderProfile::heapSelectorDeferredAnimation;
        }
        retainPhysicalPostWaveRole();
        ++level2ActiveEnemyCount_;
        ++level2SpawnedEnemyCount_;
    }
    //Keep the qualified Level2/3 boundary; Level5 deploys in the actual tail.
    if (!playableCampaignFrames_ && activeCampaignLevel_ != 4 && (level2LoopTick_ & 0x000fU) == 1U) {
        runLevel2CompletionCheck();
    }
    // FUN_1000:9298 runs callbacks in the inherited pointer order before
    // FUN_1000:14b0 recomputes live distances and selection-sorts them.
    // FUN_1000:3768 then replaces that order with allocation order whenever
    // a callback changed pool membership.
    auto actorAllocatedThisBoundary = false;
    for (std::size_t index = 0;
         index < level2ActorPoolActive_.size(); ++index) {
        actorAllocatedThisBoundary = actorAllocatedThisBoundary
            || (
                level2ActorPoolActive_[index]
                && !level2ActorActiveAtStart[index]
            );
    }
    if (
        retirement.lastNonEffectRemoval == static_cast<std::size_t>(-1)
        && !level2PlayerProjectileAllocated
        && !actorAllocatedThisBoundary
        && !level2RetiredEffectRebuildReported_
        && retirement.effectCount <= level2EntityCountAtStart
        && level2Entities_.size()
            == level2EntityCountAtStart - retirement.effectCount
    ) {
        sortLevel2ActiveByDistance();
    }
    if (retirement.lastNonEffectRemoval != static_cast<std::size_t>(-1)
        || level2PlayerProjectileAllocated || actorAllocatedThisBoundary
        || level2RetiredEffectRebuildReported_ || retirement.effectCount != 0
        || level2Entities_.size() != level2EntityCountAtStart) {
        if (actorWalk.guardianAllocatedThisBoundary && level2Visibility_.tracking() == VisibilityTracking::unknown) {
            //The legacy component presenter rebuilds membership without a source visibility receipt.
            level2ActiveListOrder_ = level2AllocationListOrder_;
        }
        if (activeCampaignLevel_ != 4 && !playableLevel1 && !actorWalk.guardianAllocatedThisBoundary) {
            level2Visibility_.invalidate();
        } else if (retirement.lastNonEffectRemoval != static_cast<std::size_t>(-1) || retirement.effectCount != 0) {
            level2Visibility_.removalSucceeded();
        } else {
            level2Visibility_.allocationSucceeded();
        }
    }
    refreshLevel2Visibility();
    if (!resuming) { presentLevel2(input, renderer, firstFrame); }
}

void GameplayModel::advanceLevel2Pickup() noexcept {
    if (playableCampaignFrames_) {
        for (std::size_t index = 0; index < level2Entities_.size(); ++index) {
            auto& pickup = level2Entities_[index];
            if (pickup.type != 25 && pickup.type != 26) { continue; }
            advanceSourcePickupSelector(pickup);
            const auto slot = level2EntityFixedPoolSlots_[index];
            if (slot < level2FixedObjectPoolSize_) { level2FixedPool_[slot] = pickup; }
        }
        return;
    }
    if (activeCampaignLevel_ != 4 || level2PickupIndex_ >= level2Entities_.size()) {
        return;
    }
    auto& pickup = level2Entities_[level2PickupIndex_];
    advanceSourcePickupSelector(pickup);
    level2FixedPool_[level2EntityFixedPoolSlots_[level2PickupIndex_]] = pickup;
}

void GameplayModel::applyLevel2Prefix(const StationaryLevel2PrefixWork& prefix) {
    if(prefix.projectileReceipt) {
        commitRecoveredLevel2ProjectilePrefix(prefix);
        return;
    }
    const auto first = phase_ == GameplayPhase::level2PreloopReturn;
    std::optional<VisibilityState> firstVisibility;
    if (first) {
        if (!level2PrefixGuards_ || level2HasAdvanced_ || level2TailConsumed_
            || !level2ActiveListOrder_.empty() || !level2Visibility_.refreshRequested()) {
            throw std::invalid_argument("first source Level2 prefix lost its preloop state");
        }
        const auto& guards = *level2PrefixGuards_;
        const auto expected = level2ResetPreloopPrefixOperations({{level2Player_, level2Entities_, {},
            level1_.projectionListOrigin_, true, guards.fadeTicks, guards.specialActorPresent,
            level2PlayerCollisionMomentum_, true, guards.effectHandles}, level2AllocationListOrder_});
        if (!sameFirstLevel2Actor(prefix.player, expected.player)
            || prefix.actors.size() != expected.actors.size() || prefix.activeOrder != expected.activeOrder
            || !std::equal(prefix.actors.begin(), prefix.actors.end(), expected.actors.begin(), sameFirstLevel2Actor)
            || prefix.operations.size() != expected.operations.size()
            || !std::equal(prefix.operations.begin(), prefix.operations.end(), expected.operations.begin(),
                [](const auto& left, const auto& right) {
                    return left.kind == right.kind && left.count == right.count && left.value == right.value;
                })) {
            throw std::invalid_argument("first source Level2 prefix differs from its recovered receipt");
        }
        // Adopt the completed3768 receipt through the existing visibility owner.
        // Only this local copy refreshes until the whole returned receipt validates.
        firstVisibility = level2Visibility_;
        auto actors = prefix.actors;
        const auto visible = firstVisibility->refresh(prefix.player.position, actors,
            level2AllocationListOrder_, level2ActiveListOrder_);
        if (!visible.refreshed || visible.activeOrder != prefix.activeOrder
            || firstVisibility->refreshRequested() != expected.refreshRequested
            || !firstVisibility->lastCamera() || !(*firstVisibility->lastCamera() == expected.lastListCamera)
            || !std::equal(actors.begin(), actors.end(), prefix.actors.begin(), sameFirstLevel2Actor)) {
            throw std::invalid_argument("first source Level2 visibility adoption differs from3768");
        }
    }
    if (prefix.actors.size() != level2Entities_.size()
        || (!first && prefix.activeOrder.size() != level2ActiveListOrder_.size())
        || prefix.player.allocationIndex != level2Player_.allocationIndex
        || prefix.player.type != level2Player_.type) {
        throw std::invalid_argument("source prefix changed entity membership");
    }
    std::vector<bool> seen(prefix.actors.size());
    for (std::size_t index = 0; index < prefix.actors.size(); ++index) {
        if (prefix.actors[index].allocationIndex != level2Entities_[index].allocationIndex
            || prefix.actors[index].type != level2Entities_[index].type) {
            throw std::invalid_argument("source prefix changed allocation identity");
        }
    }
    for (const auto index : prefix.activeOrder) {
        if (index >= seen.size() || seen[index]) {
            throw std::invalid_argument("source prefix returned invalid active order");
        }
        seen[index] = true;
    }
    level2Player_ = prefix.player;
    level2Entities_ = prefix.actors;
    level2ActiveListOrder_ = prefix.activeOrder;
    if (firstVisibility) {
        level2Visibility_ = std::move(*firstVisibility);
    }
}

void GameplayModel::refreshLevel2Visibility() {
    if (level2Visibility_.tracking() == VisibilityTracking::unknown) {
        return;
    }
    if (playableCampaignFrames_ || activeCampaignLevel_ == 4
        || phase_ == GameplayPhase::level3Native || phase_==GameplayPhase::playable) {
        const auto visible = level2Visibility_.refresh(level2Player_.position,
            level2Entities_, level2AllocationListOrder_, level2ActiveListOrder_, playableCampaignFrames_
                ? VisibilityDomain::playableCampaign : VisibilityDomain::nativeGameplay);
        level2ActiveListOrder_ = visible.activeOrder;
        return;
    }
    if (level2Entities_.size() != 7
        || std::any_of(level2Entities_.begin(), level2Entities_.end(), [](const auto& actor) {
            return !actor.active() || (actor.type != 8 && actor.type != 13 && actor.type != 30);
        })) {
        level2Visibility_.invalidate();
        return;
    }
    try {
        auto actors = level2Entities_;
        const std::vector<std::size_t> emptyOrder;
        const auto& inheritedOrder = level2Visibility_.refreshRequested()
            ? emptyOrder : level2ActiveListOrder_;
        const auto visible = level2Visibility_.refresh(level2Player_.position,
            actors, level2AllocationListOrder_, inheritedOrder);
        if (visible.activeOrder != level2ActiveListOrder_) {
            level2Visibility_.invalidate();
            return;
        }
        level2Entities_ = std::move(actors);
    } catch (const std::invalid_argument&) {
        // Legacy movement/removal paths remain usable, but cannot admit the
        // source prefix until their visibility branches have been recovered.
        level2Visibility_.invalidate();
    }
}

void GameplayModel::presentLevel2(const GameplayInput& input,
    Level2FrameRenderer* renderer, const bool firstFrame) {
    if (renderer != nullptr && std::any_of(level2Entities_.begin(), level2Entities_.end(), [](const auto& actor) {
        return actor.type == 7 && actor.stateBytes4dTo52[5] == 3 && actor.behaviorTimer > 0x1d;
    })) {
        throw std::invalid_argument("completion beam requires the native pixel presentation boundary");
    }
    auto sightExecution = Level2SightExecution::pending;
    auto cockpitExecution = Level2CockpitExecution::pending;
    if (renderer != nullptr) {
        renderer->render({level2Ground_, level2Rng_, level2Player_, level2Entities_,
            level2ActiveListOrder_, alternateSightPhase_, sightExecution,
            cockpitIndicatorX_, cockpitIndicatorVelocity_, cockpitExecution});
        if (cockpitExecution == Level2CockpitExecution::pending) {
            advanceCockpitIndicator();
        }
    } else {
        level2Ground_.advance(
            level2Player_.yawVelocity,
            level2Player_.forwardVelocity,
            level2Rng_
        );
    }
    auto mainView = initialLevel1MainView();
    mainView.origin = level2Player_.position;
    mainView.matrix = makeFixedMatrixQ14(level2Player_.pitch, level2Player_.roll, level2Player_.yaw);
    transpose(mainView.matrix);
    std::vector<std::optional<CompletionBeamPresentation>> beams(level2Entities_.size());
    // 002B7 ground precedes B996/B0B4. Retain callback output once for pixel delivery.
    for (const auto index : level2ActiveListOrder_) {
        auto& actor = level2Entities_.at(index);
        if (actor.type != 7 || actor.stateBytes4dTo52[5] != 3 || actor.behaviorTimer <= 0x1d) {
            continue;
        }
        beams[index] = presentCompletionBeam(actor, mainView, level2Rng_, CompletionBeamView::main);
        actor.flags = beams[index]->projected ? 12 : 8;
    }
    // The native presenter sees entities allocated or advanced on this
    // boundary. Its transient updater frees terminal effects before the
    // draw, so retain their terminal frame in the presentation snapshot.
    for (auto& entity : level2Entities_) {
        if ((entity.animatedModelPresenterEnabled
                || entity.renderProfile == WorldActorRenderProfile::heapSelectorNoCallback)
            && (playableCampaignFrames_ || (activeCampaignLevel_ == 1 && (entity.type == 44 || entity.type == 45))
                || ((phase_ == GameplayPhase::level3Native || phase_==GameplayPhase::playable) && entity.type == 24)
                || (activeCampaignLevel_ == 4 && (entity.type == 11 || entity.type == 44)))) {
            snapshotHeapAnimation(entity);
        }
    }
    advanceLevel2GuardianAnimation();
    if (phase_ == GameplayPhase::level3Native || phase_==GameplayPhase::playable) {
        advanceLevel3Animation(); //Main geometry used the selector saved immediately above.
    }
    snapshotLevel2Presentation(std::move(beams));
    level2PendingInput_ = input;
    if (sightExecution == Level2SightExecution::pending) {
        refreshLevel2SightPhase();
    }
    if (renderer == nullptr) {
        return;
    }
    FrameTailState state{level2LoopTick_, level2Player_.position,
        level2Player_.yawVelocity, level2Player_.forwardVelocity,
        {level2Player_.weaponCooldown, level2Player_.weaponSlotTimers[0],
            level2Player_.weaponSlotTimers[1]}, {}};
    for (const auto index : level2ActiveListOrder_) {
        const auto& actor = level2Entities_.at(index);
        state.actors.push_back({actor.type, actor.position});
    }
    // The caller must supply controller-owned fields from its retained owner.
    // Current input belongs to this tail, not the deferred one.
    FrameTailGuards guards{};
    guards.armor = level2PlayerArmor_;
    guards.heldKeys = {false, input.reverse, false, false, input.forward,
        false, input.left, input.right, input.fire};
    guards.firstFrame = firstFrame;
    guards.contactSlow = level2Type46ContactSlowdown_;
    guards.success = level2Complete_;
    guards.missionActive = phase_ == GameplayPhase::level2;
    if ((level2LoopTick_ & 0x000fU) == 0) {
        constexpr std::int16_t maximumThresholdIndex = 15;
        const auto level = static_cast<std::uint8_t>(activeCampaignLevel_);
        const auto definition = campaignLevelDefinition(level, campaignDifficultyIndex(runtimeDifficulty_));
        const auto activeIndex = static_cast<std::uint8_t>(std::clamp<std::int16_t>(
            level2ActiveEnemyCount_, 0, maximumThresholdIndex));
        guards.periodic = FrameTailPeriodicInput{level, level2ActiveEnemyCount_, level2SpawnedEnemyCount_,
            definition.table.enemyLimit, campaignSpawnThreshold(level, activeIndex),
            level2Rng_.indexA(), level2Rng_.indexB(), std::nullopt};
    }
    if (const auto tail = renderer->finish(state, guards)) {
        const auto sourceControls = renderer->prefixExecution() == Level2PrefixExecution::source
            && phase_ == GameplayPhase::level2 && !firstFrame && level2PrefixGuards_;
        const auto sourceLifecycle=sourceControls && (renderer->sourceDomain()==Level2SourceDomain::projectileLifecycle
            || renderer->sourceDomain()==Level2SourceDomain::periodicLifecycle);
        const auto sourceCombat=sourceLifecycle || (sourceControls && renderer->sourceDomain()==Level2SourceDomain::projectileCombat);
        if (((input.left || input.right || input.forward || input.reverse) && !sourceControls)
            || (input.fire && !sourceCombat) || input.exit || level2Player_.weaponSlotTimers[2] != 0) {
            throw std::invalid_argument("owned frame tail requires inactive current input and third slot");
        }
        if(sourceLifecycle) {
            const auto receipt=renderer->lifecycleTail();
            if(!receipt || receipt->work.state.loopTick!=tail->loopTick) {
                throw std::invalid_argument("Level2 lifecycle renderer omitted its source tail receipt");
            }
            if(renderer->sourceDomain()==Level2SourceDomain::periodicLifecycle
                && !renderer->periodicTail() && !renderer->ordinaryTail()) {
                throw std::invalid_argument("Level2 periodic renderer omitted its source tail profile");
            }
            if(const auto periodic=renderer->periodicTail()) {
                if(renderer->ordinaryTail()) {
                    throw std::invalid_argument("Level2 renderer selected two source tail profiles");
                }
                if(!periodic->ownedShared) {
                    throw std::invalid_argument("Level2 periodic renderer omitted its owned shared bank");
                }
                commitRecoveredLevel2PeriodicTail(*receipt,periodic->work,*periodic->ownedShared,
                    periodic->arrival ? &*periodic->arrival : nullptr,periodic->cachedEnemyFlag);
            } else if(const auto ordinary=renderer->ordinaryTail()) {
                if(ordinary->lifecycle!=receipt) {
                    throw std::invalid_argument("Level2 ordinary renderer changed its borrowed lifecycle receipt");
                }
                commitRecoveredLevel2OrdinaryTail(*receipt);
            } else {
                commitRecoveredLevel2LifecycleTail(*receipt);
            }
        } else if(sourceCombat) {commitRecoveredLevel2CombatTail(*tail);}
        else {applyLevel2Tail(*tail);}
    }
}

void GameplayModel::applyLevel2Tail(const FrameTailState& tail) {
    if (tail.loopTick != static_cast<std::uint16_t>(level2LoopTick_ + 1)
        || tail.actors.size() != level2ActiveListOrder_.size()) {
        throw std::invalid_argument("owned frame tail changed tick or actor membership");
    }
    for (std::size_t position = 0; position < tail.actors.size(); ++position) {
        if (tail.actors[position].type != level2Entities_.at(
                level2ActiveListOrder_[position]).type) {
            throw std::invalid_argument("owned frame tail changed actor identity");
        }
    }
    const auto periodic = (level2LoopTick_ & 0x000fU) == 0;
    constexpr std::size_t level2 = 1;
    constexpr std::int16_t initialEnemies = 5;
    constexpr std::int16_t warningArmor = 200;
    const auto scheduler = periodic ? FrameTailScheduler::zeroThresholdDraw : FrameTailScheduler::none;
    if (tail.scheduler != scheduler || (periodic && (activeCampaignLevel_ != level2
        || level2ActiveEnemyCount_ != initialEnemies || level2SpawnedEnemyCount_ != initialEnemies
        || level2PlayerArmor_ < warningArmor || campaignSpawnThreshold(level2, initialEnemies) != 0))) {
        throw std::invalid_argument("owned frame tail changed scheduler domain");
    }
    if (periodic) {
        level2Rng_.next();
    }
    level2Player_.position = tail.playerPosition;
    level2Player_.yawVelocity = tail.yawVelocity;
    level2Player_.forwardVelocity = tail.forwardVelocity;
    level2Player_.weaponCooldown = tail.weaponTimers[0];
    level2Player_.weaponSlotTimers[0] = tail.weaponTimers[1];
    level2Player_.weaponSlotTimers[1] = tail.weaponTimers[2];
    for (std::size_t position = 0; position < tail.actors.size(); ++position) {
        level2Entities_[level2ActiveListOrder_[position]].position = tail.actors[position].position;
    }
    level2LoopTick_ = tail.loopTick;
    level2TailConsumed_ = true;
}

void GameplayModel::startLevel2() noexcept {
    initializeLevel2({}, Level2EntryOrigin::diagnostic);
    initializeSyntheticCampaignIndicator();
    successAudioOwned_ = false;
    successAudioActive_ = false;
    phase_ = GameplayPhase::level2Handoff;
    presentationFrame_ = 0;
}

void GameplayModel::startLevel3() noexcept {
    initializeLevel3();
    initializeSyntheticCampaignIndicator();
    phase_ = GameplayPhase::level3Handoff;
    presentationFrame_ = 0;
}

void GameplayModel::setSuccessAudioActive(const bool active) noexcept {
    if (phase_ == GameplayPhase::successPresentation
        || phase_ == GameplayPhase::level2SuccessPresentation) {
        successAudioOwned_ = true;
        successAudioActive_ = active;
    }
}

void GameplayModel::beginHighscoreNameEntry() {
    if (phase_ != GameplayPhase::continuePrompt || !failureFromLevel2_ || activeCampaignLevel_ != 4
        || pressedContinueChoice_ != ContinueChoice::yes) {
        throw std::invalid_argument("highscore entry requires owned affirmative Level5 Continue");
    }
    pressedContinueChoice_.reset();
    phase_ = GameplayPhase::highscoreNameEntry;
    presentationFrame_ = 0;
}

void GameplayModel::validateLevel5Reload(const std::vector<Model3d>& sharedModels) const {
    if (!failureFromLevel2_ || activeCampaignLevel_ != 4 || remainingContinues_ == 0
        || sharedModels.empty() || campaignModelRadius(sharedModels, {}, 0) == 0
        || level1_.selectedSystem() != -1
        || std::any_of(level1_.systemStatusLevels().begin(), level1_.systemStatusLevels().end(),
            [](const auto status) { return status <= 0; })
        || std::any_of(level1_.systemTimers().begin(), level1_.systemTimers().end(),
            [](const auto timer) { return timer != 0; })) {
        throw std::invalid_argument("Level5 retry requires owned failure return, remaining budget and untimed positive systems");
    }
    validateCampaignOrdinaryActors(campaignLevelDefinition(4, 0).table, sharedModels, {});
    level5OrdinaryConstructor(sharedModels);
}

void GameplayModel::validateHighscoreHall(const std::vector<Model3d>& sharedModels) const {
    if (phase_ != GameplayPhase::highscoreNameEntry) {
        throw std::invalid_argument("highscore submission requires its owned name-entry phase");
    }
    validateLevel5Reload(sharedModels);
}

void GameplayModel::beginHighscoreHall() {
    if (phase_ != GameplayPhase::highscoreNameEntry || !failureFromLevel2_ || activeCampaignLevel_ != 4) {
        throw std::invalid_argument("highscore hall requires its owned name return");
    }
    phase_ = GameplayPhase::highscoreHall;
    presentationFrame_ = 0;
}

void GameplayModel::validateLevel5Retry(const std::vector<Model3d>& sharedModels,
    const HighscoreReturn result) const {
    auto expectedPhase = GameplayPhase::continuePrompt;
    switch (result) {
    case HighscoreReturn::emptyName:
        expectedPhase = GameplayPhase::highscoreNameEntry;
        break;
    case HighscoreReturn::recordedName:
        expectedPhase = GameplayPhase::highscoreHall;
        break;
    case HighscoreReturn::rankMiss:
        break;
    default:
        throw std::invalid_argument("unsupported Level5 highscore return");
    }
    if (phase_ != expectedPhase
        || (result == HighscoreReturn::rankMiss && pressedContinueChoice_ != ContinueChoice::yes)) {
        throw std::invalid_argument("Level5 retry requires the matching highscore return phase");
    }
    validateLevel5Reload(sharedModels);
}

void GameplayModel::completeLevel5Retry(const std::vector<Model3d>& sharedModels,
    const HighscoreReturn result, const std::vector<Model3d>& levelModels) {
    validateLevel5Retry(sharedModels, result);
    validateLevel5GuardianModel(levelModels);
    constexpr std::int16_t retryArmor = 680;
    pressedContinueChoice_.reset();
    level2Score_ = 0;
    --remainingContinues_;
    level2PlayerArmor_ = retryArmor;
    level1_.resetCampaignTransientState();
    initializeLevel5(sharedModels, failureRng_, retryArmor, levelModels);
    travel_ = 0;
    weaponIndicatorVisible_ = false;
    weaponReadyIndicator_ = false;
    explosionTicksRemaining_ = 0;
    explosionAge_ = 0;
    failureAudioEvents_.clear();
    failureSourceLoopTick_.reset();
    ++ticks_;
}

void GameplayModel::advanceContinuePrompt(const GameplayInput& input) {
    switch (updateContinueChoice(input)) {
    case ContinueDecision::yes:
        if (failureFromLevel2_) {
            retryLevel2(input);
        } else {
            retryLevel1();
        }
        break;
    case ContinueDecision::no:
    case ContinueDecision::cancelled:
        phase_ = GameplayPhase::campaignEnded;
        break;
    case ContinueDecision::pending:
        break;
    }
}

void GameplayModel::retryLevel1() {
    if (failureFromLevel2_) {
        throw std::runtime_error("continue reload for Level2/3 is not implemented");
    }
    if (remainingContinues_ == 0) {
        throw std::logic_error("continue requested without a remaining retry");
    }
    level1_.reloadAfterFailure(failureRng_);
    --remainingContinues_;
    phase_ = GameplayPhase::level1RetryHandoff;
    level1FailureTailFinished_ = false;
    failureSourceLoopTick_.reset();
    presentationFrame_ = 0;
    travel_ = 0;
    weaponIndicatorVisible_ = false;
    weaponReadyIndicator_ = false;
    damageFlashAge_ = 0;
    damageFlashActive_ = false;
    explosionTicksRemaining_ = 0;
    explosionAge_ = 0;
    failureAudioEvents_.clear();
    successAudioOwned_ = false;
    successAudioActive_ = false;
    //50c4 draws twice before the next ordinary frame. Retain the oscillator.
    constexpr std::size_t cacheDraws = 2;
    lastCockpitInitialization_.levelIndex = 0;
    lastCockpitInitialization_.displaySteps = 2;
    lastCockpitInitialization_.states[0] = {cockpitIndicatorX_, cockpitIndicatorVelocity_};
    for (std::size_t index = 1; index <= cacheDraws; ++index) {
        advanceCockpitIndicator();
        lastCockpitInitialization_.states[index] = {cockpitIndicatorX_, cockpitIndicatorVelocity_};
    }
    //The next ordinary draw supplies the final record, not this loader boundary.
    lastCockpitInitialization_.states.back() = lastCockpitInitialization_.states[cacheDraws];
}

void GameplayModel::retryLevel2(const GameplayInput& input) {
    if(nativeInputRequested(input)) {
        throw std::invalid_argument("source/default continuation rejects queued system actions");
    }
    if (activeCampaignLevel_ != 1 || level2Entities_.empty()) {
        throw std::runtime_error("continue reload requires supported Level2");
    }
    if (input.left || input.right || input.forward || input.reverse || input.fire
        || level1_.selectedSystem() != -1
        || std::any_of(level1_.systemStatusLevels().begin(),
            level1_.systemStatusLevels().end(), [](const auto status) { return status <= 0; })) {
        throw std::runtime_error("Level2 retry requires neutral input and positive unselected systems");
    }
    if (remainingContinues_ == 0) {
        throw std::logic_error("continue requested without a remaining retry");
    }
    level1_.resetCampaignTransientState();
    initializeLevel2({}, Level2EntryOrigin::retry);
    --remainingContinues_;
    presentationFrame_ = 0;
    travel_ = 0;
    weaponIndicatorVisible_ = false;
    weaponReadyIndicator_ = false;
    damageFlashAge_ = 0;
    damageFlashActive_ = false;
    explosionTicksRemaining_ = 0;
    explosionAge_ = 0;
    failureAudioEvents_.clear();
    successAudioOwned_ = false;
    successAudioActive_ = false;
    lastCockpitInitialization_.levelIndex = 1;
    lastCockpitInitialization_.states[0] = {cockpitIndicatorX_, cockpitIndicatorVelocity_};
    constexpr std::size_t cacheDraws = 2;
    for (std::size_t index = 1; index <= cacheDraws; ++index) {
        advanceCockpitIndicator();
        lastCockpitInitialization_.states[index] = {cockpitIndicatorX_, cockpitIndicatorVelocity_};
    }
    //4c16 follows the two cached HUD draws; the first display has no actor walk.
    failureSourceLoopTick_.reset(); //50ac resets12c8 and releases its failure owner.
    level2Ground_.advance(0, 0, level2Rng_);
    refreshLevel2Visibility();
    presentLevel2({}, nullptr, true);
    advanceCockpitIndicator();
    lastCockpitInitialization_.states.back() = {cockpitIndicatorX_, cockpitIndicatorVelocity_};
    runLevel2PeriodicSpawnCheck();
    phase_ = GameplayPhase::level2Handoff;
}

void GameplayModel::tick(const GameplayInput& input) {
    tickImpl(input, nullptr);
}

void GameplayModel::tickWithRenderer(const GameplayInput& input, Level2FrameRenderer& renderer) {
    if (phase_ != GameplayPhase::level2
        && !(phase_ == GameplayPhase::level2PreloopReturn
            && renderer.prefixExecution() == Level2PrefixExecution::source)) {
        throw std::invalid_argument("clocked frame renderer requires active Level2 or its genuine source preloop");
    }
    tickImpl(input, &renderer);
}

void GameplayModel::tickImpl(const GameplayInput& input, Level2FrameRenderer* renderer,
    Level1FrameRenderer* level1Renderer,const GameplayInputDomain domain) {
    if(domain==GameplayInputDomain::source && nativeInputRequested(input)) {
        throw std::invalid_argument("source/default frames do not admit queued system actions");
    }
    if (phase_ == GameplayPhase::level2PreloopReturn) {
        advanceLevel2(input, renderer);
        ++ticks_;
        return;
    }
    if (phase_ == GameplayPhase::level3Native) {
        throw std::invalid_argument("Level3 native frames require their retained display and tail owner");
    }
    if (phase_ == GameplayPhase::level3PreloopReturn) {
        throw std::invalid_argument("Level3 first frame requires its retained actor and presenter execution");
    }
    if (phase_ == GameplayPhase::level2SuccessReturn || phase_ == GameplayPhase::level3LoaderReturn) {
        throw std::invalid_argument("Level2 success return requires its owned campaign cleanup and loader");
    }
    if (phase_ == GameplayPhase::endingEntry || phase_ == GameplayPhase::endingPresentation
        || phase_ == GameplayPhase::endingTeardown || phase_ == GameplayPhase::endingReturn
        || phase_ == GameplayPhase::nativeMenu || phase_ == GameplayPhase::nativeBeginWait) {
        throw std::invalid_argument("SCENED entry requires its owned page transaction");
    }
    if (phase_ == GameplayPhase::level5SuccessReturn) {
        throw std::invalid_argument("Level5 resumed success requires campaign cleanup and SCENED presentation");
    }
    if (phase_ == GameplayPhase::level2SuccessPresentation && activeCampaignLevel_ == 4
        && (successAudioOwned_ ? !successAudioActive_
            : presentationFrame_ + 1 >= level2SuccessPresentationFrames)) {
        validateLevel5SuccessReturn(input);
    }
    if (phase_ == GameplayPhase::level2SuccessPresentation && activeCampaignLevel_ == 1
        && successAudioOwned_ && level2SuccessContinuation_
        && (successAudioOwned_ ? !successAudioActive_
            : presentationFrame_ + 1 >= level2SuccessPresentationFrames)) {
        validateLevel2SuccessReturn(input);
    }
    if ((phase_ == GameplayPhase::level5LoaderReturn || phase_ == GameplayPhase::level5)
        && (renderer != nullptr || (phase_ == GameplayPhase::level5LoaderReturn && input.fire))) {
        throw std::invalid_argument("Level5 native frames exclude strict work and loader-return firing");
    }
    if (phase_ == GameplayPhase::level5LoaderReturn) {
        if (activeCampaignLevel_ != 4 || level2HasAdvanced_ || !level2ActiveListOrder_.empty()
            || level1_.selectedSystem() != -1
            || std::any_of(level1_.systemStatusLevels().begin(), level1_.systemStatusLevels().end(),
                [](const auto status) { return status < 0; })
            || std::any_of(level1_.systemTimers().begin(), level1_.systemTimers().end(),
                [](const auto timer) { return timer != 0; })) {
            throw std::invalid_argument("Level5 first frame requires retained reset lists and untimed unselected systems");
        }
        level2Ground_ = Level1GroundState(level2Rng_);
        level2Visibility_.resetLevel();
        phase_ = GameplayPhase::level5;
    }
    if (phase_ == GameplayPhase::campaignEnded) {
        return;
    }
    const bool firstLoaderFrame = phase_ == GameplayPhase::level1RetryHandoff
        || phase_ == GameplayPhase::level1BeginHandoff;
    if (firstLoaderFrame) {
        phase_ = GameplayPhase::level1;
    }
    ++ticks_;
    if (phase_ != GameplayPhase::level1) {
        switch (phase_) {
        case GameplayPhase::successPresentation:
            if (successAudioOwned_ ? successAudioActive_
                : presentationFrame_ + 1 < successPresentationFrames) {
                ++presentationFrame_;
            } else {
                if (level1_.pendingSuccessContact()) {
                    validateLevel1SuccessReturn(input);
                    level1_.finishSuccessContact({input.left, input.right, input.forward, input.reverse,
                        input.fire, input.exit});
                }
                initializeLevel2(input, Level2EntryOrigin::campaign);
                prepareCockpitInitialDisplay();
                // Source03d1 follows tick-zero0ddc; its selection cannot spawn at active5.
                runLevel2PeriodicSpawnCheck();
                successAudioOwned_ = false;
                successAudioActive_ = false;
                phase_ = GameplayPhase::level2Handoff;
            }
            break;
        case GameplayPhase::level2Handoff:
            advanceLevel2(input);
            phase_ = GameplayPhase::level2;
            break;
        case GameplayPhase::level5:
        case GameplayPhase::level2:
        {
            if (explosionTicksRemaining_ != 0) {
                --explosionTicksRemaining_;
                ++explosionAge_;
                if (explosionTicksRemaining_ == 0) {
                    explosionAge_ = 0;
                }
            }
            const auto armorBefore = level2PlayerArmor_;
            const auto cooldownBefore = level2Player_.weaponCooldown;
            const auto projectileTimersBefore =
                level2Player_.weaponSlotTimers;
            advanceLevel2(input, renderer);
            if (level2PlayerArmor_ < armorBefore) {
                damageFlashAge_ = 0;
                damageFlashActive_ = true;
            } else if (damageFlashActive_) {
                if (damageFlashAge_ < 8) {
                    ++damageFlashAge_;
                    level2EntryPaletteActive_ = false;
                } else {
                    damageFlashActive_ = false;
                }
            }
            if (
                cooldownBefore == 0
                && level2Player_.weaponCooldown != 0
            ) {
                weaponIndicatorVisible_ = true;
                weaponReadyIndicator_ = false;
            }
            auto projectileBecameReady = false;
            for (std::size_t slot = 0;
                 slot < playerCannonSlotCount_; ++slot) {
                projectileBecameReady = projectileBecameReady
                    || (
                        projectileTimersBefore[slot] != 0
                        && level2Player_.weaponSlotTimers[slot] == 0
                    );
            }
            if (
                weaponIndicatorVisible_
                && weaponReady()
                && (
                    (
                        cooldownBefore != 0
                        && level2Player_.weaponCooldown == 0
                    )
                    || projectileBecameReady
                )
            ) {
                weaponReadyIndicator_ = true;
            }
            if (level2PlayerArmor_ < 0) {
                failureFromLevel2_ = true;
                phase_ = GameplayPhase::failurePreExplosionBlackout;
                presentationFrame_ = 0;
            } else if (level2Complete_) {
                phase_ = GameplayPhase::level2SuccessPresentation;
                presentationFrame_ = 0;
            }
            break;
        }
        case GameplayPhase::level2SuccessPresentation:
            if (successAudioOwned_ ? successAudioActive_
                : presentationFrame_ + 1 < level2SuccessPresentationFrames) {
                ++presentationFrame_;
            } else {
                if (activeCampaignLevel_ == 4) {
                    resumeLevel5Success(input);
                    break;
                }
                if (activeCampaignLevel_ == 1 && successAudioOwned_ && level2SuccessContinuation_) {
                    resumeLevel2Success(input);
                    break;
                }
                initializeLevel3FromCampaign();
                prepareCockpitInitialDisplay();
                phase_ = GameplayPhase::level3Handoff;
                presentationFrame_ = 0;
            }
            break;
        case GameplayPhase::level3Handoff:
            // Level 3 capture evidence applies input on the current main-loop
            // boundary. Override the Level 2 deferred-input compatibility
            // convention while retaining the shared integrator.
            level2PendingInput_ = input;
            advanceLevel2(input);
            phase_ = GameplayPhase::level3;
            break;
        case GameplayPhase::level3:
            level2PendingInput_ = input;
            advanceLevel2(input);
            if (level2PlayerArmor_ < 0) {
                failureFromLevel2_ = true;
                phase_ = GameplayPhase::failurePreExplosionBlackout;
                presentationFrame_ = 0;
            } else if (level2Complete_) {
                phase_ = GameplayPhase::level3SuccessPresentation;
                presentationFrame_ = 0;
            }
            break;
        case GameplayPhase::level3SuccessPresentation:
            if (
                presentationFrame_ + 1
                < level3SuccessPresentationFrames
            ) {
                ++presentationFrame_;
            } else {
                // The native presenter returns to the campaign loop after
                // its terminal frame.  The first post-return frame is kept
                // as a separate verified handoff boundary; Level 4's full
                // scheduler remains intentionally outside this model.
                phase_ = GameplayPhase::level4Handoff;
                presentationFrame_ = 0;
            }
            break;
        case GameplayPhase::level4Handoff:
            // No speculative Level 4 tick is performed until its native
            // scheduler/audio contract is captured.
            break;
        case GameplayPhase::failurePreExplosionBlackout:
            initializeFailurePresentation();
            phase_ = GameplayPhase::failureExplosion;
            presentationFrame_ = 0;
            break;
        case GameplayPhase::failureExplosion:
            if (presentationFrame_ + 1 < failureExplosionFrames) {
                advanceFailurePresentation(presentationFrame_ + 1);
                ++presentationFrame_;
            } else {
                finishFailurePresentation();
                phase_ = GameplayPhase::failurePostExplosionBlackout;
                presentationFrame_ = 0;
            }
            break;
        case GameplayPhase::failurePostExplosionBlackout:
            initializeContinuePrompt();
            break;
        case GameplayPhase::continuePrompt:
            advanceContinuePrompt(input);
            break;
        case GameplayPhase::level1RetryHandoff:
        case GameplayPhase::level1BeginHandoff:
        case GameplayPhase::level2SuccessReturn:
        case GameplayPhase::campaignEnded:
        case GameplayPhase::highscoreNameEntry:
        case GameplayPhase::highscoreHall:
        case GameplayPhase::level1:
            break;
        }
        return;
    }

    travel_ += level1_.player().forwardVelocity;
    const bool missionWasActive = level1_.missionActive();
    std::array<bool, 32> hostileWasActive{};
    std::array<FixedPosition, 32> hostilePositions{};
    const auto trackedHostiles = std::min<std::size_t>(
        level1_.entities().size(),
        hostileWasActive.size()
    );
    for (std::size_t index = 0; index < trackedHostiles; ++index) {
        hostileWasActive[index] = isHostile(level1_.entities()[index]);
        hostilePositions[index] = level1_.entities()[index].position;
    }
    if (explosionTicksRemaining_ != 0) {
        --explosionTicksRemaining_;
        ++explosionAge_;
        if (explosionTicksRemaining_ == 0) {
            explosionAge_ = 0;
        }
    }
    const auto cooldownBefore = level1_.playerWeaponCooldown();
    const auto projectileTimersBefore =
        level1_.playerProjectileTimers();
    const auto armorBefore = level1_.playerArmor();
    const Level1Input level1Input{
        input.left,
        input.right,
        input.forward,
        input.reverse,
        input.fire,
        input.exit,
    };
    if (level1Renderer) {
        level1_.advanceTickWithRenderer(level1Input,*level1Renderer);
    } else {
        level1_.advanceTick(level1Input);
    }
    const auto armorAfter = level1_.playerArmor();
    if (armorAfter < armorBefore) {
        damageFlashAge_ = 0;
        damageFlashActive_ = true;
    } else if (damageFlashActive_) {
        if (damageFlashAge_ < 8) {
            ++damageFlashAge_;
        } else {
            damageFlashActive_ = false;
        }
    }
    advanceCockpitIndicator();
    if (firstLoaderFrame) {
        lastCockpitInitialization_.displaySteps = 3;
        lastCockpitInitialization_.states.back() = {
            cockpitIndicatorX_, cockpitIndicatorVelocity_};
    }
    if (
        cooldownBefore == 0
        && level1_.playerWeaponCooldown() != 0
    ) {
        weaponIndicatorVisible_ = true;
        weaponReadyIndicator_ = false;
    }
    if ((level1_.loopTick() & 7U) == 0U) {
        alternateSightPhase_ = false;
        const auto playerYaw = level1_.player().yaw;
        for (const auto& entity : level1_.entities()) {
            if (!entity.active() || entity.type >= 14) {
                continue;
            }
            const auto difference = static_cast<std::int16_t>(
                static_cast<std::uint16_t>(
                    entity.targetHeading
                    - playerYaw
                    + 0x8000U
                )
            );
            if (std::abs(static_cast<int>(difference)) < 0x400) {
                alternateSightPhase_ = true;
                break;
            }
        }
    }
    for (std::size_t index = 0; index < trackedHostiles; ++index) {
        if (
            hostileWasActive[index]
            && !level1_.entities()[index].active()
        ) {
            explosionTicksRemaining_ = 18;
            explosionAge_ = 0;
            explosionPosition_ = hostilePositions[index];
            break;
        }
    }
    const auto& projectileTimersAfter =
        level1_.playerProjectileTimers();
    bool projectileBecameReady = false;
    for (
        std::size_t slot = 0;
        slot < projectileTimersBefore.size();
        ++slot
    ) {
        projectileBecameReady = (
            projectileBecameReady
            || (
                projectileTimersBefore[slot] != 0
                && projectileTimersAfter[slot] == 0
            )
        );
    }
    if (
        weaponIndicatorVisible_
        && weaponReady()
        && (
            (cooldownBefore != 0 && level1_.playerWeaponCooldown() == 0)
            || projectileBecameReady
        )
    ) {
        weaponReadyIndicator_ = true;
    }
    // The controlled frame continues from the post-model state checkpoint
    // through pending input. A held Fire that can allocate a slot therefore
    // updates the console before the next semantic-state boundary.
    if (input.fire && weaponReady()) {
        weaponIndicatorVisible_ = true;
        weaponReadyIndicator_ = false;
    }
    if (
        missionWasActive
        && !level1_.missionActive()
        && level1_.failureState()
    ) {
        failureFromLevel2_ = false;
        phase_ = GameplayPhase::failurePreExplosionBlackout;
        presentationFrame_ = 0;
    } else if (
        missionWasActive
        && !level1_.missionActive()
        && level1_.successState()
    ) {
        phase_ = GameplayPhase::successPresentation;
        presentationFrame_ = 0;
    }
}

std::int32_t GameplayModel::heading() const noexcept {
    return failureFromLevel2_
            || phase_ == GameplayPhase::level2Handoff
            || phase_ == GameplayPhase::level2
            || isCampaignReturnPhase(phase_)
            || phase_ == GameplayPhase::level2SuccessPresentation
            || phase_ == GameplayPhase::level3SuccessPresentation
            || isLevel3Phase(phase_) || isPlayableCampaignPhase(phase_)
        ? level2Player_.yaw
        : level1_.player().yaw;
}

std::int32_t GameplayModel::travel() const noexcept {
    return travel_;
}

std::int16_t GameplayModel::turnVelocity() const noexcept {
    return failureFromLevel2_
            || phase_ == GameplayPhase::level2Handoff
            || phase_ == GameplayPhase::level2
            || isCampaignReturnPhase(phase_)
            || phase_ == GameplayPhase::level2SuccessPresentation
            || phase_ == GameplayPhase::level3SuccessPresentation
            || isLevel3Phase(phase_) || isPlayableCampaignPhase(phase_)
        ? level2Player_.yawVelocity
        : level1_.player().yawVelocity;
}

std::int16_t GameplayModel::forwardVelocity() const noexcept {
    return failureFromLevel2_
            || phase_ == GameplayPhase::level2Handoff
            || phase_ == GameplayPhase::level2
            || isCampaignReturnPhase(phase_)
            || phase_ == GameplayPhase::level2SuccessPresentation
            || phase_ == GameplayPhase::level3SuccessPresentation
            || isLevel3Phase(phase_) || isPlayableCampaignPhase(phase_)
        ? level2Player_.forwardVelocity
        : level1_.player().forwardVelocity;
}

int GameplayModel::panoramaOffset() const noexcept {
    return wrap(
        -arithmeticShiftRightSix(signedAngle(heading())),
        1024
    );
}

std::uint64_t GameplayModel::ticks() const noexcept {
    return ticks_;
}

bool GameplayModel::shotActive() const noexcept {
    return shotCount() != 0;
}

bool GameplayModel::shotActive(const std::size_t slot) const noexcept {
    if (isCampaignGameplayPhase(phase_)) {
        return slot < playerCannonSlotCount_ && level2Player_.weaponSlotTimers[slot] != 0;
    }
    if (isCampaignReturnPhase(phase_)) {
        return false; // This continuation admits no live projectile records.
    }
    return (
        slot < maxShots
        && level1_.playerProjectileTimers()[slot] != 0
    );
}

unsigned GameplayModel::shotAge() const noexcept {
    for (std::size_t slot = 0; slot < maxShots; ++slot) {
        if (shotActive(slot)) {
            return shotAge(slot);
        }
    }
    return 0;
}

unsigned GameplayModel::shotAge(const std::size_t slot) const noexcept {
    if (isCampaignGameplayPhase(phase_)) {
        return shotActive(slot) ? 32U - level2Player_.weaponSlotTimers[slot] : 0;
    }
    return shotActive(slot)
        ? 32U - level1_.playerProjectileTimers()[slot]
        : 0;
}

unsigned GameplayModel::shotCount() const noexcept {
    if (isCampaignGameplayPhase(phase_)) {
        return static_cast<unsigned>(std::count_if(level2Player_.weaponSlotTimers.begin(),
            level2Player_.weaponSlotTimers.begin() + playerCannonSlotCount_,
            [](const auto remaining) { return remaining != 0; }));
    }
    if (isCampaignReturnPhase(phase_)) {
        return 0;
    }
    return static_cast<unsigned>(std::count_if(
        level1_.playerProjectileTimers().begin(),
        level1_.playerProjectileTimers().end(),
        [](const std::uint8_t remaining) {
            return remaining != 0;
        }
    ));
}

unsigned GameplayModel::weaponCooldown() const noexcept {
    if (isCampaignGameplayPhase(phase_) || isCampaignReturnPhase(phase_)
            || phase_ == GameplayPhase::level2SuccessPresentation) {
        return level2Player_.weaponCooldown;
    }
    return level1_.playerWeaponCooldown();
}

bool GameplayModel::weaponReady() const noexcept {
    if (isCampaignGameplayPhase(phase_) || isCampaignReturnPhase(phase_)
            || phase_ == GameplayPhase::level2SuccessPresentation) {
        return level2Player_.weaponCooldown == 0
            && (level2Player_.weaponSlotTimers[0] == 0
                || level2Player_.weaponSlotTimers[1] == 0);
    }
    return level1_.playerWeaponCooldown() == 0
        && std::any_of(
            level1_.playerProjectileTimers().begin(),
            level1_.playerProjectileTimers().end(),
            [](const std::uint8_t remaining) {
                return remaining == 0;
            }
        );
}

bool GameplayModel::weaponIndicatorVisible() const noexcept {
    return weaponIndicatorVisible_;
}

bool GameplayModel::weaponReadyIndicator() const noexcept {
    if (isLevel2Phase(phase_)) {
        // The portable semantic state is at 02cc. The displayed lower page
        // also receives 0f72's fire callback and the 0372..03cf timer updates
        // before the exact 03d1 screen boundary. Predict only this presenter
        // bit; do not advance the simulation or allocate a projectile.
        const auto cooldown = level2Player_.weaponCooldown;
        const auto first = level2Player_.weaponSlotTimers[0];
        const auto second = level2Player_.weaponSlotTimers[1];
        if (level2PendingInput_.fire && cooldown == 0
            && (first == 0 || second == 0)
            && std::find(level2ProjectilePoolActive_.begin(),
                         level2ProjectilePoolActive_.end(), false)
                != level2ProjectilePoolActive_.end()) {
            return false;
        }
        if ((cooldown == 1 && (first == 0 || second == 0))
            || (cooldown <= 1 && (first == 1 || second == 1))) {
            return true;
        }
    }
    return weaponReadyIndicator_;
}

bool GameplayModel::alternateSightPhase() const noexcept {
    return alternateSightPhase_;
}

const CockpitInitializationRecord& GameplayModel::lastCockpitInitialization() const noexcept {
    return lastCockpitInitialization_;
}

int GameplayModel::cockpitIndicatorX() const noexcept {
    return cockpitIndicatorX_;
}

bool GameplayModel::enemyActive() const noexcept {
    if (
        phase_ == GameplayPhase::level2Handoff
        || phase_ == GameplayPhase::level2
        || isCampaignReturnPhase(phase_)
            || phase_ == GameplayPhase::level2SuccessPresentation
        || phase_ == GameplayPhase::level3SuccessPresentation
        || isLevel3Phase(phase_) || isPlayableCampaignPhase(phase_)
    ) {
        return std::any_of(
            level2Entities_.begin(),
            level2Entities_.end(),
            [](const Level1Entity& entity) {
                return isHostile(entity);
            }
        );
    }
    return primaryHostile(*this) != nullptr;
}

bool GameplayModel::explosionActive() const noexcept {
    return explosionTicksRemaining_ != 0;
}

std::int32_t GameplayModel::enemyBearing() const noexcept {
    if (
        phase_ == GameplayPhase::level2Handoff
        || phase_ == GameplayPhase::level2
        || isCampaignReturnPhase(phase_)
            || phase_ == GameplayPhase::level2SuccessPresentation
        || phase_ == GameplayPhase::level3SuccessPresentation
        || isLevel3Phase(phase_) || isPlayableCampaignPhase(phase_)
    ) {
        const Level1Entity* selected = nullptr;
        for (const auto& entity : level2Entities_) {
            if (
                isHostile(entity)
                && (
                    selected == nullptr
                    || entity.presenterDistance < selected->presenterDistance
                )
            ) {
                selected = &entity;
            }
        }
        return selected == nullptr ? heading() : selected->targetHeading;
    }
    const auto* hostile = primaryHostile(*this);
    if (hostile == nullptr) {
        return heading();
    }
    return fixedHeadingBetween(
        level1_.player().position,
        hostile->position
    );
}

int GameplayModel::enemyRange() const noexcept {
    if (
        isCampaignGameplayPhase(phase_) || isCampaignReturnPhase(phase_)
    ) {
        const Level1Entity* selected = nullptr;
        for (const auto& entity : level2Entities_) {
            if (
                isHostile(entity)
                && (
                    selected == nullptr
                    || entity.presenterDistance < selected->presenterDistance
                )
            ) {
                selected = &entity;
            }
        }
        return selected == nullptr
            ? 0
            : static_cast<int>(selected->presenterDistance / 32U);
    }
    const auto* hostile = primaryHostile(*this);
    if (hostile == nullptr) {
        return 0;
    }
    return static_cast<int>(
        approximatePlanarDistance(
            level1_.player().position,
            hostile->position
        ) / 32U
    );
}

unsigned GameplayModel::explosionAge() const noexcept {
    return explosionAge_;
}

FixedPosition GameplayModel::explosionPosition() const noexcept {
    return explosionPosition_;
}

unsigned GameplayModel::score() const noexcept {
    return (
        failureFromLevel2_
        || (playableCampaignFrames_ && (phase_ == GameplayPhase::highscoreNameEntry
            || phase_ == GameplayPhase::highscoreHall))
        || phase_ == GameplayPhase::level5LoaderReturn
        || phase_ == GameplayPhase::level5
        || phase_ == GameplayPhase::level2Handoff
        || phase_ == GameplayPhase::level2
        || isCampaignReturnPhase(phase_)
            || phase_ == GameplayPhase::level2SuccessPresentation
        || phase_ == GameplayPhase::level3SuccessPresentation
        || isLevel3Phase(phase_) || isPlayableCampaignPhase(phase_)
    ) ? level2Score_ : level1_.score();
}

std::int16_t GameplayModel::playerArmor() const noexcept {
    return failureFromLevel2_
            || phase_ == GameplayPhase::level5LoaderReturn
            || phase_ == GameplayPhase::level5
            || phase_ == GameplayPhase::level2Handoff
            || phase_ == GameplayPhase::level2
            || isCampaignReturnPhase(phase_)
            || phase_ == GameplayPhase::level2SuccessPresentation
            || phase_ == GameplayPhase::level3SuccessPresentation
            || isLevel3Phase(phase_) || isPlayableCampaignPhase(phase_)
        ? level2PlayerArmor_
        : level1_.playerArmor();
}

unsigned GameplayModel::damageFlashAge() const noexcept {
    return damageFlashAge_;
}

bool GameplayModel::damageFlashActive() const noexcept {
    return damageFlashActive_;
}

bool GameplayModel::level2EntryPaletteActive() const noexcept {
    return level2EntryPaletteActive_;
}

GameplayPhase GameplayModel::phase() const noexcept {
    return phase_;
}

ContinueChoice GameplayModel::continueChoice() const noexcept {
    return continueChoice_;
}

std::optional<ContinueChoice> GameplayModel::pressedContinueChoice() const noexcept {
    return pressedContinueChoice_;
}

unsigned GameplayModel::remainingContinues() const noexcept {
    return remainingContinues_;
}

unsigned GameplayModel::presentationFrame() const noexcept {
    return presentationFrame_;
}

bool GameplayModel::level3CompletionByPickup() const noexcept {
    return level3CompletionByPickup_;
}

const ViewTransform& GameplayModel::failureView() const noexcept {
    return failureView_;
}

const Level1Entity& GameplayModel::failurePlayer() const noexcept {
    return failurePlayer_;
}

bool GameplayModel::failurePlayerVisible() const noexcept {
    return (
        phase_ == GameplayPhase::failureExplosion
        && presentationFrame_ <= 2
    );
}

bool GameplayModel::failureFromLevel2() const noexcept {
    return failureFromLevel2_;
}

const std::vector<Level1Entity>&
GameplayModel::failureMissionEntities() const noexcept {
    return failureMissionEntities_;
}

const std::vector<Level1Entity>&
GameplayModel::failureDebris() const noexcept {
    return failureDebris_;
}

const std::vector<FailureBurst>&
GameplayModel::failureBursts() const noexcept {
    return failureBursts_;
}

const std::vector<FailureVisibleItem>&
GameplayModel::failureVisibleItems() const noexcept {
    return failureVisibleItems_;
}

const StellarRng& GameplayModel::failureRng() const noexcept {
    return failureRng_;
}

const std::vector<Level1AudioEvent>&
GameplayModel::failureAudioEvents() const noexcept {
    return failureAudioEvents_;
}

const Level1State& GameplayModel::level1State() const noexcept {
    return level1_;
}

const std::vector<Level1AudioEvent>&
GameplayModel::level2AudioEvents() const noexcept {
    return level2AudioEvents_;
}

std::uint16_t GameplayModel::level2LoopTick() const noexcept {
    return level2LoopTick_;
}

const Level1Entity& GameplayModel::level2Player() const noexcept {
    return level2Player_;
}

const std::vector<Level1Entity>& GameplayModel::level2Entities() const noexcept {
    return level2Entities_;
}

const std::vector<std::size_t>&
GameplayModel::level2ActiveListOrder() const noexcept {
    return level2ActiveListOrder_;
}

const Level1GroundState& GameplayModel::level2Ground() const noexcept {
    return level2Ground_;
}

const StellarRng& GameplayModel::level2Rng() const noexcept {
    return level2Rng_;
}

int GameplayModel::level2CockpitIndicatorX() const noexcept {
    return cockpitIndicatorX_;
}

int GameplayModel::level2CockpitIndicatorVelocity() const noexcept {
    return cockpitIndicatorVelocity_;
}

}  // namespace stellar7
