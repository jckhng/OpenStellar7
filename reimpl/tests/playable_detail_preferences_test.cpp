#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"

#include <algorithm>
#include <array>
#include <iostream>

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
constexpr ProjectedPoint mainPreferences{163,126},mainBegin{163,69},preferencesExit{163,170};
constexpr ProjectedPoint detail70{189,46},detail51{170,46},detail50{169,46};
class MemoryStore final:public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        bytes=encodeHighscoreTable(table);++calls;
        return {HighscoreSaveStatus::saved,decodeHighscoreTable(bytes)};
    }
    unsigned calls=0;
    std::vector<std::uint8_t> bytes;
};
NativeMenuInput pointer(const ProjectedPoint point,const bool held) {
    NativeMenuInput input;input.pointer=NativePointerInput{point,held,false};return input;
}
void collect(const RecoveredFrontend& frontend,const NativeSessionDelivery& delivery,std::vector<std::int16_t>& pcm) {
    requireCampaignComparison(delivery.firstSample==pcm.size()
        &&delivery.firstSample+delivery.samples.size()==frontend.elapsedSamples(),"detail frontend lost contiguous PCM");
    pcm.insert(pcm.end(),delivery.samples.begin(),delivery.samples.end());
}
std::unique_ptr<RecoveredFrontend> menu(const GameplayAssets& assets,const NativeStartupAssets& art,const FrameCopyProfile copy,
    const std::size_t budget,std::vector<std::int16_t>& pcm,std::shared_ptr<HighscoreStore> store={}) {
    auto result=std::make_unique<RecoveredFrontend>(assets,art,copy,budget,std::move(store),CampaignExecution::playable,
        NativePointerCapability::portablePointer);
    for(unsigned call=0;result->startup().phase()!=NativeStartupPhase::awaitingMenuDecision&&call<10000;++call) {
        collect(*result,result->advanceStartup({}),pcm);
    }
    requireCampaignComparison(result->startup().phase()==NativeStartupPhase::awaitingMenuDecision,
        "native detail cold startup missed actual menu");
    return result;
}
void headless51(const GameplayAssets& original,const NativeStartupAssets& art,const FrameCopyProfile copy) {
    auto assets=original;
    constexpr std::array<std::int16_t,6> loadedOptions{1,0,1,1,0,0};
    constexpr std::array<std::int16_t,6> effectiveOptions{0,0,1,1,0,0}; //Real absent-device MOUSE1->0, STORY0/audioOn only.
    assets.cockpitSettings->viewSize=51;assets.cockpitSettings->options=loadedOptions;
    assets.highscoreTable->setConfiguration(*assets.cockpitSettings);
    const auto begin=[&](const std::size_t budget,std::vector<std::int16_t>& pcm) {
        auto result=std::make_unique<RecoveredFrontend>(assets,art,copy,budget,std::shared_ptr<HighscoreStore>{},
            CampaignExecution::playable,NativePointerCapability::unavailable);
        unsigned menuPoll=0;
        for(unsigned call=0;!result->session()&&call<10000;++call) {
            NativeMenuInput input;
            if(result->startup().phase()==NativeStartupPhase::awaitingMenuDecision) {
                const auto& configuration=result->startup().recoveredMenuResources()->configuration;
                requireCampaignComparison(configuration.viewSize==51&&configuration.options==effectiveOptions
                    &&result->startup().recoveredSettings()->settings.configuration.options==effectiveOptions,
                    "headless51 canonical cold settings differ from the named supported case");
                input.confirm=menuPoll++==1;
            }
            const auto delivery=result->advanceStartup(input);
            requireCampaignComparison(!delivery.pointerPosition,"headless51 published portable pointer feedback");
            collect(*result,delivery,pcm);
        }
        requireCampaignComparison(result->session()&&menuPoll==3,
            "headless51 actual released/Enter/released Begin did not retain a Level1 owner");
        return result;
    };
    std::vector<std::int16_t> leftPcm,rightPcm;
    auto left=begin(10000,leftPcm),right=begin(257,rightPcm);
    requireCampaignComparison(leftPcm==rightPcm&&left->elapsedSamples()==right->elapsedSamples(),
        "headless51 STORY0/audioOn cold Begin budgets changed whole PCM");
    test::CampaignBudgetComparison twins;
    std::uint16_t adoptionTick=0;
    for(unsigned frame=0;frame<3;++frame) {
        const auto a=left->advanceGameplay({}),b=right->advanceGameplay({});
        collect(*left,a,leftPcm);collect(*right,b,rightPcm);
        twins.compare(*left->session(),*right->session(),a,b,test::CampaignComparisonStage::boundary);
        requireCampaignComparison(!a.pointerPosition&&!b.pointerPosition,
            "headless51 adoption published portable pointer feedback");
        if(frame==0) {adoptionTick=left->session()->model().level2LoopTick();}
    }
    requireCampaignComparison(leftPcm==rightPcm&&left->session()->model().phase()==GameplayPhase::playable
        &&left->session()->model().campaignLevel()==0
        &&left->session()->model().runtimeDifficulty()==CampaignDifficulty::easy
        &&left->session()->model().level2LoopTick()==static_cast<std::uint16_t>(adoptionTick+2),
        "headless51 adoption plus two ordinary frames lost bounded native state or PCM");
}

