#include "playable_startup_resources.hpp"
#include "playable_startup_clock_domain.hpp"
#include "stellar7/audio_program_work.hpp"
#include "stellar7/driver_sequence_work.hpp"
#include <iostream>
#include <tuple>

namespace {
using namespace stellar7;
void require(const bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
template<class Action> void rejects(Action action,const char* message) {
    bool rejected=false;try {action();}catch(const std::invalid_argument&) {rejected=true;}require(rejected,message);
}
auto proofFlags(const AudioClockSequenceSelectionPhase& p) {
    return std::make_tuple(p.published,p.countdownWritten,p.countdownReturned,p.logoEventPlayed,p.logoEventSuppressed,
        p.logoFramesCompleted,p.logoFrameActive,p.logoSoundWaiting,p.logoSoundReturned,p.logoCurrentCleared,p.logoTimerFallback,
        p.logoEmptyCurrentReturned,p.songOrdinal,p.menuCountdownWritten,p.creditsCountdownWritten,p.creditsInputReturned,
        p.menuEntryCountdownWritten,p.beginDecisions,p.recoveredMenuInput,p.recoveredMenuResult,p.recoveredMenuActivated,
        p.beginFadeRequests,p.beginCountdownWritten,p.beginCountdownReturned,p.beginCurrentCleared,p.beginEmptyCurrentReturned,
        p.level1DirectResetCompleted,p.level1RetryCurrentCleared,p.level1RetryStoppedHandle.has_value());
}
void sameProof(const AudioClockSequenceSelectionPhase& a,const AudioClockSequenceSelectionPhase& b) {
    require(proofFlags(a)==proofFlags(b)&&a.prepared.sequence==b.prepared.sequence&&a.prepared.position==b.prepared.position
        &&a.prepared.active==b.prepared.active&&a.prepared.domain==b.prepared.domain&&a.prepared.program&&b.prepared.program
        &&a.prepared.program->repeatCount==b.prepared.program->repeatCount
        &&a.prepared.program->handleStatus==b.prepared.program->handleStatus,
        "active native borrow changed original menu proof/one-shot receipts");
}
//Controlled component provider: the same inherited-channel builder used by
//selectPlayableSequence, applied to the actual native Story menu callback.
//Only resource/descriptor selection changes; no PIT/CPU/device phase is seeded.
SoundBlasterCallbackState selected(const SoundBlasterCallbackState& incoming,const std::vector<std::uint8_t>& sequence) {
    constexpr std::size_t sourceBodyOffset=36;
    const auto activation=soundBlasterSequenceOperations(sequence,DriverSequenceAdmission::inheritedChannels);
    auto result=applyDriverSequenceCallbackState(incoming,activation);
    result.program=soundBlasterProgramSequenceState(*incoming.program,activation.state);result.program->repeatCount=0xffff;
    result.sequence=sequence;result.position=sourceBodyOffset;result.wait=0;result.operation=2;
    result.domain=SoundBlasterCallbackDomain::sequence;result.active=true;result.fade=0;return result;
}
void run(const std::filesystem::path& root) {
    auto data=naturalRoute::assets(root);
    const auto voice=naturalRoute::read(root/"VOICE.RES");require(!voice.empty(),"guard fixture requires actual VOICE open");
    data.attractVoiceAvailable=true;
    data.storyScenes=std::make_shared<const StorySceneAssets>(decodeStorySceneAssets(
        naturalRoute::read(root/"SCENEB1.RES"),naturalRoute::read(root/"SCENEA.RES"),
        {naturalRoute::read(root/"SCENEB2.RES"),naturalRoute::read(root/"SCENEB3.RES"),naturalRoute::read(root/"SCENEB5.RES")},
        naturalRoute::read(root/"SCENEC.RES"),voice));
    require(data.cockpitSettings&&data.cockpitSettings->options[5]==255&&data.cockpitSettings->options[2]!=0,
        "guard component requires actual Story/Music On configuration");
    const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto vga=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    constexpr auto features=static_cast<NativeMenuFeatures>(11); //Existing full3 + explicit attract8.
    RecoveredFrontend f(data,art,decodeFrameCopyProfile(vga.at(0)),10000,{},CampaignExecution::playable,NativePointerCapability::unavailable,features);
    for(unsigned call=0;f.startup().phase()!=NativeStartupPhase::awaitingIntroInput&&call<100000;++call) {f.advanceStartup({});}
    require(f.startup().storyActive(),"guard fixture missed actual native Story entry");
    NativeMenuInput escape{};escape.cancel=true;f.advanceStartup(escape);
    for(unsigned call=0;f.startup().phase()!=NativeStartupPhase::awaitingMenuDecision&&call<100000;++call) {f.advanceStartup({});}
    require(f.startup().phase()==NativeStartupPhase::awaitingMenuDecision&&f.startup().recoveredIntro()
        &&f.startup().recoveredIntro()->mode==NativeIntroPresentation::storyPlayed,"guard fixture missed real Story-menu owner");
    const auto actual=f.startup().recoveredMenuEntry()->finalPhase;
    require(actual.playableStartup&&actual.playableAutomaticAttract&&!actual.playableCampaign&&actual.sequenceSelection
        &&!actual.sequenceSelection->published&&actual.sequenceSelection->prepared.sequence==data.presentationMusicSequence
        &&playableStartupProgram(actual)&&actual.attractBorrow==AudioClockAttractBorrow::inactive,
        "actual native Story menu did not produce the failing unpublished SNG1 proof shape");
    constexpr std::int64_t horizon=120000;
    normalWorkTiming(actual,{{NormalWorkKind::instructions,1}},horizon);
    auto mixed=actual;mixed.playableCampaign=true;
    mixed.callback=selected(*actual.callback,data.logoMusicSequence);
    require(!playableStartupProgram(mixed),"controlled SNG0 component did not separate current data from SNG1 proof");
    rejects([&]{normalWorkTiming(mixed,{{NormalWorkKind::instructions,1}},horizon);},
        "sticky attract capability alone weakened unpublished sequence guards");
#ifndef STELLAR7_ATTRACT_BORROW_RED_CONTROL
    mixed.attractBorrow=AudioClockAttractBorrow::menuPresentation;
#endif
    const auto logo=normalWorkTiming(mixed,{{NormalWorkKind::playableAttractCountdownStore,1,240}},horizon);
    require(logo.phase.timer->globalCount==240&&logo.phase.attractBorrow==AudioClockAttractBorrow::menuPresentation,
        "active borrowed SNG0 could not retain its actual source240 store");
    sameProof(*actual.sequenceSelection,*logo.phase.sequenceSelection);
    auto noCapability=mixed;noCapability.playableAutomaticAttract=false;
    rejects([&]{normalWorkTiming(noCapability,{{NormalWorkKind::instructions,1}},horizon);},"active borrow lacked native capability guard");
    auto source=mixed;source.playableCampaign=false;
    rejects([&]{normalWorkTiming(source,{{NormalWorkKind::instructions,1}},horizon);},"active borrow leaked into SourceWork");
    auto badPrepared=mixed;badPrepared.sequenceSelection->prepared.active=false;
    rejects([&]{normalWorkTiming(badPrepared,{{NormalWorkKind::instructions,1}},horizon);},"active borrow bypassed basic prepared-state guards");
    auto badTimer=mixed;badTimer.timer->activeMask=0;
    rejects([&]{normalWorkTiming(badTimer,{{NormalWorkKind::instructions,1}},horizon);},"active borrow bypassed timer ownership");
    auto invalid=mixed;invalid.attractBorrow=static_cast<AudioClockAttractBorrow>(2);
    rejects([&]{normalWorkTiming(invalid,{{NormalWorkKind::instructions,1}},horizon);},"invalid active borrow domain accepted");
    auto menu=logo.phase;
    menu.callback=selected(*menu.callback,data.presentationMusicSequence); //Actual native menu reactivation builder.
    menu.attractBorrow=AudioClockAttractBorrow::inactive;menu.playableCampaign=actual.playableCampaign;
    const auto returned=normalWorkTiming(menu,{{NormalWorkKind::instructions,1}},horizon);
    sameProof(*actual.sequenceSelection,*returned.phase.sequenceSelection);
    require(playableStartupProgram(returned.phase)&&returned.phase.attractBorrow==AudioClockAttractBorrow::inactive,
        "active borrow did not return the same menu proof to its original guard");
    std::cout<<"Actual nativeStory menu + controlled SNG0 active-borrow guard; sticky/Source/basic negatives and proof carry PASS\n"<<std::flush;
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(argv[1]);return 0;}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
