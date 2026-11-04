#include "stellar7/failure_gameplay_work.hpp"
#include "stellar7/world_traversal.hpp"
#include "../src/retained_failure_scene.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <iostream>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void consumedTail(Level1State& state) {
        state.loopTick_ = 7;
        state.hasAdvanced_ = true;
        state.missionActive_ = false;
        state.failureState_ = true;
        state.failurePending_ = false;
        state.failureTailConsumed_ = true;
        state.playerArmor_ = -32;
        state.player_.position = {7, -5, 0};
        state.player_.xVelocity = 11;
        state.player_.forwardVelocity = 20;
        state.player_.yawVelocity = -80;
        state.objectListOrder_.clear();
        for (std::size_t index = 0; index < state.entities_.size(); ++index) {
            state.objectListOrder_.push_back({Level1State::ObjectKind::entity, index});
        }
        auto& old = state.projectiles_[1];
        old.type = 17; old.collisionRadius = 441; old.position = {200, 300, 0};
        old.stateBytes4dTo54[1] = 54; old.presenterDistance = 1234; old.targetHeading = 4321;
        state.allocationList_.push_back({Level1State::ObjectKind::projectile, 1});
        state.objectListOrder_.push_back({Level1State::ObjectKind::projectile, 1});
        auto& tail = state.projectiles_[0];
        tail.type = 23; tail.collisionRadius = 134; tail.position = state.player_.position;
        tail.stateBytes4dTo54[1] = 64;
        state.allocationList_.push_back({Level1State::ObjectKind::projectile, 0});
    }
    static void retainedFailure(Level1State& state) {
        consumedTail(state);state.loopTick_=3664;state.playerArmor_=-40;
        state.player_.position={};state.player_.yaw=1008;state.player_.xVelocity=0;
        state.player_.yVelocity=0;state.player_.zVelocity=0;state.player_.forwardVelocity=0;state.player_.yawVelocity=0;
        state.entities_[0].stateBytes4dTo52[1]=0;
        auto& guardian=state.entities_[1];guardian.type=44;guardian.collisionRadius=2792;guardian.flags=0;
        guardian.position={-2091,23390,0};guardian.pitch=guardian.roll=0;guardian.yaw=33648;
        guardian.pitchVelocity=guardian.rollVelocity=guardian.yawVelocity=0;guardian.forwardVelocity=0;
        guardian.xVelocity=guardian.yVelocity=guardian.zVelocity=0;guardian.weaponSlotCount=0;guardian.weaponCooldown=0;guardian.weaponSlotTimers={};
        guardian.stateBytes4dTo52={255,9,2,1,5,0};guardian.modelAnimationVariant=1;
        guardian.presentationModelAnimationVariant=1;guardian.primaryType11AnimationSelector=false;
        guardian.animatedModelPresenterEnabled=false;guardian.renderProfile=WorldActorRenderProfile::heapSelectorNoCallback;
        guardian.presenterDistance=24174;guardian.targetHeading=33680;
        state.entities_[2].position={427,-24057,0};state.entities_[3].position={427,8711,0};
        state.entities_[4].position={-32341,-24057,0};state.entities_[5].position={-32341,8711,0};
        state.entities_[2].stateBytes4dTo52[1]=99;state.entities_[3].stateBytes4dTo52[1]=100;
        state.entities_[4].stateBytes4dTo52[1]=99;state.entities_[5].stateBytes4dTo52[1]=14;
        for(auto& shot:state.projectiles_){shot.stateBytes4dTo54[1]=0;}
        for(auto& effect:state.deathEffects_){effect.entity.stateBytes4dTo52[1]=0;effect.listed=false;}
        state.allocationList_.clear();state.objectListOrder_.clear();
        for(const auto index:{5U,2U,3U,4U,1U}) {
            state.allocationList_.push_back({Level1State::ObjectKind::entity,index});
            state.objectListOrder_.push_back({Level1State::ObjectKind::entity,index});
        }
        state.projectionListOrigin_={};state.nativeInitialListPending_=true;
    }

};
struct GameplayModelTestAccess {
    static void consumedTail(GameplayModel& model) {
        Level1ReloadTestAccess::consumedTail(model.level1_);
        model.phase_ = GameplayPhase::failurePreExplosionBlackout;
        model.level1FailureTailFinished_ = true;
        model.failureFromLevel2_ = false;
        model.failureSourceLoopTick_ = model.level1_.loopTick();
        model.ticks_ = 7;
    }
    static void retainedFailure(GameplayModel& model) {
        consumedTail(model);Level1ReloadTestAccess::retainedFailure(model.level1_);
        model.failureSourceLoopTick_=3664;model.ticks_=3664;
    }
    static const FailureControlState& control(const GameplayModel& model) { return *model.recoveredFailureControl_; }
    static std::uint16_t rawLoop(const GameplayModel& model) { return *model.failureSourceLoopTick_; }
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
    GameplayAssets result;
    result.sharedModelRegistry = {256, 256};
    result.models.resize(41);
    constexpr std::array<std::int16_t, 10> types{0, 1, 11, 23, 29, 36, 37, 38, 39, 40};
    constexpr std::array<std::uint32_t, 10> radii{723, 1342, 748, 134, 862, 171, 107, 88, 107, 157};
    for (std::size_t index = 0; index < types.size(); ++index) {
        auto& model = result.models[types[index]];
        model.collisionRadius = static_cast<std::uint16_t>(radii[index]);
        ModelLod lod{}; lod.threshold = 255; lod.flags = 128;
        ModelGroup group{}; group.animationOffset = -1; group.vertexBlock = static_cast<std::size_t>(types[index]) + 1;
        group.vertices = {{{-16, 0, -16}}, {{16, 0, -16}}, {{0, 0, 16}}};
        ModelPrimitive primitive{}; primitive.shapes.push_back({0x80, {72, 72, 0, 0}, 255, {0, 1, 2}});
        group.primitives.push_back(primitive); lod.groups.push_back(group); model.lods.push_back(lod);
    }
    result.models[11].flags = 1;
    result.models[11].animationLimits = {8};
    result.modelColorTables.resize(2);
    for (unsigned index = 0; index < 256; ++index) { result.modelColorTables[0][index] = static_cast<std::uint8_t>(index); }
    for (unsigned index = 0; index < 4; ++index) { result.horizonFrames.push_back({256, 16, std::vector<std::uint8_t>(256 * 16, 1)}); }
    result.horizonFrames.push_back({32, 27, std::vector<std::uint8_t>(32 * 27)});
    result.artGroups.resize(6); result.artGroups[5].resize(17);
    constexpr std::array<std::array<std::uint16_t, 2>, 3> sizes{{{88, 63}, {96, 68}, {56, 46}}};
    for (std::size_t index = 0; index < sizes.size(); ++index) {
        result.artGroups[5][14 + index] = {sizes[index][0], sizes[index][1],
            std::vector<std::uint8_t>(std::size_t{sizes[index][0]} * sizes[index][1], 1)};
    }
    return result;
}
const Level1Entity& lift(const std::vector<Level1Entity>& entities) {
    return *std::find_if(entities.begin(), entities.end(), [](const auto& actor) { return actor.type == 11; });
}
std::unique_ptr<GameplayModel> prepared() {
    auto model = std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    GameplayModelTestAccess::consumedTail(*model);
    return model;
}
void retainedFailureLeaves() {
    auto fixture=assets();fixture.models.resize(45);auto& guardianModel=fixture.models[44];
    guardianModel.flags=3;guardianModel.collisionRadius=2792;guardianModel.animationLimits={8,8,8};
    const auto view=failureViewSetupOperations({{1791,173,0},{},{0,0,17392},0x88,7,{0,318,0,199},{256,-256,362}});
    const auto& model=fixture.models[29];
    for(const auto raw:std::array<FixedPosition,2>{{{-32341,-24057,0},{-32341,8711,0}}}) {
        const auto relative=FixedPosition{raw.x-view.view.origin.x,raw.y-view.view.origin.y,0};
        const auto center=transformToView(raw,view.view);const WorldModelViewInput modelView{view.view.matrix,relative};
        const auto cull=failureFixedWideCullOperations(center,model);
        assert(!cull.culled && !cull.clipped && cull.projectedRadius==3);
        rejects([&]{failureModelCullOperations(center,862,{0,318,0,199});});
        rejects([&]{failureModelSetupOperations(center,{0,0,0},3,29,model,256,256,modelView);});
        const auto setup=failureFixedWideSetupOperations(center,{0,0,0},3,model,256,256,modelView);
        assert(setup.vertexPrecision==1 && setup.translation.x==-8533);
        assert(setup.translation.y==(raw.y<0 ? -6058 : 2134));
        auto input=worldModelGeometryInput(fixture,setup,center,PolygonClipMode::Disabled);input.screenCenter={159,99};
        const auto geometry=failureFixedWideGeometryOperations(model.lods[0],input,modelView);
        assert(geometry.polygons.size()==1 && !geometry.polygons[0].points.empty());
        auto bad=input;bad.screenCenter.y=84;
        rejects([&]{failureFixedWideGeometryOperations(model.lods[0],bad,modelView);});
        bad=input;bad.clipping=PolygonClipMode::Enabled;
        rejects([&]{failureFixedWideGeometryOperations(model.lods[0],bad,modelView);});
        bad=input;bad.translation.y*=2;
        rejects([&]{failureFixedWideGeometryOperations(model.lods[0],bad,modelView);});
        bad=input;bad.animationSelectors={0};
        rejects([&]{failureFixedWideGeometryOperations(model.lods[0],bad,modelView);});
        WorldSceneActor actor{{0,raw},{0,0,0},29,862,0,0};
        rejects([&]{failureModelSceneOperations(fixture,view.view,{{4,actor}},{256,256,0,0},{256,-256,362});});
    }
    const FixedPosition clipped{40000,33000,0};const WorldModelViewInput flat{makeFixedMatrixQ14(0,0,0),clipped};
    const auto clipCull=failureFixedWideCullOperations(clipped,model);assert(!clipCull.culled && clipCull.clipped);
    rejects([&]{failureFixedWideSetupOperations(clipped,{0,0,0},clipCull.projectedRadius,model,256,256,flat);});
    for(const auto depth:{32500,63000}) {
        const FixedPosition center{0,depth,0};const WorldModelViewInput identity{makeFixedMatrixQ14(0,0,0),center};
        const auto cull=failureFixedWideCullOperations(center,model);
        const auto setup=failureFixedWideSetupOperations(center,{0,0,0},cull.projectedRadius,model,256,256,identity);
        auto input=worldModelGeometryInput(fixture,setup,center,PolygonClipMode::Disabled);input.screenCenter={159,99};
        auto invalid=model.lods[0];invalid.groups[0].vertices[0][1]=depth==32500 ? -32768 : 3000;
        rejects([&]{failureFixedWideGeometryOperations(invalid,input,identity);});
    }
    WorldSceneActor guardian{{0,{-2091,23390,0}},{0,0,33648},44,2792,0,0};
    guardian.heapAnimation=Level1AnimationState{2,1,1};
    const std::vector<FailureSceneItem> items{FailureModelSceneItem{1,guardian}};
    const auto inactive=retainedFailureSceneOperations(fixture,view.view,items,{256,256,0,0},{256,-256,362},0xa000,{1});
    assert(inactive.scene.culled[0] && !inactive.observations[0].animation);
    std::vector<NormalWorkOperation> expected;
    const auto append=[&](const auto& work){expected.insert(expected.end(),work.begin(),work.end());};
    const auto center=failureFirstCenterOperations(view.view,guardian.actor);append(center.operations);
    append(worldModelCallerOperations(center.center,2792,0,0).operations);
    append(failureModelCullOperations(center.center,2792,{0,318,0,199}).operations);
    append(worldModelReturnOperations(WorldActorContinuation::returnToCaller));
    assert(expected.size()==inactive.scene.operations.size());
    for(std::size_t i=0;i<expected.size();++i) {
        assert(expected[i].kind==inactive.scene.operations[i].kind && expected[i].count==inactive.scene.operations[i].count);
    }
    rejects([&]{failureModelSceneOperations(fixture,view.view,{{1,guardian}},{256,256,0,0},{256,-256,362});});
    auto noHeap=guardian;noHeap.heapAnimation.reset();
    rejects([&]{retainedFailureSceneOperations(fixture,view.view,{FailureModelSceneItem{1,noHeap}},
        {256,256,0,0},{256,-256,362},0xa000,{1});});
    guardian.drawCallback=0x130c;
    const auto enabled=retainedFailureSceneOperations(fixture,view.view,{FailureModelSceneItem{1,guardian}},
        {256,256,0,0},{256,-256,362},0xa000,{1});
    assert(enabled.observations[0].animation && enabled.observations[0].animation->countdown==0);
}
void retainedFailureRegression() {
    //Controlled consumed-tail roster; natural ancestry is established only by root replay.
    auto fixture=assets();fixture.models.resize(45);fixture.models[44].flags=3;
    fixture.models[44].collisionRadius=2792;fixture.models[44].animationLimits={8,8,8};
    auto model=std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    GameplayModelTestAccess::retainedFailure(*model);
    const auto incoming=model->level1State().entities()[1];const auto initialRng=model->level1State().rng().calls();
    for(unsigned display=0;display<GameplayModel::failureExplosionFrames;++display) {
        const auto work=model->advanceRecoveredFailureStep(fixture,(display&1U) ? 0xa000 : 0xa800,
            display==0 ? Viewport{24,294,24,145} : Viewport{0,318,0,199});
        assert(work.kind==FailureGameplayStepKind::display && work.scene && work.view);
        assert(work.view->view.origin==FixedPosition({1791,173,0}));
        assert(work.control.drawOrder.front()==FailureObjectRef({FailureObjectKind::mission,4}));
        assert(work.scene->scene.centers.front()==FixedPosition({-20823,36309,0}));
        assert(!work.scene->scene.culled.front() && work.scene->scene.polygonCounts.front()==1);
        const auto& guardian=model->level1State().entities()[1];
        assert(guardian.position==incoming.position && guardian.yaw==incoming.yaw);
        assert(guardian.stateBytes4dTo52==incoming.stateBytes4dTo52);
        assert(guardian.modelAnimationVariant==incoming.modelAnimationVariant
            && guardian.presentationModelAnimationVariant==incoming.presentationModelAnimationVariant);
        assert(guardian.presenterDistance==incoming.presenterDistance && guardian.targetHeading==incoming.targetHeading);
        assert(!guardian.animatedModelPresenterEnabled && guardian.renderProfile==WorldActorRenderProfile::heapSelectorNoCallback);
        for(std::size_t i=0;i<work.control.drawOrder.size();++i) {
            if(work.control.drawOrder[i]==FailureObjectRef({FailureObjectKind::mission,1})) {
                assert(work.scene->scene.culled[i] && !work.scene->observations[i].animation);
            }
        }
    }
    assert(model->failureRng().calls()==initialRng+264 && model->failureAudioEvents().size()==8);
    const auto leave=model->advanceRecoveredFailureStep(fixture,0xa000,{0,318,0,199});
    assert(leave.kind==FailureGameplayStepKind::leave && model->phase()==GameplayPhase::failurePostExplosionBlackout);
}

}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    const auto fixture = assets();
    auto ordinary = std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    const auto ordinaryCalls = ordinary->level1State().rng().calls();
    rejects([&] { ordinary->advanceRecoveredFailureStep(fixture, 0xa800, {24, 294, 24, 145}); });
    assert(ordinary->phase() == GameplayPhase::level1 && ordinary->level1State().rng().calls() == ordinaryCalls);
    auto invalid = prepared();
    const auto invalidTick = invalid->ticks();
    rejects([&] { invalid->advanceRecoveredFailureStep(fixture, 0xa400, {0, 319, 0, 199}); });
    auto missing = fixture; missing.horizonFrames.clear();
    rejects([&] { invalid->advanceRecoveredFailureStep(missing, 0xa800, {0, 319, 0, 199}); });
    assert(invalid->ticks() == invalidTick && invalid->phase() == GameplayPhase::failurePreExplosionBlackout);
    assert(invalid->failureDebris().empty() && invalid->level1State().rng().calls() == ordinaryCalls);

    auto model = prepared();
    const auto initialRng = model->level1State().rng().calls();
    const auto continues = model->remainingContinues();
    bool reusedEffectCache = false;
    bool renderedTailShot = false;
    for (unsigned display = 0; display < GameplayModel::failureExplosionFrames; ++display) {
        const auto incomingEffects = model->level1State().deathEffects();
        const auto work = model->advanceRecoveredFailureStep(fixture, (display & 1U) ? 0xa000 : 0xa800,
            display == 0 ? Viewport{24, 294, 24, 145} : Viewport{0, 318, 0, 199});
        assert(work.kind == FailureGameplayStepKind::display && bool(work.entry) == (display == 0));
        assert(work.horizon && work.view && work.scene && !work.sceneOperations.empty());
        assert(work.sceneOperations.front().kind == NormalWorkKind::instructions && work.sceneOperations.front().count == 8);
        assert(work.horizon->viewport == (display == 0 ? Viewport{0, 319, 0, 199} : Viewport{0, 318, 0, 199}));
        assert(model->ticks() == 8 + display && model->presentationFrame() == display);
        assert(GameplayModelTestAccess::rawLoop(*model) == 8 + display && model->level1State().loopTick() == 7);
        const auto& before = lift(work.control.expected.mission);
        const auto after = level1AnimationOperations({before.stateBytes4dTo52[2], before.stateBytes4dTo52[3],
            before.modelAnimationVariant}, fixture.models[11]).state;
        const auto& committed = lift(model->level1State().entities());
        assert(committed.stateBytes4dTo52[2] == after.reload && committed.stateBytes4dTo52[3] == after.countdown);
        assert(committed.modelAnimationVariant == after.variant && committed.presentationModelAnimationVariant == before.modelAnimationVariant);
        for (std::size_t slot = 0; slot < incomingEffects.size(); ++slot) {
            if (!incomingEffects[slot].entity.active() && work.control.expected.effects[slot].entity.active()) {
                assert(work.control.expected.effects[slot].projected == incomingEffects[slot].projected);
                reusedEffectCache = reusedEffectCache || !(incomingEffects[slot].projected == ProjectedPoint{});
            }
        }
        for (std::size_t ordinal = 0; ordinal < work.control.drawOrder.size(); ++ordinal) {
            const auto object = work.control.drawOrder[ordinal];
            const auto& observation = work.scene->observations[ordinal];
            if (object.kind == FailureObjectKind::projectile && object.index == 0) { renderedTailShot = true; }
            if (!observation.effect) { continue; }
            const auto& effect = model->level1State().deathEffects()[object.index];
            assert(effect.projected == observation.effect->state.projected);
            assert(effect.entity.stateBytes4dTo52[1] == observation.effect->state.age);
            assert(effect.entity.flags == observation.effect->state.flags);
        }
        if (display == 0) {
            assert(work.entry->expected.player.xVelocity == 11 && work.entry->expected.player.forwardVelocity == 0);
            assert(work.entry->expected.camera.position == (FixedPosition{1811, 0, 0}));
            assert(work.control.expected.debris.size() == 30 && model->failureDebris().size() == 30);
            assert(model->failureRng().calls() == initialRng + 243);
            assert(!model->level1State().projectiles()[1].active());
            assert(model->level1State().projectiles()[1].presenterDistance == 1234);
            assert(model->level1State().projectiles()[1].targetHeading == 4321);
            assert(model->level1State().projectiles()[0].active());
            assert(model->level1State().projectiles()[0].position == (FixedPosition{7, -5, 0}));
            assert(before.stateBytes4dTo52[3] == 0 && committed.stateBytes4dTo52[3] == 5);
        }
    }
    assert(renderedTailShot && reusedEffectCache);
    assert(model->failureAudioEvents().size() == 8 && model->failureBursts().size() == 8);
    for (std::size_t event = 0; event < 8; ++event) {
        assert(model->failureAudioEvents()[event].eventId == 6 && model->failureAudioEvents()[event].loopTick == 8 + event);
    }
    assert(model->failureRng().calls() == initialRng + 264);
    assert(GameplayModelTestAccess::control(*model).allocationOrder.size() == 37);
    const auto left = model->advanceRecoveredFailureStep(fixture, 0xa000, {0, 318, 0, 199});
    assert(left.kind == FailureGameplayStepKind::leave && !left.entry && !left.horizon && !left.view && !left.scene);
    assert(left.sceneOperations.empty() && left.control.control.finished);
    assert(model->phase() == GameplayPhase::failurePostExplosionBlackout && model->failureDebris().empty());
    assert(model->ticks() == 53 && GameplayModelTestAccess::rawLoop(*model) == 52);
    assert(model->remainingContinues() == continues && model->failureAudioEvents().size() == 8);
    rejects([&] { model->advanceRecoveredFailureStep(fixture, 0xa800, {0, 318, 0, 199}); });
    assert(model->ticks() == 53);
    try {retainedFailureRegression();retainedFailureLeaves();} catch(const std::exception& error) {
        std::cerr << "retained failure consumed-tail scene: " << error.what() << '\n';return 1;
    }
    return 0;
}
