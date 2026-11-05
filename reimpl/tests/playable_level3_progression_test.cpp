#include "playable_campaign_pilot.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace stellar7 {
struct GameplayModelTestAccess {
    static void constructors(GameplayModel& model, const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local) {
        StellarRng rng;
        auto actors = constructCampaignLoaderActors(campaignLevelDefinition(2, 0).table, rng, shared, local);
        model.playableCampaignFrames_ = true;
        model.initializeRetainedCampaignLevel(2, std::move(actors), rng, shared, local);
    }
    static std::int16_t active(const GameplayModel& model) { return model.level2ActiveEnemyCount_; }
    static std::int16_t spawned(const GameplayModel& model) { return model.level2SpawnedEnemyCount_; }
};
}

namespace {
using namespace stellar7;
constexpr std::size_t noObject = static_cast<std::size_t>(-1);
enum class RouteGoal { progression, combatRetry };

void require(const bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<Model3d> models(const std::filesystem::path& root, const std::string& name) {
    std::ifstream file(root / name, std::ios::binary);
    if (!file) {
        throw std::runtime_error("missing original model resource: " + name);
    }
    return decodeFirstModelTableResource({std::istreambuf_iterator<char>(file), {}});
}

std::size_t find(const GameplayModel& model, const std::int16_t first, const std::int16_t last) {
    const auto& actors = model.level2Entities();
    const auto found = std::find_if(actors.begin(), actors.end(), [first, last](const auto& actor) {
        return actor.active() && actor.type >= first && actor.type <= last;
    });
    return found == actors.end() ? noObject : static_cast<std::size_t>(found - actors.begin());
}

GameplayInput a50eWindowControls(const GameplayModel& model, const Level1Entity& guardian) {
    constexpr std::uint32_t shotSpeed = 500;
    constexpr std::uint32_t guardianSpeed = 300;
    constexpr std::uint32_t shotRadius = 134;
    constexpr std::uint32_t emissionDefenseRange = 6500;
    const auto& player = model.level2Player();
    const Level1Entity* target = &guardian;
    auto distance = approximatePlanarDistance(player.position, guardian.position);
    for (const auto& actor : model.level2Entities()) {
        if (actor.type != 47 || !actor.active() || actor.position.z > 500) {
            continue;
        }
        const auto candidate = approximatePlanarDistance(player.position, actor.position);
        if (candidate < emissionDefenseRange && candidate < distance) {
            target = &actor;
            distance = candidate;
        }
    }
    auto next = *target;
    advanceEntityKinematics(next);
    const auto state = guardian.stateBytes4dTo52[5];
    const auto closing = target == &guardian && state <= 2 ? guardianSpeed : 0U;
    const auto flight = static_cast<std::int32_t>((distance + shotSpeed + closing - 1) / (shotSpeed + closing));
    auto aim = target->position;
    aim.x += (next.position.x - target->position.x) * flight;
    aim.y += (next.position.y - target->position.y) * flight;
    const auto error = static_cast<std::int16_t>(presenterTargetHeading(player.position, aim) - player.yaw);
    const auto braking = static_cast<int>(error) - 4 * player.yawVelocity;
    GameplayInput input{};
    input.left = braking > 120;
    input.right = braking < -120;
    input.reverse = target != &guardian || state <= 2
        || approximatePlanarDistance(player.position, next.position) < distance;
    input.forward = !input.reverse && distance > 10000;

    //3A1C halves the sum of radii: only late descent/hold/early climb is hittable.
    auto impactHeight = target->position.z;
    if (target != &guardian) {
        impactHeight = std::max(0, impactHeight - 50 * flight);
    } else if (state == 1) {
        const auto descent = std::max(0, (impactHeight - 500 + 99) / 100);
        const auto hold = descent == 0 ? guardian.behaviorTimer : 20;
        impactHeight = std::max(500, impactHeight - 100 * flight);
        if (flight > descent + hold) {
            impactHeight += 100 * (flight - descent - hold);
        }
    } else if (state == 2) {
        impactHeight += 100 * std::max(0, flight - guardian.behaviorTimer);
    }
    const auto hitHeight = static_cast<std::int32_t>((target->collisionRadius + shotRadius) >> 1U);
    input.fire = std::abs(static_cast<int>(error)) < 1500 && impactHeight <= hitHeight;
    return input;
}

GameplayInput controls(const GameplayModel& model, const RouteGoal goal) {
    const auto reward = find(model, 25, 25);
    if (reward != noObject) {
        if (goal == RouteGoal::progression) {
            // Surviving minions can intercept the player on the way to the reward.
            const auto minion = find(model, 47, 47);
            if (minion != noObject) {
                return test::campaignPilotControls(model, model.level2Entities()[minion], test::CampaignPilotGoal::combat);
            }
        }
        return test::campaignPilotControls(model, model.level2Entities()[reward], test::CampaignPilotGoal::collect);
    }
    const auto guardian = find(model, 44, 44);
    if (guardian != noObject) {
        auto input = a50eWindowControls(model, model.level2Entities()[guardian]);
        if (goal == RouteGoal::combatRetry) {
            // Keep the guardian alive until it emits a minion.
            input.fire = false;
        }
        return input;
    }
    const Level1Entity* nearest = nullptr;
    auto distance = std::uint32_t{0xffffffffU};
    for (const auto& actor : model.level2Entities()) {
        if (!actor.active() || actor.stateBytes4dTo52[0] != 0xff || actor.type < 1 || actor.type > 16 || actor.type == 7) {
            continue;
        }
        const auto candidate = approximatePlanarDistance(model.level2Player().position, actor.position);
        if (candidate < distance) {
            distance = candidate;
            nearest = &actor;
        }
    }
    return nearest ? test::campaignPilotControls(model, *nearest, test::CampaignPilotGoal::combat) : GameplayInput{};
}

void diagnostic(const GameplayModel& model, const unsigned frame) {
    std::cerr << "controlled Level3 frame=" << frame << " phase=" << static_cast<int>(model.phase())
        << " level=" << model.campaignLevel() << " tick=" << model.level2LoopTick()
        << " active=" << GameplayModelTestAccess::active(model)
        << " spawned=" << GameplayModelTestAccess::spawned(model)
        << " armor=" << model.playerArmor() << " score=" << model.score()
        << " rng=" << model.level2Rng().calls() << '\n';
    for (const auto& actor : model.level2Entities()) {
        std::cerr << " actor=" << actor.type << " hp=" << unsigned(actor.stateBytes4dTo52[1])
            << " state=" << unsigned(actor.stateBytes4dTo52[5]) << " timer=" << unsigned(actor.behaviorTimer)
            << " position=" << actor.position.x << ',' << actor.position.y << ',' << actor.position.z
            << " radius=" << actor.collisionRadius << " profile=" << static_cast<int>(actor.renderProfile) << '\n';
    }
}

void naturalRetry(GameplayModel& model, const std::vector<Model3d>& shared,
    const std::vector<Model3d>& local, const unsigned frame) {
    require(model.phase() == GameplayPhase::failurePreExplosionBlackout, "legitimate lethal frame lost failure boundary");
    model.finishPlayableCampaignFrame();
    unsigned failureCalls = 0;
    while (model.phase() != GameplayPhase::continuePrompt && failureCalls < GameplayModel::failureExplosionFrames + 3) {
        model.advancePlayableFailure();
        ++failureCalls;
    }
    require(model.phase() == GameplayPhase::continuePrompt, "legitimate death did not reach Continue");
    require(model.pollPlayableContinue({}) == ContinueDecision::pending, "Continue neutral input returned a decision");
    GameplayInput up{};
    up.forward = true;
    require(model.pollPlayableContinue(up) == ContinueDecision::pending && model.continueChoice() == ContinueChoice::yes,
        "Continue did not select affirmative retry");
    GameplayInput activate{};
    activate.confirm = true;
    require(model.pollPlayableContinue(activate) == ContinueDecision::pending, "Continue activated before release");
    require(model.pollPlayableContinue({}) == ContinueDecision::yes, "Continue activation release did not commit Yes");
    const auto continues = model.remainingContinues();
    auto expectedRng = model.failureRng();
    const auto expected = constructCampaignLoaderActors(campaignLevelDefinition(2, 0).table, expectedRng, shared, local);
    model.completePlayableCampaignRetry(shared, local);
    require(model.campaignLevel() == 2 && model.phase() == GameplayPhase::playableLoaderReturn
        && model.playerArmor() == 680 && model.score() == 0 && model.remainingContinues() == continues - 1,
        "legitimate death retry lost source reset state");
    require(model.level2Entities().size() == expected.size() && model.level2Rng().calls() == expectedRng.calls(),
        "legitimate death retry lost actual constructor RNG continuity");
    auto actualRng = model.level2Rng();
    for (std::size_t index = 0; index < StellarRng::wordCount; ++index) {
        require(actualRng.next() == expectedRng.next(), "legitimate retry RNG words differ");
    }
    model.completePlayableCampaignPreloop();
    for (unsigned adjacent = 0; adjacent < 3; ++adjacent) {
        GameplayInput input{};
        input.forward = input.left = input.fire = true;
        model.advancePlayableCampaign(input);
        require(model.phase() == GameplayPhase::playable && model.campaignLevel() == 2,
            "legitimate retry did not resume the same bank");
        model.finishLevel3Radar();
        model.finishPlayableCampaignFrame();
    }
    std::cout << "Controlled Level3 constructors -> actual wave/craft/A50E/emissions/player damage -> legitimate death -> Continue/retry+3 in "
        << frame + 1 << " combat frames; guardian defeat/green/Level4 remain unqualified\n";
}

void route(const std::filesystem::path& root, const RouteGoal goal) {
    const auto shared = models(root, "COCKPIT.RES");
    const auto level3 = models(root, "LEVEL3.RES");
    const auto level4 = models(root, "LEVEL4.RES");
    require(shared.size() == 44, "actual shared registry must contain44 models");
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::constructors(*model, shared, level3);
    require(GameplayModelTestAccess::active(*model) == 4 && GameplayModelTestAccess::spawned(*model) == 4,
        "controlled Level3 entry lost its actual constructor wave count");
    model->completeLevel3Preloop();
    bool arrival = false, deployed = false, emission = false, reward = false, defeat = false;
    constexpr unsigned maximumFrames = 20000;
    for (unsigned frame = 0; frame < maximumFrames; ++frame) {
        try {
            // Once the emission is observed, release the controls and let live combat cause the death.
            const auto input = goal == RouteGoal::combatRetry && emission
                ? GameplayInput{} : controls(*model, goal);
            model->advancePlayableCampaign(input);
            arrival = arrival || find(*model, 7, 7) != noObject;
            deployed = deployed || find(*model, 44, 44) != noObject;
            emission = emission || find(*model, 47, 47) != noObject;
            reward = reward || find(*model, 25, 25) != noObject;
            defeat = defeat || std::any_of(model->level2AudioEvents().begin(), model->level2AudioEvents().end(), [](const auto& event) {
                return event.eventId == 12;
            });
            if (model->playerArmor() < 0 && goal == RouteGoal::combatRetry) {
                require(arrival && deployed && emission && GameplayModelTestAccess::spawned(*model) >= 14,
                    "legitimate retry gate requires actual wave/craft/A50E/emission ancestry");
                naturalRetry(*model, shared, level3, frame);
                return;
            }
            require(model->playerArmor() >= 0, "controlled Level3 bot exhausted armor");
            if (model->phase() == GameplayPhase::level2SuccessPresentation) {
                require(goal == RouteGoal::progression, "combat/retry gate requires legitimate death and retry");
                require(arrival && deployed && reward && defeat, "green contact skipped a retained progression boundary");
                require(model->score() >= 1000, "A50E defeat did not award guardian score");
                const auto rngBeforeSuccess = model->level2Rng().calls();
                //The audio owner reports its FC return; this model test does not synthesize audio.
                model->setSuccessAudioActive(true);
                model->setSuccessAudioActive(false);
                model->resumePlayableCampaignSuccess();
                require(model->pendingPlayableCampaignDestination() == 3, "green Level3 reward did not select Level4");
                require(model->level2Rng().calls() == rngBeforeSuccess, "success return repeated gameplay RNG");
                auto expectedRng = model->level2Rng();
                const auto expectedActors = constructCampaignLoaderActors(campaignLevelDefinition(3, 0).table,
                    expectedRng, shared, level4);
                model->completePlayableCampaignLoad(3, shared, level4);
                require(model->campaignLevel() == 3 && model->phase() == GameplayPhase::playableLoaderReturn,
                    "actual generic loader did not retain Level4 boundary");
                require(model->level2Entities().size() == expectedActors.size()
                    && model->level2Rng().calls() == expectedRng.calls(), "Level4 loader reset constructor continuity");
                auto actualRng = model->level2Rng();
                for (std::size_t index = 0; index < StellarRng::wordCount; ++index) {
                    require(actualRng.next() == expectedRng.next(), "Level4 constructor RNG words differ");
                }
                model->completePlayableCampaignPreloop();
                for (unsigned adjacent = 0; adjacent < 3; ++adjacent) {
                    GameplayInput input{};
                    input.forward = input.left = input.fire = true;
                    model->advancePlayableCampaign(input);
                    require(model->phase() == GameplayPhase::playable && model->campaignLevel() == 3,
                        "Level4 adjacent frame left ordinary play");
                    model->finishLevel3Radar();
                    model->finishPlayableCampaignFrame();
                }
                std::cout << "Controlled Level3 constructor ancestry -> ordinary wave/craft/A50E/green -> actual Level4+3 in "
                    << frame + 1 << " frames; A50E emission=" << emission << " armor=" << model->playerArmor() << '\n';
                return;
            }
            require(model->phase() == GameplayPhase::level3Native, "controlled Level3 left ordinary play before green contact");
            model->finishLevel3Radar();
            model->finishPlayableCampaignFrame();
            if (frame % 500 == 0) {
                std::cerr << "controlled Level3 frame=" << frame << " active=" << GameplayModelTestAccess::active(*model)
                    << " spawned=" << GameplayModelTestAccess::spawned(*model) << " armor=" << model->playerArmor() << '\n';
            }
        } catch (...) {
            diagnostic(*model, frame);
            throw;
        }
    }
    diagnostic(*model, maximumFrames);
    throw std::runtime_error("controlled Level3 progression did not reach actual Level4 frames");
}
}

int main(int argc, char** argv) {
    if ((argc != 2 && argc != 3) || (argc == 3 && std::string(argv[2]) != "--combat-retry")) {
        std::cerr << "expected original resource directory [--combat-retry]\n";
        return 2;
    }
    try {
        route(argv[1], argc == 3 ? RouteGoal::combatRetry : RouteGoal::progression);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

