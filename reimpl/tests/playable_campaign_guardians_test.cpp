#include "stellar7/gameplay.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace {
void require(const bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
}

namespace stellar7 {
struct GameplayModelTestAccess {
    static void clearedWave(GameplayModel& model, const std::uint8_t level,
        const std::vector<Model3d>& shared, const std::vector<Model3d>& local) {
        StellarRng rng;
        const auto definition = campaignLevelDefinition(level, 0);
        auto actors = constructCampaignLoaderActors(definition.table, rng, shared, local);
        //Controlled cleared ordinary wave, retaining actual fixed constructors/RNG.
        actors.erase(std::remove_if(actors.begin(), actors.end(), [](const auto& actor) {
            return actor.stateBytes4dTo52[0] == 0xff;
        }), actors.end());
        model.playableCampaignFrames_ = true;
        model.initializeRetainedCampaignLevel(level, std::move(actors), rng, shared, local);
        model.level2ActiveEnemyCount_ = 0;
        model.level2SpawnedEnemyCount_ = definition.table.enemyLimit;
    }

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
                && actor.stateBytes4dTo52 == physical.stateBytes4dTo52
                && actor.renderProfile == physical.renderProfile && actor.yaw == physical.yaw
                && actor.collisionRadius == physical.collisionRadius, "guardian route lost its physical pool alias");
        }
    }

    static void lateFight(GameplayModel& model, const std::uint8_t level,
        const std::vector<Model3d>& shared, const std::vector<Model3d>& local) {
        clearedWave(model, level, shared, local);
        model.completePlayableCampaignPreloop();
        model.level2Rng_.next();
        model.level2Rng_.next();
        Level1Entity guardian{};
        guardian.type = 44;
        guardian.position = {0, 10000, 0};
        guardian.presenterDistance = 10000;
        guardian.targetHeading = 0x8000;
        guardian.stateBytes4dTo52 = {0xff, 1, 2, 0, 5, 0};
        const auto boss = model.allocateLevel2Actor(guardian);
        model.level2PostWaveObjectIndex_ = boss;
        model.level2ActiveEnemyCount_ = 1;
        model.level2SpawnedEnemyCount_ += 2;
        Level1Entity shot{};
        shot.type = 23;
        shot.position = {0, 9500, 0};
        shot.collisionRadius = campaignModelRadius(shared, local, 23);
        shot.forwardVelocity = 500;
        shot.stateBytes4dTo52 = {0xfd, 45, 0, 0, 0, 0};
        shot.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
        shot.animatedModelPresenterEnabled = false;
        shot.ownerIsPlayer = true;
        shot.allocationIndex = model.level2Entities_.size();
        model.level2Entities_.push_back(shot);
        model.level2EntityActorPoolSlots_.push_back(model.level2ActorPoolSize_);
        model.level2EntityFixedPoolSlots_.push_back(model.level2FixedObjectPoolSize_);
        model.level2EntityProjectilePoolSlots_.push_back(0);
        model.level2EntityDeathEffectPoolSlots_.push_back(model.level2DeathEffectPoolSize_);
        model.level2ProjectilePool_[0] = shot;
        model.level2ProjectilePoolActive_[0] = true;
        model.level2AllocationListOrder_.push_back(shot.allocationIndex);
        model.level2ObjectListOrder_ = model.level2AllocationListOrder_;
        model.level2ActiveListOrder_ = model.level2AllocationListOrder_;
        model.level2Player_.weaponSlotTimers[0] = 13;
        model.phase_ = GameplayPhase::playable;
        model.level2LoopTick_ = 20;
        model.level2HasAdvanced_ = true;
        model.level2TailConsumed_ = true;
        model.level3RadarFinished_ = true;
    }

    static void pulseContact(GameplayModel& model, const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local) {
        constexpr std::uint8_t level = 5;
        clearedWave(model, level, shared, local);
        model.completePlayableCampaignPreloop();
        Level1Entity request{};
        request.type = 46;
        request.collisionRadius = campaignModelRadius(shared, local, 46);
        model.allocateLevel2FixedObject(request);
        model.level2ActiveListOrder_ = model.level2AllocationListOrder_;
        model.phase_ = GameplayPhase::playable;
        model.level2LoopTick_ = 20;
        model.level2HasAdvanced_ = true;
        model.level2TailConsumed_ = true;
        model.level3RadarFinished_ = true;
    }
};
} //namespace stellar7

