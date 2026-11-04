// Controlled component: real public-startup synth/clock, supplied scene data.
// This checks retained ownership, not natural campaign ancestry or original PCM.
#include "playable_startup_resources.hpp"
#include "playable_story.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <tuple>

namespace stellar7 {
struct NativeSessionTestAccess {
    static SoundBlasterSynth& synth(NativeGameplaySession& session) {return session.synth_;}
    static PresentationClock& clock(NativeGameplaySession& session) {return session.clock_;}
    static PaletteWorkState palette(const NativeGameplaySession& session) {return *session.recoveredStartupPalette_;}
    static void enterStory(NativeGameplaySession& session,const StoryScene scene) {
        if(session.story_) {throw std::logic_error("native scene fixture already active");}
        session.storyDestination_=scene==StoryScene::level2?1:scene==StoryScene::level4?3:6;
        session.activeStory_=scene;
        session.story_=std::make_shared<PlayableStoryPresenter>(*session.assets_.storyScenes,scene,
            RadarPages{session.pages_.cachePage(),session.pages_.backPage(),session.pages_.frontPage(),session.pages_.drawPage(),session.pages_.copyPage()});
    }
};
}

namespace {
using namespace stellar7;
std::optional<std::filesystem::path> reportRoot;

void writePreview(const StoryScene scene,const bool skip,const bool music,const std::vector<std::int16_t>& samples,
    const bool background,const bool voice,const bool cursorAdvanced) {
    if(!reportRoot) {return;}
    constexpr std::array<const char*,7> names{"SCENEB1","SCENEA","SCENEB2","SCENEB3","SCENEB5","SCENEC","SCENEE"};
    const auto name=std::string(names.at(static_cast<std::size_t>(scene)))+(skip?"-escape":"-complete")
        +(music?"-music-on":"-music-off");
    const auto size=static_cast<std::uint32_t>(samples.size()*sizeof(std::int16_t));
    std::ofstream wave(*reportRoot/(name+".wav"),std::ios::binary);
    const auto integer=[&](std::uint32_t value,unsigned bytes) {
        for(unsigned index=0;index<bytes;++index) {wave.put(static_cast<char>((value>>(8*index))&255));}
    };
    wave.write("RIFF",4);integer(36+size,4);wave.write("WAVEfmt ",8);integer(16,4);
    integer(1,2);integer(1,2);integer(SoundBlasterSynth::sampleRate,4);
    integer(SoundBlasterSynth::sampleRate*2,4);integer(2,2);integer(16,2);
    wave.write("data",4);integer(size,4);
    int peak=0;double squares=0;std::size_t nonzero=0;
    for(const auto sample:samples) {
        integer(static_cast<std::uint16_t>(sample),2);
        peak=std::max(peak,std::abs(static_cast<int>(sample)));squares+=static_cast<double>(sample)*sample;
        if(sample!=0) {++nonzero;}
    }
    if(!wave) {throw std::runtime_error("cannot write private scene audio preview");}
    std::ofstream report(*reportRoot/(name+".json"));
    report<<"{\"scene\":\""<<names.at(static_cast<std::size_t>(scene))<<"\",\"music\":"<<(music?"true":"false")
        <<",\"escape\":"<<(skip?"true":"false")<<",\"samples\":"<<samples.size()
        <<",\"sample_rate\":"<<SoundBlasterSynth::sampleRate<<",\"peak_s16\":"<<peak
        <<",\"rms_s16\":"<<(samples.empty()?0:std::sqrt(squares/samples.size()))<<",\"nonzero_samples\":"<<nonzero
        <<",\"background_selected\":"<<(background?"true":"false")<<",\"voice_loaded\":"<<(voice?"true":"false")
        <<",\"sequence_advanced\":"<<(cursorAdvanced?"true":"false")<<"}\n";
    if(!report) {throw std::runtime_error("cannot write private scene audio metrics");}
}

void require(const bool condition,const char* message) {
    if(!condition) {throw std::runtime_error(message);}
}

struct Harness {
    std::unique_ptr<RecoveredFrontend> frontend;
    IndexedPages pixels;
    CockpitPageState pages=CockpitPageState::fromRecoveredMenu();
    PaletteWorkState palette;
    SoundBlasterClockPhase phase;

