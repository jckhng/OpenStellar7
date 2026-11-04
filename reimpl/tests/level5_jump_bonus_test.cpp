#include "stellar7/gameplay.hpp"
#include "playable_jump.hpp"

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

std::vector<stellar7::Model3d> models(const std::filesystem::path& root, const char* name) {
    std::ifstream input(root / name, std::ios::binary);
    require(static_cast<bool>(input), "missing bonus test resource");
    return stellar7::decodeFirstModelTableResource({std::istreambuf_iterator<char>(input), {}});
}

void sameRng(stellar7::StellarRng left, stellar7::StellarRng right) {
    require(left.calls() == right.calls() && left.indexA() == right.indexA()
        && left.indexB() == right.indexB(), "bonus rejection changed RNG cursors");
    for (std::size_t word = 0; word < stellar7::StellarRng::wordCount; ++word) {
        require(left.next() == right.next(), "bonus rejection changed RNG words");
    }
}
}

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void domain(Level1State& systems, const std::int16_t selected,
        const std::int16_t increment, const std::int8_t jumpCharges) {
        systems.systemStatusLevels_.fill(1);
        systems.systemStatusLevels_[2] = jumpCharges;
        systems.systemTimers_.fill(0);
        systems.systemTimers_[2] = increment;
        systems.systemTimers_[3] = 17; //Actual48D4 retains unrelated12D4.
        systems.selectedSystem_ = selected;
    }

    static void thirdKill(Level1State& systems, const std::int16_t type) {
        systems.lastDestroyedType_ = type;
        systems.consecutiveDestroyedTypeCount_ = 2;
    }
};

struct GameplayModelTestAccess {
    static std::size_t initialize(GameplayModel& model, const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local, const std::int16_t sourceType,
        const std::int16_t selected, const std::int16_t increment,
        const std::int32_t altitude, const std::int8_t jumpCharges) {
        StellarRng rng;
        auto actors = constructCampaignLoaderActors(campaignLevelDefinition(4, 0).table, rng, shared, local);
        model.playableCampaignFrames_ = true;
        model.initializeRetainedCampaignLevel(4, std::move(actors), rng, shared, local);
        model.completePlayableCampaignPreloop();

        //Declared component entry: the real90A4 third-kill producer, with its
        //source position at the player. No ordinary victory ancestry is claimed.
        Level1ReloadTestAccess::thirdKill(model.level1_, sourceType);
        Level1Entity source{};
        source.type = sourceType;
        source.position = model.level2Player_.position;
        const auto before = model.level2Rng_.calls();
        const auto slot = model.registerLevel5DestroyedTypeBonus(source);
        require(slot.has_value(), "actual bonus producer did not allocate an FE record");
        require(model.level2Rng_.calls() == before, "actual bonus producer drew RNG");
        const auto found = std::find(model.level2EntityFixedPoolSlots_.begin(),
            model.level2EntityFixedPoolSlots_.end(), *slot);
        require(found != model.level2EntityFixedPoolSlots_.end(), "bonus lost its physical slot");
        const auto index = static_cast<std::size_t>(found - model.level2EntityFixedPoolSlots_.begin());
        const auto& bonus = model.level2Entities_[index];
        require(bonus.type == 42 && bonus.collisionRadius == 330
            && bonus.position.z == 100 && bonus.stateBytes4dTo52[0] == 0xfe
            && bonus.stateBytes4dTo52[1] == 1, "bonus constructor identity changed");
        //Declared inherited3768 pointer list at the player-contact boundary.
        model.level2ActiveListOrder_ = model.level2AllocationListOrder_;
        Level1ReloadTestAccess::domain(model.level1_, selected, increment, jumpCharges);
        model.level2Player_.position.z = altitude;
        validatePlayableJumpState(model.playableJumpState());
        return *slot;
    }

    static void contact(GameplayModel& model) { model.advancePlayableCampaign({}); }
    static const Level1Entity& fixed(const GameplayModel& model, const std::size_t slot) {
        return model.level2FixedPool_[slot];
    }
    static void strict(GameplayModel& model) {
        model.playableCampaignFrames_ = false;
        model.phase_ = GameplayPhase::level5;
        model.advanceLevel2({});
    }
};
}

