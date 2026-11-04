#include "stellar7/frame_controller.hpp"
#include "stellar7/level1_frame.hpp"
#include "stellar7/level1_loader_work.hpp"

#include <cassert>
#include <memory>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void cullModels(Level1State& state) {
        for (auto& actor : state.entities_) { actor.position.y = -16384; }
    }
};
struct GameplayModelTestAccess {
    static void postBlackout(GameplayModel& model) {
        model.phase_ = GameplayPhase::failurePostExplosionBlackout;
        model.failureFromLevel2_ = false;
        model.failureRng_ = StellarRng{};
        model.ticks_ = 73;
    }
    static std::uint64_t tickBase(const GameplayModel& model) { return model.recoveredLevel1TickBase_; }
    static void shiftBase(GameplayModel& model, const std::uint64_t base) { model.recoveredLevel1TickBase_ = base; }
    static void cullModels(GameplayModel& model) { Level1ReloadTestAccess::cullModels(model.level1_); }
    static void lamp(GameplayModel& model, const bool visible, const bool ready) {
        model.weaponIndicatorVisible_ = visible;
        model.weaponReadyIndicator_ = ready;
    }
};
}

namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

GameplayAssets assets() {
    //Geometry-free composition profile from level1_frame_test; actual source assets are integrated separately.
    GameplayAssets result;
    result.cockpit = {std::vector<std::uint8_t>(320 * 200,93),std::vector<Rgb>(256)};
    result.cockpitSettings = CockpitSettings{90,{}};
    result.scoreFont = {32,8,0,std::vector<IndexedBitmap>(96,{4,8,std::vector<std::uint8_t>(32,1)})};
    result.horizonFrames.assign(4,{256,16,std::vector<std::uint8_t>(256 * 16,7)});
    result.artGroups.resize(5);
    result.artGroups[3] = {{120,56,std::vector<std::uint8_t>(120 * 56,7)},
        {4,1,{0,0,0,0}},{120,18,std::vector<std::uint8_t>(120 * 18,3)}};
    result.artGroups[4].assign(15,{4,1,{0,0,0,0}});
    result.artGroups[4][6].pixels.back() = 9;
    for (std::size_t index = 7; index < 15; ++index) {
        result.artGroups[4][index] = {8,1,std::vector<std::uint8_t>(8,13)};
    }
    result.models.resize(30);
    result.models[1].collisionRadius = 1342;
    result.models[11].collisionRadius = 748;
    result.models[11].flags = 1;
    result.models[11].animationLimits = {8};
    result.models[29].collisionRadius = 862;
    result.sharedModelRegistry = {256,256};
    return result;
}

enum class LampHistory { hidden, unavailable, ready };

std::unique_ptr<GameplayModel> retry(const LampHistory lamp = LampHistory::hidden) {
    auto model = std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    GameplayModelTestAccess::postBlackout(*model);
    model->enterRecoveredContinue();
    model->pollRecoveredContinue({});
    GameplayInput up; up.forward = true;
    model->pollRecoveredContinue(up);
    GameplayInput confirm; confirm.confirm = true;
    model->pollRecoveredContinue(confirm);
    assert(model->pollRecoveredContinue({}) == ContinueDecision::yes);
    const bool lampVisible = lamp != LampHistory::hidden;
    const bool lampReady = lamp == LampHistory::ready;
    GameplayModelTestAccess::lamp(*model,lampVisible,lampReady);
    model->resumeRecoveredContinueRetry();
    assert(model->weaponIndicatorVisible() == lampVisible && model->weaponReadyIndicator() == lampReady);
    return model;
}

void assertOperationsEqual(const std::vector<NormalWorkOperation>& first,
    const std::vector<NormalWorkOperation>& second) {
    assert(first.size() == second.size());
    for (std::size_t index = 0; index < first.size(); ++index) {
        assert(first[index].kind == second[index].kind && first[index].count == second[index].count
            && first[index].value == second[index].value);
    }
}

