#include "playable_startup_resources.hpp"
#include "stellar7/highscore_render.hpp"

#include <iostream>
#include <algorithm>
#include <fstream>

namespace {
using namespace stellar7;
std::filesystem::path previewDirectory;
void snapshot(const char* name,const IndexedScreen& frame) {
    if(previewDirectory.empty()) {return;}
    const auto path=previewDirectory/(std::string(name)+".ppm");
    if(std::filesystem::exists(path)) {throw std::runtime_error("menu preview output already exists");}
    std::ofstream output(path,std::ios::binary);
    output<<"P6\n320 200\n255\n";
    for(const auto pixel:frame.pixels) {
        const auto color=frame.palette.at(pixel);
        const char rgb[]{static_cast<char>(color.red),static_cast<char>(color.green),static_cast<char>(color.blue)};
        output.write(rgb,3);
    }
}
void require(bool condition,const char* message) {
    if(!condition) {throw std::runtime_error(message);}
}
class MemoryStore final : public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        ++calls;saved=decodeHighscoreTable(encodeHighscoreTable(table));
        return {saved?HighscoreSaveStatus::saved:HighscoreSaveStatus::reloadFailed,saved};
    }
    unsigned calls=0;
    std::optional<HighscoreTable> saved;
};
std::unique_ptr<RecoveredFrontend> menu(const GameplayAssets& data,const NativeStartupAssets& startup,
    FrameCopyProfile copy,CampaignExecution execution,std::shared_ptr<HighscoreStore> store={}) {
    auto result=std::make_unique<RecoveredFrontend>(data,startup,copy,10000,std::move(store),execution);
    for(unsigned calls=0;result->startup().phase()!=NativeStartupPhase::awaitingMenuDecision&&calls<10000;++calls) {
        result->advanceStartup({});
    }
    require(result->startup().phase()==NativeStartupPhase::awaitingMenuDecision,"cold startup missed menu");
    return result;
}
void run(const std::filesystem::path& root) {
    const auto data=naturalRoute::assets(root);
    const auto startup=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto vga=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    require(vga.size()==1,"missing actual VGA");const auto copy=decodeFrameCopyProfile(vga.front());
    auto source=menu(data,startup,copy,CampaignExecution::sourceWork);
    const auto before=source->elapsedSamples();bool rejected=false;
    try {source->advanceStartup({false,true,false});}catch(const std::invalid_argument&) {rejected=true;}
    require(rejected&&source->elapsedSamples()==before,"source menu navigation domain widened");
    const auto store=std::make_shared<MemoryStore>();
    auto playable=menu(data,startup,copy,CampaignExecution::playable,store);
    const auto initial=playable->frame();snapshot("main",initial);
    const auto advance=[&](NativeMenuInput input) {
        const auto origin=playable->elapsedSamples();const auto delivery=playable->advanceStartup(input);
        require(delivery.firstSample==origin&&origin+delivery.samples.size()==playable->elapsedSamples(),
            "menu lost retained PCM epoch");
        require(!playable->session(),"submenu entered gameplay");return delivery;
    };
    for(std::size_t row=1;row<=3;++row) {
        advance({false,true,false});
        require(playable->startup().menu().selection()==row,"Down did not advance selected menu widget");
        advance({});
    }
    advance({false,false,true});advance({});
    snapshot("scores",playable->frame());
    IndexedPages expected;
    expected.apply(highscoreHallPageEffects(data.artGroups.at(0),data.titleFont,*data.highscoreTable,0xa000,0xa800));
    //The live pointer is transient over the same decoded Hall page.
    auto hall=playable->frame().pixels;auto reference=expected.snapshot(0xa800);
    for(int y=0;y<200;++y) {
        for(int x=0;x<320;++x) {
            if(x>=164&&x<180&&y>=173&&y<194) {continue;}
            require(hall[y*320+x]==reference[y*320+x],"Scores did not use the current resident Hall renderer");
        }
    }
    advance({false,false,true});advance({});
    require(playable->startup().menu().selection()==0&&playable->frame().pixels==initial.pixels,
        "Scores return did not reconstruct the BEGIN menu");
    require(playable->startup().countdown()>29000,"Scores return lost source1804 countdown reset");
    const auto press=[&]() {advance({false,false,true});advance({});};
    const auto down=[&]() {advance({false,true,false});advance({});};
    const auto up=[&]() {advance({true,false,false});advance({});};
    down();press(); //BRIEFING, initially CONTINUE.
    const auto intro=playable->frame().pixels;snapshot("briefing-intro",playable->frame());
    for(unsigned page=1;page<19;++page) {
        press();
        require(playable->frame().pixels!=intro,"Briefing did not advance its recovered text/model page");
        advance({});
        if(page==1) {snapshot("briefing-tank",playable->frame());}
        if(page==14) {snapshot("briefing-guardian",playable->frame());}
    }
    press();
    require(playable->frame().pixels==initial.pixels&&playable->startup().menu().selection()==0,
        "Final Briefing Continue did not rebuild BEGIN");
    down();down();press(); //PREFS, initially EXIT.
    snapshot("preferences",playable->frame());
    up();up(); //STORY then SOUNDS.
    press();
    require(playable->startup().recoveredMenuResources()->configuration.options[3]==0,
        "Preferences SOUNDS did not change its current consumer setting");
    require(playable->startup().recoveredSettings()->settings.configuration.options[3]==1,
        "Preferences changed immutable startup evidence");
    down();down();press(); //EXIT saves and reloads.
    require(store->calls==1&&store->saved.has_value(),"Preferences did not save/reload once");
    const auto original=encodeHighscoreTable(*data.highscoreTable),saved=encodeHighscoreTable(*store->saved);
    require(std::equal(original.begin()+highscoreHeaderBytes,original.end(),saved.begin()+highscoreHeaderBytes),
        "Preferences changed score/name records");
    auto loaded=data;
    loaded.highscoreTable=store->saved;
    loaded.cockpitSettings=decodeCockpitSettings(std::vector<std::uint8_t>(saved.begin(),saved.begin()+highscoreHeaderBytes));
    auto cold=menu(loaded,startup,copy,CampaignExecution::playable);
    require(cold->startup().recoveredMenuResources()->configuration.options[3]==0
        &&cold->startup().recoveredSettings()->settings.configuration.options[0]==0,
        "Cold persisted preferences did not retain SOUND0/absent mouse");
    cold->advanceStartup({false,false,true});cold->advanceStartup({false,false,false,true});
    for(unsigned call=0;!cold->session()&&call<10000;++call) {cold->advanceStartup({});}
    require(cold->session()!=nullptr,"Persisted preferences did not enter the real Begin loader");
    cold->advanceGameplay({});
    for(unsigned row=0;row<4;++row) {down();}
    advance({false,false,true});
    require(advance({}).exitRequested,"EXIT did not return a direct host-exit outcome");
    std::cout<<"Playable Scores,19 Briefing pages, preferences save/cold Begin, Exit and strict source rejection PASS\n";
}
}
int main(int argc,char** argv) {
    if(argc!=2&&argc!=3) {return 2;}
    if(argc==3) {previewDirectory=argv[2];}
    try {run(argv[1]);return 0;}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