namespace {
using namespace stellar7;
enum class Admission { playable, strict };

void accepted(const std::vector<Model3d>& shared, const std::vector<Model3d>& local,
    const std::int16_t sourceType, const std::int16_t selected,
    const std::int16_t increment = 0, const std::int32_t altitude = 0,
    const std::int8_t charges = 1) {
    auto model = std::make_unique<GameplayModel>();
    const auto slot = GameplayModelTestAccess::initialize(*model, shared, local,
        sourceType, selected, increment, altitude, charges);
    const auto payload = GameplayModelTestAccess::fixed(*model, slot).stateBytes4dTo52[5];
    const auto statuses = model->level1State().systemStatusLevels();
    const auto timers = model->level1State().systemTimers();
    const auto score = model->score();
    const auto armor = model->playerArmor();
    const auto repairEvents = std::count_if(model->level2AudioEvents().begin(), model->level2AudioEvents().end(),
        [](const auto& event) { return event.eventId == 24; });
    GameplayModelTestAccess::contact(*model);
    auto expected = statuses;
    expected[payload - 1] = 3;
    require(!GameplayModelTestAccess::fixed(*model, slot).active(), "bonus physical owner survived collection");
    require(model->level1State().systemStatusLevels() == expected
        && model->level1State().systemTimers() == timers
        && model->level1State().selectedSystem() == selected, "repair reset another canonical system field");
    require(model->level2Player().position.z == altitude && model->playerArmor() == armor,
        "repair changed Jump geometry or armor");
    require(model->score() == score + 100
        && std::count_if(model->level2AudioEvents().begin(), model->level2AudioEvents().end(),
            [](const auto& event) { return event.eventId == 24; }) == repairEvents + 1,
        "repair score/audio was not once-only");
    require(std::none_of(model->level2Entities().begin(), model->level2Entities().end(),
        [](const auto& actor) { return actor.type == 42; }), "bonus retained a compact live alias");
}

void rejected(const std::vector<Model3d>& shared, const std::vector<Model3d>& local,
    const Admission admission, const std::int16_t increment,
    const std::int32_t altitude, const std::int8_t charges) {
    auto model = std::make_unique<GameplayModel>();
    const auto slot = GameplayModelTestAccess::initialize(*model, shared, local, 11, 2,
        increment, altitude, charges);
    const auto status = model->level1State().systemStatusLevels();
    const auto timers = model->level1State().systemTimers();
    const auto score = model->score();
    const auto armor = model->playerArmor();
    const auto events = model->level2AudioEvents().size();
    const auto rng = model->level2Rng();
    bool threw = false;
    try {
        if (admission == Admission::strict) { GameplayModelTestAccess::strict(*model); }
        else { GameplayModelTestAccess::contact(*model); }
    } catch (const std::invalid_argument&) { threw = true; }
    require(threw, "unqualified repair domain was admitted");
    require(GameplayModelTestAccess::fixed(*model, slot).stateBytes4dTo52[1] == 1,
        "rejected repair retired its physical pickup");
    require(model->level1State().systemStatusLevels() == status
        && model->level1State().systemTimers() == timers
        && model->score() == score && model->playerArmor() == armor
        && model->level2AudioEvents().size() == events, "repair rejection mutated gameplay payload");
    sameRng(model->level2Rng(), rng);
}
}

int main(int argc, char** argv) {
    if (argc != 2) { return 2; }
    try {
        const auto shared = models(argv[1], "COCKPIT.RES");
        const auto local = models(argv[1], "LEVEL5.RES");
        accepted(shared, local, 11, 2); //Grounded selected Jump.
        accepted(shared, local, 11, 3); //Grounded selected passive system.
        accepted(shared, local, 13, 2, 210, 220, -2); //Other repair preserves real flight.
        accepted(shared, local, 5, 2, -210, 430, 0); //Last-charge descent, another payload.
        rejected(shared, local, Admission::playable, 220, 0, -2); //Flight starts atZ0.
        rejected(shared, local, Admission::playable, 210, 220, -2);
        rejected(shared, local, Admission::strict, 0, 0, 1);
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "Actual Level5 bonus ownership: grounded repair, other-payload flight preservation, Jump flight rejection\n";
}
