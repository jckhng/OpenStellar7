#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif
#include "type3_expected.hpp"
#include "stellar7/level1_primary_fire_work.hpp"
#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected=false;
    try {action();} catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
}
Level1ProjectilePrefixInput choice(const unsigned behavior,const std::uint16_t word) {
    auto input=type3test::input(0);
    input.entities[6].stateBytes4dTo52[5]=static_cast<std::uint8_t>(behavior);
    input.entities[6].weaponCooldown=255;
    std::array<std::uint16_t,StellarRng::wordCount> table{};table[55]=word;
    input.rng=StellarRng(table,0,110,0,0);
    return input;
}
Level1TargetPrefixProfile targetProfile(const std::uint16_t tick) {
    return {{type3test::profile(tick),680,0,0,0,Level1DamagePaletteMode::residentVga,
        Level1PlayerCombatDomain::expiryAndLoss},0,Level1TargetHitDomain::periodicClass1};
}
BitmapFont font() {
    return {32,8,0,std::vector<IndexedBitmap>(96,{4,8,std::vector<std::uint8_t>(32,1)})};
}
Level1TargetPrefixWork advance(Level1ProjectilePrefixInput input,const std::uint16_t tick=2) {
    return level1TargetPrefixOperations({std::move(input),{},{},2,0},targetProfile(tick),
        font(),CockpitPageState::fromRecoveredMenu());
}
std::uint64_t instructions(const Level1TargetHitWork& work) {
    std::uint64_t count=0;
    for(const auto& step:work.steps) {
        for(const auto& op:step.operations) {if(op.kind==NormalWorkKind::instructions) {count+=op.count;}}
    }
    return count;
}
}

