#include "native_level2_frame_fixture.hpp"
#include "playable_campaign_pilot.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <tuple>

namespace {
using namespace stellar7;
constexpr std::size_t noObject = static_cast<std::size_t>(-1);
enum class RouteScope { throughLevel4,level2TransitionAudio };

void require(const bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void sameRng(StellarRng left, StellarRng right, const bool boundary) {
    require(left.calls() == right.calls() && left.indexA() == right.indexA() && left.indexB() == right.indexB(),
        "budget composition changed RNG cursors");
    if (boundary) {
        for (std::size_t index = 0; index < StellarRng::wordCount; ++index) {
            require(left.next() == right.next(), "budget composition changed RNG words at a boundary");
        }
    }
}

void sameFrame(const IndexedScreen& left, const IndexedScreen& right) {
    require(left.pixels == right.pixels && left.palette.size() == right.palette.size(),
        "budget composition changed indexed pixels/palette size");
    for (std::size_t index = 0; index < left.palette.size(); ++index) {
        require(left.palette[index].red == right.palette[index].red
            && left.palette[index].green == right.palette[index].green
            && left.palette[index].blue == right.palette[index].blue,
            "budget composition changed a palette color");
    }
}

struct Composition {
    std::size_t oplCursor = 0;
    std::vector<std::int16_t> leftPcm, rightPcm;

    void compare(const NativeGameplaySession& left, const NativeGameplaySession& right,
        const NativeSessionDelivery& a, const NativeSessionDelivery& b, const bool boundary) {
        require(a.firstSample == b.firstSample && a.samples == b.samples,
            "budget composition changed a PCM delivery");
        leftPcm.insert(leftPcm.end(),a.samples.begin(),a.samples.end());
        rightPcm.insert(rightPcm.end(),b.samples.begin(),b.samples.end());
        require(a.events.size() == b.events.size() && a.presentations.size() == b.presentations.size()
            && a.entry.size() == b.entry.size(), "budget composition changed delivery children");
        for (std::size_t index = 0; index < a.events.size(); ++index) {
            require(a.events[index].loopTick == b.events[index].loopTick
                && a.events[index].eventId == b.events[index].eventId, "budget composition changed ordered events");
        }
        for (std::size_t index = 0; index < a.entry.size(); ++index) {
            require(a.entry[index].elapsedNanoseconds == b.entry[index].elapsedNanoseconds
                && a.entry[index].samples == b.entry[index].samples, "budget composition changed entry delivery");
            sameFrame(a.entry[index].indexed,b.entry[index].indexed);
        }
        sameFrame(left.frame(),right.frame());
        for (std::size_t index = 0; index < a.presentations.size(); ++index) {
            require(a.presentations[index].sampleEnd == b.presentations[index].sampleEnd,
                "budget composition changed a presentation endpoint");
            sameFrame(a.presentations[index].indexed,b.presentations[index].indexed);
        }
        for (const auto role : {NativePageRole::cache,NativePageRole::back,NativePageRole::front}) {
            require(left.retainedPagePixels(role) == right.retainedPagePixels(role),
                "budget composition changed retained page pixels");
        }
        const auto pagesA = left.pageObservation(), pagesB = right.pageObservation();
        require(std::tie(pagesA.projection,pagesA.cache,pagesA.back,pagesA.front,pagesA.copy,pagesA.draw)
            == std::tie(pagesB.projection,pagesB.cache,pagesB.back,pagesB.front,pagesB.copy,pagesB.draw),
            "budget composition changed retained page roles");

        const auto& modelA = left.model();
        const auto& modelB = right.model();
        require(modelA.phase() == modelB.phase() && modelA.ticks() == modelB.ticks()
            && modelA.campaignLevel() == modelB.campaignLevel() && modelA.score() == modelB.score()
            && modelA.playerArmor() == modelB.playerArmor() && modelA.remainingContinues() == modelB.remainingContinues()
            && modelA.continueChoice() == modelB.continueChoice() && modelA.level2LoopTick() == modelB.level2LoopTick()
            && modelA.level2ActiveListOrder() == modelB.level2ActiveListOrder()
            && modelA.level2Ground().points() == modelB.level2Ground().points(),
            "budget composition changed retained campaign state");
        const auto& systemsA = modelA.level1State();
        const auto& systemsB = modelB.level1State();
        require(systemsA.systemTimers() == systemsB.systemTimers()
            && systemsA.systemStatusLevels() == systemsB.systemStatusLevels()
            && systemsA.selectedSystem() == systemsB.selectedSystem(), "budget composition changed system state");
        const auto actor = [](const Level1Entity& value) {
            return std::tie(value.allocationIndex,value.flags,value.type,value.position.x,value.position.y,value.position.z,
                value.pitch,value.roll,value.yaw,value.pitchVelocity,value.rollVelocity,value.yawVelocity,
                value.xVelocity,value.yVelocity,value.zVelocity,value.forwardVelocity,value.stateBytes4dTo52,
                value.modelAnimationVariant,value.primaryType11AnimationSelector,value.behaviorTimer,value.type12MissGate,
                value.type6CloakTimer,value.weaponSlotCount,value.weaponCooldown,value.weaponSlotTimers,
                value.collisionRadius,value.presenterDistance,value.targetHeading,value.collisionContactAllocationIndex,
                value.collisionContactIsPlayer,value.presentationModelAnimationVariant,value.animatedModelPresenterEnabled,
                value.ownerIsPlayer,value.ownerAllocationIndex,value.renderProfile);
        };
        require(actor(modelA.level2Player()) == actor(modelB.level2Player()), "budget composition changed the player");
        const auto& actorsA = modelA.level2Entities();
        const auto& actorsB = modelB.level2Entities();
        require(actorsA.size() == actorsB.size(), "budget composition changed actor membership");
        for (std::size_t index = 0; index < actorsA.size(); ++index) {
            require(actor(actorsA[index]) == actor(actorsB[index]), "budget composition changed an actor record");
        }
        sameRng(modelA.level2Rng(),modelB.level2Rng(),boundary);
        sameRng(modelA.failureRng(),modelB.failureRng(),boundary);
        require(left.elapsedSamples() == right.elapsedSamples(), "budget composition changed the retained sample epoch");
        const auto driver = [](const SoundBlasterDriverState& value) {
            return std::tie(value.notes,value.velocities,value.channelVoiceCursors,value.directTimers,value.directLevelLatch,
                value.clockSource,value.normalClockCycles,value.clockPhase,value.clockPeriod,value.sequencePosition,
                value.sequenceWait,value.runningStatus,value.fade,value.fadeDivider,value.sequenceActive,
                value.carrierKsl,value.carrierFallback,value.operatorKsl,value.operatorFallback,value.operatorEnabled,
                value.driverEnabled,value.initializationOplWrites,value.sequenceAllocated,value.sequenceSelected,
                value.sequencePositions,value.sequenceAllocations,value.sequenceSelections,value.sequenceActivity);
        };
        require(driver(left.synth().driverState()) == driver(right.synth().driverState()),
            "budget composition changed the retained audio driver");
        const auto& oplA = left.synth().registerWrites();
        const auto& oplB = right.synth().registerWrites();
        require(oplA.size() == oplB.size() && oplA.size() >= oplCursor,
            "budget composition changed OPL write count/history");
        for (std::size_t index = boundary ? 0 : oplCursor; index < oplA.size(); ++index) {
            require(oplA[index].registerNumber == oplB[index].registerNumber && oplA[index].value == oplB[index].value,
                "budget composition changed ordered OPL writes");
        }
        oplCursor = oplA.size();
        if (boundary) {
            require(leftPcm == rightPcm, "budget composition changed whole PCM at a boundary");
        }
    }
};

std::vector<Model3d> models(const std::filesystem::path& root, const std::string& name) {
    std::ifstream file(root / name,std::ios::binary);
    require(static_cast<bool>(file), "native continuity resource is missing");
    return decodeFirstModelTableResource({std::istreambuf_iterator<char>(file),{}});
}

std::size_t find(const GameplayModel& model, const std::int16_t first, const std::int16_t last) {
    const auto& actors = model.level2Entities();
    const auto actor = std::find_if(actors.begin(), actors.end(), [first,last](const auto& value) {
        return value.active() && value.type >= first && value.type <= last;
    });
    return actor == actors.end() ? noObject : static_cast<std::size_t>(actor - actors.begin());
}

GameplayInput approach(const GameplayModel& model, const Level1Entity& target, const test::CampaignPilotGoal goal) {
    const auto combat = goal == test::CampaignPilotGoal::combat;
    const auto& player = model.level2Player();
    const auto error = static_cast<std::int16_t>(presenterTargetHeading(player.position,target.position) - player.yaw);
    const auto braking = static_cast<int>(error) - 4 * player.yawVelocity;
    const auto distance = approximatePlanarDistance(player.position,target.position);
    GameplayInput input{};
    input.left = braking > 120;
    input.right = braking < -120;
    input.fire = combat && std::abs(static_cast<int>(error)) < 1500;
    input.forward = (combat ? distance > 6500 : true) && std::abs(static_cast<int>(error)) < 4000;
    input.reverse = combat && distance < 3500;
    return input;
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



GameplayInput combatControls(const GameplayModel& model) {
    const auto reward = find(model,25,25);
    if (reward != noObject) {
        if (model.campaignLevel() == 2) {
            // The guardian's surviving minions still own contact damage.
            const auto minion = find(model,47,47);
            if (minion != noObject) {
                return test::campaignPilotControls(model,model.level2Entities()[minion],test::CampaignPilotGoal::combat);
            }
        }
        if (model.campaignLevel() == 1) {
            auto input = approach(model,model.level2Entities()[reward],test::CampaignPilotGoal::collect);
            input.fire = true;
            return input;
        }
        return test::campaignPilotControls(model,model.level2Entities()[reward],test::CampaignPilotGoal::collect);
    }
    const auto guardian = find(model,44,45);
    if (guardian != noObject) {
        return model.campaignLevel() == 2 ? a50eWindowControls(model,model.level2Entities()[guardian])
            : approach(model,model.level2Entities()[guardian],test::CampaignPilotGoal::combat);
    }
    const Level1Entity* nearest = nullptr;
    auto distance = std::uint32_t{0xffffffffU};
    for (const auto& actor : model.level2Entities()) {
        if (!actor.active() || actor.stateBytes4dTo52[0] != 255 || actor.type < 1 || actor.type > 16 || actor.type == 7) {
            continue;
        }
        const auto candidate = approximatePlanarDistance(model.level2Player().position,actor.position);
        if (candidate < distance) {
            nearest = &actor;
            distance = candidate;
        }
    }
    if (!nearest) {
        return {};
    }
    if (model.campaignLevel() == 2) {
        return test::campaignPilotControls(model,*nearest,test::CampaignPilotGoal::combat);
    }
    auto input = approach(model,*nearest,test::CampaignPilotGoal::combat);
    const auto error = static_cast<std::int16_t>(presenterTargetHeading(model.level2Player().position,
        nearest->position) - model.level2Player().yaw);
    const auto moving = std::abs(static_cast<int>(error)) < 10000;
    input.forward = moving && distance >= 3500;
    input.reverse = moving && distance < 3500;
    return input;
}

void route(const std::filesystem::path& resources,const RouteScope scope) {
    const auto shared = models(resources,"COCKPIT.RES");
    const std::array<std::vector<Model3d>,2> destinations{models(resources,"LEVEL3.RES"),models(resources,"LEVEL4.RES")};
    auto session = test::makePlayableLevel2Session(resources);
    auto partitioned = test::makePlayableLevel2Session(resources);
    Composition composition;
    double maximumAdvanceMilliseconds = 0, maximumFrameMilliseconds = 0;
    auto expectedSample = session->elapsedSamples();
    auto lastCycles = session->synth().driverState().normalClockCycles;
    auto lastPhase = session->model().phase();
    unsigned modalPoll = 0, retries = 0, level4Frames = 0;
    const auto initialContinues = session->model().remainingContinues();
    bool level2Guardian = false, level2Reward = false, level3Loaded = false;
    bool level3Craft = false, level3Guardian = false, level3Emission = false, level3Reward = false;
    bool level4Loaded = false;
    bool warpSelected=false,warpPcm=false,warpCursorMoved=false,nextMusicPcm=false;
    std::optional<std::size_t> warpCursor;
    unsigned level3AudioFrames=0;
    constexpr unsigned maximumCalls = 20000;
    for (unsigned call = 0; call < maximumCalls; ++call) {
        const auto before = session->model().phase();
        const auto beforeLevel = session->model().campaignLevel();
        std::optional<StellarRng> expectedLoaderRng;
        if (before == GameplayPhase::level2SuccessReturn) {
            require(beforeLevel == 1 || beforeLevel == 2, "unexpected native next-bank loader");
            expectedLoaderRng = session->model().level2Rng();
            constructCampaignLoaderActors(campaignLevelDefinition(beforeLevel + 1,0).table,
                *expectedLoaderRng,shared,destinations.at(beforeLevel - 1));
        }
        if (before != lastPhase) {
            modalPoll = 0;
            lastPhase = before;
        }
        GameplayInput input{};
        NameEntryInput text{};
        if (before == GameplayPhase::level2 || before == GameplayPhase::level3Native
            || (before == GameplayPhase::playable && beforeLevel <= 2)) {
            input = combatControls(session->model());
            if(scope==RouteScope::level2TransitionAudio&&beforeLevel==2) {input={};}
        } else if (before == GameplayPhase::playable && beforeLevel == 3) {
            input.forward = input.left = input.fire = true;
        } else if (before == GameplayPhase::continuePrompt) {
            require(retries == 0, "native route exhausted its one legitimate retry");
            input.forward = modalPoll == 1;
            input.confirm = modalPoll == 3;
            ++modalPoll;
        } else if (before == GameplayPhase::highscoreNameEntry) {
            if (modalPoll == 0) {
                text.characters = {'T','E','S','T'};
            }
            input.confirm = modalPoll == 1;
            ++modalPoll;
        } else if (before == GameplayPhase::highscoreHall) {
            input.confirm = modalPoll == 0;
            ++modalPoll;
        }
        try {
            const auto advance = [&](NativeGameplaySession& current, const std::size_t budget) {
                const auto started = std::chrono::steady_clock::now();
                auto delivery = current.advancePlayableCampaign(input,budget,text);
                const auto milliseconds = std::chrono::duration<double,std::milli>(
                    std::chrono::steady_clock::now() - started).count();
                maximumAdvanceMilliseconds = std::max(maximumAdvanceMilliseconds,milliseconds);
                if (before == GameplayPhase::level2 || before == GameplayPhase::level3Native || before == GameplayPhase::playable) {
                    maximumFrameMilliseconds = std::max(maximumFrameMilliseconds,milliseconds);
                }
                return delivery;
            };
            const auto delivery = advance(*session,10000);
            const auto partitionedDelivery = advance(*partitioned,257);
            const auto boundary = call == 0 || before != session->model().phase()
                || beforeLevel != session->model().campaignLevel() || beforeLevel == 3;
            composition.compare(*session,*partitioned,delivery,partitionedDelivery,boundary);
            require(delivery.firstSample == expectedSample, "native route reset its retained PCM epoch");
            expectedSample += delivery.samples.size();
            require(session->elapsedSamples() == expectedSample, "native route lost a PCM prefix");
            const auto cycles = session->synth().driverState().normalClockCycles;
            require(cycles >= lastCycles, "native route reset its retained work clock");
            lastCycles = cycles;
            for (const auto& presentation : delivery.presentations) {
                require(presentation.sampleEnd >= delivery.firstSample && presentation.sampleEnd <= expectedSample,
                    "native presentation escaped its retained PCM prefix");
            }
            const auto& model = session->model();
            if(beforeLevel==1&&(before==GameplayPhase::level2SuccessPresentation
                ||model.phase()==GameplayPhase::level2SuccessPresentation)) {
                const auto driver=session->synth().driverState();
                require(driver.sequenceAllocated,"Level2 warp has no success sequence allocation");
                require(session->synth().sampledVoiceObservation().phase==SampledVoicePhase::unloaded,
                    "Level2 warp unexpectedly loaded narration");
                warpSelected=warpSelected||driver.sequenceSelected;
                warpPcm=warpPcm||std::any_of(delivery.samples.begin(),delivery.samples.end(),[](auto sample) {return sample!=0;});
                if(warpCursor) {warpCursorMoved=warpCursorMoved||*warpCursor!=driver.sequencePosition;}
                else {warpCursor=driver.sequencePosition;}
            }
            if(beforeLevel==2&&model.campaignLevel()==2&&model.phase()==GameplayPhase::level3Native) {
                ++level3AudioFrames;
                const auto driver=session->synth().driverState();
                require(driver.sequenceAllocated&&driver.sequenceSelected,"Level3 lost next-bank music publication");
                require(!session->storyScene(),"Level2 to3 invented a story scene absent from4E06");
                nextMusicPcm=nextMusicPcm||std::any_of(delivery.samples.begin(),delivery.samples.end(),[](auto sample) {return sample!=0;});
            }
            if (expectedLoaderRng) {
                require(model.campaignLevel() == beforeLevel + 1, "native success loaded an unexpected bank");
                auto actualRng = model.level2Rng();
                require(actualRng.calls() == expectedLoaderRng->calls(), "native next-bank constructor reset RNG calls");
                for (std::size_t index = 0; index < StellarRng::wordCount; ++index) {
                    require(actualRng.next() == expectedLoaderRng->next(), "native next-bank constructor reset RNG words");
                }
            }
            require(session->playableCampaign() && session->recoveredClockPhase()->playableCampaign,
                "native route lost explicit playable ownership");
            require(model.campaignLevel() >= beforeLevel,
                "native route moved backwards between resource banks");
            if (model.remainingContinues() < initialContinues) {
                require(model.remainingContinues() + 1 == initialContinues, "native route consumed multiple retries");
                retries = 1;
            }
            if (model.campaignLevel() == 1) {
                level2Guardian = level2Guardian || find(model,44,45) != noObject;
                level2Reward = level2Reward || find(model,25,25) != noObject;
            } else if (model.campaignLevel() == 2) {
                level3Loaded = level3Loaded || model.phase() == GameplayPhase::level3LoaderReturn;
                level3Craft = level3Craft || find(model,7,7) != noObject;
                level3Guardian = level3Guardian || find(model,44,44) != noObject;
                level3Emission = level3Emission || find(model,47,47) != noObject;
                level3Reward = level3Reward || find(model,25,25) != noObject;
            } else if (model.campaignLevel() == 3) {
                level4Loaded = level4Loaded || model.phase() == GameplayPhase::playableLoaderReturn;
                if (before == GameplayPhase::playable && model.phase() == GameplayPhase::playable) {
                    ++level4Frames;
                }
            }
            if (level4Frames == 3) {
                require(level2Guardian && level2Reward && level3Loaded && level3Craft && level3Guardian
                    && level3Emission && level3Reward && level4Loaded,
                    "native route skipped a constructor/guardian/reward/load boundary");
                std::cout << "Controlled pre-Level2 ancestry -> actual Level2 wave/guardian/green -> native Level3 wave/A50E/green"
                    << " -> Level4+3, synchronized budgets10000/257 in " << call + 1 << " calls; retries=" << retries
                    << " armor=" << model.playerArmor() << " samples=" << expectedSample
                    << " normal_cycles=" << lastCycles << " max_advance_ms=" << maximumAdvanceMilliseconds
                    << " max_frame_ms=" << maximumFrameMilliseconds << '\n';
                return;
            }
            if(scope==RouteScope::level2TransitionAudio&&level3AudioFrames==8) {
                require(level2Guardian&&level2Reward&&level3Loaded,"audio pass bypassed guardian/reward/loader");
                require(warpSelected&&warpCursorMoved&&warpPcm,"Level2 warp music was absent or silent");
                require(nextMusicPcm,"Level3 next-bank music remained silent");
                std::cout<<"Level2 guardian/green warp: selected success music, advancing sequence, nonzero PCM;"
                    <<" no invented narration; Level3 music+8 retained frames, budgets10000/257 PASS\n";
                return;
            }
            require(model.phase() != GameplayPhase::campaignEnded && model.phase() != GameplayPhase::nativeMenu,
                "native route ended after a tactical loss");
            if (call % 500 == 0) {
                std::cerr << "native continuity call=" << call << " level=" << model.campaignLevel() + 1
                    << " phase=" << static_cast<int>(model.phase()) << " armor=" << model.playerArmor()
                    << " rng=" << model.level2Rng().calls() << '\n';
            }
        } catch (...) {
            std::cerr << "native continuity call=" << call << " beforePhase=" << static_cast<int>(before)
                << " level=" << beforeLevel + 1 << " armor=" << session->model().playerArmor() << '\n';
            std::cerr << test::playableSuccessDiagnostic(session->model()) << '\n';
            for (const auto& actor : session->model().level2PresentationEntities()) {
                std::cerr << "actor=" << actor.type << " category=" << unsigned(actor.stateBytes4dTo52[0])
                    << " hp/frame=" << unsigned(actor.stateBytes4dTo52[1]) << " terminal=" << unsigned(actor.stateBytes4dTo52[4])
                    << " flags=" << unsigned(actor.flags) << " profile=" << static_cast<int>(actor.renderProfile)
                    << " pose=" << actor.position.x << ',' << actor.position.y << ',' << actor.position.z
                    << " radius=" << actor.collisionRadius << '\n';
            }
            throw;
        }
    }
    throw std::runtime_error("native controlled Level2 route did not reach Level4+3");
}
}

int main(int argc, char** argv) {
    if (argc != 2 && (argc!=3||std::string(argv[2])!="--audio-pass")) {
        std::cerr << "expected original resource directory\n";
        return 2;
    }
    try {
        route(argv[1],argc==3 ? RouteScope::level2TransitionAudio : RouteScope::throughLevel4);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

