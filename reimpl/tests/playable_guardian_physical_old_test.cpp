#include "native_level2_frame_fixture.hpp"
#include "playable_campaign_comparison.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace stellar7;
constexpr std::uint32_t incomingRngCalls = 25667;
constexpr std::uint32_t visualMultiplier = 0x343fd;
constexpr std::uint32_t visualIncrement = 0x269ec3;
constexpr std::size_t noIndex = std::numeric_limits<std::size_t>::max();
constexpr std::uint32_t zeroTimerVisualSeed = 145;
enum class Boundary { alias, lateDestination, lateZeroTimer, pulse, earlierDestination, pulseFull, pulseUnknown };
bool isPulse(const Boundary boundary) {
    return boundary == Boundary::pulse || boundary == Boundary::pulseFull || boundary == Boundary::pulseUnknown;
}
void require(const bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
struct Expected {
    StellarRng rng;
    Level1Entity provisional{};
    Level1Entity retiredFixed{};
    std::uint32_t visual;
    std::uint64_t incarnation;
    std::size_t rewardSlot;
    std::size_t pulseSlot;
    std::uint16_t heading = 0;
};
}

namespace stellar7 {
// Declared synthetic initial boundaries on actual retained resource owners.
// These cases do not assert preceding natural combat or original-state equality.
struct NativeSessionTestAccess {
    static void initializeSyntheticFixedCaches(NativeGameplaySession& session) {
        constexpr std::size_t fixedPoolBase = 16;
        constexpr std::size_t resetFixedCount = 8;
        constexpr std::size_t fixedPoolSize = 16;
        // The full-known control declares matching history in both physical owners.
        for (std::size_t slot = resetFixedCount; slot < fixedPoolSize; ++slot) {
            session.radar_->initializeAllocation(fixedPoolBase + slot);
        }
    }
};
struct GameplayModelTestAccess {
    static Expected prepare(GameplayModel& model, const Boundary boundary) {
        const auto guardian = std::find_if(model.level2Entities_.begin(), model.level2Entities_.end(), [](const auto& actor) {
            return actor.type == 44 && actor.stateBytes4dTo52[1] == 1;
        });
        require(guardian != model.level2Entities_.end(), "fixture lost its HP1 guardian");
        const auto index = static_cast<std::size_t>(guardian - model.level2Entities_.begin());
        require(model.level2EntityActorPoolSlots_.at(index) == 0
            && guardian->collisionRadius == 1147
            && guardian->renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation,
            "fixture lost actual Level2 FF0 guardian metadata");
        require(model.level2CompletionObjectIndex_ == noIndex
            && model.playableOrdinaryConstructor_.has_value(), "fixture lost completion/constructor entry invariants");
        guardian->stateBytes4dTo52[5] = isPulse(boundary) ? 1 : 0;
        guardian->behaviorTimer = isPulse(boundary) ? 21 : 0;
        guardian->forwardVelocity = 0;
        guardian->collisionContactAllocationIndex = noIndex;
        guardian->collisionContactIsPlayer = false;
        model.level2ActorPool_[0] = *guardian;
        StellarRng rng;
        for (std::uint32_t call = 0; call < incomingRngCalls; ++call) { rng.next(); }
        model.level2Rng_ = rng;
        const auto late = boundary == Boundary::lateDestination || boundary == Boundary::lateZeroTimer
            || boundary == Boundary::earlierDestination;
        if (late) {
            // The live FF0 Type48 is retired by actual player contact before9298.
            // FF1 remains distant, so its null contact does not block AE10.
            model.level2ActorPool_[1] = *guardian;
            model.level2ActorPoolActive_[1] = true;
            model.level2ActorIncarnations_[1] = model.level2ActorIncarnations_[0];
            model.level2EntityActorPoolSlots_[index] = 1;
            model.level2ActorPoolActive_[0] = false;
            auto initialRng = rng;
            auto airborne = model.playableOrdinaryConstructor_->construct(13, initialRng);
            airborne.type = 48;
            airborne.position = model.level2Player_.position;
            airborne.pitch = airborne.roll = airborne.yaw = 0;
            airborne.forwardVelocity = airborne.yawVelocity = 0;
            airborne.behaviorTimer = 240;
            airborne.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
            airborne.animatedModelPresenterEnabled = false;
            const auto airborneIndex = model.allocateLevel2Actor(airborne, GameplayModel::ActorInitialization::validatedOrdinary);
            require(airborneIndex < model.level2Entities_.size()
                && model.level2EntityActorPoolSlots_[airborneIndex] == 0,
                "late OLD fixture failed to allocate its live FF0 contact target");
            const auto source = std::find(model.level2EntityActorPoolSlots_.begin(), model.level2EntityActorPoolSlots_.end(), 1);
            require(source != model.level2EntityActorPoolSlots_.end(), "late OLD fixture lost FF1 guardian");
            const auto sourceIndex = static_cast<std::size_t>(source - model.level2EntityActorPoolSlots_.begin());
            model.level2ActiveListOrder_ = model.level2AllocationListOrder_;
            const auto a = std::find(model.level2ActiveListOrder_.begin(), model.level2ActiveListOrder_.end(), airborneIndex);
            const auto b = std::find(model.level2ActiveListOrder_.begin(), model.level2ActiveListOrder_.end(), sourceIndex);
            require(a != model.level2ActiveListOrder_.end() && b != model.level2ActiveListOrder_.end(),
                "late OLD fixture lost its two live allocation references");
            if ((boundary == Boundary::earlierDestination && a > b)
                || (boundary != Boundary::earlierDestination && a < b)) { std::iter_swap(a, b); }
            if (boundary == Boundary::lateZeroTimer) { model.level2VisualRandomState_ = zeroTimerVisualSeed; }
            ++model.level2ActiveEnemyCount_;
        }
        std::vector<std::size_t> freeFixed;
        for (std::size_t slot = 0; slot < model.level2ResetFixedObjectCount_; ++slot) {
            require(model.level2FixedPoolKnown_[slot], "fixture lost reset-owned FE provenance");
            if (!model.level2FixedPool_[slot].active()) { freeFixed.push_back(slot); }
        }
        require(freeFixed.size() >= 2, "fixture lost two reset-owned free FE slots");
        Expected expected{rng, {}, {}, model.level2VisualRandomState_, model.level2ActorIncarnations_[0],
            freeFixed[0], freeFixed[1]};
        require(expected.rng.next() == 10703 && expected.rng.next() == 19712,
            "declared pickup-spin and A92E words changed");
        if (!isPulse(boundary)) {
            expected.provisional = model.playableOrdinaryConstructor_->construct(13, expected.rng);
            require(expected.provisional.stateBytes4dTo52[1] == 1
                && expected.provisional.stateBytes4dTo52[4] == 4
                && expected.provisional.collisionRadius == 559,
                "resource constructor lost provisional Type13 reset metadata");
            expected.visual = expected.visual * visualMultiplier + visualIncrement;
            if (boundary == Boundary::lateZeroTimer) {
                require(static_cast<std::uint8_t>(expected.visual >> 16U) == 0,
                    "declared E078 input no longer selects timer0");
                expected.heading = expected.rng.next();
            }
        } else {
            auto& retained = model.level2FixedPool_[expected.pulseSlot];
            retained.type = 46;
            retained.stateBytes4dTo52 = {0xfe, 0, 91, 92, 0, 93};
            retained.behaviorTimer = 94;
            retained.pitchVelocity = 95;
            retained.rollVelocity = 96;
            retained.yawVelocity = 97;
            retained.xVelocity = 98;
            retained.yVelocity = 99;
            retained.zVelocity = 100;
            retained.forwardVelocity = 101;
            retained.modelAnimationVariant = 3;
            retained.presentationModelAnimationVariant = 4;
            retained.animatedModelPresenterEnabled = false;
            retained.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
            model.level2FixedPoolEverUsed_[expected.pulseSlot] = true;
            expected.retiredFixed = retained;
        }
        if (boundary == Boundary::pulseUnknown) {
            model.level2FixedPoolKnown_[expected.pulseSlot] = false;
        } else if (boundary == Boundary::pulseFull) {
            // Declared synthetic complete FE history; use real allocation membership.
            // Reward consumes the sole remaining free FE before the pulse scans.
            for (std::size_t slot = 0; slot < model.level2FixedObjectPoolSize_; ++slot) {
                if (!model.level2FixedPoolKnown_[slot]) {
                    model.level2FixedPool_[slot] = {};
                    model.level2FixedPool_[slot].renderProfile = WorldActorRenderProfile::noHandles;
                    model.level2FixedPoolKnown_[slot] = true;
                }
            }
            const auto saved = model.level2FixedPool_[expected.rewardSlot];
            model.level2FixedPool_[expected.rewardSlot].stateBytes4dTo52[1] = 100;
            while (std::any_of(model.level2FixedPool_.begin(), model.level2FixedPool_.end(),
                [](const auto& actor) { return !actor.active(); })) {
                Level1Entity request{};
                request.type = 46;
                request.collisionRadius = 2260;
                const auto index = model.allocateLevel2FixedObject(request);
                require(index < model.level2Entities_.size(), "full FE control failed actual allocation");
                auto& actor = model.level2Entities_[index];
                actor.position = {20000, 20000, 0};
                model.level2FixedPool_[model.level2EntityFixedPoolSlots_[index]] = actor;
            }
            model.level2FixedPool_[expected.rewardSlot] = saved;
        }
        auto ground = model.level2Ground_;
        if (boundary != Boundary::pulseUnknown) { ground.advance(0, 0, expected.rng); }
        model.snapshotLevel2Presentation();
        require(model.playableSystemOldActorSlots_ == (boundary == Boundary::earlierDestination ? std::vector<std::size_t>{0, 1}
                : late ? std::vector<std::size_t>{1, 0} : std::vector<std::size_t>{0}),
            "fixture lost its declared inherited physical callback order");
        return expected;
    }

