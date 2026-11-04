#include "playable_startup_resources.hpp"
#include "playable_attract.hpp"
#include "stellar7/story_scene.hpp"
#include <array>
#include <iostream>
#include <memory>

namespace stellar7 {
struct NativeSessionTestAccess {
    static SoundBlasterSynth& synth(NativeGameplaySession& session) {return session.synth_;}
    static PresentationClock& clock(NativeGameplaySession& session) {return session.clock_;}
};
}
namespace {
using namespace stellar7;
constexpr std::int64_t horizon=120000;
void require(const bool value,const char* message) {if(!value){throw std::runtime_error(message);}}
struct Result {
    std::vector<std::int16_t> pcm;
    std::array<std::vector<std::uint8_t>,3> pixels;
    PaletteWorkState palette;
    std::uint64_t cycles=0;
};
Result run(const GameplayAssets& game,const NativeStartupAssets& art,const FrameCopyProfile copy,const std::size_t budget) {
    RecoveredFrontend frontend(game,art,copy,budget,{},CampaignExecution::playable,
        NativePointerCapability::portablePointer,NativeMenuFeatures::releaseMenus);
    unsigned polls=0;
    for(unsigned calls=0;!frontend.session()&&calls<120000;++calls) {
        NativeMenuInput input{};
        if(frontend.startup().phase()==NativeStartupPhase::awaitingMenuDecision) {input.confirm=polls++==1;}
        frontend.advanceStartup(input);
    }
    require(frontend.session(),"title copy fixture missed actual Begin");
    auto& session=*const_cast<NativeGameplaySession*>(frontend.session());
    auto& synth=NativeSessionTestAccess::synth(session);
    auto& clock=NativeSessionTestAccess::clock(session);
    auto phase=*session.recoveredClockPhase();phase.callback=synth.callbackWorkState();
    phase.playableAutomaticAttract=true;phase.playableCampaign=true;
    IndexedPages pixels;auto pages=CockpitPageState::fromRecoveredMenu();
    for(const auto page:{pages.cachePage(),pages.backPage(),pages.frontPage()}) {
        for(std::int16_t y=0;y<200;++y) {for(std::int16_t x=0;x<320;++x) {pixels.write(page,{x,y},0);}}
    }
    PaletteWorkState palette;std::size_t component=0;
    for(const auto color:art.menuScreen.palette) {
        for(const auto value:{color.red,color.green,color.blue}) {palette.source[component++]=value>>2;}
    }
    palette=completedFullPaletteUpload(palette,{0,256,0,63});
    auto predicted=phase;auto expectedPalette=palette;std::uint64_t expectedCycles=0,expectedFirstCopy=0,expectedSecondCopy=0;
    const auto normal=[&](const std::vector<NormalWorkOperation>& operations) {
        const auto work=normalWorkTiming(predicted,operations,horizon);
        predicted=work.phase;expectedCycles+=work.total.elapsedCycles;
    };
    const auto titleCopy=[&]() {
        normal({{NormalWorkKind::instructions,1+8+1}});
        normal(planarCopyOperations(0,320,200));
        if(expectedFirstCopy==0) {expectedFirstCopy=expectedCycles;}
        else {expectedSecondCopy=expectedCycles;}
        normal({{NormalWorkKind::instructions,2}});
    };
    titleCopy();
    const auto blackout=blackoutClockTiming(predicted,{expectedPalette,{}},pages.frontPage(),2*horizon,BlackoutPageLayout::recoveredStartup);
    for(const auto& step:blackout.steps) {expectedCycles+=step.before.total.elapsedCycles+step.palette.timing.total.elapsedCycles;}
    expectedCycles+=blackout.clear.total.elapsedCycles;predicted=blackout.clear.phase;expectedPalette=blackout.state.palette;
    titleCopy();
    for(std::uint8_t intensity=0;intensity<=60;intensity+=4) {
        const auto work=paletteClockTiming(predicted,expectedPalette,{0,256,0,intensity},2*horizon);
        predicted=work.timing.phase;expectedPalette=work.state;expectedCycles+=work.timing.total.elapsedCycles;
    }
    NativeSessionDelivery delivery;delivery.firstSample=clock.elapsedSamples();std::int16_t skip=0;
    const auto beforeCpu=synth.driverState().normalClockCycles;
    std::uint64_t observedFirstCopy=0,observedSecondCopy=0;
    const auto sink=[&](const NativeSessionDelivery&) {
        if(pixels.snapshot(pages.backPage())!=art.menuScreen.pixels) {
            observedFirstCopy=synth.driverState().normalClockCycles-beforeCpu;
        } else if(pixels.snapshot(pages.frontPage())!=art.menuScreen.pixels) {
            observedSecondCopy=synth.driverState().normalClockCycles-beforeCpu;
        }
    };
    AttractPresentation p({pixels,pages,palette,synth,clock,phase,game,art,*game.cockpitSettings,skip,{}},delivery,budget,sink);
    p.titleScreen();
    const auto actualCycles=synth.driverState().normalClockCycles-beforeCpu;
    std::cout<<"budget="<<budget<<" expected_cycles="<<expectedCycles<<" actual_cycles="<<actualCycles
        <<" expected_copy_prefix="<<expectedFirstCopy<<" observed_copy_prefix="<<observedFirstCopy
        <<" expected_second_copy="<<expectedSecondCopy<<" observed_second_copy="<<observedSecondCopy<<'\n';
    require(observedFirstCopy==expectedFirstCopy,"title omitted recovered8484 work before applying its copy");
    require(observedSecondCopy==expectedSecondCopy,"title omitted recovered8484 work before applying its second copy");
    require(actualCycles==expectedCycles,"title omitted or duplicated recovered8484 copy work");
    require(delivery.firstSample+delivery.samples.size()==clock.elapsedSamples(),"title copy lost PCM continuity");
    Result result{delivery.samples,{pixels.snapshot(pages.cachePage()),pixels.snapshot(pages.backPage()),pixels.snapshot(pages.frontPage())},palette,actualCycles};
    for(const auto& image:result.pixels) {require(image==art.menuScreen.pixels,"title copy changed source screen pixels");}
    require(palette.dac.colors==expectedPalette.dac.colors,"title copy lost palette timing");
    return result;
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"requires actual resources");const std::filesystem::path root=argv[1];
        auto game=naturalRoute::assets(root);
        game.storyScenes=std::make_shared<const StorySceneAssets>(decodeStorySceneAssets(
            naturalRoute::read(root/"SCENEB1.RES"),naturalRoute::read(root/"SCENEA.RES"),
            {naturalRoute::read(root/"SCENEB2.RES"),naturalRoute::read(root/"SCENEB3.RES"),naturalRoute::read(root/"SCENEB5.RES")},
            naturalRoute::read(root/"SCENEC.RES"),naturalRoute::read(root/"VOICE.RES")));
        game.cockpitSettings=CockpitSettings{81,{1,0,0,1,0,0}};game.highscoreTable->setConfiguration(*game.cockpitSettings);
        const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
        const auto vga=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
        const auto a=run(game,art,decodeFrameCopyProfile(vga.at(0)),10000);
        const auto b=run(game,art,decodeFrameCopyProfile(vga.at(0)),257);
        require(a.pcm==b.pcm && a.pixels==b.pixels && a.palette.dac.colors==b.palette.dac.colors && a.cycles==b.cycles,
            "title copy packet budget changed source work, PCM, pages or palette");
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
