#include "native_level2_frame_fixture.hpp"
#include "playable_campaign_comparison.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace stellar7;
constexpr std::size_t firstFreeSlot = 0;
constexpr std::size_t sourceSlot = 1;
constexpr std::uint32_t incomingRngCalls = 25667;
constexpr unsigned pickupSpinDraws = 1;
constexpr unsigned deadCallbackDraws = 1;
constexpr unsigned constructorDraws = 3;
constexpr std::uint32_t visualMultiplier = 0x343fd;
constexpr std::uint32_t visualIncrement = 0x269ec3;
constexpr unsigned adjacentFrames = 2;
void require(const bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
}

namespace stellar7 {
//Controlled initial-boundary setup and read-only assertions. No natural ancestry.
struct GameplayModelTestAccess {
    static void prepareRetiredEmission(GameplayModel& model) {
        const auto guardian = std::find_if(model.level2Entities_.begin(), model.level2Entities_.end(), [](const auto& actor) {
            return actor.type == 44 && actor.stateBytes4dTo52[1] == 1;
        });
        require(guardian != model.level2Entities_.end(), "fixture lost its HP1 guardian");
        const auto index = static_cast<std::size_t>(guardian - model.level2Entities_.begin());
        const auto oldSlot = model.level2EntityActorPoolSlots_.at(index);
        require(oldSlot == firstFreeSlot && !model.level2ActorPoolActive_.at(sourceSlot), "fixture FF ancestry changed");
        require(guardian->collisionRadius == 1147 && guardian->renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation,
            "fixture lost actual Level2 guardian metadata");
        //Declare a previously retired FF0 and a distinct live guardian FF1.
        model.level2ActorPool_[sourceSlot] = *guardian;
        model.level2ActorPoolActive_[sourceSlot] = true;
        model.level2ActorIncarnations_[sourceSlot] = model.level2ActorIncarnations_[oldSlot];
        model.level2ActorPool_[oldSlot] = {};
        model.level2ActorPool_[oldSlot].type = 48;
        model.level2ActorPool_[oldSlot].collisionRadius = 559;
        model.level2ActorPool_[oldSlot].stateBytes4dTo52 = {0xff, 0, 0, 0, 4, 0};
        model.level2ActorPool_[oldSlot].renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
        model.level2ActorPoolActive_[oldSlot] = false;
        model.level2EntityActorPoolSlots_[index] = sourceSlot;
        guardian->stateBytes4dTo52[5] = 0;
        guardian->behaviorTimer = 0;
        guardian->forwardVelocity = 0;
        guardian->collisionContactAllocationIndex = std::numeric_limits<std::size_t>::max();
        guardian->collisionContactIsPlayer = false;
        model.level2ActorPool_[sourceSlot] = *guardian;
        require(model.level2CompletionObjectIndex_ == std::numeric_limits<std::size_t>::max(), "fixture retained a completion craft");
        StellarRng rng;
        for (std::uint32_t call = 0; call < incomingRngCalls; ++call) { rng.next(); }
        model.level2Rng_ = rng;
        model.snapshotLevel2Presentation();
        require(model.playableSystemOldActorSlots_ == std::vector<std::size_t>{sourceSlot},
            "controlled entry lost the sole inherited guardian callback");
    }

    static auto expectedRng(const GameplayModel& model) {
        auto rng = model.level2Rng_;
        require(rng.next() == 10703 && rng.next() == 19712, "declared spin/dead-callback words changed");
        for (unsigned call = 0; call < constructorDraws; ++call) { rng.next(); }
        auto ground = model.level2Ground_;
        ground.advance(0, 0, rng);
        return rng;
    }

    static std::uint32_t visualState(const GameplayModel& model) { return model.level2VisualRandomState_; }

    static void aliases(const GameplayModel& model) {
        for (std::size_t index = 0; index < model.level2Entities_.size(); ++index) {
            const auto ordinary = model.level2EntityActorPoolSlots_[index];
            const auto fixed = model.level2EntityFixedPoolSlots_[index];
            const auto projectile = model.level2EntityProjectilePoolSlots_[index];
            const auto effect = model.level2EntityDeathEffectPoolSlots_[index];
            const auto& physical = ordinary < model.level2ActorPoolSize_ ? model.level2ActorPool_[ordinary]
                : fixed < model.level2FixedObjectPoolSize_ ? model.level2FixedPool_[fixed]
                : projectile < model.level2ProjectilePoolSize_ ? model.level2ProjectilePool_[projectile]
                : model.level2DeathEffectPool_.at(effect);
            const auto& actor = model.level2Entities_[index];
            require(actor.type == physical.type && actor.position == physical.position
                && actor.stateBytes4dTo52 == physical.stateBytes4dTo52 && actor.collisionRadius == physical.collisionRadius
                && actor.renderProfile == physical.renderProfile, "postmortem insertion lost physical aliases");
        }
    }