#include "retained_emitter_reuse_cases.hpp"
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    const auto profile=type3test::profile();
    const auto random=level1EnemyPrefixOperations(type3test::input(0),profile);
    type3test::sourceEqual(random,0);
    assert(random.audioEvents.empty());
    assert(random.prefix.entities[6].forwardVelocity==216);
    assert(random.prefix.entities[6].yawVelocity==-624 && random.prefix.entities[6].behaviorTimer==1);
    auto old=profile;old.ordinaryFamily=Level1OrdinaryFamily::type1;
    rejects([&]{level1EnemyPrefixOperations(type3test::input(0),old);});
    const auto birth=level1EnemyPrefixOperations(type3test::input(1),profile);
    type3test::sourceEqual(birth,1);
    const auto& owner=birth.prefix.entities[6];const auto& shot=birth.prefix.projectiles[0];
    assert(owner.stateBytes4dTo52[5]==0 && owner.behaviorTimer==1 && owner.weaponCooldown==0);
    assert(owner.weaponSlotTimers[0]==60); //Class1 has no Type11 post-fire slot decrement.
    assert(shot.type==20 && shot.ownerAllocationIndex==6 && !shot.ownerIsPlayer);
    assert(shot.stateBytes4dTo54[1]==60 && shot.forwardVelocity==500 && shot.collisionRadius==134);
    assert(birth.audioEvents.size()==1 && birth.audioEvents[0].eventId==15);
    assert(birth.prefix.activeOrder==birth.prefix.allocationOrder);

    for(const auto state:{1U,5U}) {
        const auto step=level1EnemyPrefixOperations(choice(state,0x0170),profile);
        assert(step.prefix.entities[6].forwardVelocity==216 && step.prefix.entities[6].yawVelocity==-624);
    }
    auto state1=choice(1,0);state1.entities[6].behaviorTimer=1;
    assert(level1EnemyPrefixOperations(state1,profile).prefix.entities[6].stateBytes4dTo52[5]==0);
    for(const auto threshold:{47U,48U,127U,128U}) {
        const auto step=level1EnemyPrefixOperations(choice(0,static_cast<std::uint16_t>(threshold)),profile);
        assert(step.prefix.entities[6].forwardVelocity==(threshold>=48 && threshold<128 ? 0:216));
    }
    for(const auto state:{2U,3U}) {
        auto aim=choice(state,255);aim.entities[6].yaw=0;
        const auto step=level1EnemyPrefixOperations(aim,profile);
        assert(step.prefix.entities[6].yawVelocity==-399);
        assert(step.prefix.entities[6].forwardVelocity==(state==2 ? 0:216));
    }
    for(const auto word:{0U,0x0200U}) {
        const auto step=level1EnemyPrefixOperations(choice(4,static_cast<std::uint16_t>(word)),profile);
        assert(step.prefix.entities[6].forwardVelocity==-72 && step.prefix.entities[6].behaviorTimer==63);
        assert(step.prefix.entities[6].yawVelocity==(word==0 ? 768:-768));
        auto returning=choice(4,0);returning.entities[6].forwardVelocity=-72;returning.entities[6].behaviorTimer=1;
        const auto returned=level1EnemyPrefixOperations(returning,profile);
        assert(returned.prefix.entities[6].forwardVelocity==216 && returned.prefix.entities[6].stateBytes4dTo52[5]==0);
    }

    //An already-retired actual3 owner remains the canonical owner of its live20.
    auto live=type20test::carry(birth.prefix);
    live.entities[6].stateBytes4dTo52[1]=0;
    const Level1ObjectRef ownerRef{Level1PresentationKind::entity,6};
    live.allocationOrder.erase(std::find(live.allocationOrder.begin(),live.allocationOrder.end(),ownerRef));
    live.activeOrder.erase(std::find(live.activeOrder.begin(),live.activeOrder.end(),ownerRef));
    live.projectiles[0].position={0,10000,0};live.projectiles[0].yaw=0;
    for(unsigned tick=0;tick<60;++tick) {
        const auto step=advance(live,static_cast<std::uint16_t>(tick+2));
        assert(step.damage.armor==680 && step.damage.combat.audioEvents.empty());
        assert(step.damage.combat.prefix.projectiles[0].stateBytes4dTo54[1]==59-tick);
        live=type20test::carry(step.damage.combat.prefix);
    }
    assert(!live.projectiles[0].active());
    assert(std::find(live.allocationOrder.begin(),live.allocationOrder.end(),
        Level1ObjectRef{Level1PresentationKind::projectile,0})==live.allocationOrder.end());
    auto reuse=type3test::input(1);reuse.projectiles=live.projectiles;
    const auto reused=level1EnemyPrefixOperations(reuse,profile);
    assert(reused.prefix.projectiles[0].type==20 && reused.prefix.projectiles[0].active());
    assert(reused.prefix.projectiles[0].presenterDistance==live.projectiles[0].presenterDistance);
    assert(reused.prefix.projectiles[0].targetHeading==live.projectiles[0].targetHeading);
    Level1PrimaryFireInput fire{live.player,live.projectiles,0,{},0,7,{},false,{},
        {44,0,134},Level1ProjectileAllocationDomain::retainedAirborne};
    fire.membership=Level1PrimaryFireMembership::periodicSpawned;
    assert(level1PrimaryFireOperations(fire).launch->poolSlot==0);
    fire.allocationDomain=Level1ProjectileAllocationDomain::retained;
    rejects([&]{level1PrimaryFireOperations(fire);});
    auto hit=type20test::carry(birth.prefix);hit.entities[6].stateBytes4dTo52[1]=0;
    hit.allocationOrder.erase(std::find(hit.allocationOrder.begin(),hit.allocationOrder.end(),ownerRef));
    hit.activeOrder.erase(std::find(hit.activeOrder.begin(),hit.activeOrder.end(),ownerRef));
    hit.projectiles[0].position={0,-500,0};hit.projectiles[0].yaw=0;
    const auto damaged=advance(hit);
    assert(damaged.damage.armor==624 && damaged.damage.fadeTicks==8);
    assert(!damaged.damage.combat.prefix.projectiles[0].active());
    for(const auto bad:{0,1,2,3,4}) {
        auto invalid=type3test::input(0);
        if(bad==0) {invalid.entities[6].forwardVelocity=225;}
        else if(bad==1) {invalid.entities[6].yawVelocity=769;}
        else if(bad==2) {invalid.entities[6].roll=1;}
        else if(bad==3) {invalid.entities[6].stateBytes4dTo52[4]=3;}
        else {invalid.entities[6].weaponSlotCount=2;}
        rejects([&]{level1EnemyPrefixOperations(invalid,profile);});
    }
    auto invalid=type20test::carry(birth.prefix);invalid.projectiles[0].position.z=1000;
    rejects([&]{level1EnemyPrefixOperations(invalid,profile);});

    //3A47/3A49 halve the radius sum: (723+770)/2=746, inclusive on each axis.
    auto touching=type3test::input(0);touching.entities[6].position={0,700,0};
    assert(actorOverlapOperations(touching.player,touching.entities[6]).overlaps);
    auto contacts=profile;contacts.contactDomain=Level1ActorContactDomain::ordinaryPlayer;
    const auto collided=level1EnemyPrefixOperations(touching,contacts);
    assert(collided.prefix.player.forwardVelocity==-10 && collided.prefix.impulse==-10);
    assert(collided.prefix.entities[6].forwardVelocity==-72 && collided.prefix.entities[6].behaviorTimer==63);
    assert(collided.prefix.entities[6].yawVelocity==768 && !collided.prefix.contacts.empty());
    assert(std::count_if(collided.prefix.operations.begin(),collided.prefix.operations.end(),
        [](const auto op){return op.kind==NormalWorkKind::gameplaySolidContactEvent;})==1);
    rejects([&]{level1SolidContactOperations(touching.player,touching.entities[6],6);});

    Level1TargetHitInput target{};target.target=type3test::input(0).entities[6];
    target.targetIndex=6;target.damage=1;target.activeEnemies=3;
    target.allocationOrder=type3test::input(0).allocationOrder;target.bonus={3,2};
    const auto third=level1TargetHitOperations(target,Level1TargetHitDomain::periodicClass1);
    assert(third.destroyed && third.targetHealth==0 && third.activeEnemies==2);
    assert(third.bonus.lastType==3 && third.bonus.consecutive==3);
    assert(third.effect && third.effect->age==14 && third.effect->endAge==17);
    target.bonus={3,0};
    const auto first=level1TargetHitOperations(target,Level1TargetHitDomain::periodicClass1);
    assert(instructions(third)==instructions(first)+2); //90CD CMP /90D2 JZ for zero DS1622[3].
    rejects([&]{level1TargetHitOperations(target,Level1TargetHitDomain::initialOrdinary);});

    //Whole existing-primary path owns1176,7970 score20 and late source list refresh.
    auto primary=type3test::input(0);
    auto& p=primary.projectiles[0];p.type=23;p.collisionRadius=134;p.position={0,11500,0};
    p.forwardVelocity=500;p.stateBytes4dTo54[1]=64;p.ownerIsPlayer=true;
    primary.allocationOrder.push_back({Level1PresentationKind::projectile,0});
    primary.activeOrder.insert(primary.activeOrder.begin(),{Level1PresentationKind::projectile,0});
    const auto killed=level1TargetPrefixOperations({primary,{}, {3,2},3,0},targetProfile(1),
        font(),CockpitPageState::fromRecoveredMenu());
    assert(killed.targetHits.size()==1 && killed.targetHits[0].targetIndex==6);
    assert(killed.score==20 && killed.scoreSteps.size()==1 && killed.activeEnemies==2);
    assert(killed.bonus.lastType==3 && killed.bonus.consecutive==3);
    assert(killed.damage.combat.prefix.entities.size()==7 && !killed.damage.combat.prefix.entities[6].active());
    retainedEmitterReuseCases();
}
