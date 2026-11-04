#include "stellar7/gameplay.hpp"
#include "stellar7/audio_direct_work.hpp"
#include "playable_campaign_comparison.hpp"

#include <algorithm>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void lethalSystems(Level1State& state) {
        state.systemTimers_ = {11, 22, 33, 73, 44, 55, 66};
    }
};

struct GameplayModelTestAccess {
    static void constructors(GameplayModel& model, const std::uint8_t level,
        const std::vector<Model3d>& shared, const std::vector<Model3d>& local) {
        StellarRng rng;
        for (unsigned call = 0; call < 29U + level; ++call) {
            rng.next();
        }
        auto actors = constructCampaignLoaderActors(campaignLevelDefinition(level, 0).table, rng, shared, local);
        model.playableCampaignFrames_ = true;
        model.initializeRetainedCampaignLevel(level, std::move(actors), rng, shared, local);
        if (level == 2) {
            model.completeLevel3Preloop();
        } else {
            model.completePlayableCampaignPreloop();
        }
    }

    static void lethalFrame(GameplayModel& model) {
        //One controlled lethal component boundary; subsequent destruction/UI/retry is real.
        model.level2PlayerArmor_ = -1;
        model.level2Score_ = 321;
        model.failureFromLevel2_ = true;
        model.level2FailureTailFinished_ = true;
        model.failureSourceLoopTick_ = model.level2LoopTick_;
        model.phase_ = GameplayPhase::failurePreExplosionBlackout;
        model.level6GuardianTurn_ = -217;
        model.campaignPlayerProxy_.position = {11, 22, 33};
        Level1ReloadTestAccess::lethalSystems(model.level1_);
    }

    static const Level1Entity& proxy(const GameplayModel& model) { return model.campaignPlayerProxy_; }

    static bool persistentCounters(const GameplayModel& model) {
        return model.level6GuardianTurn_ == -217 && model.level1_.systemTimers()[3] == 73;
    }

    static bool persistent(const GameplayModel& model) {
        return model.level6GuardianTurn_ == -217 && model.campaignPlayerProxy_.position == FixedPosition{11, 22, 33}
            && model.level1_.systemTimers()[3] == 73;
    }
};
}

