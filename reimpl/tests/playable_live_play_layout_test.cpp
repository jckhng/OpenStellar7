#include "playable_startup_resources.hpp"
#include "playable_preferences.hpp"
#include "playable_menu_dialog.hpp"
#include "stellar7/ui_text_work.hpp"
#include <iostream>
#include <map>
using namespace stellar7;
class MemoryStore final:public HighscoreStore {
public: unsigned calls=0; HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
    ++calls; return {HighscoreSaveStatus::saved,decodeHighscoreTable(encodeHighscoreTable(table))};
}
};
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {
        const std::filesystem::path root=argv[1];
        auto assets=naturalRoute::assets(root);
        const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
        const auto copy=decodeFrameCopyProfile(decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'}).at(0));
        const auto store=std::make_shared<MemoryStore>();
        RecoveredFrontend frontend(assets,art,copy,257,store,CampaignExecution::playable,
            NativePointerCapability::unavailable,NativeMenuFeatures::fullGraphicDetail);
        unsigned menuPolls=0;
        for(unsigned call=0;!frontend.session()&&call<10000;++call) {
            NativeMenuInput input;
            if(frontend.startup().phase()==NativeStartupPhase::awaitingMenuDecision) {input.confirm=menuPolls++==1;}
            frontend.advanceStartup(input);
        }
        if(!frontend.session()) {throw std::runtime_error("actual Begin failed");}
        const auto* owner=frontend.session();const auto* synth=&owner->synth();
        for(unsigned i=0;i<3;++i) {frontend.advanceGameplay({});}
        GameplayInput open;open.systemAction=GameplaySystemAction::preferences;
        frontend.advanceGameplay(open);frontend.advanceGameplay({});
        if(!frontend.menuBackAvailable()||frontend.session()!=owner||&owner->synth()!=synth) {throw std::runtime_error("F10 lost owner");}
        //Original19 template text is centered at(102+60,159+15), twice drawn.
        //Compare the left two glyphs; the transient pointer starts atX163.
        std::map<std::pair<int,int>,unsigned> sourceText;
        for(const auto& spec:std::array<std::array<int,3>,2>{{{{146,174,128}},{{147,175,156}}}}) {
            for(const auto& effect:uiTextOperations(assets.titleFont,UiFontLayout::fixed,"PLAY",
                {static_cast<std::int16_t>(spec[0]),static_cast<std::int16_t>(spec[1])},spec[2],0xa000,{0,319,0,199}).effects) {
                const auto& pixel=std::get<RadarPixelWrite>(effect);
                if(pixel.point.x<163) {sourceText[{pixel.point.x,pixel.point.y}]=pixel.color;}
            }
        }
        unsigned mismatches=0;
        for(const auto& [point,color]:sourceText) {
            mismatches+=frontend.frame().pixels.at(point.second*320+point.first)!=color;
        }
        const auto focus=preferencePointer(PreferenceControl::exit,PreferenceLayout::inGame);
        GameplayInput confirm;confirm.confirm=true;frontend.advanceGameplay(confirm);
        unsigned pressedMismatch=0,pressedPixels=0;
        const auto& overlay=assets.artGroups.at(0).at(22);
        for(unsigned y=0;y<overlay.height;++y) {
            for(unsigned x=0;x<24;++x) {
                const auto color=overlay.pixels[(overlay.height-1-y)*overlay.width+x];
                if(color) {++pressedPixels;pressedMismatch+=frontend.frame().pixels.at((171+y)*320+122+x)!=color;}
            }
        }
        unsigned sideMismatch=0;bool sideSymmetric=true;
        const auto& side=assets.artGroups.at(0).at(11);
        for(unsigned y=0;y<side.height;++y) {
            for(unsigned x=0;x<side.width;++x) {
                const auto color=side.pixels[(side.height-1-y)*side.width+x];
                sideSymmetric=sideSymmetric&&(color==side.pixels[y*side.width+x]);
                if(color&&116+x<122) {sideMismatch+=frontend.frame().pixels.at((168+y)*320+116+x)!=color;}
            }
        }
        std::cout<<"side_row_symmetric="<<sideSymmetric<<" side_mismatches="<<sideMismatch<<"\n";
        if(!(preferencePointer(PreferenceControl::exit,PreferenceLayout::title)==ProjectedPoint{163,170})) {
            throw std::runtime_error("title EXIT changed");
        }
        auto settings=*assets.cockpitSettings;settings.options[0]=1;auto scores=*assets.highscoreTable;
        MemoryStore pointerStore;
        PlayableMenuDialog pointer(PlayableMenuPage::preferences,assets,NativePointerCapability::portablePointer,
            NativeDetailCapability::fullRange,NativeMenuFeatures::fullGraphicDetail,PreferenceLayout::inGame);
        pointer.advance({},settings,scores,&pointerStore);
        NativeMenuInput click;click.pointer=NativePointerInput{{130,181},true,false};
        pointer.advance(click,settings,scores,&pointerStore);
        click.pointer->primary=false;
        const auto lowerHit=pointer.advance(click,settings,scores,&pointerStore)==MenuDialogResult::preferencesSaved;
        std::cout<<"source_pressed_pixels="<<pressedPixels<<" pressed_mismatches="<<pressedMismatch
            <<" lower_play_hit="<<lowerHit<<" saves="<<pointerStore.calls<<"\n";
        std::cout<<"same_owner=1 source_text_pixels="<<sourceText.size()<<" text_mismatches="<<mismatches
            <<" pointer="<<focus.x<<","<<focus.y<<" expected=163,178\n";
        for(const auto i:{18,19,21,22}) {std::cout<<"art"<<i<<"="<<assets.artGroups.at(0).at(i).width<<"x"<<assets.artGroups.at(0).at(i).height<<"\n";}
        if(mismatches!=0||pressedMismatch!=0||sideMismatch!=0||!lowerHit||pointerStore.calls!=1||!(focus==ProjectedPoint{163,178})) {return 1;}
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 2;}
}