    Harness(const GameplayAssets& data,const NativeStartupAssets& startup,const FrameCopyProfile copy)
        :frontend(std::make_unique<RecoveredFrontend>(data,startup,copy,10000,
            std::shared_ptr<HighscoreStore>{},CampaignExecution::playable)) {
        unsigned polls=0;
        for(unsigned call=0;!frontend->session()&&call<10000;++call) {
            NativeMenuInput input{};
            if(frontend->startup().phase()==NativeStartupPhase::awaitingMenuDecision) {input.confirm=polls++==1;}
            frontend->advanceStartup(input);
        }
        require(frontend->session()!=nullptr,"public startup failed to reach Begin");
        frontend->advanceGameplay({});
        auto& session=const_cast<NativeGameplaySession&>(*frontend->session());
        phase=*session.recoveredClockPhase();
        palette=NativeSessionTestAccess::palette(session);
        require(phase.playableCampaign&&synth().driverState().sequenceSelected,
            "fixture lacks retained playable Level1 music");
        resetPages();
    }

    SoundBlasterSynth& synth() {return NativeSessionTestAccess::synth(
        const_cast<NativeGameplaySession&>(*frontend->session()));}
    PresentationClock& clock() {return NativeSessionTestAccess::clock(
        const_cast<NativeGameplaySession&>(*frontend->session()));}
    void resetPages() {
        pixels=IndexedPages{};
        pages=CockpitPageState::fromRecoveredMenu();
        for(const auto page:{pages.cachePage(),pages.backPage(),pages.frontPage()}) {
            for(std::int16_t y=0;y<200;++y) {
                for(std::int16_t x=0;x<320;++x) {pixels.write(page,{x,y},0);}
            }
        }
    }
    PlayableStoryOwners owners(const GameplayAssets& data,const bool music) {
        return {pixels,pages,palette,synth(),clock(),phase,data.scoreFont,data.cockpitDacPalette,music,true};
    }
};

void sameFrame(const IndexedScreen& a,const IndexedScreen& b) {
    require(a.pixels==b.pixels&&a.palette==b.palette,"story frame changed across budgets");
}

void sameDelivery(const NativeSessionDelivery& a,const NativeSessionDelivery& b) {
    require(a.firstSample==b.firstSample&&a.samples==b.samples
        &&a.events.size()==b.events.size()&&a.presentations.size()==b.presentations.size(),
        "story PCM/events changed across budgets");
    for(std::size_t index=0;index<a.events.size();++index) {
        require(a.events[index].loopTick==b.events[index].loopTick
            &&a.events[index].eventId==b.events[index].eventId,"story event order changed across budgets");
    }
    for(std::size_t index=0;index<a.presentations.size();++index) {
        require(a.presentations[index].sampleEnd==b.presentations[index].sampleEnd,
            "story frame timestamp changed across budgets");
        sameFrame(a.presentations[index].indexed,b.presentations[index].indexed);
    }
}

void sameState(Harness& a,Harness& b) {
    require(a.clock().elapsedSamples()==b.clock().elapsedSamples()
        &&a.pages.frontPage()==b.pages.frontPage()
        &&a.pages.backPage()==b.pages.backPage()
        &&a.palette.source==b.palette.source&&a.palette.output==b.palette.output
        &&a.palette.dac.colors==b.palette.dac.colors,"story clock/pages/palette changed across budgets");
    require(a.pixels.snapshot(a.pages.frontPage())==b.pixels.snapshot(b.pages.frontPage()),
        "story indexed pixels changed across budgets");
    const auto x=a.synth().driverState(),y=b.synth().driverState();
    require(x.sequenceAllocated==y.sequenceAllocated&&x.sequenceSelected==y.sequenceSelected
        &&x.sequenceActive==y.sequenceActive&&x.sequencePosition==y.sequencePosition
        &&x.sequenceWait==y.sequenceWait&&x.sequenceAllocations==y.sequenceAllocations
        &&x.sequenceSelections==y.sequenceSelections&&x.sequenceActivity==y.sequenceActivity
        &&x.normalClockCycles==y.normalClockCycles&&x.clockPhase==y.clockPhase,
        "story retained sequence/clock changed across budgets");
    const auto voice=[](const SampledVoiceObservation& value) {
        return std::tie(value.phase,value.transferred,value.sampleCount,value.clockPhase,
            value.clockPeriod,value.dacValue,value.speakerEnabled);
    };
    require(voice(a.synth().sampledVoiceObservation())==voice(b.synth().sampledVoiceObservation()),
        "story sampled voice changed across budgets");
    require(a.phase.callback&&b.phase.callback
        &&a.phase.callback->domain==b.phase.callback->domain
        &&a.phase.callback->sequence==b.phase.callback->sequence,
        "story callback owner changed across budgets");
    const auto& left=a.synth().registerWrites();const auto& right=b.synth().registerWrites();
    require(left.size()==right.size(),"story OPL write count changed across budgets");
    for(std::size_t index=0;index<left.size();++index) {
        require(left[index].registerNumber==right[index].registerNumber
            &&left[index].value==right[index].value,"story OPL writes changed across budgets");
    }
}

void nextBank(Harness& a,Harness& b,const GameplayAssets& data,const std::size_t bank,const bool music) {
    const auto& sequence=data.campaignLevels.at(bank).musicSequence;
    std::vector<std::int16_t> firstPcm,secondPcm;
    std::uint64_t firstSample=0,secondSample=0;
    for(auto* harness:{&a,&b}) {
        require(!harness->synth().driverState().sequenceAllocated,
            "story retained its scene descriptor after exit");
        require(harness->synth().sampledVoiceObservation().phase==SampledVoicePhase::unloaded,
            "story retained sampled speech after exit");
        harness->phase.callback=harness->synth().selectPlayableSequence(sequence,PlayableSequenceMode::repeat,
            music ? PlayableMusicPublication::current : PlayableMusicPublication::suspended);
        harness->phase.playableMusicOff=!music;
        auto work=harness->clock().renderWork(harness->phase,{{NormalWorkKind::instructions,1000}},120000);
        harness->phase=work.timing.phase;
        if(harness==&a) {firstSample=work.audio.firstSample;firstPcm=std::move(work.audio.samples);}
        else {secondSample=work.audio.firstSample;secondPcm=std::move(work.audio.samples);}
        require(harness->synth().driverState().sequenceAllocated,"next real bank lost its descriptor");
    }
    require(firstSample==secondSample&&firstPcm==secondPcm,
        "next real bank PCM changed across budgets");
    sameState(a,b);
}

void scene(Harness& a,Harness& b,const GameplayAssets& data,const StorySceneAssets& stories,
    const StoryScene kind,const bool skip,const bool music,const std::size_t nextBankIndex) {
    a.resetPages();b.resetPages();
    constexpr std::size_t fastBudget=10000,smallBudget=257;
    const auto make=[&](Harness& harness) {
        return PlayableStoryPresenter(stories,kind,{harness.pages.cachePage(),harness.pages.backPage(),
            harness.pages.frontPage(),harness.pages.drawPage(),harness.pages.copyPage()});
    };
    auto first=make(a),second=make(b);
    bool background=false,voice=false,backgroundPcm=false,cursorAdvanced=false;
    std::vector<std::size_t> heardSegments;
    std::vector<std::int16_t> recorded;
    std::size_t startingCursor=0,updates=0;
    const auto step=[&](const PlayableStoryInput input) {
        const auto left=first.advance(a.owners(data,music),input,fastBudget);
        const auto right=second.advance(b.owners(data,music),input,smallBudget);
        if(reportRoot) {recorded.insert(recorded.end(),left.samples.begin(),left.samples.end());}
        sameDelivery(left,right);sameState(a,b);
        require(first.result()==second.result()&&first.page()==second.page(),
            "story controller diverged across budgets");
        const auto driver=a.synth().driverState();
        const auto observation=a.synth().sampledVoiceObservation();
        if(driver.sequenceAllocated&&a.phase.callback->program
            &&a.phase.callback->program->repeatCount==0xffff) {
            background=true;
            require(driver.sequenceSelected==music,"scene background publication ignored Music");
            require(a.phase.callback->sequence==stories.backgroundSequence || !music,
                "scene selected a substituted background resource");
            if(updates==0) {startingCursor=driver.sequencePosition;}
            else {
                cursorAdvanced=cursorAdvanced||driver.sequencePosition!=startingCursor;
                backgroundPcm=backgroundPcm||std::any_of(left.samples.begin(),left.samples.end(),
                    [](const auto sample) {return sample!=0;});
            }
        }
        voice=voice||observation.phase!=SampledVoicePhase::unloaded;
        if(observation.transferred>0
            &&(heardSegments.empty()||heardSegments.back()!=observation.sampleCount)) {
            heardSegments.push_back(observation.sampleCount);
        }
        ++updates;
    };
    step({});
    if(skip) {step({1,false});}
    for(unsigned call=0;first.result()==StorySceneResult::playing&&call<20000;++call) {step({});}
    require(first.result()==(skip ? StorySceneResult::skipped : StorySceneResult::complete),
        "story did not reach requested complete/Escape result");
    const bool composite=kind==StoryScene::bridgeOpening||kind==StoryScene::level2
        ||kind==StoryScene::level4||kind==StoryScene::level7;
    require(background==(composite&&(kind!=StoryScene::bridgeOpening||!music)),
        "scene background start/speech suppression disagrees with original wrapper");
    if(music&&(kind==StoryScene::bridgeOpening||kind==StoryScene::draxon
        ||kind==StoryScene::defeat)) {
        require(voice,"narrated scene never loaded its supplied VOICE descriptor");
    } else {require(!voice,"unnarrated bridge scene loaded sampled voice");}
    if(music&&!skip&&kind==StoryScene::defeat) {
        std::vector<std::size_t> expected;
        constexpr std::size_t defeatVoiceGroup=3;
        for(const auto& encoded:stories.voices.at(defeatVoiceGroup)) {
            expected.push_back(decodeSampledVoice(encoded).samples.size());
        }
        require(expected.size()==2&&heardSegments==expected,
            "complete SCENEE missed either supplied SSM4 speech segment");
    }
    if(music&&!skip&&composite&&kind!=StoryScene::bridgeOpening) {
        require(cursorAdvanced&&backgroundPcm,
            "complete unvoiced scene did not advance DRAXON or emit background PCM");
    }
    writePreview(kind,skip,music,recorded,background,voice,cursorAdvanced);
    nextBank(a,b,data,nextBankIndex,music);
}

enum class NativeSceneExit { complete,escape };
void nativeScene(Harness& a,Harness& b,const StoryScene kind,const NativeSceneExit exit) {
    // Declared scene-call entry on retained public-startup pages/synth/clock;
    // no claim of preceding campaign combat or native loader ancestry.
    auto& first=const_cast<NativeGameplaySession&>(*a.frontend->session());
    auto& second=const_cast<NativeGameplaySession&>(*b.frontend->session());
    NativeSessionTestAccess::enterStory(first,kind);NativeSessionTestAccess::enterStory(second,kind);
    const auto loop=first.model().level2LoopTick();
    const auto rngCalls=first.model().level2Rng().calls();
    auto nextSample=first.elapsedSamples();
    bool background=false,voice=false,musicPcm=false,cursorMoved=false;std::optional<std::size_t> cursor;
    std::vector<std::int16_t> recorded;
    for(unsigned call=0;first.storyScene()&&call<20000;++call) {
        GameplayInput input{};input.exit=exit==NativeSceneExit::escape&&call==1;
        const auto left=first.advancePlayableCampaign(input,10000);
        std::vector<std::int16_t> streamed;
        const auto right=second.advancePlayableCampaign(input,257,{},[&](const NativeSessionDelivery& chunk) {
            require(chunk.firstSample==nextSample+streamed.size(),"native story stream lost PCM epoch");
            streamed.insert(streamed.end(),chunk.samples.begin(),chunk.samples.end());
        });
        require(streamed==right.samples,"native story stream dropped/duplicated PCM");
        sameDelivery(left,right);sameFrame(first.frame(),second.frame());
        require(left.firstSample==nextSample,"native story wrapper lost PCM epoch");nextSample+=left.samples.size();
        require(first.elapsedSamples()==nextSample&&second.elapsedSamples()==nextSample,"native story clock reset");
        require(first.model().level2LoopTick()==loop&&first.model().level2Rng().calls()==rngCalls,"native story advanced gameplay/RNG");
        const auto driver=first.synth().driverState();
        const auto* phase=first.recoveredClockPhase();
        if(driver.sequenceAllocated&&phase->callback->program&&phase->callback->program->repeatCount==0xffff) {
            background=true;require(driver.sequenceSelected,"native illustrated scene did not publish background music");
            if(cursor) {cursorMoved=cursorMoved||*cursor!=driver.sequencePosition;}else {cursor=driver.sequencePosition;}
            musicPcm=musicPcm||std::any_of(left.samples.begin(),left.samples.end(),[](auto sample) {return sample!=0;});
        }
        const auto speech=first.synth().sampledVoiceObservation();
        voice=voice||(speech.phase==SampledVoicePhase::playing&&speech.transferred>0);
        if(reportRoot) {recorded.insert(recorded.end(),left.samples.begin(),left.samples.end());}
    }
    require(!first.storyScene()&&!second.storyScene(),"native story wrapper did not return");
    require(background,"native story wrapper never selected DRAXON background");
    if(exit==NativeSceneExit::complete) {require(cursorMoved&&musicPcm,"native illustrated scene background was silent");}
    require(voice==(kind==StoryScene::level7&&exit==NativeSceneExit::complete),"native B5 completion/Escape chained SCENEC incorrectly");
    require(!first.synth().driverState().sequenceAllocated
        &&first.synth().sampledVoiceObservation().phase==SampledVoicePhase::unloaded,"native story leaked audio after exit");
    writePreview(kind,exit==NativeSceneExit::escape,true,recorded,background,voice,cursorMoved);
}

void run(const std::filesystem::path& root) {
    auto data=naturalRoute::assets(root);
    const auto scenes=decodeStorySceneAssets(naturalRoute::read(root/"SCENEB1.RES"),
        naturalRoute::read(root/"SCENEA.RES"),
        {naturalRoute::read(root/"SCENEB2.RES"),naturalRoute::read(root/"SCENEB3.RES"),
            naturalRoute::read(root/"SCENEB5.RES")},
        naturalRoute::read(root/"SCENEC.RES"),naturalRoute::read(root/"VOICE.RES"),
        naturalRoute::read(root/"DRAXON.RES"),naturalRoute::read(root/"SCENEE.RES"));
    require(!scenes.backgroundSequence.empty(),"fixture lacks source DRAXON descriptor");
    data.storyScenes=std::make_shared<const StorySceneAssets>(scenes);
    const auto startup=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto vga=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    require(vga.size()==1,"fixture requires supplied VGA driver");
    const auto copy=decodeFrameCopyProfile(vga.front());
    Harness a(data,startup,copy),b(data,startup,copy);
    {
        Harness nativeA(data,startup,copy),nativeB(data,startup,copy);
        const auto previousReport=reportRoot;
        if(reportRoot) {reportRoot=*reportRoot/"native-session";std::filesystem::create_directory(*reportRoot);}
        for(const auto kind:{StoryScene::level2,StoryScene::level4,StoryScene::level7}) {
            nativeScene(nativeA,nativeB,kind,NativeSceneExit::complete);
            nativeScene(nativeA,nativeB,kind,NativeSceneExit::escape);
        }
        reportRoot=previousReport;
        std::cout<<"Native illustrated scene wrapper B2/B3/B5 complete/Escape; B5->C voice chain/cleanup PASS\n";
    }
    scene(a,b,data,scenes,StoryScene::level2,false,true,1);
    scene(a,b,data,scenes,StoryScene::level2,true,true,1);
    scene(a,b,data,scenes,StoryScene::level4,false,true,3);
    scene(a,b,data,scenes,StoryScene::level4,true,true,3);
    scene(a,b,data,scenes,StoryScene::level7,false,true,6);
    scene(a,b,data,scenes,StoryScene::level7,true,true,6);
    scene(a,b,data,scenes,StoryScene::bridgeOpening,true,true,0);
    scene(a,b,data,scenes,StoryScene::draxon,false,true,6);
    scene(a,b,data,scenes,StoryScene::defeat,false,true,0);
    scene(a,b,data,scenes,StoryScene::defeat,true,true,0);
    a.phase.callback=a.synth().setPlayableMusicPublication(PlayableMusicPublication::suspended);
    b.phase.callback=b.synth().setPlayableMusicPublication(PlayableMusicPublication::suspended);
    a.phase.playableMusicOff=true;b.phase.playableMusicOff=true;
    sameState(a,b);
    scene(a,b,data,scenes,StoryScene::level2,true,false,1);
    scene(a,b,data,scenes,StoryScene::level2,false,false,1);
    scene(a,b,data,scenes,StoryScene::bridgeOpening,false,false,0);
    scene(a,b,data,scenes,StoryScene::draxon,false,false,6);
    scene(a,b,data,scenes,StoryScene::defeat,false,false,0);
    std::cout<<"Controlled B2/B3/B5, B1/C/E complete/Escape/Music Off, next-bank retained twins PASS\n";
}
}

int main(int argc,char** argv) {
    if(argc!=2&&argc!=3) {return 2;}
    try {
        if(argc==3) {
            reportRoot=std::filesystem::path(argv[2]);
            require(!std::filesystem::exists(*reportRoot),"scene report output must be fresh");
            std::filesystem::create_directories(*reportRoot);
        }
        run(argv[1]);return 0;
    }
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
