#include "playable_startup_resources.hpp"
#include "stellar7/story_scene.hpp"
#include <algorithm>
#include <iostream>

namespace {
using namespace stellar7;
constexpr auto minimumTitleSamples=SoundBlasterSynth::sampleRate;
constexpr unsigned maximumStartupPackets=120000;

void require(const bool condition,const char* message) {
    if(!condition) {throw std::runtime_error(message);}
}

struct Run {
    std::vector<std::int16_t> pcm;
    std::vector<std::uint8_t> title;
    std::uint64_t titleFirst=0,titleEnd=0;
};

Run title(const GameplayAssets& game,const NativeStartupAssets& art,const FrameCopyProfile copy,
    const std::size_t budget,const std::uint16_t key=0,const NativeMenuFeatures features=NativeMenuFeatures::enhancedMenus) {
    RecoveredFrontend frontend(game,art,copy,budget,{},CampaignExecution::playable,
        NativePointerCapability::portablePointer,features);
    Run run;
    const auto collect=[&](const NativeSessionDelivery& delivery) {
        require(delivery.firstSample==run.pcm.size() && delivery.firstSample+delivery.samples.size()==frontend.elapsedSamples(),
            "native title pacing lost retained PCM continuity");
        run.pcm.insert(run.pcm.end(),delivery.samples.begin(),delivery.samples.end());
    };
    const auto advance=[&](const NativeMenuInput input) {
        try {collect(frontend.advanceStartup(input));}
        catch(const std::exception&) {
            std::cerr<<"title key="<<key<<" startup_phase="<<static_cast<unsigned>(frontend.startup().phase())<<'\n';throw;
        }
    };
    for(unsigned call=0;call<maximumStartupPackets;++call) {
        if(frontend.startup().titleActive() && frontend.startup().phase()==NativeStartupPhase::awaitingMenuResources) {break;}
        advance({});
    }
    require(frontend.startup().titleActive(),"native title fixture did not reach title overlays");
    advance({});
    run.title=frontend.frame().pixels;run.titleFirst=frontend.elapsedSamples();
    require(run.title!=art.menuScreen.pixels,"native title has no Stellar7 overlay");
    const auto palette=frontend.frame().palette;
    for(unsigned call=0;frontend.startup().titleActive() && call<maximumStartupPackets;++call) {
        NativeMenuInput input;input.sourceKey=key;
        advance(input);
        require(frontend.frame().pixels==run.title,"native title pacing changed title pixels");
        const auto& current=frontend.frame().palette;
        require(current.size()==palette.size(),"native title pacing changed palette extent");
        for(std::size_t index=0;index<palette.size();++index) {
            require(current[index].red==palette[index].red && current[index].green==palette[index].green
                && current[index].blue==palette[index].blue,"native title pacing changed palette");
        }
    }
    require(!frontend.startup().titleActive(),"native title never released to credits");
    run.titleEnd=frontend.elapsedSamples();
    if(key==0 || key==0x1e61) {
        require(run.titleEnd-run.titleFirst>=minimumTitleSamples,"enhanced Stellar7 title disappeared before one second");
        require(run.titleEnd-run.titleFirst<minimumTitleSamples+SoundBlasterSynth::sampleRate/50,
            "native title pacing overshot its bounded deadline");
        if(game.cockpitSettings->options[2]!=0) {
            require(std::any_of(run.pcm.begin()+run.titleFirst,run.pcm.end(),[](const auto value){return value!=0;}),
                "native title pacing inserted silence instead of retained music");
        }
    } else {
        require(run.titleEnd-run.titleFirst<SoundBlasterSynth::sampleRate/50,"Escape did not dismiss the native title promptly");
    }
    // Reach the same menu/Begin boundary without importing later engine state.
    for(unsigned call=0;frontend.startup().phase()!=NativeStartupPhase::awaitingMenuDecision && call<maximumStartupPackets;++call) {
        NativeMenuInput input;
        if(frontend.startup().creditsActive()) {input.cancel=true;}
        advance(input);
    }
    require(frontend.startup().phase()==NativeStartupPhase::awaitingMenuDecision,"paced title did not return through credits to menu");
    NativeMenuInput begin;begin.confirm=true;advance(begin);advance({});
    for(unsigned call=0;!frontend.session() && call<128;++call) {advance({});}
    require(frontend.session()!=nullptr,"paced title did not enter retained gameplay");
    const auto* session=frontend.session();const auto* synth=&session->synth();
    unsigned adjacent=0;
    for(unsigned call=0;adjacent<2 && call<16;++call) {
        const auto previous=session->model().phase();const auto tick=session->model().level2LoopTick();
        collect(frontend.advanceGameplay({}));
        require(frontend.session()==session && &session->synth()==synth,"title pacing replaced retained gameplay/device owners");
        if(previous==GameplayPhase::playable && session->model().phase()==GameplayPhase::playable) {
            require(session->model().level2LoopTick()==static_cast<std::uint16_t>(tick+1),"paced title skipped a gameplay frame");++adjacent;
        }
    }
    require(adjacent==2,"paced title did not retain adjacent gameplay frames");
    std::cout<<"music="<<game.cockpitSettings->options[2]<<" budget="<<budget<<" key="<<key
        <<" title_samples="<<run.titleEnd-run.titleFirst<<" samples="<<run.pcm.size()<<'\n';
    return run;
}
}

int main(int argc,char** argv) {
    try {
        require(argc==2,"requires original resources");const std::filesystem::path root=argv[1];
        auto game=naturalRoute::assets(root);
        game.storyScenes=std::make_shared<const StorySceneAssets>(decodeStorySceneAssets(
            naturalRoute::read(root/"SCENEB1.RES"),naturalRoute::read(root/"SCENEA.RES"),
            {naturalRoute::read(root/"SCENEB2.RES"),naturalRoute::read(root/"SCENEB3.RES"),naturalRoute::read(root/"SCENEB5.RES")},
            naturalRoute::read(root/"SCENEC.RES"),naturalRoute::read(root/"VOICE.RES")));
        const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
        const auto drivers=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
        const auto copy=decodeFrameCopyProfile(drivers.at(0));
        game.cockpitSettings=CockpitSettings{81,{1,0,1,1,0,0}};game.highscoreTable->setConfiguration(*game.cockpitSettings);
        const auto a=title(game,art,copy,10000),b=title(game,art,copy,257);
        require(a.pcm==b.pcm && a.title==b.title && a.titleFirst==b.titleFirst && a.titleEnd==b.titleEnd,
            "native title pacing depends on packet budget");
        title(game,art,copy,257,0x1e61);title(game,art,copy,257,0x011b);
        title(game,art,copy,257,0x011b,NativeMenuFeatures::releaseMenus);
        game.cockpitSettings->options[2]=0;game.highscoreTable->setConfiguration(*game.cockpitSettings);
        title(game,art,copy,257);
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
