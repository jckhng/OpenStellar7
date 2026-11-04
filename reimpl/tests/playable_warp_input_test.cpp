#include "native_level2_frame_fixture.hpp"
#include "playable_campaign_comparison.hpp"
#include "playable_campaign_pilot.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace stellar7;

constexpr std::size_t broadBudget = 10000;
constexpr std::size_t narrowBudget = 257;
constexpr unsigned maximumCalls = 2000;
constexpr unsigned adjacentLevel3Frames = 2;

enum class WarpInputPattern { neutral, heldFire, heldFireAndMovement };

const char* patternName(const WarpInputPattern pattern) {
    switch (pattern) {
    case WarpInputPattern::neutral: return "neutral";
    case WarpInputPattern::heldFire: return "held fire";
    case WarpInputPattern::heldFireAndMovement: return "held fire and movement";
    }
    throw std::logic_error("unsupported warp input pattern");
}

void require(const bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

const Level1Entity* find(const GameplayModel& model, const std::int16_t type) {
    const auto& actors = model.level2Entities();
    const auto found = std::find_if(actors.begin(), actors.end(), [type](const auto& actor) {
        return actor.active() && actor.type == type;
    });
    return found == actors.end() ? nullptr : &*found;
}

GameplayInput ordinaryInput(const GameplayModel& model) {
    if (const auto* pickup = find(model, 25)) {
        auto input = test::campaignPilotControls(model, *pickup, test::CampaignPilotGoal::collect);
        input.fire = true;
        return input;
    }
    return {};
}

bool neutralWarpPhase(const GameplayPhase phase) {
    return phase == GameplayPhase::level2SuccessPresentation
        || phase == GameplayPhase::level2SuccessReturn
        || phase == GameplayPhase::level3LoaderReturn;
}

void rejectUnsupportedInputs(NativeGameplaySession& session, const GameplayPhase phase) {
    const auto beforeSamples = session.elapsedSamples();
    const auto beforeWrites = session.synth().registerWrites().size();
    const auto reject = [&](const GameplayInput& input, const std::size_t budget,
        const NameEntryInput& text, const std::string& expected) {
        bool rejected = false;
        try {
            session.advancePlayableCampaign(input, budget, text);
        } catch (const std::invalid_argument& error) {
            rejected = error.what() == expected;
        }
        require(rejected && session.model().phase() == phase
            && session.elapsedSamples() == beforeSamples
            && session.synth().registerWrites().size() == beforeWrites,
            "unsupported warp input changed state or diagnostic");
    };
    const auto prefix = "playable campaign input has no active consumer: phase="
        + std::to_string(static_cast<int>(phase));
    GameplayInput input{};
    input.confirm = true;
    reject(input, broadBudget, {}, prefix + " confirm=1 exit=0 text=0");
    input = {};
    input.exit = true;
    reject(input, broadBudget, {}, prefix + " confirm=0 exit=1 text=0");
    reject({}, broadBudget, NameEntryInput{{'A'}}, prefix + " confirm=0 exit=0 text=1");
    reject({}, 0, {}, "playable campaign work budget is outside its supported range");
    reject({}, broadBudget + 1, {}, "playable campaign work budget is outside its supported range");
}

void route(const std::filesystem::path& resources, const WarpInputPattern pattern) {
    auto held = test::makePlayableGuardianSession(resources, 1, test::PlayableGuardianEntry::lateFight, NativeMenuFeatures::joystickControls);
    auto partitioned = test::makePlayableGuardianSession(resources, 1, test::PlayableGuardianEntry::lateFight, NativeMenuFeatures::joystickControls);
    auto neutral = test::makePlayableGuardianSession(resources, 1, test::PlayableGuardianEntry::lateFight, NativeMenuFeatures::joystickControls);
    test::CampaignBudgetComparison budgetComparison;
    test::CampaignBudgetComparison neutralComparison;
    unsigned defeats = 0;
    unsigned level3Frames = 0;
    bool pickup = false;
    bool success = false;
    bool returnSeen = false;
    bool loader = false;
    bool released = false;
    bool movementConsumed = false;
    std::array<bool, 3> rejectedPhases{};

    require(find(held->model(), 44) && find(held->model(), 23),
        "controlled HP1/one FD guardian entry is missing");
    for (unsigned call = 0; call < maximumCalls; ++call) {
        const auto phase = held->model().phase();
        const auto level = held->model().campaignLevel();
        const bool nonconsumer = neutralWarpPhase(phase);
        const auto phaseIndex = phase == GameplayPhase::level2SuccessPresentation ? 0
            : phase == GameplayPhase::level2SuccessReturn ? 1 : 2;
        if (nonconsumer && !rejectedPhases[phaseIndex]) {
            rejectUnsupportedInputs(*held, phase);
            rejectedPhases[phaseIndex] = true;
        }
        auto input = level == 1 && phase == GameplayPhase::playable ? ordinaryInput(held->model()) : GameplayInput{};
        const auto neutralInput = input;
        if (nonconsumer && pattern != WarpInputPattern::neutral) {
            input.fire = true;
            if (pattern == WarpInputPattern::heldFireAndMovement) {
                input.forward = true;
                input.right = true;
            }
        }
        if (phase == GameplayPhase::level3Native) {
            released = true;
        }
        NativeSessionDelivery a, b, c;
        try {
            a = held->advancePlayableCampaign(input, broadBudget);
            b = partitioned->advancePlayableCampaign(input, narrowBudget);
            c = neutral->advancePlayableCampaign(neutralInput, broadBudget);
        } catch (const std::exception& error) {
            std::cerr << "warp call=" << call << " phase=" << static_cast<int>(phase)
                << " level=" << unsigned(level) << " heldFire=" << input.fire
                << " pickup=" << pickup << " success=" << success
                << " error=" << error.what() << '\n';
            throw;
        }
        const bool boundary = phase != held->model().phase() || level != held->model().campaignLevel();
        const auto stage = boundary ? test::CampaignComparisonStage::boundary
            : test::CampaignComparisonStage::frame;
        const bool resumedWithMovement = pattern == WarpInputPattern::heldFireAndMovement
            && phase == GameplayPhase::level2SuccessPresentation
            && held->model().phase() == GameplayPhase::level2SuccessReturn;
        try {
            budgetComparison.compare(*held, *partitioned, a, b, stage);
            if (!movementConsumed && !resumedWithMovement) {
                neutralComparison.compare(*held, *neutral, a, c, stage);
            }
        } catch (const std::exception& error) {
            std::cerr << "comparison pattern=" << patternName(pattern) << " call=" << call
                << " beforePhase=" << static_cast<int>(phase)
                << " afterPhase=" << static_cast<int>(held->model().phase())
                << " error=" << error.what() << '\n';
            const auto& player = held->model().level2Player();
            const auto& control = neutral->model().level2Player();
            std::cerr << "held x/y/yaw/forward=" << player.position.x << '/'
                << player.position.y << '/' << player.yaw << '/' << player.forwardVelocity
                << " neutral=" << control.position.x << '/' << control.position.y
                << '/' << control.yaw << '/' << control.forwardVelocity << '\n';
            throw;
        }
        if (resumedWithMovement) {
            require(!movementConsumed && held->model().phase() == neutral->model().phase()
                && held->model().level2Player().forwardVelocity == 200
                && neutral->model().level2Player().forwardVelocity == 190,
                "held movement was not consumed at the success return boundary");
            movementConsumed = true;
        }
        for (const auto& event : a.events) {
            defeats += event.eventId == 12;
        }
        pickup = pickup || find(held->model(), 25);
        success = success || held->model().phase() == GameplayPhase::level2SuccessPresentation;
        returnSeen = returnSeen || held->model().phase() == GameplayPhase::level2SuccessReturn;
        loader = loader || held->model().phase() == GameplayPhase::level3LoaderReturn;
        require(held->model().playerArmor() >= 0, "controlled warp reached tactical death");
        if (released && held->model().phase() == GameplayPhase::level3Native) {
            ++level3Frames;
        }
        if (level3Frames == adjacentLevel3Frames) {
            require(defeats == 1 && pickup && success && returnSeen && loader
                && std::all_of(rejectedPhases.begin(), rejectedPhases.end(), [](const bool checked) {return checked;})
                && movementConsumed == (pattern == WarpInputPattern::heldFireAndMovement),
                "controlled guardian route skipped actual defeat/pickup/warp/loader");
            std::cout << patternName(pattern) << " warp"
                << (movementConsumed ? " consumes movement at success return" : " equals neutral control")
                << " through Level3+2; "
                << "budgets 10000/257; calls=" << call + 1 << '\n';
            return;
        }
    }
    std::cerr << "limit phase=" << static_cast<int>(held->model().phase())
        << " level=" << unsigned(held->model().campaignLevel())
        << " defeats=" << defeats << " pickup=" << pickup << " success=" << success
        << " return=" << returnSeen << " loader=" << loader
        << " level3Frames=" << level3Frames << '\n';
    throw std::runtime_error("held-fire warp did not reach two adjacent Level3 frames");
}
} // namespace

int main(const int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    try {
        for (const auto pattern : {WarpInputPattern::neutral, WarpInputPattern::heldFire,
            WarpInputPattern::heldFireAndMovement}) {
            route(argv[1], pattern);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
