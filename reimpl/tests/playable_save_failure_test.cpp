#include "playable_menu_dialog.hpp"
#include "playable_menu_pages.hpp"
#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"

#include <iostream>
#include <chrono>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
constexpr std::size_t musicOption=2;
constexpr std::string_view failureNotice="SAVE FAILED - RETRY EXIT/ESC UNDO";

class ResultStore final:public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        ++calls;
        if(status!=HighscoreSaveStatus::saved||omitReload) {return {status,std::nullopt};}
        bytes=encodeHighscoreTable(table);
        return {status,decodeHighscoreTable(bytes)};
    }
    HighscoreSaveStatus status=HighscoreSaveStatus::openFailed;
    bool omitReload=false;
    unsigned calls=0;
    std::vector<std::uint8_t> bytes;
};

MenuDialogResult activate(PlayableMenuDialog& dialog,CockpitSettings& configuration,
    HighscoreTable& scores,HighscoreStore* store) {
    dialog.advance({false,false,true},configuration,scores,store);
    return dialog.advance({},configuration,scores,store);
}

void expectNotice(PlayableMenuDialog& dialog,const GameplayAssets& assets,
    const CockpitSettings& configuration,const HighscoreTable& scores) {
    constexpr std::uint16_t back=0xa000,front=0xa800;
    IndexedPages actual,expected;
    dialog.paint(actual,assets,configuration,scores,back,front);
    expected.apply(preferencesPageEffects(assets.artGroups.at(0),assets.titleFont,assets.scoreFont,
        configuration,PreferenceControl::exit,std::nullopt,failureNotice,back));
    expected.apply(std::vector<IndexedPageEffect>{RadarPageCopy{back,front,{0,0},320,200}});
    paintPlayableMenuPointer(expected,assets.artGroups.at(0).at(0),dialog.pointerPosition(),back,front);
    requireCampaignComparison(actual.snapshot(front)==expected.snapshot(front),"failed save has no visible recovery instruction");
}

void dialogFailures(const GameplayAssets& assets) {
    for(const auto status:{HighscoreSaveStatus::openFailed,HighscoreSaveStatus::writeFailed,
        HighscoreSaveStatus::reloadFailed,HighscoreSaveStatus::saved}) {
        auto scores=*assets.highscoreTable;
        const auto original=encodeHighscoreTable(scores);
        auto configuration=*assets.cockpitSettings;
        configuration.options[0]=0; //Actual absent-device normalization differs from raw FAME header.
        const auto entry=configuration;
        PlayableMenuDialog dialog(PlayableMenuPage::preferences,assets);
        ResultStore store;store.status=status;store.omitReload=status==HighscoreSaveStatus::saved;
        dialog.advance({},configuration,scores,&store);
        configuration.options[musicOption]^=1; //The live canonical config is edited; its entry undo value is retained.
        requireCampaignComparison(activate(dialog,configuration,scores,&store)==MenuDialogResult::pending
            &&dialog.backAvailable()&&store.calls==1&&encodeHighscoreTable(scores)==original,
            "failed save threw, closed preferences or changed resident score/header bytes");
        expectNotice(dialog,assets,configuration,scores);
        NativeMenuInput undo;undo.cancel=true;
        requireCampaignComparison(dialog.advance(undo,configuration,scores,&store)==MenuDialogResult::closed
            &&configuration.viewSize==entry.viewSize&&configuration.options==entry.options
            &&encodeHighscoreTable(scores)==original&&store.calls==1,
            "failed-save Escape did not restore effective entry settings without another write");
    }

    auto scores=*assets.highscoreTable;auto configuration=*assets.cockpitSettings;
    const auto original=encodeHighscoreTable(scores);
    PlayableMenuDialog retry(PlayableMenuPage::preferences,assets);ResultStore store;
    retry.advance({},configuration,scores,&store);configuration.options[musicOption]^=1;
    requireCampaignComparison(activate(retry,configuration,scores,&store)==MenuDialogResult::pending,"save failure did not retain retry");
    store.status=HighscoreSaveStatus::saved;
    requireCampaignComparison(activate(retry,configuration,scores,&store)==MenuDialogResult::preferencesSaved&&store.calls==2,
        "recovered save did not retry through the existing store/reload path");
    const auto saved=encodeHighscoreTable(scores);
    requireCampaignComparison(saved==store.bytes&&std::equal(original.begin()+highscoreHeaderBytes,original.end(),
        saved.begin()+highscoreHeaderBytes),"successful retry changed score/name bytes");
}

