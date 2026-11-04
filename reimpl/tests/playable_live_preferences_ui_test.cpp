#include "playable_menu_dialog.hpp"
#include "playable_startup_resources.hpp"

#include <iostream>

namespace {
using namespace stellar7;
void require(const bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
class MemoryStore final:public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        ++calls;
        if(fail) {return {HighscoreSaveStatus::writeFailed,std::nullopt};}
        bytes=encodeHighscoreTable(table);return {HighscoreSaveStatus::saved,decodeHighscoreTable(bytes)};
    }
    bool fail=false;unsigned calls=0;std::vector<std::uint8_t> bytes;
};
void run(const std::filesystem::path& root) {
    const auto assets=naturalRoute::assets(root);
    auto settings=*assets.cockpitSettings;auto table=*assets.highscoreTable;
    settings.viewSize=81;settings.options[0]=0;
    const auto original=encodeHighscoreTable(table);
    PlayableMenuDialog dialog(PlayableMenuPage::preferences,assets,NativePointerCapability::unavailable,
        NativeDetailCapability::bounded51To81,NativeMenuFeatures::fullGraphicDetail,PreferenceLayout::inGame);
    MemoryStore store;
    const auto poll=[&](NativeMenuInput input) {return dialog.advance(input,settings,table,&store);};
    const auto position=[&](const int x,const int y) {
        require(dialog.pointerPosition().x==x&&dialog.pointerPosition().y==y,"wrong source7C focus position");
    };
    poll({});position(163,178);
    NativeMenuInput down;down.down=true;
    for(const auto target:std::array<ProjectedPoint,6>{{{200,18},{117,55},{117,84},{117,113},{117,142},{163,178}}}) {
        poll(down);poll({});position(target.x,target.y);
    }
    require(settings.options[4]==assets.cockpitSettings->options[4]&&settings.options[5]==assets.cockpitSettings->options[5],
        "live6-widget route exposed difficulty or story");
    NativeMenuInput escape;escape.cancel=true;
    require(poll(escape)==MenuDialogResult::preferencesSaved&&store.calls==1,
        "live Escape did not save/reload through the same PLAY return");
    require(std::equal(original.begin()+highscoreHeaderBytes,original.end(),store.bytes.begin()+highscoreHeaderBytes),
        "live preferences changed resident score/name rows");

    PlayableMenuDialog pointer(PlayableMenuPage::preferences,assets,NativePointerCapability::portablePointer,
        NativeDetailCapability::bounded51To81,NativeMenuFeatures::fullGraphicDetail,PreferenceLayout::inGame);
    settings.options[0]=1;
    NativeMenuInput slider;slider.pointer=NativePointerInput{{189,18},true,false};
    pointer.advance(slider,settings,table,&store);
    require(settings.viewSize==70&&pointer.pointerPosition().y==19,"live detail did not use source moved track/captureY19");
    pointer.advance({},settings,table,&store);
    store.fail=true;
    require(pointer.advance(escape,settings,table,&store)==MenuDialogResult::pending&&pointer.backAvailable(),
        "failed live Escape save escaped its retained dialog");
    require(pointer.advance(escape,settings,table,&store)==MenuDialogResult::closed&&settings.viewSize==81,
        "failed live save undo did not restore effective entry detail");
    std::cout<<"Source6widget navigation/coordinates/slider/Escape save/score rows/failure undo PASS\n";
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(argv[1]);return 0;}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
