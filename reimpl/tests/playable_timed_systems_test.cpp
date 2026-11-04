//Private proposal; requires the accompanying named GameplaySystemAction values.
#include "playable_jump.hpp"
#include "ordinary_behavior.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

namespace {
using namespace stellar7;
constexpr auto bundle=PlayableSystemDomain::timedBundle;

void require(const bool condition,const char* message) {
    if(!condition) {throw std::runtime_error(message);}
}

template<class Call> void rejects(Call call) {
    bool rejected=false;
    try {call();} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"unsupported ability state/action was admitted");
}

PlayableJumpState initial() {
    PlayableJumpState state{};
    state.statuses.fill(3);
    state.timers[3]=17;
    state.selectedSystem=-1;
    return state;
}

std::vector<std::uint16_t> sounds(const PlayableJumpResult& result) {
    std::vector<std::uint16_t> events;
    for(const auto& effect:result.effects) {
        if(const auto* audio=std::get_if<SystemAudio>(&effect)) {events.push_back(audio->event);}
    }
    return events;
}

void activationAndExpiry() {
    constexpr std::array<GameplaySystemAction,4> actions{GameplaySystemAction::shield,
        GameplaySystemAction::scanner,GameplaySystemAction::superPrimary,GameplaySystemAction::turbo};
    constexpr std::array<std::size_t,4> indices{1,4,5,6};
    for(std::size_t entry=0;entry<actions.size();++entry) {
        const auto system=indices[entry];
        const auto duration=system==6 ? 200 : 500;
        const auto activated=applyPlayableJumpAction(initial(),actions[entry],bundle);
        auto state=activated.state;
        require(state.selectedSystem==static_cast<std::int16_t>(system)
            && state.statuses[system]==-2 && state.timers[system]==duration,"activation ownership/charge mismatch");
        require(sounds(activated)==(system==6 ? std::vector<std::uint16_t>{8,20}
            : std::vector<std::uint16_t>{8}),"activation callback audio order changed");
        const auto panel=std::find_if(activated.effects.begin(),activated.effects.end(),[](const auto& effect) {
            return std::holds_alternative<SystemPanel>(effect);
        });
        require(panel!=activated.effects.end() && std::get<SystemPanel>(*panel).remainingCharges==2
            && std::get<SystemPanel>(*panel).style==SystemPanelStyle::active,"pre-negation panel is incorrect");
        const auto repeated=applyPlayableJumpAction(state,actions[entry],bundle);
        require(repeated.state.statuses==state.statuses && repeated.state.timers==state.timers
            && sounds(repeated)==std::vector<std::uint16_t>{8},"negative charge incorrectly reactivated");
        unsigned scannerEvents=0;
        for(int tick=1;tick<=duration;++tick) {
            const auto next=advancePlayableJump(state,bundle);
            state=next.state;
            const auto events=sounds(next);
            scannerEvents+=static_cast<unsigned>(std::count(events.begin(),events.end(),22));
            require(state.timers[system]==duration-tick && state.timers[3]==17,"timer order/retained12D4 mismatch");
            if(tick<duration) {require(state.statuses[system]==-2,"active status normalized early");}
            if(tick==duration) {
                require(state.statuses[system]==2,"expired ability lost remaining charges");
                require(next.scannerRefresh==(system==4 ? PlayableScannerRefresh::inactive
                    : PlayableScannerRefresh::none),"scanner925E expiry intent mismatch");
            }
        }
        require(scannerEvents==(system==4 ? 62U : 0U),"scanner event22 did not follow post-decrement multiple8");
    }
}

void sharedSelectionAndZeroCharge() {
    auto state=initial();
    state.statuses.fill(0);state.statuses[1]=1;state.statuses[4]=2;
    auto selected=applyPlayableJumpAction(state,GameplaySystemAction::cycleSelected,bundle);
    require(selected.state.selectedSystem==1 && sounds(selected)==std::vector<std::uint16_t>{16},"Tab source cycle changed");
    auto activated=applyPlayableJumpAction(selected.state,GameplaySystemAction::activateSelected,bundle);
    require(activated.state.statuses[1]==0 && activated.state.timers[1]==500,"third charge canceled active timer");
    require(std::get<SystemPanel>(activated.effects.back()).style==SystemPanelStyle::active,"active0 became unavailable early");
    state=activated.state;
    for(int tick=0;tick<500;++tick) {
        const auto next=advancePlayableJump(state,bundle);state=next.state;
        if(tick==499) {
            require(std::get<SystemPanel>(next.effects.front()).style==SystemPanelStyle::unavailable
                && state.selectedSystem==4 && sounds(next)==std::vector<std::uint16_t>{16},"expiry panel/cycle order changed");
        }
    }
    state=initial();state.statuses.fill(0);
    const auto absent=applyPlayableJumpAction(state,GameplaySystemAction::shield,bundle);
    require(absent.state.selectedSystem==-1 && sounds(absent)==std::vector<std::uint16_t>{8},"unavailable key omitted source activation sound");
}

