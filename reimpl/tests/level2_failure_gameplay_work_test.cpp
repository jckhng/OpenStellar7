#include "stellar7/failure_gameplay_work.hpp"
#include "stellar7/combat_effect_work.hpp"
#include "../src/retained_failure_scene.hpp"
#include <algorithm>
#include <array>
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
    static void level2(Level1State& state) {state.levelIndex_=1;}
};
struct GameplayModelTestAccess {
    static void prepare(GameplayModel& model) {
        Level1ReloadTestAccess::level2(model.level1_);
        model.activeCampaignLevel_=1;model.phase_=GameplayPhase::failurePreExplosionBlackout;
        model.failureFromLevel2_=true;model.level2FailureTailFinished_=true;
        model.level2HasAdvanced_=true;model.level2TailConsumed_=true;
        model.level2PlayerArmor_=-16;model.level2LoopTick_=501;model.failureSourceLoopTick_=502;
        model.ticks_=9000;model.level2Player_={};model.level2Player_.type=0;
        model.level2Player_.collisionRadius=723;model.level2Player_.position={7,-5,0};
        model.level2Player_.stateBytes4dTo52={0,1,0,0,0,0};
        model.level2Entities_.clear();model.level2AllocationListOrder_.clear();model.level2ActiveListOrder_.clear();
        model.level2EntityActorPoolSlots_.clear();model.level2EntityFixedPoolSlots_.clear();
        model.level2EntityProjectilePoolSlots_.clear();model.level2EntityDeathEffectPoolSlots_.clear();
        const auto append=[&](const Level1Entity actor,const std::size_t ordinary,const std::size_t fixed,
            const std::size_t projectile,const std::size_t effect) {
            const auto index=model.level2Entities_.size();model.level2Entities_.push_back(actor);
            model.level2AllocationListOrder_.push_back(index);model.level2ActiveListOrder_.push_back(index);
            model.level2EntityActorPoolSlots_.push_back(ordinary);model.level2EntityFixedPoolSlots_.push_back(fixed);
            model.level2EntityProjectilePoolSlots_.push_back(projectile);model.level2EntityDeathEffectPoolSlots_.push_back(effect);
        };
        for (std::size_t index=0;index<7;++index) {
            Level1Entity actor{};actor.type=index<3 ? 8 : index<5 ? 13 : 30;
            actor.collisionRadius=actor.type==8 ? 860 : actor.type==13 ? 559 : 868;
            actor.allocationIndex=index;actor.stateBytes4dTo52={static_cast<std::uint8_t>(index<5 ? 0xff : 0xfe),1,0,0,0,0};
            actor.position={static_cast<std::int32_t>(12000+index*1000),static_cast<std::int32_t>(10000+index*500),0};
            if(index<5) {model.level2ActorPool_[index]=actor;model.level2ActorPoolActive_[index]=true;}
            else {model.level2FixedPool_[index-5]=actor;model.level2FixedPoolKnown_[index-5]=true;}
            append(actor,index<5 ? index : 16,index<5 ? 16 : index-5,8,8);
        }
        model.level2ProjectilePool_={};model.level2ProjectilePoolActive_={};
        model.level2DeathEffectPool_={};model.level2DeathEffectPoolActive_={};
        for(auto& effect:model.level2DeathEffectPool_) {
            effect.flags=0x88;
            effect.stateBytes4dTo52[0]=0xfc;
        }
        for (std::size_t slot=0;slot<3;++slot) {
            Level1Entity shot{};shot.type=slot==0 ? 19 : 23;shot.collisionRadius=slot==0 ? 441 : 134;
            shot.stateBytes4dTo52={0xfd,64,0,0,0,0};shot.position={1500,5000,0};
            model.level2ProjectilePool_[slot]=shot;model.level2ProjectilePoolActive_[slot]=true;
            append(shot,16,16,slot,8);
        }
        Level1Entity effect{};effect.type=-1;effect.flags=0x88;effect.position={1000,5000,0};
        effect.stateBytes4dTo52={0xfc,10,0,0,12,0};
        model.level2DeathEffectPool_[0]=effect;model.level2DeathEffectPoolActive_[0]=true;
        append(effect,16,16,8,0);
        model.level2ObjectListOrder_=model.level2AllocationListOrder_;
        // Actual source-style visibility bookkeeping, followed by one tail-only pending primary.
        model.level2Visibility_.refresh(model.level2Player_.position,model.level2Entities_,
            model.level2AllocationListOrder_,{},VisibilityDomain::nativeGameplay);
        model.level2ActiveListOrder_.erase(std::find(model.level2ActiveListOrder_.begin(),model.level2ActiveListOrder_.end(),9));
    }
    static const FailureControlState& control(const GameplayModel& m) {return *m.recoveredFailureControl_;}
    static std::uint8_t age(const GameplayModel& m,const std::size_t slot) {return m.level2DeathEffectPool_[slot].stateBytes4dTo52[1];}
    static bool shot(const GameplayModel& m,const std::size_t slot) {return m.level2ProjectilePoolActive_[slot];}
    static std::uint16_t loop(const GameplayModel& m) {return m.level2LoopTick_;}
    static std::uint16_t failureLoop(const GameplayModel& m) {return *m.failureSourceLoopTick_;}
    static StellarRng rng(const GameplayModel& m) {return m.level2Rng_;}
    static void unconsume(GameplayModel& m) {m.level2TailConsumed_=false;}
    static void aliases(GameplayModel& m) {m.level2EntityActorPoolSlots_[1]=0;}
    static void type4(GameplayModel& model) {
        for(std::size_t slot=0;slot<3;++slot) {
            auto& actor=model.level2ActorPool_[slot];
            actor.type=4;actor.collisionRadius=928;actor.forwardVelocity=180;
            actor.yawVelocity=16;actor.weaponSlotTimers={32,0,0};
            actor.stateBytes4dTo52[4]=1;actor.stateBytes4dTo52[5]=4;
            actor.animatedModelPresenterEnabled=false;actor.renderProfile=WorldActorRenderProfile::noHandles;
            actor.stateBytes4dTo52[1]=slot==1 ? 0 : 2;
            model.level2ActorPoolActive_[slot]=actor.active();
            model.level2ActorIncarnations_[slot]=100+slot;
            model.level2Entities_[slot]=actor;
        }
        for(auto* order:{&model.level2AllocationListOrder_,&model.level2ActiveListOrder_,&model.level2ObjectListOrder_}) {
            order->erase(std::remove(order->begin(),order->end(),1),order->end());
        }
    }
    static void verifyType4(const GameplayModel& model) {
        for(std::size_t slot=0;slot<3;++slot) {assert(model.level2ActorIncarnations_[slot]==100+slot);}
        assert(!model.level2ActorPoolActive_[1] && !model.level2ActorPool_[1].active());
        assert(std::find(model.level2AllocationListOrder_.begin(),model.level2AllocationListOrder_.end(),1)
            ==model.level2AllocationListOrder_.end());
    }
    static void unsupportedType(GameplayModel& model) {
        model.level2ActorPool_[0].type=1;model.level2Entities_[0].type=1;
    }
    static void verifyPhysical(const GameplayModel& m) {
        const auto keys=m.recoveredLevel2RenderKeys();assert(keys.size()==m.level2Entities_.size());
        for(std::size_t i=0;i<keys.size();++i) {
            const auto key=keys[i];const auto& actor=m.level2Entities_[i];
            const auto& canonical=key<16 ? m.level2ActorPool_[key] : key<32 ? m.level2FixedPool_[key-16]
                : key<40 ? m.level2ProjectilePool_[key-32] : m.level2DeathEffectPool_[key-40];
            assert(actor.type==canonical.type && actor.position==canonical.position
                && actor.stateBytes4dTo52==canonical.stateBytes4dTo52);
        }
    }
};
}
namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected=false;try {action();} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
}
GameplayAssets assets() {
    GameplayAssets result;result.sharedModelRegistry={256,256};result.models.resize(41);
    constexpr std::array<std::int16_t,12> types{0,4,8,13,19,23,30,36,37,38,39,40};
    constexpr std::array<std::uint32_t,12> radii{723,928,860,559,441,134,868,171,107,88,107,157};
    for(std::size_t index=0;index<types.size();++index) {
        auto& model=result.models[types[index]];model.collisionRadius=radii[index];
        ModelLod lod{};lod.threshold=255;lod.flags=128;
        ModelGroup group{};group.animationOffset=-1;group.vertexBlock=types[index]+1;
        group.vertices={{{-16,0,-16}},{{16,0,-16}},{{0,0,16}}};
        ModelPrimitive primitive{};primitive.shapes.push_back({0x80,{72,72,0,0},255,{0,1,2}});
        group.primitives.push_back(primitive);lod.groups.push_back(group);model.lods.push_back(lod);
    }
    result.modelColorTables.resize(2);
    for(unsigned i=0;i<256;++i) {result.modelColorTables[0][i]=static_cast<std::uint8_t>(i);}
    result.campaignLevels.resize(2);auto& horizon=result.campaignLevels[1].bitmapGroups.emplace_back();
    for(unsigned i=0;i<4;++i) {horizon.push_back({256,18,std::vector<std::uint8_t>(256*18,1)});}
    horizon.push_back({72,38,std::vector<std::uint8_t>(72*38)});
    result.artGroups.resize(6);result.artGroups[5].resize(30);
    for(auto& frame:result.artGroups[5]) {frame={4,2,{1,1,1,1,1,1,1,1}};}
    return result;
}
std::unique_ptr<GameplayModel> prepared() {
    auto result=std::make_unique<GameplayModel>(GameplayModel::fromNativeStartup());
    GameplayModelTestAccess::prepare(*result);return result;
}
enum class MissionRoster { constructor, reusedType4 };
void retainedPools(const MissionRoster roster) {
    const auto fixture=assets();auto model=prepared();
    if(roster==MissionRoster::reusedType4) {GameplayModelTestAccess::type4(*model);}
    auto first=model->advanceRecoveredFailureStep(fixture,0xa000,{0,319,0,199});
    assert(first.entry && first.kind==FailureGameplayStepKind::display);
    assert(!GameplayModelTestAccess::shot(*model,0) && !GameplayModelTestAccess::shot(*model,1));
    assert(GameplayModelTestAccess::shot(*model,2));
    assert(first.control.expected.effects[0].entity.stateBytes4dTo52[1]==10);
    assert(GameplayModelTestAccess::age(*model,0)==11);
    assert(GameplayModelTestAccess::loop(*model)==501 && GameplayModelTestAccess::failureLoop(*model)==503);
    assert(first.horizon && first.horizon->pixels.front().color==64);
    unsigned events=0;
    const auto count=[&](const FailureGameplayStepWork& work) {
        for(const auto& step:work.control.steps) {for(const auto op:step.operations) {
            assert(op.kind!=NormalWorkKind::failureExplosionEvent);
#ifndef STELLAR7_LEVEL2_FAILURE_BASELINE
            if(op.kind==NormalWorkKind::failureLevel2ExplosionEvent) {assert(op.count==1 && op.value==6);++events;}
#endif
        }}
    };
    count(first);GameplayModelTestAccess::verifyPhysical(*model);
    const auto frozen=model->level2Entities();
    for(unsigned display=1;display<45;++display) {
        const auto work=model->advanceRecoveredFailureStep(fixture,display%2 ? 0xa800 : 0xa000,{0,318,0,199});
        assert(!work.entry && work.kind==FailureGameplayStepKind::display);count(work);
        assert(GameplayModelTestAccess::control(*model).display==display+1);
        GameplayModelTestAccess::verifyPhysical(*model);
        if(roster==MissionRoster::reusedType4) {
            GameplayModelTestAccess::verifyType4(*model);
            for(std::size_t slot=0;slot<3;++slot) {
                const auto& actor=model->level2Entities()[slot];const auto& initial=frozen[slot];
                assert(actor.position==initial.position && actor.pitch==initial.pitch && actor.roll==initial.roll
                    && actor.yaw==initial.yaw && actor.pitchVelocity==initial.pitchVelocity
                    && actor.rollVelocity==initial.rollVelocity && actor.yawVelocity==initial.yawVelocity
                    && actor.xVelocity==initial.xVelocity && actor.yVelocity==initial.yVelocity
                    && actor.zVelocity==initial.zVelocity && actor.forwardVelocity==initial.forwardVelocity
                    && actor.stateBytes4dTo52==initial.stateBytes4dTo52
                    && actor.weaponSlotTimers==initial.weaponSlotTimers);
            }
        }
    }
#ifndef STELLAR7_LEVEL2_FAILURE_BASELINE
    assert(events==8);
#endif
    assert(GameplayModelTestAccess::failureLoop(*model)==547);
    const auto calls=GameplayModelTestAccess::rng(*model).calls();
    const auto leave=model->advanceRecoveredFailureStep(fixture,0xa800,{0,318,0,199});
    assert(leave.kind==FailureGameplayStepKind::leave && !leave.scene && !leave.horizon);
    assert(GameplayModelTestAccess::control(*model).finished && model->phase()==GameplayPhase::failurePostExplosionBlackout);
    assert(GameplayModelTestAccess::failureLoop(*model)==547 && GameplayModelTestAccess::rng(*model).calls()==calls);
    rejects([&] {model->advanceRecoveredFailureStep(fixture,0xa000,{0,318,0,199});});
}
#ifndef STELLAR7_LEVEL2_FAILURE_BASELINE
void effectsAndLeaves() {
    auto fixture=assets();
    FailureEffectInput input{{0,1800,0},0,{0x88,{17,18},0,10,12,0x0ac8},{0,318,0,199},{159,99},0xa000,3,1};
    auto work=level2FailureEffectOperations(input,fixture.artGroups[5]);
    assert(work.state.age==11 && !work.retired && work.projected);
    rejects([&] {failureEffectOperations(input,fixture.artGroups[5]);});
    input.state.age=11;work=level2FailureEffectOperations(input,fixture.artGroups[5]);
    assert(work.retired && work.removal && work.removal->removedIndex==1 && work.removal->lastIndex==2);
    input.state.age=28;input.state.endAge=29;
    assert(level2FailureEffectOperations(input,fixture.artGroups[5]).retired);
    input.state.age=29;rejects([&] {level2FailureEffectOperations(input,fixture.artGroups[5]);});
    // Full camera matrix, reduced precision, and full failure viewport clip reuse.
    for(const auto angles:std::array<std::array<std::uint16_t,3>,2>{{{0,0,0},{1200,2400,3600}}}) {
        const auto camera=makeFixedMatrixQ14(1000,2000,3000);
        for(const auto relative:std::array<FixedPosition,2>{{{0,5000,0},{1000,40000,0}}}) {
            const auto center=transformToView(relative,{{},camera,{},0});
            const auto& model=fixture.models[8];const auto cull=level2FailureModelCullOperations(center,model);
            if(cull.culled) {continue;}
            const auto setup=level2FailureModelSetupOperations(center,angles,cull.projectedRadius,8,model,256,256,{camera,relative});
            auto geometry=worldModelGeometryInput(fixture,setup,center,PolygonClipMode::Enabled);
            geometry.screenCenter={159,99};
            const auto child=level2FailureModelGeometryOperations(model.lods[setup.selectedLodIndex],geometry,angles,{camera,relative},8);
            assert(!child.suffix.empty());
            for(const auto& polygon:child.polygons) {
                failurePolygonOperations(polygon.points,{0,318,0,199},PolygonClipMode::Enabled,
                    polygon.filled ? FailurePolygonStyle::filled : FailurePolygonStyle::outline,polygon.primaryColor,polygon.secondaryColor);
            }
        }
    }
}
#endif
#ifndef STELLAR7_LEVEL2_FAILURE_BASELINE
void nullSelectorMetadata() {
    const auto fixture=assets();
    const auto camera=makeFixedMatrixQ14(0,0,0);
    constexpr FixedPosition position{0,5000,0};
    for (const auto wide : {false,true}) {
        const auto relative=wide ? FixedPosition{0,40000,0} : position;
        const auto center=relative;
        const auto& model=fixture.models[0];
        const auto cull=level2FailureModelCullOperations(center,model);
        const auto setup=level2FailureModelSetupOperations(center,{0,0,0},cull.projectedRadius,0,model,256,256,{camera,relative});
        auto input=worldModelGeometryInput(fixture,setup,center,PolygonClipMode::Disabled);input.screenCenter={159,99};
        auto lod=model.lods[setup.selectedLodIndex];
        const auto reference=level2FailureModelGeometryOperations(lod,input,{0,0,0},{camera,relative},0);
        //Real player0 and debris37/40 contain offset0 even when the actor pointer is null.
        lod.groups[0].animationOffset=0;
        auto unused=lod.groups[0].primitives[0];unused.shapes[0].colors={99,99,0,0};
        lod.groups[0].primitives.push_back(unused);
        const auto actual=level2FailureModelGeometryOperations(lod,input,{0,0,0},{camera,relative},0);
        assert(actual.polygons.size()==reference.polygons.size() && actual.suffix.size()==reference.suffix.size());
        const auto operations=[](const auto& a,const auto& b) {
            return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](const auto& x,const auto& y) {
                return x.kind==y.kind && x.count==y.count && x.value==y.value;
            });
        };
        assert(operations(actual.suffix,reference.suffix));
        for(std::size_t i=0;i<actual.polygons.size();++i) {
            const auto& a=actual.polygons[i];const auto& b=reference.polygons[i];
            assert(a.points==b.points && a.primaryColor==b.primaryColor && a.secondaryColor==b.secondaryColor
                && operations(a.prefix,b.prefix));
        }
        lod.groups[0].primitives[0].kind=1;
        rejects([&] {level2FailureModelGeometryOperations(lod,input,{0,0,0},{camera,relative},0);});
    }
    //A non-null FD pointer still requires the referenced selector byte to be owned.
    const auto& model=fixture.models[23];const auto cull=level2FailureModelCullOperations(position,model);
    const auto setup=level2FailureModelSetupOperations(position,{0,0,0},cull.projectedRadius,23,model,256,256,{camera,position});
    auto input=worldModelGeometryInput(fixture,setup,position,PolygonClipMode::Disabled);input.screenCenter={159,99};
    input.animationSelectors.assign(6,0);auto lod=model.lods[setup.selectedLodIndex];lod.groups[0].animationOffset=6;
    rejects([&] {level2FailureModelGeometryOperations(lod,input,{0,0,0},{camera,position},23);});
}
#endif
void strictAndAtomic() {
    const auto fixture=assets();auto model=prepared();GameplayModelTestAccess::unconsume(*model);
    rejects([&] {model->advanceRecoveredFailureStep(fixture,0xa000,{0,319,0,199});});
    model=prepared();GameplayModelTestAccess::aliases(*model);
    rejects([&] {model->advanceRecoveredFailureStep(fixture,0xa000,{0,319,0,199});});
    model=prepared();auto bad=fixture;bad.models[8].collisionRadius=1;
    const auto rng=GameplayModelTestAccess::rng(*model).calls();
    rejects([&] {model->advanceRecoveredFailureStep(bad,0xa000,{0,319,0,199});});
    assert(model->phase()==GameplayPhase::failurePreExplosionBlackout);
    assert(GameplayModelTestAccess::rng(*model).calls()==rng && GameplayModelTestAccess::shot(*model,0));
    model=prepared();GameplayModelTestAccess::unsupportedType(*model);
    rejects([&] {model->advanceRecoveredFailureStep(fixture,0xa000,{0,319,0,199});});
    assert(model->phase()==GameplayPhase::failurePreExplosionBlackout);
}
}
int main() {
#ifdef _MSC_VER
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);_CrtSetReportMode(_CRT_ASSERT,0);
#endif
    try {
    retainedPools(MissionRoster::constructor);
    retainedPools(MissionRoster::reusedType4);
#ifndef STELLAR7_LEVEL2_FAILURE_BASELINE
    nullSelectorMetadata();effectsAndLeaves();
#endif
    strictAndAtomic();return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<"\n";return 1;}
}
