#include "stellar7/gameplay.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <tuple>

namespace {
void require(const bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

auto actorState(const stellar7::Level1Entity& actor) {
    return std::tie(actor.flags, actor.type, actor.collisionRadius,
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

template<std::size_t Index = 0, class Tuple>
void fieldDifferences(const Tuple& left, const Tuple& right) {
    if constexpr (Index < std::tuple_size<Tuple>::value) {
        if (std::get<Index>(left) != std::get<Index>(right)) {
            std::cerr << ' ' << Index;
        }
        fieldDifferences<Index + 1>(left, right);
    }
}

void sameRng(stellar7::StellarRng left, stellar7::StellarRng right) {
    require(left.calls() == right.calls() && left.indexA() == right.indexA()
        && left.indexB() == right.indexB(), "retained RNG cursor mismatch");
    for (std::size_t index = 0; index < stellar7::StellarRng::wordCount; ++index) {
        require(left.next() == right.next(), "retained RNG word mismatch");
    }
}
}

namespace stellar7 {
struct GameplayModelTestAccess {
    static void load(GameplayModel& model, const std::uint8_t level,
        const std::vector<Model3d>& shared, const std::vector<Model3d>& local, StellarRng rng) {
        auto actors = constructCampaignLoaderActors(campaignLevelDefinition(level, 0).table, rng, shared, local);
        model.playableCampaignFrames_ = true;
        model.initializeRetainedCampaignLevel(level, std::move(actors), rng, shared, local);
        sameRng(model.level2Rng_, rng);
    }

    using Type6Timers = std::vector<std::pair<std::size_t, std::uint16_t>>;

    static Type6Timers expectedType6Timers(const GameplayModel& model) {
        constexpr std::uint16_t cloakCycleTicks = 200;
        Type6Timers timers;
        for (std::size_t index = 0; index < model.level2Entities_.size(); ++index) {
            const auto& actor = model.level2Entities_[index];
            if (actor.type != 6) {
                continue;
            }
            auto expected = actor.type6CloakTimer;
            //9298 dispatches only inherited FF pointers; preloop actors can
            //first enter that list at the following completed main display.
            const auto inherited = std::find(model.level2ActiveListOrder_.begin(),
                model.level2ActiveListOrder_.end(), index) != model.level2ActiveListOrder_.end();
            if (inherited && actor.stateBytes4dTo52[0] == 0xff && actor.stateBytes4dTo52[4] != 0) {
                expected = expected == 0 ? cloakCycleTicks : static_cast<std::uint16_t>(expected - 1);
            }
            timers.emplace_back(model.level2EntityActorPoolSlots_[index], expected);
        }
        return timers;
    }

    static void verifyType6Timers(const GameplayModel& model, const Type6Timers& timers) {
        for (const auto& timer : timers) {
            const auto owner = std::find(model.level2EntityActorPoolSlots_.begin(),
                model.level2EntityActorPoolSlots_.end(), timer.first);
            require(owner != model.level2EntityActorPoolSlots_.end(), "Type6 callback lost its physical owner");
            const auto& actor = model.level2Entities_[static_cast<std::size_t>(owner - model.level2EntityActorPoolSlots_.begin())];
            require(actor.type6CloakTimer == timer.second, "Type6 cloak word disagrees with inherited callback membership");
        }
    }

    static void aliases(const GameplayModel& model) {
        std::vector<bool> ordinary(model.level2ActorPoolSize_);
        std::vector<bool> fixed(model.level2FixedObjectPoolSize_);
        std::vector<bool> projectile(model.level2ProjectilePoolSize_);
        std::vector<bool> effect(model.level2DeathEffectPoolSize_);
        for (std::size_t index = 0; index < model.level2Entities_.size(); ++index) {
            const auto ordinarySlot = model.level2EntityActorPoolSlots_.at(index);
            const auto fixedSlot = model.level2EntityFixedPoolSlots_.at(index);
            const auto projectileSlot = model.level2EntityProjectilePoolSlots_.at(index);
            const auto effectSlot = model.level2EntityDeathEffectPoolSlots_.at(index);
            const auto owners = static_cast<unsigned>(ordinarySlot < ordinary.size())
                + static_cast<unsigned>(fixedSlot < fixed.size())
                + static_cast<unsigned>(projectileSlot < projectile.size())
                + static_cast<unsigned>(effectSlot < effect.size());
            require(owners == 1, "logical actor does not have exactly one physical owner");
            const Level1Entity* physical = nullptr;
            if (ordinarySlot < ordinary.size()) {
                require(!ordinary[ordinarySlot] && model.level2ActorPoolActive_[ordinarySlot], "ordinary pool alias mismatch");
                ordinary[ordinarySlot] = true;
                physical = &model.level2ActorPool_[ordinarySlot];
            } else if (fixedSlot < fixed.size()) {
                require(!fixed[fixedSlot] && model.level2FixedPoolKnown_[fixedSlot], "fixed pool alias mismatch");
                fixed[fixedSlot] = true;
                physical = &model.level2FixedPool_[fixedSlot];
            } else if (projectileSlot < projectile.size()) {
                require(!projectile[projectileSlot] && model.level2ProjectilePoolActive_[projectileSlot], "projectile pool alias mismatch");
                projectile[projectileSlot] = true;
                physical = &model.level2ProjectilePool_[projectileSlot];
            } else {
                require(!effect[effectSlot] && model.level2DeathEffectPoolActive_[effectSlot], "effect pool alias mismatch");
                effect[effectSlot] = true;
                physical = &model.level2DeathEffectPool_[effectSlot];
            }
            const auto& actor = model.level2Entities_[index];
            if (actorState(actor) != actorState(*physical)) {
                std::cerr << "alias level=" << model.campaignLevel() + 1 << " tick=" << model.level2LoopTick()
                    << " actor=" << index << " type=" << actor.type << " position=" << actor.position.x << ','
                    << actor.position.y << ',' << actor.position.z << "/" << physical->position.x << ','
                    << physical->position.y << ',' << physical->position.z << " flags=" << unsigned(actor.flags)
                    << '/' << unsigned(physical->flags) << " distance=" << actor.presenterDistance << '/'
                    << physical->presenterDistance << " heading=" << actor.targetHeading << '/' << physical->targetHeading
                    << " animation=" << unsigned(actor.modelAnimationVariant) << '/' << unsigned(physical->modelAnimationVariant)
                    << " fields=";
                fieldDifferences(actorState(actor), actorState(*physical));
                std::cerr << '\n';
                throw std::runtime_error("logical/physical actor state diverged");
            }
        }
        for (std::size_t slot = 0; slot < ordinary.size(); ++slot) {
            require(ordinary[slot] == model.level2ActorPoolActive_[slot], "orphaned active ordinary record");
        }
        for (std::size_t slot = 0; slot < projectile.size(); ++slot) {
            require(projectile[slot] == model.level2ProjectilePoolActive_[slot], "orphaned active projectile record");
        }
        for (std::size_t slot = 0; slot < effect.size(); ++slot) {
            require(effect[slot] == model.level2DeathEffectPoolActive_[slot], "orphaned active effect record");
        }
    }

    static std::int16_t active(const GameplayModel& model) { return model.level2ActiveEnemyCount_; }

    static void successReturn(GameplayModel& model, const CampaignExit exit) {
        //Controlled component boundary; no claim that ordinary frames earned it.
        model.phase_ = GameplayPhase::level2SuccessReturn;
        model.level2TailConsumed_ = true;
        model.level2Complete_ = true;
        model.successAudioOwned_ = true;
        model.successAudioActive_ = false;
        model.level2SuccessContinuation_.reset();
        model.playableExit_ = exit;
    }

    static std::int16_t placeHazard(GameplayModel& model) {
        const auto found = std::find_if(model.level2Entities_.begin(), model.level2Entities_.end(), [](const auto& actor) {
            return actor.type >= 14 && actor.type <= 16;
        });
        require(found != model.level2Entities_.end(), "controlled contact requires an actual table hazard");
        const auto index = static_cast<std::size_t>(found - model.level2Entities_.begin());
        require(std::find(model.level2ActiveListOrder_.begin(), model.level2ActiveListOrder_.end(), index)
            != model.level2ActiveListOrder_.end(), "table hazard absent from retained walk");
        model.level2Player_.forwardVelocity = 0;
        model.level2Player_.yawVelocity = 0;
        found->position = model.level2Player_.position;
        model.level2ActorPool_[model.level2EntityActorPoolSlots_[index]] = *found;
        return found->type;
    }
};
}

namespace {
using namespace stellar7;

std::vector<Model3d> models(const std::filesystem::path& root, const std::string& name) {
    std::ifstream file(root / name, std::ios::binary);
    if (!file) {
        throw std::runtime_error("missing original model resource: " + name);
    }
    return decodeFirstModelTableResource({std::istreambuf_iterator<char>(file), {}});
}

void compare(const GameplayModel& left, const GameplayModel& right) {
    require(left.phase() == right.phase() && left.ticks() == right.ticks()
        && left.level2LoopTick() == right.level2LoopTick(), "adjacent frame cursor diverged");
    require(left.playerArmor() == right.playerArmor() && left.score() == right.score(), "adjacent player state diverged");
    require(left.level2ActiveListOrder() == right.level2ActiveListOrder(), "adjacent presenter order diverged");
    require(left.level2Ground().points() == right.level2Ground().points(), "adjacent ground state diverged");
    require(actorState(left.level2Player()) == actorState(right.level2Player()), "adjacent player actor diverged");
    require(left.level2Entities().size() == right.level2Entities().size(), "adjacent roster size diverged");
    for (std::size_t index = 0; index < left.level2Entities().size(); ++index) {
        require(actorState(left.level2Entities()[index]) == actorState(right.level2Entities()[index]), "adjacent actor diverged");
    }
    require(left.level2AudioEvents().size() == right.level2AudioEvents().size(), "adjacent audio count diverged");
    for (std::size_t index = 0; index < left.level2AudioEvents().size(); ++index) {
        require(left.level2AudioEvents()[index].loopTick == right.level2AudioEvents()[index].loopTick
            && left.level2AudioEvents()[index].eventId == right.level2AudioEvents()[index].eventId,
            "adjacent audio sequence diverged");
    }
    sameRng(left.level2Rng(), right.level2Rng());
    GameplayModelTestAccess::aliases(left);
    GameplayModelTestAccess::aliases(right);
}

void frame(GameplayModel& model, const GameplayInput& input) {
    const auto before = model.level2LoopTick();
    const auto randomCalls = model.level2Rng().calls();
    model.advancePlayableCampaign(input);
    require(model.phase() == (model.campaignLevel() == 2 ? GameplayPhase::level3Native : GameplayPhase::playable),
        "ordinary constructor continuation entered a guardian/failure/transition phase");
    require(model.level2LoopTick() == static_cast<std::uint16_t>(before + 1), "main did not advance exactly once");
    GameplayModelTestAccess::aliases(model);
    model.finishLevel3Radar();
    GameplayModelTestAccess::aliases(model);
    model.finishPlayableCampaignFrame();
    require(model.level2LoopTick() == static_cast<std::uint16_t>(before + 1), "tail repeated main ordinal");
    require(model.level2Rng().calls() >= randomCalls, "frame reset retained RNG");
    GameplayModelTestAccess::aliases(model);
}

void projectileMotion(const std::vector<Model3d>& shared, const std::vector<Model3d>& local) {
    auto model = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::load(*model, 2, shared, local, StellarRng{});
    model->completeLevel3Preloop();
    GameplayInput input{};
    input.forward = input.fire = true;
    model->advancePlayableCampaign(input);
    model->finishLevel3Radar();
    model->finishPlayableCampaignFrame();
    const auto shot = [&]() -> const Level1Entity& {
        const auto found = std::find_if(model->level2Entities().begin(), model->level2Entities().end(), [](const auto& actor) {
            return actor.type == 23 && actor.active();
        });
        require(found != model->level2Entities().end(), "FD motion probe lost its first player projectile");
        return *found;
    };
    auto expected = shot();
    const auto birth = expected.position;
    advanceEntityKinematics(expected);
    require(!(expected.position == birth), "FD motion probe has zero source displacement");
    model->advancePlayableCampaign({});
    require(shot().position == expected.position, "inherited FD projectile skipped source3B3A integration");
    model->finishLevel3Radar();
    expected.position.x -= model->level2Player().position.x;
    expected.position.y -= model->level2Player().position.y;
    model->finishPlayableCampaignFrame();
    require(shot().position == expected.position, "inherited FD projectile lost tail recenter displacement");
    std::cout << "Inherited player FD: source3B3A integration and adjacent tail recenter\n";
}

void level(const std::vector<Model3d>& shared, const std::vector<Model3d>& local, const std::uint8_t index) {
    StellarRng incoming;
    for (unsigned call = 0; call < 37U + index; ++call) {
        incoming.next();
    }
    auto left = std::make_unique<GameplayModel>();
    auto right = std::make_unique<GameplayModel>();
    GameplayModelTestAccess::load(*left, index, shared, local, incoming);
    GameplayModelTestAccess::load(*right, index, shared, local, incoming);
    compare(*left, *right);
    auto groundRng = left->level2Rng();
    const Level1GroundState expectedGround(groundRng);
    for (const auto model : {left.get(), right.get()}) {
        if (index == 2) {
            model->completeLevel3Preloop();
        } else {
            model->completePlayableCampaignPreloop();
        }
        require(model->level2Ground().points() == expectedGround.points(), "preloop did not consume continuing ground RNG");
        sameRng(model->level2Rng(), groundRng);
    }
    bool playerProjectile = false;
    for (unsigned ordinal = 1; ordinal <= 3; ++ordinal) {
        GameplayInput input{};
        input.forward = true;
        input.left = ordinal != 3;
        input.right = ordinal == 3;
        input.fire = true;
        const auto type6Timers = GameplayModelTestAccess::expectedType6Timers(*left);
        frame(*left, input);
        frame(*right, input);
        compare(*left, *right);
        GameplayModelTestAccess::verifyType6Timers(*left, type6Timers);
        for (const auto& actor : left->level2Entities()) {
            if (actor.type >= 17 && actor.type <= 23) {
                require(actor.collisionRadius == campaignModelRadius(shared, local, actor.type), "projectile lost actual model radius");
                playerProjectile = playerProjectile || actor.type == 23;
            }
            if (actor.type == 6) {
                require(actor.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback
                    && !actor.animatedModelPresenterEnabled, "Type6 callback lost inline selector ownership");
            }
        }
    }
    require(playerProjectile, "steering/fire frames did not allocate player projectile");
    require(left->level2Player().forwardVelocity != 0 && left->level2Player().yaw != 0, "steering/forward controls were ignored");
    if (index == 3 || index == 5 || index == 6) {
        const auto hazard = GameplayModelTestAccess::placeHazard(*left);
        require(GameplayModelTestAccess::placeHazard(*right) == hazard, "controlled hazard identity diverged");
        const auto active = GameplayModelTestAccess::active(*left);
        const auto armor = left->playerArmor();
        const auto count = std::count_if(left->level2Entities().begin(), left->level2Entities().end(), [hazard](const auto& actor) {
            return actor.type == hazard;
        });
        frame(*left, {});
        frame(*right, {});
        compare(*left, *right);
        require(GameplayModelTestAccess::active(*left) == active - 1, "hazard contact did not decrement active count");
        if (left->playerArmor() != armor - 200) {
            std::cerr << "hazard level=" << unsigned(index + 1) << " type=" << hazard
                << " armor=" << armor << "->" << left->playerArmor() << " events=";
            for (const auto& event : left->level2AudioEvents()) {
                std::cerr << event.loopTick << ':' << event.eventId << ',';
            }
            std::cerr << '\n';
            throw std::runtime_error("hazard contact did not apply source50 damage units");
        }
        require(std::count_if(left->level2Entities().begin(), left->level2Entities().end(), [hazard](const auto& actor) {
            return actor.type == hazard;
        }) == count - 1, "hazard contact did not retire its physical record");
    }
    std::cout << "Level" << unsigned(index + 1) << " actual constructors -> main/radar/tail +3; rng="
        << left->level2Rng().calls() << " actors=" << left->level2Entities().size()
        << ((index == 3 || index == 5 || index == 6) ? "; controlled hazard contact" : "") << '\n';
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

void transitions(const std::vector<Model3d>& shared, const std::vector<std::vector<Model3d>>& local) {
    for (std::uint8_t index = 0; index < 7; ++index) {
        for (const auto exit : {CampaignExit::greenReward, CampaignExit::blueWarp}) {
            auto model = std::make_unique<GameplayModel>();
            StellarRng incoming;
            for (unsigned call = 0; call < 19U + index; ++call) {
                incoming.next();
            }
            GameplayModelTestAccess::load(*model, index, shared, local[index], incoming);
            const auto constructorRng = model->level2Rng();
            const auto loaderPhase = model->phase();
            rejects([&]() { model->pendingPlayableCampaignDestination(); }, "destination accepted an unfinished success return");
            rejects([&]() { model->completePlayableCampaignEnding(); }, "ending accepted a loader phase");
            require(model->phase() == loaderPhase, "wrong-phase rejection changed the loader phase");
            sameRng(model->level2Rng(), constructorRng);
            GameplayModelTestAccess::successReturn(*model, exit);
            const auto expected = static_cast<std::uint8_t>(index + (exit == CampaignExit::greenReward ? 1 : 3));
            require(model->pendingPlayableCampaignDestination() == expected, "green/blue destination lost source increment");
            const std::uint8_t mismatched = expected == 0 ? 1 : 0;
            rejects([&]() { model->completePlayableCampaignLoad(mismatched, shared, local[mismatched]); },
                "loader accepted a mismatched destination");
            require(model->phase() == GameplayPhase::level2SuccessReturn && model->campaignLevel() == index,
                "mismatched destination changed retained state");
            sameRng(model->level2Rng(), constructorRng);
            if (expected < 7) {
                rejects([&]() { model->completePlayableCampaignEnding(); }, "ending accepted a playable destination");
                auto reference = model->level2Rng();
                const auto actors = constructCampaignLoaderActors(campaignLevelDefinition(expected, 0).table,
                    reference, shared, local[expected]);
                model->completePlayableCampaignLoad(expected, shared, local[expected]);
                require(model->campaignLevel() == expected && model->level2Entities().size() == actors.size(),
                    "success return loaded the wrong bank");
                require(model->phase() == (expected == 2 ? GameplayPhase::level3LoaderReturn : GameplayPhase::playableLoaderReturn),
                    "success return did not retain its loader boundary");
                sameRng(model->level2Rng(), reference);
                GameplayModelTestAccess::aliases(*model);
            } else {
                rejects([&]() { model->completePlayableCampaignLoad(expected, shared, local[index]); },
                    "ending destination was treated as a model bank");
                model->completePlayableCampaignEnding();
                require(model->campaignLevel() == 7 && model->phase() == GameplayPhase::endingEntry,
                    "final green/blue exit did not enter ending boundary");
                sameRng(model->level2Rng(), constructorRng);
                rejects([&]() { model->completePlayableCampaignEnding(); }, "ending boundary accepted double consumption");
            }
        }
    }
    std::cout << "Controlled success returns: seven banks, green+1/blue+3, actual loaders and ending entry\n";
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
        require(shared.size() == 44, "actual shared registry must contain44 models");
        std::vector<std::vector<Model3d>> local;
        for (unsigned index = 0; index < 7; ++index) {
            local.push_back(models(root, "LEVEL" + std::to_string(index + 1) + ".RES"));
        }
        projectileMotion(shared, local[2]);
        //This is controlled per-level constructor continuity, not a natural campaign route.
        for (std::uint8_t index = 2; index < 7; ++index) {
            level(shared, local[index], index);
        }
        transitions(shared, local);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
