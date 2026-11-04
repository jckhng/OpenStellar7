#define main retainedTargetRegression
#include "target_prefix_fixture.inc"
#undef main
#include "stellar7/level1_solid_contact_work.hpp"
#include <limits>

namespace {
std::size_t steps(const Level1ProjectilePrefixWork& work,const Level1ContactStep step) {
    return std::count_if(work.contacts.begin(),work.contacts.end(),[&](const auto& r) { return r.step==step; });
}
Level1TargetPrefixInput contactFixture() {
    auto result=initial(2,false);
    result.prefix.player.position={};result.prefix.player.yaw=0;result.prefix.player.forwardVelocity=0;
    result.prefix.player.yawVelocity=0;
    result.prefix.projectiles[0].stateBytes4dTo54[1]=0;
    const Level1ObjectRef shot{Level1PresentationKind::projectile,0};
    auto remove=[&](auto& order) {order.erase(std::remove(order.begin(),order.end(),shot),order.end());};
    remove(result.prefix.allocationOrder);remove(result.prefix.activeOrder);
    for (auto& actor:result.prefix.entities) {
        actor.collisionContactAllocationIndex=std::numeric_limits<std::size_t>::max();
        actor.collisionContactIsPlayer=false;
    }
    auto& lift=result.prefix.entities[1];
    lift.position={-276,854,150};lift.yaw=0x8000;lift.yawVelocity=0;lift.roll=0;
    lift.forwardVelocity=270;lift.stateBytes4dTo52[5]=0;lift.behaviorTimer=10;lift.weaponCooldown=255;
    return result;
}
Level1TargetPrefixProfile contactProfile() {
    auto result=profile();result.targetDomain=Level1TargetHitDomain::initialOrdinary;
    result.damage.enemy.contactDomain=Level1ActorContactDomain::ordinaryPlayer;
    return result;
}
Level1TargetPrefixInput contactCarry(const Level1TargetPrefixWork& result) {
    auto next=carry(result);next.prefix.impulse=result.damage.combat.prefix.impulse;return next;
}
}
int main() {
    retainedTargetRegression();
    auto input=contactFixture();const auto config=contactProfile();
    const auto first=level1TargetPrefixOperations(input,config,font(),CockpitPageState::fromRecoveredMenu());
    const auto& a=first.damage.combat.prefix;
    assert(steps(a,Level1ContactStep::actorPlayerOverlap)==1);
    assert(steps(a,Level1ContactStep::actorResponse)==0 && steps(a,Level1ContactStep::playerBackout)==0);
    assert(a.entities[1].position.y==584 && a.entities[1].stateBytes4dTo52[5]==0);
    assert(!a.entities[1].collisionContactIsPlayer && a.impulse==0);
    assert(first.damage.combat.audioEvents.empty());
    rejects([&] {level1TargetPrefixOperations(input,profile(),font(),CockpitPageState::fromRecoveredMenu());});
    auto nextConfig=config;++nextConfig.damage.enemy.loopTick;
    const auto second=level1TargetPrefixOperations(contactCarry(first),nextConfig,font(),CockpitPageState::fromRecoveredMenu());
    const auto& b=second.damage.combat.prefix;
    assert(steps(b,Level1ContactStep::scanWrite)>=1 && steps(b,Level1ContactStep::playerBackout)>1);
    assert(steps(b,Level1ContactStep::playerRebound)==1 && steps(b,Level1ContactStep::playerAudio)==1);
    assert(b.player.position.y==-160 && b.player.forwardVelocity==-10 && b.impulse==-10);
    assert(b.entities[1].stateBytes4dTo52[5]==5 && !b.entities[1].collisionContactIsPlayer);
    assert(second.damage.combat.audioEvents.size()==1 && second.damage.combat.audioEvents[0].eventId==9);
    assert(std::count_if(b.operations.begin(),b.operations.end(),[](auto op) {
        return op.kind==NormalWorkKind::gameplaySolidContactEvent && op.count==1 && op.value==9;
    })==1);
    for (const auto& r:b.contacts) {assert(r.operationBegin<r.operationEnd && r.operationEnd<=b.operations.size());}
    //Declared third-frame control separates actors to isolate carried0074 semantics.
    auto thirdInput=contactCarry(second);
    thirdInput.prefix.entities[1].position={12000,12000,150};
    ++nextConfig.damage.enemy.loopTick;
    const auto third=level1TargetPrefixOperations(thirdInput,nextConfig,font(),CockpitPageState::fromRecoveredMenu());
    assert(third.damage.combat.prefix.impulse==-5 && third.damage.combat.prefix.player.position.y==-170);
    rejects([&] {level1TargetPrefixOperations(thirdInput,profile(),font(),CockpitPageState::fromRecoveredMenu());});
    //Two fixed targets: source0C0A writes each in OLD order and returns the last.
    auto fixed=contactFixture();fixed.prefix.entities[1].position={12000,12000,150};
    fixed.prefix.entities[2].position={};fixed.prefix.entities[3].position={};
    const auto many=level1TargetPrefixOperations(fixed,config,font(),CockpitPageState::fromRecoveredMenu());
    std::vector<std::size_t> targets;
    for(const auto& r:many.damage.combat.prefix.contacts) {
        if(r.step==Level1ContactStep::scanWrite && !r.subject) {targets.push_back(*r.target);}
        if(r.step==Level1ContactStep::playerRebound) {assert(r.target==std::optional<std::size_t>{3});}
    }
    assert((targets==std::vector<std::size_t>{2,3}));
    assert(many.damage.combat.prefix.entities[2].collisionContactIsPlayer);
    assert(many.damage.combat.prefix.entities[3].collisionContactIsPlayer);
    //Source class1 retains callback1/backout even when DI is the player.
    auto tank=contactFixture();tank.prefix.entities[1].position={12000,12000,150};
    tank.prefix.entities[0].position={0,1050,0};tank.prefix.entities[0].forwardVelocity=-45;
    tank.prefix.entities[0].yaw=0;
    const auto tankWork=level1TargetPrefixOperations(tank,config,font(),CockpitPageState::fromRecoveredMenu());
    assert(steps(tankWork.damage.combat.prefix,Level1ContactStep::actorResponse)==1);
    assert(tankWork.damage.combat.prefix.entities[0].position.y==1050);
    //Declared source-constructor projection: physical0 can retain Type11 after reuse.
    //The separate model regression owns actual Type1 death ->4370 constructor ancestry.
    auto recycled=contactFixture();recycled.prefix.entities[1].position={12000,12000,1000};
    recycled.prefix.entities[0]=recycled.prefix.entities[1];
    recycled.prefix.entities[0].allocationIndex=0;
    recycled.prefix.entities[0].position={-8193,-32769,1000};
    recycled.prefix.entities[0].primaryType11AnimationSelector=false;
    const auto reused=level1TargetPrefixOperations(recycled,config,font(),CockpitPageState::fromRecoveredMenu());
    assert(reused.damage.combat.prefix.entities[0].type==11 && reused.damage.combat.prefix.entities[0].allocationIndex==0);
    auto adjacentConfig=config;++adjacentConfig.damage.enemy.loopTick;
    const auto reusedNext=level1TargetPrefixOperations(contactCarry(reused),adjacentConfig,font(),CockpitPageState::fromRecoveredMenu());
    assert(reusedNext.damage.combat.prefix.entities[0].type==11);
    auto strict=config;strict.damage.enemy.membership=Level1OrdinaryMembership::initialSix;
    rejects([&] {level1EnemyPrefixOperations(recycled.prefix,strict.damage.enemy);});
    //Stationary, both velocity signs and all yaw quadrants use exact loop semantics.
    Level1Entity player{};player.collisionRadius=723;
    auto target=input.prefix.entities[1];target.position={};
    for (const auto yaw:{0,0x2000,0x4000,0x8000,0xc000,0xffff}) {
        for (const auto velocity:{-200,-1,0,1,200}) {
            player.yaw=static_cast<std::uint16_t>(yaw);player.forwardVelocity=static_cast<std::int16_t>(velocity);
            auto displacement=player;displacement.position={};
            if(displacement.forwardVelocity==0) {displacement.forwardVelocity=10;}
            reverseEntityTranslation(displacement);
            if(displacement.position==FixedPosition{}) {
                rejects([&] {level1SolidContactOperations(player,target,1);});
                continue;
            }
            const auto work=level1SolidContactOperations(player,target,1);
            assert(!actorOverlapOperations(target,work.player).overlaps);
            assert(work.impulse==-(velocity ? velocity : 10));
        }
    }
    for (const auto impulse:{-200,-3,-1,0,1,3,200}) {
        const auto work=level1PlayerImpulseOperations(player,static_cast<std::int16_t>(impulse));
        assert(work.impulse==impulse/2);
        assert(work.player.forwardVelocity==(impulse ? impulse : player.forwardVelocity));
    }
    for (const auto type:{4,13,47}) {
        auto invalid=target;invalid.type=static_cast<std::int16_t>(type);
        rejects([&] {level1SolidContactOperations(player,invalid,1);});
    }
    auto invalid=target;invalid.stateBytes4dTo52[4]=5;
    rejects([&] {level1SolidContactOperations(player,invalid,1);});
    rejects([&] {level1PlayerImpulseOperations(player,201);});
    return 0;
}
