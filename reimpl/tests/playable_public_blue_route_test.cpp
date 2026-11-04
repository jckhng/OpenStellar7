#include "playable_startup_resources.hpp"
#include "playable_blue_route_controls.hpp"
#include "playable_campaign_comparison.hpp"

#include <iostream>
#include <optional>

//Read-only observations; this test has no controlled-entry factory or setter.
namespace stellar7 {
enum class F3Retirement { inactiveF3, recycled26 };
struct NativeSessionTestAccess {
    static bool blue(const NativeGameplaySession& session) {
        return session.successPalette_ && session.successPalette_->channel == SuccessPaletteChannel::blue;
    }
};
struct GameplayModelTestAccess {
    static std::size_t fixedOwner(const GameplayModel& model, const Level1Entity& actor) {
        const auto index = static_cast<std::size_t>(&actor - model.level2Entities_.data());
        const auto slot = model.level2EntityFixedPoolSlots_.at(index);
        test::requireCampaignComparison(slot < model.level2FixedPool_.size() && model.level2FixedPoolKnown_[slot],
            "natural blue target lost its known physical FE owner");
        test::requireCampaignComparison(test::campaignActorFields(actor)
            == test::campaignActorFields(model.level2FixedPool_.at(slot)), "natural blue FE alias diverged");
        return slot;
    }
    static F3Retirement retirement(const GameplayModel& model, const std::size_t slot, const std::int16_t type) {
        test::requireCampaignComparison(model.level2FixedPoolKnown_.at(slot), "retired F3 physical owner became unknown");
        const auto& record = model.level2FixedPool_.at(slot);
        if (record.type == type && !record.active() && record.stateBytes4dTo52[1] == 0) {
            return F3Retirement::inactiveF3;
        }
        if (record.type == 26 && record.active() && record.stateBytes4dTo52[0] == 0xfe) {
            return F3Retirement::recycled26;
        }
        throw std::runtime_error("F3 disappeared without physical retirement or actual26 reuse");
    }
};
}

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
constexpr std::size_t sharedModelCount = 44;
constexpr unsigned maximumCalls = 12000;
constexpr std::uint8_t initialF3Health = 16;

const Level1Entity* find(const GameplayModel& model, const std::int16_t type,
    const std::optional<std::uint8_t> behavior = {}) {
    const Level1Entity* found = nullptr;
    for (const auto& actor : model.level2Entities()) {
        if (!actor.active() || actor.type != type || actor.stateBytes4dTo52[0] != 0xfe
            || (behavior && actor.stateBytes4dTo52[4] != *behavior)) { continue; }
        requireCampaignComparison(!found, "natural blue target is ambiguous");
        found = &actor;
    }
    return found;
}

struct WarpProof {
    std::size_t bank;
    std::optional<std::size_t> producerSlot, rewardSlot;
    std::uint8_t health = initialF3Health;
    std::vector<std::uint8_t> healthHistory{initialF3Health};
    std::optional<F3Retirement> retirement;
    std::uint8_t lastLiveHealth = initialF3Health;
    unsigned earnedDamage = 0, destinationFrames = 0;
    bool born = false, contact = false, returned = false, loaded = false, preloop = false;

    void observeInitial(const GameplayModel& model) {
        if (producerSlot) { return; }
        const auto* actor = find(model, static_cast<std::int16_t>(29 + bank), 0xf3);
        const auto row = campaignLevelDefinition(static_cast<std::uint8_t>(bank), 0).table.actors.at(bank == 0 ? 5 : 6);
        requireCampaignComparison(actor && actor->stateBytes4dTo52[1] == initialF3Health
            && actor->position == FixedPosition{campaignCoordinate(row.x), campaignCoordinate(row.y), 0},
            "natural blue route did not start from untouched16HP F3");
        producerSlot = GameplayModelTestAccess::fixedOwner(model, *actor);
    }