    static void emitted(const GameplayModel& model, const std::uint32_t oldVisual) {
        const auto& source = model.level2ActorPool_.at(sourceSlot);
        const auto& birth = model.level2ActorPool_.at(firstFreeSlot);
        const auto expectedVisual = oldVisual * visualMultiplier + visualIncrement;
        require(!model.level2ActorPoolActive_[sourceSlot] && source.stateBytes4dTo52[1] == 0,
            "postmortem callback revived its source guardian");
        require(model.level2ActorPoolActive_[firstFreeSlot] && birth.type == 48 && birth.collisionRadius == 559
            && birth.position == FixedPosition{0, 10000, 500}
            && birth.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback
            && birth.behaviorTimer == static_cast<std::uint8_t>(expectedVisual >> 16U)
            && birth.stateBytes4dTo52[2] == 0,
            "retired A92E did not allocate the unadvanced source AE10 Type48");
        require(model.level2VisualRandomState_ == expectedVisual, "AE10 did not consume exactly one visual RNG update");
        const auto reward = model.level2PickupIndex_;
        require(reward < model.level2Entities_.size() && model.level2Entities_[reward].type == 25
            && model.level2EntityFixedPoolSlots_[reward] < model.level2FixedObjectPoolSize_
            && model.level2Entities_[reward].renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback,
            "FF insertion lost the pending FE pickup owner/index");
        if (model.playableSystemOldActorSlots_ != std::vector<std::size_t>{firstFreeSlot}) {
            std::cerr << "successor OLD:";
            for (const auto slot : model.playableSystemOldActorSlots_) { std::cerr << ' ' << slot; }
            std::cerr << " visibility=" << static_cast<int>(model.level2Visibility_.tracking()) << '\n';
        }
        require(model.playableSystemOldActorSlots_ == std::vector<std::size_t>{firstFreeSlot},
            "postmortem birth did not become the successor frame's inherited actor");
        aliases(model);
    }
};
}

int main(int argc, char** argv) {
    if (argc != 2) { return 2; }
    try {
        auto left = test::makePlayableGuardianSession(argv[1], 1, test::PlayableGuardianEntry::lateFight);
        auto right = test::makePlayableGuardianSession(argv[1], 1, test::PlayableGuardianEntry::lateFight);
        GameplayModelTestAccess::prepareRetiredEmission(const_cast<GameplayModel&>(left->model()));
        GameplayModelTestAccess::prepareRetiredEmission(const_cast<GameplayModel&>(right->model()));
        const auto expected = GameplayModelTestAccess::expectedRng(left->model());
        const auto oldVisual = GameplayModelTestAccess::visualState(left->model());
        const auto* leftOwner = left.get();
        const auto* rightOwner = right.get();
        const auto* leftSynth = &left->synth();
        const auto* rightSynth = &right->synth();
        test::CampaignBudgetComparison comparison;
        for (unsigned call = 0; call <= adjacentFrames; ++call) {
            const auto beforeSamples = left->elapsedSamples();
            const auto beforeWorld = left->model().level2LoopTick();
            const auto beforeClock = left->synth().driverState().normalClockCycles;
            const auto a = left->advancePlayableCampaign({}, 10000);
            const auto b = right->advancePlayableCampaign({}, 257);
            comparison.compare(*left, *right, a, b, test::CampaignComparisonStage::boundary);
            require(left.get() == leftOwner && right.get() == rightOwner
                && &left->synth() == leftSynth && &right->synth() == rightSynth
                && a.firstSample == beforeSamples && beforeSamples + a.samples.size() == left->elapsedSamples()
                && left->synth().driverState().normalClockCycles >= beforeClock
                && left->model().level2LoopTick() == static_cast<std::uint16_t>(beforeWorld + 1),
                "postmortem boundary replaced owners or lost adjacent world/PCM/clock");
            GameplayModelTestAccess::aliases(left->model());
            GameplayModelTestAccess::aliases(right->model());
            if (call == 0) {
                GameplayModelTestAccess::emitted(left->model(), oldVisual);
                GameplayModelTestAccess::emitted(right->model(), oldVisual);
                test::compareCampaignRng(expected, left->model().level2Rng(), test::CampaignComparisonStage::boundary);
                require(left->model().level2Rng().calls() >= incomingRngCalls
                    + pickupSpinDraws + deadCallbackDraws + constructorDraws, "retired AE10 dropped constructor RNG");
            }
        }
        std::cout << "Controlled resource-backed retired Level2 A92E/AE10 plus two adjacent frames; budget twins PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
