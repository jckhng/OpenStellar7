#include "playable_startup_resources.hpp"
#include "stellar7/native_attract.hpp"
#include <algorithm>
#include <iostream>
#include <set>
#include <tuple>

namespace {
using namespace stellar7;
void require(bool condition,const char* message) {if(!condition) {throw std::runtime_error(message);}}
struct Receipt {
    std::uint64_t pcmHash=1469598103934665603ULL;
    std::uint64_t samples=0;
    std::set<std::size_t> firstPages;
    std::set<NativeAttractStage> stages;
    std::vector<std::tuple<NativeAttractStage,std::size_t,std::size_t,std::uint64_t>> boundaries;
    bool speech=false;
};
GameplayAssets assets(const std::filesystem::path& root,const bool story) {
    auto data=naturalRoute::assets(root);
    const auto voice=naturalRoute::read(root/"VOICE.RES");
    require(!voice.empty(),"attract test requires actual opened VOICE.RES");data.attractVoiceAvailable=true;
    data.storyScenes=std::make_shared<const StorySceneAssets>(decodeStorySceneAssets(
        naturalRoute::read(root/"SCENEB1.RES"),naturalRoute::read(root/"SCENEA.RES"),
        {naturalRoute::read(root/"SCENEB2.RES"),naturalRoute::read(root/"SCENEB3.RES"),naturalRoute::read(root/"SCENEB5.RES")},
        naturalRoute::read(root/"SCENEC.RES"),voice));
    //Controlled configuration, actual resources. This is not an original route capture.
    data.cockpitSettings->options[5]=story?255:0;
    data.highscoreTable->setConfiguration(*data.cockpitSettings);return data;
}
Receipt route(const GameplayAssets& data,const NativeStartupAssets& art,const FrameCopyProfile copy,const std::size_t budget) {
    constexpr auto features=static_cast<NativeMenuFeatures>(11); //Historicalfull3 + automaticAttract8.
    RecoveredFrontend f(data,art,copy,budget,{},CampaignExecution::playable,NativePointerCapability::unavailable,features);
    const auto* startup=&f.startup();Receipt result;
    std::cout<<"attract cold story="<<data.cockpitSettings->options[5]<<" budget="<<budget<<" start"<<std::endl;
    const auto collect=[&](const NativeSessionDelivery& delivery) {
        require(delivery.firstSample==result.samples && !delivery.exitRequested,"attract lost contiguous retained PCM or requested host exit");
        for(const auto sample:delivery.samples) {
            const auto word=static_cast<std::uint16_t>(sample);
            for(const auto byte:{word&255U,std::uint32_t{word}>>8}) {result.pcmHash^=byte;result.pcmHash*=1099511628211ULL;}
        }
        result.samples+=delivery.samples.size();require(result.samples==f.elapsedSamples(),"attract replaced its retained clock epoch");
        require(&f.startup()==startup,"attract replaced startup owner");
        if(const auto progress=f.attractProgress()) {
            result.stages.insert(progress->stage);
            if(progress->stage==NativeAttractStage::briefing && progress->tours==0) {result.firstPages.insert(progress->page);}
            const auto boundary=std::make_tuple(progress->stage,progress->page,progress->tours);
            if(result.boundaries.empty() || std::make_tuple(std::get<0>(result.boundaries.back()),std::get<1>(result.boundaries.back()),std::get<2>(result.boundaries.back()))!=boundary) {
                result.boundaries.emplace_back(progress->stage,progress->page,progress->tours,result.samples);
                if(progress->stage!=NativeAttractStage::briefing||progress->page==0||progress->page==18) {
                    std::cout<<"attract stage="<<static_cast<int>(progress->stage)<<" page="<<progress->page
                        <<" tours="<<progress->tours<<" samples="<<result.samples<<std::endl;
                }
            }
            if(progress->stage==NativeAttractStage::intro) {result.speech=result.speech||f.startup().storyVoice().transferred>0;}
        }
    };
    for(unsigned call=0;f.startup().phase()!=NativeStartupPhase::awaitingMenuDecision && call<120000;++call) {collect(f.advanceStartup({}));}
    require(f.startup().phase()==NativeStartupPhase::awaitingMenuDecision,"attract route missed actual cold menu");
    bool second=false;
    for(unsigned call=0;call<2000000;++call) {
        collect(f.advanceStartup({}));
        const auto p=f.attractProgress();
        if(p && p->stage==NativeAttractStage::briefing && p->tours==1) {second=true;break;}
    }
    require(second && result.firstPages.size()==19 && *result.firstPages.begin()==0 && *result.firstPages.rbegin()==18,
        "actual expiry did not traverse19pages and return to secondBriefing");
    for(const auto stage:{NativeAttractStage::briefing,NativeAttractStage::intro,NativeAttractStage::credits,NativeAttractStage::postCredits}) {
        require(result.stages.count(stage)==1,"automatic tour omitted a retained source stage");
    }
    const bool expectedSpeech=data.cockpitSettings->options[5]!=0 && data.cockpitSettings->options[2]!=0;
    require(result.speech==expectedSpeech,"attract STORY/MUSIC did not control actual voice independently");
    //Actual Escape (no generated Enter) cancels02C2; retained scan exits nextpoll.
    while(!f.attractAcceptsSourceKey()) {collect(f.advanceStartup({}));}
    NativeMenuInput escape{};escape.sourceKey=0x011b;escape.cancel=true;collect(f.advanceStartup(escape));
    for(unsigned call=0;f.attractProgress() && call<20;++call) {collect(f.advanceStartup({}));}
    require(!f.attractProgress() && !f.session() && f.startup().menu().selection()==0,"actual Escape failed same-owner main-menu return");
    NativeMenuInput confirm{};confirm.confirm=true;collect(f.advanceStartup(confirm));collect(f.advanceStartup({}));
    for(unsigned call=0;!f.session() && call<128;++call) {collect(f.advanceStartup({}));}
    require(f.session()!=nullptr,"post-attract Begin missed its actual retained loader");
    const auto* session=f.session();const auto* synth=&session->synth();
    unsigned adjacent=0;
    for(unsigned call=0;adjacent<2 && call<16;++call) {
        const auto before=session->model().phase();const auto tick=session->model().level2LoopTick();
        const auto delivery=f.advanceGameplay({});
        //Startup has already moved into session, so do not dereference its audio.
        require(delivery.firstSample==result.samples && !delivery.exitRequested,"post-attract Begin lost PCM epoch");
        for(const auto sample:delivery.samples) {
            const auto word=static_cast<std::uint16_t>(sample);
            for(const auto byte:{word&255U,std::uint32_t{word}>>8}) {result.pcmHash^=byte;result.pcmHash*=1099511628211ULL;}
        }
        result.samples+=delivery.samples.size();
        require(f.session()==session && &session->synth()==synth && result.samples==f.elapsedSamples(),"post-attract Begin replaced owners");
        if(before==GameplayPhase::playable && session->model().phase()==GameplayPhase::playable) {
            require(session->model().level2LoopTick()==static_cast<std::uint16_t>(tick+1),"post-attract frames were not adjacent");++adjacent;
        }
    }
    require(adjacent==2 && session->model().campaignLevel()==0,"post-attract Begin failed adjacent Level1 frames");return result;
}
void run(const std::filesystem::path& root,const std::optional<bool> selectedStory) {
    const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto vga=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    const auto copy=decodeFrameCopyProfile(vga.at(0));
    for(const bool story:{false,true}) {
        if(selectedStory&&*selectedStory!=story) {continue;}
        const auto data=assets(root,story);const auto a=route(data,art,copy,10000),b=route(data,art,copy,257);
        require(a.pcmHash==b.pcmHash && a.samples==b.samples && a.boundaries==b.boundaries,
            "retained whole attract/Begin route changed across source packet budgets");
    }
}
}
int main(int argc,char** argv) {
    if(argc!=2&&argc!=3) {return 2;}
    try {
        std::optional<bool> story;
        if(argc==3) {
            const std::string value=argv[2];
            if(value!="0"&&value!="255") {return 2;}
            story=value=="255";
        }
        run(argv[1],story);std::cout<<"Actual expiry/Briefing19/logo-Story-credits/secondBriefing/Escape/retained Begin+2 PASS\n";return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
