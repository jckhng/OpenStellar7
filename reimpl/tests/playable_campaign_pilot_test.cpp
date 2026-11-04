#include "playable_campaign_pilot.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <tuple>

namespace {
using namespace stellar7;
void require(const bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

auto entityState(const Level1Entity& actor) {
    return std::tie(actor.allocationIndex, actor.flags, actor.type, actor.collisionRadius,
        actor.position.x, actor.position.y, actor.position.z, actor.pitch, actor.roll, actor.yaw,
        actor.pitchVelocity, actor.rollVelocity, actor.yawVelocity, actor.xVelocity, actor.yVelocity,
        actor.zVelocity, actor.forwardVelocity, actor.stateBytes4dTo52, actor.modelAnimationVariant,
        actor.primaryType11AnimationSelector, actor.behaviorTimer, actor.type12MissGate,
        actor.type6CloakTimer, actor.presenterDistance, actor.targetHeading, actor.weaponSlotCount,
        actor.weaponCooldown, actor.weaponSlotTimers, actor.collisionContactAllocationIndex,
        actor.collisionContactIsPlayer, actor.presentationModelAnimationVariant,
        actor.animatedModelPresenterEnabled, actor.ownerIsPlayer, actor.ownerAllocationIndex,
        actor.renderProfile);
}

template<class Actors> void sameActors(const Actors& left, const Actors& right) {
    require(left.size() == right.size(), "pilot changed retained actor membership");
    for (std::size_t index = 0; index < left.size(); ++index) {
        require(entityState(left[index]) == entityState(right[index]), "pilot changed a retained actor record");
    }
}

void sameRng(StellarRng left, StellarRng right) {
    require(left.calls() == right.calls() && left.indexA() == right.indexA() && left.indexB() == right.indexB(),
        "pilot changed retained RNG cursors");
    for (std::size_t index = 0; index < StellarRng::wordCount; ++index) {
        require(left.next() == right.next(), "pilot changed a retained RNG word");
    }
}
} //namespace

namespace stellar7 {
struct GameplayModelTestAccess {
    enum class Target { selectedMine, otherActor };

    static void fixture(GameplayModel& model, const std::vector<Model3d>& shared,
        const std::int16_t mineType, const Target target) {
        model.phase_ = GameplayPhase::playable;
        model.activeCampaignLevel_ = mineType == 14 ? 3 : mineType - 10;
        model.level2Player_ = {};
        model.level2Player_.collisionRadius = shared.at(0).collisionRadius;
        model.level2Player_.renderProfile = WorldActorRenderProfile::noHandles;
        Level1Entity mine{};
        mine.type = mineType;
        mine.position = {0, 6000, 0};
        mine.collisionRadius = shared.at(14).collisionRadius;
        mine.stateBytes4dTo52 = {0xff, 1, 0, 0, 4, 0};
        mine.renderProfile = WorldActorRenderProfile::noHandles;
        model.level2Entities_ = {mine};
        if (target == Target::otherActor) {
            Level1Entity actor{};
            actor.allocationIndex = 1;
            actor.type = 1;
            actor.position = {0, 12000, 0};
            actor.collisionRadius = shared.at(1).collisionRadius;
            actor.stateBytes4dTo52 = {0xff, 1, 0, 0, 1, 0};
            actor.renderProfile = WorldActorRenderProfile::noHandles;
            model.level2Entities_.push_back(actor);
        }
        for (std::size_t index = 0; index < model.level2Entities_.size(); ++index) {
            model.level2ActorPool_[index] = model.level2Entities_[index];
        }
    }

    static void observedOrbit(GameplayModel& model) {
        //Component pose from the retained Level3 retry stall, not a route entry.
        model.phase_ = GameplayPhase::playable;
        model.activeCampaignLevel_ = 2;
        model.level2Player_ = {};
        model.level2Player_.yaw = 36400;
        model.level2Player_.yawVelocity = 192;
        model.level2Player_.forwardVelocity = 200;
        Level1Entity target{};
        target.type = 12;
        target.position = {451, -9339, 150};
        target.yaw = 43720;
        target.yawVelocity = 271;
        target.forwardVelocity = 360;
        target.collisionRadius = 1016;
        target.stateBytes4dTo52 = {0xff, 1, 0, 0, 3, 3};
        model.level2Entities_ = {target};
        model.level2ActorPool_[0] = target;
    }

