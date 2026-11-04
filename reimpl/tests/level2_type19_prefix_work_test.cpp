#include "stellar7/level2_projectile_prefix_work.hpp"
#include "level2_control_prefix_fixture.hpp"
#include "score_work_fixture.inc"
#include <algorithm>
#include <tuple>

using namespace stellar7;
namespace {
constexpr std::size_t ownerIndex = 1;
constexpr Level2HostileProjectileModel descriptor{44,{0,441}};
using Op = std::tuple<NormalWorkKind,std::uint64_t,std::uint32_t>;

std::vector<Op> canonical(const std::vector<NormalWorkOperation>& operations) {
    std::vector<Op> result;
    for (const auto& operation : operations) {
        if (!result.empty() && operation.kind==NormalWorkKind::instructions
            && std::get<0>(result.back())==operation.kind) {
            std::get<1>(result.back())+=operation.count;
        } else { result.emplace_back(operation.kind,operation.count,operation.value); }
    }
    return result;
}

bool contains(const std::vector<Level1ObjectRef>& order,const Level1ObjectRef object) {
    return std::find(order.begin(),order.end(),object)!=order.end();
}

Level1TargetPrefixInput input() {
    const auto actors=fixture();
    Level1TargetPrefixInput result{};
    auto& prefix=result.prefix;
    prefix.player=actors.player;prefix.entities=actors.actors;prefix.active=true;
    prefix.lastListCamera=prefix.player.position;
    for (std::size_t index=0;index<prefix.entities.size();++index) {
        auto& actor=prefix.entities[index];actor.allocationIndex=index;
        //Synthetic pure fixture: retain all physical records, one live Type8.
        actor.stateBytes4dTo52[1]=0;actor.weaponSlotCount=0;
    }
    auto& owner=prefix.entities[ownerIndex];
    assert(owner.type==8);
    owner.stateBytes4dTo52[1]=2;owner.position={0,-12000,0};
    owner.yaw=0;owner.yawVelocity=0;owner.weaponSlotCount=2;owner.weaponCooldown=0;
    owner.weaponSlotTimers={};
    prefix.allocationOrder={{Level1PresentationKind::entity,ownerIndex}};
    prefix.activeOrder=prefix.allocationOrder;result.activeEnemies=1;
    for (std::size_t slot=0;slot<prefix.projectiles.size();++slot) {
        prefix.projectiles[slot].poolSlot=slot;
        prefix.projectiles[slot].stateBytes4dTo54[0]=253;
    }
    return result;
}

Level1TargetPrefixWork execute(const Level1TargetPrefixInput& before,
    const BitmapFont& scoreFont,const CockpitPageState& pages) {
    return level2ProjectilePrefixOperations(before,{213,680,0,descriptor},scoreFont,pages);
}

Level1TargetPrefixInput carry(const Level1TargetPrefixWork& work) {
    Level1TargetPrefixInput result{};auto& prefix=result.prefix;
    const auto& source=work.damage.combat.prefix;
    prefix.player=source.player;prefix.entities=source.entities;prefix.projectiles=source.projectiles;
    prefix.allocationOrder=source.allocationOrder;prefix.activeOrder=source.activeOrder;
    prefix.lastListCamera=source.lastListCamera;prefix.refreshRequested=source.refreshRequested;
    prefix.rng=source.rng;prefix.fadeTicks=work.damage.fadeTicks;prefix.active=true;
    result.effects=work.effects;result.bonus=work.bonus;
    result.activeEnemies=work.activeEnemies;result.score=work.score;
    return result;
}

void birthAndStrictDefault(const BitmapFont& scoreFont,const CockpitPageState& pages) {
    auto before=input();
    auto& physical=before.prefix.projectiles[0];
    physical.flags=4;physical.type=23;physical.collisionRadius=134;
    physical.ownerIsPlayer=true;physical.presenterDistance=123456;physical.targetHeading=456;
    physical.stateBytes4dTo54={253,0,7,91,6,92,93,94};
    rejects([&]{level2ProjectilePrefixOperations(before,{213,680,0},scoreFont,pages);});
    const auto work=execute(before,scoreFont,pages);
    const auto& source=work.damage.combat.prefix;const auto& shot=source.projectiles[0];
    assert(shot.type==19 && shot.collisionRadius==441 && shot.forwardVelocity==500);
    assert(shot.flags==4 && shot.stateBytes4dTo54[1]==84 && shot.stateBytes4dTo54[2]==0);
    assert(shot.stateBytes4dTo54[3]==91 && shot.stateBytes4dTo54[4]==0);
    assert(shot.stateBytes4dTo54[5]==92 && shot.stateBytes4dTo54[6]==93 && shot.stateBytes4dTo54[7]==94);
    assert(shot.position==source.entities[ownerIndex].position && shot.yaw==0);
    assert(!shot.ownerIsPlayer && shot.ownerAllocationIndex==ownerIndex);
    assert(shot.presenterDistance==123456 && shot.targetHeading==456); //Born after OLD presenter pass.
    assert(source.entities[ownerIndex].weaponCooldown==12);
    assert(source.entities[ownerIndex].weaponSlotTimers[0]==84 && source.entities[ownerIndex].weaponSlotTimers[1]==0);
    assert(contains(source.activeOrder,{Level1PresentationKind::projectile,0}) && !source.refreshRequested);
    assert(work.damage.combat.audioEvents.size()==1 && work.damage.combat.audioEvents[0].eventId==2);
    assert(std::count_if(source.operations.begin(),source.operations.end(),[](const auto& op){
        return op.kind==NormalWorkKind::gameplayLevel2EnemyFireEvent && op.count==1 && op.value==2;
    })==1);
    assert(work.targetHits.empty() && work.scoreSteps.empty() && work.damage.paletteSteps.empty());

    auto quiet=carry(work);quiet.prefix.entities[ownerIndex].weaponSlotCount=0;
    rejects([&]{level2ProjectilePrefixOperations(quiet,{214,680,0},scoreFont,pages);});
    auto bad=descriptor;bad.model19.baseRadius=134;
    rejects([&]{level2ProjectilePrefixOperations(before,{213,680,0,bad},scoreFont,pages);});
    bad=descriptor;bad.firstRegistryCapacity=256;
    rejects([&]{level2ProjectilePrefixOperations(before,{213,680,0,bad},scoreFont,pages);});
    bad=descriptor;bad.model19.scaleShift=1;
    rejects([&]{level2ProjectilePrefixOperations(before,{213,680,0,bad},scoreFont,pages);});
}

void lifetimeAndReuse(const BitmapFont& scoreFont,const CockpitPageState& pages) {
    auto before=carry(execute(input(),scoreFont,pages));
    //Synthetic position control after own birth; avoids contacts, not runtime ancestry proof.
    before.prefix.player.position={20000,20000,0};
    const auto initial=before.prefix.projectiles[0].position;
    for (int remaining=83;remaining>=0;--remaining) {
        const auto work=execute(before,scoreFont,pages);
        const auto& source=work.damage.combat.prefix;
        const auto& shot=source.projectiles[0];
        assert(shot.stateBytes4dTo54[1]==remaining);
        assert((shot.position==FixedPosition{initial.x,initial.y+(84-remaining)*500,0}));
        assert(contains(source.allocationOrder,{Level1PresentationKind::projectile,0})==(remaining!=0));
        assert(contains(source.activeOrder,{Level1PresentationKind::projectile,0})==(remaining!=0));
        assert(work.damage.armor==680 && work.damage.combat.audioEvents.empty());
        assert(work.targetHits.empty() && work.scoreSteps.empty());
        before=carry(work);
    }
    auto& owner=before.prefix.entities[ownerIndex];
    before.prefix.player.position={};owner.yaw=0;owner.yawVelocity=0;owner.weaponCooldown=0;owner.weaponSlotTimers={};
    const auto cache=before.prefix.projectiles[0].presenterDistance;
    const auto reused=execute(before,scoreFont,pages);
    assert(reused.damage.combat.prefix.projectiles[0].stateBytes4dTo54[1]==84);
    assert(reused.damage.combat.prefix.projectiles[0].presenterDistance==cache);
    assert(reused.damage.combat.prefix.allocationOrder.back()==(Level1ObjectRef{Level1PresentationKind::projectile,0}));
}

void contactAndNegatives(const BitmapFont& scoreFont,const CockpitPageState& pages) {
    const auto born=execute(input(),scoreFont,pages);
    auto before=carry(born);auto& shot=before.prefix.projectiles[0];
    before.prefix.entities[ownerIndex].weaponSlotCount=0;
    shot.position={0,-500,0};shot.stateBytes4dTo54[1]=1;
    const auto hit=execute(before,scoreFont,pages);
    assert(hit.damage.armor==600 && hit.damage.fadeTicks==8 && hit.damage.paletteSteps.size()==1);
    assert(hit.damage.combat.audioEvents.size()==1 && hit.damage.combat.audioEvents[0].eventId==6);
    assert(!hit.damage.combat.prefix.projectiles[0].active());
    assert(hit.targetHits.empty() && hit.scoreSteps.empty() && hit.score==before.score);

    auto fading=carry(hit);
    for (int remaining=7;remaining>=0;--remaining) {
        const auto next=level2ProjectilePrefixOperations(fading,{214,600,0,descriptor},scoreFont,pages);
        assert(next.damage.armor==600 && next.damage.fadeTicks==remaining);
        assert(next.damage.paletteSteps.size()==1 && next.damage.combat.audioEvents.empty());
        const auto& palette=next.damage.paletteSteps[0].request;
        assert(palette.start==0 && palette.count==256 && palette.targetColor==238 && palette.intensity==63-remaining*4);
        fading=carry(next);
    }

    auto foreign=carry(born);
    foreign.prefix.entities[ownerIndex].weaponSlotCount=0;
    foreign.prefix.player.position={10000,0,0};
    auto& fixed=foreign.prefix.entities[4];assert(fixed.type==30);
    fixed.position={};fixed.stateBytes4dTo52[1]=100;
    foreign.prefix.allocationOrder.push_back({Level1PresentationKind::entity,4});
    foreign.prefix.activeOrder.push_back({Level1PresentationKind::entity,4});
    foreign.prefix.projectiles[0].position={0,-500,0};
    const auto stopped=execute(foreign,scoreFont,pages);
    assert(!stopped.damage.combat.prefix.projectiles[0].active());
    assert(stopped.damage.combat.prefix.entities[4].stateBytes4dTo52[1]==100);
    assert(stopped.targetHits.empty() && stopped.scoreSteps.empty() && stopped.damage.combat.audioEvents.empty());

    for (const auto mutation : {0,1,2,3,4,5}) {
        auto bad=carry(born);auto& value=bad.prefix.projectiles[0];
        if (mutation==0) {value.ownerAllocationIndex=0;} //Actual Type13 record, not a Type8 numeric role.
        if (mutation==1) {value.collisionRadius=134;}
        if (mutation==2) {value.stateBytes4dTo54[1]=85;}
        if (mutation==3) {value.pitch=1;}
        if (mutation==4) {value.stateBytes4dTo54[2]=1;}
        if (mutation==5) {value.forwardVelocity=499;}
        rejects([&]{execute(bad,scoreFont,pages);});
    }
    auto pending=carry(born);pending.prefix.activeOrder.pop_back();pending.prefix.refreshRequested=true;
    rejects([&]{execute(pending,scoreFont,pages);}); //Own birth already completed late3768; no pending hostile import.
}

void poolFullAndPrimaryCoexistence(const BitmapFont& scoreFont,const CockpitPageState& pages) {
    auto full=input();
    for (std::size_t slot=0;slot<full.prefix.projectiles.size();++slot) {
        auto& shot=full.prefix.projectiles[slot];
        shot.type=19;shot.collisionRadius=441;shot.forwardVelocity=500;
        shot.position={-10000,-20000-static_cast<std::int32_t>(slot)*1000,0};
        shot.ownerAllocationIndex=ownerIndex;shot.stateBytes4dTo54[1]=3;
        full.prefix.allocationOrder.push_back({Level1PresentationKind::projectile,slot});
        full.prefix.activeOrder.push_back({Level1PresentationKind::projectile,slot});
    }
    const auto rejected=execute(full,scoreFont,pages);
    assert(rejected.damage.combat.prefix.allocationOrder.size()==full.prefix.allocationOrder.size());
    assert(rejected.damage.combat.audioEvents.empty());
    assert(rejected.damage.combat.prefix.entities[ownerIndex].weaponCooldown==0);
    assert(rejected.damage.combat.prefix.entities[ownerIndex].weaponSlotTimers[0]==0);
    assert(rejected.damage.combat.prefix.entities[ownerIndex].weaponSlotTimers[1]==0);
    for (const auto& shot:rejected.damage.combat.prefix.projectiles) {assert(shot.stateBytes4dTo54[1]==2);}

    auto mixed=input();auto& primary=mixed.prefix.projectiles[1];
    primary.type=23;primary.collisionRadius=134;primary.forwardVelocity=500;
    primary.position={-10000,0,0};primary.ownerIsPlayer=true;primary.stateBytes4dTo54[1]=64;
    mixed.prefix.allocationOrder.push_back({Level1PresentationKind::projectile,1});
    mixed.prefix.activeOrder.push_back({Level1PresentationKind::projectile,1});
    const auto work=execute(mixed,scoreFont,pages);
    const auto& pool=work.damage.combat.prefix.projectiles;
    assert(pool[0].type==19 && pool[0].stateBytes4dTo54[1]==84);
    assert(pool[1].type==23 && pool[1].stateBytes4dTo54[1]==63 && pool[1].ownerIsPlayer);
    assert((pool[1].position==FixedPosition{-10000,500,0}));
    assert(work.damage.combat.audioEvents.size()==1 && work.damage.combat.audioEvents[0].eventId==2);
}

void unchangedNoLaunch(const BitmapFont& scoreFont,const CockpitPageState& pages) {
    auto before=input();before.prefix.entities[ownerIndex].weaponSlotCount=0;
    const auto strict=level2ProjectilePrefixOperations(before,{213,680,0},scoreFont,pages);
    const auto enabled=execute(before,scoreFont,pages);
    assert(canonical(strict.damage.combat.prefix.operations)==canonical(enabled.damage.combat.prefix.operations));
    assert(fields(strict.damage.combat.prefix.entities[ownerIndex])==fields(enabled.damage.combat.prefix.entities[ownerIndex]));
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    const auto scoreFont=font();const auto pages=CockpitPageState::fromRecoveredMenu();
    birthAndStrictDefault(scoreFont,pages);
    lifetimeAndReuse(scoreFont,pages);
    contactAndNegatives(scoreFont,pages);
    unchangedNoLaunch(scoreFont,pages);
    poolFullAndPrimaryCoexistence(scoreFont,pages);
}
