#include "playable_decoy.hpp"
#include "playable_jump.hpp"
#include "playable_campaign_comparison.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace {
using namespace stellar7;
constexpr auto decoy=PlayableSystemDomain::decoyBundle;

void require(const bool condition,const char* message) {
    if(!condition) {throw std::runtime_error(message);}
}
template<class Call> void rejects(Call call) {
    bool rejected=false;
    try {call();} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"unqualified decoy domain was admitted");
}
std::vector<std::uint16_t> events(const PlayableJumpResult& result) {
    std::vector<std::uint16_t> values;
    for(const auto& effect:result.effects) {
        if(const auto* audio=std::get_if<SystemAudio>(&effect)) {values.push_back(audio->event);}
    }
    return values;
}
void activationAndCadence() {
    PlayableJumpState initial{};
    initial.statuses.fill(3);initial.selectedSystem=-1;initial.timers[3]=17;
    rejects([&]{applyPlayableJumpAction(initial,GameplaySystemAction::decoy);});
    rejects([&]{applyPlayableJumpAction(initial,GameplaySystemAction::decoy,PlayableSystemDomain::timedBundle);});
    const auto active=applyPlayableJumpAction(initial,GameplaySystemAction::decoy,decoy);
    require(active.state.timers[0]==900 && active.state.statuses[0]==-2
        && active.state.selectedSystem==0 && active.proxyRefresh==PlayableProxyRefresh::copyPlayerRecord,
        "0FF6 did not own exactly one clone and900timer");
    require(events(active)==std::vector<std::uint16_t>{8,25},"I activation audio order changed");
    const auto& panel=std::get<SystemPanel>(active.effects[3]);
    require(panel.remainingCharges==2 && panel.style==SystemPanelStyle::active,
        "8E94 panel must precede signed charge negation");
    const auto repeated=applyPlayableJumpAction(active.state,GameplaySystemAction::decoy,decoy);
    require(events(repeated)==std::vector<std::uint16_t>{8}
        && repeated.proxyRefresh==PlayableProxyRefresh::none && repeated.state.timers[0]==900,
        "active I recopied/refilled its proxy");
    auto repaired=active.state;repaired.statuses[0]=3;repaired.timers[0]=777;
    const auto reactivated=applyPlayableJumpAction(repaired,GameplaySystemAction::activateSelected,decoy);
    require(reactivated.state.statuses[0]==-2 && reactivated.state.timers[0]==777
        && reactivated.proxyRefresh==PlayableProxyRefresh::none && events(reactivated)==std::vector<std::uint16_t>{8},
        "90F2 repair caused another0FF6 copy/event");
    auto state=active.state;
    for(int tick=1;tick<=900;++tick) {
        const auto result=advancePlayableJump(state,decoy);state=result.state;
        const auto remaining=900-tick;
        require(state.timers[0]==remaining && state.timers[3]==17,"0F72 timer/12D4 ownership changed");
        require(events(result).empty(),"expiry op14 must not retrigger event25");
        if(remaining>=860) {
            require(std::get<SystemDecoyStrip>(result.effects.front()).mode
                ==((remaining&2)!=0 ? DecoyStripMode::visible : DecoyStripMode::restore),
                "initial6922 cadence used pre-decrement timer");
            require(result.effects.size()==(remaining==860 ? 3U : 1U),"1026 reveal call count changed");
            if(remaining==860) {
                require(std::get<SystemDecoyStrip>(result.effects[1]).mode==DecoyStripMode::visible
                    && std::get<SystemDecoyPalette>(result.effects[2]).mode==DecoyPaletteMode::decoy,
                    "860 must force visible strip before direct palette upload");
            }
        } else if(remaining>=60) {
            require(result.effects.empty(),"quiet I interval produced display work");
        } else if(remaining>0) {
            require(result.effects.size()==1 && std::get<SystemDecoyStrip>(result.effects.front()).mode
                ==((remaining&2)!=0 ? DecoyStripMode::visible : DecoyStripMode::restore),
                "expiry6922 cadence changed");
        } else {
            require(state.statuses[0]==2 && result.effects.size()==4
                && std::get<SystemDecoyStrip>(result.effects[0]).mode==DecoyStripMode::restore
                && std::get<SystemPanel>(result.effects[1]).style==SystemPanelStyle::ready
                && std::get<SystemDecoyStrip>(result.effects[2]).mode==DecoyStripMode::restore
                && std::get<SystemDecoyPalette>(result.effects[3]).mode==DecoyPaletteMode::restore,
                "expiry must retain both restore6922 calls around8ED2");
        }
    }
    auto last=initial;last.statuses[0]=1;
    last=applyPlayableJumpAction(last,GameplaySystemAction::decoy,decoy).state;last.timers[0]=1;
    const auto exhausted=advancePlayableJump(last,decoy);
    require(exhausted.state.selectedSystem==1 && exhausted.state.statuses[0]==0
        && events(exhausted)==std::vector<std::uint16_t>{16},"last I charge must passive-cycle after expiry panel");
    rejects([&]{normalizePlayableSystemsAfterReset(active.state,decoy);});
}
void liveProxy() {
    Level1Entity player{};player.allocationIndex=7;player.position={40000,-40000,2530};
    player.flags=0x23;player.pitch=121;player.yaw=49152;player.yawVelocity=-512;
    player.forwardVelocity=-200;player.weaponCooldown=8;player.weaponSlotTimers={1,2,3};
    player.stateBytes4dTo52={0xff,1,8,7,6,5};player.collisionRadius=723;
    Level1Entity proxy{};proxy.allocationIndex=99;
    copyPlayableDecoy(proxy,player);
    auto expected=player;expected.allocationIndex=99;
    require(test::campaignActorFields(proxy)==test::campaignActorFields(expected),"typed99-byte copy lost represented fields");
    const auto& target=playableBehaviorTarget(player,proxy,900);
    require(&target==&proxy && &playableBehaviorTarget(player,proxy,0)==&player,"9298 target lost canonical identity");
    recenterPlayableDecoy(proxy,{-1000,1000,0});
    require(proxy.position==FixedPosition{-24536,24536,2530},"active proxy did not normalize XY each tail");
    Level1Entity actor{};actor.position={1000,0,0};
    refreshPlayableTarget(actor,target);const auto oldHeading=actor.targetHeading;
    player.position={1000,8000,0};refreshPlayableDecoyPosition(proxy,player);
    refreshPlayableTarget(actor,target);
    require(actor.targetHeading==presenterTargetHeading(actor.position,player.position)
        && actor.targetHeading!=oldHeading && proxy.yaw==49152 && proxy.weaponCooldown==8,
        "AA26/05D4 XYZ write must affect later OLD reads without copying other fields");
}
void stripPixels() {
    IndexedPages pages;
    for(std::uint16_t page:{0xa000,0xa400,0xa800}) {
        for(std::int16_t y=0;y<200;++y) {
            for(std::int16_t x=0;x<320;++x) {pages.write(page,{x,y},static_cast<std::uint8_t>(page==0xa000 ? 77 : 99));}
        }
    }
    std::vector<IndexedBitmap> art(5);art[4].width=64;art[4].height=23;art[4].pixels.resize(64*23);
    for(std::size_t index=0;index<art[4].pixels.size();++index) {art[4].pixels[index]=static_cast<std::uint8_t>(index%251);}
    const auto front=pages.snapshot(0xa000);
    pages.apply(playableDecoyStripEffects(PlayableDecoyStrip::visible,art,0xa000,0xa800,0xa400));
    for(std::int16_t y=0;y<23;++y) {
        for(std::int16_t x=0;x<64;++x) {
            const auto expected=art[4].pixels[std::size_t(y)*64+x];
            require(pages.read(0xa800,{static_cast<std::int16_t>(128+x),static_cast<std::int16_t>(1+y)})==expected
                && pages.read(0xa400,{static_cast<std::int16_t>(128+x),static_cast<std::int16_t>(1+y)})==expected,
                "6922 opaque pixel/cache copy differs");
        }
    }
    require(pages.snapshot(0xa000)==front && pages.read(0xa800,{127,1})==99,"6922 wrote FRONT/outside its rectangle");
    pages.apply(playableDecoyStripEffects(PlayableDecoyStrip::restore,art,0xa000,0xa800,0xa400));
    require(pages.read(0xa800,{128,1})==77 && pages.read(0xa400,{191,23})==77,"restore did not read actual FRONT");
    art[4].width=63;
    rejects([&]{playableDecoyStripEffects(PlayableDecoyStrip::visible,art,0xa000,0xa800,0xa400);});
    rejects([&]{playableDecoyStripEffects(PlayableDecoyStrip::restore,art,0xa000,0xa000,0xa400);});
}
} //namespace
int main() {activationAndCadence();liveProxy();stripPixels();}