    static void unchanged(const GameplayModel& model, const GameplayModel& before) {
        require(model.phase_ == before.phase_ && model.ticks_ == before.ticks_
            && model.activeCampaignLevel_ == before.activeCampaignLevel_
            && model.level2LoopTick_ == before.level2LoopTick_
            && model.level2PlayerArmor_ == before.level2PlayerArmor_
            && model.level2Score_ == before.level2Score_
            && model.level2ActiveListOrder_ == before.level2ActiveListOrder_
            && model.level2AllocationListOrder_ == before.level2AllocationListOrder_
            && model.level2AllocationRevision_ == before.level2AllocationRevision_,
            "pilot changed retained model state");
        require(entityState(model.level2Player_) == entityState(before.level2Player_), "pilot changed the player");
        sameActors(model.level2Entities_, before.level2Entities_);
        sameActors(model.level2ActorPool_, before.level2ActorPool_);
        sameActors(model.level2FixedPool_, before.level2FixedPool_);
        sameActors(model.level2ProjectilePool_, before.level2ProjectilePool_);
        sameActors(model.level2DeathEffectPool_, before.level2DeathEffectPool_);
        sameRng(model.level2Rng_, before.level2Rng_);
        sameRng(model.level1_.rng(), before.level1_.rng());
    }
};
} //namespace stellar7

int main(int argc, char** argv) {
    if (argc != 2) { return 2; }
    try {
        std::ifstream input(std::filesystem::path(argv[1]) / "COCKPIT.RES", std::ios::binary);
        require(static_cast<bool>(input), "missing immutable shared-model resource");
        const auto shared = decodeFirstModelTableResource({std::istreambuf_iterator<char>(input), {}});
        require(shared.at(0).collisionRadius > 0 && shared.at(1).collisionRadius > 0
            && shared.at(14).collisionRadius > 0, "controller fixture requires actual positive radii");
        bool passed = true;
        for (const std::int16_t type : {14, 15, 16}) {
            for (const auto target : {GameplayModelTestAccess::Target::selectedMine, GameplayModelTestAccess::Target::otherActor}) {
                auto model = std::make_unique<GameplayModel>();
                GameplayModelTestAccess::fixture(*model, shared, type, target);
                const auto before = std::make_unique<GameplayModel>(*model);
                const auto index = target == GameplayModelTestAccess::Target::selectedMine ? 0U : 1U;
                const auto* borrowed = &model->level2Entities()[index];
                const auto controls = test::campaignPilotControls(*model, *borrowed, test::CampaignPilotGoal::combat);
                GameplayModelTestAccess::unchanged(*model, *before);
                require(borrowed == &model->level2Entities()[index], "pilot invalidated the retained target reference");
                const bool steering = controls.left || controls.right;
                const bool selected = target == GameplayModelTestAccess::Target::selectedMine;
                const bool correct = (selected ? !steering : steering) && controls.forward && controls.fire && !controls.reverse;
                passed = passed && correct;
                std::cout << "Declared Type" << type << (selected ? " selected mine" : " distinct mine obstacle")
                    << " steering=" << steering << " forward=" << controls.forward << " fire=" << controls.fire
                    << " immutable=1 " << (correct ? "PASS" : "FAIL") << '\n';
            }
        }
        auto orbit = std::make_unique<GameplayModel>();
        GameplayModelTestAccess::observedOrbit(*orbit);
        const auto orbitBefore = std::make_unique<GameplayModel>(*orbit);
        const auto& target = orbit->level2Entities().front();
        const auto angular = test::campaignPilotControls(*orbit, target, test::CampaignPilotGoal::combat,
            test::CampaignPilotPrediction::angular);
        const auto linear = test::campaignPilotControls(*orbit, target, test::CampaignPilotGoal::combat);
        GameplayModelTestAccess::unchanged(*orbit, *orbitBefore);
        //The observed angular intercept is 5189 units from the linear aim;
        //3A1C's half-sum collision allowance for this target/shot is only575.
        require(angular.left && !angular.right && !angular.fire && linear.fire,
            "angular pilot failed to reject the observed Type12 false firing window");
        std::cout << "Observed Type12 orbit: angular false-shot rejection, legacy policy and immutable state PASS\n";
        if (!passed) { throw std::runtime_error("selected-mine avoidance contradicts retained combat aim"); }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
