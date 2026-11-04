#include "stellar7/failure_control_work.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;
template<class Function> void rejects(Function function) {
    bool rejected=false;try {function();} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
}
std::uint64_t count(const std::vector<NormalWorkOperation>& operations) {
    std::uint64_t result=0;for(const auto op:operations) {assert(op.kind==NormalWorkKind::instructions);result+=op.count;}return result;
}
FailureControlInput input() {
    auto state=std::make_unique<Level1State>();state->reloadAfterFailure(StellarRng{});
    FailureControlInput result;result.snapshot.player=state->player();result.snapshot.mission=state->entities();
    result.snapshot.projectiles=state->projectiles();result.snapshot.effects=state->deathEffects();result.snapshot.rng=state->rng();
    result.snapshot.sourceLoopTick=2;
    for(std::size_t i=0;i<6;++i) {result.control.allocationOrder.push_back({FailureObjectKind::mission,i});result.control.presenterOrder.push_back({FailureObjectKind::mission,i});}
    return result;
}
void effectChild(FailureControlWork& work) {
    std::vector<FailureObjectRef> retired;
    for(const auto ref:work.drawOrder) {
        if(ref.kind!=FailureObjectKind::effect) {continue;}
        auto& effect=work.expected.effects.at(ref.index);effect.entity.flags=0;
        ++effect.entity.stateBytes4dTo52[1];
        if(effect.entity.stateBytes4dTo52[1]>=effect.entity.stateBytes4dTo52[4]) {
            effect.entity.stateBytes4dTo52[1]=0;effect.entity.stateBytes4dTo52[4]=0;effect.listed=false;retired.push_back(ref);
        }
    }
    work.control=applyFailureSceneRetirements(work.control,retired);
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    Level1Entity actor{};actor.position={100,-200,300};actor.pitch=0x3456;actor.roll=0x789a;actor.yaw=0xa234;
    actor.pitchVelocity=321;actor.rollVelocity=-987;actor.yawVelocity=1234;actor.xVelocity=-123;actor.yVelocity=456;actor.zVelocity=-789;actor.forwardVelocity=67;
    const auto moved=failureKinematicsOperations(actor);
    assert(count(moved.operations)==267 && moved.actor.position==(FixedPosition{-10,246,-425}));
    assert(moved.actor.pitch==13719 && moved.actor.roll==29887 && moved.actor.yaw==42758);
    actor.position={-500,700,-300};actor.pitch=0x8abc;actor.roll=0xfedc;actor.yaw=0x1234;
    actor.pitchVelocity=-53;actor.rollVelocity=62;actor.yawVelocity=-71;actor.xVelocity=80;actor.yVelocity=-90;actor.zVelocity=100;actor.forwardVelocity=83;
    const auto positive=failureKinematicsOperations(actor);
    assert(count(positive.operations)==261 && positive.actor.position==(FixedPosition{-387,536,-222}));

    auto incoming=input();incoming.snapshot.player.position={7,-5,0};
    auto& listed=incoming.snapshot.projectiles[0];listed.type=17;listed.stateBytes4dTo54[1]=10;listed.presenterDistance=1234;listed.targetHeading=4321;listed.ownerAllocationIndex=0;
    incoming.snapshot.projectiles[1]=listed;incoming.snapshot.projectiles[1].poolSlot=1;
    incoming.control.allocationOrder.push_back({FailureObjectKind::projectile,0});incoming.control.allocationOrder.push_back({FailureObjectKind::projectile,1});
    incoming.control.presenterOrder.push_back({FailureObjectKind::projectile,0});
    auto wrapped=incoming;wrapped.snapshot.sourceLoopTick=16;wrapped.snapshot.mission[2].position={40000,-40000,0};
    const auto wrap=failureEnterOperations(wrapped);
    assert(wrap.expected.mission[2].position==(FixedPosition{-25543,25541,0}));
    assert(wrap.expected.projectiles[0].position==(FixedPosition{-7,5,0}));
    assert(wrap.expected.projectiles[1].position==(FixedPosition{}));
    auto duplicate=incoming;duplicate.control.presenterOrder.push_back(duplicate.control.presenterOrder.front());
    rejects([&]() {failureEnterOperations(duplicate);});
    auto entered=failureEnterOperations(incoming);
    assert(entered.expected.player.position==(FixedPosition{}));
    assert(entered.expected.mission[0].position==(FixedPosition{-2055,2053,0}));
    assert(entered.expected.projectiles[0].stateBytes4dTo54[1]==0 && entered.expected.projectiles[1].stateBytes4dTo54[1]==10);
    assert(entered.expected.projectiles[0].presenterDistance==1234 && entered.expected.projectiles[0].targetHeading==4321);
    assert(entered.control.allocationOrder[6]==(FailureObjectRef{FailureObjectKind::player,0}));
    assert(entered.control.allocationOrder[7]==(FailureObjectRef{FailureObjectKind::projectile,1}));
    assert(entered.control.presenterOrder.back()==(FailureObjectRef{FailureObjectKind::projectile,0}));
    assert(entered.expected.camera.position==(FixedPosition{1800,0,0}));
    rejects([&]() {FailureControlInput invalid{entered.control,entered.expected};failureEnterOperations(invalid);});

    incoming=input();entered=failureEnterOperations(incoming);incoming.control=entered.control;incoming.snapshot=entered.expected;
    unsigned events=0;std::vector<FailureObjectRef> firstOrder;
    for(unsigned frame=0;frame<45;++frame) {
        auto display=failureDisplayOperations(incoming);
        if(frame==0) {assert(display.expected.debris.size()==30 && display.drawOrder.size()==7);firstOrder=display.drawOrder;}
        if(frame==2) {
            assert(std::find(display.drawOrder.begin(),display.drawOrder.end(),FailureObjectRef{FailureObjectKind::player,0})!=display.drawOrder.end());
            assert(std::find(display.control.allocationOrder.begin(),display.control.allocationOrder.end(),FailureObjectRef{FailureObjectKind::player,0})==display.control.allocationOrder.end());
        }
        for(const auto& step:display.steps) {for(const auto op:step.operations) {events+=op.kind==NormalWorkKind::failureExplosionEvent ? 1U : 0U;}}
        effectChild(display);incoming.control=display.control;incoming.snapshot=display.expected;
    }
    assert(events==8 && incoming.snapshot.rng.calls()==280 && incoming.snapshot.sourceLoopTick==47);
    assert(incoming.control.allocationOrder.size()==36 && incoming.control.presenterOrder.size()==36);
    const auto allocation=incoming.control.allocationOrder;const auto presenters=incoming.control.presenterOrder;
    const auto left=failureLeaveOperations(incoming);assert(left.control.finished && left.expected.debris.empty());
    assert(left.control.allocationOrder==allocation && left.control.presenterOrder==presenters);
    rejects([&]() {failureDisplayOperations(FailureControlInput{left.control,left.expected});});
    rejects([&]() {failureEnterOperations(FailureControlInput{left.control,left.expected});});
    rejects([&]() {failureLeaveOperations(FailureControlInput{left.control,left.expected});});
}
