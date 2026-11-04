#include "native_level2_frame_fixture.hpp"
#include "playable_campaign_comparison.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using namespace stellar7;
void require(const bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

const Level1Entity& type24(const GameplayModel& model) {
    const auto& actors = model.level2Entities();
    const auto found = std::find_if(actors.begin(), actors.end(), [](const auto& actor) {
        return actor.type == 24 && actor.active();
    });
    require(found != actors.end(), "actual bank lost its inherited Type24");
    return *found;
}

void bank(const std::filesystem::path& resource, const std::uint8_t index) {
    //Existing controlled actual-bank fixture; this proves native callbacks,
    //not ordinary-wave or natural retry ancestry.
    auto left = test::makePlayableGuardianSession(resource, index, test::PlayableGuardianEntry::clearedWave);
    auto right = test::makePlayableGuardianSession(resource, index, test::PlayableGuardianEntry::clearedWave);
    test::CampaignBudgetComparison comparison;
    constexpr std::array<std::array<std::uint8_t, 2>, 3> afterRadar{{{1, 1}, {2, 2}, {2, 0}}};
    require(type24(left->model()).modelAnimationVariant == 0
        && type24(left->model()).stateBytes4dTo52[3] == 0,
        "actual fixed constructor lost selector0/countdown0");
    const auto preloopLeft = left->advancePlayableCampaign({}, 10000);
    const auto preloopRight = right->advancePlayableCampaign({}, 257);
    comparison.compare(*left, *right, preloopLeft, preloopRight, test::CampaignComparisonStage::boundary);
    require(left->model().level2LoopTick() == 0 && type24(left->model()).modelAnimationVariant == 0
        && type24(left->model()).stateBytes4dTo52[3] == 0,
        "loader return executed a gameplay MAIN callback");
    for (const auto expected : afterRadar) {
        const auto broad = left->advancePlayableCampaign({}, 10000);
        const auto narrow = right->advancePlayableCampaign({}, 257);
        comparison.compare(*left, *right, broad, narrow, test::CampaignComparisonStage::boundary);
        const auto& actor = type24(left->model());
        require(actor.modelAnimationVariant == expected[0] && actor.stateBytes4dTo52[3] == expected[1],
            "generic playable Type24 missed its same-frame second698A/130C callback");
    }
    std::cout << "Declared native bank" << unsigned(index + 1)
        << " Type24 MAIN/radar cadence and full budget twins PASS\n";
}
} //namespace

int main(int argc, char** argv) {
    if (argc != 2) { return 2; }
    try {
        bank(argv[1], 2);
        bank(argv[1], 5);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
