#include "stellar7/audio_program_work.hpp"
#include "stellar7/audio_direct_work.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace stellar7;
namespace {
void require(bool value,const char* message) {if(!value){throw std::runtime_error(message);}}
SoundBlasterCallbackState endedOnce() {
    SoundBlasterCallbackState state;state.program.emplace();
    state.program->instrumentBank.resize(96*28);state.program->volumeTable.resize(16*64);
    state.program->repeatCount=1;state.program->handleStatus=0xffff;state.active=false;
    state.sequence.resize(93);state.position=92;state.channelEnabled.fill(1);state.voices.channels.fill(0xff);
    for(std::size_t slot=0;slot<state.voices.directTimers.size();++slot) {
        state.voices.channels[6+slot]=std::uint8_t{6};
    }
    return state;
}
template<class Function> void rejects(Function function) {
    bool rejected=false;try{function();}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"invalid ended voice ancestry was accepted");
}
void rejectsAll(const SoundBlasterCallbackState& state) {
    rejects([&]{soundBlasterProgramCallbackAdvance(state);});
    rejects([&]{soundBlasterPlayableDirectAdvance(state,gameplayTargetImpactDirectRecord,SoundBlasterDirectCaller::gameplayTargetImpact);});
    rejects([&]{soundBlasterStopAdvance(state,SoundBlasterStopProfile::retainedDirectTimers);});
}
}
int main() {
    try {
        const auto direct=soundBlasterPlayableDirectAdvance(endedOnce(),gameplayTargetImpactDirectRecord,
            SoundBlasterDirectCaller::gameplayTargetImpact).state;
        require(!direct.active && direct.program->repeatCount==1 && direct.program->handleStatus==0xffff
            && direct.position==92 && direct.wait==0 && direct.sounding[6]==1
            && direct.voices.directTimers[0]==60 && direct.voices.channels[6]==6,
            "direct producer lost ended-once/timer ownership");
        std::cout<<"producer: ended once + source event5 => voice6/timer60/retained-channel6\n";
        for(const auto association:{std::uint8_t{6},std::uint8_t{13},std::uint8_t{0xff}}) {
            auto retained=endedOnce();retained.voices.channels[6]=association;
            const auto produced=soundBlasterPlayableDirectAdvance(retained,gameplayTargetImpactDirectRecord,
                SoundBlasterDirectCaller::gameplayTargetImpact).state;
            require(produced.voices.channels==retained.voices.channels,"direct note rebound sequence channel association");
            require(soundBlasterProgramCallbackAdvance(produced).state.sounding==produced.sounding,
                "ended-once callback rejected independent direct timer");
            soundBlasterStopAdvance(produced,SoundBlasterStopProfile::retainedDirectTimers);
        }
        const auto idle=soundBlasterProgramCallbackAdvance(direct);
        require(idle.dispatches==24 && idle.state.voices.directTimers==direct.voices.directTimers
            && idle.state.sounding==direct.sounding && idle.state.position==direct.position
            && idle.state.wait==0 && !idle.state.active,"0494 must retain direct effects without ticking driver");
        auto next=direct;
        for(unsigned count=0;count<2;++count) {
            next=soundBlasterPlayableDirectAdvance(next,gameplayTargetImpactDirectRecord,
                SoundBlasterDirectCaller::gameplayTargetImpact).state;
        }
        for(std::size_t slot=0;slot<3;++slot) {
            require(next.sounding[6+slot]==1 && next.voices.directTimers[slot]==60,"independent direct slots lost ownership");
        }
        const auto stop=soundBlasterStopAdvance(next,SoundBlasterStopProfile::retainedDirectTimers);
        require(std::all_of(stop.state.sounding.begin(),stop.state.sounding.end(),[](auto value){return value==0;})
            && stop.state.voices.directTimers==next.voices.directTimers && stop.state.position==next.position
            && stop.state.program->handleStatus==0xffff && !stop.state.active,
            "2DA0 must key off voices without clearing timers/handle");
        auto invalid=direct;invalid.sounding[0]=1;rejectsAll(invalid);
        invalid=direct;invalid.voices.directTimers[0]=0;rejectsAll(invalid);
        
        
        invalid=direct;invalid.wait=1;rejectsAll(invalid);
        invalid=direct;invalid.program->repeatCount=0;rejectsAll(invalid);
        invalid=direct;invalid.program->handleStatus=0;rejectsAll(invalid);
        invalid=direct;invalid.active=true;rejectsAll(invalid);
        auto terminal=endedOnce();terminal.program->repeatCount=0;terminal.wait=1;
        const auto resumed=soundBlasterProgramCallbackAdvance(terminal);
        require(resumed.state.active && resumed.state.program->handleStatus==0
            && resumed.state.program->repeatCount==0xffff && resumed.state.wait==0,
            "silent repeat0 reentry changed");
        rejects([&]{soundBlasterDirectAdvance(direct,gameplayTargetImpactDirectRecord,SoundBlasterDirectCaller::gameplayTargetImpact);});
        std::cout<<"callback/direct/stop positive + residual/timerless/retained-channel negative controls PASS\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}


