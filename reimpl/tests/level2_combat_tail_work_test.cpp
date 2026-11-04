#include "stellar7/frame_tail_work.hpp"
#include "stellar7/weapon_lamp_work.hpp"
#include <cassert>
#include <stdexcept>
using namespace stellar7;
namespace {
template<class Action>void rejects(Action action) {bool rejected=false;try{action();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
void same(const std::vector<NormalWorkOperation>& a,const std::vector<NormalWorkOperation>& b) {
    assert(a.size()==b.size());for(std::size_t i=0;i<a.size();++i){assert(a[i].kind==b[i].kind && a[i].count==b[i].count && a[i].value==b[i].value);}
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
    FrameTailState state{16,{71,-33,0},0,120,{0,0,0},{}};
    for(const auto type:{8,8,8,13,13,30,30}) {state.actors.push_back({static_cast<std::int16_t>(type),{8192,8192,0}});}
    FrameTailGuards guards{};guards.armor=680;guards.missionActive=true;
    guards.periodic=FrameTailPeriodicInput{1,5,5,12,0,48,110,0};guards.heldKeys[4]=true;
    const auto old=level2ControlFrameTailOperations(state,guards);
    const auto shared=level2CombatFrameTailOperations(state,guards);
    same(old.operations,shared.operations);assert(shared.state.scheduler==FrameTailScheduler::zeroThresholdDraw);
    auto primary=input();primary.player.type=0;
    primary.membership=Level1PrimaryFireMembership::level2Combat;primary.combatEffects=Level1CombatEffectMembership{7,7,0};
    primary.pool[0].type=20;primary.pool[0].flags=4;primary.pool[0].yawVelocity=80;
    primary.pool[0].stateBytes4dTo54={253,0,1,27,8,41,61,62};
    const auto fire=level1PrimaryFireOperations(primary);assert(fire.launch && fire.launch->projectile.flags==4);
    const auto lamp=weaponLampOperations(0xffff,WeaponLampState::unavailable,
        {24,5,std::vector<std::uint8_t>(120,9)},CockpitPageState::fromRecoveredMenu());
    guards.heldKeys[8]=true;
    const auto fired=level2CombatFrameTailOperations(state,guards,Level1TailFireAction{fire,lamp.operations});
    assert(fired.state.weaponTimers==(std::array<std::uint8_t,3>{8,31,0}));
    std::size_t markers=0;
    for(const auto& operation:fired.operations) {
        if(operation.kind==NormalWorkKind::gameplayLevel2PrimaryFireEvent){assert(operation.value==14 && operation.count==1);++markers;}
        assert(operation.kind!=NormalWorkKind::gameplayPrimaryFireEvent);
    }
    assert(markers==1 && fired.state.actors.size()==7 && fired.state.loopTick==17);
    rejects([&]{level2ControlFrameTailOperations(state,guards);});
    rejects([&]{level2CombatFrameTailOperations(state,guards);});
    auto invalid=primary;invalid.membership=Level1PrimaryFireMembership::level2Constructor;invalid.combatEffects.reset();
    rejects([&]{level1PrimaryFireOperations(invalid);});
    guards.heldKeys[8]=false;guards.periodic->activeEnemies=4;state.actors.erase(state.actors.begin());
    assert(level2CombatFrameTailOperations(state,guards).state.scheduler==FrameTailScheduler::zeroThresholdDraw);
    rejects([&]{level2ControlFrameTailOperations(state,guards);});
    guards.periodic->activeEnemies=2;guards.periodic->spawnThreshold=20;
    rejects([&]{level2CombatFrameTailOperations(state,guards);});
    guards.periodic.reset();state.loopTick=41;state.weaponTimers={1,0,1};
    const auto ready=weaponLampOperations(0,WeaponLampState::ready,
        {24,5,std::vector<std::uint8_t>(120,7)},CockpitPageState::fromRecoveredMenu());
    const auto alreadyReady=weaponLampOperations(ready.dirty,WeaponLampState::ready,
        {24,5,std::vector<std::uint8_t>(120,7)},ready.pages);
    const auto expired=level2CombatFrameTailOperations(state,guards,{}, {ready.operations,alreadyReady.operations});
    assert(expired.state.weaponTimers==(std::array<std::uint8_t,3>{0,0,0}));
    rejects([&]{level2CombatFrameTailOperations(state,guards,{}, {ready.operations});});
}