void files(const GameplayAssets& assets,const std::filesystem::path& output) {
    requireCampaignComparison(!std::filesystem::exists(output),"save test requires a fresh output directory");
    std::filesystem::create_directory(output);
    auto configuration=*assets.cockpitSettings;auto scores=*assets.highscoreTable;
    FileHighscoreStore missing(output/"absent"/highscoreFileName);
    PlayableMenuDialog dialog(PlayableMenuPage::preferences,assets);
    requireCampaignComparison(activate(dialog,configuration,scores,&missing)==MenuDialogResult::pending
        &&!std::filesystem::exists(output/"absent"),"missing-parent save escaped recovery or created installation data");

    const auto path=output/highscoreFileName;
    requireCampaignComparison(saveHighscoreTable(path,scores).status==HighscoreSaveStatus::saved,"cannot seed private denied-write fixture");
    const auto before=naturalRoute::read(path);
#ifdef _WIN32
    const auto denied=CreateFileW(path.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    requireCampaignComparison(denied!=INVALID_HANDLE_VALUE,"cannot acquire private sharing-denied fixture");
    FileHighscoreStore locked(path);PlayableMenuDialog deniedDialog(PlayableMenuPage::preferences,assets);
    try {
        requireCampaignComparison(activate(deniedDialog,configuration,scores,&locked)==MenuDialogResult::pending,
            "sharing-denied save did not remain recoverable");
    } catch(...) {CloseHandle(denied);throw;}
    CloseHandle(denied);
    requireCampaignComparison(naturalRoute::read(path)==before,"denied save modified prior FAME bytes");
    requireCampaignComparison(activate(deniedDialog,configuration,scores,&locked)==MenuDialogResult::preferencesSaved,
        "unlocking the existing file did not permit an ordinary retry");
#endif
    requireCampaignComparison(!loadHighscoreTable(output/"missing.hal"),"missing score file was accepted");
    bool missingSettings=false;
    try {loadCockpitSettings(output/"absent");}catch(const std::runtime_error&) {missingSettings=true;}
    requireCampaignComparison(missingSettings&&!std::filesystem::exists(output/"absent"),
        "missing settings were initialized or written without valid player data");
    const auto malformed=output/"malformed";std::filesystem::create_directory(malformed);
    const auto malformedPath=malformed/highscoreFileName;
    {std::ofstream file(malformedPath,std::ios::binary);file<<"broken";}
    const auto broken=naturalRoute::read(malformedPath);
    requireCampaignComparison(!loadHighscoreTable(malformedPath),"malformed score file was accepted");
    bool rejected=false;
    try {loadCockpitSettings(malformed);}catch(const std::runtime_error&) {rejected=true;}
    requireCampaignComparison(rejected&&naturalRoute::read(malformedPath)==broken,"malformed settings were reset or overwritten");
    auto badTail=std::vector<std::uint8_t>(scores.header().begin(),scores.header().end());badTail.push_back(1);
    {std::ofstream file(malformedPath,std::ios::binary|std::ios::trunc);
        file.write(reinterpret_cast<const char*>(badTail.data()),static_cast<std::streamsize>(badTail.size()));}
    requireCampaignComparison(!loadHighscoreTable(malformedPath)&&naturalRoute::read(malformedPath)==badTail,
        "malformed score tail was accepted or silently rewritten");
}

void cold(const GameplayAssets& assets,const NativeStartupAssets& art,const FrameCopyProfile copy) {
    auto leftStore=std::make_shared<ResultStore>(),rightStore=std::make_shared<ResultStore>();
    auto left=std::make_unique<RecoveredFrontend>(assets,art,copy,10000,leftStore,CampaignExecution::playable);
    auto right=std::make_unique<RecoveredFrontend>(assets,art,copy,257,rightStore,CampaignExecution::playable);
    std::vector<std::int16_t> leftPcm,rightPcm;
    const auto toMenu=[&](RecoveredFrontend& frontend,std::vector<std::int16_t>& pcm) {
        for(unsigned call=0;frontend.startup().phase()!=NativeStartupPhase::awaitingMenuDecision&&call<10000;++call) {
            const auto delivery=frontend.advanceStartup({});pcm.insert(pcm.end(),delivery.samples.begin(),delivery.samples.end());
        }
        requireCampaignComparison(frontend.startup().phase()==NativeStartupPhase::awaitingMenuDecision,"save cold fixture missed menu");
    };
    toMenu(*left,leftPcm);toMenu(*right,rightPcm);
    const auto step=[&](const NativeMenuInput input=NativeMenuInput{}) {
        const auto a=left->advanceStartup(input),b=right->advanceStartup(input);
        leftPcm.insert(leftPcm.end(),a.samples.begin(),a.samples.end());rightPcm.insert(rightPcm.end(),b.samples.begin(),b.samples.end());
        requireCampaignComparison(leftPcm==rightPcm&&a.firstSample==b.firstSample&&!a.exitRequested&&!b.exitRequested,
            "cold save recovery changed crossbudget PCM or exited");
        test::compareCampaignFrame(left->frame(),right->frame());
    };
    const auto pulse=[&](const NativeMenuInput input) {step(input);step();};
    pulse({false,true,false});pulse({false,true,false});pulse({false,false,true}); //PREFS.
    const auto entry=left->startup().recoveredMenuResources()->configuration;
    pulse({true,false,false});pulse({false,false,false,true});pulse({true,false,false}); //EXIT -> STORY -> LEVEL -> MUSIC.
    pulse({false,false,true}); //MUSIC toggle.
    pulse({false,true,false});pulse({false,true,false});pulse({false,false,true}); //LEVEL -> EXIT -> save failure.
    requireCampaignComparison(leftStore->calls==1&&rightStore->calls==1&&left->menuBackAvailable()&&right->menuBackAvailable(),
        "cold failed save did not expose modal undo");
    NativeMenuInput undo;undo.cancel=true;step(undo);step();
    const auto restored=left->startup().recoveredMenuResources()->configuration;
    requireCampaignComparison(restored.options==entry.options&&restored.viewSize==entry.viewSize
        &&!left->menuBackAvailable()&&!right->menuBackAvailable(),"cold Escape changed normalization or failed to close");
    pulse({false,false,true});
    for(unsigned call=0;!left->session()&&call<10000;++call) {step();}
    requireCampaignComparison(left->session()&&right->session(),"cold failed-save undo did not permit Begin");
    test::CampaignBudgetComparison comparison;comparison.leftPcm=std::move(leftPcm);comparison.rightPcm=std::move(rightPcm);
    for(unsigned frame=0;frame<2;++frame) {
        const auto a=left->advanceGameplay({}),b=right->advanceGameplay({});
        comparison.compare(*left->session(),*right->session(),a,b,test::CampaignComparisonStage::boundary);
    }
    requireCampaignComparison(left->session()->synth().driverState().sequenceSelected==(entry.options[musicOption]!=0),
        "cold undo did not restore canonical music publication before Begin");
}
}
int main(int argc,char** argv) {
    if(argc!=3) {return 2;}
    try {
        const auto assets=naturalRoute::assets(argv[1]);dialogFailures(assets);
        const auto output=std::filesystem::path(argv[2])/("save-failure-"
            +std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        files(assets,output);
        const auto art=decodeNativeStartupAssets(naturalRoute::read(std::filesystem::path(argv[1])/"STELART.RES"));
        const auto drivers=decodeTaggedPayloads(naturalRoute::read(std::filesystem::path(argv[1])/"STELLAR.RES"),{'V','G','A',':'});
        cold(assets,art,decodeFrameCopyProfile(drivers.at(0)));
        std::cout<<"Native save retry/undo/visible error/denied preservation/cold Begin PASS\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