namespace {
using namespace stellar7;

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

void sameRng(StellarRng actual, StellarRng expected) {
    require(actual.calls() == expected.calls() && actual.indexA() == expected.indexA()
        && actual.indexB() == expected.indexB(), "retry reset its retained RNG cursor");
    for (std::size_t index = 0; index < StellarRng::wordCount; ++index) {
        require(actual.next() == expected.next(), "retry changed retained RNG words");
    }
}

template<class Callback>
void rejects(Callback callback, const char* message) {
    bool rejected = false;
    try {
        callback();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, message);
}

void retry(const std::vector<Model3d>& shared, const std::vector<Model3d>& local, const std::uint8_t level) {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::constructors(*model, level, shared, local);
    rejects([&]() { model->advancePlayableFailure(); }, "destruction accepted an ordinary constructor boundary");
    rejects([&]() { model->pollPlayableContinue({}); }, "Continue accepted an ordinary constructor boundary");
    rejects([&]() { model->completePlayableCampaignRetry(shared, local); }, "retry accepted an ordinary constructor boundary");
    GameplayModelTestAccess::lethalFrame(*model);
    const auto continues = model->remainingContinues();
    const auto beforeFailure = model->level2Rng().calls();
    unsigned calls = 0;
    while (model->phase() != GameplayPhase::continuePrompt && calls < GameplayModel::failureExplosionFrames + 3) {
        model->advancePlayableFailure();
        ++calls;
    }
    require(calls == GameplayModel::failureExplosionFrames + 2 && model->phase() == GameplayPhase::continuePrompt,
        "destruction did not finish its45-frame presenter and Continue entry");
    require(model->failureRng().calls() > beforeFailure, "destruction did not continue campaign RNG");
    require(GameplayModelTestAccess::persistent(*model), "destruction reset retained timer/turn/proxy");
    const auto failureRng = model->failureRng();
    rejects([&]() { model->completePlayableCampaignRetry(shared, local); }, "retry accepted pending Continue");
    require(model->pollPlayableContinue({}) == ContinueDecision::pending, "Continue neutral poll committed a choice");
    GameplayInput up{};
    up.forward = true;
    require(model->pollPlayableContinue(up) == ContinueDecision::pending && model->continueChoice() == ContinueChoice::yes,
        "released navigation did not select Yes");
    GameplayInput press{};
    press.confirm = true;
    require(model->pollPlayableContinue(press) == ContinueDecision::pending, "Continue committed before activation release");
    require(model->pollPlayableContinue(press) == ContinueDecision::pending, "held Continue activation committed a choice");
    require(model->pollPlayableContinue({}) == ContinueDecision::yes, "released Yes activation did not return affirmative");
    auto reference = failureRng;
    const auto expected = constructCampaignLoaderActors(campaignLevelDefinition(level, 0).table, reference, shared, local);
    model->completePlayableCampaignRetry(shared, local);
    require(model->phase() == GameplayPhase::playableLoaderReturn && model->campaignLevel() == level,
        "retry loaded a different bank or legacy phase");
    require(model->remainingContinues() == continues - 1 && model->score() == 0 && model->playerArmor() == 680,
        "retry did not restore source armor/score/continues");
    require(model->level2Entities().size() == expected.size(), "retry lost actual table actors");
    sameRng(model->level2Rng(), reference);
    require(GameplayModelTestAccess::persistent(*model), "retry reset retained timer3/DS41B4/proxy");
    for (std::size_t index = 0; index < Level1State::systemCount; ++index) {
        require(model->level1State().systemTimers()[index] == (index == 3 ? 73 : 0), "retry transient timer reset differs from48D4");
    }
    model->completePlayableCampaignPreloop();
    require(GameplayModelTestAccess::persistent(*model), "retry preloop reset retained timer3/DS41B4/proxy");
    auto expectedProxy = GameplayModelTestAccess::proxy(*model);
    const auto retainedProxyPosition = expectedProxy.position;
    for (unsigned adjacent = 0; adjacent < 3; ++adjacent) {
        const auto audioCursor = model->level2AudioEvents().size();
        GameplayInput input{};
        input.forward = input.left = input.fire = true;
        model->advancePlayableCampaign(input);
        require(model->phase() == GameplayPhase::playable && model->campaignLevel() == level,
            "same-bank retry adjacent frame left playable phase");
        model->finishLevel3Radar();
        model->finishPlayableCampaignFrame();
        const auto& events = model->level2AudioEvents();
        const auto fires = std::count_if(events.begin() + audioCursor, events.end(),
            [](const auto& event) { return event.eventId == gameplayPrimaryFireEvent; });
        if (adjacent == 0) {
            require(fires == 1, "retry first MAIN did not allocate its source05D4 primary");
            const auto& actors = model->level2Entities();
            const auto shot = std::find_if(actors.begin(), actors.end(), [](const auto& actor) {
                return actor.type == 23 && actor.stateBytes4dTo52[0] == 0xfd && actor.ownerIsPlayer;
            });
            require(shot != actors.end(), "retry primary event lost its owned newborn FD23");
            //06F9..070E copies only XYZ to4858, even with inactive I.
            expectedProxy.position = shot->position;
            require(!(expectedProxy.position == retainedProxyPosition), "retry fire did not overwrite the seeded proxy XYZ");
        } else {
            require(fires == 0, "retry held cooldown emitted another05D4 primary");
        }
        require(test::campaignActorFields(GameplayModelTestAccess::proxy(*model))
            == test::campaignActorFields(expectedProxy), "retry fire changed non-XYZ proxy fields or missed source XYZ refresh");
        require(GameplayModelTestAccess::persistentCounters(*model), "retry adjacent frames reset timer3/DS41B4");
    }
    require(model->playerArmor() >= 0, "retry adjacent frames lost source armor");
    std::cout << "Controlled Level" << unsigned(level + 1) << " lethal ->45 destruction -> released Yes -> actual same-bank retry+3\n";
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "expected original resource directory\n";
        return 2;
    }
    try {
        const std::filesystem::path root(argv[1]);
        const auto shared = models(root, "COCKPIT.RES");
        for (std::uint8_t level = 1; level < 7; ++level) {
            retry(shared, models(root, "LEVEL" + std::to_string(level + 1) + ".RES"), level);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
