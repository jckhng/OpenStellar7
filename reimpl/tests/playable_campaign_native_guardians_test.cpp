#include "native_level2_frame_fixture.hpp"
#include "playable_campaign_comparison.hpp"
#include "playable_campaign_pilot.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using namespace stellar7;
using stellar7::test::CampaignComparisonStage;
using stellar7::test::PlayableGuardianEntry;

void require(const bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

struct GuardianCase {
    std::uint8_t level;
    PlayableGuardianEntry entry;
    std::uint8_t health;
};

constexpr std::array<GuardianCase, 5> cases{{
    {0, PlayableGuardianEntry::lateFight, 1},
    {3, PlayableGuardianEntry::lateFight, 1},
    {4, PlayableGuardianEntry::clearedWave, 50},
    {5, PlayableGuardianEntry::clearedWave, 50},
    {6, PlayableGuardianEntry::lateFight, 1}
}};
constexpr std::size_t broadBudget = 10000;
constexpr std::size_t narrowBudget = 257;
constexpr unsigned maximumCalls = 2000;
constexpr int guardianScore = 1000;
constexpr std::size_t shieldSystem = 1;
constexpr std::size_t firstFixedIdentity = 16;

const Level1Entity* find(const GameplayModel& model, const std::int16_t type) {
    const auto& actors = model.level2Entities();
    const auto found = std::find_if(actors.begin(), actors.end(), [type](const auto& actor) {
        return actor.active() && actor.type == type;
    });
    return found == actors.end() ? nullptr : &*found;
}

GameplayInput controls(const GameplayModel& model, const PlayableGuardianEntry entry) {
    if (const auto* reward = find(model, 25)) {
        return test::campaignPilotControls(model, *reward, test::CampaignPilotGoal::collect);
    }
    if (entry == PlayableGuardianEntry::lateFight) { return {}; }
    for (const auto type : {44, 45}) {
        if (const auto* guardian = find(model, type)) {
            auto input=test::campaignPilotControls(model, *guardian, test::CampaignPilotGoal::combat);
            const auto& systems=model.level1State();
            if(systems.systemStatusLevels()[shieldSystem]>0&&systems.systemTimers()[shieldSystem]==0) {
                input.systemAction=GameplaySystemAction::shield;
            }
            return input;
        }
    }
    return {};
}

void endingPage(NativeGameplaySession& left, NativeGameplaySession& right,
    test::CampaignBudgetComparison& comparison) {
    const auto samples = left.elapsedSamples();
    const auto cycles = left.synth().driverState().normalClockCycles;
    const auto world = left.model().level2LoopTick();
    const auto randomCalls = left.model().level2Rng().calls();
    const auto leftWork = left.advancePlayableCampaign({}, broadBudget);
    const auto rightWork = right.advancePlayableCampaign({}, narrowBudget);
    comparison.compare(left, right, leftWork, rightWork, CampaignComparisonStage::boundary);
    const auto voice = left.synth().sampledVoiceObservation();
    require(left.model().phase() == GameplayPhase::endingPresentation
        && left.model().presentationFrame() == 1
        && left.pageObservation().projection == NativePageProjection::retainedEndingPage,
        "guardian victory did not execute actual SCENED0");
    require(leftWork.firstSample == samples && !leftWork.samples.empty()
        && left.elapsedSamples() > samples && left.synth().driverState().normalClockCycles > cycles,
        "first ending page lost its contiguous native PCM/clock");
    require(voice.phase == SampledVoicePhase::loaded && voice.transferred == 0,
        "first ending page lost its actual sampled voice");
    require(left.model().level2LoopTick() == world && left.model().level2Rng().calls() == randomCalls,
        "ending scene repeated world or random work");
    require(std::any_of(left.frame().pixels.begin(), left.frame().pixels.end(),
        [](const auto pixel) { return pixel != 0; }), "actual ending page is empty");
}

void route(const std::filesystem::path& root, const GuardianCase fixture) {
    //Late-fight entries declare legal HP1 and one owned FD after19 lifetime ticks.
    //Cleared-wave entries retain actual477A HP50. Neither claims natural wave clear.
    auto left = test::makePlayableGuardianSession(root, fixture.level, fixture.entry);
    auto right = test::makePlayableGuardianSession(root, fixture.level, fixture.entry);
    test::CampaignBudgetComparison comparison;
    const auto score = left->model().score();
    const auto ticks = left->model().ticks();
    const auto armor = left->model().playerArmor();
    const auto samples = left->elapsedSamples();
    bool craft = false, guardian = false, reward = false;
    unsigned arrivals = 0, defeats = 0;
    unsigned shieldActivations = 0;
    bool shieldedGateImpact = false;
    if (fixture.entry == PlayableGuardianEntry::lateFight) {
        const auto* boss = find(left->model(), 44);
        const auto* shot = find(left->model(), 23);
        const auto& actors = left->model().level2Entities();
        require(boss && boss->stateBytes4dTo52[1] == fixture.health
            && shot && shot->ownerIsPlayer && shot->stateBytes4dTo52[1] == 45
            && std::count_if(actors.begin(), actors.end(), [](const auto& actor) {
                return actor.active() && actor.type == 23;
            }) == 1,
            "late-fight declaration lost HP1/one retained player FD");
        guardian = true;
    }
    for (unsigned calls = 1; calls <= maximumCalls; ++calls) {
        const auto input = controls(left->model(), fixture.entry);
        const auto charges=left->model().level1State().systemStatusLevels()[shieldSystem];
        const auto previous=left->model().phase();
        const auto leftWork = left->advancePlayableCampaign(input, broadBudget);
        const auto rightWork = right->advancePlayableCampaign(input, narrowBudget);
        const auto& model = left->model();
        const bool nextBank = model.campaignLevel() == fixture.level + 1
            && model.phase() == GameplayPhase::playable && model.level2LoopTick() >= 3;
        comparison.compare(*left, *right, leftWork, rightWork,
            nextBank ? CampaignComparisonStage::boundary : CampaignComparisonStage::frame);
        if(input.systemAction==GameplaySystemAction::shield) {
            const auto& systems=model.level1State();
            require(charges>0&&systems.systemStatusLevels()[shieldSystem]==-(charges-1)
                &&systems.systemTimers()[shieldSystem]>0,"pilot failed to consume its retained shield charge");
            ++shieldActivations;
        }
        if(fixture.level==4&&previous!=GameplayPhase::level2SuccessPresentation
            &&model.phase()==GameplayPhase::level2SuccessPresentation) {
            const auto& presented=model.level2PresentationEntities();
            const auto& order=model.level2PresentationActiveListOrder();
            require(model.level1State().systemTimers()[shieldSystem]>0
                &&std::any_of(model.level2Entities().begin(),model.level2Entities().end(),[](const auto& actor) {
                    return actor.active()&&actor.type==-1;
                }),"gate regression missed the shield impact allocation");
            require(order.size()==1&&presented.at(order.front()).type==25
                &&model.level2PresentationPoolSlot(order.front())==firstFixedIdentity
                &&model.level2ActiveListOrder()==order,"gate radar admitted the newborn effect or lost inherited identity");
            shieldedGateImpact=true;
        }
        for (const auto& event : leftWork.events) {
            arrivals += event.eventId == 17;
            defeats += event.eventId == 12;
        }
        for (const auto& actor : model.level2Entities()) {
            craft = craft || actor.type == 7;
            reward = reward || actor.type == 25;
            if (!guardian && actor.active() && actor.type == 44) {
                require(actor.stateBytes4dTo52[1] == fixture.health,
                    "actual477A constructor lost full guardian health");
                guardian = true;
            }
        }
        require(model.playerArmor() >= 0, "native guardian regression reached tactical death");
        if (fixture.level == 6) {
            require(model.campaignLevel() == 7 && model.phase() == GameplayPhase::endingEntry
                && left->pageObservation().projection == NativePageProjection::retainedEndingEntry
                && model.playerArmor() == armor && model.ticks() == ticks + 1
                && model.score() == score + guardianScore && guardian && defeats == 1 && !reward,
                "final guardian lost its actual1176/7512 native ending caller");
            endingPage(*left, *right, comparison);
            std::cout << "Controlled native Level7 HP1/one FD -> actual7512/SCENED0/voice; budget twins PASS\n";
            return;
        }
        if (!nextBank) { continue; }
        require(guardian && reward && defeats == 1 && model.score() == score + guardianScore,
            "native guardian route skipped actual defeat/reward/contact/success");
        if (fixture.entry == PlayableGuardianEntry::clearedWave) {
            require(craft && arrivals == 1 && shieldActivations>0, "cleared-wave route bypassed arrival or retained shield use");
            if(fixture.level==4) {require(shieldedGateImpact,"Level5 route bypassed the shielded gate boundary");}
        } else {
            require(arrivals == 0, "pending late-fight reward repeated guardian arrival");
        }
        std::cout << "Controlled native Level" << unsigned(fixture.level + 1)
            << (fixture.entry == PlayableGuardianEntry::lateFight ? " HP1/one FD" : " fullHP50 after wave clear")
            << " -> actual reward/contact/success/nextbank+3; budget twins PASS calls=" << calls
            << " samples=" << left->elapsedSamples() - samples << " shields=" << shieldActivations << '\n';
        return;
    }
    throw std::runtime_error("native guardian regression did not reach its parent continuation");
}
} //namespace

int main(int argc, char** argv) {
    if (argc != 2) { return 2; }
    try {
        for (const auto fixture : cases) { route(argv[1], fixture); }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
