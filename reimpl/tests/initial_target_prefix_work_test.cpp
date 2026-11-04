#include "stellar7/gameplay.hpp"
#include "stellar7/simulation_prefix_work.hpp"

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
template<class Action> void rejects(Action action) {
    bool rejected=false;
    try { action(); } catch (const std::invalid_argument&) { rejected=true; }
    assert(rejected);
}
struct Ground final : Level1FrameRenderer {
    void render(Level1RenderState state) override {
        state.ground.advance(state.player.yawVelocity,state.player.forwardVelocity,state.rng);
    }
};
BitmapFont font() {
    return {32,8,0,std::vector<IndexedBitmap>(96,{4,8,std::vector<std::uint8_t>(32,1)})};
}
Level1TargetPrefixProfile profile() {
    Level1EnemyPrefixProfile enemy{1,256,{0,441},{0,134},Level1ProjectileAllocationDomain::retained,
        Level1ActorContactDomain::ordinaryActors,Level1OrdinaryMembership::periodicSpawned};
    return {{enemy,680,0,0,0,Level1DamagePaletteMode::residentVga,Level1PlayerCombatDomain::expiryAndLoss},0,Level1TargetHitDomain::initialOrdinary};
}
Level1TargetPrefixInput initial(const std::uint8_t health,const bool shotFirst) {
    auto model=std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    Ground ground;
    model->tickRetainedLevel1({},ground);
    model->finishLevel1Frame();
    const auto& state=model->level1State();
    Level1TargetPrefixInput result{{state.player(),state.entities(),state.projectiles(),{}, {},{},
        false,state.rng(),0,false,0,true,{false,false}},{},{},2,0};
    auto& actor=result.prefix.entities[1];
    actor.position={12000,4000,0};
    actor.stateBytes4dTo52[1]=health;
    actor.stateBytes4dTo52[5]=4;
    actor.behaviorTimer=10;
    actor.forwardVelocity=270;
    actor.yawVelocity=0;
    actor.weaponCooldown=255;
    auto& shot=result.prefix.projectiles[0];
    shot.type=23; shot.collisionRadius=134;
    shot.position={12000,3500,0}; shot.yaw=0;
    shot.forwardVelocity=500; shot.stateBytes4dTo54={253,10,0,0,0,0,0,0};
    shot.ownerIsPlayer=true;
    for (std::size_t index=0;index<result.prefix.entities.size();++index) {
        result.prefix.allocationOrder.push_back({Level1PresentationKind::entity,index});
        result.prefix.activeOrder.push_back({Level1PresentationKind::entity,index});
    }
    result.prefix.allocationOrder.push_back({Level1PresentationKind::projectile,0});
    if (shotFirst) { result.prefix.activeOrder.insert(result.prefix.activeOrder.begin(),{Level1PresentationKind::projectile,0}); }
    else { result.prefix.activeOrder.push_back({Level1PresentationKind::projectile,0}); }
    //Source0BC8 partial writes must preserve unrelated reused FC fields.
    auto& effect=result.effects[0];
    effect.entity.flags=0x23;
    effect.entity.collisionRadius=655;
    effect.entity.pitch=19;
    effect.entity.presenterDistance=777;
    effect.entity.targetHeading=888;
    effect.projected={41,42};
    return result;
}
Level1TargetPrefixInput carry(const Level1TargetPrefixWork& value) {
    const auto& prefix=value.damage.combat.prefix;
    return {{prefix.player,prefix.entities,prefix.projectiles,prefix.allocationOrder,prefix.activeOrder,
        prefix.lastListCamera,prefix.refreshRequested,prefix.rng,value.damage.fadeTicks,false,0,true,{false,false}},
        value.effects,value.bonus,value.activeEnemies,value.score};
}
bool has(const std::vector<Level1ObjectRef>& order,const Level1ObjectRef ref) {
    return std::find(order.begin(),order.end(),ref)!=order.end();
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    const auto scoreFont=font();
    const auto pages=CockpitPageState::fromRecoveredMenu();
    for (const bool shotFirst : {false,true}) {
        for (const std::uint8_t health : {std::uint8_t{1},std::uint8_t{2}}) {
            auto input=initial(health,shotFirst);
            const auto result=level1TargetPrefixOperations(input,profile(),scoreFont,pages);
            const auto& work=result.damage.combat.prefix;
            assert(result.targetHits.size()==1 && result.targetHits[0].targetIndex==1);
            assert(work.entities[1].stateBytes4dTo52[1]==health-1);
            assert(work.entities[1].stateBytes4dTo52[0]==255 && work.entities[1].stateBytes4dTo52[4]==3);
            //Terminal health0 still runs its old-list callback before late rebuild.
            assert(work.entities[1].behaviorTimer==9);
            assert(!work.projectiles[0].active() && !has(work.allocationOrder,{Level1PresentationKind::projectile,0}));
            assert(result.effects[0].listed && has(work.activeOrder,{Level1PresentationKind::deathEffect,0}));
            const auto& effect=result.effects[0];
            assert(effect.entity.flags==0x23 && effect.entity.collisionRadius==655 && effect.entity.pitch==19);
            assert(effect.entity.presenterDistance==777 && effect.entity.targetHeading==888);
            assert((effect.projected==ProjectedPoint{41,42}));
            assert(effect.entity.stateBytes4dTo52[1]==result.targetHits[0].work.effect->age);
            assert(result.score==(health==1 ? 50U : 0U) && result.activeEnemies==(health==1 ? 1 : 2));
            assert(result.scoreSteps.size()==(health==1 ? 1U : 0U));
            assert(has(work.activeOrder,{Level1PresentationKind::entity,1})==(health!=1));
            const auto event=health==1 ? 6 : 5;
            assert(result.damage.combat.audioEvents.size()==1 && result.damage.combat.audioEvents[0].eventId==event);
            unsigned markers=0;
            for (const auto& operation : work.operations) {
                if (operation.kind==NormalWorkKind::gameplayTargetHitEvent) {
                    assert(operation.count==1 && operation.value==event); ++markers;
                }
            }
            assert(markers==1);
            auto nextProfile=profile(); nextProfile.damage.enemy.loopTick=2;
            const auto next=level1TargetPrefixOperations(carry(result),nextProfile,scoreFont,pages);
            assert(next.targetHits.empty() && next.scoreSteps.empty() && next.score==result.score);
            //Prefix computes FC presenter fields but never performs main0AC8 age.
            assert(next.effects[0].entity.stateBytes4dTo52[1]==effect.entity.stateBytes4dTo52[1]);
            rejects([&] { level1DamagePrefixOperations(input.prefix,profile().damage); });
        }
    }
    auto input=initial(1,true);
    input.bonus={11,2};
    rejects([&] { level1TargetPrefixOperations(input,profile(),scoreFont,pages); });
    input=initial(1,true);
    input.prefix.allocationOrder.push_back(input.prefix.allocationOrder.front());
    rejects([&] { level1TargetPrefixOperations(input,profile(),scoreFont,pages); });
    input=initial(1,true);
    auto oldProfile=profile();oldProfile.targetDomain=Level1TargetHitDomain::ordinaryTank;
    rejects([&] { level1TargetPrefixOperations(input,oldProfile,scoreFont,pages); });
    assert(input.prefix.entities[1].active()&&input.score==0&&input.activeEnemies==2);
    auto unsupported=profile(); unsupported.difficulty=1;
    rejects([&] { level1TargetPrefixOperations(input,unsupported,scoreFont,pages); });
    //Fixed29 lethal source caller skips7970 rather than issuing a zero award.
    input=initial(1,true);
    input.prefix.entities[1].position={-12000,-4000,0};
    auto& fixed=input.prefix.entities[2];fixed.position={12000,4000,0};
    fixed.stateBytes4dTo52[1]=1;
    const auto fixedWork=level1TargetPrefixOperations(input,profile(),scoreFont,pages);
    assert(fixedWork.targetHits.size()==1&&fixedWork.targetHits[0].targetIndex==2);
    assert(!fixedWork.damage.combat.prefix.entities[2].active());
    assert(fixedWork.score==0&&fixedWork.scoreSteps.empty()&&fixedWork.activeEnemies==2);
    return 0;
}
