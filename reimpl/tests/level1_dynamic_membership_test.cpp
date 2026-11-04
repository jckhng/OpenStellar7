#include "stellar7/gameplay.hpp"
#include "stellar7/simulation_prefix_work.hpp"
#include "stellar7/radar_work.hpp"
#include <algorithm>
#include <cassert>
#include <memory>
#include <stdexcept>
using namespace stellar7;
template<class Action>void rejects(Action action){bool rejected=false;try{action();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
struct Ground final : Level1FrameRenderer {
    void render(Level1RenderState state) override {
        state.ground.advance(state.player.yawVelocity,state.player.forwardVelocity,state.rng);
    }
};

Level1ProjectilePrefixInput initial() {
    auto model = std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
    Ground ground;
    model->tickRetainedLevel1({},ground);
    model->finishLevel1Frame();
    const auto& state = model->level1State();
    Level1ProjectilePrefixInput input{state.player(),state.entities(),state.projectiles(),{}, {},{},
        false,state.rng(),0,false,0,true,{false,false}};
    for (std::size_t index = 0; index < Level1State::entityCount; ++index) {
        input.allocationOrder.push_back({Level1PresentationKind::entity,index});
    }
    for (const auto index : state.activeListOrder()) {
        input.activeOrder.push_back({Level1PresentationKind::entity,index});
    }
    return input;
}


Level1ProjectilePrefixInput carry(const Level1ProjectilePrefixWork& work) {
    return {work.player,work.entities,work.projectiles,work.allocationOrder,work.activeOrder,
        work.lastListCamera,work.refreshRequested,work.rng,0,false,0,true,{false,false}};
}
Level1ProjectilePrefixInput spawned(bool listed) {
    auto value=initial();
    Level1Entity actor{};actor.allocationIndex=6;actor.type=1;actor.collisionRadius=1342;
    actor.position={-30000,30000,0};actor.forwardVelocity=225;actor.weaponSlotCount=2;
    actor.stateBytes4dTo52={255,1,0,0,1,0};actor.animatedModelPresenterEnabled=false;
    actor.renderProfile=WorldActorRenderProfile::noHandles;
    value.entities.push_back(actor);value.allocationOrder.push_back({Level1PresentationKind::entity,6});
    value.refreshRequested=true;
    if(listed){value.activeOrder.push_back({Level1PresentationKind::entity,6});}
    return value;
}
Level1EnemyPrefixProfile profile() {
    Level1EnemyPrefixProfile value{1,256,{0,441},{0,134}};
    value.membership=Level1OrdinaryMembership::periodicSpawned;return value;
}
Level1ProjectilePrefixInput newOwner() {
    auto value=spawned(true);
    value.entities[0].position={-20000,20000,0};
    value.entities[0].forwardVelocity=0;
    auto& actor=value.entities[6];actor.position={-2048,2048,0};actor.yaw=0xa000;
    actor.forwardVelocity=0;actor.stateBytes4dTo52[5]=2;
    return value;
}
Level1ProjectilePrefixInput pendingFireAndBirth() {
    auto value=spawned(false);
    auto& shot=value.projectiles[0];
    shot.poolSlot=0;shot.type=23;shot.collisionRadius=134;shot.ownerIsPlayer=true;
    shot.ownerAllocationIndex=static_cast<std::size_t>(-1);
    shot.position=value.player.position;shot.forwardVelocity=500;
    shot.stateBytes4dTo54={253,64,0,0,0,0,0,0};
    value.allocationOrder.insert(value.allocationOrder.end()-1,{Level1PresentationKind::projectile,0});
    return value;
}
int main(){
    auto p=profile();auto input=spawned(false);
    auto strict=p;strict.membership=Level1OrdinaryMembership::initialSix;
    rejects([&]{level1EnemyPrefixOperations(input,strict);});
    const auto first=level1EnemyPrefixOperations(input,p);
    assert(first.prefix.entities.size()==7&&first.prefix.activeOrder.size()==7);
    assert(first.prefix.entities[6].position==input.entities[6].position);
    assert(first.prefix.entities[6].stateBytes4dTo52[5]==0);
    const auto next=level1EnemyPrefixOperations(carry(first.prefix),p);
    assert(!(next.prefix.entities[6].position==input.entities[6].position));
    input=newOwner();const auto launched=level1EnemyPrefixOperations(input,p);
    assert(launched.audioEvents.size()==1&&launched.audioEvents.front().eventId==2);
    assert(launched.prefix.projectiles[0].active()&&launched.prefix.projectiles[0].ownerAllocationIndex==6);
    const auto adjacent=level1EnemyPrefixOperations(carry(launched.prefix),p);
    assert(adjacent.prefix.projectiles[0].ownerAllocationIndex==6);
    assert(adjacent.prefix.projectiles[0].stateBytes4dTo54[1]==53);
    auto bad=spawned(false);bad.entities[6].type=3;bad.entities[6].collisionRadius=770;bad.entities[6].forwardVelocity=216;
    rejects([&]{level1EnemyPrefixOperations(bad,p);});
    bad=spawned(false);bad.refreshRequested=false;rejects([&]{level1EnemyPrefixOperations(bad,p);});
    bad=spawned(false);std::swap(bad.allocationOrder.front(),bad.allocationOrder.back());rejects([&]{level1EnemyPrefixOperations(bad,p);});
    bad=carry(launched.prefix);bad.projectiles[0].ownerAllocationIndex=2;rejects([&]{level1EnemyPrefixOperations(bad,p);});
    const auto simultaneous=pendingFireAndBirth();
    const auto combined=level1EnemyPrefixOperations(simultaneous,p);
    assert(combined.prefix.allocationOrder[6]==(Level1ObjectRef{Level1PresentationKind::projectile,0}));
    assert(combined.prefix.allocationOrder[7]==(Level1ObjectRef{Level1PresentationKind::entity,6}));
    assert(combined.prefix.activeOrder.size()==8&&combined.prefix.projectiles[0].stateBytes4dTo54[1]==64);
    assert(combined.prefix.projectiles[0].position==simultaneous.projectiles[0].position);
    assert(combined.prefix.entities[6].position==simultaneous.entities[6].position);
    bad=simultaneous;std::swap(bad.allocationOrder[6],bad.allocationOrder[7]);
    rejects([&]{level1EnemyPrefixOperations(bad,p);});
    bad=simultaneous;std::swap(bad.allocationOrder[5],bad.allocationOrder[6]);
    rejects([&]{level1EnemyPrefixOperations(bad,p);});
    bad=simultaneous;bad.projectiles[0].stateBytes4dTo54[1]=63;
    rejects([&]{level1EnemyPrefixOperations(bad,p);});
    bad=simultaneous;bad.entities[6].stateBytes4dTo52[5]=1;
    rejects([&]{level1EnemyPrefixOperations(bad,p);});
    bad=simultaneous;bad.refreshRequested=false;
    rejects([&]{level1EnemyPrefixOperations(bad,p);});
    rejects([&]{level1EnemyPrefixOperations(simultaneous,strict);});
    const auto original=initial();const auto old=level1EnemyPrefixOperations(original,strict);const auto same=level1EnemyPrefixOperations(original,p);
    assert(old.prefix.operations.size()==same.prefix.operations.size());
    for(std::size_t i=0;i<old.prefix.operations.size();++i){const auto a=old.prefix.operations[i],b=same.prefix.operations[i];assert(a.kind==b.kind&&a.count==b.count&&a.value==b.value);}
}