    void observeAfter(const NativeGameplaySession& session, const std::size_t playerProjectilesBefore) {
        const auto& model = session.model();
        const auto* actor = find(model, static_cast<std::int16_t>(29 + bank), 0xf3);
        if (!actor && !retirement) {
            lastLiveHealth = health;
            retirement = GameplayModelTestAccess::retirement(model, *producerSlot, static_cast<std::int16_t>(29 + bank));
        }
        //Zero here is damage bookkeeping only. A recycled slot does not expose
        //an observed F3 HP0; retain the last live health and actual record kind.
        const auto remaining = actor ? actor->stateBytes4dTo52[1] : std::uint8_t{0};
        requireCampaignComparison(remaining <= health, "natural F3 health increased");
        const auto damage = static_cast<unsigned>(health - remaining);
        requireCampaignComparison(damage <= playerProjectilesBefore,
            "F3 health dropped without enough inherited player FD records");
        earnedDamage += damage;
        health = remaining;
        if (actor && damage != 0) { healthHistory.push_back(remaining); }
        if (actor) {
            requireCampaignComparison(producerSlot == GameplayModelTestAccess::fixedOwner(model, *actor),
                "natural F3 changed its physical owner");
        }
        if (const auto* blue = find(model, 26)) {
            requireCampaignComparison(!actor && earnedDamage == initialF3Health,
                "actual26 bypassed full natural F3 damage/retirement");
            const auto slot = GameplayModelTestAccess::fixedOwner(model, *blue);
            requireCampaignComparison(!rewardSlot || *rewardSlot == slot, "actual26 changed its physical owner");
            rewardSlot = slot;
            born = true;
        }
        if (model.phase() == GameplayPhase::level2SuccessPresentation) {
            requireCampaignComparison(born && NativeSessionTestAccess::blue(session),
                "natural26 contact did not own native blue success");
            contact = true;
        }
    }

    void complete() const {
        requireCampaignComparison(producerSlot && rewardSlot && retirement && earnedDamage == initialF3Health
            && born && contact && returned && loaded && preloop && destinationFrames >= 3,
            "natural blue route missed a producer/contact/+3 loader milestone");
        std::cout << "bank=" << bank + 1 << " producerFE=" << *producerSlot << " rewardFE=" << *rewardSlot << " HP=";
        for (const auto value : healthHistory) { std::cout << unsigned(value) << ','; }
        std::cout << " lastLive=" << unsigned(lastLiveHealth) << " physicalRetirement="
            << (*retirement == F3Retirement::inactiveF3 ? "inactiveF3/HP0" : "recycled26")
            << " bluecontact/+3 loader/preloop/frames=" << destinationFrames << '\n';
    }
};

std::size_t playerProjectiles(const GameplayModel& model) {
    std::size_t count = 0;
    for (const auto& actor : model.level2Entities()) {
        if (!actor.active() || !actor.ownerIsPlayer || actor.type != 23
            || actor.stateBytes4dTo52[0] != 0xfd) { continue; }
        requireCampaignComparison(actor.position.z == 0 && actor.forwardVelocity == 500
            && actor.stateBytes4dTo52[1] <= 64, "natural blue shot lost its ground primary constructor");
        ++count;
    }
    return count;
}

std::unique_ptr<RecoveredFrontend> begin(const GameplayAssets& data, const NativeStartupAssets& startup,
    const FrameCopyProfile copy, const std::size_t budget, std::vector<std::int16_t>& pcm) {
    auto frontend = std::make_unique<RecoveredFrontend>(data, startup, copy, budget,
        std::shared_ptr<HighscoreStore>{}, CampaignExecution::playable);
    unsigned menuPoll = 0;
    for (unsigned call = 0; !frontend->session() && call < maximumCalls; ++call) {
        NativeMenuInput input{};
        if (frontend->startup().phase() == NativeStartupPhase::awaitingMenuDecision) {
            input.confirm = menuPoll++ == 1;
        }
        const auto before = frontend->elapsedSamples();
        const auto delivery = frontend->advanceStartup(input);
        requireCampaignComparison(delivery.firstSample == before
            && before + delivery.samples.size() == frontend->elapsedSamples(), "startup lost PCM prefix");
        pcm.insert(pcm.end(), delivery.samples.begin(), delivery.samples.end());
    }
    requireCampaignComparison(frontend->session() && menuPoll == 3, "actual Begin did not reach source Level1");
    return frontend;
}

GameplayInput destinationControls(const GameplayModel& model) {
    const Level1Entity* nearest = nullptr;
    for (const auto& actor : model.level2Entities()) {
        if (!actor.active() || actor.stateBytes4dTo52[0] != 0xff || actor.type < 1
            || actor.type > 16 || actor.type == 7) { continue; }
        if (!nearest || approximatePlanarDistance(model.level2Player().position, actor.position)
            < approximatePlanarDistance(model.level2Player().position, nearest->position)) { nearest = &actor; }
    }
    return nearest ? test::campaignPilotControls(model, *nearest, test::CampaignPilotGoal::combat,
        test::CampaignPilotPrediction::angular) : GameplayInput{};
}

