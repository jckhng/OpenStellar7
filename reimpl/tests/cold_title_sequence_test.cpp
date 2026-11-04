#include "playable_startup_resources.hpp"
#include <iostream>

namespace {
using namespace stellar7;
void require(const bool value,const char* message) {
    if (!value) { throw std::runtime_error(message); }
}
std::vector<std::uint8_t> titleWithOverlays(const GameplayAssets& game,const NativeStartupAssets& art) {
    auto pixels=art.menuScreen.pixels;
    enum class Blit { transparent, opaque };
    const auto paint=[&](const IndexedBitmap& bitmap,const int left,const int top,const Blit mode) {
        for (int y=0;y<bitmap.height;++y) {
            for (int x=0;x<bitmap.width;++x) {
                const auto color=bitmap.pixels[y*bitmap.width+x];
                if (left+x<320 && top+y<200 && (color!=0 || mode==Blit::opaque)) { pixels[(top+y)*320+left+x]=color; }
            }
        }
    };
    require(game.artGroups.at(2).size()==2,"title group must have two original overlays");
    paint(game.artGroups.at(2).at(0),70,10,Blit::transparent);
    paint(game.artGroups.at(2).at(1),56,193,Blit::opaque);
    return pixels;
}
struct TitleRun {
    std::vector<std::int16_t> pcm;
    std::uint64_t base=0,overlays=0,credits=0;
};
TitleRun natural(const GameplayAssets& game,const NativeStartupAssets& art,const FrameCopyProfile copy,const std::size_t budget) {
    RecoveredFrontend frontend(game,art,copy,budget,{},CampaignExecution::playable,
        NativePointerCapability::portablePointer,NativeMenuFeatures::releaseMenus);
    const auto expected=titleWithOverlays(game,art);
    PaletteWorkState palette;std::size_t component=0;
    for(const auto color:art.menuScreen.palette) {
        for(const auto value:{color.red,color.green,color.blue}) { palette.source[component++]=value>>2; }
    }
    const auto selected=completedFullPaletteUpload(palette,{0,256,0,60});
    const auto titlePalette=expandVgaDacPalette({selected.dac.colors.begin(),selected.dac.colors.end()});
    TitleRun result;unsigned titleQueries=0;
    for(unsigned count=0;frontend.startup().phase()!=NativeStartupPhase::awaitingMenuConstruction && count<120000;++count) {
        const auto first=frontend.elapsedSamples();
        const bool titleQuery=frontend.startup().titleActive() && frontend.startup().phase()==NativeStartupPhase::awaitingMenuResources;
        if(titleQuery) { ++titleQueries; }
        const auto delivery=frontend.advanceStartup({});
        require(delivery.firstSample==first && first+delivery.samples.size()==frontend.elapsedSamples(),"title lost retained PCM");
        result.pcm.insert(result.pcm.end(),delivery.samples.begin(),delivery.samples.end());
        for(const auto& frame:delivery.presentations) {
            if(frame.indexed.pixels==art.menuScreen.pixels && result.base==0) { result.base=frame.sampleEnd; }
            if(frame.indexed.pixels==expected) {
                require(result.base!=0 && frame.sampleEnd>=result.base,"title overlays preceded SCR1");
                if(result.overlays==0) { result.overlays=frame.sampleEnd; }
                require(frame.indexed.palette.size()==titlePalette.size(),"title palette extent changed");
                for(std::size_t index=0;index<titlePalette.size();++index) {
                    const auto a=frame.indexed.palette[index],b=titlePalette[index];
                    require(a.red==b.red && a.green==b.green && a.blue==b.blue,"title did not retain source PAL1");
                }
            }
        }
    }
    require(result.base!=0 && result.overlays!=0,"cold startup omitted title before credits");
    require(titleQueries==2,"cold title did not consume both nonblocking queries");
    result.credits=frontend.elapsedSamples();
    require(result.credits-result.overlays<2*SoundBlasterSynth::sampleRate,"cold title inserted an attract countdown hold");
    require(frontend.startup().phase()==NativeStartupPhase::awaitingMenuConstruction,"title did not reach credits caller");
    std::cout<<"story="<<unsigned(game.cockpitSettings->options[5])<<" music="<<unsigned(game.cockpitSettings->options[2])
        <<" budget="<<budget<<" base="<<result.base<<" overlays="<<result.overlays<<" credits="<<result.credits<<'\n';
    return result;
}
void titleKey(const GameplayAssets& game,const NativeStartupAssets& art,const FrameCopyProfile copy,
    const std::uint16_t key,const unsigned query) {
    RecoveredFrontend frontend(game,art,copy,257,{},CampaignExecution::playable,
        NativePointerCapability::portablePointer,NativeMenuFeatures::releaseMenus);
    for(unsigned count=0;(!frontend.startup().titleActive() || frontend.startup().phase()!=NativeStartupPhase::awaitingMenuResources)
        && count<120000;++count) { frontend.advanceStartup({}); }
    require(frontend.startup().titleActive(),"title key fixture missed actual source query");
    bool rejected=false;
    try { const_cast<NativeStartupSequence&>(frontend.startup()).initializeMenuResources(); }
    catch(const std::logic_error&) { rejected=true; }
    require(rejected,"direct resource setup bypassed title queries");
    if(query==2) { frontend.advanceStartup({}); }
    NativeMenuInput input;input.sourceKey=key;frontend.advanceStartup(input);
    const bool escape=(key>>8)==1;
    if(!escape && query==1) { require(frontend.startup().titleActive(),"ordinary title key acted as Escape");frontend.advanceStartup({}); }
    require(!frontend.startup().titleActive(),"title key did not complete its nonblocking path");
    frontend.advanceStartup({});frontend.advanceStartup({});
    const auto& credits=*frontend.startup().recoveredCredits();
    require(escape ? credits.finalPhase.sequenceSelection->creditsSkipReturned
        : credits.finalPhase.sequenceSelection->creditsCountdownWritten,"title key wrote the wrong source19CA result");
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"requires original resource root");const std::filesystem::path root=argv[1];
        auto game=naturalRoute::assets(root);
        game.storyScenes=std::make_shared<const StorySceneAssets>(decodeStorySceneAssets(
            naturalRoute::read(root/"SCENEB1.RES"),naturalRoute::read(root/"SCENEA.RES"),
            {naturalRoute::read(root/"SCENEB2.RES"),naturalRoute::read(root/"SCENEB3.RES"),naturalRoute::read(root/"SCENEB5.RES")},
            naturalRoute::read(root/"SCENEC.RES"),naturalRoute::read(root/"VOICE.RES")));
        const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
        const auto drivers=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
        const auto copy=decodeFrameCopyProfile(drivers.at(0));
        for(const auto profile:std::array<std::array<std::int16_t,2>,3>{{{0,1},{255,1},{0,0}}}) {
            game.cockpitSettings=CockpitSettings{81,{1,0,profile[1],1,0,profile[0]}};
            game.highscoreTable->setConfiguration(*game.cockpitSettings);
            const auto a=natural(game,art,copy,10000),b=natural(game,art,copy,257);
            require(a.pcm==b.pcm && a.base==b.base && a.overlays==b.overlays && a.credits==b.credits,
                "title changed its retained source timeline across budgets");
        }
        titleKey(game,art,copy,0x1e61,1);titleKey(game,art,copy,0x011b,1);titleKey(game,art,copy,0x011b,2);
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
