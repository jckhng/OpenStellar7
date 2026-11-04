#include "playable_startup_resources.hpp"
#include "stellar7/story_scene.hpp"
#include <algorithm>
#include <iostream>
#include <tuple>

namespace {
using namespace stellar7;
void require(const bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
void sameFrame(const IndexedScreen& a,const IndexedScreen& b) {
    require(a.pixels==b.pixels&&a.palette.size()==b.palette.size(),"streaming changed indexed pixels/palette extent");
    for(std::size_t i=0;i<a.palette.size();++i) {
        require(std::tie(a.palette[i].red,a.palette[i].green,a.palette[i].blue)==std::tie(b.palette[i].red,b.palette[i].green,b.palette[i].blue),
            "streaming changed palette components");
    }
}
void sameDelivery(const NativeSessionDelivery& a,const NativeSessionDelivery& b) {
    require(a.firstSample==b.firstSample&&a.samples==b.samples&&a.presentations.size()==b.presentations.size()
        &&a.events.size()==b.events.size(),"streaming changed complete PCM/presentation/event receipt");
    for(std::size_t i=0;i<a.presentations.size();++i) {
        require(a.presentations[i].sampleEnd==b.presentations[i].sampleEnd,"streaming changed frame timestamp");
        sameFrame(a.presentations[i].indexed,b.presentations[i].indexed);
    }
    for(std::size_t i=0;i<a.events.size();++i) {
        require(a.events[i].eventId==b.events[i].eventId&&a.events[i].loopTick==b.events[i].loopTick,"streaming changed source event order");
    }
}
auto voiceFields(const SampledVoiceObservation& value) {
    return std::make_tuple(value.phase,value.transferred,value.sampleCount,value.clockPhase,value.clockPeriod,value.dacValue,value.speakerEnabled);
}
void collect(const NativeSessionDelivery& delivery,std::vector<std::int16_t>& pcm) {
    require(delivery.firstSample==pcm.size(),"cold PCM epoch is not contiguous");
    pcm.insert(pcm.end(),delivery.samples.begin(),delivery.samples.end());
}
struct Prefix {
    explicit Prefix(const std::uint64_t first):first(first) {}
    void append(const NativeSessionDelivery& chunk) {
        require(chunk.firstSample==first+samples.size(),"Story sink PCM prefixes are not contiguous");
        require(chunk.entry.empty()&&!chunk.exitRequested&&!chunk.pointerPosition,"Story prefix altered unrelated delivery metadata");
        for(const auto& frame:chunk.presentations) {
            require(frame.sampleEnd>=chunk.firstSample&&frame.sampleEnd<=chunk.firstSample+chunk.samples.size(),"Story prefix frame escaped its audio interval");
        }
        samples.insert(samples.end(),chunk.samples.begin(),chunk.samples.end());
        frames.insert(frames.end(),chunk.presentations.begin(),chunk.presentations.end());
        events.insert(events.end(),chunk.events.begin(),chunk.events.end());++calls;
    }
    void verify(const NativeSessionDelivery& whole) const {
        require(whole.firstSample==first&&samples==whole.samples,"Story sink PCM does not equal complete receipt");
        //Frontend adds its final frame after the provider's sink has returned.
        require(frames.size()<=whole.presentations.size()&&events.size()==whole.events.size(),"Story sink is not a whole receipt prefix");
        for(std::size_t i=0;i<frames.size();++i) {
            require(frames[i].sampleEnd==whole.presentations[i].sampleEnd,"Story sink duplicated/reordered a frame timestamp");
            sameFrame(frames[i].indexed,whole.presentations[i].indexed);
        }
        for(std::size_t i=0;i<events.size();++i) {
            require(events[i].eventId==whole.events[i].eventId&&events[i].loopTick==whole.events[i].loopTick,"Story sink changed event order");
        }
    }
    std::uint64_t first;std::vector<std::int16_t> samples;std::vector<NativeTimedFrame> frames;
    std::vector<Level1AudioEvent> events;std::size_t calls=0;
};
enum class SpeechEscapeGate { retainedPoll,secondVoice };
void busySpeechEscape(const GameplayAssets& data,const NativeStartupAssets& art,const FrameCopyProfile copy,const SpeechEscapeGate gate) {
    RecoveredFrontend frontend(data,art,copy,10000,{},CampaignExecution::playable,
        NativePointerCapability::portablePointer,NativeMenuFeatures::releaseMenus);
    constexpr auto samplesPerMillisecond=SoundBlasterSynth::sampleRate/1000;
    bool waiting=false;std::vector<std::size_t> observedVoices;
    for(unsigned call=0;call<30000;++call) {
        NativeMenuInput input;
        input.joystick=NativeJoystickSample{};
        if(gate==SpeechEscapeGate::retainedPoll&&frontend.startup().storyScene()==StoryScene::bridgeOpening
            &&frontend.startup().storyVoice().transferred>0) {input.confirm=true;}
        const auto first=frontend.elapsedSamples();const auto delivery=frontend.advanceStartup(input);
        require(delivery.firstSample==first&&first+delivery.samples.size()==frontend.elapsedSamples(),"Escape fixture lost its PCM epoch");
        const auto voice=frontend.startup().storyVoice();
        if(frontend.startup().storyScene()==StoryScene::bridgeOpening&&voice.transferred>0
            &&(observedVoices.empty()||observedVoices.back()!=voice.sampleCount)) {observedVoices.push_back(voice.sampleCount);}
        //Exercise both a retained descriptor poll and natural second-voice cleanup.
        const bool reachedGate=gate==SpeechEscapeGate::retainedPoll ? delivery.samples.size()<samplesPerMillisecond : observedVoices.size()==2;
        if(frontend.startup().storyScene()==StoryScene::bridgeOpening&&voice.phase==SampledVoicePhase::playing
            &&voice.transferred>0&&reachedGate) {waiting=true;break;}
        if(frontend.startup().phase()==NativeStartupPhase::awaitingMenuDecision) {break;}
    }
    require(waiting&&frontend.startup().storyActive()&&frontend.presentationInputActive(),"Escape fixture missed retained B1 speech wait");
    NativeMenuInput escape;escape.cancel=true;
    const auto first=frontend.elapsedSamples();const auto pressed=frontend.advanceStartup(escape);
    const auto released=frontend.advanceStartup({});
    require(pressed.firstSample==first&&released.firstSample==first+pressed.samples.size()
        &&frontend.elapsedSamples()==released.firstSample+released.samples.size(),"Escape cleanup lost contiguous PCM");
    require(frontend.startup().phase()==NativeStartupPhase::awaitingMenuMusic&&!frontend.presentationInputActive(),
        "Escape pulse was discarded during retained B1 speech wait");
    require(frontend.startup().storyVoice().phase==SampledVoicePhase::unloaded,"Escape cleanup retained sampled speech");

    const auto advance=[&](const NativeMenuInput input) {
        const auto first=frontend.elapsedSamples();const auto delivery=frontend.advanceStartup(input);
        require(delivery.firstSample==first&&first+delivery.samples.size()==frontend.elapsedSamples(),"busy Escape lost menu PCM continuity");
    };
    for(unsigned call=0;call<30000&&frontend.startup().phase()!=NativeStartupPhase::awaitingMenuDecision;++call) {advance({});}
    require(frontend.startup().phase()==NativeStartupPhase::awaitingMenuDecision,"busy Escape failed retained menu construction");
    NativeMenuInput begin;begin.confirm=true;advance(begin);
    for(unsigned call=0;!frontend.session()&&call<128;++call) {advance({});}
    require(frontend.session(),"busy Escape menu failed actual Begin");
    for(unsigned frame=0;frame<3;++frame) {
        const auto first=frontend.elapsedSamples();const auto delivery=frontend.advanceGameplay({});
        require(delivery.firstSample==first&&first+delivery.samples.size()==frontend.elapsedSamples(),"busy Escape lost adjacent PCM continuity");
    }
    require(frontend.session()->model().phase()==GameplayPhase::playable,"busy Escape failed adjacent ordinary frames");
}
void earlyIntroEscape(const GameplayAssets& data,const NativeStartupAssets& art,const FrameCopyProfile copy) {
    RecoveredFrontend a(data,art,copy,10000,{},CampaignExecution::playable,NativePointerCapability::portablePointer,NativeMenuFeatures::releaseMenus);
    RecoveredFrontend b(data,art,copy,257,{},CampaignExecution::playable,NativePointerCapability::portablePointer,NativeMenuFeatures::releaseMenus);
    std::vector<std::int16_t> pcmA,pcmB;
    const auto advance=[&](RecoveredFrontend& frontend,const NativeMenuInput input,std::vector<std::int16_t>& pcm) {
        collect(frontend.advanceStartup(input),pcm);
    };
    const auto prelude=[&](RecoveredFrontend& frontend,std::vector<std::int16_t>& pcm) {
        for(unsigned call=0;call<30000&&frontend.startup().phase()!=NativeStartupPhase::awaitingIntroInput;++call) {advance(frontend,{},pcm);}
        require(frontend.startup().phase()==NativeStartupPhase::awaitingIntroInput,"early Escape fixture missed intro");
    };
    prelude(a,pcmA);prelude(b,pcmB);
    constexpr unsigned cancelPoll=4;
    for(unsigned poll=1;poll<=cancelPoll;++poll) {
        NativeMenuInput input;input.cancel=poll==cancelPoll;
        advance(a,input,pcmA);advance(b,input,pcmB);
    }
    require(a.startup().phase()==NativeStartupPhase::awaitingMenuMusic&&b.startup().phase()==NativeStartupPhase::awaitingMenuMusic,
        "fourth-poll Escape did not return intro to menu music");

    const auto menu=[&](RecoveredFrontend& frontend,std::vector<std::int16_t>& pcm) {
        for(unsigned call=0;call<30000&&frontend.startup().phase()!=NativeStartupPhase::awaitingMenuDecision;++call) {advance(frontend,{},pcm);}
        require(frontend.startup().phase()==NativeStartupPhase::awaitingMenuDecision,"early Escape failed credits/menu entry");
    };
    menu(a,pcmA);menu(b,pcmB);
    require(pcmA==pcmB&&a.elapsedSamples()==b.elapsedSamples(),"early Escape changed menu PCM across budgets");
    sameFrame(a.frame(),b.frame());
    require(a.startup().storyVoice().phase==SampledVoicePhase::unloaded&&b.startup().storyVoice().phase==SampledVoicePhase::unloaded,
        "early Escape retained speech at menu");

    NativeMenuInput begin;begin.confirm=true;
    advance(a,begin,pcmA);advance(b,begin,pcmB);
    for(unsigned call=0;(!a.session()||!b.session())&&call<128;++call) {
        if(!a.session()) {advance(a,{},pcmA);}
        if(!b.session()) {advance(b,{},pcmB);}
    }
    require(a.session()&&b.session(),"early Escape menu failed actual Begin");
    for(unsigned frame=0;frame<3;++frame) {collect(a.advanceGameplay({}),pcmA);collect(b.advanceGameplay({}),pcmB);}
    require(a.session()->model().phase()==GameplayPhase::playable&&b.session()->model().phase()==GameplayPhase::playable
        &&a.session()->model().level2LoopTick()==b.session()->model().level2LoopTick(),"early Escape failed adjacent ordinary frames");
    require(pcmA==pcmB&&a.elapsedSamples()==b.elapsedSamples(),"early Escape/Begin changed retained PCM across budgets");
    sameFrame(a.frame(),b.frame());
}
void run(const std::filesystem::path& root) {
    auto data=naturalRoute::assets(root);
    data.storyScenes=std::make_shared<const StorySceneAssets>(decodeStorySceneAssets(naturalRoute::read(root/"SCENEB1.RES"),
        naturalRoute::read(root/"SCENEA.RES"),{naturalRoute::read(root/"SCENEB2.RES"),naturalRoute::read(root/"SCENEB3.RES"),
        naturalRoute::read(root/"SCENEB5.RES")},naturalRoute::read(root/"SCENEC.RES"),naturalRoute::read(root/"VOICE.RES")));
    data.cockpitSettings=CockpitSettings{81,{1,0,1,1,0,255}};data.highscoreTable->setConfiguration(*data.cockpitSettings);
    const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto vga=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    require(vga.size()==1,"stream test requires one actual VGA driver");const auto copy=decodeFrameCopyProfile(vga[0]);
    busySpeechEscape(data,art,copy,SpeechEscapeGate::retainedPoll);
    busySpeechEscape(data,art,copy,SpeechEscapeGate::secondVoice);
    earlyIntroEscape(data,art,copy);
    RecoveredFrontend plain(data,art,copy,10000,{},CampaignExecution::playable,NativePointerCapability::portablePointer,NativeMenuFeatures::releaseMenus);
    RecoveredFrontend streamed(data,art,copy,10000,{},CampaignExecution::playable,NativePointerCapability::portablePointer,NativeMenuFeatures::releaseMenus);
    RecoveredFrontend packet(data,art,copy,257,{},CampaignExecution::playable,NativePointerCapability::portablePointer,NativeMenuFeatures::releaseMenus);
    std::vector<std::int16_t> plainPcm,streamPcm,packetPcm;
    const auto prelude=[&](RecoveredFrontend& f,std::vector<std::int16_t>& pcm) {
        for(unsigned call=0;call<30000&&f.startup().phase()!=NativeStartupPhase::awaitingIntroInput;++call) {collect(f.advanceStartup({}),pcm);}
        require(f.startup().phase()==NativeStartupPhase::awaitingIntroInput,"stream test missed actual post-logo boundary");
    };
    prelude(plain,plainPcm);prelude(streamed,streamPcm);prelude(packet,packetPcm);
    require(plainPcm==streamPcm&&plainPcm==packetPcm,"stream test prelude budgets changed PCM");
    std::vector<std::size_t> expected,observed;
    for(const auto& encoded:data.storyScenes->voices[0]) {expected.push_back(decodeSampledVoice(encoded).samples.size());}
    std::size_t sinkCalls=0,splitDeliveries=0,storyCalls=0;
    while(plain.startup().phase()==NativeStartupPhase::awaitingIntroInput&&storyCalls++<30000) {
        Prefix a(streamed.elapsedSamples()),b(packet.elapsedSamples());
        const auto x=plain.advanceStartup({});
        const auto y=streamed.advanceStartup({},[&](const NativeSessionDelivery& chunk) {a.append(chunk);});
        const auto z=packet.advanceStartup({},[&](const NativeSessionDelivery& chunk) {b.append(chunk);});
        a.verify(y);b.verify(z);sameDelivery(x,y);sameDelivery(x,z);
        collect(x,plainPcm);collect(y,streamPcm);collect(z,packetPcm);
        require(plain.startup().phase()==streamed.startup().phase()&&plain.startup().phase()==packet.startup().phase()
            &&plain.startup().storyScene()==streamed.startup().storyScene()&&plain.startup().storyPage()==streamed.startup().storyPage(),"streaming changed Story controller state");
        sameFrame(plain.frame(),streamed.frame());sameFrame(plain.frame(),packet.frame());
        const auto voice=streamed.startup().storyVoice();
        require(voiceFields(voice)==voiceFields(plain.startup().storyVoice())&&voiceFields(voice)==voiceFields(packet.startup().storyVoice()),"streaming changed actual voice state");
        if(streamed.startup().storyScene()==StoryScene::bridgeOpening&&voice.transferred>0
            &&(observed.empty()||observed.back()!=voice.sampleCount)) {observed.push_back(voice.sampleCount);}
        sinkCalls+=a.calls;if(a.calls>1) {++splitDeliveries;}
    }
    require(plain.startup().phase()==NativeStartupPhase::awaitingMenuMusic&&expected.size()==4&&observed==expected,
        "streamed actual B1/A did not complete all four source descriptors in order");
    const auto finish=[&](RecoveredFrontend& f,std::vector<std::int16_t>& pcm) {
        for(unsigned call=0;call<120000&&f.startup().phase()!=NativeStartupPhase::awaitingMenuDecision;++call) {
            const bool credits=f.startup().creditsActive();
            Prefix prefix(f.elapsedSamples());
            const auto delivery=f.advanceStartup({},[&](const NativeSessionDelivery& chunk) {prefix.append(chunk);});
            if(credits) {prefix.verify(delivery);}
            collect(delivery,pcm);
        }
        require(f.startup().phase()==NativeStartupPhase::awaitingMenuDecision,"streaming missed first menu boundary");
    };
    finish(plain,plainPcm);finish(streamed,streamPcm);finish(packet,packetPcm);
    require(plainPcm==streamPcm&&plainPcm==packetPcm&&sinkCalls>storyCalls&&splitDeliveries>0,"streaming changed whole cold PCM or failed to split long receipts");
    sameFrame(plain.frame(),streamed.frame());sameFrame(plain.frame(),packet.frame());
    std::cout<<"actual cold B1/A four descriptors: wholePCM/state/pages10000/257 + exact sink prefixes; calls="<<sinkCalls<<" split="<<splitDeliveries<<'\n';
}
}
int main(int argc,char** argv) {try {require(argc==2,"requires actual DATA");run(argv[1]);} catch(const std::exception& error) {std::cerr<<"story-streaming: "<<error.what()<<'\n';return 1;}}