    static void aliases(const GameplayModel& model) {
        std::array<bool, 16> ordinarySeen{};
        for (std::size_t index = 0; index < model.level2Entities_.size(); ++index) {
            const auto ordinary = model.level2EntityActorPoolSlots_[index];
            const auto fixed = model.level2EntityFixedPoolSlots_[index];
            const auto projectile = model.level2EntityProjectilePoolSlots_[index];
            const auto effect = model.level2EntityDeathEffectPoolSlots_[index];
            const auto& physical = ordinary < model.level2ActorPoolSize_ ? model.level2ActorPool_[ordinary]
                : fixed < model.level2FixedObjectPoolSize_ ? model.level2FixedPool_[fixed]
                : projectile < model.level2ProjectilePoolSize_ ? model.level2ProjectilePool_[projectile]
                : model.level2DeathEffectPool_.at(effect);
            if (ordinary < ordinarySeen.size()) {
                require(!ordinarySeen[ordinary], "FF birth left duplicate compact physical aliases");
                ordinarySeen[ordinary] = true;
            }
            const auto& actor = model.level2Entities_[index];
            require(actor.type == physical.type && actor.position == physical.position
                && actor.stateBytes4dTo52 == physical.stateBytes4dTo52
                && actor.collisionRadius == physical.collisionRadius
                && actor.renderProfile == physical.renderProfile,
                "physical/compact aliases diverged at guardian boundary");
        }
    }

