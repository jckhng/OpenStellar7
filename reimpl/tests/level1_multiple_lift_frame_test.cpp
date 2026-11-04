#include "stellar7/level1_frame.hpp"

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
    static void prepareBirth(Level1State& state) {
        for (auto& actor : state.entities_) { actor.position.y = -16384; }
        std::array<std::uint16_t,StellarRng::wordCount> table{};
        table[43] = 43525; table[42] = 3332; table[41] = 43981;
        state.rng_ = StellarRng(table,48,110,0,state.rng_.calls());
    }
    static void phases(Level1State& state) {
        assert(state.entities_.size() == 7 && state.entities_[6].type == 11);
        auto& initial = state.entities_[1];
        initial.stateBytes4dTo52[3] = 0; initial.modelAnimationVariant = 7;
        auto& spawned = state.entities_[6];
        spawned.stateBytes4dTo52[3] = 1; spawned.modelAnimationVariant = 3;
    }
};
struct GameplayModelTestAccess {
    static void prepareBirth(GameplayModel& model) { Level1ReloadTestAccess::prepareBirth(model.level1_); }
    static void phases(GameplayModel& model) { Level1ReloadTestAccess::phases(model.level1_); }
};
}

namespace {
using namespace stellar7;
const char* diagnosticPhase = "initialization";

bool sameAnimation(const Level1AnimationState a,const Level1AnimationState b) {
    return a.reload == b.reload && a.countdown == b.countdown && a.variant == b.variant;
}
Level1AnimationState animation(const Level1Entity& actor) {
    return {actor.stateBytes4dTo52[2],actor.stateBytes4dTo52[3],actor.modelAnimationVariant};
}
void setAnimation(Level1Entity& actor,const Level1AnimationState value) {
    actor.stateBytes4dTo52[2] = value.reload;
    actor.stateBytes4dTo52[3] = value.countdown;
    actor.modelAnimationVariant = value.variant;
}
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
void sameOperations(const std::vector<NormalWorkOperation>& a,const std::vector<NormalWorkOperation>& b) {
    assert(a.size() == b.size());
    for (std::size_t index = 0; index < a.size(); ++index) {
        assert(a[index].kind == b[index].kind && a[index].count == b[index].count && a[index].value == b[index].value);
    }
}
std::uint64_t instructions(const std::vector<NormalWorkOperation>& operations) {
    std::uint64_t result = 0;
    for (const auto& op : operations) {
        if (op.kind == NormalWorkKind::instructions) { result += op.count; }
    }
    return result;
}
GameplayAssets assets() {
    GameplayAssets out;
    out.horizonFrames.assign(4,{256,16,std::vector<std::uint8_t>(256*16,7)});
    out.artGroups.resize(5);
    out.artGroups[3] = {{120,56,std::vector<std::uint8_t>(120*56,7)},
        {4,1,{0,0,0,0}},{120,18,std::vector<std::uint8_t>(120*18,3)}};
    out.artGroups[4].assign(41,{4,1,{0,0,0,0}});
    out.artGroups[4][6].pixels.back() = 9;
    for (std::size_t i = 7; i < 15; ++i) {
        out.artGroups[4][i] = {8,1,std::vector<std::uint8_t>(8,13)};
    }
    out.artGroups[4][39] = out.artGroups[4][40] = {24,5,std::vector<std::uint8_t>(120,7)};
    out.models.resize(44);
    out.models[1].collisionRadius = 1342; out.models[1].flags = 1;
    out.models[3].collisionRadius = 770; out.models[3].flags = 1;
    out.models[11].collisionRadius = 748; out.models[11].flags = 1;
    out.models[11].animationLimits = {8};
    out.models[17].collisionRadius = 441;
    out.models[20].collisionRadius = out.models[23].collisionRadius = 134;
    ModelGroup shotGroup{};
    shotGroup.animationOffset = 0;
    shotGroup.vertexBlock = 23;
    shotGroup.vertices = {{{0,-2048,0}},{{-50,-100,42}},{{50,-100,0}},{{0,110,-42}}};
    ModelPrimitive shotPrimitive{};
    shotPrimitive.shapes = {{0x81,{8,8,1,1},0,{1,2,3}}};
    shotGroup.primitives = {shotPrimitive,shotPrimitive};
    out.models[23].flags = 1;
    out.models[23].lods = {{30,128,{shotGroup}}};
    out.modelColorTables.resize(2);
    for (std::size_t index = 0; index < 256; ++index) {
        out.modelColorTables[0][index] = static_cast<std::uint8_t>(index);
    }
    out.models[29].collisionRadius = 862;
    out.sharedModelRegistry = {256,256};
    return out;
}
FrameTailGuards guards() {
    FrameTailGuards result{};
    result.keyboardHead = result.keyboardTail = 0x1e;
    return result;
}
struct Environment {
    GameplayAssets data = assets();
    CockpitPageState pages = CockpitPageState::fromRecoveredMenu();
    RadarState radar{data.artGroups[3]};
    IndexedPages pixels;
    Environment() {
        for (const std::uint16_t page : {0xa000,0xa400,0xa800}) {
            for (std::int16_t y = 0; y < 200; ++y) {
                for (std::int16_t x = 0; x < 320; ++x) { pixels.write(page,{x,y},93); }
            }
        }
        for (std::size_t index = 0; index < 7; ++index) { radar.initializeAllocation(index); }
        pixels.apply(radar.initializeLevel1(680,{},0,
            {pages.cachePage(),pages.backPage(),pages.frontPage(),pages.backPage(),pages.cachePage()}));
    }
};
Level1Entity spawnedLift(const GameplayAssets& data) {
    Level1PeriodicSpawnInput input{2,2,10,6,6,{}};
    input.ordinaryPool[0] = {1,false,0}; input.ordinaryPool[1] = {1,true,1};
    std::array<std::uint16_t,StellarRng::wordCount> table{};
    table[55] = 43525; table[54] = 3332; table[53] = 43981;
    const auto birth = level1PeriodicSpawnOperations(input,StellarRng(table,48,110,0,16),data.models);
    assert(birth.spawn && birth.provider && birth.spawn->actor.type == 11);
    assert(birth.spawn->actor.allocationIndex == 6 && !birth.spawn->actor.primaryType11AnimationSelector);
    return birth.spawn->actor;
}
std::size_t ordinal(const Level1FramePlan& frame,const std::size_t allocation) {
    const auto found = std::find_if(frame.radarActors.begin(),frame.radarActors.end(),
        [allocation](const auto& actor) { return actor.allocation == allocation; });
    assert(found != frame.radarActors.end());
    return static_cast<std::size_t>(found-frame.radarActors.begin());
}
void checkPhases(const Level1FramePlan& frame,const std::size_t allocation,
    const Level1AnimationState incoming,const Model3d& model) {
    const auto index = ordinal(frame,allocation);
    const auto main = level1AnimationOperations(incoming,model);
    const auto radar = level1AnimationOperations(main.state,model);
    assert(frame.scene.animationStates[index] && frame.animationsAfterRadar[index]);
    assert(sameAnimation(*frame.scene.animationStates[index],main.state));
    assert(sameAnimation(*frame.animationsAfterRadar[index],radar.state));
}
void frameReceipts() {
    auto env = std::make_unique<Environment>();
    Level1State level; level.reloadAfterFailure(StellarRng{});
    auto actors = level.entities();
    for (auto& actor : actors) { actor.position.y = -16384; }
    auto ground = level.ground(); auto rng = level.rng();
    std::array<Level1Projectile,Level1State::projectilePoolSize> pool{};
    std::vector<Level1ObjectRef> order;
    for (std::size_t index = 0; index < actors.size(); ++index) { order.push_back({Level1PresentationKind::entity,index}); }
    Level1ProjectileRenderState input{ground,rng,level.player(),actors,pool,order,2};
    const Level1PresentationState presentation{{140,4},false};
    const CockpitSettings settings{90,{}};
    diagnosticPhase = "strict single-lift frame";
    auto single = std::unique_ptr<Level1FramePlan>(new Level1FramePlan(planLevel1CombatFrame(
        env->data,input,presentation,680,settings,env->pages,env->radar,env->pixels)));
    diagnosticPhase = "dynamic single-lift frame";
    auto dynamicSingle = std::unique_ptr<Level1FramePlan>(new Level1FramePlan(planLevel1PeriodicCombatFrame(
        env->data,input,presentation,680,settings,env->pages,env->radar,env->pixels)));
    sameOperations(single->operations,dynamicSingle->operations);
    assert(sameAnimation(single->animationAfterMain,dynamicSingle->animationAfterMain));
    assert(sameAnimation(single->animationAfterRadar,dynamicSingle->animationAfterRadar));
    assert(sameAnimation(*single->animationsAfterRadar[1],single->animationAfterRadar));
    diagnosticPhase = "source periodic constructor component";
    auto born = spawnedLift(env->data); born.position = {6000,-16384,1000};
    actors.push_back(born); order.push_back({Level1PresentationKind::entity,6});
    const Level1AnimationState initial{5,0,7}, spawned{5,1,3};
    setAnimation(actors[1],initial); setAnimation(actors[6],spawned);
    diagnosticPhase = "distinct multiple-lift frame";
    auto multiple = std::unique_ptr<Level1FramePlan>(new Level1FramePlan(planLevel1PeriodicCombatFrame(
        env->data,input,presentation,680,settings,env->pages,env->radar,env->pixels)));
    checkPhases(*multiple,1,initial,env->data.models[11]);
    checkPhases(*multiple,6,spawned,env->data.models[11]);
    assert(sameAnimation(*multiple->scene.animationStates[1],{5,5,0}));
    assert(sameAnimation(*multiple->animationsAfterRadar[1],{5,4,0}));
    assert(sameAnimation(*multiple->scene.animationStates[6],{5,0,3}));
    assert(sameAnimation(*multiple->animationsAfterRadar[6],{5,5,4}));
    assert(sameAnimation(animation(actors[1]),initial) && sameAnimation(animation(actors[6]),spawned));
    //Only the second actor's radar callback changes from expiry144 to decrement15.
    setAnimation(actors[6],{5,2,3});
    diagnosticPhase = "radar decrement control";
    auto decrement = std::unique_ptr<Level1FramePlan>(new Level1FramePlan(planLevel1PeriodicCombatFrame(
        env->data,input,presentation,680,settings,env->pages,env->radar,env->pixels)));
    sameOperations(multiple->scene.operations,decrement->scene.operations);
    assert(instructions(multiple->radar.operations) == instructions(decrement->radar.operations) + 129);
    assert(instructions(multiple->operations) == instructions(decrement->operations) + 129);
    //Ordered receipts follow the selected list, never physical allocation index.
    setAnimation(actors[6],spawned); std::swap(order[1],order[6]);
    diagnosticPhase = "reordered lift frame";
    auto reordered = std::unique_ptr<Level1FramePlan>(new Level1FramePlan(planLevel1PeriodicCombatFrame(
        env->data,input,presentation,680,settings,env->pages,env->radar,env->pixels)));
    checkPhases(*reordered,1,initial,env->data.models[11]);
    checkPhases(*reordered,6,spawned,env->data.models[11]);
    assert(sameAnimation(reordered->animationAfterMain,*reordered->scene.animationStates[1]));
    //The effect composition uses the same whole ordered callback vector.
    Level1CombatRenderContext context{order,{}};
    diagnosticPhase = "effect-composition frame";
    auto effects = std::unique_ptr<Level1CombatFramePlan>(new Level1CombatFramePlan(planLevel1EffectCombatFrame(
        env->data,input,context,presentation,680,settings,env->pages,env->radar,env->pixels)));
    sameOperations(reordered->operations,effects->frame.operations);
    checkPhases(effects->frame,1,initial,env->data.models[11]);
    checkPhases(effects->frame,6,spawned,env->data.models[11]);
    //A retired initial selector is not invoked or copied into the surviving lift.
    const auto initialCache = effects->frame.radarState.projection(1);
    actors[1].stateBytes4dTo52[1] = 0;
    order.erase(std::find(order.begin(),order.end(),Level1ObjectRef{Level1PresentationKind::entity,1}));
    diagnosticPhase = "retired initial lift frame";
    auto retired = std::unique_ptr<Level1CombatFramePlan>(new Level1CombatFramePlan(planLevel1EffectCombatFrame(
        env->data,input,context,presentation,680,settings,env->pages,effects->frame.radarState,env->pixels)));
    assert(retired->frame.radarActors.size() == 6 && retired->frame.animationsAfterRadar.size() == 6);
    checkPhases(retired->frame,6,spawned,env->data.models[11]);
    assert(retired->frame.radarState.projection(1).cachedCenter == initialCache.cachedCenter);
    assert(sameAnimation(animation(actors[1]),initial));
    actors[1].stateBytes4dTo52[1] = 1;
    order.push_back({Level1PresentationKind::entity,1});
    //Strict composition, selector ownership, callback and metadata guards remain.
    rejects([&] { planLevel1CombatFrame(env->data,input,presentation,680,settings,env->pages,env->radar,env->pixels); });
    actors[6].renderProfile = WorldActorRenderProfile::noHandles;
    rejects([&] { planLevel1PeriodicCombatFrame(env->data,input,presentation,680,settings,env->pages,env->radar,env->pixels); });
    actors[6].renderProfile = WorldActorRenderProfile::heapSelectorDeferredAnimation;
    actors[6].animatedModelPresenterEnabled = false;
    rejects([&] { planLevel1PeriodicCombatFrame(env->data,input,presentation,680,settings,env->pages,env->radar,env->pixels); });
    actors[6].animatedModelPresenterEnabled = true; actors[6].modelAnimationVariant = 8;
    rejects([&] { planLevel1PeriodicCombatFrame(env->data,input,presentation,680,settings,env->pages,env->radar,env->pixels); });
    actors[6].modelAnimationVariant = 3; actors[6].stateBytes4dTo52[3] = 6;
    rejects([&] { planLevel1PeriodicCombatFrame(env->data,input,presentation,680,settings,env->pages,env->radar,env->pixels); });
    actors[6].stateBytes4dTo52[3] = 1;
    order.push_back(order.front());
    rejects([&] { planLevel1PeriodicCombatFrame(env->data,input,presentation,680,settings,env->pages,env->radar,env->pixels); });
    order.pop_back();
    //The existing ten ordinary slots can each retain independent selector bytes.
    for (std::size_t allocation = actors.size(); allocation < 14; ++allocation) {
        auto extra = born;
        extra.allocationIndex = allocation;
        setAnimation(extra,{5,static_cast<std::uint8_t>(allocation%6),static_cast<std::uint8_t>(allocation%8)});
        actors.push_back(extra); order.push_back({Level1PresentationKind::entity,allocation});
        env->radar.initializeAllocation(allocation);
    }
    diagnosticPhase = "ten ordinary capacity";
    auto capacity = std::unique_ptr<Level1FramePlan>(new Level1FramePlan(planLevel1PeriodicCombatFrame(
        env->data,input,presentation,680,settings,env->pages,env->radar,env->pixels)));
    assert(capacity->radarActors.size() == 14);
    for (const auto& actor : actors) {
        if (actor.type == 11) { checkPhases(*capacity,actor.allocationIndex,animation(actor),env->data.models[11]); }
    }
}
void retainedModel() {
    auto env = std::make_unique<Environment>();
    auto model = std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    GameplayModelTestAccess::prepareBirth(*model);
    diagnosticPhase = "model periodic birth";
    auto birth = std::unique_ptr<Level1GameplayFrameWork>(new Level1GameplayFrameWork(model->advanceRecoveredLevel1Frame(
        env->data,{90,{}},env->pages,env->radar,env->pixels,guards())));
    assert(birth->periodic && birth->periodic->spawn && birth->periodic->spawn->actor.type == 11);
    assert(model->level1State().entities().size() == 7 && !model->level1State().entities()[6].primaryType11AnimationSelector);
    assert(birth->periodic->spawn->actor.flags == 0 && birth->periodic->spawn->actor.animatedModelPresenterEnabled
        && birth->periodic->spawn->actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation);
    auto admissionPages = birth->render.pages; admissionPages.flip();
    diagnosticPhase = "fresh appended Type11 late3768 admission";
    auto admitted = std::unique_ptr<Level1GameplayFrameWork>(new Level1GameplayFrameWork(model->advanceRecoveredLevel1Frame(
        env->data,{90,{}},admissionPages,birth->render.radarState,birth->render.pixels,guards())));
    const auto& state = model->level1State();
    assert(state.loopTick() == 2 && model->ticks() == 2);
    const auto admittedOrdinal = ordinal(admitted->render,6);
    assert(admitted->render.scene.animationStates[admittedOrdinal] && admitted->render.animationsAfterRadar[admittedOrdinal]);
    const auto& admittedMain = *admitted->render.scene.animationStates[admittedOrdinal];
    const auto& admittedRadar = *admitted->render.animationsAfterRadar[admittedOrdinal];
    assert(sameAnimation(admittedRadar,level1AnimationOperations(admittedMain,env->data.models[11]).state));
    assert(sameAnimation(animation(state.entities()[6]),admittedRadar));
    assert(state.entities()[6].presentationModelAnimationVariant == birth->periodic->spawn->actor.modelAnimationVariant);
    GameplayModelTestAccess::phases(*model);
    auto pages = admitted->render.pages; pages.flip();
    diagnosticPhase = "model first controlled-phase adjacent";
    auto next = std::unique_ptr<Level1GameplayFrameWork>(new Level1GameplayFrameWork(model->advanceRecoveredLevel1Frame(
        env->data,{90,{}},pages,admitted->render.radarState,admitted->render.pixels,guards())));
    assert(state.loopTick() == 3 && model->ticks() == 3);
    for (const auto allocation : {std::size_t{1},std::size_t{6}}) {
        const auto index = ordinal(next->render,allocation);
        const auto incoming = allocation == 1 ? Level1AnimationState{5,0,7} : Level1AnimationState{5,1,3};
        checkPhases(next->render,allocation,incoming,env->data.models[11]);
        const auto& actor = state.entities()[allocation];
        assert(sameAnimation(animation(actor),*next->render.animationsAfterRadar[index]));
        assert(actor.presentationModelAnimationVariant == incoming.variant);
    }
    assert(state.type11AnimationVariant() == 0);
    auto adjacentPages = next->render.pages; adjacentPages.flip();
    diagnosticPhase = "model second adjacent";
    auto adjacent = std::unique_ptr<Level1GameplayFrameWork>(new Level1GameplayFrameWork(model->advanceRecoveredLevel1Frame(
        env->data,{90,{}},adjacentPages,next->render.radarState,next->render.pixels,guards())));
    assert(state.loopTick() == 4 && model->ticks() == 4);
    assert(sameAnimation(animation(state.entities()[1]),{5,2,0}));
    assert(sameAnimation(animation(state.entities()[6]),{5,3,4}));
    assert(state.type11AnimationVariant() == 0);
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    try {
        frameReceipts();
        retainedModel();
    } catch (const std::exception& error) {
        std::cerr << "multiple lift fixture: " << diagnosticPhase << ": " << error.what() << '\n';
        return 1;
    }
}