void loaderAndEpoch() {
    const auto data = assets();
    const auto checkEffectPool = [](const RadarState& radar) {
        constexpr std::size_t effectAllocationBase = 32;
        for (std::size_t slot = 0; slot < Level1State::deathEffectPoolSize; ++slot) {
            const auto& projection = radar.projection(effectAllocationBase + slot);
            assert(projection.center.x == 0 && projection.center.y == 0 && projection.center.z == 0);
            assert(projection.cachedCenter.x == 0 && projection.cachedCenter.y == 0 && projection.cachedCenter.z == 0);
            assert(projection.projectedX == 0 && projection.projectedY == 0 && projection.projectedRadius == 0);
            assert(!projection.projected);
        }
    };
    auto fresh = std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    auto model = retry();
    assert(model->ticks() == 73 && GameplayModelTestAccess::tickBase(*model) == 73);
    assert(model->phase() == GameplayPhase::level1RetryHandoff && model->level1State().loopTick() == 0);
    const auto calls = model->level1State().rng().calls();
    assert(calls == 16 && model->lastCockpitInitialization().displaySteps == 2);
    auto pages = CockpitPageState::fromRecoveredMenu();
    auto swapped = pages; swapped.flip();
    IndexedPages pixels;
    const auto originalTicks = model->ticks();
    const auto first = planLevel1LoaderPages(data,*model,pages,pixels,Level1LoaderPageDomain::recoveredRetry);
    const auto other = planLevel1LoaderPages(data,*model,swapped,pixels,Level1LoaderPageDomain::recoveredRetry);
    checkEffectPool(first.radar);
    checkEffectPool(other.radar);
    assertOperationsEqual(first.operations,other.operations);
    assert(first.pages.frontPage() == 0xa000 && first.pages.backPage() == 0xa800);
    assert(other.pages.frontPage() == 0xa800 && other.pages.backPage() == 0xa000);
    assert(first.pages.cachePage() == 0xa400 && other.pages.cachePage() == 0xa400);
    for (std::int16_t y = 0; y < 200; ++y) {
        for (std::int16_t x = 0; x < 320; ++x) {
            const ProjectedPoint point{x,y};
            assert(first.pixels.read(first.pages.frontPage(),point) == other.pixels.read(other.pages.frontPage(),point));
            assert(first.pixels.read(first.pages.backPage(),point) == other.pixels.read(other.pages.backPage(),point));
            assert(first.pixels.read(0xa400,point) == other.pixels.read(0xa400,point));
        }
    }
    assert(model->ticks() == originalTicks && model->level1State().rng().calls() == calls);
    rejects([&] { planLevel1LoaderPages(data,*model,pages,pixels); });
    rejects([&] { planLevel1LoaderPages(data,*fresh,swapped,pixels); });
    rejects([&] { planLevel1LoaderPages(data,*fresh,pages,pixels,Level1LoaderPageDomain::recoveredRetry); });
    rejects([&] { planLevel1LoaderPages(data,*model,pages,pixels,static_cast<Level1LoaderPageDomain>(99)); });
    rejects([&] { planLevel1LoaderPages(data,*model,CockpitPageState{},pixels,Level1LoaderPageDomain::recoveredRetry); });
    const auto freshPlan = planLevel1LoaderPages(data,*fresh,pages,pixels);
    checkEffectPool(freshPlan.radar);
    assert(freshPlan.pages.frontPage() == 0xa000 && fresh->ticks() == 0);
    rejects([&] { fresh->completeRecoveredLevel1Retry(); });
    rejects([&] { model->advanceRecoveredLevel1Frame(data,*data.cockpitSettings,first.pages,first.radar,first.pixels,{}); });
    model->completeRecoveredLevel1Retry();
    assert(model->phase() == GameplayPhase::level1 && model->ticks() == originalTicks);
    assert(model->level1State().rng().calls() == calls && model->level1State().activeListOrder().empty());
    rejects([&] { model->completeRecoveredLevel1Retry(); });
    rejects([&] { planLevel1LoaderPages(data,*model,pages,pixels,Level1LoaderPageDomain::recoveredRetry); });
    GameplayModelTestAccess::cullModels(*model);
    GameplayModelTestAccess::shiftBase(*model,74);
    rejects([&] { model->advanceRecoveredLevel1Frame(data,*data.cockpitSettings,first.pages,first.radar,first.pixels,{}); });
    GameplayModelTestAccess::shiftBase(*model,72);
    rejects([&] { model->advanceRecoveredLevel1Frame(data,*data.cockpitSettings,first.pages,first.radar,first.pixels,{}); });
    GameplayModelTestAccess::shiftBase(*model,73);
    FrameTailGuards guards{};
    guards.keyboardHead = guards.keyboardTail = 0x1e;
    const auto frame = model->advanceRecoveredLevel1Frame(data,*data.cockpitSettings,first.pages,first.radar,first.pixels,guards);
    assert(model->ticks() == 74 && model->level1State().loopTick() == 1);
    assert(frame.tail.state.loopTick == 1 && frame.render.radarActors.size() == 6);
    assert(model->level1State().rng().calls() == 29 && GameplayModelTestAccess::tickBase(*model) == 73);
}