    static void verify(const GameplayModel& model, const Boundary boundary, const Expected& expected) {
        test::compareCampaignRng(expected.rng, model.level2Rng_, test::CampaignComparisonStage::boundary);
        require(model.level2VisualRandomState_ == expected.visual,
            "guardian branch changed visual RNG call order");
        const auto& source = model.level2ActorPool_[0];
        if (boundary == Boundary::alias) {
            auto pose = expected.provisional.position;
            pose.z += 500;
            require(model.level2ActorPoolActive_[0] && source.type == 48
                && source.position == pose && source.pitch == expected.provisional.pitch
                && source.roll == 0 && source.yaw == expected.provisional.yaw
                && source.forwardVelocity == 0 && source.yawVelocity == 0
                && source.stateBytes4dTo52[1] == 1 && source.stateBytes4dTo52[4] == 4
                && source.collisionRadius == 559
                && source.behaviorTimer == static_cast<std::uint8_t>(expected.visual >> 16U)
                && source.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback
                && !source.animatedModelPresenterEnabled,
                "source==firstfree did not copy the reset provisional13 pose into unadvanced48");
            require(model.level2ActorIncarnations_[0] == expected.incarnation + 1,
                "source==firstfree did not advance exactly one incarnation");
            const auto index = model.postWaveObjectIndex();
            require(index < model.level2Entities_.size() && model.level2EntityActorPoolSlots_[index] == 0,
                "DS1292 lost its physical alias to the replacement FF0");
        } else if (boundary == Boundary::earlierDestination) {
            const auto& guardian = model.level2ActorPool_[1];
            require(!model.level2ActorPoolActive_[1] && guardian.stateBytes4dTo52[1] == 0
                && model.level2ActorPoolActive_[0] && source.position == FixedPosition{0, 10000, 500}
                && source.type == 48 && source.collisionRadius == 559
                && source.behaviorTimer == static_cast<std::uint8_t>(expected.visual >> 16U)
                && source.stateBytes4dTo52[2] == 0 && source.forwardVelocity == 0
                && model.level2ActorIncarnations_[0] == expected.incarnation + 1,
                "earlier OLD destination received a second callback after its birth");
        } else if (boundary == Boundary::pulseFull || boundary == Boundary::pulseUnknown) {
            require(!model.level2ActorPoolActive_[0] && source.type == 45
                && source.behaviorTimer == 20 && source.stateBytes4dTo52[1] == 0,
                "FE control failed to reach actual dead timer21 callback");
            if (boundary == Boundary::pulseFull) {
                require(std::all_of(model.level2FixedPool_.begin(), model.level2FixedPool_.end(),
                    [](const auto& actor) { return actor.active(); }), "full FE pulse changed occupancy");
                std::array<bool, 16> fixedSeen{};
                std::size_t fixedAliases = 0;
                for (const auto slot : model.level2EntityFixedPoolSlots_) {
                    if (slot >= fixedSeen.size()) { continue; }
                    require(!fixedSeen[slot], "full FE pulse duplicated a physical fixed alias");
                    fixedSeen[slot] = true;
                    ++fixedAliases;
                }
                require(fixedAliases == model.level2FixedObjectPoolSize_,
                    "full FE pulse unexpectedly changed allocation membership");
            } else {
                require(!model.level2FixedPoolKnown_[expected.pulseSlot]
                    && !model.level2FixedPool_[expected.pulseSlot].active(),
                    "unknown FE pulse repaired provenance or allocated into unknown storage");
            }
        } else if (boundary == Boundary::pulse) {
            require(!model.level2ActorPoolActive_[0] && source.stateBytes4dTo52[1] == 0
                && source.type == 45 && source.behaviorTimer == 20
                && source.stateBytes4dTo52[2] == 0 && source.stateBytes4dTo52[5] == 1
                && source.forwardVelocity == 0 && source.yawVelocity == 0,
                "dead timer21 callback lost its source45/timer20 state");
            const auto& pulse = model.level2FixedPool_[expected.pulseSlot];
            const auto& retained = expected.retiredFixed;
            require(pulse.type == 46 && pulse.collisionRadius == 2260 && pulse.position == source.position
                && pulse.stateBytes4dTo52 == std::array<std::uint8_t, 6>{0xfe, 100, 91, 92, 0, 93}
                && pulse.pitch == 0 && pulse.roll == 0 && pulse.yaw == 0
                && pulse.behaviorTimer == retained.behaviorTimer
                && pulse.pitchVelocity == retained.pitchVelocity && pulse.rollVelocity == retained.rollVelocity
                && pulse.yawVelocity == retained.yawVelocity && pulse.xVelocity == retained.xVelocity
                && pulse.yVelocity == retained.yVelocity && pulse.zVelocity == retained.zVelocity
                && pulse.forwardVelocity == retained.forwardVelocity
                && pulse.modelAnimationVariant == retained.modelAnimationVariant
                && pulse.presentationModelAnimationVariant == retained.presentationModelAnimationVariant
                && pulse.renderProfile == retained.renderProfile
                && pulse.animatedModelPresenterEnabled == retained.animatedModelPresenterEnabled,
                "4506 pulse lost nonzero retained FE fields");
        } else {
            const auto& guardian = model.level2ActorPool_[1];
            require(!model.level2ActorPoolActive_[1] && guardian.stateBytes4dTo52[1] == 0
                && model.level2ActorPoolActive_[0] && source.position == FixedPosition{0, 10000, 450}
                && source.collisionRadius == 559
                && model.level2ActorIncarnations_[0] == expected.incarnation + 1,
                "later inherited FF0 did not receive its newborn callback exactly once");
            const auto timer = static_cast<std::uint8_t>(expected.visual >> 16U);
            if (boundary == Boundary::lateZeroTimer) {
                require(source.type == 47 && source.behaviorTimer == 60
                    && source.forwardVelocity == 100 && source.yaw == expected.heading,
                    "later timer0 birth did not consume its extra heading word and become47");
            } else {
                const auto nextTimer = static_cast<std::uint8_t>(timer - 1);
                const auto selector = static_cast<std::int8_t>(nextTimer) <= 12
                    ? static_cast<std::uint8_t>(13 - static_cast<std::int8_t>(nextTimer)) : std::uint8_t{0};
                require(timer != 0 && source.type == 48 && source.behaviorTimer == nextTimer
                    && source.forwardVelocity == 0 && source.stateBytes4dTo52[2] == selector,
                    "later Type48 birth lost its single timer/selector callback");
            }
        }
        const auto reward = model.level2PickupIndex_;
        require(reward < model.level2Entities_.size() && model.level2Entities_[reward].type == 25
            && model.level2EntityFixedPoolSlots_[reward] == expected.rewardSlot,
            "guardian transaction lost its preceding FE reward");
        if (boundary != Boundary::pulseUnknown) { aliases(model); }
    }
};
}