void route(const std::filesystem::path& resources) {
    const auto data = naturalRoute::assets(resources);
    const auto startup = decodeNativeStartupAssets(naturalRoute::read(resources / "STELART.RES"));
    const auto drivers = decodeTaggedPayloads(naturalRoute::read(resources / "STELLAR.RES"), {'V', 'G', 'A', ':'});
    requireCampaignComparison(drivers.size() == 1, "missing actual VGA driver");
    const auto copy = decodeFrameCopyProfile(drivers.front());
    test::CampaignBudgetComparison comparison;
    auto left = begin(data, startup, copy, 10000, comparison.leftPcm);
    auto right = begin(data, startup, copy, 257, comparison.rightPcm);
    requireCampaignComparison(comparison.leftPcm == comparison.rightPcm, "startup budgets changed whole PCM");
    const auto* leftOwner = left->session();
    const auto* rightOwner = right->session();
    const auto continues = leftOwner->model().remainingContinues();
    auto previousClock = leftOwner->synth().driverState().normalClockCycles;
    const std::vector<Model3d> shared(data.models.begin(), data.models.begin() + sharedModelCount);
    naturalBlue::Controls controls;
    WarpProof first{0}, second{3};
    for (unsigned call = 0; call < maximumCalls; ++call) {
        const auto& beforeModel = leftOwner->model();
        const auto before = beforeModel.phase();
        const auto bank = beforeModel.campaignLevel();
        auto& proof = bank == 0 ? first : second;
        GameplayInput input{};
        if (before == GameplayPhase::playable) {
            if (bank == 0 || bank == 3) {
                proof.observeInitial(beforeModel); //Before first control in each bank.
                input = controls(beforeModel);
            } else if (bank == 6) { input = destinationControls(beforeModel); }
        }
        //The source Level1->common-pool adoption call remains neutral.
        const auto inheritedPlayerShots = playerProjectiles(beforeModel);
        std::optional<StellarRng> expectedLoader;
        if (before == GameplayPhase::level2SuccessReturn) {
            requireCampaignComparison((bank == 0 || bank == 3)
                && beforeModel.pendingPlayableCampaignDestination() == bank + 3,
                "actual blue26 lost its source+3 destination");
            proof.returned = true;
            expectedLoader = beforeModel.level2Rng();
            constructCampaignLoaderActors(campaignLevelDefinition(static_cast<std::uint8_t>(bank + 3), 0).table,
                *expectedLoader, shared, data.campaignLevels.at(bank + 3).models);
        }
        const auto sample = left->elapsedSamples();
        const auto a = left->advanceGameplay(input);
        const auto b = right->advanceGameplay(input);
        const auto& model = leftOwner->model();
        const bool boundary = before != model.phase() || bank != model.campaignLevel()
            || (before == GameplayPhase::playable && bank == 6 && second.destinationFrames == 2);
        comparison.compare(*leftOwner, *rightOwner, a, b, boundary
            ? test::CampaignComparisonStage::boundary : test::CampaignComparisonStage::frame);
        requireCampaignComparison(left->session() == leftOwner && right->session() == rightOwner
            && a.firstSample == sample && sample + a.samples.size() == left->elapsedSamples(),
            "natural blue route replaced session or lost PCM prefix");
        const auto clock = leftOwner->synth().driverState().normalClockCycles;
        requireCampaignComparison(clock >= previousClock, "natural blue route reset retained work clock");
        previousClock = clock;
        requireCampaignComparison(model.remainingContinues() == continues && model.playerArmor() >= 0,
            "natural blue route died or retried before positive milestones");
        if ((bank == 0 || bank == 3) && proof.producerSlot && model.campaignLevel() == bank) {
            proof.observeAfter(*leftOwner, inheritedPlayerShots);
        }
        if (expectedLoader) {
            requireCampaignComparison(model.campaignLevel() == bank + 3
                && model.phase() == GameplayPhase::playableLoaderReturn, "natural warp missed actual destination loader");
            test::compareCampaignRng(model.level2Rng(), *expectedLoader, test::CampaignComparisonStage::boundary);
            proof.loaded = true;
        }
        auto& departed = model.campaignLevel() == 3 ? first : second;
        if (model.campaignLevel() == departed.bank + 3 && model.phase() == GameplayPhase::playablePreloopReturn) {
            departed.preloop = true;
        }
        if (before == GameplayPhase::playable && model.phase() == GameplayPhase::playable
            && bank == model.campaignLevel() && bank == departed.bank + 3) { ++departed.destinationFrames; }
        if (model.campaignLevel() == 6 && second.destinationFrames == 3) {
            first.complete();
            second.complete();
            //Repeat full history/RNG comparison at this final named boundary.
            requireCampaignComparison(comparison.leftPcm == comparison.rightPcm, "final whole PCM differs");
            test::compareCampaignRng(model.level2Rng(), rightOwner->model().level2Rng(), test::CampaignComparisonStage::boundary);
            std::cout << "Actual startup/Begin, natural16HP F3/26/blue+3 twice -> Level7+3 budget twins; calls="
                << call + 1 << " samples=" << left->elapsedSamples() << '\n';
            return;
        }
    }
    throw std::runtime_error("natural startup blue route did not reach positive Level7+3 within fixed cap");
}
}

int main(int argc, char** argv) {
    if (argc != 2) { return 2; }
    try { route(argv[1]); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