void route(const std::filesystem::path& root) {
    auto assets=naturalRoute::assets(root);
    assets.cockpitSettings->viewSize=82;assets.cockpitSettings->options[0]=1;
    assets.highscoreTable->setConfiguration(*assets.cockpitSettings);
    const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto vga=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    const auto copy=decodeFrameCopyProfile(vga.at(0));
    auto store=std::make_shared<MemoryStore>();
    std::vector<std::int16_t> uiPcm;
    auto ui=menu(assets,art,copy,10000,uiPcm,store);
    const auto poll=[&](const NativeMenuInput input) {
        const auto delivery=ui->advanceStartup(input);collect(*ui,delivery,uiPcm);return delivery;
    };
    const auto click=[&](const ProjectedPoint point) {poll(pointer(point,true));return poll(pointer(point,false));};
    click(mainPreferences);
    poll(pointer(detail70,true));
    requireCampaignComparison(ui->startup().recoveredMenuResources()->configuration.viewSize==70,
        "held GRAPHIC DETAIL has no canonical main-menu consumer");
    poll(pointer(detail51,true));
    requireCampaignComparison(ui->startup().recoveredMenuResources()->configuration.viewSize==51,"legal detail51 was rejected");
    poll(pointer(detail50,true));
    requireCampaignComparison(ui->startup().recoveredMenuResources()->configuration.viewSize==51,
        "unsupported detail50 mutated canonical configuration");
    poll(pointer(detail70,true));poll(pointer(detail70,false));
    click(preferencesExit);
    requireCampaignComparison(store->calls==1&&ui->startup().recoveredMenuResources()->configuration.viewSize==70
        &&ui->startup().recoveredSettings()->settings.configuration.viewSize==82,
        "detail save/reload lost canonical70 or changed immutable initial82");
    const auto original=encodeHighscoreTable(*assets.highscoreTable);
    requireCampaignComparison(std::equal(original.begin()+highscoreHeaderBytes,original.end(),store->bytes.begin()+highscoreHeaderBytes),
        "detail preference save changed score/name records");
    click(mainBegin);
    for(unsigned call=0;!ui->session()&&call<64;++call) {poll({});}
    requireCampaignComparison(ui->session()!=nullptr,"changed detail70 did not enter its retained Begin loader");
    ui->advanceGameplay({});
    const auto edited70Frame=ui->frame();
    const auto edited70Phase=ui->session()->model().phase();
    const auto edited70Tick=ui->session()->model().level2LoopTick();
    requireCampaignComparison(edited70Phase==GameplayPhase::playable&&ui->session()->model().campaignLevel()==0,
        "edited70 frame is not the common first Level1 adoption boundary");
    //Different UI histories may have different absolute PCM epochs; compare only this shared model/frame boundary.
    std::vector<std::int16_t> full82Pcm;
    auto full82=menu(assets,art,copy,10000,full82Pcm);
    const auto full82Poll=[&](const NativeMenuInput input) {
        const auto delivery=full82->advanceStartup(input);collect(*full82,delivery,full82Pcm);
    };
    full82Poll(pointer(mainBegin,true));full82Poll(pointer(mainBegin,false));
    for(unsigned call=0;!full82->session()&&call<64;++call) {full82Poll({});}
    requireCampaignComparison(full82->session()!=nullptr,"fresh82 pixel control missed actual Begin loader");
    full82->advanceGameplay({});
    requireCampaignComparison(full82->session()->model().phase()==edited70Phase
        &&full82->session()->model().level2LoopTick()==edited70Tick&&full82->session()->model().campaignLevel()==0,
        "fresh82 control is not at the same first adoption model boundary");
    requireCampaignComparison(full82->frame().pixels!=edited70Frame.pixels,
        "actual specimen70/82 first frames have no visible indexed-pixel difference; detail pixel consumer remains unqualified");

    auto saved=assets;
    saved.highscoreTable=decodeHighscoreTable(store->bytes);
    saved.cockpitSettings=decodeCockpitSettings(std::vector<std::uint8_t>(store->bytes.begin(),store->bytes.begin()+highscoreHeaderBytes));
    std::vector<std::int16_t> leftPcm,rightPcm;
    auto left=menu(saved,art,copy,10000,leftPcm),right=menu(saved,art,copy,257,rightPcm);
    requireCampaignComparison(leftPcm==rightPcm&&left->elapsedSamples()==right->elapsedSamples()
        &&left->startup().recoveredSettings()->settings.configuration.viewSize==70
        &&right->startup().recoveredSettings()->settings.configuration.viewSize==70,
        "saved detail70 cold budgets lost configuration or whole PCM");
    test::compareCampaignFrame(left->frame(),right->frame());
    for(const auto page:{NativeMenuPage::cache,NativeMenuPage::back,NativeMenuPage::front}) {
        requireCampaignComparison(left->startup().menuPagePixels(page)==right->startup().menuPagePixels(page),
            "saved detail cold budgets changed retained menu pages");
    }
    const auto step=[&](const NativeMenuInput input) {
        const auto a=left->advanceStartup(input),b=right->advanceStartup(input);
        collect(*left,a,leftPcm);collect(*right,b,rightPcm);
        requireCampaignComparison(a.firstSample==b.firstSample&&a.samples==b.samples
            &&a.events.size()==b.events.size()&&a.presentations.size()==b.presentations.size()
            &&a.pointerPosition.has_value()==b.pointerPosition.has_value(),"detail Begin budgets changed ordered delivery");
        if(a.pointerPosition) {requireCampaignComparison(a.pointerPosition->x==b.pointerPosition->x
            &&a.pointerPosition->y==b.pointerPosition->y,"detail budgets changed source pointer feedback");}
        for(std::size_t i=0;i<a.events.size();++i) {
            requireCampaignComparison(a.events[i].loopTick==b.events[i].loopTick&&a.events[i].eventId==b.events[i].eventId,
                "detail Begin budgets changed ordered audio events");
        }
        for(std::size_t i=0;i<a.presentations.size();++i) {
            requireCampaignComparison(a.presentations[i].sampleEnd==b.presentations[i].sampleEnd,
                "detail Begin budgets changed presentation endpoint");
            test::compareCampaignFrame(a.presentations[i].indexed,b.presentations[i].indexed);
        }
        test::compareCampaignFrame(left->frame(),right->frame());
    };
    step(pointer(mainBegin,true));step(pointer(mainBegin,false));
    for(unsigned call=0;!left->session()&&call<64;++call) {step({});}
    requireCampaignComparison(left->session()&&right->session(),"saved detail70 cold Begin did not retain session owners");
    test::CampaignBudgetComparison twins;
    for(unsigned frame=0;frame<3;++frame) {
        const auto a=left->advanceGameplay({}),b=right->advanceGameplay({});
        collect(*left,a,leftPcm);collect(*right,b,rightPcm);
        twins.compare(*left->session(),*right->session(),a,b,test::CampaignComparisonStage::boundary);
        if(frame==0) {
            requireCampaignComparison(left->session()->model().phase()==edited70Phase
                &&left->session()->model().level2LoopTick()==edited70Tick&&left->session()->model().campaignLevel()==0,
                "saved cold70 frame is not at the edited70 common adoption boundary");
            test::compareCampaignFrame(edited70Frame,left->frame());
        }
    }
    requireCampaignComparison(leftPcm==rightPcm,"detail UI/save/coldBegin budgets changed whole ordered PCM");
    headless51(assets,art,copy);
    std::cout<<"Bounded native GRAPHIC DETAIL51..81: real70/82 pixels/save/cold70 and headless51 STORY0/audioOn budget twins PASS\n";
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {route(argv[1]);return 0;}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