int main(int argc, char** argv) {
    if (argc != 2) { return 2; }
    unsigned failures = 0;
    for (const auto boundary : {Boundary::alias, Boundary::lateDestination, Boundary::lateZeroTimer,
        Boundary::pulse, Boundary::earlierDestination, Boundary::pulseFull, Boundary::pulseUnknown}) {
        const auto label = boundary == Boundary::alias ? "source-firstfree-alias"
            : boundary == Boundary::lateDestination ? "late-old-destination48"
            : boundary == Boundary::lateZeroTimer ? "late-old-timer0-to47"
            : boundary == Boundary::pulse ? "retained-pulse46"
            : boundary == Boundary::earlierDestination ? "earlier-old-destination48"
            : boundary == Boundary::pulseFull ? "full-known-FE-no-phantom-RNG" : "unknown-FE-rejected";
        try {
            auto left = test::makePlayableGuardianSession(argv[1], 1, test::PlayableGuardianEntry::lateFight);
            auto right = test::makePlayableGuardianSession(argv[1], 1, test::PlayableGuardianEntry::lateFight);
            const auto expected = GameplayModelTestAccess::prepare(const_cast<GameplayModel&>(left->model()), boundary);
            GameplayModelTestAccess::prepare(const_cast<GameplayModel&>(right->model()), boundary);
            if (boundary == Boundary::pulseFull) {
                NativeSessionTestAccess::initializeSyntheticFixedCaches(*left);
                NativeSessionTestAccess::initializeSyntheticFixedCaches(*right);
            }
            if (boundary == Boundary::pulseUnknown) {
                for (auto* session : {left.get(), right.get()}) {
                    auto rejected = false;
                    try { session->advancePlayableCampaign({}, session == left.get() ? 10000 : 257); }
                    catch (const std::invalid_argument& error) {
                        rejected = std::string(error.what()) == "Level2 pulse reached unknown fixed-record storage";
                    }
                    require(rejected, "unknown FE provenance was not rejected at the pulse boundary");
                    GameplayModelTestAccess::verify(session->model(), boundary, expected);
                }
                std::cout << label << " PASS (unsupported boundary; adjacent execution excluded)\n";
                continue;
            }
            test::CampaignBudgetComparison comparison;
            for (unsigned frame = 0; frame < 3; ++frame) {
                const auto beforeSamples = left->elapsedSamples();
                const auto beforeLoop = left->model().level2LoopTick();
                const auto a = left->advancePlayableCampaign({}, 10000);
                const auto b = right->advancePlayableCampaign({}, 257);
                comparison.compare(*left, *right, a, b, test::CampaignComparisonStage::boundary);
                require(a.firstSample == beforeSamples && beforeSamples + a.samples.size() == left->elapsedSamples()
                    && left->model().level2LoopTick() == static_cast<std::uint16_t>(beforeLoop + 1),
                    "guardian transaction lost adjacent world/PCM continuity");
                GameplayModelTestAccess::aliases(left->model());
                GameplayModelTestAccess::aliases(right->model());
                if (frame == 0) {
                    GameplayModelTestAccess::verify(left->model(), boundary, expected);
                    GameplayModelTestAccess::verify(right->model(), boundary, expected);
                }
            }
            std::cout << label << " PASS\n";
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << label << " FAIL: " << error.what() << '\n';
        }
    }
    return failures == 0 ? 0 : 1;
}
