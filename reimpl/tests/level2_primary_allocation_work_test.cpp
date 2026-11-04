#include "stellar7/level1_primary_fire_work.hpp"
#include <cassert>
#include <stdexcept>

using namespace stellar7;
namespace {
template<class Function> void rejects(Function function) {
    bool rejected=false;try {function();} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
}
bool same(const std::vector<NormalWorkOperation>& a,const std::vector<NormalWorkOperation>& b) {
    if(a.size()!=b.size()) {return false;}
    for(std::size_t i=0;i<a.size();++i) {
        if(a[i].kind!=b[i].kind || a[i].count!=b[i].count || a[i].value!=b[i].value) {return false;}
    }
    return true;
}
Level1PrimaryFireInput input() {
    Level1PrimaryFireInput result{};result.player.type=31;result.player.collisionRadius=723;
    result.player.position={71,-33,0};result.player.yaw=0x1780;
    result.allocationCount=7;result.model={44,0,134};
    result.membership=Level1PrimaryFireMembership::level2Constructor;
    result.allocationDomain=Level1ProjectileAllocationDomain::retained;
    for(std::size_t slot=0;slot<result.pool.size();++slot) {
        result.pool[slot].poolSlot=slot;result.pool[slot].stateBytes4dTo54[0]=253;
    }
    return result;
}
}
int main() {
    auto fresh=input();
    const auto first=level1PrimaryFireOperations(fresh);
    assert(first.launch && first.launch->poolSlot==0 && first.launch->allocationIndex==7
        && first.mainCooldown==9 && first.weaponTimers[0]==32 && first.refreshRequested);
    auto ordinary=fresh;ordinary.membership=Level1PrimaryFireMembership::initialSix;
    rejects([&] {level1PrimaryFireOperations(ordinary);});
    ordinary.allocationCount=6;
    const auto shared=level1PrimaryFireOperations(ordinary);
    assert(same(first.beforeEvent14,shared.beforeEvent14)
        && same(first.afterEvent14BeforeLamp,shared.afterEvent14BeforeLamp)
        && same(first.afterLamp,shared.afterLamp));

    auto carried=fresh;
    auto& old=carried.pool[0];old.flags=4;old.type=22;old.collisionRadius=441;
    old.pitchVelocity=17;old.rollVelocity=-29;old.yawVelocity=81;
    old.xVelocity=-123;old.yVelocity=57;old.zVelocity=98;
    old.presenterDistance=4093;old.targetHeading=65400;
    old.stateBytes4dTo54={253,0,1,27,2,41,61,62};
    const auto reused=level1PrimaryFireOperations(carried);
    assert(reused.launch && reused.launch->poolSlot==0);
    const auto& shot=reused.launch->projectile;
    assert(shot.flags==4 && shot.type==23 && shot.collisionRadius==134 && shot.position==carried.player.position
        && shot.yaw==carried.player.yaw && shot.forwardVelocity==500 && shot.zVelocity==0 && shot.ownerIsPlayer);
    assert(shot.pitchVelocity==17 && shot.rollVelocity==-29 && shot.yawVelocity==81
        && shot.xVelocity==-123 && shot.yVelocity==57 && shot.presenterDistance==4093 && shot.targetHeading==65400);
    assert((shot.stateBytes4dTo54==std::array<std::uint8_t,8>{253,64,0,27,0,41,61,62}));
    assert(same(first.beforeEvent14,reused.beforeEvent14));
    carried.membership=Level1PrimaryFireMembership::initialSix;carried.allocationCount=6;
    rejects([&] {level1PrimaryFireOperations(carried);});
    fresh.mainCooldown=8;
    const auto busy=level1PrimaryFireOperations(fresh);
    assert(busy.outcome==Level1PrimaryFireOutcome::cooldown && !busy.launch
        && busy.mainCooldown==8 && busy.afterEvent14BeforeLamp.empty() && busy.afterLamp.empty());
    fresh=input();fresh.allocationCount=8;
    rejects([&] {level1PrimaryFireOperations(fresh);});
    fresh=input();fresh.model.baseRadius=862;
    rejects([&] {level1PrimaryFireOperations(fresh);});
}