namespace {
using namespace stellar7;
constexpr std::size_t noObject = static_cast<std::size_t>(-1);

std::vector<Model3d> models(const std::filesystem::path& root, const std::string& name) {
    std::ifstream file(root / name, std::ios::binary);
    if (!file) { throw std::runtime_error("missing model resource: " + name); }
    return decodeFirstModelTableResource({std::istreambuf_iterator<char>(file), {}});
}

std::size_t find(const GameplayModel& model, const std::int16_t type) {
    const auto& actors = model.level2Entities();
    const auto found = std::find_if(actors.begin(), actors.end(), [type](const auto& actor) {
        return actor.type == type && actor.active();
    });
    return found == actors.end() ? noObject : static_cast<std::size_t>(found - actors.begin());
}

GameplayInput approach(const GameplayModel& model, const Level1Entity& target, const bool combat) {
    const auto& player = model.level2Player();
    const auto error = static_cast<std::int16_t>(presenterTargetHeading(player.position, target.position) - player.yaw);
    const auto braking = static_cast<int>(error) - 4 * player.yawVelocity;
    const auto distance = approximatePlanarDistance(player.position, target.position);
    GameplayInput input{};
    input.left = braking > 120;
    input.right = braking < -120;
    input.fire = combat && std::abs(static_cast<int>(error)) < 1500;
    input.forward = (combat ? distance > 6000 : true) && std::abs(static_cast<int>(error)) < 4000;
    input.reverse = combat && distance < 3500;
    return input;
}

void botRoute(const std::vector<Model3d>& shared, const std::vector<Model3d>& local, const std::uint8_t level) {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::clearedWave(*model, level, shared, local);
    model->completePlayableCampaignPreloop();
    bool craft = false, guardian = false, moved = false, pulse = false, emission = false;
    auto guardianFrame = unsigned{0};
    FixedPosition initialGuardian{};
    const auto initialRandom = model->level2Rng().calls();
    constexpr unsigned maximumFrames = 12000;
    for (unsigned frame = 0; frame < maximumFrames; ++frame) {
        const auto boss = find(*model, 44);
        const auto reward = find(*model, 25);
        GameplayInput input{};
        if (boss != noObject) {
            input = approach(*model, model->level2Entities()[boss], true);
            input.fire = input.fire && frame >= guardianFrame + (level == 5 ? 400U : 100U);
        } else if (reward != noObject) {
            input = approach(*model, model->level2Entities()[reward], false);
            input.fire = true;
        } else {
            const auto craftIndex = find(*model, 7);
            if (craftIndex != noObject && model->level2Entities()[craftIndex].stateBytes4dTo52[5] != 0) {
                auto escape = model->level2Entities()[craftIndex];
                escape.position.x = 2 * model->level2Player().position.x - escape.position.x;
                escape.position.y = 2 * model->level2Player().position.y - escape.position.y;
                input = approach(*model, escape, false);
            }
        }
        const auto before = model->level2LoopTick();
        try {
            model->advancePlayableCampaign(input);
            GameplayModelTestAccess::aliases(*model);
            require(model->level2LoopTick() == static_cast<std::uint16_t>(before + 1), "guardian main repeated or skipped a frame");
            if (model->phase() == GameplayPhase::endingEntry || model->phase() == GameplayPhase::level2SuccessPresentation) {
                require(craft && guardian && moved && model->score() >= 1000, "guardian route skipped arrival, behavior or defeat");
                require(model->level2Rng().calls() > initialRandom, "guardian route lost continuing RNG");
                for (const auto id : {17, 12}) {
                    require(std::count_if(model->level2AudioEvents().begin(), model->level2AudioEvents().end(),
                        [id](const auto& event) { return event.eventId == id; }) == 1, "guardian route duplicated arrival or defeat");
                }
                if (level == 6) {
                    require(model->phase() == GameplayPhase::endingEntry && model->campaignLevel() == 7,
                        "AA26 defeat created a reward instead of7512 ending entry");
                    require(find(*model, 25) == noObject, "Level7 defeat fabricated a green reward");
                } else {
                    require(model->phase() == GameplayPhase::level2SuccessPresentation && find(*model, 25) != noObject,
                        "guardian defeat lost actual reward/contact");
                    model->setSuccessAudioActive(true);
                    model->setSuccessAudioActive(false);
                    model->resumePlayableCampaignSuccess();
                    require(model->phase() == GameplayPhase::level2SuccessReturn, "guardian success lost its suspended parent");
                    GameplayModelTestAccess::aliases(*model);
                }
                std::cout << "Controlled Level" << unsigned(level + 1) << " cleared wave -> craft/guardian/defeat"
                    << (level == 6 ? "/ending" : "/reward/contact/return") << " frames=" << frame + 1
                    << " pulse=" << pulse << " emission=" << emission << " armor=" << model->playerArmor() << '\n';
                return;
            }
            require(model->phase() == GameplayPhase::playable, "guardian route left live combat before success");
            craft = craft || find(*model, 7) != noObject;
            const auto current = find(*model, 44);
            if (current != noObject) {
                const auto& actor = model->level2Entities()[current];
                if (!guardian) {
                    guardian = true;
                    guardianFrame = frame;
                    initialGuardian = actor.position;
                    require(actor.collisionRadius == campaignModelRadius(shared, local, 44), "guardian lost actual local metadata");
                } else {
                    moved = moved || !(actor.position == initialGuardian);
                }
            }
            pulse = pulse || find(*model, 46) != noObject;
            emission = emission || find(*model, 48) != noObject;
            model->finishLevel3Radar();
            model->finishPlayableCampaignFrame();
            GameplayModelTestAccess::aliases(*model);
        } catch (...) {
            std::cerr << "Level" << unsigned(level + 1) << " frame=" << frame << " phase=" << static_cast<int>(model->phase())
                << " armor=" << model->playerArmor() << " score=" << model->score() << '\n';
            for (const auto& actor : model->level2Entities()) {
                std::cerr << " type=" << actor.type << " health=" << unsigned(actor.stateBytes4dTo52[1])
                    << " state=" << unsigned(actor.stateBytes4dTo52[5]) << " position=" << actor.position.x << ','
                    << actor.position.y << ',' << actor.position.z << " profile=" << static_cast<int>(actor.renderProfile) << '\n';
            }
            throw;
        }
    }
    throw std::runtime_error("guardian route did not reach its retained exit");
}

void actualConstructor(const std::vector<Model3d>& shared, const std::vector<Model3d>& local, const std::uint8_t level) {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::clearedWave(*model, level, shared, local);
    model->completePlayableCampaignPreloop();
    const auto incoming = model->level2Rng().calls();
    bool craft = false;
    unsigned callbacks = 0;
    constexpr std::array<std::uint8_t, 7> health{{15, 20, 20, 40, 50, 50, 20}};
    for (unsigned frame = 0; frame < 600; ++frame) {
        model->advancePlayableCampaign({});
        require(model->phase() == GameplayPhase::playable, "constructor fixture left combat before guardian entry");
        craft = craft || find(*model, 7) != noObject;
        const auto boss = find(*model, 44);
        if (boss != noObject) {
            const auto& actor = model->level2Entities()[boss];
            require(actor.collisionRadius == campaignModelRadius(shared, local, 44), "477A lost actual guardian radius");
            require(actor.stateBytes4dTo52[1] == health[level], "477A lost actual initial guardian health");
            require(actor.stateBytes4dTo52[4] == 5, "477A lost guardian classification");
            if (level == 0) {
                require(local.front().flags == 3 && local.front().animationLimits == std::vector<std::uint8_t>{8, 8, 8},
                    "A6BE scalar invariant requires all three actual eight-variant selectors");
                require(actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation
                    || actor.renderProfile == WorldActorRenderProfile::heapSelectorNoCallback,
                    "A6BE lost its owned three-selector heap");
            }
            ++callbacks;
        }
        GameplayModelTestAccess::aliases(*model);
        model->finishLevel3Radar();
        model->finishPlayableCampaignFrame();
        if (callbacks == 3) {
            require(craft && model->level2Rng().calls() > incoming, "477A bypassed actual arrival or continuing RNG");
            std::cout << "Controlled Level" << unsigned(level + 1) << " cleared wave -> actual craft/477A health"
                << unsigned(health[level]) << "/three main boundaries\n";
            return;
        }
    }
    throw std::runtime_error("actual guardian constructor was not reached");
}

void lateFight(const std::vector<Model3d>& shared, const std::vector<Model3d>& local, const std::uint8_t level) {
    auto model = std::make_unique<GameplayModel>();
    //Declared late-fight entry: legal health1 and one actual in-flight cannon
    //record after19 lifetime ticks. This does not claim natural boss victory.
    GameplayModelTestAccess::lateFight(*model, level, shared, local);
    bool defeated = false;
    for (unsigned frame = 0; frame < 2000; ++frame) {
        const auto reward = find(*model, 25);
        auto input = reward != noObject ? approach(*model, model->level2Entities()[reward], false) : GameplayInput{};
        model->advancePlayableCampaign(input);
        GameplayModelTestAccess::aliases(*model);
        defeated = defeated || find(*model, 44) == noObject;
        if (model->phase() == GameplayPhase::endingEntry) {
            require(level == 6 && defeated && model->campaignLevel() == 7 && model->score() == 1000,
                "Level7 1176 did not enter7512 with its actual defeat");
            require(find(*model, 25) == noObject, "Level7 defeat created a green reward");
            std::cout << "Controlled Level7 health1/one FD -> actual1176/7512 ending\n";
            return;
        }
        if (model->phase() == GameplayPhase::level2SuccessPresentation) {
            require(level != 6 && defeated && model->score() >= 1000, "late fight skipped guardian defeat");
            model->setSuccessAudioActive(true);
            model->setSuccessAudioActive(false);
            model->resumePlayableCampaignSuccess();
            require(model->phase() == GameplayPhase::level2SuccessReturn, "late fight lost actual reward/contact return");
            GameplayModelTestAccess::aliases(*model);
            std::cout << "Controlled Level" << unsigned(level + 1) << " health1/one FD -> actual1176/25/contact/return\n";
            return;
        }
        require(model->phase() == GameplayPhase::playable, "late fight left combat before its exit");
        model->finishLevel3Radar();
        model->finishPlayableCampaignFrame();
    }
    throw std::runtime_error("late-fight fixture did not reach its retained exit");
}

void pendingReward(const std::vector<Model3d>& shared, const std::vector<Model3d>& local) {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::lateFight(*model, 1, shared, local);
    for (unsigned frame = 0; frame < 50; ++frame) {
        model->advancePlayableCampaign({});
        require(model->phase() == GameplayPhase::playable, "pending reward fixture left combat");
        require(find(*model, 7) == noObject && find(*model, 44) == noObject && find(*model, 45) == noObject,
            "pending reward restarted the guardian arrival");
        require(find(*model, 25) != noObject, "guardian defeat lost its pending reward");
        GameplayModelTestAccess::aliases(*model);
        model->finishLevel3Radar();
        model->finishPlayableCampaignFrame();
    }
    const auto& events = model->level2AudioEvents();
    require(std::none_of(events.begin(), events.end(), [](const auto& event) { return event.eventId == 17; }),
        "pending reward repeated event17");
    require(std::count_if(events.begin(), events.end(), [](const auto& event) { return event.eventId == 12; }) == 1,
        "pending reward repeated guardian defeat");
    std::cout << "Controlled Level2 health1/one FD: pending reward retains dead guardian for50frames\n";
}

void pulseContact(const std::vector<Model3d>& shared, const std::vector<Model3d>& local) {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::pulseContact(*model, shared, local);
    const auto before = model->playerArmor();
    model->advancePlayableCampaign({});
    require(model->playerArmor() == before - 1, "Level6 pulse did not perform0890's direct armor decrement");
    require(std::none_of(model->level2AudioEvents().begin(), model->level2AudioEvents().end(),
        [](const auto& event) { return event.eventId == 6; }), "direct pulse armor decrement invoked0E7C audio");
    GameplayModelTestAccess::aliases(*model);
    model->finishLevel3Radar();
    model->finishPlayableCampaignFrame();
    std::cout << "Controlled Level6 Type46 contact: one armor, no0E7C impact event\n";
}
} //namespace

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) { return 2; }
    try {
        const std::filesystem::path root(argv[1]);
        const auto shared = models(root, "COCKPIT.RES");
        bool passed = true;
        for (const std::uint8_t level : {0, 3, 4, 5, 6}) {
            try {
                const auto local = models(root, "LEVEL" + std::to_string(level + 1) + ".RES");
                if (argc == 3 && std::string(argv[2]) == "--bot") {
                    botRoute(shared, local, level);
                } else {
                    actualConstructor(shared, local, level);
                    lateFight(shared, local, level);
                }
            } catch (const std::exception& error) {
                passed = false;
                std::cerr << error.what() << '\n';
            }
        }
        if (argc == 2) {
            pulseContact(shared, models(root, "LEVEL6.RES"));
            pendingReward(shared, models(root, "LEVEL2.RES"));
        }
        return passed ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