void repairAndReset() {
    auto active=applyPlayableJumpAction(initial(),GameplaySystemAction::turbo,bundle).state;
    for(int tick=0;tick<11;++tick) {active=advancePlayableJump(active,bundle).state;}
    //90F2 canonical charge write, with timer intentionally retained.
    active.statuses[6]=3;
    const auto refreshed=applyPlayableJumpAction(active,GameplaySystemAction::activateSelected,bundle);
    require(refreshed.state.statuses[6]==-2 && refreshed.state.timers[6]==189
        && sounds(refreshed)==std::vector<std::uint16_t>({8,20}),"repair caused a timer refill or omitted callback20");
    auto reset=refreshed.state;reset.timers[6]=0;
    const auto normalized=normalizePlayableSystemsAfterReset(reset,bundle);
    require(normalized.state.statuses[6]==2 && normalized.state.selectedSystem==6
        && normalized.state.timers[3]==17,"48D4/8F1C charge normalization changed");
    rejects([&]{normalizePlayableSystemsAfterReset(refreshed.state,bundle);});
}

void scannerAndGameplayChoices() {
    Level1Entity type6{};type6.type=6;type6.flags=0x80;type6.stateBytes4dTo52[2]=0;
    applyType6Scanner(type6,Type6ScannerState::active);
    require((type6.flags&0x80)==0 && type6.stateBytes4dTo52[2]==1,"925E reveal changed");
    applyType6Scanner(type6,Type6ScannerState::inactive);
    require((type6.flags&0x80)!=0 && type6.stateBytes4dTo52[2]==0,"925E expiry cloak changed");
    require(playableArmorAfterDamage(680,50,PlayableShield::inactive)==480
        && playableArmorAfterDamage(680,50,PlayableShield::active)==680
        && playableArmorAfterDamage(680,-25,PlayableShield::active)==580,"shield lost signed0E7C distinction");
    GameplayInput forward{};forward.forward=true;
    GameplayInput reverse{};reverse.reverse=true;
    GameplayInput turn{};turn.left=true;
    require(advancePlayableForwardVelocity(200,reverse,PlayablePropulsion::turbo)==240
        && advancePlayableForwardVelocity(390,{},PlayablePropulsion::turbo)==400
        && advancePlayableForwardVelocity(-200,forward,PlayablePropulsion::turbo)==-160
        && advancePlayableForwardVelocity(200,forward,PlayablePropulsion::ordinary)==200
        && advancePlayableForwardVelocity(200,turn,PlayablePropulsion::ordinary)==200,
        "turbo replaced source horizontal-control branch incorrectly");
    const auto primary=playablePrimaryProfile(1);
    require(primary.selector==1 && primary.lifetime==80 && primary.cooldown==5 && primary.event==0
        && !primary.requiresCannonSlot,"super05D4 branch mismatch");
    const auto normal=playablePrimaryProfile(0);
    require(normal.selector==0 && normal.lifetime==64 && normal.cooldown==9 && normal.event==14
        && normal.requiresCannonSlot,"normal05D4 profile changed");
}

void domainNegatives() {
    const auto base=initial();
    rejects([&]{applyPlayableJumpAction(base,GameplaySystemAction::shield);});
    rejects([&]{applyPlayableJumpAction(base,static_cast<GameplaySystemAction>(255),bundle);});
    for(const auto unsupported:{0,3}) {
        auto selected=base;selected.selectedSystem=static_cast<std::int16_t>(unsupported);
        rejects([&]{applyPlayableJumpAction(selected,GameplaySystemAction::activateSelected,bundle);});
        require(selected.statuses==base.statuses && selected.timers==base.timers,"rejection changed source storage");
    }
    auto malformed=base;malformed.timers[0]=1;rejects([&]{advancePlayableJump(malformed,bundle);});
    malformed=base;malformed.timers[6]=201;rejects([&]{advancePlayableJump(malformed,bundle);});
    malformed=base;malformed.statuses[1]=-2;rejects([&]{advancePlayableJump(malformed,bundle);});
    auto apex=base;apex.playerZ=2530;apex.timers[2]=0;
    rejects([&]{applyPlayableJumpAction(apex,GameplaySystemAction::jump,bundle);});
}
} //namespace

int main() {
    activationAndExpiry();sharedSelectionAndZeroCharge();repairAndReset();
    scannerAndGameplayChoices();domainNegatives();
}
