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
struct GameplayModelTestAccess {
    static Level1State& level(GameplayModel& model) { return model.level1_; }
};
struct Level1ReloadTestAccess {
    static void prepare(Level1State& state) {
        for (auto& actor : state.entities_) { actor.position.y = -16384; }
        state.entities_[0].forwardVelocity = 0;
        state.entities_[0].weaponCooldown = 255;
        state.entities_[1].behaviorTimer = 63;
    }
    static void hit(Level1State& state) {
        auto& target = state.entities_[0];
        target.position = {2000,8000,0};
        target.stateBytes4dTo52[5] = 1;
        target.behaviorTimer = 10; target.yawVelocity = 0; target.forwardVelocity = 0;
        target.presenterDistance = 0;
        auto& shot = state.projectiles_[0];
        shot.type = 23; shot.collisionRadius = 134; shot.position = {2000,7500,0};
        shot.forwardVelocity = 500; shot.stateBytes4dTo54 = {253,10,0,0,0,0,0,0};
        shot.ownerIsPlayer = true;
        shot.ownerAllocationIndex = static_cast<std::size_t>(-1);
        state.allocationList_.push_back({Level1State::ObjectKind::projectile,0});
        state.objectListOrder_.insert(state.objectListOrder_.begin(),{Level1State::ObjectKind::projectile,0});
        state.nativeInitialListPending_ = false;
    }
    static void chooseBirth(Level1State& state) {
        assert(state.loopTick_ == 16 && !state.entities_[0].active() && state.entities_[1].active());
        constexpr auto maximumGroundDraws = 2 * Level1GroundState::pointCount;
        constexpr std::size_t constructorDraws = 3;
        constexpr std::uint16_t type11Choice = 43525;
        static_assert(maximumGroundDraws + constructorDraws < StellarRng::wordCount / 2);
        assert(state.entities_[1].stateBytes4dTo52[5] == 0 && state.entities_[1].behaviorTimer > 0);
        std::array<std::uint16_t,StellarRng::wordCount> table{};
        //B39E's A half remains zero; each possible ground/spawn B read selects11.
        std::fill(table.end() - (maximumGroundDraws + constructorDraws),table.end(),type11Choice);
        state.rng_ = StellarRng(table,48,110,0,state.rng_.calls());
    }
    static void lastLiftRoster(Level1State& state) {
        //Synthetic source-shaped3303 roster, after a genuine owned first frame.
        auto lift=state.entities_[1];
        state.entities_[0].stateBytes4dTo52[1]=0;
        auto retired=state.entities_[0];
        retired.allocationIndex=1;retired.type=3;retired.collisionRadius=770;
        retired.forwardVelocity=0;retired.yawVelocity=0;retired.weaponSlotCount=1;
        retired.renderProfile=WorldActorRenderProfile::noHandles;
        retired.animatedModelPresenterEnabled=false;
        state.entities_[1]=retired;
        lift.allocationIndex=state.entities_.size();
        //Reuse the culled-model fixture; real-resource replay covers visible geometry.
        lift.position={2000,-8000,150};lift.pitch=lift.roll=lift.yaw=0;
        lift.pitchVelocity=lift.rollVelocity=lift.yawVelocity=0;
        lift.stateBytes4dTo52[5]=0;lift.behaviorTimer=63;
        lift.primaryType11AnimationSelector=false;
        lift.renderProfile=WorldActorRenderProfile::heapSelectorDeferredAnimation;
        state.entities_.push_back(lift);
        state.activeEnemyCount_=1;state.spawnedEnemyCount_=10;
        state.allocationList_={{Level1State::ObjectKind::entity,6},
            {Level1State::ObjectKind::entity,2},{Level1State::ObjectKind::entity,3},
            {Level1State::ObjectKind::entity,4},{Level1State::ObjectKind::entity,5}};
        state.objectListOrder_=state.allocationList_;
        state.activeListOrder_={6,2,3,4,5};
        state.semanticActiveListOrder_=state.activeListOrder_;
        state.nativeInitialListPending_=false;
    }
    static void hitLastLift(Level1State& state) {
        auto& lift=state.entities_[6];
        assert(lift.active()&&!lift.primaryType11AnimationSelector);
        auto& shot=state.projectiles_[0];
        assert(!shot.active());
        shot=Level1Projectile{};shot.poolSlot=0;
        shot.type=23;shot.collisionRadius=134;
        shot.position={lift.position.x,lift.position.y+270-500,0};
        shot.forwardVelocity=500;shot.stateBytes4dTo54={253,10,0,0,0,0,0,0};
        shot.ownerIsPlayer=true;shot.ownerAllocationIndex=static_cast<std::size_t>(-1);
        state.allocationList_.push_back({Level1State::ObjectKind::projectile,0});
        state.objectListOrder_.push_back({Level1State::ObjectKind::projectile,0});
    }
    static void phases(Level1State& state) {
        assert(state.entities_[0].type == 11 && state.entities_[1].type == 11);
        state.entities_[0].stateBytes4dTo52[3] = 1; state.entities_[0].modelAnimationVariant = 3;
        state.entities_[1].stateBytes4dTo52[3] = 0; state.entities_[1].modelAnimationVariant = 7;
    }
};
}
namespace {
using namespace stellar7;
const char* diagnosticPhase = "initialization";
std::uint16_t diagnosticLoop = 0;
GameplayAssets assets() {
    GameplayAssets result;
    result.horizonFrames.assign(4,{256,16,std::vector<std::uint8_t>(256*16,7)});
    result.artGroups.resize(6);
    result.artGroups[5].assign(40,{8,5,std::vector<std::uint8_t>(40,7)});
    result.artGroups[3] = {{120,56,std::vector<std::uint8_t>(120*56,7)},
        {4,1,{0,0,0,0}},{120,18,std::vector<std::uint8_t>(120*18,3)}};
    result.artGroups[4].assign(15,{4,1,{0,0,0,0}});
    for (std::size_t index = 7; index < 15; ++index) {
        result.artGroups[4][index] = {8,1,std::vector<std::uint8_t>(8,13)};
    }
    result.models.resize(44);
    result.models[1].collisionRadius = 1342;
    //Reuse the world-model geometry and two-group Type1 LOD fixtures.
    ModelGroup tankGroup{};
    tankGroup.vertexBlock = 10;
    tankGroup.animationOffset = -1;
    tankGroup.vertices = {{{0, -256, 0}}, {{-10, 1000, 0}}, {{10, 1000, 0}}, {{0, 1000, 10}}};
    ModelPrimitive tankPrimitive{};
    tankPrimitive.shapes = {{0x81, {8, 8, 1, 1}, 0, {1, 2, 3}}};
    auto tankVariant = tankPrimitive;
    tankVariant.shapes[0].colors = {9, 9, 1, 1};
    tankGroup.primitives = {tankPrimitive, tankVariant};
    result.models[1].lods = {{10, 128, {tankGroup, tankGroup}},
        {30, 128, {tankGroup, tankGroup}}, {255, 128, {tankGroup, tankGroup}}};
    result.modelColorTables.resize(2);
    for (std::size_t index=0;index<256;++index) {
        result.modelColorTables[0][index]=static_cast<std::uint8_t>(index);
    }
    result.models[3].collisionRadius = 770; result.models[3].flags = 1;
    result.models[11].collisionRadius = 748;
    result.models[11].flags = 1;
    result.models[11].animationLimits = {8};
    result.models[17].collisionRadius = 441;
    result.models[20].collisionRadius = 134;
    result.models[23].collisionRadius = 134;
    result.models[29].collisionRadius = 862;
    result.sharedModelRegistry[0] = result.sharedModelRegistry[1] = 256;
    result.scoreFont = {32,8,0,std::vector<IndexedBitmap>(96,{4,8,std::vector<std::uint8_t>(32,1)})};
    return result;
}
FrameTailGuards guards() {
    FrameTailGuards result{}; result.keyboardHead = result.keyboardTail = 0x1e;
    return result;
}
bool sameAnimation(const Level1AnimationState a,const Level1AnimationState b) {
    return a.reload == b.reload && a.countdown == b.countdown && a.variant == b.variant;
}
std::size_t ordinal(const Level1FramePlan& frame,const std::size_t allocation) {
    const auto found = std::find_if(frame.radarActors.begin(),frame.radarActors.end(),
        [allocation](const auto& actor) { return actor.allocation == allocation; });
    assert(found != frame.radarActors.end());
    return static_cast<std::size_t>(found-frame.radarActors.begin());
}
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
struct Environment {
    GameplayAssets data = assets();
    CockpitPageState pages = CockpitPageState::fromRecoveredMenu();
    IndexedPages pixels;
    RadarState radar{data.artGroups[3]};
    Environment() {
        for (const std::uint16_t page : {0xa000,0xa400,0xa800}) {
            for (std::int16_t y = 0; y < 200; ++y) {
                for (std::int16_t x = 0; x < 320; ++x) { pixels.write(page,{x,y},93); }
            }
        }
        for (std::size_t index = 0; index < 7; ++index) { radar.initializeAllocation(index); }
        for (std::size_t slot = 0; slot < Level1State::deathEffectPoolSize; ++slot) { radar.initializeAllocation(32+slot); }
        pixels.apply(radar.initializeLevel1(680,{},0,
            {pages.cachePage(),pages.backPage(),pages.frontPage(),pages.backPage(),pages.cachePage()}));
    }
    void carry(const Level1GameplayFrameWork& frame) {
        pages = frame.render.pages; pages.flip();
        pixels = frame.render.pixels; radar = frame.render.radarState;
        if (frame.lamp) { pages = frame.lamp->pages; pixels.apply(frame.lamp->effects); }
    }
};
void recycledInitialTank() {
    auto env = std::make_unique<Environment>();
    auto model = std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    auto& state = GameplayModelTestAccess::level(*model);
    Level1ReloadTestAccess::prepare(state);
    auto step = [&]() {
        diagnosticLoop = state.loopTick();
        auto frame = std::unique_ptr<Level1GameplayFrameWork>(new Level1GameplayFrameWork(model->advanceRecoveredLevel1Frame(
            env->data,{90,{}},env->pages,env->radar,env->pixels,guards())));
        env->carry(*frame);
        return frame;
    };
    diagnosticPhase = "owned first frame";
    auto first = step();
    assert(state.entities().size() == 6 && state.loopTick() == 1);
    Level1ReloadTestAccess::hit(state); env->radar.initializeAllocation(16);
    diagnosticPhase = "lethal initial Type1";
    auto killed = step();
    assert(!state.entities()[0].active() && state.entities()[0].type == 1);
    assert(state.score() == 20 && state.activeEnemyCount() == 1 && state.spawnedEnemyCount() == 2);
    assert(killed->prefixScores.size() == 1 && state.deathEffects()[0].listed);
    //Carry the source's own fourteen adjacent complete frames to periodic16.
    diagnosticPhase = "adjacent frames before periodic16";
    while (state.loopTick() < 16) { step(); }
    assert(!state.deathEffects()[0].listed);
    Level1ReloadTestAccess::chooseBirth(state);
    const auto score = state.score(); const auto eventCount = state.audioEvents().size();
    diagnosticPhase = "source periodic16 reuse0";
    const auto beforeBirthRngCalls = state.rng().calls();
    auto birth = step();
    assert(birth->render.ground.rng.calls() >= beforeBirthRngCalls
        && birth->render.ground.rng.calls() - beforeBirthRngCalls <= 2 * Level1GroundState::pointCount);
    assert(birth->periodic && birth->periodic->spawn && birth->periodic->provider);
    const auto& receipt = *birth->periodic->spawn;
    assert(receipt.poolSlot == 0 && receipt.actor.allocationIndex == 0 && receipt.actor.type == 11);
    assert(state.entities().size() == 6 && state.entities()[0].type == 11 && state.entities()[1].type == 11);
    assert(!state.entities()[0].primaryType11AnimationSelector && state.entities()[1].primaryType11AnimationSelector);
    assert(state.entities()[0].renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation);
    assert(receipt.actor.flags == 0 && receipt.actor.animatedModelPresenterEnabled);
    assert(state.entities()[0].stateBytes4dTo52[3] == 0 && state.entities()[0].modelAnimationVariant == 0);
    assert(birth->periodic->provider->entityIndex == 0 && !birth->periodic->provider->guestElapsedQualified);
    assert(state.activeEnemyCount() == 2 && state.spawnedEnemyCount() == 3);
    assert(state.score() == score && state.audioEvents().size() == eventCount);
    assert(env->radar.projection(0).cachedCenter == FixedPosition{});
    auto ground = state.ground(); auto rng = state.rng();
    std::vector<Level1ObjectRef> strictOrder;
    for (std::size_t index = 0; index < state.entities().size(); ++index) {
        strictOrder.push_back({Level1PresentationKind::entity,index});
    }
    Level1ProjectileRenderState strict{ground,rng,state.player(),state.entities(),state.projectiles(),strictOrder,state.loopTick()};
    rejects([&] { planLevel1CombatFrame(env->data,strict,{{140,4},false},680,{90,{}},env->pages,env->radar,env->pixels); });
    diagnosticPhase = "fresh reused0 Type11 late3768 admission";
    auto admitted = step();
    assert(state.loopTick() == 18 && model->ticks() == 18);
    const auto admittedOrdinal = ordinal(admitted->render,0);
    assert(admitted->render.scene.animationStates[admittedOrdinal] && admitted->render.animationsAfterRadar[admittedOrdinal]);
    const auto& admittedMain = *admitted->render.scene.animationStates[admittedOrdinal];
    const auto& admittedRadar = *admitted->render.animationsAfterRadar[admittedOrdinal];
    assert(sameAnimation(admittedRadar,level1AnimationOperations(admittedMain,env->data.models[11]).state));
    assert(state.entities()[0].modelAnimationVariant == admittedRadar.variant
        && state.entities()[0].stateBytes4dTo52[3] == admittedRadar.countdown);
    assert(state.entities()[0].presentationModelAnimationVariant == receipt.actor.modelAnimationVariant);
    Level1ReloadTestAccess::phases(state);
    diagnosticPhase = "first controlled-phase frame after reused0 Type11";
    auto next = step();
    const auto reused = ordinal(next->render,0), initial = ordinal(next->render,1);
    assert(sameAnimation(*next->render.scene.animationStates[reused],{5,0,3}));
    assert(sameAnimation(*next->render.animationsAfterRadar[reused],{5,5,4}));
    assert(sameAnimation(*next->render.scene.animationStates[initial],{5,5,0}));
    assert(sameAnimation(*next->render.animationsAfterRadar[initial],{5,4,0}));
    assert(state.entities()[0].modelAnimationVariant == 4 && state.entities()[0].stateBytes4dTo52[3] == 5);
    assert(state.entities()[1].modelAnimationVariant == 0 && state.entities()[1].stateBytes4dTo52[3] == 4);
    assert(state.entities()[0].presentationModelAnimationVariant == 3
        && state.entities()[1].presentationModelAnimationVariant == 7 && state.type11AnimationVariant() == 0);
    diagnosticPhase = "second frame after reused0 Type11";
    auto adjacent = step();
    assert(state.entities()[0].modelAnimationVariant == 4 && state.entities()[0].stateBytes4dTo52[3] == 3);
    assert(state.entities()[1].modelAnimationVariant == 0 && state.entities()[1].stateBytes4dTo52[3] == 2);
    assert(state.entities().size() == 6 && state.loopTick() == 20 && model->ticks() == 20 && state.score() == 20);
    assert(adjacent->render.animationsAfterRadar.size() == adjacent->render.radarActors.size());
}
void lastNonprimaryLift() {
    auto env=std::make_unique<Environment>();
    auto model=std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    auto& state=GameplayModelTestAccess::level(*model);
    Level1ReloadTestAccess::prepare(state);
    const auto step=[&]() {
        diagnosticLoop=state.loopTick();
        auto work=std::unique_ptr<Level1GameplayFrameWork>(new Level1GameplayFrameWork(
            model->advanceRecoveredLevel1Frame(env->data,{90,{}},env->pages,env->radar,env->pixels,guards())));
        env->carry(*work);return work;
    };
    diagnosticPhase="last lift owned first frame";step();
    Level1ReloadTestAccess::lastLiftRoster(state);
    diagnosticPhase="last nonprimary lift retained render";
    const auto live=step();
    const auto liftOrdinal=ordinal(live->render,6);
    assert(live->render.animationsAfterRadar[liftOrdinal]);
    const auto retained=env->radar.projection(6);
    const auto selector=state.entities()[6].stateBytes4dTo52;
    const auto variant=state.entities()[6].modelAnimationVariant;
    assert(!state.entities()[0].active()&&state.entities()[0].type==1);
    assert(!state.entities()[1].active()&&state.entities()[1].type==3);
    assert(std::none_of(state.entities().begin(),state.entities().end(),
        [](const auto& actor) {return actor.primaryType11AnimationSelector;}));
    Level1ReloadTestAccess::hitLastLift(state);env->radar.initializeAllocation(16);
    diagnosticPhase="last nonprimary lift lethal1176 late3768 frame";
    const auto killed=step();
    assert(!state.entities()[6].active()&&state.entities()[6].type==11);
    assert(state.score()==50&&state.activeEnemyCount()==0&&state.spawnedEnemyCount()==10);
    assert(killed->prefixScores.size()==1&&state.deathEffects()[0].listed);
    assert(state.entities()[6].stateBytes4dTo52[2]==selector[2]
        &&state.entities()[6].stateBytes4dTo52[3]==selector[3]
        &&state.entities()[6].modelAnimationVariant==variant);
    const auto assertNoLift=[&](const Level1GameplayFrameWork& frame) {
        assert(frame.render.radarActors.size()==frame.render.animationsAfterRadar.size());
        assert(std::none_of(frame.render.radarActors.begin(),frame.render.radarActors.end(),
            [](const auto& actor) {return actor.type==11;}));
        assert(std::none_of(frame.render.scene.animationStates.begin(),frame.render.scene.animationStates.end(),
            [](const auto& value) {return value.has_value();}));
        assert(std::none_of(frame.render.animationsAfterRadar.begin(),frame.render.animationsAfterRadar.end(),
            [](const auto& value) {return value.has_value();}));
        const auto& cache=frame.render.radarState.projection(6);
        assert(cache.cachedCenter==retained.cachedCenter&&cache.projectedX==retained.projectedX
            &&cache.projectedY==retained.projectedY&&cache.projectedRadius==retained.projectedRadius);
    };
    assertNoLift(*killed);
    env->data.models[11].animationLimits.clear();
    diagnosticPhase="no-lift adjacent frames and terminalFC";
    for(std::size_t frame=0;frame<4;++frame) {const auto next=step();assertNoLift(*next);}
    assert(!state.deathEffects()[0].listed&&state.activeListOrder().size()==4);
    auto ground=state.ground();auto rng=state.rng();
    std::vector<Level1ObjectRef> order;
    for(const auto index:state.activeListOrder()) {order.push_back({Level1PresentationKind::entity,index});}
    Level1ProjectileRenderState input{ground,rng,state.player(),state.entities(),state.projectiles(),order,state.loopTick()};
    Level1CombatRenderContext context{order,{},Level1CombatLiftMembership::allowRetiredInitial};
    context.ordinaryFamily=Level1OrdinaryFamily::type1And3;
    context.projectileAllocationDomain=Level1ProjectileAllocationDomain::retainedRoster;
    //Legacy entry keeps the primary-initial-lift requirement even with retained FD identities.
    rejects([&]{planLevel1EffectCombatFrame(env->data,input,context,{{140,4},false},680,
        {90,{}},env->pages,env->radar,env->pixels);});
    context.liftMembership=Level1CombatLiftMembership::required;
    rejects([&]{planLevel1EffectCombatFrame(env->data,input,context,{{140,4},false},680,
        {90,{}},env->pages,env->radar,env->pixels);});
}
void type3Adapter() {
    auto env = std::make_unique<Environment>();
    Level1State level; level.reloadAfterFailure(StellarRng{});
    auto actors = level.entities(); for (auto& actor : actors) { actor.position.y = -16384; }
    auto tank = actors[0]; tank.allocationIndex = 6; tank.type = 3; tank.collisionRadius = 770;
    tank.position = {6000,-16384,0}; tank.forwardVelocity = 216; tank.weaponSlotCount = 1;
    tank.renderProfile = WorldActorRenderProfile::noHandles; actors.push_back(tank);
    std::vector<Level1ObjectRef> order;
    for (std::size_t index = 0; index < actors.size(); ++index) { order.push_back({Level1PresentationKind::entity,index}); }
    std::array<Level1Projectile,Level1State::projectilePoolSize> shots{};
    auto& shot = shots[0]; shot.type = 20; shot.collisionRadius = 134;
    shot.position = {7000,-16384,0}; shot.forwardVelocity = 500;
    shot.ownerAllocationIndex = 6; shot.stateBytes4dTo54 = {253,60,0,0,0,0,0,0};
    order.push_back({Level1PresentationKind::projectile,0}); env->radar.initializeAllocation(16);
    auto ground = level.ground(); auto rng = level.rng();
    Level1ProjectileRenderState input{ground,rng,level.player(),actors,shots,order,2};
    Level1CombatRenderContext context{order,{}};
    const Level1PresentationState presentation{{140,4},false};
    auto plan = [&]() { return planLevel1EffectCombatFrame(env->data,input,context,presentation,680,{90,{}},env->pages,env->radar,env->pixels); };
    rejects([&] { plan(); });
    context.ordinaryFamily = Level1OrdinaryFamily::type1And3;
    auto accepted = std::unique_ptr<Level1CombatFramePlan>(new Level1CombatFramePlan(plan()));
    assert(accepted->frame.radarActors.size() == 8 && accepted->frame.animationsAfterRadar[1]);
    actors[6].roll = 22; rejects([&] { plan(); }); actors[6].roll = 0;
    shot.roll = 22; rejects([&] { plan(); }); shot.roll = 0;
    shot.position.z = 150; rejects([&] { plan(); }); shot.position.z = 0;
    shot.type = 17; shot.collisionRadius = 441; rejects([&] { plan(); });
    shot.type = 20; shot.collisionRadius = 134;
    context.ordinaryFamily = static_cast<Level1OrdinaryFamily>(99); rejects([&] { plan(); });
    context.ordinaryFamily = Level1OrdinaryFamily::type1And3;
    context.projectileAllocationDomain=Level1ProjectileAllocationDomain::retainedRoster;
    shot.roll=22;shot.position.z=150;
    assert(plan().frame.radarActors.size()==8);
    auto strict=context;strict.projectileAllocationDomain=Level1ProjectileAllocationDomain::retainedAirborne;
    rejects([&]{planLevel1EffectCombatFrame(env->data,input,strict,presentation,680,{90,{}},env->pages,env->radar,env->pixels);});
    //The current physical occupant can be1 while the retained shot remains20.
    actors[6]=actors[0];actors[6].allocationIndex=6;actors[6].position={6000,-16384,0};
    context.ordinaryFamily=Level1OrdinaryFamily::type1;
    const auto reused20=plan();
    assert(reused20.frame.radarActors.back().type==20 && reused20.frame.radarActors.size()==8);
    //A surviving17 likewise does not become20 when that same record becomes11.
    actors[6]=actors[1];actors[6].allocationIndex=6;actors[6].position={6000,-16384,1000};
    actors[6].primaryType11AnimationSelector=false;
    actors[6].renderProfile=WorldActorRenderProfile::heapSelectorDeferredAnimation;
    shot.type=17;shot.collisionRadius=441;shot.roll=0;shot.position.z=0;shot.stateBytes4dTo54[1]=54;
    const auto reused17=plan();
    assert(reused17.frame.radarActors.back().type==17 && reused17.frame.radarActors.size()==8);
    auto strict17=context;strict17.projectileAllocationDomain=Level1ProjectileAllocationDomain::retainedAirborne;
    rejects([&]{planLevel1EffectCombatFrame(env->data,input,strict17,presentation,680,{90,{}},env->pages,env->radar,env->pixels);});
    shot.ownerAllocationIndex=2;rejects([&]{plan();});
    shot.ownerAllocationIndex=actors.size();rejects([&]{plan();});
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    try {
        recycledInitialTank();
        lastNonprimaryLift();
        diagnosticPhase = "Type3 adapter controls";
        type3Adapter();
    } catch (const std::exception& error) {
        std::cerr << "retained ordinary fixture: " << diagnosticPhase << ", loop " << diagnosticLoop
            << ": " << error.what() << '\n';
        return 1;
    }
}
