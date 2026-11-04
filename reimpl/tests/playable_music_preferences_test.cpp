#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"
#include <algorithm>
#include <iostream>

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
class MemoryStore final:public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        bytes=encodeHighscoreTable(table);++calls;
        return {HighscoreSaveStatus::saved,decodeHighscoreTable(bytes)};
    }
    std::vector<std::uint8_t> bytes;
    unsigned calls=0;
};
std::unique_ptr<RecoveredFrontend> menu(const GameplayAssets& assets,const NativeStartupAssets& art,
    FrameCopyProfile copy,std::size_t budget,std::shared_ptr<HighscoreStore> store={}) {
    auto result=std::make_unique<RecoveredFrontend>(assets,art,copy,budget,std::move(store),CampaignExecution::playable);
    for(unsigned call=0;result->startup().phase()!=NativeStartupPhase::awaitingMenuDecision&&call<10000;++call) {
        result->advanceStartup({});
    }
    requireCampaignComparison(result->startup().phase()==NativeStartupPhase::awaitingMenuDecision,"Music startup missed actual menu");
    if (assets.cockpitSettings->options[2] == 0) {
        const auto& startup=result->startup();
        const auto* presentation=startup.recoveredPresentation();
        const auto* release=startup.recoveredLogoRelease();
        const auto* song=startup.recoveredMenuSong();
        requireCampaignComparison(presentation && presentation->loadedHead && !presentation->selectedHead
            && presentation->finalPhase.sequenceSelection && !presentation->finalPhase.sequenceSelection->published
            && presentation->finalPhase.sequenceSelection->prepared.sequence==assets.logoMusicSequence,
            "Cold MUSIC0 lost its real unpublished logo allocation");
        requireCampaignComparison(release && release->metadataReleased && release->finalPhase.sequenceSelection
            && release->finalPhase.sequenceSelection->logoTimerFallback
            && release->finalPhase.sequenceSelection->logoEmptyCurrentReturned
            && !release->finalPhase.sequenceSelection->logoCurrentCleared,
            "Cold MUSIC0 minted selected logo stop receipts or missed480 IRQ fallback");
        requireCampaignComparison(song && song->song.payload==assets.presentationMusicSequence
            && song->song.repeat==0xffff && !song->song.next
            && song->finalPhase.sequenceSelection && !song->finalPhase.sequenceSelection->published
            && song->finalPhase.sequenceSelection->prepared.sequence==song->song.payload,
            "Cold MUSIC0 lost its independent unpublished menu-song allocation");
        requireCampaignComparison(startup.recoveredLogo()->finalPhase.sequenceSelection->logoEventSuppressed
            ==(assets.cockpitSettings->options[3]==0),"Cold MUSIC0 coupled logo SOUNDS to MUSIC");
    }
    return result;
}
void begin(RecoveredFrontend& frontend) {
    frontend.advanceStartup({false,false,true});frontend.advanceStartup({});
    const auto* begin=frontend.startup().recoveredBegin();
    requireCampaignComparison(begin && begin->finalPhase.sequenceSelection,"Begin lost its retained phase");
    const auto& phase=begin->finalPhase;
    if (phase.playableStartup && phase.playableMusicOff) {
        requireCampaignComparison(phase.sequenceSelection->beginEmptyCurrentReturned
            && !phase.sequenceSelection->beginCurrentCleared && phase.timer->globalCount==0
            && phase.callback->domain==SoundBlasterCallbackDomain::level2RetryNoHandle,
            "MUSIC0 Begin minted a selected stop instead of returning empty51A6");
    }
    for(unsigned call=0;!frontend.session()&&call<10000;++call) {frontend.advanceStartup({});}
    requireCampaignComparison(frontend.session()!=nullptr,"Saved music setting missed actual Begin loader");
    frontend.advanceGameplay({});
}
void configure(GameplayAssets& assets,int music,int sounds) {
    assets.cockpitSettings->options[2]=static_cast<std::int16_t>(music);
    assets.cockpitSettings->options[3]=static_cast<std::int16_t>(sounds);
    assets.highscoreTable->setConfiguration(*assets.cockpitSettings);
}
void run(const std::filesystem::path& root) {
    const auto data=naturalRoute::assets(root);
    const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto drivers=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    const auto copy=decodeFrameCopyProfile(drivers.at(0));
    auto store=std::make_shared<MemoryStore>();auto ui=menu(data,art,copy,10000,store);
    const auto poll=[&](NativeMenuInput input) {
        const auto sample=ui->elapsedSamples();const auto delivery=ui->advanceStartup(input);
        requireCampaignComparison(delivery.firstSample==sample&&sample+delivery.samples.size()==ui->elapsedSamples(),
            "MUSIC menu lost retained PCM epoch");
    };
    const auto pulse=[&](NativeMenuInput input) {poll(input);poll({});};
    pulse({false,true,false});pulse({false,true,false});pulse({false,false,true}); //PREFS/EXIT.
    pulse({true,false,false});pulse({true,false,false});pulse({false,false,false,true}); //STORY/SOUNDS/MUSIC.
    pulse({false,false,true});
    requireCampaignComparison(ui->startup().recoveredMenuResources()->configuration.options[2]==0,
        "MUSIC On/Off has no effective setting consumer");
    requireCampaignComparison(ui->startup().recoveredSettings()->settings.configuration.options[2]==1,
        "MUSIC toggle changed immutable startup receipt");
    pulse({false,true,false});pulse({false,true,false});pulse({false,false,true}); //LEVEL/EXIT/save.
    requireCampaignComparison(store->calls==1&&!store->bytes.empty(),"MUSIC Exit did not save/reload");
    const auto encoded=encodeHighscoreTable(*data.highscoreTable);
    requireCampaignComparison(std::equal(encoded.begin()+highscoreHeaderBytes,encoded.end(),
        store->bytes.begin()+highscoreHeaderBytes),"Music save changed score/name bytes");
    begin(*ui); //Loaded1, switchedOff before firstBegin.
    auto saved=data;saved.highscoreTable=decodeHighscoreTable(store->bytes);
    saved.cockpitSettings=decodeCockpitSettings(std::vector<std::uint8_t>(store->bytes.begin(),store->bytes.begin()+highscoreHeaderBytes));
    auto cold=menu(saved,art,copy,10000);begin(*cold);
    requireCampaignComparison(!cold->session()->synth().driverState().sequenceSelected,
        "Saved MUSIC0 published a sequence on cold Begin");
    auto coldOn=menu(saved,art,copy,10000);
    const auto pulseCold=[&](NativeMenuInput input) {coldOn->advanceStartup(input);coldOn->advanceStartup({});};
    pulseCold({false,true,false});pulseCold({false,true,false});pulseCold({false,false,true});
    pulseCold({true,false,false});pulseCold({true,false,false});pulseCold({false,false,false,true});
    pulseCold({false,false,true}); //Cold0->PREFS MUSIC On before firstBegin.
    requireCampaignComparison(coldOn->startup().recoveredMenuResources()->configuration.options[2]==1
        && coldOn->startup().recoveredSettings()->settings.configuration.options[2]==0,
        "Cold MUSIC0->On changed historical configuration or missed current publication");
    pulseCold({false,true,false});pulseCold({false,true,false});pulseCold({false,false,true});
    begin(*coldOn);
    requireCampaignComparison(coldOn->session()->synth().driverState().sequenceSelected,
        "Cold MUSIC0->On did not enter selected Level1 music");
    bool rejected=false;
    try {
        RecoveredFrontend strict(saved,art,copy,10000,{},CampaignExecution::sourceWork);
        for(unsigned call=0;!strict.session() && call<10;++call) {strict.advanceStartup({});}
    } catch (const std::invalid_argument&) {rejected=true;}
    requireCampaignComparison(rejected,"Strict source startup admitted saved MUSIC0 native publication");
    for(int music=0;music<=1;++music) {
        for(int sounds=0;sounds<=1;++sounds) {
            auto assets=data;configure(assets,music,sounds);
            auto left=menu(assets,art,copy,10000),right=menu(assets,art,copy,257);begin(*left);begin(*right);
            test::CampaignBudgetComparison twins;
            const auto initial=left->session()->synth().driverState();bool directSeen=false;
            for(unsigned frame=0;frame<32;++frame) {
                GameplayInput input{};input.fire=true;
                const auto a=left->advanceGameplay(input),b=right->advanceGameplay(input);
                twins.compare(*left->session(),*right->session(),a,b,test::CampaignComparisonStage::boundary);
                const auto state=left->session()->synth().driverState();
                requireCampaignComparison(state.sequenceAllocated&&state.sequenceSelected==(music!=0),
                    "MUSIC/SOUNDS publication or descriptor allocation is wrong");
                if(music==0) {
                    requireCampaignComparison(state.sequencePosition==initial.sequencePosition&&state.sequenceWait==initial.sequenceWait,
                        "Unpublished music cursor/wait advanced during gameplay");
                }
                const bool active=std::any_of(state.directTimers.begin(),state.directTimers.end(),[](auto value){return value!=0;});
                if(sounds==0) {requireCampaignComparison(!active,"SOUNDS0 emitted direct effect timers");}
                directSeen=directSeen||active;
            }
            requireCampaignComparison(directSeen==(sounds!=0),"Sound effects did not remain independent from MUSIC");
        }
    }
    std::cout<<"MUSIC preference persistence/coldBegin/four audio profiles/budget twins PASS\n";
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(argv[1]);return 0;}catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
