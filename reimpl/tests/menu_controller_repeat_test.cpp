#include "stellar7/audio_program_work.hpp"
#include "stellar7/audio_repeat_work.hpp"
#include "retained_menu_clock_domain.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using namespace stellar7;
constexpr std::size_t bodyOffset=36;
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
template<class F> void rejects(F function) {
    bool rejected=false;
    try {function();} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"unsupported original domain was silently admitted");
}
SoundBlasterCallbackState fixture(std::initializer_list<std::uint8_t> events) {
    SoundBlasterCallbackState state{};state.program.emplace();
    state.program->instrumentBank.resize(96*28);
    state.program->volumeTable.resize(16*64);
    state.sequence.resize(bodyOffset);state.sequence.insert(state.sequence.end(),events);
    state.position=bodyOffset;state.voices.channels.fill(0xff);state.voices.notes.fill(60);
    state.channelEnabled.fill(1);
    for(auto& voice:state.program->voices) {voice.program=0xff;}
    return state;
}
std::vector<std::uint8_t> registers(const SoundBlasterCallbackAdvance& work) {
    std::vector<std::uint8_t> result;
    for(const auto& block:work.blocks) {
        if(block.kind==SoundBlasterWorkKind::registerWrite) {result.push_back(block.registerNumber);}
    }
    return result;
}
void sustain() {
    const auto on=soundBlasterProgramCallbackAdvance(fixture({0xb5,0x40,127,1,0xfc}));
    const auto one=soundBlasterProgramCallbackAdvance(fixture({0xb5,0x40,1,1,0xfc}));
    require(on.dispatches==150 && one.dispatches==150 && registers(on).empty()
        &&on.state.voices.channelDeferrals[5]==1 &&one.state.voices.channelDeferrals[5]==1,
        "2001 nonzero sustain did not normalize1 with exact instruction path");
    const auto quiet=soundBlasterProgramCallbackAdvance(fixture({0xb5,0x40,0,1,0xfc}));
    require(quiet.dispatches==188&&registers(quiet).empty(),"2001 zero sustain did not scan all9 unmatched slots");

    auto held=on.state;
    held.voices.channels[0]=5;held.voices.notes[0]=60;held.sounding[0]=1;
    held.sequence.resize(bodyOffset);held.sequence.insert(held.sequence.end(),{0x95,60,0,1,0xfc});
    held.position=bodyOffset;held.wait=0;
    const auto deferred=soundBlasterProgramCallbackAdvance(held);
    require(deferred.state.sounding[0]==1&&deferred.state.program->voices[0].program==60
        &&registers(deferred).empty(),"noteOff failed to retain the existing sustain marker");

    auto release=deferred.state;
    release.sequence.resize(bodyOffset);release.sequence.insert(release.sequence.end(),{0xb5,0x40,0,1,0xfc});
    release.position=bodyOffset;release.wait=0;
    release.voices.channels[1]=7;release.program->voices[1].program=61;release.sounding[1]=1;
    release.voices.channels[2]=5;release.program->voices[2].program=0xff;release.sounding[2]=1;
    release.voices.channels[3]=5;release.voices.notes[3]=63;
    release.program->voices[3].program=63;release.sounding[3]=1;
    release.voices.directTimers={9,8,7};
    const auto off=soundBlasterProgramCallbackAdvance(release);
    require(off.state.voices.channelDeferrals[5]==0&&off.state.sounding[0]==0&&off.state.sounding[3]==0
        &&off.state.program->voices[0].program==0xff&&off.state.program->voices[3].program==0xff
        &&off.state.sounding[1]==1&&off.state.program->voices[1].program==61&&off.state.sounding[2]==1,
        "sustain release changed the wrong channel/marker or skipped a deferred voice");
    require(registers(off)==std::vector<std::uint8_t>({0xa0,0xb0,0xa3,0xb3})
        &&off.state.voices.directTimers==std::array<std::uint16_t,3>{8,7,6},
        "release changed physical keyoff order or the independent direct timer callback");
    for(const auto unsupported:{0x4b,0x4c,0x60}) {
        rejects([&]{soundBlasterProgramCallbackAdvance(fixture({0xb5,static_cast<std::uint8_t>(unsupported),0,1,0xfc}));});
    }
}
void repeat() {
    auto state=fixture({0xcf,0x7f,0,0xb5,0x4e,1,1,0xfc});
    state.program->loopMarker=34;state.position=state.sequence.size()-1;
    rejects([&]{soundBlasterProgramCallbackAdvance(state);});
    const auto ended=soundBlasterMenuCallbackAdvance(state);
    require(!ended.state.active&&ended.state.program->handleStatus==0xffff
        &&ended.state.program->repeatCount==0xffff&&ended.state.position==bodyOffset&&ended.state.wait==0,
        "menu FC did not preserve allocation semantics/restart marker");
    const auto resumed=soundBlasterMenuCallbackAdvance(ended.state);
    require(resumed.state.active&&resumed.state.program->handleStatus==0
        &&resumed.state.program->loopMarker==34&&resumed.state.position==bodyOffset+7,
        "0494 did not resume the retained marker through actual parser work");
    state.fade=3;
    rejects([&]{soundBlasterProgramCallbackAdvance(state);});
    rejects([&]{soundBlasterLevel1CallbackAdvance(state);});
    rejects([&]{soundBlasterLevel2CallbackAdvance(state);});
    const auto fading=soundBlasterMenuCallbackAdvance(state);
    require(fading.state.fade==3&&!fading.state.active&&fading.state.position==bodyOffset,
        "menu EOF imposed an absent source fade guard or rewrote the active fade");
    const auto fadingResume=soundBlasterMenuCallbackAdvance(fading.state);
    require(fadingResume.state.active&&fadingResume.state.fade==3&&fadingResume.state.program->fadeDivider==19,
        "repeating EOF lost source fade phase on the next callback");
}
void ownerPredicates() {
    const auto live=fixture({0xb5,0x4e,1,1,0xfc});
    SoundBlasterClockPhase phase{};phase.callback=live;
    phase.sequenceSelection=AudioClockSequenceSelectionPhase{live};
    auto& selected=*phase.sequenceSelection;
    selected.published=true;selected.songOrdinal=1;selected.menuEntryCountdownWritten=true;
    require(retainedMenuProgram(phase)&&retainedMenuPrediction(phase,live),"retained menu owner was not admitted");
    auto ended=live;ended.active=false;ended.program->handleStatus=0xffff;
    phase.callback=ended;
    require(retainedMenuProgram(phase),"menu predicate excluded actual0494 EOF reentry");
    selected.beginCurrentCleared=true;selected.beginCountdownReturned=true;selected.published=false;
    phase.callback->domain=SoundBlasterCallbackDomain::noHandle;phase.callback->program.reset();
    require(!retainedMenuProgram(phase)&&retainedMenuPrediction(phase,live),
        "completed BeginStop receipt lost the earlier live selected-menu callback owner");
    selected.beginCountdownReturned=false;
    require(!retainedMenuPrediction(phase,live),"prediction admitted an uncompleted Begin current clear");
    selected.beginCountdownReturned=true;selected.prepared.sequence.push_back(0);
    require(!retainedMenuPrediction(phase,live),"prediction admitted a different resource payload");
}
} //namespace
int main() {
    try {sustain();repeat();ownerPredicates();std::cout<<"Menu sustain/EOF/Begin receipt kernel PASS\n";return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
