#include "stellar7/level2_projectile_prefix_work.hpp"
#include "stellar7/frame_tail_work.hpp"
#include "stellar7/weapon_lamp_work.hpp"
#include "level2_control_prefix_fixture.hpp"
#include "score_work_fixture.inc"
#include <algorithm>
#include <tuple>
#include <string>
#include <limits>

using namespace stellar7;
namespace {
constexpr auto lifecycle=Level1PlayerCombatDomain::expiryAndLoss;
constexpr Level2HostileProjectileModel hostile{44,{0,441}};
constexpr std::size_t ownerIndex=1;
using Op=std::tuple<NormalWorkKind,std::uint64_t,std::uint32_t>;

std::vector<Op> canonical(const std::vector<NormalWorkOperation>& operations) {
    std::vector<Op> result;
    for(const auto& operation:operations) {
        if(!result.empty() && operation.kind==NormalWorkKind::instructions
            && std::get<0>(result.back())==operation.kind) {std::get<1>(result.back())+=operation.count;}
        else {result.emplace_back(operation.kind,operation.count,operation.value);}
    }
    return result;
}

Level1TargetPrefixInput prefixInput() {
    const auto before=fixture();Level1TargetPrefixInput result{};auto& input=result.prefix;
    input.player=before.player;input.entities=before.actors;input.active=true;input.rng=StellarRng{};
    for(std::size_t index=0;index<input.entities.size();++index) {
        auto& actor=input.entities[index];actor.allocationIndex=index;
        actor.stateBytes4dTo52[1]=0;actor.weaponSlotCount=0;
    }
    auto& owner=input.entities[ownerIndex];assert(owner.type==8);
    owner.stateBytes4dTo52[1]=2;owner.position={10000,10000,0};
    input.allocationOrder={{Level1PresentationKind::entity,ownerIndex}};
    input.activeOrder=input.allocationOrder;result.activeEnemies=1;result.score=1350;
    for(std::size_t slot=0;slot<input.projectiles.size();++slot) {
        input.projectiles[slot].poolSlot=slot;input.projectiles[slot].stateBytes4dTo54[0]=253;
    }
    return result;
}

void playerShot(Level1TargetPrefixInput& input,const std::size_t slot) {
    auto& shot=input.prefix.projectiles[slot];shot.type=19;shot.collisionRadius=441;
    shot.position={0,-500,0};shot.forwardVelocity=500;shot.ownerAllocationIndex=ownerIndex;
    shot.stateBytes4dTo54[1]=1;
    input.prefix.allocationOrder.push_back({Level1PresentationKind::projectile,slot});
    input.prefix.activeOrder.push_back({Level1PresentationKind::projectile,slot});
}

void prefixDamage(const BitmapFont& scoreFont,const CockpitPageState& pages) {
    auto input=prefixInput();
    for(const auto armor:{199,1,0}) {
        rejects([&]{level2ProjectilePrefixOperations(input,{376,static_cast<std::int16_t>(armor),0,hostile},scoreFont,pages);});
        const auto work=level2ProjectilePrefixOperations(input,{376,static_cast<std::int16_t>(armor),0,hostile,lifecycle},scoreFont,pages);
        assert(work.damage.armor==armor && work.damage.combat.audioEvents.empty());
    }
    playerShot(input,0);
    for(const auto armor:{184,80,79}) {
        const auto work=level2ProjectilePrefixOperations(input,{376,static_cast<std::int16_t>(armor),0,hostile,lifecycle},scoreFont,pages);
        assert(work.damage.armor==armor-80 && work.damage.fadeTicks==8);
        assert(!work.damage.combat.prefix.projectiles[0].active());
        assert((work.damage.combat.prefix.projectiles[0].position==FixedPosition{}));
        assert(work.damage.combat.audioEvents.size()==1 && work.damage.combat.audioEvents[0].eventId==6);
        assert(work.damage.paletteSteps.size()==1 && work.targetHits.empty() && work.scoreSteps.empty());
        assert(work.score==1350 && work.damage.combat.prefix.rng.calls()==input.prefix.rng.calls());
    }
    for(std::size_t slot=1;slot<input.prefix.projectiles.size();++slot) {playerShot(input,slot);}
    rejects([&]{level2ProjectilePrefixOperations(input,{376,200,0,hostile},scoreFont,pages);});
    const auto multiple=level2ProjectilePrefixOperations(input,{376,200,0,hostile,lifecycle},scoreFont,pages);
    assert(multiple.damage.armor==-440 && multiple.damage.combat.audioEvents.size()==8);
    assert(multiple.damage.paletteSteps.size()==8 && multiple.damage.combat.prefix.activeOrder.size()==1);
    for(const auto& shot:multiple.damage.combat.prefix.projectiles) {
        assert(!shot.active() && shot.position==FixedPosition{}); //All OLD continue after first loss.
    }
    //Synthetic simultaneous admitted inventory reaches the conservative source bound.
    for(const auto index:{0U,3U}) {
        auto& actor=input.prefix.entities[index];assert(actor.type==13);
        actor.stateBytes4dTo52[1]=1;actor.position={0,700,0};
        actor.xVelocity=0;actor.yVelocity=-112;actor.yawVelocity=0;
        input.prefix.allocationOrder.insert(input.prefix.allocationOrder.begin()+1,{Level1PresentationKind::entity,index});
        input.prefix.activeOrder.insert(input.prefix.activeOrder.begin()+1,{Level1PresentationKind::entity,index});
        ++input.activeEnemies;
    }
    const auto bounded=level2ProjectilePrefixOperations(input,{376,0,0,hostile,lifecycle},scoreFont,pages);
    assert(bounded.damage.armor==-896 && bounded.activeEnemies==1);
    assert(bounded.damage.combat.audioEvents.size()==12 && bounded.damage.paletteSteps.size()==10);
    assert(bounded.damage.combat.prefix.activeOrder.size()==1);
    assert(bounded.damage.combat.prefix.entities[0].stateBytes4dTo52[1]==0);
    assert(bounded.damage.combat.prefix.entities[3].stateBytes4dTo52[1]==0);
    for(const auto& shot:bounded.damage.combat.prefix.projectiles) {assert(!shot.active() && shot.position==FixedPosition{});}
    assert(bounded.score==1350 && bounded.targetHits.empty() && bounded.scoreSteps.empty());
    assert(bounded.damage.combat.prefix.rng.calls()==input.prefix.rng.calls());
    rejects([&]{level2ProjectilePrefixOperations(input,{376,-1,0,hostile,lifecycle},scoreFont,pages);});
    rejects([&]{level2ProjectilePrefixOperations(input,{376,681,0,hostile,lifecycle},scoreFont,pages);});
    rejects([&]{level2ProjectilePrefixOperations(input,{376,680,0,hostile,static_cast<Level1PlayerCombatDomain>(2)},scoreFont,pages);});
}

void periodicThreeLifts(const BitmapFont& scoreFont,const CockpitPageState& pages) {
    auto before=prefixInput();
    before.prefix.entities[ownerIndex].stateBytes4dTo52[1]=0;
    before.prefix.allocationOrder.clear();before.prefix.activeOrder.clear();before.activeEnemies=0;
    for(auto& actor:before.prefix.entities) {
        actor.animatedModelPresenterEnabled=false;actor.renderProfile=WorldActorRenderProfile::noHandles;
    }
    const auto liftTemplate=before.prefix.entities[0];
    for(const auto index:{0U,3U,6U}) {
        auto& actor=before.prefix.entities[index];actor=liftTemplate;actor.allocationIndex=index;
        actor.stateBytes4dTo52={255,1,0,0,4,0};actor.collisionRadius=559;
        actor.position={static_cast<std::int32_t>(12000+index*1000),14000,0};
        actor.xVelocity=0;actor.yVelocity=0;actor.yawVelocity=0;actor.weaponSlotCount=0;
        actor.collisionContactAllocationIndex=std::numeric_limits<std::size_t>::max();actor.collisionContactIsPlayer=false;
        before.prefix.allocationOrder.push_back({Level1PresentationKind::entity,index});
        before.prefix.activeOrder.push_back({Level1PresentationKind::entity,index});++before.activeEnemies;
    }
    const Level2PeriodicOrdinaryProfile descriptor{44,{0,1342},{0,928},{0,441},{0,134},{0,1,2,3,16,17,4}};
    const Level2ProjectilePrefixProfile periodic{376,680,0,hostile,lifecycle,descriptor};
    const auto distant=level2ProjectilePrefixOperations(before,periodic,scoreFont,pages);
    assert(distant.activeEnemies==3 && distant.damage.armor==680 && distant.damage.combat.audioEvents.empty());
    auto strict=periodic;strict.periodic.reset();
    rejects([&]{level2ProjectilePrefixOperations(before,strict,scoreFont,pages);});
    auto four=before;auto& extra=four.prefix.entities[2];extra=liftTemplate;extra.allocationIndex=2;
    extra.animatedModelPresenterEnabled=false;extra.renderProfile=WorldActorRenderProfile::noHandles;
    extra.stateBytes4dTo52={255,1,0,0,4,0};
    four.prefix.allocationOrder.push_back({Level1PresentationKind::entity,2});
    four.prefix.activeOrder.push_back({Level1PresentationKind::entity,2});++four.activeEnemies;
    rejects([&]{level2ProjectilePrefixOperations(four,periodic,scoreFont,pages);});
    for(const auto index:{0U,3U,6U}) {
        auto& actor=before.prefix.entities[index];actor.position={0,700,0};actor.xVelocity=0;actor.yVelocity=-112;
    }
    const auto contact=level2ProjectilePrefixOperations(before,periodic,scoreFont,pages);
    assert(contact.activeEnemies==0 && contact.damage.armor==680-3*128 && contact.damage.paletteSteps.size()==3);
    for(const auto index:{0U,3U,6U}) {assert(contact.damage.combat.prefix.entities[index].stateBytes4dTo52[1]==0);}
    for(std::size_t slot=0;slot<before.prefix.projectiles.size();++slot) {playerShot(before,slot);}
    auto lethal=periodic;lethal.armor=0;bool bounded=false;
    try {level2ProjectilePrefixOperations(before,lethal,scoreFont,pages);}
    catch(const std::logic_error& error) {bounded=std::string(error.what())=="Level2 damage exceeds the admitted OLD inventory";}
    assert(bounded); //The unqualified -1024/HUD domain remains rejected.
}

FrameTailState tailState(const std::uint16_t loop) {
    FrameTailState state{loop,{71,-33,0},0,0,{0,0,0},{}};
    for(const auto type:{8,8,8,13,13,30,30}) {
        state.actors.push_back({static_cast<std::int16_t>(type),{8192,8192,0}});
    }
    return state;
}

FrameTailGuards guards(const std::int16_t armor) {
    FrameTailGuards result{};result.armor=armor;result.missionActive=true;
    result.periodic=FrameTailPeriodicInput{1,5,5,12,0,48,110,0};
    return result;
}

void tailLifecycle() {
    for(const std::uint16_t loop:{41,48,64}) {
        for(const auto armor:{680,200}) {
            const auto before=tailState(loop);const auto admitted=guards(static_cast<std::int16_t>(armor));
            const auto old=level2CombatFrameTailOperations(before,admitted);
            const auto work=level2LifecycleFrameTailOperations(before,admitted);
            assert(canonical(old.operations)==canonical(work.work.operations));
            assert(old.state.loopTick==work.work.state.loopTick && old.state.scheduler==work.work.state.scheduler);
            assert(work.missionActive && !work.failureRequested && !work.lowArmorWarningRequested && work.audioEvents.empty());
        }
    }
    for(const std::uint16_t loop:{41,48,63,64}) {
        for(const auto armor:{199,1,0,-1,-896}) {
            const auto before=tailState(loop);const auto admitted=guards(static_cast<std::int16_t>(armor));
            rejects([&]{level2CombatFrameTailOperations(before,admitted);});
            const auto work=level2LifecycleFrameTailOperations(before,admitted);
            assert(work.missionActive==(armor>=0) && work.failureRequested==(armor<0));
            assert(work.work.state.loopTick==loop+1 && work.work.state.playerPosition==FixedPosition{});
            assert(work.work.state.scheduler==((loop&15)==0 ? FrameTailScheduler::zeroThresholdDraw : FrameTailScheduler::none));
            assert(work.lowArmorWarningRequested==(loop==64));
            assert(work.audioEvents.size()==(loop==64 ? 1U : 0U));
            std::size_t warnings=0;
            for(const auto& operation:work.work.operations) {
                assert(operation.kind!=NormalWorkKind::gameplayPlayerCombatEvent);
                if(operation.kind==NormalWorkKind::gameplayLevel2PlayerCombatEvent) {
                    assert(operation.count==1 && operation.value==3);++warnings;
                }
            }
            assert(warnings==(loop==64 ? 1U : 0U));
            if(loop==64) {assert(work.audioEvents[0].loopTick==64 && work.audioEvents[0].eventId==3);}
        }
    }
    auto bad=guards(-897);rejects([&]{level2LifecycleFrameTailOperations(tailState(41),bad);});
    bad=guards(681);rejects([&]{level2LifecycleFrameTailOperations(tailState(41),bad);});
    bad=guards(199);bad.periodic->spawnThreshold=1;
    rejects([&]{level2LifecycleFrameTailOperations(tailState(64),bad);});
    bad=guards(199);bad.firstFrame=true;
    rejects([&]{level2LifecycleFrameTailOperations(tailState(64),bad);});
    bad=guards(199);bad.abilities[0]=1;
    rejects([&]{level2LifecycleFrameTailOperations(tailState(64),bad);});
    bad=guards(199);bad.missionActive=false;
    rejects([&]{level2LifecycleFrameTailOperations(tailState(64),bad);});
}

void lethalTimerAndFire(const CockpitPageState& pages) {
    const auto ready=weaponLampOperations(0,WeaponLampState::ready,{24,5,std::vector<std::uint8_t>(120,7)},pages);
    const auto already=weaponLampOperations(ready.dirty,WeaponLampState::ready,
        {24,5,std::vector<std::uint8_t>(120,7)},ready.pages);
    auto before=tailState(64);before.weaponTimers={1,0,1};
    const auto work=level2LifecycleFrameTailOperations(before,guards(-1),{}, {ready.operations,already.operations});
    assert((work.work.state.weaponTimers==std::array<std::uint8_t,3>{0,0,0}));
    assert(work.work.state.loopTick==65 && work.failureRequested && work.lowArmorWarningRequested);
    const auto marker=std::find_if(work.work.operations.begin(),work.work.operations.end(),[](const auto& operation) {
        return operation.kind==NormalWorkKind::gameplayLevel2PlayerCombatEvent;
    });
    const auto firstPort=std::find_if(work.work.operations.begin(),work.work.operations.end(),[](const auto& operation) {
        return operation.kind==NormalWorkKind::port8Write || operation.kind==NormalWorkKind::port16Write;
    });
    assert(marker!=work.work.operations.end() && firstPort!=work.work.operations.end() && marker<firstPort);
    rejects([&]{level2LifecycleFrameTailOperations(before,guards(-1),{}, {ready.operations});});

    Level1PrimaryFireInput primary{};primary.player.type=0;primary.player.collisionRadius=723;
    primary.player.position=before.playerPosition;primary.model={44,0,134};primary.allocationCount=7;
    primary.membership=Level1PrimaryFireMembership::level2Combat;
    primary.allocationDomain=Level1ProjectileAllocationDomain::retained;
    primary.combatEffects=Level1CombatEffectMembership{7,7,0};
    for(std::size_t slot=0;slot<primary.pool.size();++slot) {
        primary.pool[slot].poolSlot=slot;primary.pool[slot].stateBytes4dTo54[0]=253;
    }
    const auto firing=level1PrimaryFireOperations(primary);assert(firing.launch);
    const auto lamp=weaponLampOperations(0xffff,WeaponLampState::unavailable,
        {24,5,std::vector<std::uint8_t>(120,9)},pages);
    before.weaponTimers={};auto admitted=guards(-1);admitted.heldKeys[8]=true;
    const auto fired=level2LifecycleFrameTailOperations(before,admitted,Level1TailFireAction{firing,lamp.operations});
    assert(fired.failureRequested && fired.work.state.loopTick==65);
    assert((fired.work.state.weaponTimers==std::array<std::uint8_t,3>{8,31,0}));
    const auto primaryMarker=std::find_if(fired.work.operations.begin(),fired.work.operations.end(),[](const auto& operation) {
        return operation.kind==NormalWorkKind::gameplayLevel2PrimaryFireEvent;
    });
    const auto warningMarker=std::find_if(fired.work.operations.begin(),fired.work.operations.end(),[](const auto& operation) {
        return operation.kind==NormalWorkKind::gameplayLevel2PlayerCombatEvent;
    });
    assert(primaryMarker<warningMarker && warningMarker!=fired.work.operations.end());
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    const auto scoreFont=font();const auto pages=CockpitPageState::fromRecoveredMenu();
    periodicThreeLifts(scoreFont,pages);
    prefixDamage(scoreFont,pages);
    tailLifecycle();
    lethalTimerAndFire(pages);
}
