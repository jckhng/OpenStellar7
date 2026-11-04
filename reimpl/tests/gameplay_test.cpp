#include "stellar7/gameplay.hpp"
#include "stellar7/completion_beam.hpp"
#include "stellar7/input_movie.hpp"
#include "stellar7/native_checkpoint.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
#include <memory>
#include <cstdlib>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace stellar7 {

struct GameplayModelTestAccess {
    static void assertLevel5RemovedOrdinaryBehavior() {
        std::vector<Model3d> models(14);
        models[0].collisionRadius = 100;
        models[5].collisionRadius = 1008;
        models[5].flags = 1;
        models[8].collisionRadius = 860;
        models[8].flags = 1;
        models[11].collisionRadius = 748;
        models[11].flags = 1;
        models[11].animationLimits = {8};
        models[13].collisionRadius = 559;
        for (unsigned targetCase = 0; targetCase < 5; ++targetCase) {
            const auto targetSlot = targetCase == 1 ? std::size_t{3} : std::size_t{0};
            auto model = std::make_unique<GameplayModel>();
            model->initializeLevel5(models, StellarRng{}, 680);
            for (unsigned frame = 0; frame < 2; ++frame) {
                model->tick({});
                model->finishLevel5Frame();
            }
            GameplayInput fire;
            fire.fire = true;
            model->tick(fire);
            model->finishLevel5Frame();
            model->tick({});
            model->finishLevel5Frame();
            const auto shotSlot = std::find(model->level2EntityProjectilePoolSlots_.begin(),
                model->level2EntityProjectilePoolSlots_.end(), 0);
            const auto shotIndex = static_cast<std::size_t>(shotSlot - model->level2EntityProjectilePoolSlots_.begin());
            model->level2Entities_[shotIndex].position = {0, 5000, 0};
            for (auto& actor : model->level2Entities_) {
                if (actor.type != 23) {
                    actor.position = {30000, 30000, 0};
                    actor.behaviorTimer = 7;
                    actor.stateBytes4dTo52[5] = 0;
                    actor.forwardVelocity = 0;
                    actor.xVelocity = actor.yVelocity = actor.zVelocity = 0;
                }
            }
            auto& target = model->level2Entities_[targetSlot];
            target.position = {0, 5500, 0};
            target.weaponSlotTimers[0] = 4;
            target.presenterDistance = 5500;
            if (targetCase == 2) {
                target.position.z = 150;
                target.yaw = 0x8000;
                target.yawVelocity = 0;
                target.stateBytes4dTo52[5] = 3;
                target.weaponSlotTimers[0] = 0;
            }
            if (targetCase >= 3) {
                target.type = targetCase == 3 ? 5 : 8;
                target.yaw = 0x8000;
                target.yawVelocity = 0;
                target.renderProfile = WorldActorRenderProfile::noHandles;
                target.stateBytes4dTo52[4] = targetCase == 3 ? 1 : 2;
                target.stateBytes4dTo52[5] = targetCase == 3 ? 1 : 0;
                target.weaponSlotCount = 3;
            }
            const auto profile = target.renderProfile;
            const auto score = model->level2Score_;
            model->level2ActiveListOrder_ = model->level2AllocationListOrder_;
            model->tick({});
            assert(!model->level2ActorPoolActive_[targetSlot]);
            const auto& removed = model->level2ActorPool_[targetSlot];
            assert(removed.stateBytes4dTo52[1] == 0 && removed.renderProfile == profile);
            assert(model->level1State().lastDestroyedType() == removed.type);
            assert(model->level1State().consecutiveDestroyedTypeCount() == 1);
            if (targetCase >= 3) {
                assert(removed.type == (targetCase == 3 ? 5 : 8));
                assert(removed.yawVelocity == 0);
                assert(removed.weaponSlotTimers[0] == 3);
                assert(model->level2Score_ == score + (targetCase == 3 ? 75 : 25));
            } else if (targetSlot == 0) {
                assert(removed.type == 11);
                if (targetCase == 2) {
                    assert(removed.behaviorTimer == 63 && removed.stateBytes4dTo52[5] == 4);
                    assert(removed.weaponSlotTimers[0] == 59);
                    const auto nested = std::find(model->level2EntityProjectilePoolSlots_.begin(),
                        model->level2EntityProjectilePoolSlots_.end(), 0);
                    assert(nested != model->level2EntityProjectilePoolSlots_.end());
                    const auto& shot = model->level2Entities_[static_cast<std::size_t>(nested - model->level2EntityProjectilePoolSlots_.begin())];
                    assert(shot.type == 20 && shot.ownerAllocationIndex == targetSlot);
                    assert(shot.position == FixedPosition({0, 5500, 150}));
                    assert(shot.stateBytes4dTo52[1] == 60 && model->level2AudioEvents_.back().eventId == 15);
                } else {
                    assert(removed.behaviorTimer == 6 && removed.weaponSlotTimers[0] == 3);
                }
                assert(removed.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation);
                assert(model->level2Score_ == score + 50);
            } else {
                assert(removed.type == 13 && removed.xVelocity == 0 && removed.yVelocity == -224);
                assert(model->level2Score_ == score + 10);
            }
            assert(std::count(model->level2DeathEffectPoolActive_.begin(), model->level2DeathEffectPoolActive_.end(), true) == 1);
            const auto retired = removed;
            model->finishLevel5Frame();
            for (unsigned frame = 0; frame < 2; ++frame) {
                model->tick({});
                model->finishLevel5Frame();
                const auto& retained = model->level2ActorPool_[targetSlot];
                assert(!model->level2ActorPoolActive_[targetSlot]);
                assert(retained.position == retired.position && retained.behaviorTimer == retired.behaviorTimer);
                assert(retained.stateBytes4dTo52 == retired.stateBytes4dTo52);
                assert(retained.modelAnimationVariant == retired.modelAnimationVariant);
            }
            model->failureFromLevel2_ = true;
            model->phase_ = GameplayPhase::continuePrompt;
            model->pressedContinueChoice_ = ContinueChoice::yes;
            model->level2PlayerArmor_ = -1;
            model->failureRng_ = model->level2Rng_;
            model->completeLevel5Retry(models, HighscoreReturn::rankMiss);
            assert(model->level1State().lastDestroyedType() == retired.type);
            assert(model->level1State().consecutiveDestroyedTypeCount() == 0);
        }
    }

    static void assertLevel5CombatBonus() {
        std::vector<Model3d> models(43);
        models[0].collisionRadius = 100;
        models[5].collisionRadius = 1008;
        models[8].collisionRadius = 860;
        models[11].collisionRadius = 748;
        models[11].flags = 1;
        models[11].animationLimits = {8};
        models[13].collisionRadius = 559;
        models[42].collisionRadius = 330;
        auto model = std::make_unique<GameplayModel>();
        model->initializeLevel5(models, StellarRng{}, 680);
        for (unsigned frame = 0; frame < 2; ++frame) {
            model->tick({});
            model->finishLevel5Frame();
        }
        const auto level1Entities = model->level1State().entities().size();
        for (std::size_t victimSlot = 0; victimSlot < 3; ++victimSlot) {
            for (auto& actor : model->level2Entities_) {
                actor.position = {30000, 30000, 0};
                actor.forwardVelocity = 0;
                actor.xVelocity = actor.yVelocity = actor.zVelocity = 0;
                actor.behaviorTimer = 7;
                actor.weaponSlotTimers.fill(60);
                actor.stateBytes4dTo52[5] = 0;
            }
            model->level2Player_.weaponCooldown = 0;
            model->level2Player_.weaponSlotTimers.fill(0);
            GameplayInput fire;
            fire.fire = true;
            model->tick(fire);
            model->finishLevel5Frame();
            model->tick({});
            model->finishLevel5Frame();
            const auto shotSlot = std::find(model->level2EntityProjectilePoolSlots_.begin(),
                model->level2EntityProjectilePoolSlots_.end(), 0);
            assert(shotSlot != model->level2EntityProjectilePoolSlots_.end());
            model->level2Entities_[static_cast<std::size_t>(shotSlot - model->level2EntityProjectilePoolSlots_.begin())].position = {0, 5000, 0};
            const auto victim = std::find(model->level2EntityActorPoolSlots_.begin(),
                model->level2EntityActorPoolSlots_.end(), victimSlot);
            assert(victim != model->level2EntityActorPoolSlots_.end());
            auto& actor = model->level2Entities_[static_cast<std::size_t>(victim - model->level2EntityActorPoolSlots_.begin())];
            actor.position = {0, 5500, 0};
            actor.forwardVelocity = 0;
            actor.yawVelocity = 0;
            model->level2ActiveListOrder_ = model->level2AllocationListOrder_;
            model->tick({});
            model->finishLevel5Frame();
            assert(!model->level2ActorPoolActive_[victimSlot]);
        }
        assert(model->level2Score_ == 150);
        assert(model->level1State().lastDestroyedType() == 11);
        assert(model->level1State().consecutiveDestroyedTypeCount() == 0);
        assert(model->level1State().entities().size() == level1Entities);
        const auto pickup = std::find_if(model->level2Entities_.begin(), model->level2Entities_.end(),
            [](const auto& actor) { return actor.type == 42; });
        assert(pickup != model->level2Entities_.end());
        const auto index = static_cast<std::size_t>(pickup - model->level2Entities_.begin());
        assert(model->liveLevel2PoolSlot(index) == 16);
        assert(pickup->stateBytes4dTo52 == (std::array<std::uint8_t, 6>{254, 1, 3, 0, 0, 3}));
        assert(pickup->position == FixedPosition({0, 5500, 100}));
        assert(pickup->yaw == 0 && pickup->roll == 0);
        assert(pickup->yawVelocity == 0x800 && pickup->rollVelocity == 0x800);
        assert(pickup->renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback);
        model->tick({});
        model->finishLevel5Frame();
        const auto retained = std::find_if(model->level2Entities_.begin(), model->level2Entities_.end(),
            [](const auto& actor) { return actor.type == 42; });
        assert(retained->yaw == 0x800 && retained->roll == 0x800);
        model->level2Player_.position = {0, 5500, 0};
        const auto armor = model->playerArmor();
        model->tick({});
        model->finishLevel5Frame();
        assert(model->level2Score_ == 250 && model->playerArmor() == armor);
        assert(model->level2AudioEvents_.back().eventId == 24);
        assert(model->level1State().systemStatusLevels()[2] == 3);
        assert(std::none_of(model->level2Entities_.begin(), model->level2Entities_.end(),
            [](const auto& actor) { return actor.type == 42; }));
    }

    static void assertLevel5PlayerFire() {
        std::vector<Model3d> models(14);
        models[0].collisionRadius = 100;
        models[5].collisionRadius = 1008;
        models[5].flags = 1;
        models[8].collisionRadius = 860;
        models[8].flags = 1;
        models[11].collisionRadius = 748;
        models[11].flags = 1;
        models[11].animationLimits = {8};
        models[13].collisionRadius = 559;
        auto model = std::make_unique<GameplayModel>();
        model->initializeLevel5(models, StellarRng{}, 680);
        GameplayInput fire;
        fire.fire = true;
        bool rejected = false;
        try { model->tick(fire); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected && model->phase_ == GameplayPhase::level5LoaderReturn && model->ticks_ == 0);
        for (unsigned frame = 0; frame < 2; ++frame) {
            model->tick({});
            model->finishLevel5Frame();
        }
        model->tick(fire);
        const auto precedingOrder = model->level2ActiveListOrder_;
        const auto precedingEntities = model->level2Entities_.size();
        const auto events = model->level2AudioEvents_.size();
        const auto pose = model->level2Player_.position;
        const auto rngCalls = model->level2Rng_.calls();
        model->finishLevel5Frame();
        assert(model->level2Entities_.size() == precedingEntities + 1);
        assert(model->level2ActiveListOrder_ == precedingOrder);
        assert(model->level2AllocationListOrder_.size() == precedingEntities + 1);
        assert(model->level2Visibility_.refreshRequested());
        assert(model->level2Rng_.calls() == rngCalls);
        assert(model->level2AudioEvents_.size() == events + 1);
        assert(model->level2AudioEvents_.back().eventId == 14);
        assert(model->weaponIndicatorVisible_ && !model->weaponReadyIndicator_);
        assert(model->level2Player_.weaponCooldown == 8);
        assert((model->level2Player_.weaponSlotTimers == std::array<std::uint8_t, 3>{31, 0, 0}));
        const auto shotIndex = model->level2AllocationListOrder_.back();
        const auto shot = model->level2Entities_[shotIndex];
        assert(shot.type == 23 && shot.ownerIsPlayer && shot.collisionRadius == 134);
        assert(shot.position == pose && shot.forwardVelocity == 500);
        assert(shot.stateBytes4dTo52[1] == 64 && model->level2EntityProjectilePoolSlots_[shotIndex] == 0);
        model->tick(fire);
        assert(model->level2Entities_[shotIndex].position == shot.position);
        assert(model->level2Entities_[shotIndex].stateBytes4dTo52[1] == 64);
        assert(std::find(model->level2ActiveListOrder_.begin(), model->level2ActiveListOrder_.end(), shotIndex)
            != model->level2ActiveListOrder_.end());
        model->finishLevel5Frame();
        assert(model->level2AudioEvents_.size() == events + 1);
        auto movedShot = shot;
        advanceEntityKinematics(movedShot);
        model->tick(fire);
        assert(model->level2Entities_[shotIndex].position == movedShot.position);
        assert(model->level2Entities_[shotIndex].stateBytes4dTo52[1] == 63);
        model->finishLevel5Frame();
        for (unsigned frame = 0; frame < 5; ++frame) {
            model->tick(fire);
            model->finishLevel5Frame();
        }
        assert(model->level2Player_.weaponCooldown == 1 && !model->weaponReadyIndicator_);
        model->tick(fire);
        model->finishLevel5Frame();
        assert(model->level2Player_.weaponCooldown == 0 && model->weaponReadyIndicator_);
        assert(model->level2AudioEvents_.size() == events + 1);
        model->tick(fire);
        const auto firstSlot = model->level2Player_.weaponSlotTimers[0];
        model->finishLevel5Frame();
        assert(model->level2Player_.weaponCooldown == 8 && !model->weaponReadyIndicator_);
        assert(model->level2Player_.weaponSlotTimers[0] == firstSlot - 1);
        assert(model->level2Player_.weaponSlotTimers[1] == 31);
        assert(model->level2AudioEvents_.size() == events + 2);
        assert(model->level2Entities_[model->level2AllocationListOrder_.back()].stateBytes4dTo52[1] == 64);
        auto partial = std::make_unique<GameplayModel>();
        partial->initializeLevel5(models, StellarRng{}, 680);
        auto& freed = partial->level2ProjectilePool_[0];
        freed.pitchVelocity = 11;
        freed.rollVelocity = -12;
        freed.yawVelocity = 13;
        freed.xVelocity = -14;
        freed.yVelocity = 15;
        freed.zVelocity = -16;
        freed.stateBytes4dTo52 = {253, 0, 99, 8, 5, 17};
        freed.presenterDistance = 12345;
        freed.targetHeading = 0x1234;
        partial->level2PendingInput_ = fire;
        assert(partial->consumeLevel2DeferredTail());
        const auto& reused = partial->level2Entities_.back();
        assert(reused.pitchVelocity == 11 && reused.rollVelocity == -12 && reused.yawVelocity == 13);
        assert(reused.xVelocity == -14 && reused.yVelocity == 15 && reused.zVelocity == 0);
        assert((reused.stateBytes4dTo52 == std::array<std::uint8_t, 6>{253, 64, 0, 8, 0, 17}));
        assert(reused.presenterDistance == 12345 && reused.targetHeading == 0x1234);
        auto terminal = std::make_unique<GameplayModel>();
        terminal->initializeLevel5(models, StellarRng{}, 680);
        for (unsigned frame = 0; frame < 2; ++frame) {
            terminal->tick({});
            terminal->finishLevel5Frame();
        }
        terminal->tick(fire);
        terminal->finishLevel5Frame();
        terminal->tick({});
        terminal->finishLevel5Frame();
        auto terminalShot = std::find(terminal->level2EntityProjectilePoolSlots_.begin(),
            terminal->level2EntityProjectilePoolSlots_.end(), 0);
        const auto terminalIndex = static_cast<std::size_t>(terminalShot - terminal->level2EntityProjectilePoolSlots_.begin());
        auto& lastShot = terminal->level2Entities_[terminalIndex];
        lastShot.position = {0, 5000, 0};
        lastShot.stateBytes4dTo52[1] = 1;
        auto lastPosition = lastShot;
        advanceEntityKinematics(lastPosition);
        for (auto& actor : terminal->level2Entities_) {
            if (actor.type != 23) { actor.position = {30000, 30000, 0}; }
        }
        auto& target = terminal->level2Entities_[3];
        target.position = lastPosition.position;
        target.stateBytes4dTo52[1] = 2;
        const auto terminalEvents = terminal->level2AudioEvents_.size();
        terminal->tick({});
        assert(terminal->level2Entities_[3].stateBytes4dTo52[1] == 1);
        assert(!terminal->level2ProjectilePoolActive_[0]);
        assert(terminal->level2ProjectilePool_[0].position == lastPosition.position);
        assert(terminal->level2AudioEvents_.size() == terminalEvents + 1);
        assert(terminal->level2AudioEvents_.back().eventId == 5);
    }

    static void assertLevel5RankMiss() {
        std::vector<Model3d> models(14);
        models[0].collisionRadius = 100;
        models[5].collisionRadius = 1008;
        models[5].flags = 1;
        models[8].collisionRadius = 860;
        models[8].flags = 1;
        models[11].collisionRadius = 748;
        models[11].flags = 1;
        models[11].animationLimits = {8};
        models[13].collisionRadius = 559;
        auto source = std::make_unique<GameplayModel>();
        source->initializeLevel5(models, StellarRng{}, 680);
        source->failureFromLevel2_ = true;
        source->phase_ = GameplayPhase::continuePrompt;
        source->pressedContinueChoice_ = ContinueChoice::yes;
        source->remainingContinues_ = 2;
        source->level2PlayerArmor_ = -24;
        source->failureRng_ = StellarRng{};
        auto expectedRng = source->failureRng_;
        for (unsigned index = 0; index < 15; ++index) { expectedRng.next(); }
        auto accepted = std::make_unique<GameplayModel>(*source);
        accepted->completeLevel5Retry(models, HighscoreReturn::rankMiss);
        assert(accepted->phase_ == GameplayPhase::level5LoaderReturn && accepted->activeCampaignLevel_ == 4);
        assert(accepted->remainingContinues_ == 1 && accepted->level2PlayerArmor_ == 680);
        assert(accepted->level2Score_ == 0 && !accepted->pressedContinueChoice_);
        assert(accepted->ticks_ == source->ticks_ + 1 && accepted->level2LoopTick_ == 0);
        assert(accepted->level2Rng_.calls() == expectedRng.calls());
        for (unsigned index = 0; index < 56; ++index) {
            assert(accepted->level2Rng_.next() == expectedRng.next());
        }
        for (unsigned invalidCase = 0; invalidCase < 7; ++invalidCase) {
            auto invalid = std::make_unique<GameplayModel>(*source);
            auto resources = models;
            if (invalidCase == 0) { invalid->phase_ = GameplayPhase::highscoreHall; }
            if (invalidCase == 1) { invalid->pressedContinueChoice_ = ContinueChoice::no; }
            if (invalidCase == 2) { invalid->pressedContinueChoice_.reset(); }
            if (invalidCase == 3) { invalid->remainingContinues_ = 0; }
            if (invalidCase == 4) { resources[0].collisionRadius = 0; }
            if (invalidCase == 5) { resources[5].flags = 2; }
            if (invalidCase == 6) { resources[8].collisionRadius = 0; }
            const auto budget = invalid->remainingContinues_;
            const auto ticks = invalid->ticks_;
            const auto phase = invalid->phase_;
            const auto choice = invalid->pressedContinueChoice_;
            auto rng = invalid->level2Rng_;
            bool rejected = false;
            try { invalid->completeLevel5Retry(resources, HighscoreReturn::rankMiss); }
            catch (const std::invalid_argument&) { rejected = true; }
            assert(rejected && invalid->remainingContinues_ == budget && invalid->ticks_ == ticks);
            assert(invalid->phase_ == phase && invalid->pressedContinueChoice_ == choice);
            assert(invalid->level2PlayerArmor_ == -24 && invalid->level2Rng_.calls() == rng.calls());
            for (unsigned index = 0; index < 56; ++index) { assert(invalid->level2Rng_.next() == rng.next()); }
        }
    }

    static void assertLevel5GroundType5() {
        for (const auto behavior : {1, 4}) {
            GameplayModel model;
            model.initializeLevel2({}, GameplayModel::Level2EntryOrigin::retry);
            model.level2Entities_.resize(1);
            model.initializeCampaignPools(4, 1);
            model.phase_ = GameplayPhase::level5;
            model.level2Visibility_.resetLevel();
            auto& actor = model.level2Entities_[0];
            actor.type = 5;
            actor.position = {30000, 30000, 0};
            actor.forwardVelocity = behavior == 1 ? 0 : 135;
            actor.yawVelocity = 0;
            actor.behaviorTimer = 0;
            actor.stateBytes4dTo52 = {255, 3, 0, 0, 1, static_cast<std::uint8_t>(behavior)};
            actor.weaponSlotCount = 3;
            model.advanceLevel2({});
            assert(model.level2Entities_[0].forwardVelocity == (behavior == 1 ? 135 : -67));
            if (behavior == 4) {
                assert(model.level2Entities_[0].behaviorTimer == 63);
                assert(std::abs(model.level2Entities_[0].yawVelocity) == 512);
            }
        }
        GameplayModel model;
        model.initializeLevel2({}, GameplayModel::Level2EntryOrigin::retry);
        model.activeCampaignLevel_ = 4;
        auto& owner = model.level2Entities_[0];
        owner.type = 5;
        owner.position = {30000, 30000, 0};
        owner.yaw = owner.targetHeading = 0x1234;
        owner.stateBytes4dTo52 = {255, 3, 0, 0, 1, 0};
        owner.weaponSlotCount = 3;
        owner.weaponCooldown = 0;
        owner.weaponSlotTimers = {};
        assert(model.fireLevel2EnemyProjectile(0));
        const auto& shot = model.level2Entities_.back();
        assert(shot.type == 22 && shot.position == FixedPosition({30000, 30000, 0}));
        assert(shot.yaw == 0x1234 && shot.forwardVelocity == 500 && shot.collisionRadius == 134);
        assert((shot.stateBytes4dTo52 == std::array<std::uint8_t, 6>{253, 84, 0, 0, 0, 0}));
        assert(model.level2Entities_[0].weaponCooldown == 12);
        assert((model.level2Entities_[0].weaponSlotTimers == std::array<std::uint8_t, 3>{84, 0, 0}));
        assert(model.level2AudioEvents_.back().eventId == 15);
    }

    static void assertLevel5FailureHorizon() {
        GameplayModel model;
        model.activeCampaignLevel_ = 4;
        model.failureFromLevel2_ = true;
        model.phase_ = GameplayPhase::failureExplosion;
        GameplayAssets assets;
        assets.cockpit.palette.resize(256);
        assets.campaignLevels.resize(5);
        assets.campaignLevels[4].bitmapGroups.resize(1);
        IndexedBitmap horizon;
        horizon.width = 256;
        horizon.height = 27;
        horizon.pixels.assign(horizon.width * horizon.height, 123);
        assets.campaignLevels[4].bitmapGroups[0].assign(4, horizon);
        const IndexedScreen destination{std::vector<std::uint8_t>(320 * 200, 99), assets.cockpit.palette};
        const auto frame = renderFailurePageIndexed(assets, model, destination, {0, 319, 0, 199});
        for (int y = 0; y < 200; ++y) {
            const auto color = y < 73 ? 169 : y < 100 ? 123 : 47;
            for (int x = 0; x < 320; ++x) {
                assert(frame.pixels[y * 320 + x] == color);
            }
        }
    }

    static void assertLevel2FixedPoolOwnership() {
        auto model = std::make_unique<GameplayModel>();
        model->initializeLevel2({}, GameplayModel::Level2EntryOrigin::retry);
        for (std::size_t slot = 2; slot < GameplayModel::level2FixedObjectPoolSize_; ++slot) {
            Level1Entity fixed{};
            fixed.type = 46;
            fixed.collisionRadius = 2260;
            fixed.stateBytes4dTo52 = {254, 100, 0, 0, 0, 0};
            const auto index = model->allocateLevel2FixedObject(fixed);
            assert(index != static_cast<std::size_t>(-1));
            model->snapshotLevel2Presentation();
            assert(model->level2PresentationPoolSlot(index) == 16 + slot);
            assert(model->level2PresentationPoolSlot(5) == 16);
            assert(model->level2PresentationPoolSlot(6) == 17);
            const auto expected = slot < 8 ? WorldActorRenderProfile::noHandles : WorldActorRenderProfile::unknown;
            assert(model->level2Entities_[index].renderProfile == expected);
        }
        Level1Entity full{};
        full.type = 46;
        assert(model->allocateLevel2FixedObject(full) == static_cast<std::size_t>(-1));
    }

    static void assertRetainedFailureRejectsCallbacks() {
        GameplayModel model;
        model.failureFromLevel2_ = true;
        model.phase_ = GameplayPhase::failureExplosion;
        Level1Entity effect{};
        effect.type = -1;
        effect.renderProfile = WorldActorRenderProfile::bitmapEffectCallback;
        effect.stateBytes4dTo52[0] = 0xfc;
        model.failureMissionEntities_.push_back(effect);
        GameplayAssets assets;
        assets.cockpit.palette.resize(256);
        const IndexedScreen destination{std::vector<std::uint8_t>(320 * 200), assets.cockpit.palette};
        bool rejected = false;
        try { renderFailurePageIndexed(assets, model, destination, {0, 318, 0, 199}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }

    static void assertFailureRetiresProjectiles() {
        for (const auto type : {17, 19, 20, 23}) {
            GameplayModel model;
            model.initializeLevel2({}, GameplayModel::Level2EntryOrigin::campaign);
            model.level2PendingInput_.fire = true;
            assert(model.consumeLevel2DeferredTail());
            auto& projectile = model.level2Entities_.back();
            assert(projectile.stateBytes4dTo52[0] == 0xfd);
            projectile.type = static_cast<std::int16_t>(type);
            model.failureFromLevel2_ = true;
            model.phase_ = GameplayPhase::failurePreExplosionBlackout;
            model.initializeFailurePresentation();
            assert(std::none_of(model.failureMissionEntities_.begin(), model.failureMissionEntities_.end(),
                [](const auto& entity) { return entity.stateBytes4dTo52[0] == 0xfd; }));
            assert(std::none_of(model.level2ProjectilePoolActive_.begin(), model.level2ProjectilePoolActive_.end(),
                [](const bool active) { return active; }));
            assert(projectile.stateBytes4dTo52[1] == 0);
        }
    }

    static void assertFinalLevel2FailureTail() {
        GameplayModel model;
        model.initializeLevel2({}, GameplayModel::Level2EntryOrigin::campaign);
        model.phase_ = GameplayPhase::failurePreExplosionBlackout;
        model.failureFromLevel2_ = true;
        model.level2HasAdvanced_ = true;
        model.level2LoopTick_ = 955;
        model.level2PlayerArmor_ = -4;
        model.level2Player_.position = {100, -50, 0};
        model.level2Player_.yawVelocity = 0;
        model.level2Player_.forwardVelocity = 0;
        model.level2Player_.weaponCooldown = 2;
        model.level2Player_.weaponSlotTimers = {5, 1, 0};
        model.level2PendingInput_.left = true;
        model.level2PendingInput_.forward = true;
        const auto position = model.level2Entities_.front().position;
        const auto calls = model.level2Rng_.calls();
        const auto ticks = model.ticks();
        assert(model.pendingLevel2FailureTail());
        model.finishLevel2FailureTail();
        assert(!model.pendingLevel2FailureTail());
        assert(model.ticks() == ticks && model.level2LoopTick() == 955);
        assert(model.failureSourceLoopTick_ == 956);
        assert(model.level2Player_.position == FixedPosition{});
        assert(model.level2Player_.yawVelocity == 80 && model.level2Player_.forwardVelocity == 10);
        assert(model.level2Player_.weaponCooldown == 1);
        assert((model.level2Player_.weaponSlotTimers == std::array<std::uint8_t, 3>{4, 0, 0}));
        assert(model.level2Entities_.front().position.x == position.x - 100);
        assert(model.level2Entities_.front().position.y == position.y + 50);
        assert(model.level2Rng_.calls() == calls && model.level2AudioEvents_.empty());
        bool rejected = false;
        try { model.finishLevel2FailureTail(); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected && model.failureSourceLoopTick_ == 956);
        model.tick({});
        assert(model.phase() == GameplayPhase::failureExplosion);
        assert(model.failureSourceLoopTick_ == 957 && model.level2LoopTick() == 955);
        assert(model.level2Player_.weaponCooldown == 1);

        GameplayModel periodic;
        periodic.initializeLevel2({}, GameplayModel::Level2EntryOrigin::campaign);
        periodic.phase_ = GameplayPhase::failurePreExplosionBlackout;
        periodic.failureFromLevel2_ = true;
        periodic.level2HasAdvanced_ = true;
        periodic.level2LoopTick_ = 960;
        assert(!periodic.pendingLevel2FailureTail());
    }

    static void assertPresentationIdentitySurvivesRetirement() {
        GameplayModel model;
        model.initializeLevel2({}, GameplayModel::Level2EntryOrigin::campaign);
        model.phase_ = GameplayPhase::level2Handoff;
        const auto fixedFirst = model.level2PresentationPoolSlot(5);
        const auto fixedSecond = model.level2PresentationPoolSlot(6);
        assert(fixedFirst == 16 && fixedSecond == 17);
        const auto revision = model.level2AllocationRevision();
        auto& retiring = model.level2Entities_[4];
        retiring.position = {};
        retiring.xVelocity = 0;
        retiring.yVelocity = 0;
        model.tick({});
        assert(model.level2Entities().size() == 6);
        assert(model.level2Entities()[4].allocationIndex == 4);
        assert(model.level2PresentationPoolSlot(4) == fixedFirst);
        assert(model.level2PresentationPoolSlot(5) == fixedSecond);
        assert(model.level2AllocationRevision() == revision);
        auto replacement = model.level2Entities_[3];
        const auto allocated = model.allocateLevel2Actor(replacement);
        assert(allocated < model.level2Entities_.size());
        assert(model.level2AllocationRevision() == revision + 1);
        assert(!model.level2PresentationPoolSlot(5));
    }

    enum class FailureTail { deferred, consumed };

    static void configureFailureCounter(GameplayModel& model,
        const std::uint16_t tick, const FailureTail tail) {
        configureLevel2RetryDeath(model);
        model.level2LoopTick_ = tick;
        model.level2HasAdvanced_ = true;
        model.level2TailConsumed_ = tail == FailureTail::consumed;
    }

    static void assertFailureCounter(const GameplayModel& model, const std::uint16_t expected) {
        assert(model.failureSourceLoopTick_ && *model.failureSourceLoopTick_ == expected);
    }

    static void configureUnsupportedFailureCounter(GameplayModel& model) {
        model.activeCampaignLevel_ = 2;
        model.phase_ = GameplayPhase::failurePreExplosionBlackout;
    }

    static void assertUnknownFailureCounter(const GameplayModel& model) {
        assert(!model.failureSourceLoopTick_);
    }

    static void configureLevel2RetryDeath(GameplayModel& model) {
        model.startLevel2();
        model.level2PlayerArmor_ = -1;
        model.level2Score_ = 1476;
        model.level2CRandState_ = 0x12345678;
        model.level2VisualRandomState_ = 0x87654321;
        model.level2Visibility_.invalidate();
        model.failureFromLevel2_ = true;
        model.phase_ = GameplayPhase::failurePreExplosionBlackout;
    }

    static void assertLevel2RetryStreams(const GameplayModel& model) {
        assert(model.level2CRandState_ == 0x12345678);
        assert(model.level2VisualRandomState_ == 0x87654321);
        assert(model.level2Visibility_.tracking() == VisibilityTracking::unknown);
        assert(!model.level2HasAdvanced_ && !model.level2TailConsumed_);
    }

    static void assertCampaignEntryPools(GameplayModel& model) {
        constexpr std::size_t initialOrdinaryCount = 5;
        for (std::size_t slot = 0; slot < model.level2ProjectilePoolSize_; ++slot) {
            const auto& projectile = model.level2ProjectilePool_[slot];
            assert(projectile.type == 0);
            assert(projectile.flags == 0);
            assert(projectile.collisionRadius == 0);
            assert((projectile.position == FixedPosition{}));
            assert(projectile.pitch == 0 && projectile.roll == 0 && projectile.yaw == 0);
            assert(projectile.pitchVelocity == 0 && projectile.rollVelocity == 0
                && projectile.yawVelocity == 0);
            assert(projectile.xVelocity == 0 && projectile.yVelocity == 0
                && projectile.zVelocity == 0 && projectile.forwardVelocity == 0);
            assert((projectile.stateBytes4dTo52
                == std::array<std::uint8_t, 6>{0xfd, 0, 0, 0, 0, 0}));
            assert(projectile.presenterDistance == 0 && projectile.targetHeading == 0);
            assert(projectile.weaponSlotCount == 0 && projectile.weaponCooldown == 0);
            assert((projectile.weaponSlotTimers == std::array<std::uint8_t, 3>{}));
            assert(projectile.behaviorTimer == 0 && !projectile.ownerIsPlayer);
            assert(projectile.ownerAllocationIndex == static_cast<std::size_t>(-1));
            assert(projectile.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback);
            assert(!model.level2ProjectilePoolActive_[slot]);
            assert(!model.level2ProjectilePoolWasActiveAtBoundaryStart_[slot]);
        }
        for (std::size_t slot = 0; slot < model.level2ActorPoolSize_; ++slot) {
            assert(model.level2ActorPoolActive_[slot] == (slot < initialOrdinaryCount));
            if (slot >= initialOrdinaryCount) {
                assert(model.level2ActorPool_[slot].stateBytes4dTo52[0] == 0xff);
                assert(!model.level2ActorPool_[slot].active());
            }
        }
        for (std::size_t slot = 0; slot < model.level2DeathEffectPoolSize_; ++slot) {
            assert(model.level2DeathEffectPool_[slot].flags == 0x88);
            assert(model.level2DeathEffectPool_[slot].stateBytes4dTo52[0] == 0xfc);
            assert(!model.level2DeathEffectPoolActive_[slot]);
        }
        for (std::size_t index = 0; index < model.level2Entities_.size(); ++index) {
            const auto expected = index < initialOrdinaryCount ? index : model.level2ActorPoolSize_;
            assert(model.level2EntityActorPoolSlots_[index] == expected);
            assert(model.level2EntityProjectilePoolSlots_[index] == model.level2ProjectilePoolSize_);
            assert(model.level2EntityDeathEffectPoolSlots_[index] == model.level2DeathEffectPoolSize_);
            assert(model.level2AllocationListOrder_[index] == index);
        }
        assert(model.level2ActiveEnemyCount_ == initialOrdinaryCount);
        assert(model.level2SpawnedEnemyCount_ == initialOrdinaryCount);
        const auto rngCalls = model.level2Rng_.calls();
        const auto allocated = allocateLevel2Actor(model, 8);
        assert(allocated == initialOrdinaryCount);
        assert(model.level2EntityActorPoolSlots_[allocated] == initialOrdinaryCount);
        assert(model.level2AllocationListOrder_.back() == allocated);
        assert(model.level2Rng_.calls() == rngCalls);
        for (std::size_t index = allocated + 1; index < model.level2Entities_.size(); ++index) {
            assert(model.level2Entities_[index].type == 30);
            assert(model.level2EntityActorPoolSlots_[index] == model.level2ActorPoolSize_);
        }
    }

    static void assertDiagnosticEntryPools(const GameplayModel& model) {
        const auto& source = model.level1_.projectiles();
        for (std::size_t slot = 0; slot < source.size(); ++slot) {
            const auto& actual = model.level2ProjectilePool_[slot];
            assert(actual.type == source[slot].type);
            assert(actual.flags == source[slot].flags);
            assert(actual.collisionRadius == source[slot].collisionRadius);
            assert(actual.position == source[slot].position);
            assert(actual.yaw == source[slot].yaw);
        }
        for (std::size_t index = 0; index < model.level2Entities_.size(); ++index) {
            assert(model.level2EntityActorPoolSlots_[index] == index);
        }
    }

    static void unsupportedContinue(GameplayModel& model) {
        model.failureFromLevel2_ = true;
    }
    static void configureIndicatorTransition(GameplayModel& model, const GameplayPhase phase,
        const std::int16_t x, const std::int16_t velocity) {
        model.phase_ = phase;
        model.successAudioOwned_ = true;
        model.successAudioActive_ = false;
        model.cockpitIndicatorX_ = x;
        model.cockpitIndicatorVelocity_ = velocity;
    }

    static void configureLevel1SuccessProbe(GameplayModel& model) {
        model.phase_ = GameplayPhase::successPresentation;
        model.presentationFrame_ = 0;
    }

    static void configureLevel2SuccessProbe(GameplayModel& model) {
        model.startLevel2();
        model.phase_ = GameplayPhase::level2SuccessPresentation;
        model.presentationFrame_ = 0;
    }
    static bool level2Complete(const GameplayModel& model) noexcept {
        return model.level2Complete_;
    }

    static void setLevel2PlayerPosition(
        GameplayModel& model,
        const stellar7::FixedPosition position
    ) {
        model.level2Player_.position = position;
    }

    static void setLevel2CockpitIndicatorX(
        GameplayModel& model,
        const std::int16_t x
    ) {
        model.cockpitIndicatorX_ = x;
    }

    static void setLevel2EntityPlanarVelocity(
        GameplayModel& model,
        const std::size_t index,
        const std::int16_t xVelocity,
        const std::int16_t yVelocity
    ) {
        assert(index < model.level2Entities_.size());
        model.level2Entities_[index].xVelocity = xVelocity;
        model.level2Entities_[index].yVelocity = yVelocity;
    }

    static void configureLevel2Type30Contact(GameplayModel& model) {
        model.startLevel2();
        model.phase_ = GameplayPhase::level2;
        model.level2HasAdvanced_ = true;
        model.level2PendingInput_ = {};
        model.level2Player_.position = {};
        model.level2Player_.pitch = 0;
        model.level2Player_.roll = 0;
        model.level2Player_.yaw = 0;
        model.level2Player_.pitchVelocity = 0;
        model.level2Player_.rollVelocity = 0;
        model.level2Player_.yawVelocity = 0;
        model.level2Player_.xVelocity = 0;
        model.level2Player_.yVelocity = 0;
        model.level2Player_.zVelocity = 0;
        model.level2Player_.forwardVelocity = 100;
        model.level2PlayerCollisionMomentum_ = 0;
        for (auto& entity : model.level2Entities_) {
            entity.position = {20000, -20000, 0};
        }
        auto target = std::find_if(
            model.level2Entities_.begin(),
            model.level2Entities_.end(),
            [](const Level1Entity& entity) {
                return entity.type == 30;
            }
        );
        assert(target != model.level2Entities_.end());
        target->position = {0, 850, 0};
    }

    static void configureLevel2Type42Contact(GameplayModel& model) {
        configureLevel2Type30Contact(model);
        auto target = std::find_if(
            model.level2Entities_.begin(),
            model.level2Entities_.end(),
            [](const Level1Entity& entity) {
                return entity.type == 30
                    && entity.position == FixedPosition{0, 850, 0};
            }
        );
        assert(target != model.level2Entities_.end());
        target->type = 42;
        target->stateBytes4dTo52 = {254, 1, 0, 0, 0, 1};
    }

    static void configureLevel2Type46Contact(GameplayModel& model) {
        configureLevel2Type30Contact(model);
        auto target = std::find_if(
            model.level2Entities_.begin(),
            model.level2Entities_.end(),
            [](const Level1Entity& entity) {
                return entity.type == 30
                    && entity.position == FixedPosition{0, 850, 0};
            }
        );
        assert(target != model.level2Entities_.end());
        target->type = 46;
        target->collisionRadius = 2260;
        target->stateBytes4dTo52 = {254, 100, 0, 0, 0, 0};
    }

    static void configureLevel2GuardianCycle(GameplayModel& model) {
        model.startLevel2();
        model.phase_ = GameplayPhase::level2;
        model.level2HasAdvanced_ = true;
        model.level2LoopTick_ = 1;
        model.level2PendingInput_ = {};
        const auto guardianIndex = std::size_t{5};
        auto& guardian = model.level2Entities_[guardianIndex];
        guardian.type = 44;
        guardian.collisionRadius = 1147;
        guardian.position = {0, -22972, 0};
        guardian.yaw = 0;
        guardian.yawVelocity = 0;
        guardian.forwardVelocity = 100;
        guardian.presenterDistance = 22972;
        guardian.targetHeading = 0;
        guardian.stateBytes4dTo52 = {255, 20, 2, 0, 5, 0};
        guardian.behaviorTimer = 0;
        guardian.weaponSlotCount = 0;
        guardian.weaponCooldown = 0;
        guardian.weaponSlotTimers = {};
        model.level2ActiveListOrder_ = {guardianIndex};
        model.level2AllocationListOrder_ = {guardianIndex};
        model.level2ObjectListOrder_ = {guardianIndex};
        model.level2PostWaveObjectIndex_ = guardianIndex;

        StellarRng probe;
        for (;;) {
            const auto before = probe;
            if (probe.next() < 300U) {
                model.level2Rng_ = before;
                break;
            }
        }
    }

    static void configureLevel2ZeroRadiusGuardianPresentation(
        GameplayModel& model
    ) {
        model.startLevel2();
        model.phase_ = GameplayPhase::level2;
        model.level2Player_.position = {};
        Level1Entity guardian{};
        guardian.allocationIndex = 0;
        guardian.type = 44;
        guardian.collisionRadius = 0;
        guardian.position = {0, 5000, 0};
        guardian.stateBytes4dTo52 = {255, 20, 0, 0, 5, 0};
        model.level2PresentationEntities_ = {guardian};
        model.level2PresentationActiveListOrder_ = {0};
    }

    static void configureLevel3LocalGuardianPresentation(
        GameplayModel& model
    ) {
        model.startLevel3();
        model.phase_ = GameplayPhase::level3;
        model.level2Player_.position = {};
        Level1Entity guardian{};
        guardian.allocationIndex = 0;
        guardian.type = 44;
        guardian.collisionRadius = 2365;
        guardian.position = {0, 5000, 0};
        guardian.stateBytes4dTo52 = {255, 20, 2, 1, 5, 1};
        model.level2Entities_ = {guardian};
        model.level2PresentationEntities_ = {guardian};
        model.level2ActiveListOrder_ = {0};
        model.level2PresentationActiveListOrder_ = {0};
    }

    static void configureLevel2DeliveryAnimation(GameplayModel& model, int type = 7) {
        configureLevel2ZeroRadiusGuardianPresentation(model);
        auto& delivery = model.level2PresentationEntities_[0];
        delivery.type = static_cast<std::int16_t>(type);
        delivery.collisionRadius = 13052;
        delivery.stateBytes4dTo52[2] = 1;
    }

    static void configureLevel2GuardianObjectPoolEmission(
        GameplayModel& model
    ) {
        model.startLevel2();
        model.phase_ = GameplayPhase::level2;
        model.level2HasAdvanced_ = true;
        model.level2LoopTick_ = 2343;
        model.level2PendingInput_ = {};
        model.level2Player_.position = {};
        model.level2Entities_.assign(5, Level1Entity{});

        auto& guardian = model.level2Entities_[0];
        guardian.allocationIndex = 0;
        guardian.type = 45;
        guardian.position = {-2305, -1180, 0};
        guardian.yaw = 21040;
        guardian.presenterDistance = 2747;
        guardian.targetHeading = 54080;
        guardian.stateBytes4dTo52 = {255, 10, 2, 0, 5, 1};
        guardian.behaviorTimer = 21;

        for (std::size_t index = 1; index <= 2; ++index) {
            auto& object = model.level2Entities_[index];
            object.allocationIndex = index;
            object.type = 30;
            object.collisionRadius = 868;
            object.position = {
                static_cast<std::int32_t>(20000 + index * 1000),
                20000,
                0,
            };
            object.stateBytes4dTo52 = {254, 100, 0, 0, 0, 0};
        }
        for (std::size_t index = 3; index <= 4; ++index) {
            auto& projectile = model.level2Entities_[index];
            projectile.allocationIndex = index;
            projectile.type = 23;
            projectile.collisionRadius = 134;
            projectile.position = {
                static_cast<std::int32_t>(-20000 - index * 1000),
                -20000,
                0,
            };
            projectile.stateBytes4dTo52 = {253, 40, 0, 0, 0, 0};
        }

        model.level2ActorPoolActive_.fill(false);
        model.level2ActorPool_[1] = guardian;
        model.level2ActorPoolActive_[1] = true;
        model.level2EntityActorPoolSlots_ = {
            1,
            model.level2ActorPoolSize_,
            model.level2ActorPoolSize_,
            model.level2ActorPoolSize_,
            model.level2ActorPoolSize_,
        };
        model.level2ProjectilePoolActive_.fill(false);
        model.level2ProjectilePool_[0] = model.level2Entities_[3];
        model.level2ProjectilePool_[1] = model.level2Entities_[4];
        model.level2ProjectilePoolActive_[0] = true;
        model.level2ProjectilePoolActive_[1] = true;
        model.level2EntityProjectilePoolSlots_ = {
            model.level2ProjectilePoolSize_,
            model.level2ProjectilePoolSize_,
            model.level2ProjectilePoolSize_,
            0,
            1,
        };
        model.level2EntityDeathEffectPoolSlots_.assign(
            5,
            model.level2DeathEffectPoolSize_
        );
        model.level2ActiveListOrder_ = {1, 4, 2, 3, 0};
        model.level2AllocationListOrder_ = {1, 2, 0, 3, 4};
        model.level2ObjectListOrder_ = model.level2AllocationListOrder_;
        const auto inactiveFixed = model.level2FixedObjectPoolSize_;
        model.level2EntityFixedPoolSlots_ = {inactiveFixed, 0, 1, inactiveFixed, inactiveFixed};
        model.level2FixedPoolEverUsed_.fill(false);
        model.level2FixedPoolEverUsed_[0] = true;
        model.level2FixedPoolEverUsed_[1] = true;
    }

    static void configureLevel2GuardianContactConsumption(
        GameplayModel& model
    ) {
        model.startLevel2();
        model.phase_ = GameplayPhase::level2;
        model.level2HasAdvanced_ = true;
        model.level2PendingInput_ = {};
        model.level2Player_.position = {20000, 20000, 0};
        model.level2Entities_.assign(1, Level1Entity{});

        auto& guardian = model.level2Entities_[0];
        guardian.allocationIndex = 0;
        guardian.type = 44;
        guardian.position = {};
        guardian.stateBytes4dTo52 = {255, 10, 2, 0, 5, 0};
        guardian.collisionContactAllocationIndex = 0;

        model.level2ActorPoolActive_.fill(false);
        model.level2ActorPool_[0] = guardian;
        model.level2ActorPoolActive_[0] = true;
        model.level2EntityActorPoolSlots_ = {0};
        model.level2EntityProjectilePoolSlots_ = {
            model.level2ProjectilePoolSize_,
        };
        model.level2EntityDeathEffectPoolSlots_ = {
            model.level2DeathEffectPoolSize_,
        };
        model.level2ActiveListOrder_ = {0};
        model.level2AllocationListOrder_ = {0};
        model.level2ObjectListOrder_ = {0};
        model.level2CompletionObjectIndex_ = static_cast<std::size_t>(-1);
    }

    static void configureLevel2HostileObstacleContact(GameplayModel& model) {
        configureLevel2GuardianContactConsumption(model);
        model.level2Player_.position = {};
        model.level2Player_.forwardVelocity = 0;
        auto& obstacle = model.level2Entities_[0];
        obstacle.type = 30;
        obstacle.position = {3990, 4970, 0};
        obstacle.collisionRadius = 868;
        obstacle.stateBytes4dTo52 = {254, 100, 0, 0, 0, 0};
        obstacle.collisionContactAllocationIndex = static_cast<std::size_t>(-1);
        model.level2ActorPoolActive_[0] = false;
        model.level2EntityActorPoolSlots_[0] = model.level2ActorPoolSize_;

        Level1Entity projectile{};
        projectile.allocationIndex = 1;
        projectile.type = 19;
        projectile.position = {4453, 4576, 0};
        projectile.collisionRadius = 441;
        projectile.stateBytes4dTo52 = {253, 44, 0, 0, 0, 0};
        projectile.ownerAllocationIndex = 2;
        model.level2Entities_.push_back(projectile);
        model.level2ProjectilePool_[0] = projectile;
        model.level2ProjectilePoolActive_.fill(false);
        model.level2ProjectilePoolActive_[0] = true;
        model.level2EntityActorPoolSlots_.push_back(model.level2ActorPoolSize_);
        model.level2EntityProjectilePoolSlots_.push_back(0);
        model.level2EntityDeathEffectPoolSlots_.push_back(
            model.level2DeathEffectPoolSize_
        );
        model.level2AllocationListOrder_ = {0, 1};
        model.level2ActiveListOrder_ = {0, 1};
        model.level2ObjectListOrder_ = {0, 1};
    }

    static bool level2FirstProjectileRetired(const GameplayModel& model) {
        return !model.level2ProjectilePoolActive_[0]
            && model.level2ProjectilePool_[0].stateBytes4dTo52[1] == 0;
    }

    static void configureLevel2SimultaneousTerminalEffects(GameplayModel& model) {
        configureLevel2GuardianContactConsumption(model);
        auto& obstacle = model.level2Entities_[0];
        obstacle.type = 30;
        obstacle.stateBytes4dTo52 = {254, 100, 0, 0, 0, 0};
        for (std::size_t slot = 0; slot < 2; ++slot) {
            Level1Entity effect{};
            effect.allocationIndex = slot + 1;
            effect.type = -1;
            effect.position = slot == 0
                ? FixedPosition{-3161, 6566, 0}
                : FixedPosition{-2539, 5262, 0};
            effect.stateBytes4dTo52 = slot == 0
                ? std::array<std::uint8_t, 6>{252, 11, 0, 0, 12, 0}
                : std::array<std::uint8_t, 6>{252, 19, 0, 0, 20, 0};
            model.level2Entities_.push_back(effect);
            model.level2DeathEffectPool_[slot] = effect;
            model.level2DeathEffectPoolActive_[slot] = true;
            model.level2EntityActorPoolSlots_.push_back(model.level2ActorPoolSize_);
            model.level2EntityProjectilePoolSlots_.push_back(model.level2ProjectilePoolSize_);
            model.level2EntityDeathEffectPoolSlots_.push_back(slot);
        }
        model.level2AllocationListOrder_ = {0, 1, 2};
        model.level2ActiveListOrder_ = {0, 1, 2};
        model.level2ObjectListOrder_ = {0, 1, 2};
    }

    static void configureLevel2ProjectileExpiryBeforeTerminalEffect(GameplayModel& model) {
        configureLevel2SimultaneousTerminalEffects(model);
        model.level2Entities_.resize(2);
        auto projectile = Level1Entity{};
        projectile.type = 19;
        projectile.position = {5000, 5000, 0};
        projectile.stateBytes4dTo52 = {253, 1, 0, 0, 0, 0};
        model.level2Entities_.insert(model.level2Entities_.begin() + 1, projectile);
        for (std::size_t index = 0; index < model.level2Entities_.size(); ++index) {
            model.level2Entities_[index].allocationIndex = index;
        }
        model.level2DeathEffectPoolActive_[1] = false;
        model.level2ProjectilePool_[0] = projectile;
        model.level2ProjectilePoolActive_[0] = true;
        model.level2EntityActorPoolSlots_ = {0, model.level2ActorPoolSize_, model.level2ActorPoolSize_};
        model.level2EntityProjectilePoolSlots_ = {model.level2ProjectilePoolSize_, 0, model.level2ProjectilePoolSize_};
        model.level2EntityDeathEffectPoolSlots_ = {model.level2DeathEffectPoolSize_, model.level2DeathEffectPoolSize_, 0};
        model.level2AllocationListOrder_ = {0, 2, 1};
        model.level2ActiveListOrder_ = {0, 2, 1};
        model.level2ObjectListOrder_ = {0, 2, 1};
    }

    static void configureLevel2MixedRetirements(GameplayModel& model) {
        configureLevel2SimultaneousTerminalEffects(model);
        auto& target = model.level2Entities_[0];
        target.type = 8;
        target.position = {0, 500, 0};
        target.collisionRadius = 860;
        target.stateBytes4dTo52 = {255, 1, 0, 0, 2, 0};
        model.level2ActorPool_[0] = target;
        auto expired = Level1Entity{};
        expired.type = 19;
        expired.position = {10000, 10000, 0};
        expired.stateBytes4dTo52 = {253, 1, 0, 0, 0, 0};
        auto shot = Level1Entity{};
        shot.type = 23;
        shot.position = {};
        shot.forwardVelocity = 500;
        shot.collisionRadius = 134;
        shot.ownerIsPlayer = true;
        shot.stateBytes4dTo52 = {253, 64, 0, 0, 0, 0};
        model.level2Entities_.insert(model.level2Entities_.begin() + 1, {expired, shot});
        for (std::size_t index = 0; index < model.level2Entities_.size(); ++index) {
            model.level2Entities_[index].allocationIndex = index;
        }
        model.level2ProjectilePool_[0] = expired;
        model.level2ProjectilePool_[1] = shot;
        model.level2ProjectilePoolActive_[0] = true;
        model.level2ProjectilePoolActive_[1] = true;
        const auto noActor = model.level2ActorPoolSize_;
        const auto noShot = model.level2ProjectilePoolSize_;
        const auto noEffect = model.level2DeathEffectPoolSize_;
        model.level2EntityActorPoolSlots_ = {0, noActor, noActor, noActor, noActor};
        model.level2EntityProjectilePoolSlots_ = {noShot, 0, 1, noShot, noShot};
        model.level2EntityDeathEffectPoolSlots_ = {noEffect, noEffect, noEffect, 0, 1};
        model.level2AllocationListOrder_ = {0, 3, 4, 1, 2};
        model.level2ActiveListOrder_ = model.level2AllocationListOrder_;
        model.level2ObjectListOrder_ = model.level2AllocationListOrder_;
        model.level2ActiveEnemyCount_ = 1;
        model.level2SpawnedEnemyCount_ = 11;
        model.level2LoopTick_ = 1;
    }

    static const std::vector<std::size_t>& level2RetiredEffectPositions(const GameplayModel& model) {
        return model.level2RetiredEffectSemanticActivePositions_;
    }

    static std::vector<std::size_t> level2RetiredEffectIdentities(
        const GameplayModel& model
    ) {
        std::vector<std::size_t> identities;
        for (const auto& effect : model.level2RetiredEffectSemanticEntities_) {
            identities.push_back(effect.allocationIndex);
        }
        return identities;
    }

    static void configureLevel2GuardianType48Replacement(
        GameplayModel& model
    ) {
        model.startLevel2();
        model.phase_ = GameplayPhase::level2;
        model.level2HasAdvanced_ = true;
        model.level2LoopTick_ = 1;
        model.level2PendingInput_ = {};
        model.level2Player_.position = {20000, 20000, 0};
        model.level2Entities_.clear();
        model.level2ActorPoolActive_.fill(false);
        model.level2ProjectilePoolActive_.fill(false);
        model.level2EntityActorPoolSlots_.clear();
        model.level2EntityProjectilePoolSlots_.clear();
        model.level2EntityDeathEffectPoolSlots_.clear();

        Level1Entity guardian{};
        guardian.allocationIndex = 0;
        guardian.type = 44;
        guardian.position = {5000, 5000, 0};
        guardian.stateBytes4dTo52 = {255, 20, 2, 0, 5, 0};
        guardian.collisionContactAllocationIndex =
            static_cast<std::size_t>(-1);
        model.level2Entities_.push_back(guardian);
        model.level2ActorPool_[1] = guardian;
        model.level2ActorPoolActive_[1] = true;
        model.level2EntityActorPoolSlots_.push_back(1);
        model.level2EntityProjectilePoolSlots_.push_back(
            model.level2ProjectilePoolSize_
        );
        model.level2EntityDeathEffectPoolSlots_.push_back(
            model.level2DeathEffectPoolSize_
        );

        Level1Entity target{};
        target.allocationIndex = 1;
        target.type = 48;
        target.collisionRadius = 559;
        target.position = {0, 500, 0};
        target.stateBytes4dTo52 = {255, 1, 0, 0, 4, 0};
        model.level2Entities_.push_back(target);
        model.level2ActorPool_[0] = target;
        model.level2ActorPoolActive_[0] = true;
        model.level2EntityActorPoolSlots_.push_back(0);
        model.level2EntityProjectilePoolSlots_.push_back(
            model.level2ProjectilePoolSize_
        );
        model.level2EntityDeathEffectPoolSlots_.push_back(
            model.level2DeathEffectPoolSize_
        );

        Level1Entity projectile{};
        projectile.allocationIndex = 2;
        projectile.type = 23;
        projectile.collisionRadius = 134;
        projectile.forwardVelocity = 500;
        projectile.stateBytes4dTo52 = {253, 64, 0, 0, 0, 0};
        model.level2Entities_.push_back(projectile);
        model.level2ProjectilePool_[0] = projectile;
        model.level2ProjectilePoolActive_[0] = true;
        model.level2EntityActorPoolSlots_.push_back(
            model.level2ActorPoolSize_
        );
        model.level2EntityProjectilePoolSlots_.push_back(0);
        model.level2EntityDeathEffectPoolSlots_.push_back(
            model.level2DeathEffectPoolSize_
        );

        model.level2AllocationListOrder_ = {2, 1, 0};
        model.level2ObjectListOrder_ = model.level2AllocationListOrder_;
        model.level2ActiveListOrder_ = model.level2AllocationListOrder_;
        model.level2CompletionObjectIndex_ = static_cast<std::size_t>(-1);
        model.level2ActiveEnemyCount_ = 2;
        model.level2SpawnedEnemyCount_ = 2;
        armLevel2GuardianType48Choice(model);
    }

    static void configureLevel2GuardianPlayerContact(GameplayModel& model) {
        configureLevel2GuardianContactConsumption(model);
        model.level2Player_.position = {};
        model.level2Player_.yaw = 0;
        model.level2Player_.forwardVelocity = 100;
        model.level2PlayerArmor_ = 32767;
        auto& guardian = model.level2Entities_[0];
        // The 90-unit player integration enters the 935-unit averaged
        // collision radius; one native translation backout returns it to
        // the origin.
        guardian.position = {0, 1000, 0};
        guardian.collisionRadius = 1147;
        model.level2ActorPool_[0] = guardian;
    }

    static std::size_t allocateLevel2FixedObject(
        GameplayModel& model,
        const std::int16_t type
    ) {
        Level1Entity fixed{};
        fixed.type = type;
        fixed.stateBytes4dTo52 = {254, 100, 0, 0, 0, 0};
        return model.allocateLevel2FixedObject(fixed);
    }

    static std::size_t allocateLevel2Actor(
        GameplayModel& model,
        const std::int16_t type
    ) {
        Level1Entity actor{};
        actor.type = type;
        actor.stateBytes4dTo52 = {255, 1, 0, 0, 4, 0};
        return model.allocateLevel2Actor(actor);
    }

    static void armLevel2GuardianType48Choice(GameplayModel& model) {
        StellarRng probe;
        for (;;) {
            const auto before = probe;
            if ((probe.next() & 0x003fU) == 0) {
                model.level2Rng_ = before;
                return;
            }
        }
    }

    static std::uint8_t level2LatestCRandByte(
        const GameplayModel& model
    ) {
        return static_cast<std::uint8_t>(
            (model.level2CRandState_ >> 16U) & 0x7fffU
        );
    }

    static std::uint8_t level2LatestVisualRandomByte(
        const GameplayModel& model
    ) {
        return static_cast<std::uint8_t>(
            (model.level2VisualRandomState_ >> 16U) & 0xffU
        );
    }

    static void configureLevel2Type47GroundRiding(GameplayModel& model) {
        configureLevel2GuardianContactConsumption(model);
        model.level2Player_.position = {};
        auto& entity = model.level2Entities_[0];
        entity.type = 47;
        entity.position = {-5000, -6000, 0};
        entity.forwardVelocity = 100;
        entity.targetHeading = 0;
        entity.stateBytes4dTo52 = {255, 1, 0, 0, 4, 0};
        entity.behaviorTimer = 0;
        entity.collisionContactAllocationIndex =
            static_cast<std::size_t>(-1);
        entity.collisionContactIsPlayer = false;
    }

    static void configureLevel2Type47PlayerImpact(GameplayModel& model) {
        configureLevel2Type47GroundRiding(model);
        auto& entity = model.level2Entities_[0];
        entity.position = {0, 300, 0};
        entity.xVelocity = 0;
        entity.yVelocity = -50;
        entity.forwardVelocity = 0;
        entity.collisionRadius = 559;
        model.level2ActiveEnemyCount_ = 1;
        model.level2PlayerArmor_ = 32767;
    }

    static void configureLevel2Type48PlayerImpact(GameplayModel& model) {
        configureLevel2Type47GroundRiding(model);
        auto& entity = model.level2Entities_[0];
        entity.type = 48;
        entity.position = {0, 400, 0};
        entity.xVelocity = 0;
        entity.yVelocity = 0;
        entity.forwardVelocity = 0;
        entity.collisionRadius = 559;
        entity.stateBytes4dTo52 = {255, 1, 0, 0, 4, 0};
        entity.behaviorTimer = 20;
        model.level2Player_.forwardVelocity = 100;
        model.level2PlayerArmor_ = 32767;
    }

    static void configureLevel2Type47MutualImpact(GameplayModel& model) {
        model.startLevel2();
        model.phase_ = GameplayPhase::level2;
        model.level2HasAdvanced_ = true;
        model.level2PendingInput_ = {};
        model.level2Player_.position = {20000, 20000, 0};
        model.level2Entities_.assign(2, Level1Entity{});
        for (std::size_t index = 0; index < 2; ++index) {
            auto& entity = model.level2Entities_[index];
            entity.allocationIndex = index;
            entity.type = 47;
            entity.collisionRadius = 559;
            entity.position = {1000, 1000, 0};
            entity.presenterDistance = static_cast<std::uint32_t>(
                6500 * (index + 1)
            );
            entity.stateBytes4dTo52 = {255, 1, 0, 0, 4, 0};
        }
        model.level2ActorPoolActive_.fill(false);
        for (std::size_t index = 0; index < 2; ++index) {
            model.level2ActorPool_[index] = model.level2Entities_[index];
            model.level2ActorPoolActive_[index] = true;
        }
        model.level2EntityActorPoolSlots_ = {0, 1};
        model.level2EntityProjectilePoolSlots_.assign(
            2,
            model.level2ProjectilePoolSize_
        );
        model.level2EntityDeathEffectPoolSlots_.assign(
            2,
            model.level2DeathEffectPoolSize_
        );
        model.level2ActiveListOrder_ = {0, 1};
        model.level2AllocationListOrder_ = {0, 1};
        model.level2ObjectListOrder_ = {0, 1};
        model.level2ActiveEnemyCount_ = 2;
    }

    static void advanceLevel2(GameplayModel& model) {
        model.advanceLevel2({});
    }

    static void configureLevel2SightProbe(
        GameplayModel& model,
        const std::int16_t type,
        const bool active,
        const std::uint16_t loopTick
    ) {
        model.startLevel2();
        model.phase_ = GameplayPhase::level2;
        model.level2LoopTick_ = loopTick;
        model.level2Player_.position = {};
        model.level2Player_.yaw = 0;
        model.level2Entities_.assign(1, Level1Entity{});
        model.level2Entities_[0].allocationIndex = 0;
        model.level2Entities_[0].type = type;
        model.level2Entities_[0].position = {0, 1000, 0};
        model.level2Entities_[0].stateBytes4dTo52 = {
            0xff,
            static_cast<std::uint8_t>(active ? 1 : 0),
            0,
            0,
            0,
            0,
        };
        model.level2ActiveListOrder_ = {0};
    }

    static void configureLevel2RadarCategoryProbe(
        GameplayModel& model, const bool terminalEffect = false
    ) {
        model.startLevel2();
        model.phase_ = GameplayPhase::level2;
        model.level2HasAdvanced_ = true;
        model.level2Player_.position = {8, -18, 0};
        model.level2Player_.yaw = 37642;
        model.level2Entities_.assign(2, Level1Entity{});
        auto& result = model.level2Entities_[0];
        result.allocationIndex = 0;
        result.type = 25;
        result.position = {2470, -4948, 0};
        result.stateBytes4dTo52 = {254, 100, 3, 0, 0, 1};
        auto& projectile = model.level2Entities_[1];
        projectile.allocationIndex = 1;
        projectile.type = 23;
        projectile.position = {646, -1287, 0};
        projectile.stateBytes4dTo52 = {253, 61, 0, 0, 0, 0};
        if (terminalEffect) {
            projectile.type = -1;
            projectile.stateBytes4dTo52 = {252, 0, 0, 0, 0, 0};
        }
        model.level2EntityActorPoolSlots_.assign(
            2,
            model.level2ActorPoolSize_
        );
        model.level2EntityProjectilePoolSlots_.assign(
            2,
            model.level2ProjectilePoolSize_
        );
        model.level2EntityDeathEffectPoolSlots_.assign(
            2,
            model.level2DeathEffectPoolSize_
        );
        model.level2ActiveListOrder_ = {0, 1};
        model.level2AllocationListOrder_ = {0, 1};
        model.level2ObjectListOrder_ = {0, 1};
    }

    static void setAlternateSightPhase(
        GameplayModel& model,
        const bool alternate
    ) {
        model.alternateSightPhase_ = alternate;
    }

    static void retireLevel2RadarProbe(GameplayModel& model) {
        model.level2PresentationEntities_ = model.level2Entities_;
        model.level2PresentationEntities_.push_back(model.level2Entities_[1]);
        model.level2PresentationActiveListOrder_ = {0, 2};
        model.level2Entities_[1] = Level1Entity{};
        model.level2ActiveListOrder_ = {0};
    }

    static void setLevel2Score(
        GameplayModel& model,
        const std::uint32_t score
    ) {
        model.level2Score_ = score;
    }

    static void refreshLevel2SightPhase(GameplayModel& model) {
        model.refreshLevel2SightPhase();
    }

    static std::int16_t level2CollisionMomentum(
        const GameplayModel& model
    ) {
        return model.level2PlayerCollisionMomentum_;
    }

    static std::int16_t level2ActiveEnemyCount(
        const GameplayModel& model
    ) {
        return model.level2ActiveEnemyCount_;
    }

    static std::int16_t level2SpawnedEnemyCount(
        const GameplayModel& model
    ) {
        return model.level2SpawnedEnemyCount_;
    }

    static void configureLevel3ForcedSpawnFrontier(
        GameplayModel& model
    ) {
        model.startLevel3();
        model.phase_ = GameplayPhase::level3;
        model.level2LoopTick_ = 32;
        model.level2HasAdvanced_ = true;
        model.level2PendingInput_ = {};
        // Native forced-frontier checkpoint loop 32, recovered directly from
        // the private DS RNG table.  The next scheduler word is 0xf06c,
        // selecting Type 12 under the Level 3 weight table.
        constexpr std::array<std::uint16_t, StellarRng::wordCount> table{{
            18039, 47163, 55298, 36204, 44268, 34850, 26692, 36252,
            5880, 11073, 1653, 46875, 39655, 11930, 52659, 4436,
            56393, 42362, 26754, 50307, 33035, 28297, 15419, 46303,
            34406, 48840, 40887, 784, 58029, 33093, 3253, 57010,
            63174, 29771, 22727, 28673, 9083, 60579, 2841, 14192,
            10395, 34207, 31937, 17811, 51747, 62910, 14727, 18800,
            5210, 33436, 3701, 23631, 2784, 62470, 15245, 24549,
        }};
        model.level2Rng_ = StellarRng(table, 46, 108, 24549, 1);
        model.level2ActiveEnemyCount_ = 0;
        model.level2SpawnedEnemyCount_ = 4;
        model.advanceLevel2({});
    }

    static void configureLevel3DenseSpawnFrontier(
        GameplayModel& model
    ) {
        model.startLevel3();
        model.phase_ = GameplayPhase::level3;
        model.level2LoopTick_ = 32;
        model.level2HasAdvanced_ = true;
        model.level2PendingInput_ = {};
        model.level2Player_.position = {-199, 26, 0};
        model.level2Player_.yaw = 14992;
        model.level2Player_.yawVelocity = 512;
        model.level2Player_.forwardVelocity = 200;
        model.level2Ground_ = Level1GroundState({{
            {5696, 4192},
            {8800, 1216},
            {4544, 1152},
            {-896, 1472},
            {-6656, 2720},
            {1408, 3552},
        }});

        model.level2Entities_.assign(6, Level1Entity{});
        const auto setEntity = [&model](
            const std::size_t index,
            const std::int16_t type,
            const std::uint32_t collisionRadius,
            const FixedPosition position,
            const std::uint16_t yaw,
            const std::int16_t yawVelocity,
            const std::array<std::uint8_t, 6> state,
            const std::uint32_t presenterDistance,
            const std::uint16_t targetHeading,
            const std::uint8_t weaponSlotCount
        ) {
            auto& entity = model.level2Entities_[index];
            entity.allocationIndex = index;
            entity.type = type;
            entity.collisionRadius = collisionRadius;
            entity.position = position;
            entity.yaw = yaw;
            entity.yawVelocity = yawVelocity;
            entity.stateBytes4dTo52 = state;
            entity.presenterDistance = presenterDistance;
            entity.targetHeading = targetHeading;
            entity.weaponSlotCount = weaponSlotCount;
        };
        setEntity(
            0, 10, 0xfffff000U, {-13340, -10827, 0}, 8192,
            -1792, {255, 1, 0, 0, 2, 0}, 17210, 56336, 3
        );
        setEntity(
            1, 10, 0xfffff000U, {-25628, -6731, 0}, 0,
            -2304, {255, 1, 0, 0, 2, 0}, 27962, 51856, 3
        );
        setEntity(
            2, 10, 0xfffff000U, {-17436, -23115, 0}, 0,
            2048, {255, 1, 0, 0, 2, 0}, 29604, 58864, 3
        );
        setEntity(
            3, 2, 0, {19284, 21941, 0}, 24785,
            271, {255, 2, 0, 0, 1, 3}, 29221, 25200, 1
        );
        setEntity(
            4, 24, 0, {-17436, -14923, 0}, 0,
            0, {252, 100, 2, 0, 0, 0}, 22842, 56592, 0
        );
        setEntity(
            5, 31, 0, {-17436, -14923, 0}, 0,
            0, {254, 100, 0, 0, 0, 0}, 22842, 56592, 0
        );
        model.level2ActiveListOrder_ = {2, 3, 1, 4, 5, 0};
        model.level2AllocationListOrder_ = {0, 1, 2, 3, 4, 5};
        model.level2ObjectListOrder_ = model.level2AllocationListOrder_;
        model.level2PresentationEntities_ = model.level2Entities_;
        model.level2PresentationActiveListOrder_ = model.level2ActiveListOrder_;
        model.level2EntityActorPoolSlots_ = {
            0, 1, 2, 3, model.level2ActorPoolSize_,
            model.level2ActorPoolSize_,
        };
        model.level2ActorPool_ = {};
        model.level2ActorPoolActive_.fill(false);
        for (std::size_t index = 0; index < 4; ++index) {
            model.level2ActorPool_[index] = model.level2Entities_[index];
            model.level2ActorPoolActive_[index] = true;
        }
        model.level2ActiveEnemyCount_ = 0;
        model.level2SpawnedEnemyCount_ = 4;
        model.level2CompletionObjectIndex_ =
            static_cast<std::size_t>(-1);
        model.level2PostWaveObjectIndex_ =
            static_cast<std::size_t>(-1);
        model.level2PickupIndex_ = static_cast<std::size_t>(-1);
        model.level2Complete_ = false;
        constexpr std::array<std::uint16_t, StellarRng::wordCount> table{{
            18039, 47163, 55298, 36204, 44268, 34850, 26692, 36252,
            5880, 11073, 1653, 46875, 39655, 11930, 52659, 4436,
            56393, 42362, 26754, 50307, 33035, 28297, 15419, 46303,
            34406, 48840, 40887, 784, 58029, 33093, 3253, 57010,
            63174, 29771, 22727, 28673, 9083, 60579, 2841, 14192,
            10395, 34207, 31937, 17811, 51747, 62910, 14727, 18800,
            5210, 33436, 3701, 23631, 2784, 62470, 15245, 24549,
        }};
        model.level2Rng_ = StellarRng(table, 46, 108, 24549, 1);
    }

    static void configureLevel3Type12Callback(
        GameplayModel& model,
        const std::uint8_t behavior,
        const std::int32_t z,
        const bool insideRange,
        const bool playerContact = false
    ) {
        model.startLevel3();
        model.phase_ = GameplayPhase::level3;
        model.level2HasAdvanced_ = true;
        model.level2LoopTick_ = 1;
        model.level2PendingInput_ = {};
        model.level2Player_ = {};
        model.level2Player_.allocationIndex = 6;
        model.level2Player_.type = 0;
        model.level2Player_.stateBytes4dTo52 = {0, 1, 0, 0, 0, 0};
        model.level2Player_.position = {
            0,
            insideRange ? 2000 : 70000,
            0,
        };

        Level1Entity entity{};
        entity.allocationIndex = 0;
        entity.type = 12;
        entity.position = {0, 1000, z};
        entity.collisionRadius = 0;
        entity.yaw = 0;
        entity.targetHeading = 0;
        entity.stateBytes4dTo52 = {255, 1, 0, 0, 3, behavior};
        entity.weaponSlotCount = 1;
        entity.collisionContactIsPlayer = playerContact;

        model.level2Entities_ = {entity};
        model.level2PresentationEntities_ = model.level2Entities_;
        model.level2ActiveListOrder_ = {0};
        model.level2AllocationListOrder_ = {0};
        model.level2ObjectListOrder_ = {0};
        model.level2ActorPool_ = {};
        model.level2ActorPoolActive_.fill(false);
        model.level2ActorPool_[0] = entity;
        model.level2ActorPoolActive_[0] = true;
        model.level2EntityActorPoolSlots_ = {0};
        model.level2ProjectilePool_ = {};
        model.level2ProjectilePoolActive_.fill(false);
        model.level2ProjectilePoolWasActiveAtBoundaryStart_.fill(false);
        model.level2EntityProjectilePoolSlots_ = {
            model.level2ProjectilePoolSize_,
        };
        model.level2DeathEffectPool_ = {};
        model.level2DeathEffectPoolActive_.fill(false);
        model.level2DeathEffectPoolPendingPresenterRetirement_.fill(false);
        model.level2EntityDeathEffectPoolSlots_ = {
            model.level2DeathEffectPoolSize_,
        };
        model.level2ActiveEnemyCount_ = 1;
        model.level2SpawnedEnemyCount_ = 1;
        model.level2CompletionObjectIndex_ = static_cast<std::size_t>(-1);
        model.level2PostWaveObjectIndex_ = static_cast<std::size_t>(-1);
        model.level2PickupIndex_ = static_cast<std::size_t>(-1);
        model.level2Complete_ = false;
    }

    static void configureLevel3Type12MissGate(
        GameplayModel& model,
        const std::uint8_t gate = 1
    ) {
        // The presenter refresh derives a large heading error from this
        // diagonal source/player pair before the callback runs.
        model.level2Entities_[0].position = {1000, 1000, 300};
        model.level2Entities_[0].type12MissGate = gate;
    }

    static void configureLevel3OrdinaryPlayerContactProbe(
        GameplayModel& model,
        const std::int16_t type,
        const std::uint32_t collisionRadius,
        const FixedPosition position
    ) {
        model.startLevel3();
        model.phase_ = GameplayPhase::level3;
        model.level2HasAdvanced_ = true;
        model.level2LoopTick_ = 1;
        model.level2PendingInput_ = {};
        model.level2Player_ = {};
        model.level2Player_.allocationIndex = 6;
        model.level2Player_.type = 0;
        model.level2Player_.collisionRadius = 0;
        model.level2Player_.position = {};
        model.level2Player_.forwardVelocity = 0;
        model.level2PlayerArmor_ = 680;
        model.level2AudioEvents_.clear();

        Level1Entity entity{};
        entity.allocationIndex = 0;
        entity.type = type;
        entity.collisionRadius = collisionRadius;
        entity.position = position;
        entity.stateBytes4dTo52 = type == 10
            ? std::array<std::uint8_t, 6>{255, 1, 0, 0, 2, 0}
            : std::array<std::uint8_t, 6>{255, 1, 0, 0, 0, 0};

        model.level2Entities_ = {entity};
        model.level2PresentationEntities_ = model.level2Entities_;
        model.level2ActorPool_ = {};
        model.level2ActorPoolActive_.fill(false);
        model.level2ActorPool_[0] = entity;
        model.level2ActorPoolActive_[0] = true;
        model.level2EntityActorPoolSlots_ = {0};
        model.level2ProjectilePool_ = {};
        model.level2ProjectilePoolActive_.fill(false);
        model.level2ProjectilePoolWasActiveAtBoundaryStart_.fill(false);
        model.level2EntityProjectilePoolSlots_ = {
            model.level2ProjectilePoolSize_,
        };
        model.level2DeathEffectPool_ = {};
        model.level2DeathEffectPoolActive_.fill(false);
        model.level2DeathEffectPoolPendingPresenterRetirement_.fill(false);
        model.level2EntityDeathEffectPoolSlots_ = {
            model.level2DeathEffectPoolSize_,
        };
        model.level2ActiveListOrder_ = {0};
        model.level2AllocationListOrder_ = {0};
        model.level2ObjectListOrder_ = {0};
        model.level2ActiveEnemyCount_ = 1;
        model.level2SpawnedEnemyCount_ = 1;
        model.level2CompletionObjectIndex_ = static_cast<std::size_t>(-1);
        model.level2PostWaveObjectIndex_ = static_cast<std::size_t>(-1);
        model.level2PickupIndex_ = static_cast<std::size_t>(-1);
        model.level2Complete_ = false;
    }

    static void configureLevel3GuardianEmission(
        GameplayModel& model
    ) {
        model.startLevel3();
        model.phase_ = GameplayPhase::level3;
        model.level2HasAdvanced_ = true;
        model.level2LoopTick_ = 1;
        model.level2PendingInput_ = {};
        model.level2Player_.position = {20000, 20000, 0};
        model.level2Entities_.assign(1, Level1Entity{});

        auto& guardian = model.level2Entities_[0];
        guardian.allocationIndex = 0;
        guardian.type = 44;
        guardian.collisionRadius = 1147;
        guardian.position = {0, 0, 0};
        guardian.yaw = 0;
        guardian.presenterDistance = 28284;
        guardian.targetHeading = 0;
        guardian.stateBytes4dTo52 = {255, 20, 2, 0, 5, 0};
        guardian.behaviorTimer = 0;
        guardian.weaponSlotCount = 0;
        guardian.collisionContactAllocationIndex =
            static_cast<std::size_t>(-1);

        model.level2ActorPool_ = {};
        model.level2ActorPoolActive_.fill(false);
        model.level2ActorPool_[0] = guardian;
        model.level2ActorPoolActive_[0] = true;
        model.level2EntityActorPoolSlots_ = {0};
        model.level2EntityProjectilePoolSlots_ = {
            model.level2ProjectilePoolSize_,
        };
        model.level2EntityDeathEffectPoolSlots_ = {
            model.level2DeathEffectPoolSize_,
        };
        model.level2ActiveListOrder_ = {0};
        model.level2AllocationListOrder_ = {0};
        model.level2ObjectListOrder_ = {0};
        model.level2CompletionObjectIndex_ =
            static_cast<std::size_t>(-1);
        model.level2PostWaveObjectIndex_ = 0;
        model.level2ActiveEnemyCount_ = 1;
        model.level2SpawnedEnemyCount_ = 12;

        StellarRng probe;
        for (;;) {
            const auto before = probe;
            if ((probe.next() & 0x003fU) == 0) {
                model.level2Rng_ = before;
                break;
            }
        }
    }

    static void configureLevel3Type44Burst(GameplayModel& model) {
        configureLevel3GuardianEmission(model);
        auto& source = model.level2Entities_[0];
        source.position.z = 500;
        source.stateBytes4dTo52[5] = 1;
        source.behaviorTimer = 13;
        model.level2PostWaveObjectIndex_ = static_cast<std::size_t>(-1);
        model.level2CompletionObjectIndex_ = static_cast<std::size_t>(-1);
    }

    static void suppressLevel3Type44Burst(GameplayModel& model) {
        model.level2CompletionObjectIndex_ = 0;
    }

    static void fillLevel3ActorPool(GameplayModel& model) {
        model.level2ActorPoolActive_.fill(true);
    }

    static void configureLevel3Type47MutualOverlap(GameplayModel& model) {
        configureLevel3GuardianEmission(model);
        model.level2Entities_.assign(2, Level1Entity{});
        model.level2ActorPool_ = {};
        model.level2ActorPoolActive_.fill(false);
        for (std::size_t index = 0; index < 2; ++index) {
            auto& entity = model.level2Entities_[index];
            entity.allocationIndex = index;
            entity.type = 47;
            entity.collisionRadius = 0;
            entity.position = {0, 0, 0};
            entity.stateBytes4dTo52 = {255, 1, 0, 0, 4, 0};
            entity.collisionContactAllocationIndex =
                static_cast<std::size_t>(-1);
            model.level2ActorPool_[index] = entity;
            model.level2ActorPoolActive_[index] = true;
        }
        model.level2EntityActorPoolSlots_ = {0, 1};
        model.level2EntityProjectilePoolSlots_ = {
            model.level2ProjectilePoolSize_,
            model.level2ProjectilePoolSize_,
        };
        model.level2EntityDeathEffectPoolSlots_ = {
            model.level2DeathEffectPoolSize_,
            model.level2DeathEffectPoolSize_,
        };
        model.level2ActiveListOrder_ = {0, 1};
        model.level2AllocationListOrder_ = {0, 1};
        model.level2ObjectListOrder_ = {0, 1};
        model.level2ActiveEnemyCount_ = 2;
        model.level2SpawnedEnemyCount_ = 2;
        model.level2PostWaveObjectIndex_ = static_cast<std::size_t>(-1);
    }

    static void armLevel3Type44LaunchChoice(GameplayModel& model) {
        StellarRng probe;
        for (;;) {
            const auto before = probe;
            if (probe.next() > 199U) {
                model.level2Rng_ = before;
                return;
            }
        }
    }

    static void configureLevel3Type44ContactGate(GameplayModel& model) {
        assert(!model.level2Entities_.empty());
        auto& guardian = model.level2Entities_[0];
        guardian.position.z = 2200;
        guardian.presenterDistance = 3000;
        guardian.collisionContactAllocationIndex = 0;
    }

    static void configureLevel3Type44FarDescentGate(GameplayModel& model) {
        configureLevel3GuardianEmission(model);
        auto& guardian = model.level2Entities_[0];
        model.level2Player_.forwardVelocity = 0;
        model.level2Player_.yawVelocity = 0;
        guardian.position = {0, 5000, 2200};
        guardian.targetHeading = presenterTargetHeading(
            guardian.position,
            model.level2Player_.position
        );
        guardian.yaw = guardian.targetHeading;
        guardian.stateBytes4dTo52[5] = 0;
        guardian.collisionContactAllocationIndex =
            static_cast<std::size_t>(-1);
        guardian.collisionContactIsPlayer = false;
        model.level2ActorPool_[0] = guardian;
        armLevel3Type44LaunchChoice(model);
    }

    static void configureLevel3Type44HalfTurnBoundary(GameplayModel& model) {
        configureLevel3GuardianEmission(model);
        auto& guardian = model.level2Entities_[0];
        model.level2Player_.forwardVelocity = 0;
        model.level2Player_.yawVelocity = 0;
        guardian.position = {-4799, -14412, 2100};
        guardian.presenterDistance = 16211;
        guardian.targetHeading = presenterTargetHeading(
            guardian.position,
            model.level2Player_.position
        );
        guardian.yaw = static_cast<std::uint16_t>(
            guardian.targetHeading - 0x8000U
        );
        guardian.stateBytes4dTo52[5] = 0;
        guardian.collisionContactAllocationIndex =
            static_cast<std::size_t>(-1);
        guardian.collisionContactIsPlayer = false;
        model.level2ActorPool_[0] = guardian;
        armLevel3Type44LaunchChoice(model);
    }

    static void configureLevel3Type44DescentBoundary(GameplayModel& model) {
        configureLevel3GuardianEmission(model);
        auto& guardian = model.level2Entities_[0];
        model.level2Player_.forwardVelocity = 0;
        model.level2Player_.yawVelocity = 0;
        guardian.position.z = 600;
        guardian.presenterDistance = 3000;
        guardian.yaw = presenterTargetHeading(
            guardian.position,
            model.level2Player_.position
        );
        guardian.targetHeading = guardian.yaw;
        guardian.stateBytes4dTo52[5] = 1;
        guardian.behaviorTimer = 0;
    }

    static void configureLevel3Type44HoldExpiry(GameplayModel& model) {
        configureLevel3GuardianEmission(model);
        auto& guardian = model.level2Entities_[0];
        model.level2Player_.forwardVelocity = 0;
        model.level2Player_.yawVelocity = 0;
        guardian.position.z = 500;
        guardian.yaw = presenterTargetHeading(
            guardian.position,
            model.level2Player_.position
        );
        guardian.targetHeading = guardian.yaw;
        guardian.stateBytes4dTo52[5] = 1;
        guardian.behaviorTimer = 1;
        // A live completion object suppresses the timer-10 Type-47 burst.
        model.level2CompletionObjectIndex_ = 0;
    }

    static void configureLevel3Type44State1ContactExit(GameplayModel& model) {
        configureLevel3Type44HoldExpiry(model);
        auto& guardian = model.level2Entities_[0];
        guardian.behaviorTimer = 10;
        guardian.collisionContactAllocationIndex = 0;
    }

    static void configureLevel3ProjectileHitAndGuardianContact(
        GameplayModel& model
    ) {
        configureLevel3Type44HoldExpiry(model);
        auto& guardian = model.level2Entities_[0];
        guardian.position = {0, 0, 500};
        guardian.collisionRadius = 2365;
        guardian.forwardVelocity = 0;
        guardian.behaviorTimer = 10;
        guardian.stateBytes4dTo52[1] = 2;
        guardian.collisionContactAllocationIndex =
            static_cast<std::size_t>(-1);
        model.level2ActorPool_[0] = guardian;

        Level1Entity contact{};
        contact.allocationIndex = 1;
        contact.type = 47;
        contact.position = guardian.position;
        contact.collisionRadius = 0;
        contact.stateBytes4dTo52 = {255, 1, 0, 0, 4, 0};
        model.level2Entities_.push_back(contact);
        model.level2ActorPool_[1] = contact;
        model.level2ActorPoolActive_[1] = true;
        model.level2EntityActorPoolSlots_.push_back(1);
        model.level2EntityProjectilePoolSlots_.push_back(
            model.level2ProjectilePoolSize_
        );
        model.level2EntityDeathEffectPoolSlots_.push_back(
            model.level2DeathEffectPoolSize_
        );

        Level1Entity projectile{};
        projectile.allocationIndex = 2;
        projectile.type = 23;
        projectile.position = guardian.position;
        projectile.collisionRadius = 134;
        projectile.stateBytes4dTo52 = {253, 64, 0, 0, 0, 0};
        projectile.ownerIsPlayer = true;
        model.level2Entities_.push_back(projectile);
        model.level2ProjectilePool_[0] = projectile;
        model.level2ProjectilePoolActive_[0] = true;
        model.level2EntityActorPoolSlots_.push_back(
            model.level2ActorPoolSize_
        );
        model.level2EntityProjectilePoolSlots_.push_back(0);
        model.level2EntityDeathEffectPoolSlots_.push_back(
            model.level2DeathEffectPoolSize_
        );
        model.level2ActiveListOrder_ = {0, 1, 2};
        model.level2AllocationListOrder_ = {0, 1, 2};
        model.level2ObjectListOrder_ = {0, 1, 2};
    }

    static void configureLevel3Type44DeathEffectOverlap(
        GameplayModel& model
    ) {
        configureLevel3Type44HoldExpiry(model);
        auto& guardian = model.level2Entities_[0];
        guardian.behaviorTimer = 12;

        Level1Entity effect{};
        effect.allocationIndex = 1;
        effect.flags = 0x8c;
        effect.type = -1;
        effect.collisionRadius = 0;
        effect.position = guardian.position;
        effect.stateBytes4dTo52 = {252, 4, 0, 0, 12, 0};
        model.level2Entities_.push_back(effect);
        model.level2EntityActorPoolSlots_.push_back(
            model.level2ActorPoolSize_
        );
        model.level2EntityProjectilePoolSlots_.push_back(
            model.level2ProjectilePoolSize_
        );
        model.level2EntityDeathEffectPoolSlots_.push_back(0);
        model.level2DeathEffectPool_[0] = effect;
        model.level2DeathEffectPoolActive_[0] = true;
        model.level2ActiveListOrder_.push_back(1);
        model.level2AllocationListOrder_.push_back(1);
        model.level2ObjectListOrder_.push_back(1);
    }

    static void configureLevel3DeathEffectBehindView(GameplayModel& model) {
        configureLevel3Type44DeathEffectOverlap(model);
        auto& effect = model.level2Entities_[1];
        effect.position = {
            model.level2Player_.position.x,
            model.level2Player_.position.y - 1000,
            model.level2Player_.position.z,
        };
        model.level2DeathEffectPool_[0] = effect;
    }

    static void configureLevel3TerminalEffectSort(GameplayModel& model) {
        configureLevel3Type44DeathEffectOverlap(model);
        auto& guardian = model.level2Entities_[0];
        guardian.position = {
            model.level2Player_.position.x,
            model.level2Player_.position.y + 3000,
            500,
        };
        auto& effect = model.level2Entities_[1];
        effect.stateBytes4dTo52[1] = 11;
        effect.stateBytes4dTo52[4] = 12;
        model.level2DeathEffectPool_[0] = effect;

        Level1Entity farActor{};
        farActor.allocationIndex = 2;
        farActor.type = 47;
        farActor.position = {0, 0, 0};
        farActor.stateBytes4dTo52 = {255, 1, 0, 0, 4, 0};
        model.level2Entities_.push_back(farActor);
        model.level2ActorPool_[1] = farActor;
        model.level2ActorPoolActive_[1] = true;
        model.level2EntityActorPoolSlots_.push_back(1);
        model.level2EntityProjectilePoolSlots_.push_back(
            model.level2ProjectilePoolSize_
        );
        model.level2EntityDeathEffectPoolSlots_.push_back(
            model.level2DeathEffectPoolSize_
        );
        model.level2ActiveListOrder_.push_back(2);
        model.level2AllocationListOrder_.push_back(2);
        model.level2ObjectListOrder_.push_back(2);
    }

    static void configureLevel3PostTerminalEffectTie(GameplayModel& model) {
        configureLevel3TerminalEffectSort(model);
        model.advanceLevel2({});
        assert(model.level2Entities_.size() == 2);
        model.level2Player_.forwardVelocity = 0;
        model.level2Player_.yawVelocity = 0;
        for (std::size_t index = 0; index < model.level2Entities_.size();
             ++index) {
            auto& entity = model.level2Entities_[index];
            entity.type = 0;
            entity.collisionRadius = std::numeric_limits<std::uint32_t>::max();
            entity.position = index == 0
                ? stellar7::FixedPosition{1000, 1000, 0}
                : stellar7::FixedPosition{5000, 5000, 0};
            entity.pitchVelocity = 0;
            entity.rollVelocity = 0;
            entity.yawVelocity = 0;
            entity.xVelocity = 0;
            entity.yVelocity = 0;
            entity.zVelocity = 0;
            entity.forwardVelocity = 0;
            entity.stateBytes4dTo52[2] = static_cast<std::uint8_t>(index + 1);
        }
    }

    static void configureLevel3Type44State2PlayerContact(
        GameplayModel& model
    ) {
        configureLevel3GuardianEmission(model);
        model.level2LoopTick_ = 1558;
        model.level2Player_.position = {17, -190, 0};
        model.level2Player_.yaw = 944;
        model.level2Player_.yawVelocity = -272;
        model.level2Player_.forwardVelocity = -190;
        model.level2PendingInput_ = {
            true, false, false, true, false, false,
        };
        auto& guardian = model.level2Entities_[0];
        guardian.collisionRadius = 2365;
        guardian.position = {-190, 1007, 1100};
        guardian.roll = 64671;
        guardian.yaw = 38928;
        guardian.yawVelocity = -144;
        guardian.forwardVelocity = 300;
        guardian.presenterDistance = 1274;
        guardian.targetHeading = 34528;
        guardian.stateBytes4dTo52 = {255, 14, 2, 1, 5, 2};
        guardian.behaviorTimer = 0;
        model.level2ActorPool_[0] = guardian;
    }

    static void configureLevel3Type44State1PlayerContactBackout(
        GameplayModel& model
    ) {
        configureLevel3GuardianEmission(model);
        model.level2Player_.position = {0, 0, 0};
        model.level2Player_.yawVelocity = 0;
        model.level2Player_.forwardVelocity = 0;
        model.level2PendingInput_ = {};
        auto& guardian = model.level2Entities_[0];
        guardian.collisionRadius = 2000;
        guardian.position = {0, 1900, 0};
        guardian.roll = 0;
        guardian.yaw = 0x8000;
        guardian.yawVelocity = 0;
        guardian.forwardVelocity = 1000;
        guardian.presenterDistance = 1900;
        guardian.targetHeading = 0x8000;
        guardian.stateBytes4dTo52 = {255, 20, 2, 1, 5, 1};
        guardian.behaviorTimer = 16;
        model.level2ActorPool_[0] = guardian;
    }

    static void configureLevel3Type44ReturnDistance(GameplayModel& model) {
        configureLevel3GuardianEmission(model);
        auto& guardian = model.level2Entities_[0];
        model.level2Player_.forwardVelocity = 0;
        model.level2Player_.yawVelocity = 0;
        guardian.position.z = 2100;
        guardian.presenterDistance = 17000;
        guardian.yaw = presenterTargetHeading(
            guardian.position,
            model.level2Player_.position
        );
        guardian.targetHeading = guardian.yaw;
        guardian.stateBytes4dTo52[5] = 3;
        guardian.behaviorTimer = 0;
    }

    static void configureLevel3Type44State2ContactClear(GameplayModel& model) {
        configureLevel3GuardianEmission(model);
        auto& guardian = model.level2Entities_[0];
        model.level2Player_.forwardVelocity = 0;
        model.level2Player_.yawVelocity = 0;
        guardian.position.z = 2000;
        guardian.stateBytes4dTo52[5] = 2;
        guardian.behaviorTimer = 0;
        guardian.collisionContactAllocationIndex = 0;
    }

    static void configureLevel3Type44MutualContactBackout(
        GameplayModel& model
    ) {
        configureLevel3GuardianEmission(model);
        model.level2LoopTick_ = 1193;
        model.level2Player_.position = {89, -81, 0};
        model.level2Player_.yaw = 8752;
        model.level2Player_.yawVelocity = 512;
        model.level2Player_.forwardVelocity = -120;
        model.level2PendingInput_ = {
            false, false, true, false, false, false,
        };

        Level1Entity descending{};
        descending.allocationIndex = 0;
        descending.type = 44;
        descending.collisionRadius = 2365;
        descending.position = {-2094, 1705, 1000};
        descending.roll = 62560;
        descending.yaw = 42048;
        descending.yawVelocity = -32;
        descending.forwardVelocity = 300;
        descending.presenterDistance = 2852;
        descending.targetHeading = 42016;
        descending.stateBytes4dTo52 = {255, 20, 2, 1, 5, 1};

        Level1Entity returning{};
        returning.allocationIndex = 1;
        returning.type = 44;
        returning.collisionRadius = 2365;
        returning.position = {557, -691, 2100};
        returning.roll = 82;
        returning.yaw = 5200;
        returning.yawVelocity = 16;
        returning.forwardVelocity = 300;
        returning.presenterDistance = 785;
        returning.targetHeading = 6800;
        returning.stateBytes4dTo52 = {255, 20, 2, 1, 5, 2};

        model.level2Entities_ = {descending, returning};
        model.level2ActorPool_ = {};
        model.level2ActorPoolActive_.fill(false);
        model.level2ActorPool_[0] = descending;
        model.level2ActorPool_[1] = returning;
        model.level2ActorPoolActive_[0] = true;
        model.level2ActorPoolActive_[1] = true;
        model.level2EntityActorPoolSlots_ = {0, 1};
        model.level2EntityProjectilePoolSlots_ = {
            model.level2ProjectilePoolSize_,
            model.level2ProjectilePoolSize_,
        };
        model.level2EntityDeathEffectPoolSlots_ = {
            model.level2DeathEffectPoolSize_,
            model.level2DeathEffectPoolSize_,
        };
        model.level2ActiveListOrder_ = {0, 1};
        model.level2AllocationListOrder_ = {0, 1};
        model.level2ObjectListOrder_ = {0, 1};
        model.level2PostWaveObjectIndex_ = static_cast<std::size_t>(-1);
        model.level2CompletionObjectIndex_ = static_cast<std::size_t>(-1);
        model.level2ActiveEnemyCount_ = 2;
        model.level2SpawnedEnemyCount_ = 12;
    }

    static void configureLevel3Completion(
        GameplayModel& model
    ) {
        configureLevel3Type12Callback(model, 0, 1000, true);
        model.level2Complete_ = true;
    }

    static void configureLevel3CompletionDeployment(
        GameplayModel& model
    ) {
        model.startLevel3();
        model.phase_ = GameplayPhase::level3;
        model.level2HasAdvanced_ = false;
        model.level2LoopTick_ = 0;
        model.level2PendingInput_ = {};
        model.level2Entities_.clear();
        model.level2ActiveListOrder_.clear();
        model.level2AllocationListOrder_.clear();
        model.level2ObjectListOrder_.clear();
        model.level2PresentationEntities_.clear();
        model.level2PresentationActiveListOrder_.clear();
        model.level2ActorPool_ = {};
        model.level2ActorPoolActive_.fill(false);
        model.level2ProjectilePool_ = {};
        model.level2ProjectilePoolActive_.fill(false);
        model.level2ProjectilePoolWasActiveAtBoundaryStart_.fill(false);
        model.level2DeathEffectPool_ = {};
        model.level2DeathEffectPoolActive_.fill(false);
        model.level2DeathEffectPoolPendingPresenterRetirement_.fill(false);
        model.level2EntityActorPoolSlots_.clear();
        model.level2EntityProjectilePoolSlots_.clear();
        model.level2EntityDeathEffectPoolSlots_.clear();
        model.level2ActiveEnemyCount_ = 0;
        model.level2SpawnedEnemyCount_ = 12;
        model.level2CompletionObjectIndex_ = static_cast<std::size_t>(-1);
        model.level2PostWaveObjectIndex_ = static_cast<std::size_t>(-1);
        model.level2PickupIndex_ = static_cast<std::size_t>(-1);
        model.level2Complete_ = false;
    }

    static void configureLevel3PostWaveDeployment(
        GameplayModel& model
    ) {
        model.startLevel3();
        model.phase_ = GameplayPhase::level3;
        model.level2HasAdvanced_ = true;
        model.level2LoopTick_ = 1;
        model.level2PendingInput_ = {};
        model.level2Entities_.assign(1, Level1Entity{});

        auto& completion = model.level2Entities_[0];
        completion.allocationIndex = 0;
        completion.type = 7;
        completion.position = {1200, -900, 0};
        completion.yaw = 0;
        completion.targetHeading = 0;
        completion.presenterDistance = 1200;
        completion.stateBytes4dTo52 = {255, 100, 0, 0, 5, 3};
        completion.behaviorTimer = 30;
        completion.forwardVelocity = 400;

        model.level2ActorPool_ = {};
        model.level2ActorPoolActive_.fill(false);
        model.level2ActorPool_[0] = completion;
        model.level2ActorPoolActive_[0] = true;
        model.level2EntityActorPoolSlots_ = {0};
        model.level2ProjectilePool_ = {};
        model.level2ProjectilePoolActive_.fill(false);
        model.level2EntityProjectilePoolSlots_ = {
            model.level2ProjectilePoolSize_,
        };
        model.level2DeathEffectPool_ = {};
        model.level2DeathEffectPoolActive_.fill(false);
        model.level2EntityDeathEffectPoolSlots_ = {
            model.level2DeathEffectPoolSize_,
        };
        model.level2ActiveListOrder_ = {0};
        model.level2AllocationListOrder_ = {0};
        model.level2ObjectListOrder_ = {0};
        model.level2CompletionObjectIndex_ =
            static_cast<std::size_t>(-1);
        model.level2PostWaveObjectIndex_ =
            static_cast<std::size_t>(-1);
        model.level2ActiveEnemyCount_ = 1;
        model.level2SpawnedEnemyCount_ = 12;
        model.level2Complete_ = false;
    }

    static void configureLevel3Type7Tail(GameplayModel& model) {
        configureLevel3PostWaveDeployment(model);
        auto& completion = model.level2Entities_[0];
        completion.stateBytes4dTo52[5] = 1;
        completion.behaviorTimer = 0x50;
        completion.forwardVelocity = 400;
        model.level2CompletionObjectIndex_ = 0;
    }

    static void configureLevel2PostWaveDeployment(GameplayModel& model) {
        configureLevel3PostWaveDeployment(model);
        model.activeCampaignLevel_ = 1;
        model.phase_ = GameplayPhase::level2;
    }

    static void configureLevel3Type7HoldExpiryWithPostWave(
        GameplayModel& model
    ) {
        configureLevel3Type7Tail(model);
        auto& completion = model.level2Entities_[0];
        completion.stateBytes4dTo52[5] = 2;
        completion.behaviorTimer = 1;
        // Native loop 850 still has a live Type 44 when this hold expires.
        model.level2PostWaveObjectIndex_ = 0;
    }

    static void configureLevel2Type7HoldExpiry(
        GameplayModel& model, const bool visible
    ) {
        configureLevel3Type7HoldExpiryWithPostWave(model);
        model.activeCampaignLevel_ = 1;
        model.phase_ = GameplayPhase::level2;
        model.level2Player_ = {};
        model.level2Entities_[0].position = {0, visible ? 10000 : -10000, 0};
        model.level2Entities_[0].flags = 0;
    }

    static void configureLevel3Type7VisibleWarp(GameplayModel& model) {
        configureLevel3PostWaveDeployment(model);
        model.level2Player_ = {};
        auto& completion = model.level2Entities_[0];
        completion.position = {0, 10000, 0};
        completion.flags = 0x0c;
        completion.stateBytes4dTo52[5] = 3;
        completion.behaviorTimer = 0x46;
        model.level2CompletionObjectIndex_ = 0;
    }

    static void assertGroundBeforeCompletionBeam() {
        GameplayModel model;
        configureLevel2Type7HoldExpiry(model, true);
        std::array<GroundPoint, Level1GroundState::pointCount> points{};
        points.fill({-30000, -30000});
        model.level2Ground_ = Level1GroundState(points);
        auto expectedGround = model.level2Ground_;
        auto expectedRng = model.level2Rng_;
        expectedGround.advance(0, 0, expectedRng);
        assert(expectedRng.calls() > model.level2Rng_.calls());
        advanceLevel2(model);
        assert(model.level2Ground_.points() == expectedGround.points());
        const auto* beam = model.level2CompletionBeam(0);
        assert(beam && beam->projected);
        for (const auto color : beam->colors) {
            assert(color == static_cast<std::uint8_t>((expectedRng.next() & 15U) + 128U));
        }
        assert(model.level2Rng_.calls() == expectedRng.calls());
        auto actualRng = model.level2Rng_;
        for (std::size_t word = 0; word < StellarRng::wordCount; ++word) {
            assert(actualRng.next() == expectedRng.next());
        }
        const auto calls = model.level2Rng_.calls();
        const auto first = completionBeamPixels(*beam, initialLevel1MainView().viewport);
        const auto second = completionBeamPixels(*beam, initialLevel1MainView().viewport);
        assert(!first.empty() && first.size() == second.size());
        assert(model.level2Rng_.calls() == calls);
        for (std::size_t index = 0; index < first.size(); ++index) {
            assert(first[index].point == second[index].point && first[index].color == second[index].color);
        }
    }

    static void assertCompletionBeamGuardsAndClipping() {
        class UnsupportedRenderer final : public Level2FrameRenderer {
        public:
            bool invoked = false;
            void render(Level2RenderState) override { invoked = true; }
        } renderer;
        GameplayModel unsupported;
        configureLevel3Type7VisibleWarp(unsupported);
        const auto rendererCalls = unsupported.level2Rng_.calls();
        bool rejected = false;
        try { unsupported.presentLevel2({}, &renderer, false); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected && !renderer.invoked && unsupported.level2Rng_.calls() == rendererCalls);

        Level1Entity actor{};
        actor.type = 7;
        actor.flags = 8;
        actor.stateBytes4dTo52[5] = 3;
        actor.behaviorTimer = 70;
        actor.position = {0, 10000, 0};
        auto view = initialLevel1MainView();
        StellarRng rng;
        const auto initialCalls = rng.calls();
        assert(!presentCompletionBeam(actor, view, rng, CompletionBeamView::radar).projected);
        assert(rng.calls() == initialCalls);
        actor.position.y = -10000;
        assert(!presentCompletionBeam(actor, view, rng, CompletionBeamView::main).projected);
        assert(rng.calls() == initialCalls);
        actor.position.y = 127;
        assert(!presentCompletionBeam(actor, view, rng, CompletionBeamView::main).projected);
        assert(rng.calls() == initialCalls);
        actor.position = {0, 10000, 0};
        auto beam = presentCompletionBeam(actor, view, rng, CompletionBeamView::main);
        assert(rng.calls() == initialCalls + beam.colors.size());
        beam.anchor = {view.viewport.left, view.viewport.bottom};
        const auto pixels = completionBeamPixels(beam, view.viewport);
        assert(pixels.size() == 1 && pixels[0].point == beam.anchor && pixels[0].color == beam.colors[5]);
        beam.anchor.x = view.viewport.left - 10;
        assert(completionBeamPixels(beam, view.viewport).empty());
        assert(rng.calls() == initialCalls + beam.colors.size());
        beam.anchor = {159, 140};
        beam.timer = 30;
        const auto clipped = completionBeamPixels(beam, view.viewport);
        assert(!clipped.empty());
        for (const auto& pixel : clipped) {
            assert(pixel.point.y >= 140 && pixel.point.y <= view.viewport.bottom);
        }
    }

    static void configureLevel3Type7OffscreenWarp(GameplayModel& model) {
        configureLevel3Type7VisibleWarp(model);
        model.level2Entities_[0].position = {0, -10000, 0};
        model.level2Entities_[0].flags = 0x08;
    }

    static void configureLevel3Type7BankCentering(GameplayModel& model) {
        configureLevel3PostWaveDeployment(model);
        auto& completion = model.level2Entities_[0];
        // Heading zero advances along +Y, preserving a zero heading error
        // while the craft remains below the player on that axis.
        completion.position = {0, -10000, 5000};
        completion.roll = 65495;
        completion.yaw = 0;
        completion.yawVelocity = 0;
        completion.targetHeading = 0;
        completion.presenterDistance = 10000;
        completion.stateBytes4dTo52[5] = 0;
        model.level2ActorPool_[0] = completion;
        model.level2CompletionObjectIndex_ = 0;
    }

    static void configureLevel3Type7Arrival(GameplayModel& model) {
        configureLevel3Type7BankCentering(model);
        auto& completion = model.level2Entities_[0];
        completion.position = {0, -1200, 5000};
        completion.roll = 0;
        completion.presenterDistance = 1200;
        model.level2ActorPool_[0] = completion;
    }

    static void configureLevel3Type7ClimbBank(GameplayModel& model) {
        configureLevel3PostWaveDeployment(model);
        auto& completion = model.level2Entities_[0];
        completion.position.z = 5000;
        completion.roll = 7;
        completion.stateBytes4dTo52[5] = 4;
        completion.forwardVelocity = 0;
        completion.yawVelocity = 0;
        model.level2ActorPool_[0] = completion;
        model.level2CompletionObjectIndex_ = 0;
    }

    static void configureLevel3GuardianLethalProjectile(
        GameplayModel& model,
        const std::uint8_t guardianHealth = 1
    ) {
        model.startLevel3();
        model.phase_ = GameplayPhase::level3;
        model.level2HasAdvanced_ = true;
        model.level2LoopTick_ = 1;
        model.level2PendingInput_ = {};
        model.level2Player_.position = {};
        model.level2Player_.forwardVelocity = 0;
        model.level2PlayerArmor_ = 680;
        model.level2Entities_.assign(2, Level1Entity{});

        auto& guardian = model.level2Entities_[0];
        guardian.allocationIndex = 0;
        guardian.type = 44;
        guardian.collisionRadius = 1147;
        guardian.position = {0, 2000, 0};
        guardian.stateBytes4dTo52 = {
            255,
            guardianHealth,
            2,
            0,
            5,
            0,
        };
        guardian.collisionContactAllocationIndex =
            static_cast<std::size_t>(-1);

        auto& projectile = model.level2Entities_[1];
        projectile.allocationIndex = 1;
        projectile.type = 23;
        projectile.collisionRadius = 134;
        projectile.position = guardian.position;
        projectile.forwardVelocity = 0;
        projectile.stateBytes4dTo52 = {253, 64, 0, 0, 0, 0};
        projectile.ownerIsPlayer = true;
        projectile.ownerAllocationIndex = static_cast<std::size_t>(-1);

        model.level2ActorPool_ = {};
        model.level2ActorPoolActive_.fill(false);
        model.level2ActorPool_[0] = guardian;
        model.level2ActorPoolActive_[0] = true;
        model.level2ProjectilePool_ = {};
        model.level2ProjectilePoolActive_.fill(false);
        model.level2ProjectilePool_[0] = projectile;
        model.level2ProjectilePoolActive_[0] = true;
        model.level2EntityActorPoolSlots_ = {
            0,
            model.level2ActorPoolSize_,
        };
        model.level2EntityProjectilePoolSlots_ = {
            model.level2ProjectilePoolSize_,
            0,
        };
        model.level2EntityDeathEffectPoolSlots_ = {
            model.level2DeathEffectPoolSize_,
            model.level2DeathEffectPoolSize_,
        };
        model.level2ActiveListOrder_ = {1, 0};
        model.level2AllocationListOrder_ = {1, 0};
        model.level2ObjectListOrder_ = model.level2AllocationListOrder_;
        model.level2CompletionObjectIndex_ =
            static_cast<std::size_t>(-1);
        model.level2PostWaveObjectIndex_ = 0;
        model.level2PickupIndex_ = static_cast<std::size_t>(-1);
        model.level2ActiveEnemyCount_ = 1;
        model.level2SpawnedEnemyCount_ = 12;
        model.level2Complete_ = false;
    }

    static void assertFreshLevel2PickupAdmission() {
        for (const std::uint8_t timer : {22, 21}) {
            auto model = std::make_unique<GameplayModel>();
            configureLevel3GuardianLethalProjectile(*model);
            model->phase_ = GameplayPhase::level2;
            model->activeCampaignLevel_ = 1;
            auto& guardian = model->level2Entities_[0];
            guardian.type = 45;
            guardian.stateBytes4dTo52[5] = 1;
            guardian.behaviorTimer = timer;
            model->level2ActorPool_[0] = guardian;
            model->level2EntityFixedPoolSlots_.assign(2, model->level2FixedObjectPoolSize_);
            model->level2FixedPoolEverUsed_.fill(false);
            model->tick({});
            const auto pickup = std::find_if(model->level2Entities_.begin(), model->level2Entities_.end(),
                [](const auto& actor) { return actor.type == 25; });
            assert(pickup != model->level2Entities_.end());
            assert(pickup->renderProfile == (timer == 21 ? WorldActorRenderProfile::unknown
                : WorldActorRenderProfile::inlineSelectorNoCallback));
        }
    }

    static void configureLevel3GuardianLethalWithSurvivingProjectiles(
        GameplayModel& model
    ) {
        configureLevel3GuardianLethalProjectile(model);
        for (std::size_t slot = 1; slot <= 2; ++slot) {
            Level1Entity survivor{};
            survivor.allocationIndex = model.level2Entities_.size();
            survivor.type = 23;
            survivor.collisionRadius = 134;
            survivor.position = {
                static_cast<std::int32_t>(10000 + slot * 1000),
                -10000,
                0,
            };
            survivor.stateBytes4dTo52 = {253, 64, 0, 0, 0, 0};
            survivor.ownerIsPlayer = true;
            survivor.ownerAllocationIndex = static_cast<std::size_t>(-1);
            const auto index = model.level2Entities_.size();
            model.level2Entities_.push_back(survivor);
            model.level2ProjectilePool_[slot] = survivor;
            model.level2ProjectilePoolActive_[slot] = true;
            model.level2EntityActorPoolSlots_.push_back(
                model.level2ActorPoolSize_
            );
            model.level2EntityProjectilePoolSlots_.push_back(slot);
            model.level2EntityDeathEffectPoolSlots_.push_back(
                model.level2DeathEffectPoolSize_
            );
            model.level2AllocationListOrder_.push_back(index);
            model.level2ActiveListOrder_.push_back(index);
        }
        // The allocation list is independent of the callback order. The
        // impact projectile runs before the guardian in the latter.
        model.level2AllocationListOrder_ = {0, 1, 2, 3};
        model.level2ObjectListOrder_ = model.level2AllocationListOrder_;
        model.level2ActiveListOrder_ = {1, 0, 2, 3};
    }

    static void configureLevel3ProjectileBeforeMovingGuardianMiss(
        GameplayModel& model
    ) {
        configureLevel3GuardianLethalProjectile(model, 20);

        auto& guardian = model.level2Entities_[0];
        guardian.position = {6152, 17138, 1100};
        guardian.collisionRadius = 2365;
        guardian.yaw = 1088;
        guardian.yawVelocity = 512;
        guardian.forwardVelocity = 300;
        guardian.stateBytes4dTo52 = {255, 20, 2, 1, 5, 1};
        model.level2ActorPool_[0] = guardian;

        auto& projectile = model.level2Entities_[1];
        projectile.position = {5386, 17987, 0};
        projectile.yaw = 62608;
        projectile.forwardVelocity = 500;
        model.level2ProjectilePool_[0] = projectile;

        // Native Level 3 loop 1061->1062 processes this projectile before
        // the guardian. The shot misses the guardian's old center but would
        // overlap its already-advanced center if portable passes used it.
        model.level2ActiveListOrder_ = {1, 0};
        model.level2AllocationListOrder_ = {0, 1};
        model.level2ObjectListOrder_ = model.level2AllocationListOrder_;
    }

    static void configureLevel3ProjectileRebuildCollisionOrder(
        GameplayModel& model
    ) {
        model.startLevel3();
        model.phase_ = GameplayPhase::level3;
        model.level2HasAdvanced_ = true;
        model.level2LoopTick_ = 2124;
        model.level2PendingInput_ = {};
        model.level2Player_.position = {};
        model.level2Player_.forwardVelocity = 0;
        model.level2Entities_.assign(3, Level1Entity{});

        auto& struck = model.level2Entities_[0];
        struck.allocationIndex = 0;
        struck.type = 44;
        struck.collisionRadius = 2365;
        struck.position = {4861, -1746, 500};
        struck.yaw = 12832;
        struck.yawVelocity = -48;
        struck.forwardVelocity = 300;
        struck.targetHeading = 12752;
        struck.stateBytes4dTo52 = {255, 20, 2, 1, 5, 2};
        struck.behaviorTimer = 11;
        struck.collisionContactAllocationIndex = 1;

        auto& returning = model.level2Entities_[1];
        returning.allocationIndex = 1;
        returning.type = 44;
        returning.collisionRadius = 2365;
        returning.position = {3799, -4297, 2100};
        returning.yaw = 63520;
        returning.yawVelocity = -200;
        returning.forwardVelocity = 300;
        returning.targetHeading = 7664;
        returning.stateBytes4dTo52 = {255, 19, 2, 1, 5, 3};
        returning.collisionContactAllocationIndex = 0;

        auto& projectile = model.level2Entities_[2];
        projectile.allocationIndex = 2;
        projectile.type = 23;
        projectile.collisionRadius = 134;
        projectile.position = {4578, -1645, 500};
        projectile.stateBytes4dTo52 = {253, 40, 1, 0, 0, 0};
        projectile.ownerIsPlayer = true;
        projectile.ownerAllocationIndex = static_cast<std::size_t>(-1);

        model.level2ActorPool_ = {};
        model.level2ActorPoolActive_.fill(false);
        model.level2ActorPool_[0] = struck;
        model.level2ActorPool_[1] = returning;
        model.level2ActorPoolActive_[0] = true;
        model.level2ActorPoolActive_[1] = true;
        model.level2ProjectilePool_ = {};
        model.level2ProjectilePoolActive_.fill(false);
        model.level2ProjectilePool_[0] = projectile;
        model.level2ProjectilePoolActive_[0] = true;
        model.level2DeathEffectPool_ = {};
        model.level2DeathEffectPoolActive_.fill(false);
        model.level2DeathEffectPoolPendingPresenterRetirement_.fill(false);
        model.level2EntityActorPoolSlots_ = {
            0,
            1,
            model.level2ActorPoolSize_,
        };
        model.level2EntityProjectilePoolSlots_ = {
            model.level2ProjectilePoolSize_,
            model.level2ProjectilePoolSize_,
            0,
        };
        model.level2EntityDeathEffectPoolSlots_ = {
            model.level2DeathEffectPoolSize_,
            model.level2DeathEffectPoolSize_,
            model.level2DeathEffectPoolSize_,
        };
        model.level2ActiveListOrder_ = {1, 0, 2};
        model.level2AllocationListOrder_ = {0, 1, 2};
        model.level2ObjectListOrder_ = model.level2AllocationListOrder_;
        model.level2CompletionObjectIndex_ =
            static_cast<std::size_t>(-1);
        model.level2PostWaveObjectIndex_ =
            static_cast<std::size_t>(-1);
        model.level2PickupIndex_ = static_cast<std::size_t>(-1);
        model.level2ActiveEnemyCount_ = 2;
        model.level2SpawnedEnemyCount_ = 12;
        model.level2Complete_ = false;
    }

    static void configureLevel3RetiringEffectCollisionOrder(
        GameplayModel& model
    ) {
        model.startLevel3();
        model.phase_ = GameplayPhase::level3;
        model.level2HasAdvanced_ = true;
        model.level2LoopTick_ = 2134;
        model.level2PendingInput_ = {};
        model.level2Player_.position = {};
        model.level2Player_.forwardVelocity = 0;
        model.level2Entities_.assign(3, Level1Entity{});

        auto& struck = model.level2Entities_[0];
        struck.allocationIndex = 0;
        struck.type = 44;
        struck.collisionRadius = 2365;
        struck.position = {3840, -1585, 500};
        struck.yaw = 12352;
        struck.yawVelocity = -48;
        struck.forwardVelocity = 300;
        struck.targetHeading = 12224;
        struck.stateBytes4dTo52 = {255, 15, 2, 1, 5, 2};
        struck.behaviorTimer = 1;
        struck.collisionContactAllocationIndex = 1;

        auto& returning = model.level2Entities_[1];
        returning.allocationIndex = 1;
        returning.type = 44;
        returning.collisionRadius = 2365;
        returning.position = {5949, -4066, 2100};
        returning.yaw = 61520;
        returning.yawVelocity = -200;
        returning.forwardVelocity = 300;
        returning.targetHeading = 10144;
        returning.stateBytes4dTo52 = {255, 19, 2, 1, 5, 3};
        returning.collisionContactAllocationIndex = 0;

        auto& effect = model.level2Entities_[2];
        effect.allocationIndex = 2;
        effect.flags = 0x8c;
        effect.type = -1;
        effect.position = {6190, -2450, 500};
        effect.stateBytes4dTo52 = {252, 11, 0, 0, 12, 0};

        model.level2ActorPool_ = {};
        model.level2ActorPoolActive_.fill(false);
        model.level2ActorPool_[0] = struck;
        model.level2ActorPool_[1] = returning;
        model.level2ActorPoolActive_[0] = true;
        model.level2ActorPoolActive_[1] = true;
        model.level2ProjectilePool_ = {};
        model.level2ProjectilePoolActive_.fill(false);
        model.level2DeathEffectPool_ = {};
        model.level2DeathEffectPoolActive_.fill(false);
        model.level2DeathEffectPoolPendingPresenterRetirement_.fill(false);
        model.level2DeathEffectPool_[0] = effect;
        model.level2DeathEffectPoolActive_[0] = true;
        model.level2EntityActorPoolSlots_ = {
            0,
            1,
            model.level2ActorPoolSize_,
        };
        model.level2EntityProjectilePoolSlots_ = {
            model.level2ProjectilePoolSize_,
            model.level2ProjectilePoolSize_,
            model.level2ProjectilePoolSize_,
        };
        model.level2EntityDeathEffectPoolSlots_ = {
            model.level2DeathEffectPoolSize_,
            model.level2DeathEffectPoolSize_,
            0,
        };
        model.level2ActiveListOrder_ = {1, 2, 0};
        model.level2AllocationListOrder_ = {0, 1, 2};
        model.level2ObjectListOrder_ = model.level2AllocationListOrder_;
        model.level2CompletionObjectIndex_ =
            static_cast<std::size_t>(-1);
        model.level2PostWaveObjectIndex_ =
            static_cast<std::size_t>(-1);
        model.level2PickupIndex_ = static_cast<std::size_t>(-1);
        model.level2ActiveEnemyCount_ = 2;
        model.level2SpawnedEnemyCount_ = 12;
        model.level2Complete_ = false;
    }

    static std::vector<std::int16_t> level2AllocationTypes(
        const GameplayModel& model
    ) {
        std::vector<std::int16_t> types;
        for (const auto index : model.level2AllocationListOrder_) {
            assert(index < model.level2Entities_.size());
            types.push_back(model.level2Entities_[index].type);
        }
        return types;
    }

    static void configureLevel2Type8Hit(
        GameplayModel& model,
        const std::uint8_t health
    ) {
        model.startLevel2();
        model.phase_ = GameplayPhase::level2;
        model.level2HasAdvanced_ = false;
        for (auto& entity : model.level2Entities_) {
            entity.position = {20000, -20000, 0};
        }
        auto target = std::find_if(
            model.level2Entities_.begin(),
            model.level2Entities_.end(),
            [](const Level1Entity& entity) {
                return entity.type == 8;
            }
        );
        assert(target != model.level2Entities_.end());
        target->position = {0, 500, 0};
        target->stateBytes4dTo52[1] = health;

        auto projectile = Level1Entity{};
        projectile.allocationIndex = model.level2Entities_.size();
        projectile.type = 23;
        projectile.collisionRadius = 134;
        projectile.position = {};
        projectile.yaw = 0;
        projectile.forwardVelocity = 500;
        projectile.stateBytes4dTo52 = {253, 64, 0, 0, 0, 0};
        model.level2ProjectilePool_[0] = projectile;
        model.level2ProjectilePoolActive_[0] = true;
        const auto projectileIndex = model.level2Entities_.size();
        model.level2Entities_.push_back(projectile);
        model.level2EntityActorPoolSlots_.push_back(
            model.level2ActorPoolSize_
        );
        model.level2EntityProjectilePoolSlots_.push_back(0);
        model.level2EntityDeathEffectPoolSlots_.push_back(
            model.level2DeathEffectPoolSize_
        );
        model.level2AllocationListOrder_.push_back(projectileIndex);
        model.level2ObjectListOrder_ = model.level2AllocationListOrder_;
        model.level2ActiveListOrder_ = model.level2AllocationListOrder_;
    }

    static void configureLevel2CampaignIdentityHit(GameplayModel& model) {
        configureLevel2Type8Hit(model, 2);
        model.level2EntityFixedPoolSlots_.assign(model.level2Entities_.size(), model.level2FixedObjectPoolSize_);
        model.level2FixedPoolEverUsed_.fill(false);
        auto fixedSlot = std::size_t{0};
        for (std::size_t index = 0; index < model.level2Entities_.size(); ++index) {
            if (model.level2Entities_[index].type == 30) {
                model.level2ActorPoolActive_[model.level2EntityActorPoolSlots_[index]] = false;
                model.level2EntityActorPoolSlots_[index] = model.level2ActorPoolSize_;
                model.level2EntityFixedPoolSlots_[index] = fixedSlot;
                model.level2FixedPoolEverUsed_[fixedSlot++] = true;
            }
        }
    }

    static void offsetLevel2Type8HitTarget(GameplayModel& model) {
        const auto target = std::find_if(
            model.level2Entities_.begin(),
            model.level2Entities_.end(),
            [](const Level1Entity& entity) {
                return entity.type == 8
                    && entity.position == FixedPosition{0, 500, 0};
            }
        );
        assert(target != model.level2Entities_.end());
        target->type = 47;
        target->collisionRadius = 559;
        target->position.x = 80;
        target->xVelocity = 100;
        target->behaviorTimer = 21;
        const auto targetIndex = static_cast<std::size_t>(
            std::distance(model.level2Entities_.begin(), target)
        );
        const auto projectile = std::find_if(
            model.level2Entities_.begin(),
            model.level2Entities_.end(),
            [](const Level1Entity& entity) {
                return entity.type == 23;
            }
        );
        assert(projectile != model.level2Entities_.end());
        const auto projectileIndex = static_cast<std::size_t>(
            std::distance(model.level2Entities_.begin(), projectile)
        );
        auto& active = model.level2ActiveListOrder_;
        active.erase(std::find(active.begin(), active.end(), projectileIndex));
        active.insert(
            std::find(active.begin(), active.end(), targetIndex),
            projectileIndex
        );
    }

    static void configureLevel2TerminalEffectThenHit(GameplayModel& model) {
        configureLevel2Type8Hit(model, 2);
        Level1Entity effect{};
        effect.allocationIndex = model.level2Entities_.size();
        effect.type = -1;
        effect.position = {2000, -4000, 0};
        effect.stateBytes4dTo52 = {252, 11, 0, 0, 12, 0};
        const auto effectIndex = model.level2Entities_.size();
        model.level2Entities_.push_back(effect);
        model.level2EntityActorPoolSlots_.push_back(
            model.level2ActorPoolSize_
        );
        model.level2EntityProjectilePoolSlots_.push_back(
            model.level2ProjectilePoolSize_
        );
        model.level2EntityDeathEffectPoolSlots_.push_back(0);
        model.level2DeathEffectPool_[0] = effect;
        model.level2DeathEffectPoolActive_[0] = true;
        const auto projectile = std::find_if(
            model.level2Entities_.begin(),
            model.level2Entities_.end(),
            [](const Level1Entity& entity) {
                return entity.type == 23;
            }
        );
        assert(projectile != model.level2Entities_.end());
        const auto projectileIndex = static_cast<std::size_t>(
            std::distance(model.level2Entities_.begin(), projectile)
        );
        auto& active = model.level2ActiveListOrder_;
        active.insert(
            std::find(active.begin(), active.end(), projectileIndex),
            effectIndex
        );
        model.level2AllocationListOrder_ = active;
        model.level2ObjectListOrder_ = active;
    }

    static std::vector<std::size_t> level2ActiveEffectSlots(
        const GameplayModel& model
    ) {
        std::vector<std::size_t> slots;
        for (std::size_t slot = 0;
             slot < model.level2DeathEffectPoolSize_;
             ++slot) {
            if (model.level2DeathEffectPoolActive_[slot]) {
                slots.push_back(slot);
            }
        }
        return slots;
    }

    static void configureLevel2GuardianHit(GameplayModel& model) {
        configureLevel2Type8Hit(model, 1);
        const auto target = std::find_if(
            model.level2Entities_.begin(),
            model.level2Entities_.end(),
            [](const Level1Entity& entity) {
                return entity.type == 8
                    && entity.position == FixedPosition{0, 500, 0};
            }
        );
        assert(target != model.level2Entities_.end());
        target->type = 44;
        target->collisionRadius = 1147;
        target->targetHeading = 0;
        target->stateBytes4dTo52 = {255, 1, 2, 0, 5, 0};
        model.level2PostWaveObjectIndex_ = static_cast<std::size_t>(
            std::distance(model.level2Entities_.begin(), target)
        );
    }

    static void configureLevel2Type30Hit(GameplayModel& model) {
        configureLevel2Type8Hit(model, 2);
        auto actorTarget = std::find_if(
            model.level2Entities_.begin(),
            model.level2Entities_.end(),
            [](const Level1Entity& entity) {
                return entity.type == 8
                    && entity.position == FixedPosition{0, 500, 0};
            }
        );
        assert(actorTarget != model.level2Entities_.end());
        actorTarget->position = {20000, -20000, 0};
        auto fixedTarget = std::find_if(
            model.level2Entities_.begin(),
            model.level2Entities_.end(),
            [](const Level1Entity& entity) {
                return entity.type == 30;
            }
        );
        assert(fixedTarget != model.level2Entities_.end());
        fixedTarget->position = {0, 500, 0};
        fixedTarget->stateBytes4dTo52[1] = 100;
    }

    static void configureLevel2FixedPickupHit(GameplayModel& model) {
        configureLevel2Type8Hit(model, 2);
        for (auto& entity : model.level2Entities_) {
            entity.position = entity.type == 23
                ? FixedPosition{0, 1500, 0}
                : FixedPosition{20000, -20000, 0};
        }
        Level1Entity pickup{};
        pickup.type = 25;
        pickup.collisionRadius = 897;
        pickup.position = {0, 2000, 0};
        pickup.stateBytes4dTo52 = {254, 100, 2, 0, 0, 1};
        model.allocateLevel2FixedObject(pickup);
    }

    static void hideLevel2DeathEffects(GameplayModel& model) {
        const auto removeEffects = [](
            std::vector<std::size_t>& order,
            const std::vector<Level1Entity>& entities
        ) {
            order.erase(
                std::remove_if(
                    order.begin(),
                    order.end(),
                    [&entities](const std::size_t index) {
                        return index < entities.size()
                            && entities[index].type == -1;
                    }
                ),
                order.end()
            );
        };
        removeEffects(
            model.level2ActiveListOrder_,
            model.level2Entities_
        );
        removeEffects(
            model.level2PresentationActiveListOrder_,
            model.level2PresentationEntities_
        );
        model.level2PendingTerminalEffectPresentations_.clear();
    }

    static std::vector<std::uint8_t> level2PresentedEffectFrames(
        const GameplayModel& model
    ) {
        std::vector<std::uint8_t> result;
        for (const auto index : model.level2PresentationActiveListOrder_) {
            if (
                index < model.level2PresentationEntities_.size()
                && model.level2PresentationEntities_[index].type == -1
            ) {
                result.push_back(
                    model.level2PresentationEntities_[index]
                        .stateBytes4dTo52[1]
                );
            }
        }
        return result;
    }

    static const auto& level2ProjectilePoolActive(
        const GameplayModel& model
    ) {
        return model.level2ProjectilePoolActive_;
    }
};

}

std::vector<std::uint8_t> level3NativeCheckpointFixture() {
    std::vector<std::uint8_t> data(0x10000U, 0);
    const auto write16 = [&data](
        const std::size_t offset,
        const std::uint16_t value
    ) {
        data[offset] = static_cast<std::uint8_t>(value);
        data[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
    };
    const auto write32 = [&data](
        const std::size_t offset,
        const std::uint32_t value
    ) {
        data[offset] = static_cast<std::uint8_t>(value);
        data[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
        data[offset + 2] = static_cast<std::uint8_t>(value >> 16U);
        data[offset + 3] = static_cast<std::uint8_t>(value >> 24U);
    };
    constexpr std::uint16_t player = 0xb91aU;
    constexpr std::uint16_t guardian = 0xbbf8U;
    constexpr std::uint16_t ground = 0x6000U;
    write16(0x522eU, player);
    write16(0x51fcU, 1);
    write16(0x45d2U, guardian);
    write16(0x4e44U, 1);
    write16(0x49f8U, guardian);
    write16(0x12c8U, 700);
    write16(0x1378U, 150);
    write16(0x137aU, 4);
    write16(0x47f8U, 680);
    write16(0x12aaU, 1);
    write16(0x12acU, 17);
    write32(0x51f6U, 2740);
    write32(0x3988U, 0x187ebcaaU);
    write16(0x19d0U, 0x30U);
    write16(0x19d2U, 0x6eU);
    write16(0x0730U, ground);
    write16(0x4188U, 6);

    data[player + 0x4eU] = 1;
    write16(player + 0x34U, 512);
    write16(player + 0x3cU, 200);
    data[guardian + 0x4eU] = 20;
    write16(guardian + 0x02U, 44);
    write32(guardian + 0x04U, 2365);
    write32(guardian + 0x0cU, 10000);
    write16(guardian + 0x1cU, 0x6100U);

    for (std::size_t slot = 0; slot < 8; ++slot) {
        write16(0x4972U + slot * 2U, static_cast<std::uint16_t>(
            0xc852U + slot * 0x66U
        ));
        write16(0x47d8U + slot * 2U, static_cast<std::uint16_t>(
            0xcb82U + slot * 0x66U
        ));
    }
    return data;
}

void testGameplay() {
    stellar7::GameplayModelTestAccess::assertLevel5RemovedOrdinaryBehavior();
    stellar7::GameplayModelTestAccess::assertLevel5CombatBonus();
    stellar7::GameplayModelTestAccess::assertLevel5PlayerFire();
    stellar7::GameplayModelTestAccess::assertLevel5RankMiss();
    stellar7::GameplayModelTestAccess::assertLevel5FailureHorizon();
    stellar7::GameplayModelTestAccess::assertLevel5GroundType5();
    stellar7::GameplayModelTestAccess::assertFinalLevel2FailureTail();
    stellar7::GameplayModelTestAccess::assertFailureRetiresProjectiles();
    stellar7::GameplayModelTestAccess::assertRetainedFailureRejectsCallbacks();
    stellar7::GameplayModelTestAccess::assertPresentationIdentitySurvivesRetirement();
    stellar7::IndexedScreen entryFrame;
    entryFrame.pixels.resize(320 * 200, 17);
    std::vector<std::uint8_t> entryDac(256 * 3, 37);
    entryDac[0] = 5;
    entryDac[1] = 7;
    entryDac[2] = 11;
    for (unsigned step = 0; step < stellar7::levelEntryFadeSteps; ++step) {
        const auto frame = stellar7::renderLevelEntryIndexed(entryFrame, entryDac, step);
        assert(frame.pixels == entryFrame.pixels);
        const unsigned intensity = step * stellar7::levelEntryFadeIntensityStep;
        const unsigned red = 37 * intensity / 63 + 5 * (63 - intensity) / 63;
        assert(frame.palette[17].red == ((red << 2) | (red >> 4)));
    }
    bool invalidEntryStepRejected = false;
    try {
        stellar7::renderLevelEntryIndexed(entryFrame, entryDac, stellar7::levelEntryFadeSteps);
    } catch (const std::runtime_error&) {
        invalidEntryStepRejected = true;
    }
    assert(invalidEntryStepRejected);

    stellar7::GameplayModel restoredGuardian;
    stellar7::loadLevel3NativeCheckpoint(
        restoredGuardian,
        level3NativeCheckpointFixture()
    );
    assert(restoredGuardian.phase() == stellar7::GameplayPhase::level3);
    assert(restoredGuardian.ticks() == 700);
    assert(restoredGuardian.level2LoopTick() == 700);
    assert(restoredGuardian.playerArmor() == 680);
    assert(restoredGuardian.score() == 2740);
    assert(restoredGuardian.level2Entities().size() == 1);
    assert(restoredGuardian.level2Entities()[0].type == 44);
    assert(restoredGuardian.level2Entities()[0].stateBytes4dTo52[1] == 20);
    restoredGuardian.tick({false, false, true, false, true, false});
    assert(restoredGuardian.level2LoopTick() == 701);
    assert(std::any_of(
        restoredGuardian.level2Entities().begin(),
        restoredGuardian.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 23 && entity.active();
        }
    ));
    // Pinned DOSBox-X loop 728->729 proves that releasing Left recenters
    // yaw by 0x50 while Up remains held: yaw velocity 512 -> 432 and forward
    // velocity remains 200.  Steering recentering is axis-specific; only
    // throttle damping is suppressed by another directional input.
    assert(restoredGuardian.level2Player().yawVelocity == 432);
    assert(restoredGuardian.level2Player().forwardVelocity == 200);

    stellar7::GameplayInputRecorder recorder;
    recorder.observe(1, {true, false, true, false, false, false});
    recorder.observe(2, {true, false, true, false, false, false});
    recorder.observe(3, {true, false, true, false, true, false});
    recorder.observe(4, {});
    const std::string recordedMovie = recorder.finish(4);
    assert((
        recordedMovie
        == "dos-re-state-input-script-v1\n"
           "# state_field=loop_tick\n"
           "# controller=portable-interactive-recording-v1\n"
           "# terminal_value=4\n"
           "# release_value=5\n"
           "1=down.left+up\n"
           "3=down.spc\n"
           "4=up.left+up+spc\n"
    ));
    assert(recorder.events().size() == 3);

    stellar7::GameplayInputRecorder heldAtExitRecorder;
    heldAtExitRecorder.observe(1, {false, true, false, false, true, false});
    const std::string heldAtExitMovie = heldAtExitRecorder.finish(1);
    assert(
        heldAtExitMovie.find("1=down.right+spc\n") != std::string::npos
    );
    assert(
        heldAtExitMovie.find("2=up.right+spc\n") != std::string::npos
    );

    stellar7::GameplayModel model;
    assert(model.heading() == 0);
    assert(model.travel() == 0);
    assert(model.turnVelocity() == 0);
    assert(model.forwardVelocity() == 0);
    assert(model.panoramaOffset() == 0);
    assert(!model.shotActive());
    assert(model.shotCount() == 0);
    assert(model.weaponReady());
    assert(!model.weaponIndicatorVisible());
    stellar7::GameplayModel level2WeaponReadiness;
    level2WeaponReadiness.startLevel2();
    level2WeaponReadiness.tick({false, false, false, false, true});
    level2WeaponReadiness.tick({false, false, false, false, true});
    assert(level2WeaponReadiness.level2Player().weaponCooldown == 8);
    assert(level2WeaponReadiness.weaponCooldown() == 8);
    assert(!level2WeaponReadiness.weaponReady());
    assert(model.enemyActive());
    assert(!model.explosionActive());
    assert(model.enemyBearing() == 0x2000);
    assert(model.enemyRange() == 88);
    assert(model.score() == 0);
    assert(model.phase() == stellar7::GameplayPhase::level1);
    assert(model.presentationFrame() == 0);
    assert(model.cockpitIndicatorX() == 142);

    stellar7::GameplayModel directLevel2;
    directLevel2.startLevel2();
    assert(
        directLevel2.phase() == stellar7::GameplayPhase::level2Handoff
    );
    assert(directLevel2.level2LoopTick() == 0);
    assert(directLevel2.playerArmor() == 488);
    assert(directLevel2.level2Entities().size() == 7);
    directLevel2.tick({});
    assert(directLevel2.phase() == stellar7::GameplayPhase::level2);
    assert(directLevel2.level2LoopTick() == 1);

    // Intact Level 2 model lookup and native guardian record: radius 1147.
    // Do not inherit the zero from the DS-transplanted v13 route.
    stellar7::GameplayModel level2GuardianAllocation;
    stellar7::GameplayModelTestAccess::configureLevel2PostWaveDeployment(
        level2GuardianAllocation
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(level2GuardianAllocation);
    const auto allocatedGuardian = std::find_if(
        level2GuardianAllocation.level2Entities().begin(),
        level2GuardianAllocation.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) { return entity.type == 44; }
    );
    assert(allocatedGuardian != level2GuardianAllocation.level2Entities().end());
    assert(allocatedGuardian->collisionRadius == 1147);
    // Native loop 1837 main-view boundary: countdown 2, selector 1;
    // the frame just drawn used selector 0. Radar and main each advance it.
    assert(allocatedGuardian->stateBytes4dTo52[3] == 2);
    assert(allocatedGuardian->modelAnimationVariant == 1);
    assert(allocatedGuardian->presentationModelAnimationVariant == 0);
    assert(allocatedGuardian->renderProfile == stellar7::WorldActorRenderProfile::heapSelectorDeferredAnimation);
    stellar7::GameplayModelTestAccess::assertLevel2FixedPoolOwnership();
    stellar7::GameplayModelTestAccess::assertFreshLevel2PickupAdmission();
    constexpr std::array<std::array<std::uint8_t, 3>, 3> adjacentGuardianAnimation{{
        {{1, 1, 0}}, {{2, 2, 1}}, {{2, 3, 2}},
    }};
    for (const auto expected : adjacentGuardianAnimation) {
        stellar7::GameplayModelTestAccess::advanceLevel2(level2GuardianAllocation);
        const auto guardian = std::find_if(level2GuardianAllocation.level2Entities().begin(),
            level2GuardianAllocation.level2Entities().end(), [](const auto& actor) { return actor.type == 44; });
        assert(guardian != level2GuardianAllocation.level2Entities().end());
        assert(guardian->presentationModelAnimationVariant == expected[0]);
        assert(guardian->modelAnimationVariant == expected[1]);
        assert(guardian->stateBytes4dTo52[3] == expected[2]);
        assert(guardian->renderProfile == stellar7::WorldActorRenderProfile::heapSelectorDeferredAnimation);
    }

    stellar7::GameplayModel level2SolidContact;
    stellar7::GameplayModelTestAccess::configureLevel2Type30Contact(
        level2SolidContact
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(level2SolidContact);
    assert((
        level2SolidContact.level2Player().position
        == stellar7::FixedPosition{0, 0, 0}
    ));
    assert(level2SolidContact.level2Player().forwardVelocity == -90);
    assert(
        stellar7::GameplayModelTestAccess::level2CollisionMomentum(
            level2SolidContact
        ) == -90
    );

    stellar7::GameplayModel level2Type46Contact;
    stellar7::GameplayModelTestAccess::configureLevel2Type46Contact(
        level2Type46Contact
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(level2Type46Contact);
    assert((
        level2Type46Contact.level2Player().position
        == stellar7::FixedPosition{0, 90, 0}
    ));
    assert(level2Type46Contact.level2Player().forwardVelocity == 90);
    assert(
        stellar7::GameplayModelTestAccess::level2CollisionMomentum(
            level2Type46Contact
        ) == 0
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(level2Type46Contact);
    assert(level2Type46Contact.level2Player().forwardVelocity == 20);

    stellar7::GameplayModel level2PickupContact;
    stellar7::GameplayModelTestAccess::configureLevel2Type42Contact(
        level2PickupContact
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(level2PickupContact);
    assert(std::none_of(
        level2PickupContact.level2Entities().begin(),
        level2PickupContact.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 42;
        }
    ));
    assert(level2PickupContact.score() == 100);
    assert(level2PickupContact.level2AudioEvents().back().eventId == 24);

    // Native Level 2 loop 382->383 retires a Type-19 shot against Type 30.
    // Its 0xfd state tag is shared with presenter effects, but it still
    // enters the projectile contact callback (FUN_1000:04d8).
    stellar7::GameplayModel level2HostileObstacleContact;
    stellar7::GameplayModelTestAccess::configureLevel2HostileObstacleContact(
        level2HostileObstacleContact
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(level2HostileObstacleContact);
    assert(stellar7::GameplayModelTestAccess::level2FirstProjectileRetired(
        level2HostileObstacleContact
    ));
    assert(level2HostileObstacleContact.level2Entities().size() == 1);
    assert(level2HostileObstacleContact.level2Entities()[0].type == 30);
    assert(level2HostileObstacleContact.level2Entities()[0].stateBytes4dTo52[1] == 100);

    // Native loop 786 retains two distinct terminal effect pointers in the
    // presenter snapshot after both have left the live allocation list.
    stellar7::GameplayModel level2SimultaneousTerminalEffects;
    stellar7::GameplayModelTestAccess::configureLevel2SimultaneousTerminalEffects(
        level2SimultaneousTerminalEffects
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(level2SimultaneousTerminalEffects);
    assert((stellar7::GameplayModelTestAccess::level2RetiredEffectIdentities(
        level2SimultaneousTerminalEffects
    ) == std::vector<std::size_t>{1, 2}));

    stellar7::GameplayModel terminalAfterProjectile;
    stellar7::GameplayModelTestAccess::configureLevel2ProjectileExpiryBeforeTerminalEffect(terminalAfterProjectile);
    stellar7::GameplayModelTestAccess::advanceLevel2(terminalAfterProjectile);
    assert((stellar7::GameplayModelTestAccess::level2RetiredEffectIdentities(terminalAfterProjectile)
        == std::vector<std::size_t>{1}));
    assert((terminalAfterProjectile.level2PresentationActiveListOrder() == std::vector<std::size_t>{0, 1}));

    stellar7::GameplayModel mixedRetirements;
    stellar7::GameplayModelTestAccess::configureLevel2MixedRetirements(mixedRetirements);
    stellar7::GameplayModelTestAccess::advanceLevel2(mixedRetirements);
    const auto& mixedEntities = mixedRetirements.level2PresentationEntities();
    assert(mixedEntities.size() == 3);
    std::vector<bool> mixedSeen(mixedEntities.size(), false);
    for (const auto index : mixedRetirements.level2PresentationActiveListOrder()) {
        assert(index < mixedEntities.size());
        assert(!mixedSeen[index]);
        mixedSeen[index] = true;
    }
    assert(std::all_of(mixedSeen.begin(), mixedSeen.end(), [](const auto seen) { return seen; }));
    assert((stellar7::GameplayModelTestAccess::level2RetiredEffectPositions(mixedRetirements)
        == std::vector<std::size_t>{1, 2}));
    std::vector<std::size_t> mixedSlots;
    for (std::size_t index = 0; index < mixedEntities.size(); ++index) {
        const auto slot = mixedRetirements.level2PresentationPoolSlot(index);
        assert(slot);
        mixedSlots.push_back(*slot);
    }
    assert((mixedSlots == std::vector<std::size_t>{40, 41, 42}));

    stellar7::GameplayModel level2GuardianCycle;
    stellar7::GameplayModelTestAccess::configureLevel2GuardianCycle(
        level2GuardianCycle
    );
    const auto guardian = [&level2GuardianCycle]() -> const auto& {
        return *std::find_if(
            level2GuardianCycle.level2Entities().begin(),
            level2GuardianCycle.level2Entities().end(),
            [](const stellar7::Level1Entity& entity) {
                return entity.type == 44 || entity.type == 45;
            }
        );
    };
    stellar7::GameplayModelTestAccess::advanceLevel2(level2GuardianCycle);
    assert(guardian().type == 44);
    assert(guardian().forwardVelocity == 100);
    assert(guardian().stateBytes4dTo52[5] == 1);
    assert(guardian().behaviorTimer == 36);
    stellar7::GameplayModelTestAccess::advanceLevel2(level2GuardianCycle);
    assert(guardian().type == 45);
    assert(guardian().forwardVelocity == 0);
    assert(guardian().behaviorTimer == 35);
    for (int tick = 0; tick < 15; ++tick) {
        stellar7::GameplayModelTestAccess::advanceLevel2(
            level2GuardianCycle
        );
    }
    assert(guardian().behaviorTimer == 20);
    assert(std::any_of(
        level2GuardianCycle.level2Entities().begin(),
        level2GuardianCycle.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 46;
        }
    ));

    stellar7::GameplayModel level2GuardianObjectPoolEmission;
    stellar7::GameplayModelTestAccess::
        configureLevel2GuardianObjectPoolEmission(
            level2GuardianObjectPoolEmission
        );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2GuardianObjectPoolEmission
    );
    const auto& objectPoolEntities =
        level2GuardianObjectPoolEmission.level2Entities();
    const auto objectPoolGuardian = std::find_if(
        objectPoolEntities.begin(),
        objectPoolEntities.end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 45;
        }
    );
    const auto objectPoolPulse = std::find_if(
        objectPoolEntities.begin(),
        objectPoolEntities.end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 46;
        }
    );
    assert(objectPoolGuardian != objectPoolEntities.end());
    assert(objectPoolPulse != objectPoolEntities.end());
    assert(objectPoolGuardian->allocationIndex == 0);
    assert(objectPoolPulse->allocationIndex == 3);
    assert(objectPoolPulse->collisionRadius == 0);
    assert(objectPoolPulse->pitch == 0);
    assert(objectPoolPulse->roll == 0);
    assert(objectPoolPulse->yaw == 0);
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2GuardianObjectPoolEmission
    );
    const auto& sortedObjectPoolEntities =
        level2GuardianObjectPoolEmission.level2Entities();
    const auto pulseIndex = static_cast<std::size_t>(
        std::find_if(
            sortedObjectPoolEntities.begin(),
            sortedObjectPoolEntities.end(),
            [](const stellar7::Level1Entity& entity) {
                return entity.type == 46;
            }
        ) - sortedObjectPoolEntities.begin()
    );
    const auto guardianIndex = static_cast<std::size_t>(
        std::find_if(
            sortedObjectPoolEntities.begin(),
            sortedObjectPoolEntities.end(),
            [](const stellar7::Level1Entity& entity) {
                return entity.type == 45;
            }
        ) - sortedObjectPoolEntities.begin()
    );
    const auto& sortedOrder =
        level2GuardianObjectPoolEmission.level2ActiveListOrder();
    assert(
        std::find(sortedOrder.begin(), sortedOrder.end(), pulseIndex)
        < std::find(sortedOrder.begin(), sortedOrder.end(), guardianIndex)
    );

    stellar7::GameplayModel level2GuardianContactConsumption;
    stellar7::GameplayModelTestAccess::
        configureLevel2GuardianContactConsumption(
            level2GuardianContactConsumption
        );
    stellar7::GameplayModelTestAccess::armLevel2GuardianType48Choice(
        level2GuardianContactConsumption
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2GuardianContactConsumption
    );
    assert(
        level2GuardianContactConsumption.level2Entities()[0]
            .collisionContactAllocationIndex
        == static_cast<std::size_t>(-1)
    );
    assert(std::none_of(
        level2GuardianContactConsumption.level2Entities().begin(),
        level2GuardianContactConsumption.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 48;
        }
    ));
    stellar7::GameplayModelTestAccess::armLevel2GuardianType48Choice(
        level2GuardianContactConsumption
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2GuardianContactConsumption
    );
    const auto emittedType48 = std::find_if(
        level2GuardianContactConsumption.level2Entities().begin(),
        level2GuardianContactConsumption.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 48;
        }
    );
    assert(
        emittedType48
        != level2GuardianContactConsumption.level2Entities().end()
    );
    assert(emittedType48->renderProfile == stellar7::WorldActorRenderProfile::inlineSelectorNoCallback);

    stellar7::GameplayModel level2GuardianType48Replacement;
    stellar7::GameplayModelTestAccess::
        configureLevel2GuardianType48Replacement(
            level2GuardianType48Replacement
        );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2GuardianType48Replacement
    );
    // Native route-v11 reaches FUN_1000_7970 at loop tick 1946 with
    // argument 5 after a player projectile destroys a type >= 46 actor.
    assert(level2GuardianType48Replacement.score() == 5);
    const auto replacementType48 = std::find_if(
        level2GuardianType48Replacement.level2Entities().begin(),
        level2GuardianType48Replacement.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 48;
        }
    );
    assert(
        replacementType48
        != level2GuardianType48Replacement.level2Entities().end()
    );
    // Native route-v13 tick 10138 reuses the just-retired Type-48 record;
    // its still-pending callback descends and ages the replacement once.
    assert(replacementType48->position.z == 450);
    assert(replacementType48->renderProfile == stellar7::WorldActorRenderProfile::inlineSelectorNoCallback);
    assert(replacementType48->presenterDistance != 0);
    // Level 2 DS:3988 starts at 1: the first e078 byte is 41, then the
    // pending callback decrements it. Level 3's inherited seed is different.
    assert(replacementType48->behaviorTimer == 40);
    assert(
        replacementType48->behaviorTimer
        == static_cast<std::uint8_t>(
            stellar7::GameplayModelTestAccess::level2LatestVisualRandomByte(
                level2GuardianType48Replacement
            ) - 1U
        )
    );

    stellar7::GameplayModel level2GuardianPlayerContact;
    stellar7::GameplayModelTestAccess::configureLevel2GuardianPlayerContact(
        level2GuardianPlayerContact
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2GuardianPlayerContact
    );
    assert(level2GuardianPlayerContact.playerArmor() == 32767);
    assert((
        level2GuardianPlayerContact.level2Player().position
        == stellar7::FixedPosition{0, 0, 0}
    ));
    assert(
        level2GuardianPlayerContact.level2Player().forwardVelocity == -90
    );
    assert(!level2GuardianPlayerContact.level2AudioEvents().empty());
    assert(level2GuardianPlayerContact.level2AudioEvents().back().eventId == 9);

    stellar7::GameplayModel level2FixedObjectCapacity;
    level2FixedObjectCapacity.startLevel2();
    for (std::size_t index = 2; index < 16; ++index) {
        assert(
            stellar7::GameplayModelTestAccess::allocateLevel2FixedObject(
                level2FixedObjectCapacity,
                46
            ) != static_cast<std::size_t>(-1)
        );
    }
    assert(
        stellar7::GameplayModelTestAccess::allocateLevel2FixedObject(
            level2FixedObjectCapacity,
            46
        ) == static_cast<std::size_t>(-1)
    );
    assert(level2FixedObjectCapacity.level2Entities().size() == 21);

    stellar7::GameplayModel level2ActorBeforeFixedObjects;
    level2ActorBeforeFixedObjects.startLevel2();
    assert(
        stellar7::GameplayModelTestAccess::allocateLevel2Actor(
            level2ActorBeforeFixedObjects,
            48
        ) == 5
    );
    assert(level2ActorBeforeFixedObjects.level2Entities()[5].type == 48);
    assert(level2ActorBeforeFixedObjects.level2Entities()[6].type == 30);

    stellar7::GameplayModel level2Type47GroundRiding;
    stellar7::GameplayModelTestAccess::configureLevel2Type47GroundRiding(
        level2Type47GroundRiding
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2Type47GroundRiding
    );
    assert(level2Type47GroundRiding.level2Entities()[0].xVelocity == 112);
    assert(level2Type47GroundRiding.level2Entities()[0].yVelocity == 112);

    stellar7::GameplayModel level2Type47PlayerImpact;
    stellar7::GameplayModelTestAccess::configureLevel2Type47PlayerImpact(
        level2Type47PlayerImpact
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2Type47PlayerImpact
    );
    assert(level2Type47PlayerImpact.level2Entities().empty());
    assert(level2Type47PlayerImpact.playerArmor() == 32639);
    assert(
        stellar7::GameplayModelTestAccess::level2ActiveEnemyCount(
            level2Type47PlayerImpact
        ) == 0
    );

    stellar7::GameplayModel level2Type48PlayerImpact;
    stellar7::GameplayModelTestAccess::configureLevel2Type48PlayerImpact(
        level2Type48PlayerImpact
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2Type48PlayerImpact
    );
    assert(level2Type48PlayerImpact.level2Entities().empty());
    assert((
        level2Type48PlayerImpact.level2Player().position
        == stellar7::FixedPosition{0, 90, 0}
    ));
    assert(level2Type48PlayerImpact.level2Player().forwardVelocity == 90);
    assert(
        stellar7::GameplayModelTestAccess::level2CollisionMomentum(
            level2Type48PlayerImpact
        ) == 0
    );
    assert(level2Type48PlayerImpact.playerArmor() == 32747);
    const auto& type48Audio =
        level2Type48PlayerImpact.level2AudioEvents();
    assert(type48Audio.size() >= 2);
    assert(type48Audio[type48Audio.size() - 2].eventId == 9);
    assert(type48Audio.back().eventId == 6);

    stellar7::GameplayModel level2Type47MutualImpact;
    stellar7::GameplayModelTestAccess::configureLevel2Type47MutualImpact(
        level2Type47MutualImpact
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2Type47MutualImpact
    );
    assert(std::none_of(
        level2Type47MutualImpact.level2Entities().begin(),
        level2Type47MutualImpact.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 47;
        }
    ));
    assert(
        stellar7::GameplayModelTestAccess::level2ActiveEnemyCount(
            level2Type47MutualImpact
        ) == 0
    );
    const auto mutualFrames =
        stellar7::GameplayModelTestAccess::level2PresentedEffectFrames(
            level2Type47MutualImpact
        );
    assert((mutualFrames == std::vector<std::uint8_t>{18, 21}));

    stellar7::GameplayModel level2Type30Hit;
    stellar7::GameplayModelTestAccess::configureLevel2Type30Hit(
        level2Type30Hit
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(level2Type30Hit);
    const auto hitType30 = std::find_if(
        level2Type30Hit.level2Entities().begin(),
        level2Type30Hit.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 30
                && entity.position == stellar7::FixedPosition{0, 500, 0};
        }
    );
    assert(hitType30 != level2Type30Hit.level2Entities().end());
    assert(hitType30->stateBytes4dTo52[1] == 99);

    // Intact Level 2 post-guardian state includes a projectile hit on the
    // fixed-pool Type-25 pickup. 072a excludes projectile/effect categories,
    // not fixed-pool objects. It survives with health 99 and a hit effect.
    stellar7::GameplayModel level2FixedPickupHit;
    stellar7::GameplayModelTestAccess::configureLevel2FixedPickupHit(
        level2FixedPickupHit
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(level2FixedPickupHit);
    const auto hitPickup = std::find_if(
        level2FixedPickupHit.level2Entities().begin(),
        level2FixedPickupHit.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) { return entity.type == 25; }
    );
    assert(hitPickup != level2FixedPickupHit.level2Entities().end());
    assert(hitPickup->stateBytes4dTo52[1] == 99);
    assert(std::none_of(
        level2FixedPickupHit.level2Entities().begin(),
        level2FixedPickupHit.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) { return entity.type == 23; }
    ));

    stellar7::GameplayModel level2NonlethalType8Hit;
    stellar7::GameplayModelTestAccess::configureLevel2Type8Hit(
        level2NonlethalType8Hit,
        2
    );
    // Native route-v13 tick 10160 retains the surviving guardian hit effect
    // at the target's pre-integration center when the projectile callback
    // precedes the target callback, just like the lethal tick-7708 case.
    stellar7::GameplayModelTestAccess::offsetLevel2Type8HitTarget(
        level2NonlethalType8Hit
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2NonlethalType8Hit
    );
    const auto nonlethalEffect = std::find_if(
        level2NonlethalType8Hit.level2Entities().begin(),
        level2NonlethalType8Hit.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == -1;
        }
    );
    assert(
        nonlethalEffect
        != level2NonlethalType8Hit.level2Entities().end()
    );
    assert(nonlethalEffect->stateBytes4dTo52[1] == 2);
    assert(nonlethalEffect->stateBytes4dTo52[4] == 12);
    assert((
        nonlethalEffect->position == stellar7::FixedPosition{80, 500, 0}
    ));

    stellar7::GameplayModel effectIdentity;
    stellar7::GameplayModelTestAccess::configureLevel2CampaignIdentityHit(effectIdentity);
    stellar7::GameplayModelTestAccess::advanceLevel2(effectIdentity);
    const auto assertFixedIdentity = [](const stellar7::GameplayModel& model) {
        std::vector<std::size_t> fixed;
        const auto& presented = model.level2PresentationEntities();
        for (std::size_t index = 0; index < presented.size(); ++index) {
            if (presented[index].type == 30) {
                const auto slot = model.level2PresentationPoolSlot(index);
                assert(slot);
                fixed.push_back(*slot);
            }
        }
        assert((fixed == std::vector<std::size_t>{16, 17}));
    };
    assertFixedIdentity(effectIdentity);
    for (unsigned step = 0; step < 10; ++step) {
        stellar7::GameplayModelTestAccess::advanceLevel2(effectIdentity);
    }
    assertFixedIdentity(effectIdentity);
    const auto& terminalEffects = effectIdentity.level2PresentationEntities();
    const auto terminalEffect = std::find_if(terminalEffects.begin(), terminalEffects.end(), [](const auto& entity) {
        return entity.type == -1 && entity.stateBytes4dTo52[1] == 12;
    });
    assert(terminalEffect != terminalEffects.end());
    assert(effectIdentity.level2PresentationPoolSlot(static_cast<std::size_t>(terminalEffect - terminalEffects.begin())) == 40);

    stellar7::GameplayModel level2TerminalEffectThenHit;
    stellar7::GameplayModelTestAccess::configureLevel2TerminalEffectThenHit(
        level2TerminalEffectThenHit
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2TerminalEffectThenHit
    );
    assert((
        stellar7::GameplayModelTestAccess::level2ActiveEffectSlots(
            level2TerminalEffectThenHit
        ) == std::vector<std::size_t>{1}
    ));

    stellar7::GameplayModel level2LethalType8Hit;
    stellar7::GameplayModelTestAccess::configureLevel2Type8Hit(
        level2LethalType8Hit,
        1
    );
    // Native route-v12 tick 7708 proves that FUN_1000:1176 places the
    // lethal Type 47 transient at the target's pre-integration collision
    // position when the projectile callback precedes the target callback.
    // Keep its later integrated center observably offset so this ordering
    // distinction cannot regress.
    stellar7::GameplayModelTestAccess::offsetLevel2Type8HitTarget(
        level2LethalType8Hit
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2LethalType8Hit
    );
    assert(level2LethalType8Hit.level2AudioEvents().size() == 2);
    assert(level2LethalType8Hit.level2AudioEvents()[0].eventId == 9);
    assert(level2LethalType8Hit.level2AudioEvents()[1].eventId == 6);
    const auto lethalEffect = std::find_if(
        level2LethalType8Hit.level2Entities().begin(),
        level2LethalType8Hit.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == -1;
        }
    );
    assert(lethalEffect != level2LethalType8Hit.level2Entities().end());
    assert(lethalEffect->stateBytes4dTo52[1] == 15);
    assert(lethalEffect->stateBytes4dTo52[4] == 17);
    assert((
        lethalEffect->position == stellar7::FixedPosition{80, 500, 0}
    ));

    stellar7::GameplayModel level2LethalGuardianHit;
    stellar7::GameplayModelTestAccess::configureLevel2GuardianHit(
        level2LethalGuardianHit
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2LethalGuardianHit
    );
    // This overlapping fixture also emits player-contact9. Original1176
    // chooses guardian12 instead of ordinary death6; contact9 remains.
    assert(level2LethalGuardianHit.level2AudioEvents().size() == 2);
    assert(level2LethalGuardianHit.level2AudioEvents()[0].eventId == 9);
    assert(level2LethalGuardianHit.level2AudioEvents()[1].eventId == 12);
    const auto guardianResult = std::find_if(
        level2LethalGuardianHit.level2Entities().begin(),
        level2LethalGuardianHit.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 25;
        }
    );
    assert(
        guardianResult
        != level2LethalGuardianHit.level2Entities().end()
    );
    assert(guardianResult->collisionRadius == 897);
    assert((
        guardianResult->stateBytes4dTo52
        == std::array<std::uint8_t, 6>{254, 100, 3, 0, 0, 1}
    ));
    const auto guardianResultYaw = guardianResult->yaw;
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2LethalGuardianHit
    );
    const auto guardianResultAfterAdvance = std::find_if(
        level2LethalGuardianHit.level2Entities().begin(),
        level2LethalGuardianHit.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 25;
        }
    );
    assert(
        guardianResultAfterAdvance
        != level2LethalGuardianHit.level2Entities().end()
    );
    // Native route-v13 ticks 10231 and 10265 retain the fixed pickup's
    // world yaw even though its presenter spin velocity field is nonzero.
    assert(guardianResultAfterAdvance->yaw == guardianResultYaw);

    stellar7::GameplayModel cockpitIndicator;
    for (int tick = 0; tick < 100; ++tick) {
        cockpitIndicator.tick({});
    }
    assert(cockpitIndicator.cockpitIndicatorX() == 158);
    cockpitIndicator.tick({});
    assert(cockpitIndicator.cockpitIndicatorX() == 162);

    model.tick({true, false, false, false, false});
    assert(model.heading() == 0);
    assert(model.turnVelocity() == 0);
    model.tick({true, false, false, false, false});
    assert(model.heading() == 80);
    assert(model.turnVelocity() == 80);
    assert(model.panoramaOffset() == 1023);
    for (int step = 0; step < 5; ++step) {
        model.tick({true, false, false, false, false});
    }
    assert(model.heading() == 1680);
    assert(model.turnVelocity() == 480);
    model.tick({true, false, false, false, false});
    assert(model.heading() == 2192);
    assert(model.turnVelocity() == 512);
    model.tick({false, false, false, false, false});
    assert(model.heading() == 2704);
    assert(model.turnVelocity() == 512);
    for (int step = 0; step < 7; ++step) {
        model.tick({false, false, false, false, false});
    }
    assert(model.turnVelocity() == 0);

    stellar7::GameplayModel rightControl;
    rightControl.tick({false, true, false, false, false});
    rightControl.tick({false, true, false, false, false});
    assert(rightControl.heading() == 65456);
    assert(rightControl.turnVelocity() == -80);
    assert(rightControl.panoramaOffset() == 2);

    stellar7::GameplayModel forwardControl;
    for (int step = 0; step < 21; ++step) {
        forwardControl.tick({false, false, true, false, false});
    }
    assert(forwardControl.forwardVelocity() == 200);
    assert(forwardControl.travel() == 1900);
    forwardControl.tick({false, false, false, false, false});
    assert(forwardControl.travel() == 2100);
    assert(forwardControl.forwardVelocity() == 200);
    forwardControl.tick({false, false, false, false, false});
    assert(forwardControl.travel() == 2300);
    assert(forwardControl.forwardVelocity() == 190);

    // The pristine tick-152 user-route boundary proves that active steering
    // suppresses neutral throttle damping. FUN_1000_9e10 damps both axes only
    // when no directional input is present.
    stellar7::GameplayModel steerWhileCoasting;
    for (int step = 0; step < 21; ++step) {
        steerWhileCoasting.tick({false, false, true, false, false});
    }
    steerWhileCoasting.tick({true, false, false, false, false});
    steerWhileCoasting.tick({true, false, false, false, false});
    assert(steerWhileCoasting.turnVelocity() == 80);
    assert(steerWhileCoasting.forwardVelocity() == 200);

    stellar7::GameplayModel reverseControl;
    for (int step = 0; step < 21; ++step) {
        reverseControl.tick({false, false, false, true, false});
    }
    assert(reverseControl.forwardVelocity() == -200);
    assert(reverseControl.travel() == -1900);

    model.tick({false, false, false, false, true});
    assert(!model.shotActive());
    model.tick({false, false, false, false, false});
    assert(model.shotActive());
    assert(model.shotCount() == 1);
    assert(model.weaponCooldown() == 8);
    assert(model.weaponIndicatorVisible());
    assert(!model.weaponReadyIndicator());

    stellar7::GameplayModel pass;
    for (int step = 0; step < 15; ++step) {
        pass.tick({false, false, false, false, false});
    }
    assert(pass.level1State().loopTick() == 15);
    assert(pass.enemyBearing() == 0x1EA0);
    assert(pass.enemyRange() == 82);

    stellar7::GameplayModel combat;
    for (int step = 0; step < 200 && combat.score() == 0; ++step) {
        const auto difference = static_cast<std::int16_t>(
            combat.enemyBearing() - combat.heading()
        );
        const int desiredVelocity = std::clamp(
            static_cast<int>(difference) / 4,
            -0x200,
            0x200
        );
        const int currentVelocity = combat.turnVelocity();
        const bool left = currentVelocity < desiredVelocity - 0x28;
        const bool right = currentVelocity > desiredVelocity + 0x28;
        const bool fire = (
            std::abs(static_cast<int>(difference)) < 0x100
            && std::abs(currentVelocity) < 0xA0
        );
        combat.tick({left, right, false, false, fire});
    }
    assert(combat.enemyActive());
    assert(combat.explosionActive());
    assert(combat.score() == 20);

    stellar7::IndexedScreen cockpit{
        std::vector<std::uint8_t>(320 * 200, 0),
        std::vector<stellar7::Rgb>(256),
    };
    for (std::size_t index = 0; index < cockpit.palette.size(); ++index) {
        cockpit.palette[index] = {
            static_cast<std::uint8_t>(index),
            static_cast<std::uint8_t>(index),
            static_cast<std::uint8_t>(index),
        };
    }
    std::vector<stellar7::IndexedBitmap> horizon{
        {256, 16, std::vector<std::uint8_t>(256 * 16, 7)},
    };
    std::vector<std::vector<stellar7::IndexedBitmap>> art(6);
    art[3].resize(1, {1, 1, {0}});
    art[3][0] = {1, 1, {5}};
    art[4].resize(42, {1, 1, {0}});
    art[4][39] = {1, 1, {11}};
    art[4][40] = {1, 1, {12}};
    art[4][41] = {1, 1, {6}};
    art[5].resize(31, {1, 1, {0}});
    art[5][14] = {
        4,
        2,
        std::vector<std::uint8_t>(8, 123),
    };
    art[5][15] = {
        4,
        2,
        std::vector<std::uint8_t>(8, 124),
    };
    art[5][16] = {
        4,
        2,
        std::vector<std::uint8_t>(8, 125),
    };
    art[5][25] = {3, 3, std::vector<std::uint8_t>(9, 9)};
    const stellar7::GameplayAssets assets{
        std::move(cockpit),
        std::move(horizon),
        std::move(art),
    };

    // Checkpoint 142's first native guardian display frame uses the Level 3
    // sky/ground indices at logical (100,50) and (100,100).  The portable
    // renderer previously reused the Level 2 values 64/78 here; keep this
    // assertion adjacent to the renderer fixture before changing it.
    stellar7::GameplayModel level3DisplayModel;
    level3DisplayModel.startLevel3();
    const auto level3DisplayFrame = stellar7::renderGameplayIndexed(
        assets,
        level3DisplayModel
    );
    assert(level3DisplayFrame.pixels[50 * 320 + 100] == 168);
    assert(level3DisplayFrame.pixels[100 * 320 + 100] == 203);

    // The optional native guardian surface only restores the cockpit/HUD
    // perimeter; the world viewport remains renderer-driven.
    auto guardianSurfaceAssets = assets;
    guardianSurfaceAssets.level3GuardianFrames.push_back({
        std::vector<std::uint8_t>(320 * 200, 222),
        std::vector<stellar7::Rgb>(256),
    });
    const auto cleanGuardianFrame = stellar7::renderGameplayIndexed(
        guardianSurfaceAssets,
        level3DisplayModel
    );
    assert(cleanGuardianFrame.pixels[10 * 320 + 10] != 222);
    guardianSurfaceAssets.useUnprovenLevel3GuardianSurface = true;
    const auto guardianSurfaceFrame = stellar7::renderGameplayIndexed(
        guardianSurfaceAssets,
        level3DisplayModel
    );
    assert(guardianSurfaceFrame.pixels[10 * 320 + 10] == 222);
    // The running guardian sequence also proves the two upper viewport side
    // trims are static presenter pixels; the inner world remains dynamic.
    assert(guardianSurfaceFrame.pixels[70 * 320 + 30] == 222);
    assert(guardianSurfaceFrame.pixels[70 * 320 + 285] == 222);
    // The native post-display page also retains the six-pixel lower side
    // trims. Keep these coordinates explicit so composition cannot expose
    // the dynamic world at the perimeter.
    assert(guardianSurfaceFrame.pixels[140 * 320 + 24] == 222);
    assert(guardianSurfaceFrame.pixels[145 * 320 + 31] == 222);
    assert(guardianSurfaceFrame.pixels[140 * 320 + 292] == 222);
    assert(guardianSurfaceFrame.pixels[145 * 320 + 295] == 222);
    assert(guardianSurfaceFrame.pixels[100 * 320 + 100] != 222);

    // Checkpoint 164's native 6d46 capture proves that the central cockpit
    // strip is transferred from the cached x=142 page rather than redrawn as
    // an ordinary indicator sprite. Verify both the identity destination and
    // the measured x=134 lane permutation used by the running sequence.
    auto pageTransferAssets = guardianSurfaceAssets;
    auto& cachedPage = pageTransferAssets.level3GuardianFrames.front();
    for (const int y : {122, 123, 125, 126}) {
        for (int i = 0; i < 8; ++i) {
            cachedPage.pixels[static_cast<std::size_t>(y * 320 + 142 + i)] =
                static_cast<std::uint8_t>(30 + i);
        }
    }
    for (int x = 134; x < 150; ++x) {
        cachedPage.pixels[124 * 320 + x] =
            static_cast<std::uint8_t>(80 + x - 134);
    }
    const auto pageTransferIdentity = stellar7::renderGameplayIndexed(
        pageTransferAssets,
        level3DisplayModel
    );
    for (const int y : {122, 123, 124, 125, 126}) {
        for (int i = 0; i < 8; ++i) {
            assert(
                pageTransferIdentity.pixels[y * 320 + 142 + i]
                == cachedPage.pixels[y * 320 + 142 + i]
            );
        }
    }
    auto shiftedIndicatorModel = level3DisplayModel;
    stellar7::GameplayModelTestAccess::setLevel2CockpitIndicatorX(
        shiftedIndicatorModel,
        134
    );
    const auto pageTransferShifted = stellar7::renderGameplayIndexed(
        pageTransferAssets,
        shiftedIndicatorModel
    );
    for (const int y : {122, 123, 125, 126}) {
        for (int i = 0; i < 8; ++i) {
            assert(
                pageTransferShifted.pixels[y * 320 + 134 + i]
                == cachedPage.pixels[y * 320 + 142 + i]
            );
        }
    }
    constexpr std::array<int, 8> expectedRow124Source = {
        142, 143, 144, 145, 138, 147, 140, 149
    };
    for (int i = 0; i < 8; ++i) {
        assert(
            pageTransferShifted.pixels[124 * 320 + 134 + i]
            == cachedPage.pixels[
                124 * 320 + expectedRow124Source[static_cast<std::size_t>(i)]
            ]
        );
    }

    // A native guardian page is partially redrawn rather than cleared each
    // loop. The history-aware path must preserve an existing world pixel
    // when the current model has no rasterized primitive at that coordinate.
    stellar7::IndexedScreen historySeed{
        std::vector<std::uint8_t>(320 * 200, 173),
        std::vector<stellar7::Rgb>(256),
    };
    auto historyAssets = guardianSurfaceAssets;
    historyAssets.artGroups[3].push_back({
        56,
        6,
        std::vector<std::uint8_t>(56 * 6, 151),
    });
    const auto historyFrame = stellar7::renderGameplayIndexedWithHistory(
        historyAssets,
        level3DisplayModel,
        historySeed
    );
    assert(historyFrame.pixels[100 * 320 + 100] == 173);
    assert(historyFrame.pixels[122 * 320 + 142] == 173);
    assert(historyFrame.pixels[10 * 320 + 10] == 222);

    // A native page update redraws the world into the retained destination
    // page, but the cached cockpit strip is carried forward. This is the
    // double-buffer probe used by the Level 3 residual investigation.
    const auto pageHistoryFrame =
        stellar7::renderGameplayIndexedWithPageHistory(
            historyAssets,
            level3DisplayModel,
            historySeed
        );
    assert(pageHistoryFrame.pixels[100 * 320 + 100] == 203);
    assert(pageHistoryFrame.pixels[122 * 320 + 142] == 173);
    assert(pageHistoryFrame.pixels[10 * 320 + 10] == 222);

    auto level2WithoutDeathEffect = level2LethalType8Hit;
    stellar7::GameplayModelTestAccess::hideLevel2DeathEffects(
        level2WithoutDeathEffect
    );
    const auto level2WithoutDeathEffectFrame =
        stellar7::renderGameplayIndexed(assets, level2WithoutDeathEffect);
    const auto level2WithDeathEffectFrame =
        stellar7::renderGameplayIndexed(assets, level2LethalType8Hit);
    // The non-turbo native proof renders lethal frame 15 on the same loop
    // boundary where the effect first appears in state.
    assert(
        level2WithDeathEffectFrame.pixels
        != level2WithoutDeathEffectFrame.pixels
    );
    auto level2Score2740 = level2LethalType8Hit;
    stellar7::GameplayModelTestAccess::setLevel2Score(
        level2Score2740,
        2740
    );
    const auto level2Score2740Frame = stellar7::renderGameplayIndexed(
        assets,
        level2Score2740
    );
    // Controlled native route-v13 loop 10231 draws the active score field at
    // x=240. Its cockpit font uses a narrow 7 and the original angular 4.
    assert(level2Score2740Frame.pixels[155 * 320 + 239] == 0);
    assert(level2Score2740Frame.pixels[155 * 320 + 240] == 239);
    assert(level2Score2740Frame.pixels[155 * 320 + 246] == 239);
    assert(level2Score2740Frame.pixels[155 * 320 + 251] == 0);
    assert(level2Score2740Frame.pixels[155 * 320 + 253] == 239);
    assert(level2Score2740Frame.pixels[156 * 320 + 252] == 239);
    assert(level2Score2740Frame.pixels[157 * 320 + 251] == 239);
    assert(level2Score2740Frame.pixels[159 * 320 + 253] == 239);
    assert(level2Score2740Frame.pixels[155 * 320 + 257] == 239);
    stellar7::GameplayModel level2RadarCategories;
    stellar7::GameplayModelTestAccess::configureLevel2RadarCategoryProbe(
        level2RadarCategories
    );
    const auto level2RadarCategoryFrame = stellar7::renderGameplayIndexed(
        assets,
        level2RadarCategories
    );
    // Native route-v13 loop 10231 uses the Type-25 result override and the
    // category -3 projectile color from DS:12ea/12f1.
    assert(level2RadarCategoryFrame.pixels[160 * 320 + 154] == 234);
    assert(level2RadarCategoryFrame.pixels[163 * 320 + 154] == 238);
    stellar7::GameplayModelTestAccess::configureLevel2RadarCategoryProbe(
        level2RadarCategories, true
    );
    const auto level2RadarEffectFrame = stellar7::renderGameplayIndexed(
        assets, level2RadarCategories
    );
    // Original 698a indexes DS:12ee-category without a health gate. The
    // Type -1 category -4 contact uses index 78, including its terminal draw.
    assert(level2RadarEffectFrame.pixels[163 * 320 + 154] == 78);
    stellar7::GameplayModelTestAccess::retireLevel2RadarProbe(level2RadarCategories);
    assert(stellar7::renderGameplayIndexed(assets, level2RadarCategories)
        .pixels[163 * 320 + 154] == 78);
    auto level2OcclusionAssets = assets;
    level2OcclusionAssets.artGroups[5][14] = {
        640,
        400,
        std::vector<std::uint8_t>(640 * 400, 123),
    };
    auto level2PhaseZero = level2LethalType8Hit;
    stellar7::GameplayModelTestAccess::setAlternateSightPhase(
        level2PhaseZero,
        false
    );
    const auto level2OcclusionFrame = stellar7::renderGameplayIndexed(
        level2OcclusionAssets,
        level2PhaseZero
    );
    // FUN_1000_674e runs after the entity presenter. At v13 loop 10231 the
    // eight-tick lock bit is clear, so the phase-zero outer sight overlays
    // the opaque guardian explosion while an unoccupied phase-one row does
    // not.
    assert(level2OcclusionFrame.pixels[77 * 320 + 135] == 95);
    assert(level2OcclusionFrame.pixels[80 * 320 + 135] == 123);
    assert(level2OcclusionFrame.pixels[75 * 320 + 143] == 95);
    auto level2PhaseOne = level2PhaseZero;
    stellar7::GameplayModelTestAccess::setAlternateSightPhase(
        level2PhaseOne,
        true
    );
    const auto level2PhaseOneFrame = stellar7::renderGameplayIndexed(
        level2OcclusionAssets,
        level2PhaseOne
    );
    assert(level2PhaseOneFrame.pixels[77 * 320 + 135] == 123);
    assert(level2PhaseOneFrame.pixels[80 * 320 + 135] == 95);

    stellar7::GameplayModel level2IndicatorCadence;
    level2IndicatorCadence.startLevel2();
    const auto& level2Initial = level2IndicatorCadence.level2Entities();
    assert(level2Initial.size() == 7);
    assert(level2Initial[3].type == 13 && level2Initial[3].yaw == 768);
    assert((level2Initial[3].position == stellar7::FixedPosition{-24576, 28672, 0}));
    assert(level2Initial[6].type == 30 && level2Initial[6].yaw == 0);
    assert((level2Initial[6].position == stellar7::FixedPosition{8192, -20480, 0}));
    stellar7::GameplayModel level3Placement;
    level3Placement.startLevel3();
    const auto& level3Initial = level3Placement.level2Entities();
    assert(level3Initial.size() == 6);
    assert(level3Initial[3].type == 2 && level3Initial[3].yaw == 16384);
    assert((level3Initial[3].position == stellar7::FixedPosition{16384, 24576, 0}));
    assert(level3Initial[4].type == 24 && level3Initial[5].type == 31);
    assert(level3Initial[4].yaw == 0 && level3Initial[5].yaw == 0);
    assert((level3Initial[4].position == stellar7::FixedPosition{-20480, -12288, 0}));
    assert(level3Initial[4].position == level3Initial[5].position);
    assert(level2IndicatorCadence.level2CockpitIndicatorX() == 142);
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2IndicatorCadence
    );
    assert(level2IndicatorCadence.level2CockpitIndicatorX() == 138);
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2IndicatorCadence
    );
    assert(level2IndicatorCadence.level2CockpitIndicatorX() == 134);
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2IndicatorCadence
    );
    assert(level2IndicatorCadence.level2CockpitIndicatorX() == 130);
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2IndicatorCadence
    );
    assert(level2IndicatorCadence.level2CockpitIndicatorX() == 134);

    stellar7::GameplayModel level2SightCadence;
    stellar7::GameplayModelTestAccess::configureLevel2SightProbe(
        level2SightCadence,
        44,
        true,
        7
    );
    stellar7::GameplayModelTestAccess::setAlternateSightPhase(
        level2SightCadence,
        true
    );
    stellar7::GameplayModelTestAccess::refreshLevel2SightPhase(
        level2SightCadence
    );
    assert(level2SightCadence.alternateSightPhase());
    stellar7::GameplayModelTestAccess::configureLevel2SightProbe(
        level2SightCadence,
        44,
        true,
        8
    );
    stellar7::GameplayModelTestAccess::setAlternateSightPhase(
        level2SightCadence,
        true
    );
    stellar7::GameplayModelTestAccess::refreshLevel2SightPhase(
        level2SightCadence
    );
    assert(!level2SightCadence.alternateSightPhase());
    stellar7::GameplayModelTestAccess::configureLevel2SightProbe(
        level2SightCadence,
        8,
        true,
        8
    );
    stellar7::GameplayModelTestAccess::refreshLevel2SightPhase(
        level2SightCadence
    );
    assert(level2SightCadence.alternateSightPhase());
    // 9cb6 scans listed pointers without a health test. A Type -1 terminal
    // effect remains a sight candidate at the native 2288 boundary.
    stellar7::GameplayModelTestAccess::configureLevel2SightProbe(
        level2SightCadence, -1, false, 8);
    stellar7::GameplayModelTestAccess::refreshLevel2SightPhase(level2SightCadence);
    assert(level2SightCadence.alternateSightPhase());
    const auto level2EffectIsVisible = [&assets](
        const stellar7::GameplayModel& source
    ) {
        auto hidden = source;
        stellar7::GameplayModelTestAccess::hideLevel2DeathEffects(hidden);
        return stellar7::renderGameplayIndexed(assets, source).pixels
            != stellar7::renderGameplayIndexed(assets, hidden).pixels;
    };
    auto level2LethalPresentationSequence = level2LethalType8Hit;
    assert(
        stellar7::GameplayModelTestAccess::level2PresentedEffectFrames(
            level2LethalPresentationSequence
        ) == std::vector<std::uint8_t>{15}
    );
    assert(level2EffectIsVisible(level2LethalPresentationSequence));
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2LethalPresentationSequence
    );
    assert(
        stellar7::GameplayModelTestAccess::level2PresentedEffectFrames(
            level2LethalPresentationSequence
        ) == std::vector<std::uint8_t>{16}
    );
    assert(level2EffectIsVisible(level2LethalPresentationSequence));
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2LethalPresentationSequence
    );
    assert(
        stellar7::GameplayModelTestAccess::level2PresentedEffectFrames(
            level2LethalPresentationSequence
        ) == std::vector<std::uint8_t>{17}
    );
    assert(level2EffectIsVisible(level2LethalPresentationSequence));
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level2LethalPresentationSequence
    );
    assert(!level2EffectIsVisible(level2LethalPresentationSequence));

    const auto successPhase0 = stellar7::level1SuccessPalette(
        assets.cockpit.palette,
        0
    );
    const auto successPhase1 = stellar7::level1SuccessPalette(
        assets.cockpit.palette,
        1
    );
    const auto successPhase94 = stellar7::level1SuccessPalette(
        assets.cockpit.palette,
        94
    );
    const auto successPhase2 = stellar7::level1SuccessPalette(
        assets.cockpit.palette,
        2
    );
    assert(successPhase0[15] == assets.cockpit.palette[15]);
    assert((successPhase0[16] == stellar7::Rgb{0, 255, 0}));
    assert((successPhase0[17] == stellar7::Rgb{0, 251, 0}));
    assert((successPhase0[63] == stellar7::Rgb{0, 65, 0}));
    assert((successPhase1[16] == stellar7::Rgb{0, 251, 0}));
    assert((successPhase1[17] == stellar7::Rgb{0, 255, 0}));
    assert((successPhase1[63] == stellar7::Rgb{0, 69, 0}));
    assert(successPhase94 == successPhase0);
    assert(stellar7::level1SuccessPhaseOrdinal(0) == 0);
    assert(stellar7::level1SuccessPhaseOrdinal(1) == 2);
    assert(stellar7::level1SuccessPhaseOrdinal(2) == 4);
    assert(stellar7::level1SuccessPhaseOrdinal(3) == 6);
    const auto frozenSuccess = stellar7::renderLevel1SuccessIndexed(
        assets.cockpit,
        1
    );
    assert(frozenSuccess.pixels == assets.cockpit.pixels);
    assert(frozenSuccess.palette == successPhase2);

    auto panoramaAssets = assets;
    panoramaAssets.horizonFrames = {
        {256, 16, std::vector<std::uint8_t>(256 * 16, 7)},
        {256, 16, std::vector<std::uint8_t>(256 * 16, 8)},
        {256, 16, std::vector<std::uint8_t>(256 * 16, 9)},
        {256, 16, std::vector<std::uint8_t>(256 * 16, 10)},
    };
    stellar7::GameplayModel panoramaModel;
    const auto initialPresentation = stellar7::renderGameplay(
        panoramaAssets,
        panoramaModel
    );
    assert(initialPresentation.pixels[68 * 320 + 24].red == 7);
    bool hasInitialHostile = false;
    for (int y = 146; y <= 180; ++y) {
        for (int x = 112; x <= 205; ++x) {
            hasInitialHostile = hasInitialHostile
                || initialPresentation.pixels[y * 320 + x].red == 228;
        }
    }
    assert(hasInitialHostile);
    panoramaModel.tick({true, false, false, false, false});
    panoramaModel.tick({true, false, false, false, false});
    const auto steeredPresentation = stellar7::renderGameplay(
        panoramaAssets,
        panoramaModel
    );
    assert(steeredPresentation.pixels[68 * 320 + 24].red == 10);
    assert(steeredPresentation.pixels[68 * 320 + 25].red == 7);
    bool hasSteeredHostile = false;
    bool hasSteeredLandmark = false;
    for (int y = 146; y <= 180; ++y) {
        for (int x = 112; x <= 205; ++x) {
            const auto index = steeredPresentation.pixels[y * 320 + x].red;
            hasSteeredHostile = hasSteeredHostile || index == 228;
            hasSteeredLandmark = hasSteeredLandmark || index == 138;
        }
    }
    assert(hasSteeredHostile);
    assert(hasSteeredLandmark);

    stellar7::GameplayModel throttlePresentation;
    for (int step = 0; step < 21; ++step) {
        throttlePresentation.tick({false, false, true, false, false});
    }
    const auto forwardPresentation = stellar7::renderGameplay(
        panoramaAssets,
        throttlePresentation
    );
    bool hasForwardHostile = false;
    for (int y = 146; y <= 180; ++y) {
        for (int x = 112; x <= 205; ++x) {
            hasForwardHostile = hasForwardHostile
                || forwardPresentation.pixels[y * 320 + x].red == 228;
        }
    }
    assert(hasForwardHostile);

    const auto frame = stellar7::renderGameplay(assets, model);
    const auto indexedFrame = stellar7::renderGameplayIndexed(assets, model);
    assert(indexedFrame.pixels.size() == frame.pixels.size());
    assert(indexedFrame.palette == assets.cockpit.palette);
    for (std::size_t offset = 0; offset < frame.pixels.size(); ++offset) {
        assert(
            frame.pixels[offset]
            == indexedFrame.palette[indexedFrame.pixels[offset]]
        );
    }
    assert(frame.width == 320);
    assert(frame.height == 200);
    assert(frame.pixels.size() == 320 * 200);
    assert((frame.pixels.front() == stellar7::Rgb{0, 0, 0}));
    assert(frame.pixels[68 * 320 + 50].red == 7);
    assert(frame.pixels[127 * 320 + 96].red == 5);
    assert(frame.pixels[156 * 320 + 14].red == 6);
    assert(frame.pixels[84 * 320 + 131].red == 31);
    assert(frame.pixels[84 * 320 + 187].red == 31);
    assert(frame.pixels[74 * 320 + 159].red == 31);
    assert(frame.pixels[94 * 320 + 159].red == 31);
    assert(frame.pixels[75 * 320 + 143].red == 95);
    assert(frame.pixels[85 * 320 + 159].red == 95);
    assert(frame.pixels[170 * 320 + 243].red == 11);

    for (int step = 0; step < 3; ++step) {
        model.tick({false, false, false, false, false});
    }
    const auto secondShotStage = stellar7::renderGameplay(assets, model);
    assert(secondShotStage.pixels[84 * 320 + 147].red == 31);
    assert(secondShotStage.pixels[84 * 320 + 171].red == 31);
    assert(secondShotStage.pixels[84 * 320 + 146].red != 31);

    for (int step = 0; step < 6; ++step) {
        model.tick({false, false, false, false, false});
    }
    assert(model.weaponCooldown() == 0);
    assert(model.weaponReady());
    assert(model.weaponReadyIndicator());
    const auto readyFrame = stellar7::renderGameplay(assets, model);
    assert(readyFrame.pixels[170 * 320 + 243].red == 12);

    model.tick({false, false, false, false, true});
    assert(model.shotCount() == 1);
    model.tick({false, false, false, false, false});
    assert(model.shotCount() == 2);
    assert(model.weaponCooldown() == 8);
    assert(!model.weaponReadyIndicator());
    const auto secondLaunch = stellar7::renderGameplay(assets, model);
    assert(secondLaunch.pixels[170 * 320 + 243].red == 11);

    for (int step = 0; step < 9; ++step) {
        model.tick({false, false, false, false, false});
    }
    assert(model.weaponCooldown() == 0);
    assert(model.shotCount() == 2);
    assert(!model.weaponReady());
    assert(!model.weaponReadyIndicator());
    for (int step = 0; step < 13; ++step) {
        model.tick({false, false, false, false, false});
    }
    assert(model.shotCount() == 1);
    assert(model.weaponReady());
    assert(model.weaponReadyIndicator());

    stellar7::GameplayModel naturalFailurePresentation;
    for (
        int step = 0;
        step < 2000
            && naturalFailurePresentation.phase()
                == stellar7::GameplayPhase::level1;
        ++step
    ) {
        naturalFailurePresentation.tick({});
    }
    assert(
        naturalFailurePresentation.phase()
        == stellar7::GameplayPhase::failurePreExplosionBlackout
    );
    assert(naturalFailurePresentation.level1State().loopTick() == 1148);
    const auto frozenFailureTick =
        naturalFailurePresentation.level1State().loopTick();
    naturalFailurePresentation.tick({});
    assert(
        naturalFailurePresentation.phase()
        == stellar7::GameplayPhase::failureExplosion
    );
    assert(naturalFailurePresentation.presentationFrame() == 0);
    assert(
        naturalFailurePresentation.failureDebris().size()
        == stellar7::GameplayModel::failureDebrisCount
    );
    assert(naturalFailurePresentation.failureRng().calls() == 2289);
    const auto& firstFailureDebris =
        naturalFailurePresentation.failureDebris().front();
    assert(firstFailureDebris.type == 37);
    assert(firstFailureDebris.collisionRadius == 107);
    assert((
        firstFailureDebris.position
        == stellar7::FixedPosition{-14, 42, 61}
    ));
    assert(firstFailureDebris.pitch == 22902);
    assert(firstFailureDebris.roll == 39497);
    assert(firstFailureDebris.yaw == 35917);
    assert(firstFailureDebris.pitchVelocity == 24581);
    assert(firstFailureDebris.rollVelocity == 24465);
    assert(firstFailureDebris.yawVelocity == -26490);
    assert(firstFailureDebris.xVelocity == -14);
    assert(firstFailureDebris.yVelocity == 42);
    assert(firstFailureDebris.zVelocity == 61);
    assert(naturalFailurePresentation.failureBursts().size() == 1);
    assert(naturalFailurePresentation.failureAudioEvents().size() == 1);
    assert(naturalFailurePresentation.failureAudioEvents().front().eventId == 6);
    assert((
        naturalFailurePresentation.failureBursts().front().position
        == stellar7::FixedPosition{62, -295, 103}
    ));
    const auto emptyFailureDrawTrace =
        stellar7::level1FailureModelDrawTraceJson(
            assets,
            naturalFailurePresentation
        );
    assert(
        emptyFailureDrawTrace
        == "{\"format_version\":1,\"presentation_frame\":0,\"draws\":[]}"
    );
    auto orderedTraceAssets = assets;
    orderedTraceAssets.models.resize(30);
    const stellar7::Model3d traceModel{
        0,
        0,
        0,
        0,
        1,
        1,
        {0, 0, 0},
        {{
            0xffff,
            0,
            {{
                -1,
                0,
                {{
                    {0, 0, 0},
                    {1, 0, 0},
                    {0, 1, 0},
                }},
                {{
                    0,
                    0,
                    {},
                    {{
                        0x80,
                        {7, 7, 7, 7},
                        0,
                        {0, 1, 2},
                    }},
                }},
            }},
        }},
    };
    for (const auto type : {0, 1, 11, 29}) {
        orderedTraceAssets.models[static_cast<std::size_t>(type)] =
            traceModel;
    }
    // Historical zero-radius diagnostic for the still-relaxed rendering
    // gate. Checkpoint 195 supersedes the former native-radius-zero claim.
    auto zeroRadiusGuardianAssets = assets;
    zeroRadiusGuardianAssets.models.resize(45);
    auto zeroRadiusGuardianModel = traceModel;
    zeroRadiusGuardianModel.lods[0].groups[0].vertices = {
        {-128, 0, -128},
        {128, 0, -128},
        {0, 0, 128},
    };
    zeroRadiusGuardianAssets.models[44] =
        std::move(zeroRadiusGuardianModel);
    stellar7::GameplayModel zeroRadiusGuardian;
    stellar7::GameplayModelTestAccess::
        configureLevel2ZeroRadiusGuardianPresentation(zeroRadiusGuardian);
    const auto zeroRadiusGuardianFrame = stellar7::renderGameplayIndexed(
        zeroRadiusGuardianAssets,
        zeroRadiusGuardian
    );
    const auto zeroRadiusGuardianControl = stellar7::renderGameplayIndexed(
        assets,
        zeroRadiusGuardian
    );
    auto zeroRadiusGuardianViewportDifference = std::size_t{0};
    for (int y = 24; y <= 145; ++y) {
        for (int x = 24; x <= 295; ++x) {
            if (
                zeroRadiusGuardianFrame.pixels[
                    static_cast<std::size_t>(y * 320 + x)
                ]
                != zeroRadiusGuardianControl.pixels[
                    static_cast<std::size_t>(y * 320 + x)
                ]
            ) {
                ++zeroRadiusGuardianViewportDifference;
            }
        }
    }
    assert(zeroRadiusGuardianViewportDifference != 0);

    // Native Type 7 points +1c at its inline +4f selector. Group offset zero
    // must use that byte, not force variant zero (cf1c-cf23).
    auto deliveryAnimationAssets = zeroRadiusGuardianAssets;
    auto deliveryAnimationModel = deliveryAnimationAssets.models[44];
    auto& deliveryGroup = deliveryAnimationModel.lods[0].groups[0];
    deliveryGroup.animationOffset = 0;
    deliveryGroup.primitives.push_back(deliveryGroup.primitives.front());
    deliveryGroup.primitives[1].shapes[0].colors = {13, 13, 13, 13};
    deliveryAnimationAssets.models[7] = deliveryAnimationModel;
    stellar7::GameplayModel deliveryAnimation;
    stellar7::GameplayModelTestAccess::configureLevel2DeliveryAnimation(
        deliveryAnimation
    );
    const auto deliveryAnimationFrame = stellar7::renderGameplayIndexed(
        deliveryAnimationAssets, deliveryAnimation
    );
    assert(std::count(deliveryAnimationFrame.pixels.begin(),
                      deliveryAnimationFrame.pixels.end(), 13) > 0);
    // Native Type 25 uses the same inline selector: all six groups select
    // variant 1 at tick 2270, then variant 2 at 2271 (cf79 capture).
    deliveryAnimationAssets.models[25] = deliveryAnimationModel;
    stellar7::GameplayModelTestAccess::configureLevel2DeliveryAnimation(
        deliveryAnimation, 25);
    const auto pickupAnimationFrame = stellar7::renderGameplayIndexed(
        deliveryAnimationAssets, deliveryAnimation);
    assert(std::count(pickupAnimationFrame.pixels.begin(),
                      pickupAnimationFrame.pixels.end(), 13) > 0);

    // Level 2 Type 44 selects LEVEL2.RES local model 0. Keep a distinct
    // global-index-44 color in the fixture so a fallback to the LEVEL1.RES
    // guardian is observable without relying on private assets.
    auto level2GuardianGlobalAssets = zeroRadiusGuardianAssets;
    level2GuardianGlobalAssets.level2Models = {
        level2GuardianGlobalAssets.models[44],
    };
    auto level2GuardianLocalAssets = level2GuardianGlobalAssets;
    level2GuardianLocalAssets.level2Models[0]
        .lods[0].groups[0].primitives[0].shapes[0].colors = {9, 9, 9, 9};
    const auto level2GuardianGlobalFrame = stellar7::renderGameplayIndexed(
        level2GuardianGlobalAssets,
        zeroRadiusGuardian
    );
    const auto level2GuardianLocalFrame = stellar7::renderGameplayIndexed(
        level2GuardianLocalAssets,
        zeroRadiusGuardian
    );
    auto level2GuardianResourceDifference = std::size_t{0};
    for (int y = 24; y <= 145; ++y) {
        for (int x = 24; x <= 295; ++x) {
            if (
                level2GuardianGlobalFrame.pixels[
                    static_cast<std::size_t>(y * 320 + x)
                ]
                != level2GuardianLocalFrame.pixels[
                    static_cast<std::size_t>(y * 320 + x)
                ]
            ) {
                ++level2GuardianResourceDifference;
            }
        }
    }
    assert(level2GuardianResourceDifference != 0);

    // A contemporaneous full-machine Level 3 loop-840 capture registers 44
    // global models followed by four LEVEL3.RES-local models. Native Type 44
    // resolves to local model 0 and reaches its kind-zero dispatcher. Make
    // the local surface distinct so a forced global-model override is visible.
    auto level3GuardianGlobalAssets = zeroRadiusGuardianAssets;
    level3GuardianGlobalAssets.level3Models = {
        level3GuardianGlobalAssets.models[44],
    };
    auto level3GuardianLocalAssets = level3GuardianGlobalAssets;
    level3GuardianLocalAssets.level3Models[0]
        .lods[0].groups[0].primitives[0].shapes[0].colors = {13, 13, 13, 13};
    stellar7::GameplayModel level3LocalGuardian;
    stellar7::GameplayModelTestAccess::configureLevel3LocalGuardianPresentation(
        level3LocalGuardian
    );
    const auto level3GuardianGlobalFrame = stellar7::renderGameplayIndexed(
        level3GuardianGlobalAssets,
        level3LocalGuardian
    );
    const auto level3GuardianLocalFrame = stellar7::renderGameplayIndexed(
        level3GuardianLocalAssets,
        level3LocalGuardian
    );
    auto level3GuardianResourceDifference = std::size_t{0};
    for (int y = 24; y <= 145; ++y) {
        for (int x = 24; x <= 295; ++x) {
            if (
                level3GuardianGlobalFrame.pixels[
                    static_cast<std::size_t>(y * 320 + x)
                ]
                != level3GuardianLocalFrame.pixels[
                    static_cast<std::size_t>(y * 320 + x)
                ]
            ) {
                ++level3GuardianResourceDifference;
            }
        }
    }
    assert(level3GuardianResourceDifference != 0);

    const auto orderedFailureDrawTrace =
        stellar7::level1FailureModelDrawTraceJson(
            orderedTraceAssets,
            naturalFailurePresentation
        );
    assert(
        orderedFailureDrawTrace.find("\"entity_matrix\":{\"values\":[")
        != std::string::npos
    );
    assert(
        orderedFailureDrawTrace.find("\"view_matrix\":{\"values\":[")
        != std::string::npos
    );
    assert(
        orderedFailureDrawTrace.find("\"combined_matrix\":{\"values\":[")
        != std::string::npos
    );
    assert(
        orderedFailureDrawTrace.find("\"viewport\":[")
        != std::string::npos
    );
    std::size_t traceCursor = 0;
    // FUN_1000_cb7a rejects the offscreen type-11 wreck and two type-29
    // debris items before traversal; visible entities retain list order.
    for (const auto type : {29, 29, 1, 1, 0}) {
        const auto marker =
            "\"entity_type\":" + std::to_string(type);
        traceCursor = orderedFailureDrawTrace.find(marker, traceCursor);
        assert(traceCursor != std::string::npos);
        traceCursor += marker.size();
    }
    auto selectionSortAssets = assets;
    selectionSortAssets.models.resize(30);
    auto depthGroup = [](
        const std::int16_t originX,
        const std::uint8_t color
    ) {
        return stellar7::ModelGroup{
            -1,
            0,
            {{
                {originX, 0, 0},
                {0, 0, 1},
                {-10, 0, 0},
                {10, 0, 0},
                {0, 0, 10},
            }},
            {{
                0,
                0,
                {},
                {{
                    0x80,
                    {color, color, color, color},
                    1,
                    {2, 3, 4},
                }},
            }},
        };
    };
    selectionSortAssets.models[0] = {
        0,
        0,
        0,
        0,
        1,
        1,
        {0, 0, 0},
        {{
            0xffff,
            0,
            {
                depthGroup(1700, 7),
                depthGroup(1750, 8),
                depthGroup(1700, 9),
                depthGroup(1600, 10),
            },
        }},
    };
    const auto selectionSortTrace =
        stellar7::level1FailureModelDrawTraceJson(
            selectionSortAssets,
            naturalFailurePresentation
        );
    traceCursor = 0;
    for (const auto color : {10, 9, 7, 8}) {
        const auto marker = "\"color\":" + std::to_string(color);
        traceCursor = selectionSortTrace.find(marker, traceCursor);
        assert(traceCursor != std::string::npos);
        traceCursor += marker.size();
    }
    for (
        unsigned frameIndex = 1;
        frameIndex < stellar7::GameplayModel::failureExplosionFrames;
        ++frameIndex
    ) {
        naturalFailurePresentation.tick({});
        assert(
            naturalFailurePresentation.phase()
            == stellar7::GameplayPhase::failureExplosion
        );
        assert(
            naturalFailurePresentation.presentationFrame() == frameIndex
        );
        assert(
            naturalFailurePresentation.failureAudioEvents().size()
            == std::min<unsigned>(frameIndex + 1, 8)
        );
        assert(
            naturalFailurePresentation.failureAudioEvents().back().eventId
            == 6
        );
        if (frameIndex == 1) {
            const auto& first =
                naturalFailurePresentation.failureDebris().front();
            assert((
                first.position
                == stellar7::FixedPosition{-28, 84, 122}
            ));
            assert(first.pitch == 47483);
            assert(first.roll == 63962);
            assert(first.yaw == 9427);
            assert(first.zVelocity == 56);
            assert(naturalFailurePresentation.failureRng().calls() == 2292);
            assert(
                naturalFailurePresentation.failureBursts().size() == 2
            );
            const auto failureFrame =
                stellar7::renderGameplayIndexed(
                    assets,
                    naturalFailurePresentation
                );
            assert(failureFrame.pixels[91 * 320 + 137] == 123);
            assert(failureFrame.pixels[91 * 320 + 140] == 123);
            assert(failureFrame.pixels[92 * 320 + 137] == 123);
            assert(failureFrame.pixels[92 * 320 + 140] == 123);
            assert(failureFrame.pixels[91 * 320 + 136] != 123);
        }
    }
    const auto& finalFirstFailureDebris =
        naturalFailurePresentation.failureDebris().front();
    assert((
        finalFirstFailureDebris.position
        == stellar7::FixedPosition{-546, 1638, -130}
    ));
    assert(finalFirstFailureDebris.pitch == 39470);
    assert(finalFirstFailureDebris.roll == 51657);
    assert(finalFirstFailureDebris.yaw == 12337);
    assert(finalFirstFailureDebris.pitchVelocity == 12290);
    assert(finalFirstFailureDebris.rollVelocity == 12232);
    assert(finalFirstFailureDebris.yawVelocity == -13245);
    assert(finalFirstFailureDebris.xVelocity == -7);
    assert(finalFirstFailureDebris.yVelocity == 21);
    assert(finalFirstFailureDebris.zVelocity == -10);
    assert(naturalFailurePresentation.failureRng().calls() == 2310);
    assert(naturalFailurePresentation.failureBursts().size() == 8);
    naturalFailurePresentation.tick({});
    assert(
        naturalFailurePresentation.phase()
        == stellar7::GameplayPhase::failurePostExplosionBlackout
    );
    naturalFailurePresentation.tick({});
    assert(
        naturalFailurePresentation.phase()
        == stellar7::GameplayPhase::continuePrompt
    );
    assert(
        naturalFailurePresentation.level1State().loopTick()
        == frozenFailureTick
    );

    auto continuedFailure = naturalFailurePresentation;
    assert(continuedFailure.continueChoice() == stellar7::ContinueChoice::no);
    assert(continuedFailure.remainingContinues() == 2);
    continuedFailure.tick({});
    continuedFailure.tick({false, false, true});
    assert(continuedFailure.continueChoice() == stellar7::ContinueChoice::yes);
    continuedFailure.tick({false, false, true});
    assert(continuedFailure.continueChoice() == stellar7::ContinueChoice::yes);
    stellar7::GameplayInput confirmContinue;
    confirmContinue.confirm = true;
    continuedFailure.tick(confirmContinue);
    assert(continuedFailure.phase() == stellar7::GameplayPhase::continuePrompt);
    assert(continuedFailure.pressedContinueChoice() == stellar7::ContinueChoice::yes);
    auto unsupportedContinue = continuedFailure;
    stellar7::GameplayModelTestAccess::unsupportedContinue(unsupportedContinue);
    bool unsupportedRetryRejected = false;
    try {
        unsupportedContinue.tick({});
    } catch (const std::runtime_error&) {
        unsupportedRetryRejected = true;
    }
    assert(unsupportedRetryRejected);
    assert(unsupportedContinue.remainingContinues() == 2);
    continuedFailure.tick({});
    assert(continuedFailure.phase() == stellar7::GameplayPhase::level1RetryHandoff);
    assert(continuedFailure.playerArmor() == 680);
    assert(continuedFailure.score() == 0);
    assert(continuedFailure.remainingContinues() == 1);
    assert(continuedFailure.level1State().loopTick() == 0);
    assert(continuedFailure.level1State().rng().calls()
        == naturalFailurePresentation.failureRng().calls() + 16);
    assert(continuedFailure.level1State().systemStatusLevels()
        == naturalFailurePresentation.level1State().systemStatusLevels());
    assert(continuedFailure.lastCockpitInitialization().states[0].x
        == naturalFailurePresentation.cockpitIndicatorX());
    continuedFailure.tick({});
    assert(continuedFailure.phase() == stellar7::GameplayPhase::level1);
    assert(continuedFailure.level1State().loopTick() == 0);
    assert(continuedFailure.lastCockpitInitialization().states[3].x
        == continuedFailure.cockpitIndicatorX());
    continuedFailure.tick({});
    assert(continuedFailure.level1State().loopTick() == 1);

    auto declinedFailure = naturalFailurePresentation;
    declinedFailure.tick(confirmContinue);
    declinedFailure.tick({});
    assert(declinedFailure.phase() == stellar7::GameplayPhase::campaignEnded);
    assert(declinedFailure.remainingContinues() == 2);
    const auto endedTick = declinedFailure.ticks();
    declinedFailure.tick({});
    assert(declinedFailure.ticks() == endedTick);
    auto cancelledFailure = naturalFailurePresentation;
    cancelledFailure.tick({false, false, false, false, false, true});
    assert(cancelledFailure.phase() == stellar7::GameplayPhase::campaignEnded);

    for (unsigned frame = 0; frame < 10000
            && continuedFailure.phase() != stellar7::GameplayPhase::continuePrompt; ++frame) {
        continuedFailure.tick({});
    }
    assert(continuedFailure.phase() == stellar7::GameplayPhase::continuePrompt);
    continuedFailure.tick({});
    continuedFailure.tick({false, false, true});
    continuedFailure.tick(confirmContinue);
    continuedFailure.tick({});
    assert(continuedFailure.phase() == stellar7::GameplayPhase::level1RetryHandoff);
    assert(continuedFailure.remainingContinues() == 0);
    for (unsigned frame = 0; frame < 10000
            && continuedFailure.phase() != stellar7::GameplayPhase::campaignEnded; ++frame) {
        continuedFailure.tick({});
        assert(continuedFailure.phase() != stellar7::GameplayPhase::continuePrompt);
    }
    assert(continuedFailure.phase() == stellar7::GameplayPhase::campaignEnded);

    stellar7::GameplayAssets dialogAssets;
    dialogAssets.uiPalette.resize(256);
    dialogAssets.artGroups.resize(1);
    dialogAssets.artGroups[0].resize(23);
    dialogAssets.artGroups[0][0] = {1, 1, {201}};
    dialogAssets.artGroups[0][11] = {1, 1, {202}};
    dialogAssets.artGroups[0][12] = {40, 40, std::vector<std::uint8_t>(1600, 0)};
    dialogAssets.artGroups[0][20] = {264, 2, std::vector<std::uint8_t>(528, 7)};
    dialogAssets.artGroups[0][22] = {1, 2, {203, 204}};
    auto selectedPrompt = naturalFailurePresentation;
    auto promptPixels = stellar7::renderGameplayIndexed(dialogAssets, selectedPrompt);
    assert(promptPixels.pixels[118 * 320 + 161] == 201);
    selectedPrompt.tick(confirmContinue);
    promptPixels = stellar7::renderGameplayIndexed(dialogAssets, selectedPrompt);
    assert(promptPixels.pixels[108 * 320 + 114] == 202);
    assert(promptPixels.pixels[111 * 320 + 120] == 204);
    assert(promptPixels.pixels[112 * 320 + 120] == 203);
    selectedPrompt = naturalFailurePresentation;
    selectedPrompt.tick({});
    selectedPrompt.tick({false, false, true});
    selectedPrompt.tick(confirmContinue);
    promptPixels = stellar7::renderGameplayIndexed(dialogAssets, selectedPrompt);
    assert(promptPixels.pixels[74 * 320 + 161] == 201);
    assert(promptPixels.pixels[77 * 320 + 114] == 202);
    assert(promptPixels.pixels[68 * 320 + 120] == 203);

    stellar7::GameplayInputRecorder continueRecorder;
    continueRecorder.observe(1, confirmContinue);
    continueRecorder.observe(2, {});
    assert(continueRecorder.finish(2).find("1=down.enter\n2=up.enter\n")
        != std::string::npos);

    stellar7::GameplayModel dirtyLevel1;
    for (unsigned tick = 0; tick < 2; ++tick) {
        dirtyLevel1.tick({false, false, false, false, true});
    }
    assert(std::any_of(dirtyLevel1.level1State().projectiles().begin(),
        dirtyLevel1.level1State().projectiles().end(),
        [](const stellar7::Level1Projectile& projectile) {
            return projectile.type != 0 && projectile.collisionRadius != 0;
        }));
    auto diagnosticPoolEntry = dirtyLevel1;
    diagnosticPoolEntry.startLevel2();
    stellar7::GameplayModelTestAccess::assertDiagnosticEntryPools(diagnosticPoolEntry);
    // Controlled presenter completion isolates source reset from natural-route timing.
    stellar7::GameplayModelTestAccess::configureLevel1SuccessProbe(dirtyLevel1);
    dirtyLevel1.setSuccessAudioActive(false);
    dirtyLevel1.tick({});
    assert(dirtyLevel1.phase() == stellar7::GameplayPhase::level2Handoff);
    stellar7::GameplayModelTestAccess::assertCampaignEntryPools(dirtyLevel1);

    stellar7::GameplayModel level2Retry;
    stellar7::GameplayModelTestAccess::configureLevel2RetryDeath(level2Retry);
    for (unsigned frame = 0; frame < stellar7::GameplayModel::failureExplosionFrames + 3
        && level2Retry.phase() != stellar7::GameplayPhase::continuePrompt; ++frame) {
        const bool finalFailureDisplay = level2Retry.phase() == stellar7::GameplayPhase::failureExplosion
            && level2Retry.presentationFrame() + 1 == stellar7::GameplayModel::failureExplosionFrames;
        const auto failureCallsBefore = level2Retry.failureRng().calls();
        level2Retry.tick({});
        if (finalFailureDisplay) {
            assert(level2Retry.phase() == stellar7::GameplayPhase::failurePostExplosionBlackout);
            assert(level2Retry.failureDebris().empty());
            assert(level2Retry.failureRng().calls() == failureCallsBefore);
            assert(level2Retry.level2Entities().size() == 7);
            auto blackoutAssets = assets;
            blackoutAssets.emulateTargetPaletteTiming = true;
            blackoutAssets.cockpitDacPalette.assign(256 * 3, 0);
            blackoutAssets.cockpitDacPalette[0] = 1;
            blackoutAssets.cockpitDacPalette[1] = 2;
            blackoutAssets.cockpitDacPalette[2] = 3;
            blackoutAssets.cockpitDacPalette[17 * 3] = 22;
            blackoutAssets.cockpitDacPalette[17 * 3 + 1] = 43;
            blackoutAssets.cockpitDacPalette[17 * 3 + 2] = 63;
            const auto sourcePaletteBefore = blackoutAssets.cockpitDacPalette;
            const auto blackoutFrame = stellar7::renderGameplayIndexed(blackoutAssets, level2Retry);
            // Separately truncated source/target terms differ from one combined division.
            assert((blackoutFrame.palette[17] == stellar7::Rgb{4, 12, 20}));
            assert(std::all_of(blackoutFrame.pixels.begin(), blackoutFrame.pixels.end(),
                [](const std::uint8_t pixel) { return pixel == 0; }));
            assert(blackoutAssets.cockpitDacPalette == sourcePaletteBefore);
            assert(level2Retry.failureRng().calls() == failureCallsBefore);
            blackoutAssets.cockpitDacPalette[17 * 3] = 64;
            bool invalidBlackoutPaletteRejected = false;
            try {
                stellar7::renderGameplayIndexed(blackoutAssets, level2Retry);
            } catch (const std::runtime_error&) {
                invalidBlackoutPaletteRejected = true;
            }
            assert(invalidBlackoutPaletteRejected);
            blackoutAssets.cockpitDacPalette = sourcePaletteBefore;
            blackoutAssets.cockpitDacPalette.pop_back();
            bool shortBlackoutPaletteRejected = false;
            try {
                stellar7::renderGameplayIndexed(blackoutAssets, level2Retry);
            } catch (const std::runtime_error&) {
                shortBlackoutPaletteRejected = true;
            }
            assert(shortBlackoutPaletteRejected);
        }
    }
    assert(level2Retry.phase() == stellar7::GameplayPhase::continuePrompt);
    auto expectedRetryRng = level2Retry.failureRng();
    constexpr std::size_t level2ActorDraws = 3 * 2 + 2 * 3;
    for (std::size_t draw = 0; draw < level2ActorDraws; ++draw) {
        expectedRetryRng.next();
    }
    stellar7::Level1GroundState expectedRetryGround(
        std::array<stellar7::GroundPoint, stellar7::Level1GroundState::pointCount>{});
    expectedRetryGround.advance(0, 0, expectedRetryRng);
    expectedRetryGround.advance(0, 0, expectedRetryRng);
    expectedRetryRng.next(); // Source tick-zero scheduler selection after display.
    const auto retryIndicatorBefore = level2Retry.cockpitIndicatorX();
    const auto retryStatuses = level2Retry.level1State().systemStatusLevels();
    level2Retry.tick({});
    level2Retry.tick({false, false, true});
    level2Retry.tick(confirmContinue);
    auto heldInputRetry = level2Retry;
    bool heldRetryRejected = false;
    try {
        heldInputRetry.tick({true});
    } catch (const std::runtime_error&) {
        heldRetryRejected = true;
    }
    assert(heldRetryRejected);
    assert(heldInputRetry.remainingContinues() == 2);
    assert(heldInputRetry.level2Rng().calls() == level2Retry.level2Rng().calls());
    assert(heldInputRetry.playerArmor() == level2Retry.playerArmor());
    level2Retry.tick({});
    assert(level2Retry.phase() == stellar7::GameplayPhase::level2Handoff);
    assert(level2Retry.playerArmor() == 680 && level2Retry.score() == 0);
    assert(level2Retry.remainingContinues() == 1 && level2Retry.level2LoopTick() == 0);
    assert(level2Retry.level1State().systemStatusLevels() == retryStatuses);
    assert(level2Retry.level2Ground().points() == expectedRetryGround.points());
    auto actualRetryRng = level2Retry.level2Rng();
    assert(actualRetryRng.calls() == expectedRetryRng.calls());
    for (std::size_t draw = 0; draw < stellar7::StellarRng::wordCount; ++draw) {
        assert(actualRetryRng.next() == expectedRetryRng.next());
    }
    assert(level2Retry.lastCockpitInitialization().states[0].x == retryIndicatorBefore);
    assert(level2Retry.lastCockpitInitialization().states[3].x
        == level2Retry.cockpitIndicatorX());
    stellar7::GameplayModelTestAccess::assertLevel2RetryStreams(level2Retry);
    auto retryPools = level2Retry;
    stellar7::GameplayModelTestAccess::assertCampaignEntryPools(retryPools);
    level2Retry.tick({});
    assert(level2Retry.phase() == stellar7::GameplayPhase::level2);
    assert(level2Retry.level2LoopTick() == 1);

    stellar7::GameplayModel deferredCounterFailure;
    stellar7::GameplayModelTestAccess::configureFailureCounter(deferredCounterFailure,
        std::numeric_limits<std::uint16_t>::max()-1,
        stellar7::GameplayModelTestAccess::FailureTail::deferred);
    deferredCounterFailure.tick({});
    stellar7::GameplayModelTestAccess::assertFailureCounter(deferredCounterFailure, 0);
    for (unsigned frame = 1; frame < stellar7::GameplayModel::failureExplosionFrames; ++frame) {
        deferredCounterFailure.tick({});
        stellar7::GameplayModelTestAccess::assertFailureCounter(deferredCounterFailure,
            static_cast<std::uint16_t>(frame));
    }
    const auto finalFailureRngCalls = deferredCounterFailure.failureRng().calls();
    deferredCounterFailure.tick({});
    assert(deferredCounterFailure.failureDebris().empty());
    deferredCounterFailure.tick({});
    assert(deferredCounterFailure.phase() == stellar7::GameplayPhase::continuePrompt);
    stellar7::GameplayModelTestAccess::assertFailureCounter(deferredCounterFailure, 44);
    assert(deferredCounterFailure.failureRng().calls() == finalFailureRngCalls);

    stellar7::GameplayModel consumedCounterFailure;
    stellar7::GameplayModelTestAccess::configureFailureCounter(consumedCounterFailure, 101,
        stellar7::GameplayModelTestAccess::FailureTail::consumed);
    consumedCounterFailure.tick({});
    stellar7::GameplayModelTestAccess::assertFailureCounter(consumedCounterFailure, 102);
    stellar7::GameplayModelTestAccess::configureUnsupportedFailureCounter(consumedCounterFailure);
    consumedCounterFailure.tick({});
    stellar7::GameplayModelTestAccess::assertUnknownFailureCounter(consumedCounterFailure);

    stellar7::GameplayModel level1SuccessLifetime;
    stellar7::GameplayModelTestAccess::configureLevel1SuccessProbe(level1SuccessLifetime);
    const auto frozenSuccessLoopTick = level1SuccessLifetime.level1State().loopTick();
    level1SuccessLifetime.setSuccessAudioActive(true);
    for (unsigned frame = 0; frame < 250; ++frame) {
        level1SuccessLifetime.tick({});
    }
    assert(level1SuccessLifetime.phase()
        == stellar7::GameplayPhase::successPresentation);
    assert(level1SuccessLifetime.level1State().loopTick() == frozenSuccessLoopTick);
    level1SuccessLifetime.setSuccessAudioActive(false);
    level1SuccessLifetime.tick({});
    assert(level1SuccessLifetime.phase() == stellar7::GameplayPhase::level2Handoff);

    stellar7::GameplayModel level2SuccessLifetime;
    stellar7::GameplayModelTestAccess::configureLevel2SuccessProbe(level2SuccessLifetime);
    stellar7::GameplayModelTestAccess::setLevel2Score(level2SuccessLifetime, 2500);
    // Original 7078 tests the active sound handle. The decoded Sound Blaster
    // success sequence ends after sample252645 at44100Hz on the recovered
    // PIT clock. At1455 samples/frame it needs174 frames.
    for (unsigned frame = 1; frame < 174; ++frame) {
        level2SuccessLifetime.tick({});
        assert(level2SuccessLifetime.phase()
            == stellar7::GameplayPhase::level2SuccessPresentation);
    }
    level2SuccessLifetime.tick({});
    assert(level2SuccessLifetime.phase() == stellar7::GameplayPhase::level3Handoff);
    assert(level2SuccessLifetime.score() == 2500);
    stellar7::GameplayModelTestAccess::configureLevel2SuccessProbe(level2SuccessLifetime);
    level2SuccessLifetime.setSuccessAudioActive(true);
    for (unsigned frame = 0; frame < 250; ++frame) {
        level2SuccessLifetime.tick({});
    }
    assert(level2SuccessLifetime.phase()
        == stellar7::GameplayPhase::level2SuccessPresentation);
    assert(level2SuccessLifetime.level2LoopTick() == 0);
    level2SuccessLifetime.setSuccessAudioActive(false);
    level2SuccessLifetime.tick({});
    assert(level2SuccessLifetime.phase() == stellar7::GameplayPhase::level3Handoff);

    // Native Level 3 loop-zero and first-16 checkpoint series. The route
    // carries Left+Up for tick 1, then releases both before tick 2.
    stellar7::GameplayModel level3Entry;
    level3Entry.startLevel3();
    assert(
        level3Entry.phase() == stellar7::GameplayPhase::level3Handoff
    );
    assert(level3Entry.level2LoopTick() == 0);
    assert(level3Entry.playerArmor() == 680);
    assert(level3Entry.score() == 2740);
    assert(level3Entry.level2Entities().size() == 6);
    assert(level3Entry.level2Player().weaponSlotCount == 0);
    assert((
        level3Entry.level2Ground().points()[0]
        == stellar7::GroundPoint{6016, 1632}
    ));
    assert(level3Entry.level2Rng().indexA() == 32);
    assert(level3Entry.level2Rng().indexB() == 94);
    assert(level3Entry.level2Rng().last() == 27136);
    assert(level3Entry.level2Rng().calls() == 8);

    // Native level3-fire-call-entry-v3 proves that FUN_1000:05d4 ignores
    // the player record's zero weapon-slot-count byte.  With Space held at
    // the exact Level 3 entry boundary, loop 1 contains a Type 23 shot and
    // the two fixed cannon timer bytes read 31,0 after the ordinary
    // end-of-boundary decrement.
    stellar7::GameplayModel level3FireEntry;
    level3FireEntry.startLevel3();
    level3FireEntry.tick({false, false, false, false, true, false});
    assert(level3FireEntry.level2LoopTick() == 1);
    assert(level3FireEntry.level2Player().weaponSlotCount == 0);
    assert(level3FireEntry.level2Player().weaponCooldown == 8);
    assert(level3FireEntry.level2Player().weaponSlotTimers[0] == 31);
    assert(level3FireEntry.level2Player().weaponSlotTimers[1] == 0);
    assert(level3FireEntry.level2Entities().size() == 7);
    assert(level3FireEntry.level2Entities()[6].type == 23);
    assert(level3FireEntry.level2Entities()[6].collisionRadius == 0);
    assert(level3FireEntry.level2Entities()[6].forwardVelocity == 500);
    assert(level3FireEntry.level2Entities()[6].stateBytes4dTo52[0] == 0xfd);
    assert(level3FireEntry.level2Entities()[6].stateBytes4dTo52[1] == 64);
    const auto& level3FireProjectilePool =
        stellar7::GameplayModelTestAccess::level2ProjectilePoolActive(
            level3FireEntry
        );
    assert(level3FireProjectilePool[0]);
    assert(!level3FireProjectilePool[1]);

    level3Entry.tick({true, false, true, false, false, false});
    assert(level3Entry.phase() == stellar7::GameplayPhase::level3);
    assert(level3Entry.level2LoopTick() == 1);
    assert((
        level3Entry.level2Player().position
        == stellar7::FixedPosition{-1, 9, 0}
    ));
    assert(level3Entry.level2Player().yaw == 80);
    assert(level3Entry.level2Player().yawVelocity == 80);
    assert(level3Entry.level2Player().forwardVelocity == 10);
    assert((
        level3Entry.level2ActiveListOrder()
        == std::vector<std::size_t>{3, 1, 2, 4, 5, 0}
    ));
    assert(level3Entry.level2Entities()[0].type == 10);
    assert(level3Entry.level2Entities()[0].yaw == 63744);
    assert(level3Entry.level2Entities()[3].type == 2);
    assert((
        level3Entry.level2Entities()[3].position
        == stellar7::FixedPosition{16240, 24576, 0}
    ));
    assert(level3Entry.level2Entities()[3].yaw == 16384);
    assert(level3Entry.level2Entities()[3].yawVelocity == 271);
    assert(level3Entry.level2Entities()[3].forwardVelocity == 0);
    assert(level3Entry.level2Rng().calls() == 19);

    level3Entry.tick({});
    assert(level3Entry.level2LoopTick() == 2);
    assert((
        level3Entry.level2Player().position
        == stellar7::FixedPosition{0, 0, 0}
    ));
    assert(level3Entry.level2Player().yaw == 80);
    assert(level3Entry.level2Player().yawVelocity == 0);
    assert(level3Entry.level2Player().forwardVelocity == 0);
    assert((
        level3Entry.level2Entities()[0].position
        == stellar7::FixedPosition{-16383, -8201, 0}
    ));
    assert(level3Entry.level2Rng().calls() == 25);

    for (unsigned tick = 3; tick <= 16; ++tick) {
        level3Entry.tick({});
    }
    assert(level3Entry.level2LoopTick() == 16);
    assert(level3Entry.level2Entities()[0].yaw == 36864);
    assert(level3Entry.level2Entities()[1].yaw == 36864);
    assert(level3Entry.level2Entities()[2].yaw == 32768);
    assert(level3Entry.level2Entities()[3].yaw == 20449);
    assert(level3Entry.level2Entities()[3].stateBytes4dTo52[5] == 3);
    assert(level3Entry.level2Rng().indexA() == 68);
    assert(level3Entry.level2Rng().indexB() == 18);
    assert(level3Entry.level2Rng().last() == 62383);
    assert(level3Entry.level2Rng().calls() == 46);

    for (unsigned tick = 17; tick <= 25; ++tick) {
        level3Entry.tick({});
    }
    assert(level3Entry.level2LoopTick() == 25);
    assert(level3Entry.level2Entities().size() == 6);
    assert(level3Entry.level2Entities()[0].yaw == 20736);
    assert(level3Entry.level2Entities()[1].yaw == 16128);
    assert(level3Entry.level2Entities()[2].yaw == 51200);
    assert(level3Entry.level2Entities()[3].yaw == 22888);
    assert(level3Entry.level2Rng().indexA() == 48);
    assert(level3Entry.level2Rng().indexB() == 110);
    assert(level3Entry.level2Rng().last() == 15134);

    for (unsigned tick = 26; tick <= 50; ++tick) {
        level3Entry.tick({});
    }
    assert(level3Entry.level2LoopTick() == 50);
    assert(level3Entry.level2Entities().size() == 9);
    assert((
        level3Entry.level2ActiveListOrder()
        == std::vector<std::size_t>{0, 1, 2, 3, 4, 5, 6, 7, 8}
    ));
    assert(level3Entry.level2Entities()[0].yaw == 41472);
    assert(level3Entry.level2Entities()[0].stateBytes4dTo52[5] == 3);
    assert(level3Entry.level2Entities()[0].behaviorTimer == 63);
    assert(level3Entry.level2Entities()[0].weaponSlotTimers[0] == 66);
    assert(level3Entry.level2Entities()[3].yaw == 26688);
    assert(level3Entry.level2Entities()[3].yawVelocity == 0);
    assert(level3Entry.level2Entities()[6].type == 18);
    assert((
        level3Entry.level2Entities()[6].position
        == stellar7::FixedPosition{-13261, -6633, 0}
    ));
    assert(level3Entry.level2Entities()[6].stateBytes4dTo52[1] == 59);
    assert(level3Entry.level2Entities()[7].type == 18);
    assert(level3Entry.level2Entities()[7].stateBytes4dTo52[1] == 64);
    assert(level3Entry.level2Entities()[8].type == 18);
    assert(level3Entry.level2Entities()[8].stateBytes4dTo52[1] == 66);
    assert(level3Entry.level2Rng().indexA() == 102);
    assert(level3Entry.level2Rng().indexB() == 52);
    assert(level3Entry.level2Rng().last() == 8786);

    for (unsigned tick = 51; tick <= 75; ++tick) {
        level3Entry.tick({});
    }
    assert(level3Entry.level2LoopTick() == 75);
    assert(level3Entry.level2Entities().size() == 9);
    assert((
        level3Entry.level2ActiveListOrder()
        == std::vector<std::size_t>{3, 1, 2, 4, 5, 0, 8, 7, 6}
    ));
    assert(level3Entry.level2Entities()[0].behaviorTimer == 38);
    assert(level3Entry.level2Entities()[6].stateBytes4dTo52[1] == 34);
    assert(level3Entry.level2Entities()[7].stateBytes4dTo52[1] == 39);
    assert(level3Entry.level2Entities()[8].stateBytes4dTo52[1] == 41);
    assert(level3Entry.level2Rng().indexA() == 50);
    assert(level3Entry.level2Rng().indexB() == 0);
    assert(level3Entry.level2Rng().last() == 54556);

    for (unsigned tick = 76; tick <= 100; ++tick) {
        level3Entry.tick({});
    }
    assert(level3Entry.level2LoopTick() == 100);
    assert(level3Entry.level2Entities().size() == 9);
    assert((
        level3Entry.level2ActiveListOrder()
        == std::vector<std::size_t>{3, 1, 2, 4, 5, 8, 7, 0, 6}
    ));
    assert(level3Entry.level2Entities()[0].behaviorTimer == 13);
    assert(level3Entry.level2Entities()[6].stateBytes4dTo52[1] == 9);
    assert(level3Entry.level2Entities()[7].stateBytes4dTo52[1] == 14);
    assert(level3Entry.level2Entities()[8].stateBytes4dTo52[1] == 16);
    assert(level3Entry.level2Rng().indexA() == 108);
    assert(level3Entry.level2Rng().indexB() == 58);
    assert(level3Entry.level2Rng().last() == 13110);

    for (unsigned tick = 101; tick <= 125; ++tick) {
        level3Entry.tick({});
    }
    assert(level3Entry.level2LoopTick() == 125);
    assert(level3Entry.level2Entities().size() == 7);
    assert((
        level3Entry.level2ActiveListOrder()
        == std::vector<std::size_t>{3, 1, 2, 4, 5, 0, 6}
    ));
    assert(level3Entry.level2Entities()[0].stateBytes4dTo52[5] == 3);
    assert(level3Entry.level2Entities()[0].behaviorTimer == 0);
    assert(level3Entry.level2Entities()[6].type == 18);
    assert((
        level3Entry.level2Entities()[6].position
        == stellar7::FixedPosition{-12423, -6086, 0}
    ));
    assert(level3Entry.level2Entities()[6].yaw == 54272);
    assert(level3Entry.level2Entities()[6].stateBytes4dTo52[1] == 57);
    assert(level3Entry.level2Rng().indexA() == 54);
    assert(level3Entry.level2Rng().indexB() == 4);
    assert(level3Entry.level2Rng().last() == 43711);

    for (unsigned tick = 126; tick <= 150; ++tick) {
        level3Entry.tick({});
    }
    assert(level3Entry.level2LoopTick() == 150);
    assert(level3Entry.level2Entities().size() == 7);
    assert(level3Entry.level2Entities()[0].stateBytes4dTo52[5] == 0);
    assert(level3Entry.level2Entities()[0].behaviorTimer == 0);
    assert(level3Entry.level2Entities()[0].yaw == 58880);
    assert(level3Entry.level2Entities()[0].targetHeading == 53984);
    assert(level3Entry.level2Entities()[0].presenterDistance == 19458);
    assert((
        level3Entry.level2Entities()[6].position
        == stellar7::FixedPosition{-1423, -211, 0}
    ));
    assert(level3Entry.level2Entities()[6].stateBytes4dTo52[1] == 32);
    const auto& level3Tick150ProjectilePool =
        stellar7::GameplayModelTestAccess::level2ProjectilePoolActive(
            level3Entry
        );
    assert(level3Tick150ProjectilePool[0]);
    assert(!level3Tick150ProjectilePool[1]);
    assert(!level3Tick150ProjectilePool[2]);
    assert(level3Entry.level2Rng().indexA() == 110);
    assert(level3Entry.level2Rng().indexB() == 60);
    assert(level3Entry.level2Rng().last() == 57010);

    for (unsigned tick = 151; tick <= 175; ++tick) {
        level3Entry.tick({});
    }
    assert(level3Entry.level2LoopTick() == 175);
    assert(level3Entry.level2Entities().size() == 9);
    assert((
        level3Entry.level2ActiveListOrder()
        == std::vector<std::size_t>{3, 1, 2, 4, 5, 0, 8, 6, 7}
    ));
    assert(level3Entry.level2Entities()[0].stateBytes4dTo52[5] == 3);
    assert(level3Entry.level2Entities()[0].behaviorTimer == 48);
    assert((
        level3Entry.level2Entities()[6].position
        == stellar7::FixedPosition{9577, 5664, 0}
    ));
    assert(level3Entry.level2Entities()[6].stateBytes4dTo52[1] == 7);
    assert((
        level3Entry.level2Entities()[7].position
        == stellar7::FixedPosition{-6329, -3757, 0}
    ));
    assert(level3Entry.level2Entities()[7].yaw == 53504);
    assert(level3Entry.level2Entities()[7].stateBytes4dTo52[1] == 44);
    assert((
        level3Entry.level2Entities()[8].position
        == stellar7::FixedPosition{-8716, -11839, 0}
    ));
    assert(level3Entry.level2Entities()[8].yaw == 44544);
    assert(level3Entry.level2Entities()[8].stateBytes4dTo52[1] == 49);
    assert(level3Entry.level2Rng().indexA() == 54);
    assert(level3Entry.level2Rng().indexB() == 4);
    assert(level3Entry.level2Rng().last() == 36204);

    for (unsigned tick = 176; tick <= 200; ++tick) {
        level3Entry.tick({});
    }
    assert(level3Entry.level2LoopTick() == 200);
    assert(level3Entry.level2Entities().size() == 8);
    assert((
        level3Entry.level2ActiveListOrder()
        == std::vector<std::size_t>{3, 1, 2, 4, 5, 0, 7, 6}
    ));
    assert(level3Entry.level2Entities()[0].stateBytes4dTo52[5] == 3);
    assert(level3Entry.level2Entities()[0].behaviorTimer == 23);
    assert(level3Entry.level2Entities()[3].stateBytes4dTo52[5] == 0);
    assert((
        level3Entry.level2Entities()[6].position
        == stellar7::FixedPosition{5096, 1293, 0}
    ));
    assert(level3Entry.level2Entities()[6].yaw == 53504);
    assert(level3Entry.level2Entities()[6].stateBytes4dTo52[1] == 19);
    assert((
        level3Entry.level2Entities()[7].position
        == stellar7::FixedPosition{2559, -17189, 0}
    ));
    assert(level3Entry.level2Entities()[7].yaw == 44544);
    assert(level3Entry.level2Entities()[7].stateBytes4dTo52[1] == 24);
    assert(level3Entry.level2Rng().indexA() == 0);
    assert(level3Entry.level2Rng().indexB() == 62);
    assert(level3Entry.level2Rng().last() == 44801);

    // Native natural-route continuation v14 resumes the exact loop-200
    // state and stops at the complete 0x850c semantic boundary for loop
    // 250. No input transitions occur after loop 1 on this route.
    for (unsigned tick = 201; tick <= 250; ++tick) {
        level3Entry.tick({});
    }
    assert(level3Entry.level2LoopTick() == 250);
    assert(level3Entry.playerArmor() == 680);
    assert(level3Entry.score() == 2740);
    assert(level3Entry.level2Entities().size() == 9);
    assert((
        level3Entry.level2ActiveListOrder()
        == std::vector<std::size_t>{3, 1, 2, 4, 5, 0, 8, 7, 6}
    ));
    assert((
        level3Entry.level2Player().position
        == stellar7::FixedPosition{0, 0, 0}
    ));
    assert(level3Entry.level2Player().yaw == 80);
    assert(level3Entry.level2Player().yawVelocity == 0);
    assert(level3Entry.level2Player().forwardVelocity == 0);

    const auto& level3Tick250Type10 = level3Entry.level2Entities()[0];
    assert(level3Tick250Type10.type == 10);
    assert((
        level3Tick250Type10.position
        == stellar7::FixedPosition{-16383, -8201, 0}
    ));
    assert(level3Tick250Type10.yaw == 10752);
    assert(level3Tick250Type10.yawVelocity == -1792);
    assert(level3Tick250Type10.behaviorTimer == 46);
    assert(level3Tick250Type10.weaponSlotTimers[0] == 66);
    assert(level3Tick250Type10.targetHeading == 53984);
    assert(level3Tick250Type10.presenterDistance == 19458);

    const auto& level3Tick250Type2 = level3Entry.level2Entities()[3];
    assert(level3Tick250Type2.type == 2);
    assert((
        level3Tick250Type2.position
        == stellar7::FixedPosition{16241, 24567, 0}
    ));
    assert(level3Tick250Type2.yaw == 26688);
    assert(level3Tick250Type2.forwardVelocity == 0);

    assert(level3Entry.level2Entities()[4].type == 24);
    assert((
        level3Entry.level2Entities()[4].position
        == stellar7::FixedPosition{-20479, -12297, 0}
    ));
    assert(level3Entry.level2Entities()[5].type == 31);
    assert((
        level3Entry.level2Entities()[5].position
        == stellar7::FixedPosition{-20479, -12297, 0}
    ));
    assert(level3Entry.level2Entities()[6].type == 18);
    assert((
        level3Entry.level2Entities()[6].position
        == stellar7::FixedPosition{-5559, -3089, 0}
    ));
    assert(level3Entry.level2Entities()[6].yaw == 53760);
    assert(level3Entry.level2Entities()[6].forwardVelocity == 500);
    assert(level3Entry.level2Entities()[6].stateBytes4dTo52[1] == 42);
    assert(level3Entry.level2Entities()[7].type == 18);
    assert((
        level3Entry.level2Entities()[7].position
        == stellar7::FixedPosition{-7700, -12058, 0}
    ));
    assert(level3Entry.level2Entities()[7].yaw == 44800);
    assert(level3Entry.level2Entities()[7].stateBytes4dTo52[1] == 47);
    assert(level3Entry.level2Entities()[8].type == 18);
    assert((
        level3Entry.level2Entities()[8].position
        == stellar7::FixedPosition{-16604, 282, 0}
    ));
    assert(level3Entry.level2Entities()[8].yaw == 256);
    assert(level3Entry.level2Entities()[8].stateBytes4dTo52[1] == 49);
    assert((
        level3Entry.level2Ground().points()
        == std::array<stellar7::GroundPoint, 6>{
            stellar7::GroundPoint{3072, 4512},
            stellar7::GroundPoint{2167, 4102},
            stellar7::GroundPoint{7632, 3877},
            stellar7::GroundPoint{5696, 3360},
            stellar7::GroundPoint{2912, 4096},
            stellar7::GroundPoint{5984, 3200},
        }
    ));
    assert(level3Entry.level2Rng().indexA() == 110);
    assert(level3Entry.level2Rng().indexB() == 60);
    assert(level3Entry.level2Rng().last() == 64864);
    assert(level3Entry.level2Rng().calls() == 305);

    // Native natural-route continuation v15 advances the same saved state
    // to loop 300. This boundary includes the retirement of two Type 18
    // projectiles and the Type 2 actor's next movement state.
    for (unsigned tick = 251; tick <= 300; ++tick) {
        level3Entry.tick({});
    }
    assert(level3Entry.level2LoopTick() == 300);
    assert(level3Entry.level2Entities().size() == 7);
    assert((
        level3Entry.level2ActiveListOrder()
        == std::vector<std::size_t>{1, 2, 3, 4, 5, 0, 6}
    ));
    assert((
        level3Entry.level2Player().position
        == stellar7::FixedPosition{0, 0, 0}
    ));
    assert(level3Entry.level2Player().yaw == 80);
    assert(level3Entry.level2Player().yawVelocity == 0);
    assert(level3Entry.level2Player().forwardVelocity == 0);
    assert(level3Entry.level2Entities()[0].type == 10);
    assert(level3Entry.level2Entities()[0].yaw == 52224);
    assert(level3Entry.level2Entities()[0].yawVelocity == -1792);
    assert(level3Entry.level2Entities()[0].stateBytes4dTo52[5] == 1);
    assert(level3Entry.level2Entities()[0].behaviorTimer == 3);
    assert(level3Entry.level2Entities()[1].type == 10);
    assert(level3Entry.level2Entities()[1].yaw == 37888);
    assert(level3Entry.level2Entities()[2].type == 10);
    assert(level3Entry.level2Entities()[2].yaw == 24576);
    assert(level3Entry.level2Entities()[3].type == 2);
    assert((
        level3Entry.level2Entities()[3].position
        == stellar7::FixedPosition{16593, 20824, 0}
    ));
    assert(level3Entry.level2Entities()[3].yaw == 40408);
    assert(level3Entry.level2Entities()[3].yawVelocity == 490);
    assert(level3Entry.level2Entities()[3].forwardVelocity == 144);
    assert(level3Entry.level2Entities()[3].stateBytes4dTo52[5] == 1);
    assert(level3Entry.level2Entities()[3].behaviorTimer == 4);
    assert(level3Entry.level2Entities()[6].type == 18);
    assert((
        level3Entry.level2Entities()[6].position
        == stellar7::FixedPosition{-15937, -7977, 0}
    ));
    assert(level3Entry.level2Entities()[6].yaw == 54016);
    assert(level3Entry.level2Entities()[6].forwardVelocity == 500);
    assert(level3Entry.level2Entities()[6].stateBytes4dTo52[1] == 65);
    assert(level3Entry.level2Rng().indexA() == 58);
    assert(level3Entry.level2Rng().indexB() == 8);
    assert(level3Entry.level2Rng().last() == 12991);
    assert(level3Entry.level2Rng().calls() == 331);

    // Native natural-route continuation v16 reaches loop 400. It covers a
    // fresh three-projectile volley, Type 2 weapon timing, and another Type
    // 10 behavior cycle without replaying the preceding campaign levels.
    for (unsigned tick = 301; tick <= 400; ++tick) {
        level3Entry.tick({});
    }
    assert(level3Entry.level2LoopTick() == 400);
    assert(level3Entry.level2Entities().size() == 10);
    assert((
        level3Entry.level2ActiveListOrder()
        == std::vector<std::size_t>{1, 2, 4, 5, 0, 3, 9, 7, 8, 6}
    ));
    assert(level3Entry.level2Entities()[0].type == 10);
    assert(level3Entry.level2Entities()[0].yaw == 4096);
    assert(level3Entry.level2Entities()[0].stateBytes4dTo52[5] == 3);
    assert(level3Entry.level2Entities()[0].behaviorTimer == 42);
    assert(level3Entry.level2Entities()[1].yaw == 4096);
    assert(level3Entry.level2Entities()[2].yaw == 32768);
    assert(level3Entry.level2Entities()[3].type == 2);
    assert((
        level3Entry.level2Entities()[3].position
        == stellar7::FixedPosition{14494, 9556, 0}
    ));
    assert(level3Entry.level2Entities()[3].yaw == 22448);
    assert(level3Entry.level2Entities()[3].yawVelocity == 0);
    assert(level3Entry.level2Entities()[3].forwardVelocity == 144);
    assert(level3Entry.level2Entities()[3].stateBytes4dTo52[5] == 3);
    assert(level3Entry.level2Entities()[3].behaviorTimer == 1);
    assert(level3Entry.level2Entities()[3].weaponSlotTimers[0] == 45);
    const std::array<stellar7::FixedPosition, 4>
        level3Tick400ProjectilePositions{
            stellar7::FixedPosition{-4063, -1621, 0},
            stellar7::FixedPosition{-5665, -12341, 0},
            stellar7::FixedPosition{8445, 5168, 0},
            stellar7::FixedPosition{-17160, 2257, 0},
        };
    const std::array<std::uint16_t, 4> level3Tick400ProjectileYaw{
        54272, 45312, 22804, 768,
    };
    const std::array<std::uint8_t, 4> level3Tick400ProjectileLifetime{
        38, 43, 45, 45,
    };
    for (std::size_t index = 0; index < 4; ++index) {
        const auto& projectile = level3Entry.level2Entities()[6 + index];
        assert(projectile.type == 18);
        assert(projectile.position == level3Tick400ProjectilePositions[index]);
        assert(projectile.yaw == level3Tick400ProjectileYaw[index]);
        assert(projectile.forwardVelocity == 500);
        assert(
            projectile.stateBytes4dTo52[1]
            == level3Tick400ProjectileLifetime[index]
        );
    }
    assert(level3Entry.level2Rng().indexA() == 68);
    assert(level3Entry.level2Rng().indexB() == 18);
    assert(level3Entry.level2Rng().last() == 54803);
    assert(level3Entry.level2Rng().calls() == 438);

    // Native forced-frontier capture v6: with DS:12aa poked to zero, the
    // first bounded scheduler allocation is Type 12 on the loop-32 boundary.
    stellar7::GameplayModel level3ForcedSpawn;
    stellar7::GameplayModelTestAccess::configureLevel3ForcedSpawnFrontier(
        level3ForcedSpawn
    );
    assert(level3ForcedSpawn.level2LoopTick() == 33);
    assert(level3ForcedSpawn.level2Entities().size() == 7);
    assert((
        level3ForcedSpawn.level2ActiveListOrder()
        == std::vector<std::size_t>{0, 1, 2, 3, 5, 6, 4}
    ));
    const auto& allocatedLevel3Type12 = level3ForcedSpawn.level2Entities()[4];
    assert(allocatedLevel3Type12.type == 12);
    assert((
        allocatedLevel3Type12.position
        == stellar7::FixedPosition{-32769, -1, 1000}
    ));
    assert(allocatedLevel3Type12.forwardVelocity == 360);
    assert(allocatedLevel3Type12.collisionRadius == 0);
    assert(allocatedLevel3Type12.stateBytes4dTo52 == (
        std::array<std::uint8_t, 6>{255, 1, 5, 0, 3, 0}
    ));
    assert(allocatedLevel3Type12.weaponSlotCount == 1);
    assert(
        stellar7::GameplayModelTestAccess::level2ActiveEnemyCount(
            level3ForcedSpawn
        ) == 1
    );
    assert(
        stellar7::GameplayModelTestAccess::level2SpawnedEnemyCount(
            level3ForcedSpawn
        ) == 5
    );
    assert(level3ForcedSpawn.level2Entities().size() == 7);
    const auto& level3Type12 = level3ForcedSpawn.level2Entities()[4];
    assert(level3Type12.type == 12);
    assert(level3Type12.collisionRadius == 0);
    assert(level3Type12.stateBytes4dTo52 == (
        std::array<std::uint8_t, 6>{255, 1, 5, 0, 3, 0}
    ));
    assert(level3Type12.weaponSlotCount == 1);

    // Native v6 loop-32 context validates the first dense Level 3 spawn
    // boundary. The held Left+Up input from the native dense capture is
    // replayed through the v5 loop-100 boundary to pin the shared callback,
    // projectile-pool, list-mutation, and RNG chronology.
    stellar7::GameplayModel level3DenseSpawn;
    stellar7::GameplayModelTestAccess::configureLevel3DenseSpawnFrontier(
        level3DenseSpawn
    );
    const stellar7::GameplayInput level3DenseHeldInput{
        true, false, true, false, false, false,
    };
    level3DenseSpawn.tick(level3DenseHeldInput);
    assert(level3DenseSpawn.level2LoopTick() == 33);
    assert(level3DenseSpawn.level2Entities().size() == 7);
    assert((
        level3DenseSpawn.level2ActiveListOrder()
        == std::vector<std::size_t>{0, 1, 2, 3, 5, 6, 4}
    ));
    assert(level3DenseSpawn.level2Entities()[4].type == 12);
    assert((
        level3DenseSpawn.level2Entities()[4].position
        == stellar7::FixedPosition{-32769, -1, 1000}
    ));
    assert(level3DenseSpawn.level2Entities()[4].forwardVelocity == 360);
    assert(level3DenseSpawn.level2Rng().indexA() == 22);
    assert(level3DenseSpawn.level2Rng().indexB() == 84);
    assert(level3DenseSpawn.level2Rng().last() == 57466);
    assert(level3DenseSpawn.level2Rng().calls() == 13);
    for (unsigned loop = 34; loop <= 100; ++loop) {
        level3DenseSpawn.tick(level3DenseHeldInput);
        const auto& type12 = level3DenseSpawn.level2Entities()[4];
        if (loop == 40) {
            assert((type12.position == stellar7::FixedPosition{-31376, 2610, 1000}));
            assert(type12.behaviorTimer == 6);
            assert(level3DenseSpawn.level2Rng().indexA() == 52);
            assert(level3DenseSpawn.level2Rng().indexB() == 2);
            assert(level3DenseSpawn.level2Rng().last() == 21627);
        } else if (loop == 41) {
            assert((type12.position == stellar7::FixedPosition{-31182, 3022, 1000}));
            assert(type12.behaviorTimer == 5);
            assert(level3DenseSpawn.level2Rng().indexA() == 38);
            assert(level3DenseSpawn.level2Rng().indexB() == 100);
            assert(level3DenseSpawn.level2Rng().last() == 8662);
        } else if (loop == 47) {
            assert((type12.position == stellar7::FixedPosition{-30092, 5685, 1000}));
            assert(type12.yawVelocity == -271);
            assert(type12.roll == 271);
            assert(type12.stateBytes4dTo52[5] == 2);
            assert(level3DenseSpawn.level2Rng().indexA() == 32);
            assert(level3DenseSpawn.level2Rng().indexB() == 94);
            assert(level3DenseSpawn.level2Rng().last() == 788);
        } else if (loop == 50) {
            assert((type12.position == stellar7::FixedPosition{-29560, 7127, 850}));
            assert(type12.yaw == 64723);
            assert(type12.roll == 986);
            assert(type12.targetHeading == 46640);
            assert(level3DenseSpawn.level2Rng().indexA() == 96);
            assert(level3DenseSpawn.level2Rng().indexB() == 46);
            assert(level3DenseSpawn.level2Rng().last() == 62204);
        } else if (loop == 55) {
            assert((type12.position == stellar7::FixedPosition{-28620, 9654, 600}));
            assert(type12.yaw == 63368);
            assert(type12.yawVelocity == -271);
            assert(type12.roll == 1908);
            assert(type12.stateBytes4dTo52[5] == 2);
            assert(type12.targetHeading == 45712);
            assert(level3DenseSpawn.level2Rng().indexA() == 30);
            assert(level3DenseSpawn.level2Rng().indexB() == 92);
            assert(level3DenseSpawn.level2Rng().last() == 52254);
        } else if (loop == 75) {
            assert(level3DenseSpawn.level2Entities().size() == 10);
            assert((
                level3DenseSpawn.level2ActiveListOrder()
                == std::vector<std::size_t>{3, 5, 4, 2, 1, 9, 8, 6, 7, 0}
            ));
            assert((type12.position == stellar7::FixedPosition{-24911, 19781, 150}));
            assert(type12.yaw == 57948);
            assert(type12.yawVelocity == -271);
            assert(type12.roll == 3662);
            assert(type12.forwardVelocity == 360);
            assert(type12.targetHeading == 42128);
            assert(type12.stateBytes4dTo52[5] == 3);
            const auto& type3 = level3DenseSpawn.level2Entities()[5];
            assert(type3.type == 3);
            assert((type3.position == stellar7::FixedPosition{-32667, 2028, 0}));
            assert(type3.yaw == 61945);
            assert(type3.yawVelocity == -399);
            assert(type3.forwardVelocity == 216);
            assert(type3.targetHeading == 48464);
            assert(type3.stateBytes4dTo52[5] == 3);
            assert(level3DenseSpawn.level2Rng().indexA() == 30);
            assert(level3DenseSpawn.level2Rng().indexB() == 92);
            assert(level3DenseSpawn.level2Rng().last() == 11721);
        } else if (loop == 100) {
            assert(level3DenseSpawn.level2Entities().size() == 15);
            assert((
                level3DenseSpawn.level2ActiveListOrder()
                == std::vector<std::size_t>{3, 4, 5, 1, 2, 13, 14, 6, 7, 12, 9, 0, 8, 11, 10}
            ));
            assert((type12.position == stellar7::FixedPosition{-21108, 26611, 150}));
            assert(type12.yaw == 51173);
            assert(type12.yawVelocity == -271);
            assert(type12.roll == 4195);
            assert(type12.forwardVelocity == 360);
            assert(type12.targetHeading == 39808);
            assert(type12.stateBytes4dTo52[5] == 3);
            const auto& type3 = level3DenseSpawn.level2Entities()[5];
            assert(type3.type == 3);
            assert((type3.position == stellar7::FixedPosition{29291, 9786, 0}));
            assert(type3.yaw == 3990);
            assert(type3.yawVelocity == 399);
            assert(type3.forwardVelocity == 216);
            assert(type3.targetHeading == 19760);
            assert(type3.stateBytes4dTo52[5] == 3);
            for (std::size_t index = 8; index < 15; ++index) {
                assert(level3DenseSpawn.level2Entities()[index].type == 18);
                assert(level3DenseSpawn.level2Entities()[index].forwardVelocity == 500);
            }
            assert(level3DenseSpawn.level2Rng().indexA() == 96);
            assert(level3DenseSpawn.level2Rng().indexB() == 46);
            assert(level3DenseSpawn.level2Rng().last() == 29636);
        }
    }

    // Native forced-frontier continuation v8 extends the same held-input
    // probe beyond the previous loop-100 boundary. Keep these assertions
    // deliberately focused on scheduler/list, actor identity, and RNG so a
    // later implementation change cannot silently lose the recovered tail.
    for (unsigned loop = 101; loop <= 125; ++loop) {
        level3DenseSpawn.tick(level3DenseHeldInput);
    }
    assert(level3DenseSpawn.level2LoopTick() == 125);
    assert(level3DenseSpawn.level2Entities().size() == 15);
    assert((
        level3DenseSpawn.level2ActiveListOrder()
        == std::vector<std::size_t>{3, 1, 4, 2, 5, 13, 6, 7, 9, 14, 0, 11, 12, 10, 8}
    ));
    assert((
        level3DenseSpawn.level2Entities()[4].position
        == stellar7::FixedPosition{-16087, 22487, 150}
    ));
    assert(level3DenseSpawn.level2Entities()[4].yaw == 44398);
    assert(level3DenseSpawn.level2Entities()[4].roll == 4301);
    assert(level3DenseSpawn.level2Entities()[5].type == 3);
    assert((
        level3DenseSpawn.level2Entities()[5].position
        == stellar7::FixedPosition{21557, 10252, 0}
    ));
    assert(level3DenseSpawn.level2Entities()[5].yaw == 13965);
    assert(level3DenseSpawn.level2Rng().indexA() == 90);
    assert(level3DenseSpawn.level2Rng().indexB() == 40);
    assert(level3DenseSpawn.level2Rng().last() == 50919);

    for (unsigned loop = 126; loop <= 150; ++loop) {
        level3DenseSpawn.tick(level3DenseHeldInput);
    }
    assert(level3DenseSpawn.level2LoopTick() == 150);
    assert(level3DenseSpawn.level2Entities().size() == 14);
    assert((
        level3DenseSpawn.level2ActiveListOrder()
        == std::vector<std::size_t>{10, 13, 2, 1, 3, 12, 6, 7, 5, 0, 8, 4, 9, 11}
    ));
    assert((
        level3DenseSpawn.level2Entities()[4].position
        == stellar7::FixedPosition{-8298, 11815, 150}
    ));
    assert(level3DenseSpawn.level2Entities()[4].yaw == 39264);
    assert(level3DenseSpawn.level2Entities()[4].roll == 2738);
    assert(level3DenseSpawn.level2Entities()[4].behaviorTimer == 57);
    assert(level3DenseSpawn.level2Entities()[8].type == 20);
    assert(level3DenseSpawn.level2Entities()[9].type == 17);
    assert(level3DenseSpawn.level2Entities()[8].collisionRadius == 0);
    assert(level3DenseSpawn.level2Entities()[9].collisionRadius == 0);
    assert(level3DenseSpawn.level2Rng().indexA() == 40);
    assert(level3DenseSpawn.level2Rng().indexB() == 102);
    assert(level3DenseSpawn.level2Rng().last() == 51327);

    for (unsigned loop = 151; loop <= 175; ++loop) {
        level3DenseSpawn.tick(level3DenseHeldInput);
        if (loop == 160) {
            const auto launchedType18 = std::find_if(
                level3DenseSpawn.level2Entities().begin(),
                level3DenseSpawn.level2Entities().end(),
                [](const stellar7::Level1Entity& entity) {
                    return entity.type == 18
                        && entity.stateBytes4dTo52[1] == 65
                        && entity.position
                            == stellar7::FixedPosition{-13365, -10268, 0};
                }
            );
            assert(launchedType18 != level3DenseSpawn.level2Entities().end());
            assert(launchedType18->yaw == 1792);
            assert(launchedType18->collisionRadius == 0);
        }
    }
    assert(level3DenseSpawn.level2LoopTick() == 175);
    assert(level3DenseSpawn.level2Entities().size() == 13);
    assert((
        level3DenseSpawn.level2ActiveListOrder()
        == std::vector<std::size_t>{3, 2, 1, 6, 7, 5, 0, 11, 12, 8, 4, 9, 10}
    ));
    assert((
        level3DenseSpawn.level2Entities()[4].position
        == stellar7::FixedPosition{1628, 4340, 150}
    ));
    assert(level3DenseSpawn.level2Entities()[4].yaw == 39264);
    assert(level3DenseSpawn.level2Entities()[4].roll == 540);
    assert(level3DenseSpawn.level2Entities()[4].behaviorTimer == 32);
    assert(level3DenseSpawn.level2Entities()[5].type == 3);
    assert(level3DenseSpawn.level2Entities()[5].stateBytes4dTo52[5] == 1);
    assert(level3DenseSpawn.level2Entities()[8].type == 20);
    assert(level3DenseSpawn.level2Entities()[9].type == 17);
    assert(level3DenseSpawn.level2Rng().indexA() == 96);
    assert(level3DenseSpawn.level2Rng().indexB() == 46);
    assert(level3DenseSpawn.level2Rng().last() == 16650);

    // The isolated native loop-180 checkpoint remains usable even though the
    // backend stopped responding while requesting the following checkpoint.
    // Pin the two active actors that are unambiguous in the DS record.
    for (unsigned loop = 176; loop <= 180; ++loop) {
        level3DenseSpawn.tick(level3DenseHeldInput);
    }
    assert(level3DenseSpawn.level2LoopTick() == 180);
    assert((
        level3DenseSpawn.level2Entities()[4].position
        == stellar7::FixedPosition{3437, 3522, 150}
    ));
    assert(level3DenseSpawn.level2Entities()[4].yaw == 39264);
    assert(level3DenseSpawn.level2Entities()[4].behaviorTimer == 27);
    assert((
        level3DenseSpawn.level2Entities()[5].position
        == stellar7::FixedPosition{16838, 4067, 0}
    ));
    assert(level3DenseSpawn.level2Entities()[5].yaw == 18379);
    assert(level3DenseSpawn.level2Rng().indexA() == 64);
    assert(level3DenseSpawn.level2Rng().indexB() == 14);
    assert(level3DenseSpawn.level2Rng().last() == 33878);

    // Checkpoint v38 extends the validated semantic 0x850c boundary through
    // loop 183.  The breakpoint is after the complete entity callback and
    // presenter update, so these values are directly comparable to the
    // portable tick state rather than to the intermediate 0x0824:03d5 probe.
    for (unsigned loop = 181; loop <= 185; ++loop) {
        level3DenseSpawn.tick(level3DenseHeldInput);
        assert(level3DenseSpawn.level2Entities().size() == 13);
        const auto& type12 = level3DenseSpawn.level2Entities()[4];
        const auto& type3 = level3DenseSpawn.level2Entities()[5];
        assert(type12.type == 12);
        assert(type3.type == 3);
        assert(type12.yaw == 39264);
        assert(type12.behaviorTimer == static_cast<std::uint8_t>(27 - (loop - 180)));
        assert(type3.yaw == static_cast<std::uint16_t>(18379 + (loop - 180)));
        if (loop == 181) {
            assert((
                level3DenseSpawn.level2ActiveListOrder()
                == std::vector<std::size_t>{3, 2, 1, 5, 7, 6, 11, 0, 12, 9, 8, 4, 10}
            ));
            assert((type12.position == stellar7::FixedPosition{3779, 3380, 150}));
            assert((type3.position == stellar7::FixedPosition{16758, 4177, 0}));
            assert(level3DenseSpawn.level2Rng().indexA() == 62);
            assert(level3DenseSpawn.level2Rng().indexB() == 12);
            assert(level3DenseSpawn.level2Rng().last() == 65094);
        } else if (loop == 182) {
            assert((
                level3DenseSpawn.level2ActiveListOrder()
                == std::vector<std::size_t>{3, 2, 1, 5, 7, 6, 11, 0, 12, 9, 8, 4, 10}
            ));
            assert((type12.position == stellar7::FixedPosition{4113, 3244, 150}));
            assert((type3.position == stellar7::FixedPosition{16670, 4293, 0}));
            assert(level3DenseSpawn.level2Rng().indexA() == 60);
            assert(level3DenseSpawn.level2Rng().indexB() == 10);
            assert(level3DenseSpawn.level2Rng().last() == 7522);
        } else if (loop == 183) {
            assert((
                level3DenseSpawn.level2ActiveListOrder()
                == std::vector<std::size_t>{3, 2, 1, 5, 7, 6, 11, 0, 12, 9, 4, 8, 10}
            ));
            assert((type12.position == stellar7::FixedPosition{4439, 3114, 150}));
            assert((type3.position == stellar7::FixedPosition{16574, 4415, 0}));
            assert(level3DenseSpawn.level2Rng().indexA() == 54);
            assert(level3DenseSpawn.level2Rng().indexB() == 4);
            assert(level3DenseSpawn.level2Rng().last() == 36118);
        } else if (loop == 184) {
            assert((
                level3DenseSpawn.level2ActiveListOrder()
                == std::vector<std::size_t>{3, 2, 1, 5, 7, 6, 11, 12, 0, 9, 4, 8, 10}
            ));
            assert((type12.position == stellar7::FixedPosition{4757, 2989, 150}));
            assert((type3.position == stellar7::FixedPosition{16470, 4542, 0}));
            assert(type3.yaw == 18383);
            assert(level3DenseSpawn.level2Rng().indexA() == 48);
            assert(level3DenseSpawn.level2Rng().indexB() == 110);
            assert(level3DenseSpawn.level2Rng().last() == 36508);
        } else {
            assert((
                level3DenseSpawn.level2ActiveListOrder()
                == std::vector<std::size_t>{3, 2, 1, 5, 7, 6, 11, 12, 0, 9, 4, 10, 8}
            ));
            assert((type12.position == stellar7::FixedPosition{5067, 2869, 150}));
            assert((type3.position == stellar7::FixedPosition{16358, 4673, 0}));
            assert(type3.yaw == 18384);
            assert(level3DenseSpawn.level2Rng().indexA() == 42);
            assert(level3DenseSpawn.level2Rng().indexB() == 104);
            assert(level3DenseSpawn.level2Rng().last() == 1375);
        }
    }
    // The native script releases Left+Up immediately after the loop-185
    // checkpoint; loop 186 therefore verifies the first neutral-input state.
    const stellar7::GameplayInput level3DenseReleasedInput{};
    level3DenseSpawn.tick(level3DenseReleasedInput);
    assert(level3DenseSpawn.level2LoopTick() == 186);
    assert((
        level3DenseSpawn.level2ActiveListOrder()
        == std::vector<std::size_t>{3, 2, 1, 5, 7, 6, 11, 12, 0, 9, 4, 10, 8}
    ));
    assert((
        level3DenseSpawn.level2Entities()[4].position
        == stellar7::FixedPosition{5368, 2754, 150}
    ));
    assert((
        level3DenseSpawn.level2Entities()[5].position
        == stellar7::FixedPosition{16237, 4809, 0}
    ));
    assert(level3DenseSpawn.level2Entities()[4].behaviorTimer == 21);
    assert(level3DenseSpawn.level2Entities()[5].yaw == 18385);
    assert(level3DenseSpawn.level2Rng().indexA() == 32);
    assert(level3DenseSpawn.level2Rng().indexB() == 94);
    assert(level3DenseSpawn.level2Rng().last() == 35509);

    // Sparse native v42 checkpoints continue the released-input route to
    // loop 200.  Pin the scheduler/list mutations as well as the two
    // surviving campaign actors and RNG at each captured boundary.
    for (unsigned loop = 187; loop <= 200; ++loop) {
        level3DenseSpawn.tick(level3DenseReleasedInput);
        if (loop == 187) {
            assert(level3DenseSpawn.level2Entities().size() == 13);
            assert((
                level3DenseSpawn.level2ActiveListOrder()
                == std::vector<std::size_t>{3, 2, 1, 5, 7, 6, 11, 12, 0, 9, 4, 10, 8}
            ));
            assert((level3DenseSpawn.level2Entities()[4].position == stellar7::FixedPosition{5658, 2634, 150}));
            assert((level3DenseSpawn.level2Entities()[5].position == stellar7::FixedPosition{16105, 4940, 0}));
            assert(level3DenseSpawn.level2Entities()[4].behaviorTimer == 20);
            assert(level3DenseSpawn.level2Entities()[5].yaw == 18386);
            assert(level3DenseSpawn.level2Rng().indexA() == 22);
            assert(level3DenseSpawn.level2Rng().indexB() == 84);
            assert(level3DenseSpawn.level2Rng().last() == 39110);
        } else if (loop == 190) {
            assert(level3DenseSpawn.level2Entities().size() == 13);
            assert((
                level3DenseSpawn.level2ActiveListOrder()
                == std::vector<std::size_t>{3, 2, 1, 5, 7, 6, 11, 12, 0, 9, 10, 4, 8}
            ));
            assert((level3DenseSpawn.level2Entities()[4].position == stellar7::FixedPosition{6475, 2230, 150}));
            assert((level3DenseSpawn.level2Entities()[5].position == stellar7::FixedPosition{15656, 5289, 0}));
            assert(level3DenseSpawn.level2Entities()[4].behaviorTimer == 17);
            assert(level3DenseSpawn.level2Entities()[5].yaw == 18389);
            assert(level3DenseSpawn.level2Rng().indexA() == 8);
            assert(level3DenseSpawn.level2Rng().indexB() == 70);
            assert(level3DenseSpawn.level2Rng().last() == 32523);
        } else if (loop == 195) {
            assert(level3DenseSpawn.level2Entities().size() == 13);
            assert((
                level3DenseSpawn.level2ActiveListOrder()
                == std::vector<std::size_t>{3, 2, 1, 5, 7, 6, 11, 12, 9, 10, 0, 4, 8}
            ));
            assert((level3DenseSpawn.level2Entities()[4].position == stellar7::FixedPosition{7742, 1379, 150}));
            assert((level3DenseSpawn.level2Entities()[5].position == stellar7::FixedPosition{14813, 5693, 0}));
            assert(level3DenseSpawn.level2Entities()[4].behaviorTimer == 12);
            assert(level3DenseSpawn.level2Entities()[5].yaw == 18394);
            assert(level3DenseSpawn.level2Rng().indexA() == 104);
            assert(level3DenseSpawn.level2Rng().indexB() == 54);
            assert(level3DenseSpawn.level2Rng().last() == 21486);
        } else if (loop == 200) {
            assert(level3DenseSpawn.level2Entities().size() == 11);
            assert((
                level3DenseSpawn.level2ActiveListOrder()
                == std::vector<std::size_t>{0, 1, 2, 3, 6, 7, 4, 5, 8, 10, 9}
            ));
            assert((level3DenseSpawn.level2Entities()[4].position == stellar7::FixedPosition{8924, 293, 150}));
            assert((level3DenseSpawn.level2Entities()[5].position == stellar7::FixedPosition{14737, 6030, 0}));
            assert(level3DenseSpawn.level2Entities()[4].behaviorTimer == 7);
            assert(level3DenseSpawn.level2Entities()[5].yaw == 19991);
            assert(level3DenseSpawn.level2Rng().indexA() == 22);
            assert(level3DenseSpawn.level2Rng().indexB() == 84);
            assert(level3DenseSpawn.level2Rng().last() == 58629);
        }
    }

    // Native FUN_1000:952c callback regressions recovered from the Type 12
    // assembly. These probes keep the state boundaries independent of the
    // neutral-route scheduler chronology.
    stellar7::GameplayModel level3Type12Cruise;
    stellar7::GameplayModelTestAccess::configureLevel3Type12Callback(
        level3Type12Cruise,
        0,
        1000,
        true
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(level3Type12Cruise);
    assert(level3Type12Cruise.level2Entities()[0].forwardVelocity == 360);
    assert(level3Type12Cruise.level2Entities()[0].stateBytes4dTo52[5] == 0);

    stellar7::GameplayModel level3Type12Descent;
    stellar7::GameplayModelTestAccess::configureLevel3Type12Callback(
        level3Type12Descent,
        2,
        250,
        false
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(level3Type12Descent);
    assert(level3Type12Descent.level2Entities()[0].position.z == 200);
    assert(level3Type12Descent.level2Entities()[0].stateBytes4dTo52[5] == 2);
    stellar7::GameplayModelTestAccess::advanceLevel2(level3Type12Descent);
    assert(level3Type12Descent.level2Entities()[0].position.z == 150);
    assert(level3Type12Descent.level2Entities()[0].stateBytes4dTo52[5] == 3);

    stellar7::GameplayModel level3Type12Return;
    stellar7::GameplayModelTestAccess::configureLevel3Type12Callback(
        level3Type12Return,
        5,
        900,
        false
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(level3Type12Return);
    assert(level3Type12Return.level2Entities()[0].position.z == 1000);
    assert(level3Type12Return.level2Entities()[0].stateBytes4dTo52[5] == 0);

    stellar7::GameplayModel level3Type12Contact;
    stellar7::GameplayModelTestAccess::configureLevel3Type12Callback(
        level3Type12Contact,
        2,
        800,
        true,
        true
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(level3Type12Contact);
    assert(level3Type12Contact.level2Entities()[0].position.z == 900);
    assert(level3Type12Contact.level2Entities()[0].stateBytes4dTo52[5] == 5);
    assert(!level3Type12Contact.level2Entities()[0].collisionContactIsPlayer);

    // Native FUN_1000:3a1c treats collision radii as signed 32-bit values.
    // Level 3 Type 10 carries -4096 and is therefore non-solid even at exact
    // player overlap. Type 2 and the player both carry zero; one coordinate
    // unit of separation is consequently a miss, not a model-sized contact.
    stellar7::GameplayModel level3NegativeRadiusContact;
    stellar7::GameplayModelTestAccess::configureLevel3OrdinaryPlayerContactProbe(
        level3NegativeRadiusContact,
        10,
        0xfffff000U,
        {0, 0, 0}
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3NegativeRadiusContact
    );
    assert(level3NegativeRadiusContact.playerArmor() == 680);
    assert(level3NegativeRadiusContact.level2AudioEvents().empty());
    assert((
        level3NegativeRadiusContact.level2Player().position
        == stellar7::FixedPosition{0, 0, 0}
    ));

    stellar7::GameplayModel level3ZeroRadiusNearMiss;
    stellar7::GameplayModelTestAccess::configureLevel3OrdinaryPlayerContactProbe(
        level3ZeroRadiusNearMiss,
        2,
        0,
        {1, 0, 0}
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3ZeroRadiusNearMiss
    );
    assert(level3ZeroRadiusNearMiss.playerArmor() == 680);
    assert(level3ZeroRadiusNearMiss.level2AudioEvents().empty());
    assert((
        level3ZeroRadiusNearMiss.level2Player().position
        == stellar7::FixedPosition{0, 0, 0}
    ));

    stellar7::GameplayModel level3Type12Fire;
    stellar7::GameplayModelTestAccess::configureLevel3Type12Callback(
        level3Type12Fire,
        3,
        300,
        true
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(level3Type12Fire);
    assert(level3Type12Fire.level2Entities().size() == 2);
    assert(level3Type12Fire.level2Entities()[0].type == 12);
    assert(level3Type12Fire.level2Entities()[0].stateBytes4dTo52[5] == 4);
    assert(level3Type12Fire.level2Entities()[0].behaviorTimer == 63);
    assert(level3Type12Fire.level2Entities()[1].type == 17);

    // Native 1000:9616 is the Type 12 miss gate.  The byte tested there is
    // the low byte retained from the state-1 RNG draw; values 0 and 1 send
    // an in-range, misaligned actor to the altitude-return state instead of
    // leaving it in the attack state.
    stellar7::GameplayModel level3Type12MissGate;
    stellar7::GameplayModelTestAccess::configureLevel3Type12Callback(
        level3Type12MissGate,
        3,
        300,
        true
    );
    stellar7::GameplayModelTestAccess::configureLevel3Type12MissGate(
        level3Type12MissGate
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type12MissGate
    );
    assert(
        level3Type12MissGate.level2Entities()[0].stateBytes4dTo52[5] == 5
    );
    stellar7::GameplayModel level3Type12MissGateControl;
    stellar7::GameplayModelTestAccess::configureLevel3Type12Callback(
        level3Type12MissGateControl,
        3,
        300,
        true
    );
    stellar7::GameplayModelTestAccess::configureLevel3Type12MissGate(
        level3Type12MissGateControl,
        2
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type12MissGateControl
    );
    assert(
        level3Type12MissGate.level2Rng().calls()
        == level3Type12MissGateControl.level2Rng().calls()
    );
    assert(
        level3Type12MissGateControl.level2Entities()[0]
            .stateBytes4dTo52[5] == 3
    );

    // Native Level 3 FUN_1000:a50e emits two Type-47 records when its
    // state-1 visual timer decrements through 10 with no completion object.
    // This is distinct from the Level 2 a92e low-six-bit launch gate.
    stellar7::GameplayModel level3Type44Burst;
    stellar7::GameplayModelTestAccess::configureLevel3Type44Burst(
        level3Type44Burst
    );
    stellar7::GameplayModel level3Type44BurstSuppressed;
    stellar7::GameplayModelTestAccess::configureLevel3Type44Burst(
        level3Type44BurstSuppressed
    );
    stellar7::GameplayModelTestAccess::suppressLevel3Type44Burst(
        level3Type44BurstSuppressed
    );
    for (int tick = 0; tick < 3; ++tick) {
        stellar7::GameplayModelTestAccess::advanceLevel2(
            level3Type44Burst
        );
        stellar7::GameplayModelTestAccess::advanceLevel2(
            level3Type44BurstSuppressed
        );
    }
    // Native tick 1036 calls FUN_1000:ae10 twice. Each call allocates through
    // FUN_1000:4370 as provisional Type 13 and therefore retains all three
    // initializer RNG draws even though ae10 overwrites the record to Type 47.
    assert(
        level3Type44Burst.level2Rng().calls()
        == level3Type44BurstSuppressed.level2Rng().calls() + 6
    );

    // FUN_1000:4370 searches the actor pool before invoking its three-draw
    // Type-13 initializer. Native loop 1554 reaches the two-shot burst with
    // all sixteen records occupied, so both failed allocations consume no
    // initializer RNG words.
    stellar7::GameplayModel level3Type44BurstFullPool;
    stellar7::GameplayModelTestAccess::configureLevel3Type44Burst(
        level3Type44BurstFullPool
    );
    stellar7::GameplayModelTestAccess::fillLevel3ActorPool(
        level3Type44BurstFullPool
    );
    stellar7::GameplayModel level3Type44BurstFullPoolSuppressed;
    stellar7::GameplayModelTestAccess::configureLevel3Type44Burst(
        level3Type44BurstFullPoolSuppressed
    );
    stellar7::GameplayModelTestAccess::fillLevel3ActorPool(
        level3Type44BurstFullPoolSuppressed
    );
    stellar7::GameplayModelTestAccess::suppressLevel3Type44Burst(
        level3Type44BurstFullPoolSuppressed
    );
    for (int tick = 0; tick < 3; ++tick) {
        stellar7::GameplayModelTestAccess::advanceLevel2(
            level3Type44BurstFullPool
        );
        stellar7::GameplayModelTestAccess::advanceLevel2(
            level3Type44BurstFullPoolSuppressed
        );
    }
    assert(
        level3Type44BurstFullPool.level2Rng().calls()
        == level3Type44BurstFullPoolSuppressed.level2Rng().calls()
    );
    assert(std::none_of(
        level3Type44BurstFullPool.level2Entities().begin(),
        level3Type44BurstFullPool.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 47;
        }
    ));
    const auto level3Type44AtEmission = std::find_if(
        level3Type44Burst.level2Entities().begin(),
        level3Type44Burst.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 44;
        }
    );
    assert(level3Type44AtEmission != level3Type44Burst.level2Entities().end());
    assert(level3Type44AtEmission->position.z == 500);
    assert(level3Type44AtEmission->behaviorTimer == 10);
    // FUN_1000:a50e writes 5 - abs((timer >> 1) - 5) through the model
    // selector pointer. At the timer-10 emission boundary this is selector5.
    assert(level3Type44AtEmission->presentationModelAnimationVariant == 5);
    const auto level3Type47BurstCount = std::count_if(
        level3Type44Burst.level2Entities().begin(),
        level3Type44Burst.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 47;
        }
    );
    assert(level3Type47BurstCount == 2);
    for (const auto& entity : level3Type44Burst.level2Entities()) {
        if (entity.type != 47) {
            continue;
        }
        assert(entity.position.z == 1000);
        assert(entity.collisionRadius == 0);
        assert(entity.forwardVelocity == 0);
        assert(entity.behaviorTimer == 0);
        assert((
            entity.stateBytes4dTo52
            == std::array<std::uint8_t, 6>{255, 1, 0, 0, 4, 0}
        ));
    }

    // Level 3's zero-radius Type-47 guardian emissions can occupy identical
    // coordinates. FUN_1000:0784 records their contact, but its Type-47
    // mutual-damage branch is gated to campaign index 1 (Level 2). Native
    // loop 1380 therefore retains both records and creates no explosion.
    stellar7::GameplayModel level3Type47MutualOverlap;
    stellar7::GameplayModelTestAccess::configureLevel3Type47MutualOverlap(
        level3Type47MutualOverlap
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type47MutualOverlap
    );
    assert(std::count_if(
        level3Type47MutualOverlap.level2Entities().begin(),
        level3Type47MutualOverlap.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 47;
        }
    ) == 2);
    assert(std::none_of(
        level3Type47MutualOverlap.level2Entities().begin(),
        level3Type47MutualOverlap.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == -1;
        }
    ));

    // FUN_1000:a50e falls through from state 0 to state 2 when the launch
    // gate is blocked by a live contact record. The native branch still
    // consumes the RNG word before taking that path.
    stellar7::GameplayModel level3Type44ContactGate;
    stellar7::GameplayModelTestAccess::configureLevel3GuardianEmission(
        level3Type44ContactGate
    );
    stellar7::GameplayModelTestAccess::configureLevel3Type44ContactGate(
        level3Type44ContactGate
    );
    stellar7::GameplayModelTestAccess::armLevel3Type44LaunchChoice(
        level3Type44ContactGate
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type44ContactGate
    );
    assert(
        level3Type44ContactGate.level2Entities()[0].stateBytes4dTo52[5]
        == 2
    );

    // FUN_1000:a50e enters descent state 1 when the launch record is more
    // than 4000 units away. This direction is visible on the native
    // loop-763 guardian boundary and controls whether player fire can meet
    // its altitude.
    stellar7::GameplayModel level3Type44FarDescentGate;
    stellar7::GameplayModelTestAccess::configureLevel3Type44FarDescentGate(
        level3Type44FarDescentGate
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type44FarDescentGate
    );
    assert(
        level3Type44FarDescentGate.level2Entities()[0]
            .stateBytes4dTo52[5] == 1
    );

    // Pinned loop 1035->1036 reaches the exact signed half-turn delta
    // 0x8000. FUN_1000:ac3a canonicalizes it to +0x7fff before clamping,
    // while a50e's 16-bit magnitude remains negative and therefore passes
    // the near-heading branch into distant descent state 1.
    stellar7::GameplayModel level3Type44HalfTurnBoundary;
    stellar7::GameplayModelTestAccess::configureLevel3Type44HalfTurnBoundary(
        level3Type44HalfTurnBoundary
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type44HalfTurnBoundary
    );
    assert(
        level3Type44HalfTurnBoundary.level2Entities()[0]
            .stateBytes4dTo52[5] == 1
    );
    assert(
        level3Type44HalfTurnBoundary.level2Entities()[0].yawVelocity == 0x200
    );

    // Native a50e tests the post-subtraction Z value against 0x1f5, so a
    // 600-unit record reaches the 20-tick hold after becoming Z=500.
    stellar7::GameplayModel level3Type44DescentBoundary;
    stellar7::GameplayModelTestAccess::configureLevel3Type44DescentBoundary(
        level3Type44DescentBoundary
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type44DescentBoundary
    );
    assert(
        level3Type44DescentBoundary.level2Entities()[0].position.z == 500
    );
    assert(
        level3Type44DescentBoundary.level2Entities()[0].behaviorTimer == 20
    );

    // Pinned loop 792->793 proves that the state-1 hold enters state 2 on
    // the same callback that decrements its timer from one to zero. The
    // first state-2 climb remains deferred until the following boundary.
    stellar7::GameplayModel level3Type44HoldExpiry;
    stellar7::GameplayModelTestAccess::configureLevel3Type44HoldExpiry(
        level3Type44HoldExpiry
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type44HoldExpiry
    );
    assert(level3Type44HoldExpiry.level2Entities()[0].behaviorTimer == 0);
    assert(
        level3Type44HoldExpiry.level2Entities()[0].stateBytes4dTo52[5] == 2
    );
    assert(level3Type44HoldExpiry.level2Entities()[0].position.z == 500);

    // Native loop 1036->1037 acquires a Type-47 contact while Type 44 is in
    // state 1. After timer 10 decrements to 9, a50e falls through to state 2
    // immediately instead of waiting for the hold timer to reach zero.
    stellar7::GameplayModel level3Type44State1ContactExit;
    stellar7::GameplayModelTestAccess::configureLevel3Type44State1ContactExit(
        level3Type44State1ContactExit
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type44State1ContactExit
    );
    assert(
        level3Type44State1ContactExit.level2Entities()[0].behaviorTimer == 9
    );
    assert(
        level3Type44State1ContactExit.level2Entities()[0]
            .stateBytes4dTo52[5] == 2
    );

    // Native loop 1735 applies a nonlethal player-projectile hit and a
    // Type-47 contact to the guardian on the same callback boundary. The
    // projectile retirement does not suppress the remaining collision walk.
    stellar7::GameplayModel level3ProjectileHitAndGuardianContact;
    stellar7::GameplayModelTestAccess::
        configureLevel3ProjectileHitAndGuardianContact(
            level3ProjectileHitAndGuardianContact
        );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3ProjectileHitAndGuardianContact
    );
    const auto simultaneousHitGuardian = std::find_if(
        level3ProjectileHitAndGuardianContact.level2Entities().begin(),
        level3ProjectileHitAndGuardianContact.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 44;
        }
    );
    assert(
        simultaneousHitGuardian
        != level3ProjectileHitAndGuardianContact.level2Entities().end()
    );
    assert(simultaneousHitGuardian->stateBytes4dTo52[1] == 1);
    assert(simultaneousHitGuardian->stateBytes4dTo52[5] == 2);

    // Pinned Level 3 loop 1532 carries a zero contact word on Type 44 while
    // a Type -1 death effect overlaps it. The effect presenter is not an
    // actor collision source; loop 1533 therefore retains guardian state 1.
    stellar7::GameplayModel level3Type44DeathEffectOverlap;
    stellar7::GameplayModelTestAccess::configureLevel3Type44DeathEffectOverlap(
        level3Type44DeathEffectOverlap
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type44DeathEffectOverlap
    );
    assert(
        level3Type44DeathEffectOverlap.level2Entities()[0]
            .collisionContactAllocationIndex
        == static_cast<std::size_t>(-1)
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type44DeathEffectOverlap
    );
    assert(
        level3Type44DeathEffectOverlap.level2Entities()[0]
            .stateBytes4dTo52[5] == 1
    );

    // FUN_1000:b996 clears bit 2 when a presenter-owned death effect is
    // behind the main view. Level 3 loop 1558 exposes the same 0x8c->0x88
    // transition on a live Type -1 record.
    stellar7::GameplayModel level3DeathEffectBehindView;
    stellar7::GameplayModelTestAccess::configureLevel3DeathEffectBehindView(
        level3DeathEffectBehindView
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3DeathEffectBehindView
    );
    assert(
        level3DeathEffectBehindView.level2Entities()[1].flags == 0x88
    );

    // Native loop 1559 still distance-sorts the surviving presenter list
    // after a terminal Type -1 record leaves the allocation list. Actor or
    // projectile membership changes use the separate allocation rebuild.
    stellar7::GameplayModel level3TerminalEffectSort;
    stellar7::GameplayModelTestAccess::configureLevel3TerminalEffectSort(
        level3TerminalEffectSort
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3TerminalEffectSort
    );
    assert(level3TerminalEffectSort.level2ActiveListOrder().size() == 2);
    assert(
        level3TerminalEffectSort.level2Entities()[
            level3TerminalEffectSort.level2ActiveListOrder()[0]
        ].type == 47
    );

    // A terminal effect rebuild is not exporter-only state. The following
    // callback boundary inherits the rebuilt allocation order and does not
    // run FUN_1000:14b0 again on that same rebuild boundary.
    stellar7::GameplayModel level3PostTerminalEffectTie;
    stellar7::GameplayModelTestAccess::configureLevel3PostTerminalEffectTie(
        level3PostTerminalEffectTie
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3PostTerminalEffectTie
    );
    assert(level3PostTerminalEffectTie.level2ActiveListOrder().size() == 2);
    assert(level3PostTerminalEffectTie.level2ActiveListOrder()[0] == 0);

    // Native loop 1558->1559 backs out the Type-44 forward translation on
    // player contact, then its state-2 callback clears the contact and climbs.
    stellar7::GameplayModel level3Type44State2PlayerContact;
    stellar7::GameplayModelTestAccess::configureLevel3Type44State2PlayerContact(
        level3Type44State2PlayerContact
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type44State2PlayerContact
    );
    assert(
        level3Type44State2PlayerContact.level2Entities()[0].position.x == -207
    );
    assert(
        level3Type44State2PlayerContact.level2Entities()[0].position.y == 1197
    );
    assert(
        level3Type44State2PlayerContact.level2Entities()[0].position.z == 1200
    );
    assert(
        level3Type44State2PlayerContact.level2Entities()[0]
            .collisionContactAllocationIndex
        == static_cast<std::size_t>(-1)
    );
    assert(
        !level3Type44State2PlayerContact.level2Entities()[0]
            .collisionContactIsPlayer
    );

    // Native full-state route v30 loop 1697->1698 reaches player contact
    // only after the Type 44 common integration. FUN_1000:0784 applies its
    // ordinary Type-0-target backout even while the Type 44 is in behavior 1;
    // the later a50e callback changes behavior 1 -> 2 but retains contact.
    stellar7::GameplayModel level3Type44State1PlayerContactBackout;
    stellar7::GameplayModelTestAccess::configureLevel3Type44State1PlayerContactBackout(
        level3Type44State1PlayerContactBackout
    );
    const auto level3Type44State1ContactArmorBefore =
        level3Type44State1PlayerContactBackout.playerArmor();
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type44State1PlayerContactBackout
    );
    assert(
        level3Type44State1PlayerContactBackout.level2Entities()[0]
            .position.y == 1900
    );
    assert(
        level3Type44State1PlayerContactBackout.level2Entities()[0]
            .stateBytes4dTo52[5] == 2
    );
    assert(
        level3Type44State1PlayerContactBackout.level2Entities()[0]
            .collisionContactIsPlayer
    );
    assert(
        level3Type44State1PlayerContactBackout.playerArmor()
        == level3Type44State1ContactArmorBefore - 100
    );
    assert(
        !level3Type44State1PlayerContactBackout.level2AudioEvents().empty()
    );
    assert(
        level3Type44State1PlayerContactBackout.level2AudioEvents().back().eventId
        == 6
    );

    // Native state 3 returns to state 0 from presenter distance, independent
    // of the guardian's current Z altitude.
    stellar7::GameplayModel level3Type44ReturnDistance;
    stellar7::GameplayModelTestAccess::configureLevel3Type44ReturnDistance(
        level3Type44ReturnDistance
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type44ReturnDistance
    );
    assert(
        level3Type44ReturnDistance.level2Entities()[0].stateBytes4dTo52[5]
        == 0
    );

    // Native state 2 clears the contact word before beginning the return
    // climb. This is observable on the first zero-timer callback.
    stellar7::GameplayModel level3Type44State2ContactClear;
    stellar7::GameplayModelTestAccess::configureLevel3Type44State2ContactClear(
        level3Type44State2ContactClear
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type44State2ContactClear
    );
    assert(
        level3Type44State2ContactClear.level2Entities()[0]
            .collisionContactAllocationIndex
        == static_cast<std::size_t>(-1)
    );

    // Native full-state route v30 loop 1193->1194: the returning Type 44
    // overlaps the descending Type 44 only after its common integration.
    // FUN_1000:0784 records the last contact, calls FUN_1000:3d10 to remove
    // the source's forward step, and leaves only the player-origin rebase.
    // The callback then consumes the contact while changing state 2 -> 3.
    stellar7::GameplayModel level3Type44MutualContactBackout;
    stellar7::GameplayModelTestAccess::configureLevel3Type44MutualContactBackout(
        level3Type44MutualContactBackout
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type44MutualContactBackout
    );
    assert(
        level3Type44MutualContactBackout.level2Entities()[1].position.x == 468
    );
    assert(
        level3Type44MutualContactBackout.level2Entities()[1].position.y == -610
    );
    assert(
        level3Type44MutualContactBackout.level2Entities()[1].position.z == 2100
    );
    assert(
        level3Type44MutualContactBackout.level2Entities()[1]
            .stateBytes4dTo52[5] == 3
    );
    assert(
        level3Type44MutualContactBackout.level2Entities()[1]
            .collisionContactAllocationIndex
        == static_cast<std::size_t>(-1)
    );

    // Native FUN_1000:0784 calls FUN_1000:3b3a before the Type-47 callback.
    // The callback's previous 9a34 result therefore moves the record on the
    // next boundary, even though the callback itself has zero forward speed.
    // This fixture uses the first native guardian-presentation displacement
    // observed at checkpoint 111 (x=40,y=-158) as the retained velocity.
    const auto level3Type47MovementIndex = static_cast<std::size_t>(
        std::find_if(
            level3Type44Burst.level2Entities().begin(),
            level3Type44Burst.level2Entities().end(),
            [](const stellar7::Level1Entity& entity) {
                return entity.type == 47;
            }
        ) - level3Type44Burst.level2Entities().begin()
    );
    assert(level3Type47MovementIndex < level3Type44Burst.level2Entities().size());
    const auto level3Type47BeforeMovement =
        level3Type44Burst.level2Entities()[level3Type47MovementIndex].position;
    stellar7::GameplayModelTestAccess::setLevel2PlayerPosition(
        level3Type44Burst,
        {0, 0, 0}
    );
    stellar7::GameplayModelTestAccess::setLevel2EntityPlanarVelocity(
        level3Type44Burst,
        level3Type47MovementIndex,
        40,
        -158
    );
    // FUN_1000:9a34 is shared by the Type 47/48 callbacks.  The emitted
    // records descend in 50-unit steps and retain selector 4 while grounded.
    stellar7::GameplayModelTestAccess::advanceLevel2(level3Type44Burst);
    assert(level3Type44Burst.level2Entities()[level3Type47MovementIndex].position.x
        == level3Type47BeforeMovement.x + 40);
    assert(level3Type44Burst.level2Entities()[level3Type47MovementIndex].position.y
        == level3Type47BeforeMovement.y - 158);
    assert(std::count_if(
        level3Type44Burst.level2Entities().begin(),
        level3Type44Burst.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 47;
        }
    ) == 2);
    const auto level3Type47AfterDescent = std::find_if(
        level3Type44Burst.level2Entities().begin(),
        level3Type44Burst.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 47;
        }
    );
    assert(
        level3Type47AfterDescent
        != level3Type44Burst.level2Entities().end()
    );
    assert(level3Type47AfterDescent->position.z == 950);
    for (int tick = 0; tick < 19; ++tick) {
        stellar7::GameplayModelTestAccess::advanceLevel2(
            level3Type44Burst
        );
    }
    assert(level3Type47AfterDescent->position.z == 0);

    // FUN_1000:46c8 is reached at the shared periodic boundary once the
    // campaign count is exhausted and no active actor remains.  Exercise the
    // allocator itself instead of only setting the terminal flag by hand.
    stellar7::GameplayModel level3CompletionDeployment;
    stellar7::GameplayModelTestAccess::configureLevel3CompletionDeployment(
        level3CompletionDeployment
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3CompletionDeployment
    );
    const auto level3Type7 = std::find_if(
        level3CompletionDeployment.level2Entities().begin(),
        level3CompletionDeployment.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 7;
        }
    );
    assert(
        level3Type7 != level3CompletionDeployment.level2Entities().end()
    );
    assert(level3Type7->position.z == 5000);
    assert(level3Type7->forwardVelocity == 400);
    assert(level3CompletionDeployment.level2AudioEvents().back().eventId == 17);

    // Pinned DOSBox-X Level 3 loop 687->688 advances the Type 7 completion
    // craft's signed roll from -41 to -39 while yaw velocity and heading
    // error are both zero. FUN_1000:af8c therefore retains the shared
    // roll-centering term even when the craft is not turning.
    stellar7::GameplayModel level3Type7BankCentering;
    stellar7::GameplayModelTestAccess::configureLevel3Type7BankCentering(
        level3Type7BankCentering
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type7BankCentering
    );
    assert(level3Type7BankCentering.level2Entities()[0].roll == 65497);

    // The arrival callback changes behavior 0 -> 1 and installs timer 0x50
    // without a separate roll decrement. A zero-bank craft therefore stays
    // at zero when its next movement crosses inside 1000 units.
    stellar7::GameplayModel level3Type7Arrival;
    stellar7::GameplayModelTestAccess::configureLevel3Type7Arrival(
        level3Type7Arrival
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(level3Type7Arrival);
    assert(level3Type7Arrival.level2Entities()[0].stateBytes4dTo52[5] == 1);
    assert(level3Type7Arrival.level2Entities()[0].behaviorTimer == 0x50);
    assert(level3Type7Arrival.level2Entities()[0].roll == 0);

    // Native loop 922 enters climb state 4 with roll 7. Three callbacks use
    // signed SAR rounding and reach roll -717 at loop 925; truncating signed
    // division toward zero instead produces the off-by-one value -716.
    stellar7::GameplayModel level3Type7ClimbBank;
    stellar7::GameplayModelTestAccess::configureLevel3Type7ClimbBank(
        level3Type7ClimbBank
    );
    for (int tick = 0; tick < 3; ++tick) {
        stellar7::GameplayModelTestAccess::advanceLevel2(
            level3Type7ClimbBank
        );
    }
    assert(
        static_cast<std::int16_t>(
            level3Type7ClimbBank.level2Entities()[0].roll
        ) == -717
    );
    assert(level3Type7ClimbBank.level2Entities()[0].pitch == 432);
    assert(level3Type7ClimbBank.level2Entities()[0].forwardVelocity == 150);

    // The native Type 7 completion object emits Type 44 on its timer-29
    // boundary before FUN_ae10 creates the Level 3 Type 47 guardian.
    stellar7::GameplayModel level3PostWave;
    stellar7::GameplayModelTestAccess::configureLevel3PostWaveDeployment(
        level3PostWave
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(level3PostWave);
    const auto level3Type44 = std::find_if(
        level3PostWave.level2Entities().begin(),
        level3PostWave.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 44;
        }
    );
    assert(level3Type44 != level3PostWave.level2Entities().end());
    assert(level3PostWave.level2Entities().size() == 2);
    assert(level3Type44->allocationIndex == 1);

    // Native FUN_1000:af8c drives the completion craft through the full
    // delivery tail before FUN_1000:477a installs the Level 3 guardian.
    // Start at the captured state-1 boundary (forward=400, timer=80), then
    // pin the state-2 hold and the state-3 guardian handoff.
    stellar7::GameplayModel level3Type7Tail;
    stellar7::GameplayModelTestAccess::configureLevel3Type7Tail(
        level3Type7Tail
    );
    for (int tick = 0; tick < 80; ++tick) {
        stellar7::GameplayModelTestAccess::advanceLevel2(
            level3Type7Tail
        );
    }
    assert(level3Type7Tail.level2Entities()[0].stateBytes4dTo52[5] == 2);
    assert(level3Type7Tail.level2Entities()[0].behaviorTimer == 0x1d);
    assert(level3Type7Tail.level2Entities()[0].forwardVelocity == 0);
    for (int tick = 0; tick < 29; ++tick) {
        stellar7::GameplayModelTestAccess::advanceLevel2(
            level3Type7Tail
        );
    }
    assert(level3Type7Tail.level2Entities()[0].stateBytes4dTo52[5] == 3);
    assert(level3Type7Tail.level2Entities()[0].behaviorTimer == 0x46);

    // The Type 7 state-2 hold expires independently of an already active
    // Type 44. Pinned loop 850 has timer 2 with the guardian alive, and the
    // craft is in state 3 by loop 859.
    stellar7::GameplayModel level3Type7HoldExpiryWithPostWave;
    stellar7::GameplayModelTestAccess::
        configureLevel3Type7HoldExpiryWithPostWave(
            level3Type7HoldExpiryWithPostWave
        );
    stellar7::GameplayModel level3Type7OffscreenWarp;
    stellar7::GameplayModelTestAccess::configureLevel3Type7OffscreenWarp(
        level3Type7OffscreenWarp
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type7HoldExpiryWithPostWave
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type7OffscreenWarp
    );
    assert(
        level3Type7HoldExpiryWithPostWave.level2Entities()[0]
            .stateBytes4dTo52[5] == 3
    );
    assert(
        level3Type7HoldExpiryWithPostWave.level2Entities()[0]
            .behaviorTimer == 0x46
    );
    // Pinned loops 851->852 enter state 3 with the Type 7 craft off-screen
    // (flags 00->08). FUN_1000:af8c installs FUN_1000:b0b4 but the new
    // presenter has no visible craft, so this transition consumes no Type 7
    // presentation RNG draws. It does not prove a one-tick dispatch delay.
    assert(
        level3Type7HoldExpiryWithPostWave.level2Rng().calls()
        == level3Type7OffscreenWarp.level2Rng().calls()
    );
    // Native Level 2 loop 1796 consumes the ten beam draws on the visible
    // state-2 -> state-3 transition itself. Visibility, not prior state,
    // distinguishes this from the off-screen Level 3 transition above.
    stellar7::GameplayModel level2VisibleHoldExpiry;
    stellar7::GameplayModel level2HiddenHoldExpiry;
    stellar7::GameplayModelTestAccess::configureLevel2Type7HoldExpiry(
        level2VisibleHoldExpiry, true
    );
    stellar7::GameplayModelTestAccess::configureLevel2Type7HoldExpiry(
        level2HiddenHoldExpiry, false
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(level2VisibleHoldExpiry);
    stellar7::GameplayModelTestAccess::advanceLevel2(level2HiddenHoldExpiry);
    assert(level2VisibleHoldExpiry.level2Entities()[0].stateBytes4dTo52[5] == 3);
    assert(level2VisibleHoldExpiry.level2Rng().calls()
        == level2HiddenHoldExpiry.level2Rng().calls() + 10);
    stellar7::GameplayModelTestAccess::assertGroundBeforeCompletionBeam();
    stellar7::GameplayModelTestAccess::assertCompletionBeamGuardsAndClipping();

    // FUN_1000:b0b4 emits ten streaks only when the installed
    // state-3 presenter receives an object carrying the projected-visible
    // flag. Each streak consumes one FUN_1000:b39e draw.
    stellar7::GameplayModel level3Type7VisibleWarp;
    stellar7::GameplayModelTestAccess::configureLevel3Type7VisibleWarp(
        level3Type7VisibleWarp
    );
    stellar7::GameplayModelTestAccess::advanceLevel2(
        level3Type7VisibleWarp
    );
    assert(
        level3Type7VisibleWarp.level2Rng().calls()
        == level3Type7OffscreenWarp.level2Rng().calls() + 10
    );
    for (int tick = 0; tick < 41; ++tick) {
        stellar7::GameplayModelTestAccess::advanceLevel2(
            level3Type7Tail
        );
    }
    const auto level3TailGuardian = std::find_if(
        level3Type7Tail.level2Entities().begin(),
        level3Type7Tail.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 44;
        }
    );
    assert(level3TailGuardian != level3Type7Tail.level2Entities().end());
    const auto level3TailGuardianIndex = std::find_if(
        level3Type7Tail.level2Entities().begin(),
        level3Type7Tail.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 44;
        }
    );
    assert(level3TailGuardianIndex != level3Type7Tail.level2Entities().end());

    // Finish state 3 without restarting the arrival sound. Native operation
    // 0x14 is a Sound Blaster no-op; the craft enters its climbing tail.
    for (int tick = 0; tick < 29; ++tick) {
        stellar7::GameplayModelTestAccess::advanceLevel2(
            level3Type7Tail
        );
    }
    const auto level3Type7AfterHandoff = std::find_if(
        level3Type7Tail.level2Entities().begin(),
        level3Type7Tail.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 7;
        }
    );
    assert(
        level3Type7AfterHandoff
        != level3Type7Tail.level2Entities().end()
    );
    assert(level3Type7AfterHandoff->stateBytes4dTo52[5] == 4);
    assert(level3Type7Tail.level2AudioEvents().empty());
    for (int tick = 0; tick < 120; ++tick) {
        stellar7::GameplayModelTestAccess::advanceLevel2(
            level3Type7Tail
        );
    }
    assert(
        std::find_if(
            level3Type7Tail.level2Entities().begin(),
            level3Type7Tail.level2Entities().end(),
            [](const stellar7::Level1Entity& entity) {
                return entity.type == 7;
            }
        ) == level3Type7Tail.level2Entities().end()
    );

    // A nonlethal native-equivalent player projectile hit decrements the
    // guardian health and retains the short hit effect without allocating a
    // pickup. Checkpoint 166's real DOSBox-X route observes this same
    // Type-44 20 -> 19 transition at loop 775.
    stellar7::GameplayModel level3GuardianNonlethalHit;
    stellar7::GameplayModelTestAccess::configureLevel3GuardianLethalProjectile(
        level3GuardianNonlethalHit,
        2
    );
    level3GuardianNonlethalHit.tick({});
    const auto level3GuardianAfterHit = std::find_if(
        level3GuardianNonlethalHit.level2Entities().begin(),
        level3GuardianNonlethalHit.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 44;
        }
    );
    assert(
        level3GuardianAfterHit
        != level3GuardianNonlethalHit.level2Entities().end()
    );
    assert(level3GuardianAfterHit->stateBytes4dTo52[1] == 1);
    assert(std::none_of(
        level3GuardianNonlethalHit.level2Entities().begin(),
        level3GuardianNonlethalHit.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 25;
        }
    ));
    const auto level3GuardianHitEffect = std::find_if(
        level3GuardianNonlethalHit.level2Entities().begin(),
        level3GuardianNonlethalHit.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == -1;
        }
    );
    assert(
        level3GuardianHitEffect
        != level3GuardianNonlethalHit.level2Entities().end()
    );
    assert(level3GuardianHitEffect->stateBytes4dTo52[1] == 2);
    assert(level3GuardianHitEffect->stateBytes4dTo52[4] == 12);
    assert(level3GuardianNonlethalHit.level2AudioEvents().back().eventId == 5);

    // Native Level 3 loop 1061->1062 processes the active-list Type-23 shot
    // before the moving Type-44 guardian. It tests the guardian's boundary-
    // start center and misses; using the guardian's pre-integrated center
    // would consume the shot and remove one health one tick too early.
    stellar7::GameplayModel level3ProjectileBeforeMovingGuardianMiss;
    stellar7::GameplayModelTestAccess::
        configureLevel3ProjectileBeforeMovingGuardianMiss(
            level3ProjectileBeforeMovingGuardianMiss
        );
    level3ProjectileBeforeMovingGuardianMiss.tick({});
    const auto movingGuardianAfterMiss = std::find_if(
        level3ProjectileBeforeMovingGuardianMiss.level2Entities().begin(),
        level3ProjectileBeforeMovingGuardianMiss.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 44;
        }
    );
    assert(
        movingGuardianAfterMiss
        != level3ProjectileBeforeMovingGuardianMiss.level2Entities().end()
    );
    assert(movingGuardianAfterMiss->stateBytes4dTo52[1] == 20);
    assert(std::any_of(
        level3ProjectileBeforeMovingGuardianMiss.level2Entities().begin(),
        level3ProjectileBeforeMovingGuardianMiss.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 23;
        }
    ));
    assert(std::none_of(
        level3ProjectileBeforeMovingGuardianMiss.level2Entities().begin(),
        level3ProjectileBeforeMovingGuardianMiss.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == -1;
        }
    ));

    // Native route-v32 loop 2124->2125 keeps the inherited collision walk
    // (returning Type 44, struck Type 44, projectile) through FUN_1000:0784.
    // The later projectile impact rebuild must not reorder that already-run
    // collision pass. At 1000:09ef the struck guardian has integrated once
    // from (4861,-1746) to (4578,-1645) and has no fresh overlap to back out.
    stellar7::GameplayModel level3ProjectileRebuildCollisionOrder;
    stellar7::GameplayModelTestAccess::
        configureLevel3ProjectileRebuildCollisionOrder(
            level3ProjectileRebuildCollisionOrder
        );
    level3ProjectileRebuildCollisionOrder.tick({});
    const auto struckAfterRebuild = std::find_if(
        level3ProjectileRebuildCollisionOrder.level2Entities().begin(),
        level3ProjectileRebuildCollisionOrder.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 44
                && entity.position.z == 500;
        }
    );
    assert(
        struckAfterRebuild
        != level3ProjectileRebuildCollisionOrder.level2Entities().end()
    );
    assert(std::any_of(
        level3ProjectileRebuildCollisionOrder.level2Entities().begin(),
        level3ProjectileRebuildCollisionOrder.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == -1;
        }
    ));
    assert(struckAfterRebuild->position.x == 4578);
    assert(struckAfterRebuild->position.y == -1645);

    // Native route-v32 loop 2134->2135 retires the terminal Type -1 effect
    // only after FUN_1000:0784. The inherited order therefore remains
    // returning Type 44, effect, struck Type 44 for collision timing. The
    // returning guardian overlaps the struck guardian's boundary-start
    // center and removes its own 300-unit translation.
    stellar7::GameplayModel level3RetiringEffectCollisionOrder;
    stellar7::GameplayModelTestAccess::
        configureLevel3RetiringEffectCollisionOrder(
            level3RetiringEffectCollisionOrder
        );
    level3RetiringEffectCollisionOrder.tick({});
    const auto returningAfterEffectRetirement = std::find_if(
        level3RetiringEffectCollisionOrder.level2Entities().begin(),
        level3RetiringEffectCollisionOrder.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 44
                && entity.position.z == 2100;
        }
    );
    assert(
        returningAfterEffectRetirement
        != level3RetiringEffectCollisionOrder.level2Entities().end()
    );
    assert(returningAfterEffectRetirement->position.x == 5949);
    assert(returningAfterEffectRetirement->position.y == -4066);

    // Natural Level 3 loop 3292 proves that the guardian callback deploys
    // the fixed-pool pickup before the impacting projectile pointer is
    // removed. That final swap-back leaves the pickup before both surviving
    // projectile pointers in the allocation list.
    stellar7::GameplayModel level3GuardianLethalPointerOrder;
    stellar7::GameplayModelTestAccess::
        configureLevel3GuardianLethalWithSurvivingProjectiles(
            level3GuardianLethalPointerOrder
        );
    level3GuardianLethalPointerOrder.tick({});
    const std::vector<std::int16_t> expectedGuardianLethalAllocationTypes{
        -1,
        25,
        23,
        23,
    };
    assert(
        stellar7::GameplayModelTestAccess::level2AllocationTypes(
            level3GuardianLethalPointerOrder
        ) == expectedGuardianLethalAllocationTypes
    );
    const auto level3GuardianBoundaryPickup = std::find_if(
        level3GuardianLethalPointerOrder.level2Entities().begin(),
        level3GuardianLethalPointerOrder.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 25;
        }
    );
    assert(
        level3GuardianBoundaryPickup
        != level3GuardianLethalPointerOrder.level2Entities().end()
    );
    assert(level3GuardianBoundaryPickup->presenterDistance == 0);
    assert(level3GuardianBoundaryPickup->targetHeading == 0);

    // A lethal player projectile against the Level 3 guardian creates the
    // Type-25 pickup; contact with that pickup is the shared terminal gate.
    // Keep this two-boundary sequence separate from the controlled native
    // substitution probe so it verifies the portable collision ordering.
    stellar7::GameplayModel level3GuardianSuccess;
    stellar7::GameplayModelTestAccess::configureLevel3GuardianLethalProjectile(
        level3GuardianSuccess
    );
    level3GuardianSuccess.tick({});
    assert(
        std::find_if(
            level3GuardianSuccess.level2Entities().begin(),
            level3GuardianSuccess.level2Entities().end(),
            [](const stellar7::Level1Entity& entity) {
                return entity.type == 44;
            }
        ) == level3GuardianSuccess.level2Entities().end()
    );
    const auto level3GuardianDeathEffect = std::find_if(
        level3GuardianSuccess.level2Entities().begin(),
        level3GuardianSuccess.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == -1;
        }
    );
    assert(
        level3GuardianDeathEffect
        != level3GuardianSuccess.level2Entities().end()
    );
    assert(level3GuardianDeathEffect->flags == 0x8c);
    const std::array<std::uint8_t, 6> expectedGuardianDeathState{
        0xfc, 0x0f, 0, 0, 0x11, 0,
    };
    assert(
        level3GuardianDeathEffect->stateBytes4dTo52
        == expectedGuardianDeathState
    );
    const auto level3GuardianPickup = std::find_if(
        level3GuardianSuccess.level2Entities().begin(),
        level3GuardianSuccess.level2Entities().end(),
        [](const stellar7::Level1Entity& entity) {
            return entity.type == 25;
        }
    );
    assert(level3GuardianPickup != level3GuardianSuccess.level2Entities().end());
    // The native loop-774 health transplant preserves the accepted natural
    // collision and reaches loop 775 with the pickup displaced until it no
    // longer overlaps the destroyed Type-44 collision volume.  The
    // projectile radius is much smaller, so this position distinguishes the
    // original FUN_1000:45c6 clearance owner.
    assert((
        level3GuardianPickup->position
        == stellar7::FixedPosition{0, 1400, 0}
    ));
    // Native Level 3 allocations retain a zero collision radius because the
    // LEVEL3.RES model table has no Type-25 radius entry.  This explains why
    // the moving pickup cannot be collected by ordinary steering; the
    // shared presenter contact remains a controlled exact-position boundary.
    assert(level3GuardianPickup->collisionRadius == 0);
    const std::array<std::uint8_t, 6> expectedGuardianPickupState{
        0xfe, 0x64, 0x03, 0, 0, 1,
    };
    assert(
        level3GuardianPickup->stateBytes4dTo52
        == expectedGuardianPickupState
    );
    assert(level3GuardianSuccess.level2AudioEvents().size() == 1);
    assert(level3GuardianSuccess.level2AudioEvents().front().eventId == 12);
    assert(
        level3GuardianSuccess.phase() == stellar7::GameplayPhase::level3
    );
    stellar7::GameplayModelTestAccess::setLevel2PlayerPosition(
        level3GuardianSuccess,
        level3GuardianPickup->position
    );
    level3GuardianSuccess.tick({});
    assert(
        stellar7::GameplayModelTestAccess::level2Complete(
            level3GuardianSuccess
        )
    );
    assert(
        level3GuardianSuccess.phase()
        == stellar7::GameplayPhase::level3SuccessPresentation
    );
    assert(level3GuardianSuccess.level3CompletionByPickup());

    stellar7::GameplayModel level3Completion;
    stellar7::GameplayModelTestAccess::configureLevel3Completion(
        level3Completion
    );
    level3Completion.tick({});
    assert(
        level3Completion.phase()
        == stellar7::GameplayPhase::level3SuccessPresentation
    );
    for (
        unsigned frame = 0;
        frame + 1
            < stellar7::GameplayModel::level3SuccessPresentationFrames;
        ++frame
    ) {
        level3Completion.tick({});
    }
    assert(
        level3Completion.presentationFrame()
        == stellar7::GameplayModel::level3SuccessPresentationFrames - 1
    );
    level3Completion.tick({});
    assert(
        level3Completion.phase()
        == stellar7::GameplayPhase::level4Handoff
    );
    assert(level3Completion.presentationFrame() == 0);
    level3Completion.tick({});
    assert(level3Completion.phase() == stellar7::GameplayPhase::level4Handoff);


    auto level3RenderAssets = assets;
    level3RenderAssets.campaignLevels.resize(3);
    level3RenderAssets.campaignLevels[2].resourceName = "LEVEL3.RES";
    level3RenderAssets.campaignLevels[2].bitmapGroups = {{
        {256, 17, std::vector<std::uint8_t>(256 * 17, 123)},
        {256, 17, std::vector<std::uint8_t>(256 * 17, 123)},
        {256, 17, std::vector<std::uint8_t>(256 * 17, 123)},
        {256, 17, std::vector<std::uint8_t>(256 * 17, 123)},
    }};
    // A captured terminal frame must replace the synthetic cockpit surface
    // at the same presentation index that the native presenter reached.
    stellar7::GameplayModel level3CapturedSuccess;
    stellar7::GameplayModelTestAccess::configureLevel3Completion(
        level3CapturedSuccess
    );
    level3CapturedSuccess.tick({});
    auto capturedFrame0 = level3RenderAssets.cockpit;
    auto capturedFrame1 = level3RenderAssets.cockpit;
    capturedFrame0.pixels[0] = 17;
    capturedFrame1.pixels[0] = 29;
    level3RenderAssets.level3SuccessFrames = {
        capturedFrame0,
        capturedFrame1,
    };
    // Controlled save-state presenter captures contain deterministic stale
    // Mode-X page pixels. Ordinary interactive rendering must not import
    // those surfaces until a natural-run display capture proves them.
    assert(
        stellar7::renderGameplayIndexed(
            level3RenderAssets,
            level3CapturedSuccess
        ).pixels[0] != 17
    );
    level3RenderAssets.useUnprovenLevel3SuccessSurfaces = true;
    assert(
        stellar7::renderGameplayIndexed(
            level3RenderAssets,
            level3CapturedSuccess
        ).pixels[0] == 17
    );
    level3CapturedSuccess.tick({});
    assert(
        stellar7::renderGameplayIndexed(
            level3RenderAssets,
            level3CapturedSuccess
        ).pixels[0] == 29
    );

    // Type-25 contact enters the shared presenter through its partial-redraw
    // layout.  Keep that route distinct from the canonical completion track.
    auto contactFrame0 = level3RenderAssets.cockpit;
    auto contactFrame1 = level3RenderAssets.cockpit;
    contactFrame0.pixels[0] = 41;
    contactFrame1.pixels[0] = 43;
    level3RenderAssets.level3ContactSuccessFrames = {
        contactFrame0,
        contactFrame1,
    };
    auto handoffFrame = level3RenderAssets.cockpit;
    handoffFrame.pixels[0] = 67;
    level3RenderAssets.level3HandoffFrames = {handoffFrame};
    assert(
        stellar7::renderGameplayIndexed(
            level3RenderAssets,
            level3Completion
        ).pixels[0] == 67
    );
    assert(
        stellar7::renderGameplayIndexed(
            level3RenderAssets,
            level3GuardianSuccess
        ).pixels[0] == 41
    );
    const auto level3ContactInitial = stellar7::renderGameplayIndexed(
        level3RenderAssets,
        level3GuardianSuccess
    );
    assert((level3ContactInitial.palette[16] == stellar7::Rgb{0, 255, 0}));
    assert((level3ContactInitial.palette[63] == stellar7::Rgb{0, 65, 0}));
    level3GuardianSuccess.tick({});
    const auto level3ContactAfterOnePortableTick =
        stellar7::renderGameplayIndexed(
            level3RenderAssets,
            level3GuardianSuccess
        );
    assert(level3ContactAfterOnePortableTick.pixels[0] == 43);
    // FUN_1000:516b waits for 70 Hz vertical retrace. One 33 ms portable tick
    // therefore advances two native triangle-wave writes.
    assert((
        level3ContactAfterOnePortableTick.palette[16]
        == stellar7::Rgb{0, 247, 0}
    ));
    assert((
        level3ContactAfterOnePortableTick.palette[17]
        == stellar7::Rgb{0, 251, 0}
    ));
    // The exact presenter loop performs palette writes only after the portal
    // surface is complete. Screenshot-derived contact frames 98+ contain a
    // wipe and illustrated scene from post-return capture side effects and
    // must not be selected while the Type-25 presenter remains active.
    level3RenderAssets.level3ContactSuccessFrames.assign(
        99,
        level3RenderAssets.cockpit
    );
    level3RenderAssets.level3ContactSuccessFrames[97].pixels[0] = 47;
    level3RenderAssets.level3ContactSuccessFrames[98].pixels[0] = 89;
    for (unsigned frame = 1; frame < 98; ++frame) {
        level3GuardianSuccess.tick({});
    }
    assert(level3GuardianSuccess.presentationFrame() == 98);
    assert(
        stellar7::renderGameplayIndexed(
            level3RenderAssets,
            level3GuardianSuccess
        ).pixels[0] == 47
    );
    const auto level3Frame = stellar7::renderGameplayIndexed(
        level3RenderAssets,
        level3Entry
    );
    assert(
        std::count(
            level3Frame.pixels.begin(),
            level3Frame.pixels.end(),
            static_cast<std::uint8_t>(123)
        ) > 1000
    );
    // The executable's campaign-name table maps level index 2 to RIGEL.
    // Its leading R sets the top-left header pixel that ANTARES leaves clear.
    assert(level3Frame.pixels[30 * 320 + 132] == 0xef);

    // Level 3's player projectile has a zero live collision radius, but the
    // native forward-fire route still presents and collides with the shot.
    // The Level 1 tick-550 apparent-size-zero suppression must not hide it.
    stellar7::GameplayModel level3VisibleShot;
    stellar7::GameplayModel level3IdleShotControl;
    level3VisibleShot.startLevel3();
    level3IdleShotControl.startLevel3();
    level3VisibleShot.tick({false, false, false, false, true, false});
    level3IdleShotControl.tick({});
    level3VisibleShot.tick({});
    level3IdleShotControl.tick({});
    auto level3ShotAssets = level3RenderAssets;
    level3ShotAssets.models.resize(24);
    auto level3ShotModel = traceModel;
    level3ShotModel.lods[0].groups[0].vertices = {
        {-32, 0, 0},
        {32, 0, 0},
        {0, 0, 32},
    };
    level3ShotAssets.models[23] = std::move(level3ShotModel);
    const auto level3ShotFrame = stellar7::renderGameplayIndexed(
        level3ShotAssets,
        level3VisibleShot
    );
    const auto level3IdleShotFrame = stellar7::renderGameplayIndexed(
        level3ShotAssets,
        level3IdleShotControl
    );
    auto level3ShotViewportDifference = std::size_t{0};
    for (int y = 24; y <= 145; ++y) {
        for (int x = 24; x <= 295; ++x) {
            if (
                level3ShotFrame.pixels[static_cast<std::size_t>(y * 320 + x)]
                != level3IdleShotFrame.pixels[
                    static_cast<std::size_t>(y * 320 + x)
                ]
            ) {
                ++level3ShotViewportDifference;
            }
        }
    }
    assert(level3ShotViewportDifference != 0);

    // FUN_1000:6f52 is the shared terminal presenter reached by the Level 3
    // completion path.  The terminal source page is the cached cockpit, not
    // the active RIGEL viewport; the campaign horizon marker must therefore
    // disappear when the success frame is frozen.
    level3RenderAssets.level3SuccessFrames.clear();
    stellar7::GameplayModel level3TerminalCompletion;
    stellar7::GameplayModelTestAccess::configureLevel3Completion(
        level3TerminalCompletion
    );
    level3TerminalCompletion.tick({});
    const auto level3SuccessFrame = stellar7::renderGameplayIndexed(
        level3RenderAssets,
        level3TerminalCompletion
    );
    // The terminal presenter draws its sight after the cached cockpit page;
    // this guards the native ordering recovered at Level 3 presenter entry.
    assert(level3SuccessFrame.pixels[75 * 320 + 143] == 95);
    assert(
        std::count(
            level3SuccessFrame.pixels.begin(),
            level3SuccessFrame.pixels.end(),
            static_cast<std::uint8_t>(123)
        ) == 0
    );
    const auto assertInitialization = [](const stellar7::CockpitInitializationRecord& record,
        const std::uint8_t level, const std::array<stellar7::CockpitIndicatorState, 4>& states) {
        assert(record.levelIndex == level);
        for (std::size_t index = 0; index < states.size(); ++index) {
            assert(record.states[index].x == states[index].x);
            assert(record.states[index].velocity == states[index].velocity);
        }
    };
    stellar7::GameplayModel initialCockpitRecord;
    const std::array<stellar7::CockpitIndicatorState, 4> initialStates{{
        {130, 4}, {134, 4}, {138, 4}, {142, 4}}};
    assertInitialization(initialCockpitRecord.lastCockpitInitialization(), 0, initialStates);
    initialCockpitRecord.tick({});
    assertInitialization(initialCockpitRecord.lastCockpitInitialization(), 0, initialStates);
    initialCockpitRecord.startLevel2();
    assertInitialization(initialCockpitRecord.lastCockpitInitialization(), 0, initialStates);
    initialCockpitRecord.startLevel3();
    assertInitialization(initialCockpitRecord.lastCockpitInitialization(), 0, initialStates);

    for (const auto phase : {stellar7::GameplayPhase::successPresentation,
             stellar7::GameplayPhase::level2SuccessPresentation}) {
        stellar7::GameplayModel indicatorTransition;
        stellar7::GameplayModelTestAccess::configureIndicatorTransition(
            indicatorTransition, phase, 151, 4);
        indicatorTransition.tick({});
        assert(indicatorTransition.cockpitIndicatorX() == 163);
        assert(indicatorTransition.level2CockpitIndicatorX() == 163);
        assert(indicatorTransition.level2CockpitIndicatorVelocity() == 4);
        const auto level = static_cast<std::uint8_t>(
            phase == stellar7::GameplayPhase::successPresentation ? 1 : 2);
        const std::array<stellar7::CockpitIndicatorState, 4> forwardStates{{
            {151, 4}, {155, 4}, {159, 4}, {163, 4}}};
        assertInitialization(indicatorTransition.lastCockpitInitialization(), level, forwardStates);
        indicatorTransition.tick({});
        assertInitialization(indicatorTransition.lastCockpitInitialization(), level, forwardStates);
        stellar7::GameplayModelTestAccess::configureIndicatorTransition(
            indicatorTransition, phase, 175, 4);
        indicatorTransition.tick({});
        assert(indicatorTransition.level2CockpitIndicatorX() == 171);
        assert(indicatorTransition.level2CockpitIndicatorVelocity() == -4);
        const std::array<stellar7::CockpitIndicatorState, 4> upperTurnStates{{
            {175, 4}, {179, 4}, {175, -4}, {171, -4}}};
        assertInitialization(indicatorTransition.lastCockpitInitialization(), level, upperTurnStates);
        stellar7::GameplayModelTestAccess::configureIndicatorTransition(
            indicatorTransition, phase, 133, -4);
        indicatorTransition.tick({});
        const std::array<stellar7::CockpitIndicatorState, 4> lowerTurnStates{{
            {133, -4}, {129, -4}, {133, 4}, {137, 4}}};
        assertInitialization(indicatorTransition.lastCockpitInitialization(), level, lowerTurnStates);
    }

}
