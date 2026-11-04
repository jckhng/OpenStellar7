#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"
#include "stellar7/story_scene.hpp"
#include <algorithm>
#include <iostream>
#include <set>
#include <tuple>
#include <utility>

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
class MemoryStore final:public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        bytes=encodeHighscoreTable(table);++calls;return {HighscoreSaveStatus::saved,decodeHighscoreTable(bytes)};
    }
    unsigned calls=0;std::vector<std::uint8_t> bytes;
};
GameplayAssets assets(const std::filesystem::path& root) {
    auto data=naturalRoute::assets(root);
    data.storyScenes=std::make_shared<const StorySceneAssets>(decodeStorySceneAssets(
        naturalRoute::read(root/"SCENEB1.RES"),naturalRoute::read(root/"SCENEA.RES"),
        {naturalRoute::read(root/"SCENEB2.RES"),naturalRoute::read(root/"SCENEB3.RES"),naturalRoute::read(root/"SCENEB5.RES")},
        naturalRoute::read(root/"SCENEC.RES"),naturalRoute::read(root/"VOICE.RES"),
        naturalRoute::read(root/"DRAXON.RES")));
    return data;
}
void sameFrame(const IndexedScreen& a,const IndexedScreen& b) {
    requireCampaignComparison(a.pixels==b.pixels && a.palette.size()==b.palette.size(),"Story changed indexed pixels across budgets");
    for(std::size_t i=0;i<a.palette.size();++i) {
        requireCampaignComparison(a.palette[i].red==b.palette[i].red&&a.palette[i].green==b.palette[i].green
            &&a.palette[i].blue==b.palette[i].blue,"Story changed palette across budgets");
    }
}
void sameDelivery(const NativeSessionDelivery& a,const NativeSessionDelivery& b) {
    requireCampaignComparison(a.firstSample==b.firstSample&&a.samples==b.samples&&a.presentations.size()==b.presentations.size(),
        "Story lost retained PCM/presentation order across budgets");
    for(std::size_t i=0;i<a.presentations.size();++i) {
        requireCampaignComparison(a.presentations[i].sampleEnd==b.presentations[i].sampleEnd,"Story presentation timestamp changed");
        sameFrame(a.presentations[i].indexed,b.presentations[i].indexed);
    }
}
void begin(RecoveredFrontend& frontend) {
    frontend.advanceStartup({false,false,true});frontend.advanceStartup({});
    for(unsigned call=0;!frontend.session()&&call<128;++call) {frontend.advanceStartup({});}
    requireCampaignComparison(frontend.session()!=nullptr,"Story setting did not reach actual retained Begin");
    frontend.advanceGameplay({});frontend.advanceGameplay({});frontend.advanceGameplay({});
    requireCampaignComparison(frontend.session()->model().phase()==GameplayPhase::playable
        &&frontend.session()->model().campaignLevel()==0,"Story Begin failed adjacent ordinary frames");
}
void coldTwins(const GameplayAssets& data,const NativeStartupAssets& art,const FrameCopyProfile copy,const bool enabled) {
    RecoveredFrontend a(data,art,copy,10000,{},CampaignExecution::playable,NativePointerCapability::unavailable,NativeMenuFeatures::full);
    RecoveredFrontend b(data,art,copy,257,{},CampaignExecution::playable,NativePointerCapability::unavailable,NativeMenuFeatures::full);
    std::vector<std::int16_t> pcmA,pcmB;
    const auto collect=[&](const RecoveredFrontend& frontend,const NativeSessionDelivery& delivery,std::vector<std::int16_t>& pcm) {
        requireCampaignComparison(delivery.firstSample==pcm.size()&&delivery.firstSample+delivery.samples.size()==frontend.elapsedSamples(),
            "Story cold startup lost its contiguous PCM epoch");
        pcm.insert(pcm.end(),delivery.samples.begin(),delivery.samples.end());
    };
    const auto beforeIntro=[&](RecoveredFrontend& frontend,std::vector<std::int16_t>& pcm) {
        for(unsigned call=0;frontend.startup().phase()!=NativeStartupPhase::awaitingIntroInput&&call<20000;++call) {
            collect(frontend,frontend.advanceStartup({}),pcm);
        }
        requireCampaignComparison(frontend.startup().phase()==NativeStartupPhase::awaitingIntroInput,
            "Story prelude missed retained post-logo input boundary");
    };
    //The legacy logo/countdown exposes transport-sized polls. Align at the real
    //post-logo boundary before comparing complete native Story update deliveries.
    beforeIntro(a,pcmA);beforeIntro(b,pcmB);
    requireCampaignComparison(pcmA==pcmB,"Story prelude budgets changed whole PCM");
    using VoiceDescriptor=std::pair<std::size_t,std::uint64_t>;
    std::vector<VoiceDescriptor> expectedSegments,observedSegments;
    for(const auto& encoded:data.storyScenes->voices[0]) {
        const auto decoded=decodeSampledVoice(encoded);
        expectedSegments.emplace_back(decoded.samples.size(),std::uint64_t{SampledVoiceStream::outputRate}*(256U-decoded.timeConstant));
    }
    requireCampaignComparison(expectedSegments.size()==4
        &&std::set<VoiceDescriptor>(expectedSegments.begin(),expectedSegments.end()).size()==expectedSegments.size(),
        "B1 native segment proof requires four distinct actual VOICE descriptors");
    std::set<StoryScene> episodes;bool voice=false;
    for(unsigned call=0;a.startup().phase()==NativeStartupPhase::awaitingIntroInput&&call<20000;++call) {
        requireCampaignComparison(b.startup().phase()==NativeStartupPhase::awaitingIntroInput,"Story phase boundary changed across budgets");
        NativeSessionDelivery x,y;
        try {x=a.advanceStartup({});y=b.advanceStartup({});}
        catch(const std::exception& error) {
            throw std::runtime_error("cold Story scene="+std::to_string(a.startup().storyScene()?static_cast<unsigned>(*a.startup().storyScene()):99)
                +" page="+std::to_string(a.startup().storyPage())+": "+error.what());
        }
        sameDelivery(x,y);sameFrame(a.frame(),b.frame());
        collect(a,x,pcmA);collect(b,y,pcmB);
        if(a.startup().storyScene()) {episodes.insert(*a.startup().storyScene());}
        const auto observation=a.startup().storyVoice(),other=b.startup().storyVoice();
        const auto fields=[](const SampledVoiceObservation& value) {
            return std::tie(value.phase,value.transferred,value.sampleCount,value.clockPhase,value.clockPeriod,value.dacValue,value.speakerEnabled);
        };
        requireCampaignComparison(fields(observation)==fields(other),"Story sampled voice descriptor/transfer phase changed across budgets");
        voice=voice||observation.transferred>0;
        if(a.startup().storyScene()==StoryScene::bridgeOpening&&observation.transferred>0) {
            const VoiceDescriptor descriptor{observation.sampleCount,observation.clockPeriod};
            if(observedSegments.empty()||observedSegments.back()!=descriptor) {observedSegments.push_back(descriptor);}
        }
    }
    requireCampaignComparison(a.startup().phase()==NativeStartupPhase::awaitingMenuMusic
        &&b.startup().phase()==NativeStartupPhase::awaitingMenuMusic,"Story failed its returned menu-song boundary");
    constexpr std::size_t musicOption=2;
    const bool expectedVoice=enabled&&data.cockpitSettings->options[musicOption]!=0;
    requireCampaignComparison(enabled ? episodes==std::set<StoryScene>{StoryScene::bridgeOpening,StoryScene::earthOpening}
        &&voice==expectedVoice : episodes.empty()&&!voice,
        "STORY and MUSIC did not control real cold B1/A/speech independently");
    requireCampaignComparison(expectedVoice ? observedSegments==expectedSegments : observedSegments.empty(),
        "B1 did not transfer the actual four native sampled segments in source order");
    const auto toMenu=[&](RecoveredFrontend& frontend,std::vector<std::int16_t>& pcm) {
        for(unsigned call=0;frontend.startup().phase()!=NativeStartupPhase::awaitingMenuDecision&&call<20000;++call) {
            const auto phase=frontend.startup().phase();
            try {collect(frontend,frontend.advanceStartup({}),pcm);}
            catch(const std::exception& error) {throw std::runtime_error("Story menu phase="+std::to_string(static_cast<unsigned>(phase))+": "+error.what());}
        }
        requireCampaignComparison(frontend.startup().phase()==NativeStartupPhase::awaitingMenuDecision,"Story cold route missed menu");
    };
    toMenu(a,pcmA);toMenu(b,pcmB);sameFrame(a.frame(),b.frame());
    constexpr std::size_t soundsOption=3;
    if(data.cockpitSettings->options[soundsOption]==0) {
        const auto* songA=a.startup().recoveredMenuSong();const auto* songB=b.startup().recoveredMenuSong();
        requireCampaignComparison(songA&&songB&&songA->before.directTimers==std::array<std::uint16_t,3>{}
            &&songB->before.directTimers==std::array<std::uint16_t,3>{},
            "SOUNDS Off reached a Story direct effect timer");
    }
    requireCampaignComparison(pcmA==pcmB&&a.elapsedSamples()==b.elapsedSamples(),"Story cold route changed whole retained PCM");
    requireCampaignComparison(a.startup().storyVoice().phase==SampledVoicePhase::unloaded,"Cold Story leaked speech into menu");
    begin(a);begin(b);
    requireCampaignComparison(a.elapsedSamples()==b.elapsedSamples(),"Story cold Begin changed retained sample epoch");
    sameFrame(a.frame(),b.frame());
}
void run(const std::filesystem::path& root) {
    auto data=assets(root);data.cockpitSettings->options[5]=255;data.highscoreTable->setConfiguration(*data.cockpitSettings);
    const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto drivers=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    const auto copy=decodeFrameCopyProfile(drivers.at(0));coldTwins(data,art,copy,true);
    auto store=std::make_shared<MemoryStore>();
    RecoveredFrontend ui(data,art,copy,10000,store,CampaignExecution::playable,NativePointerCapability::unavailable,NativeMenuFeatures::full);
    for(unsigned call=0;ui.startup().phase()!=NativeStartupPhase::awaitingMenuDecision&&call<20000;++call) {ui.advanceStartup({});}
    const auto pulse=[&](NativeMenuInput input) {ui.advanceStartup(input);ui.advanceStartup({});};
    pulse({false,true,false});pulse({false,true,false});pulse({false,false,true}); //PREFS starts on EXIT.
    pulse({true,false,false});pulse({false,false,true}); //STORY/Off.
    requireCampaignComparison(ui.startup().recoveredMenuResources()->configuration.options[5]==0,"STORY preference is inert");
    pulse({false,true,false});pulse({false,false,true});
    requireCampaignComparison(store->calls==1&&!store->bytes.empty(),"STORY Exit failed durable header save/reload");
    const auto before=encodeHighscoreTable(*data.highscoreTable);
    requireCampaignComparison(std::equal(before.begin()+highscoreHeaderBytes,before.end(),store->bytes.begin()+highscoreHeaderBytes),
        "STORY save changed score/name records");
    auto saved=data;saved.highscoreTable=decodeHighscoreTable(store->bytes);
    saved.cockpitSettings=decodeCockpitSettings(std::vector<std::uint8_t>(store->bytes.begin(),store->bytes.begin()+highscoreHeaderBytes));
    coldTwins(saved,art,copy,false);
    pulse({false,true,false});pulse({false,true,false});pulse({false,false,true});
    pulse({true,false,false});pulse({false,false,true});
    requireCampaignComparison(ui.startup().recoveredMenuResources()->configuration.options[5]==255,
        "STORY Off->On preference is inert");
    pulse({false,true,false});pulse({false,false,true});
    requireCampaignComparison(store->calls==2,"STORY On failed second durable header save/reload");
    requireCampaignComparison(std::equal(before.begin()+highscoreHeaderBytes,before.end(),store->bytes.begin()+highscoreHeaderBytes),
        "STORY On second save changed score/name records");
    auto savedOn=data;savedOn.highscoreTable=decodeHighscoreTable(store->bytes);
    savedOn.cockpitSettings=decodeCockpitSettings(std::vector<std::uint8_t>(store->bytes.begin(),store->bytes.begin()+highscoreHeaderBytes));
    requireCampaignComparison(savedOn.cockpitSettings->options[5]==255,"STORY On second saved header did not encode ACTIVE");
    coldTwins(savedOn,art,copy,true);begin(ui);
    std::cout<<"Actual Story ACTIVE cold typewriter/B1/A/4segments, menu/save/coldSKIPPED/save/coldACTIVE/Begin+3 retained twins; original equality unqualified\n";
}
}
namespace {
enum class ColdAudioMode {musicOff,soundsOff,bothOff};
void audioModes(const std::filesystem::path& root) {
    constexpr std::size_t musicOption=2;
    constexpr std::size_t soundsOption=3;
    constexpr std::size_t storyOption=5;
    const auto original=assets(root);
    const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto drivers=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    requireCampaignComparison(drivers.size()==1,"cold audio mode requires one actual VGA driver");
    const auto copy=decodeFrameCopyProfile(drivers.front());
    for(const auto mode:{ColdAudioMode::musicOff,ColdAudioMode::soundsOff,ColdAudioMode::bothOff}) {
        auto data=original;
        data.cockpitSettings->options[storyOption]=255;
        data.cockpitSettings->options[musicOption]=mode==ColdAudioMode::soundsOff ? 1 : 0;
        data.cockpitSettings->options[soundsOption]=mode==ColdAudioMode::musicOff ? 1 : 0;
        data.highscoreTable->setConfiguration(*data.cockpitSettings);
        try {coldTwins(data,art,copy,true);}
        catch(const std::exception& error) {
            const auto name=mode==ColdAudioMode::musicOff ? "MusicOff/SoundsOn"
                : mode==ColdAudioMode::soundsOff ? "MusicOn/SoundsOff" : "MusicOff/SoundsOff";
            throw std::runtime_error(std::string(name)+": "+error.what());
        }
    }
    std::cout<<"Configured full-profile ACTIVE Story cold audio cases passed retained10000/257; no durable edit/original scheduling claim\n";
}
}
int main(int argc,char** argv) {
    try {
        if(argc==3&&std::string(argv[2])=="audio-modes") {audioModes(argv[1]);}
        else if(argc==2) {run(argv[1]);}
        else {throw std::invalid_argument("requires DATA [audio-modes]");}
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
