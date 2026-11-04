//Private proposal: component/pure fullBundle contract, not compiled or run.
#include "playable_bomb.hpp"
#include "playable_jump.hpp"

#include <stdexcept>

namespace {
using namespace stellar7;
void require(const bool condition,const char* message) {
    if(!condition) {throw std::runtime_error(message);}
}
template<class Call> void rejects(Call call) {
    bool rejected=false;
    try {call();} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"unknown/malformed Bomb record was admitted");
}

void slotOwnership() {
    std::array<Level1Entity,playableBombFixedRecordCount> records{};
    std::array<bool,playableBombFixedRecordCount> known{};
    for(auto& record:records) {record.stateBytes4dTo52[1]=1;}
    known.fill(true);
    require(!playableBombSlot(records,known),"fullFE changed to a fabricated free record");
    records[5].stateBytes4dTo52[1]=0;records[7].stateBytes4dTo52[1]=0;
    require(playableBombSlot(records,known)==5,"4506 did not choose the first physical HP0 record");
    known[6]=false;
    require(playableBombSlot(records,known)==5,"unvisited later record incorrectly blocked a known first free slot");
    known[2]=false;
    rejects([&]{playableBombSlot(records,known);});
}

void partialBirth() {
    constexpr std::uint32_t actualRadius=300; //Static bound COCKPIT model43, scale0.
    Level1Entity player{};player.position={100,200,2530};
    Level1Entity retained{};
    retained.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
    retained.animatedModelPresenterEnabled=false;
    retained.stateBytes4dTo52={0xfe,0,7,8,9,10};
    retained.presenterDistance=1234;retained.targetHeading=0x4567;
    retained.behaviorTimer=29;retained.weaponCooldown=3;
    retained.collisionContactIsPlayer=true;
    const auto bomb=preparePlayableBombActor(retained,player,actualRadius);
    require(bomb.type==43 && bomb.collisionRadius==300 && bomb.flags==0
        && bomb.stateBytes4dTo52[0]==0xfe && bomb.stateBytes4dTo52[1]==1
        && bomb.stateBytes4dTo52[4]==0,"4506/8E20 specified writes changed");
    require(bomb.position==FixedPosition{100,-800,0} && bomb.forwardVelocity==0,
        "Bomb was not placed one source backward step behind grounded player pose");
    require(bomb.renderProfile==retained.renderProfile
        && bomb.stateBytes4dTo52[2]==7 && bomb.stateBytes4dTo52[3]==8 && bomb.stateBytes4dTo52[5]==10
        && bomb.presenterDistance==1234 && bomb.targetHeading==0x4567
        && bomb.behaviorTimer==29 && bomb.weaponCooldown==3 && bomb.collisionContactIsPlayer,
        "Bomb constructor invented resets outside its partial record writes");
    retained.xVelocity=17;retained.yVelocity=-11;retained.zVelocity=9;
    const auto inherited=preparePlayableBombActor(retained,player,actualRadius);
    require(inherited.position==FixedPosition{117,-811,9} && inherited.zVelocity==9,
        "Bomb discarded retained3B3A velocity or grounded after the source step");
    player.yaw=0x4000;retained.xVelocity=retained.yVelocity=retained.zVelocity=0;
    require(preparePlayableBombActor(retained,player,actualRadius).position==FixedPosition{1100,200,0},
        "Bomb ignored player yaw during backward placement");
    auto occupied=retained;occupied.stateBytes4dTo52[1]=1;
    rejects([&]{preparePlayableBombActor(occupied,player,actualRadius);});
    rejects([&]{preparePlayableBombActor(retained,player,0);});
}
void activation() {
    PlayableJumpState state{};state.statuses.fill(3);state.selectedSystem=-1;state.timers[3]=17;
    rejects([&]{applyPlayableJumpAction(state,GameplaySystemAction::bomb);});
    rejects([&]{applyPlayableJumpAction(state,GameplaySystemAction::bomb,PlayableSystemDomain::decoyBundle);});
    const auto first=applyPlayableJumpAction(state,GameplaySystemAction::bomb,PlayableSystemDomain::fullBundle);
    require(first.state.statuses[3]==2 && first.state.timers[3]==17 && first.state.selectedSystem==3
        && first.bombRefresh==PlayableBombRefresh::deploy,"B invented timer/negative status or lost8ED2");
    std::vector<std::uint16_t> audio;
    for(const auto& effect:first.effects) {
        if(const auto* event=std::get_if<SystemAudio>(&effect)) {audio.push_back(event->event);}
    }
    require(audio==std::vector<std::uint16_t>{8,7},"8E94/8E20 audio order changed");
    auto last=state;last.statuses[3]=1;
    const auto exhausted=applyPlayableJumpAction(last,GameplaySystemAction::bomb,PlayableSystemDomain::fullBundle);
    require(exhausted.state.statuses[3]==0 && exhausted.state.selectedSystem==4
        && exhausted.state.timers[3]==17,"B last-charge immediate passive cycle changed");
    require(playableBombContactScore(4)==30 && playableBombContactScore(43)==3
        && playableBombContactScore(44)==3 && playableBombContactScore(45)==46
        && playableBombContactScore(48)==48,"Bomb score used guarded ordinary/projectile fallback");
    rejects([&]{playableBombContactScore(-1);});
    rejects([&]{playableBombContactScore(49);});
}
} //namespace

int main() {slotOwnership();partialBirth();activation();}