void controllerRestart() {
    FrameControllerInitial input{};
    input.origin = FrameControllerOrigin::sourceColdDefaults;
    input.loopTick = 77; input.keyboardHead = input.keyboardTail = 0x1e;
    for (std::size_t index = 0; index < input.keyboardSlots.size(); ++index) {
        input.keyboardSlots[index] = static_cast<std::uint16_t>(0x1200 + index);
    }
    FrameController owner;
    rejects([&] { owner.restartNativeLoop(); });
    owner.initialize(input);
    std::array<bool,9> keys{}; keys[4] = keys[6] = keys[8] = true;
    Level1FrameTailWork tail{};
    tail.state.loopTick = 78; tail.keyboardHead = 0x1e;
    owner.completeNativeTail(77,tail,keys);
    const auto keyboard = owner.keyboardState();
    const auto held = owner.augmentGuards({}).heldKeys;
    owner.restartNativeLoop();
    assert(owner.expectedLoopTick() == 0 && owner.origin() == FrameControllerOrigin::sourceColdDefaults);
    const auto after = owner.keyboardState();
    assert(after.metadata48 == keyboard.metadata48 && after.metadataAlt38 == keyboard.metadataAlt38);
    assert(after.axisRoles == keyboard.axisRoles && after.lastKey == keyboard.lastKey && after.consumedScan == keyboard.consumedScan);
    assert(after.head == keyboard.head && after.tail == keyboard.tail && after.slots == keyboard.slots);
    assert(after.ringBegin == keyboard.ringBegin && after.ringEnd == keyboard.ringEnd);
    assert(owner.augmentGuards({}).heldKeys == held && held == keys);
    rejects([&] { owner.consumeEmptyTail(78); });
    owner.consumeEmptyTail(0);
    assert(owner.expectedLoopTick() == 1 && owner.augmentGuards({}).heldKeys == held);
    owner.delivery(FrameControllerDelivery::configuration);
    rejects([&] { owner.restartNativeLoop(); });
    input.origin = FrameControllerOrigin::restoredDiagnosticInitial;
    input.keyboardHead = input.keyboardTail = 0x36;
    FrameController diagnostic; diagnostic.initialize(input);
    rejects([&] { diagnostic.restartNativeLoop(); });
    assert(diagnostic.expectedLoopTick() == 77);
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    loaderAndEpoch();
    controllerRestart();
    retry(LampHistory::unavailable);
    retry(LampHistory::ready);
    return 0;
}
