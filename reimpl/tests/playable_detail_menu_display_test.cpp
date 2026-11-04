#include "playable_startup_resources.hpp"
#include "playable_menu_dialog.hpp"
#include "playable_detail.hpp"
#include "briefing_preview.hpp"

#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace stellar7;
void require(const bool condition,const char* message) {
    if(!condition) {throw std::runtime_error(message);}
}
void preferenceInsets(const GameplayAssets& assets,const std::filesystem::path& preview) {
    constexpr std::uint16_t page=0xa000;
    constexpr std::uint8_t background=0,lightEdge=147,twoLineShadow=135,textColor=239;
    const auto& widgets=preferenceWidgets();
    for(const std::int16_t difficulty:{0,1,2}) {
        for(const std::int16_t story:{0,255}) {
            CockpitSettings settings{81,{0,0,1,1,difficulty,story}};
            for(const auto pressed:{std::optional<PreferenceControl>{},std::optional<PreferenceControl>{PreferenceControl::difficulty},
                std::optional<PreferenceControl>{PreferenceControl::story}}) {
                IndexedPages pages;
                pages.apply(preferencesPageEffects(assets.artGroups.at(0),assets.titleFont,assets.scoreFont,
                    settings,PreferenceControl::exit,pressed,{},page));
                if(!preview.empty() && !pressed) {
                    const auto path=preview/("preferences-"+std::to_string(difficulty)+"-"+std::to_string(story)+".ppm");
                    require(!std::filesystem::exists(path),"preferences preview already exists");
                    std::ofstream output(path,std::ios::binary);output<<"P6\n320 200\n255\n";
                    for(const auto pixel:pages.snapshot(page)) {
                        const auto color=assets.uiPalette.at(pixel);
                        const char rgb[]{static_cast<char>(color.red),static_cast<char>(color.green),static_cast<char>(color.blue)};
                        output.write(rgb,3);
                    }
                    require(bool(output),"failed to write preferences preview");
                }
                for(const auto control:{PreferenceControl::mouse,PreferenceControl::joystick,PreferenceControl::music,
                    PreferenceControl::sounds,PreferenceControl::difficulty,PreferenceControl::story}) {
                    const auto origin=widgets.at(static_cast<std::size_t>(control)).origin;
                    const bool twoLines=control==PreferenceControl::difficulty || control==PreferenceControl::story;
                    // Original1C8A..1CF9: a14-row interior below the top edge.
                    // Original1B54..1BC3: the other controls keep their7-row inset.
                    const int top=origin.y+(twoLines?5:8),bottom=origin.y+(twoLines?18:14);
                    const auto pixel=[&](const int x,const int y) {
                        return pages.read(page,{static_cast<std::int16_t>(x),static_cast<std::int16_t>(y)});
                    };
                    for(int x=origin.x+32;x<origin.x+102;++x) {
                        require(pixel(x,top)==background,"preferences background does not enclose both text rows");
                        require(pixel(x,top-1)==lightEdge,"preferences inset top edge differs from original");
                        for(int y=top+1;y<=bottom;++y) {
                            require(pixel(x,y)==background || pixel(x,y)==textColor,
                                "preferences text extends outside its original recessed background");
                        }
                        if(twoLines) {require(pixel(x,origin.y+19)==twoLineShadow,"two-line preferences shadow differs from original");}
                    }
                }
            }
        }
    }
}
std::vector<std::uint8_t> pixels(const std::vector<IndexedPageEffect>& effects) {
    std::vector<std::uint8_t> result(320*200);
    for(const auto& effect:effects) {
        const auto* write=std::get_if<RadarPixelWrite>(&effect);
        require(write!=nullptr,"briefing emitted an unexpected nonpixel effect");
        result.at(write->point.y*320+write->point.x)=write->color;
    }
    return result;
}
void navigation(const GameplayAssets& assets) {
    for(const std::int16_t detail:{0,40,41,81,82}) {
        CockpitSettings configuration{detail,{0,0,1,1,0,255}};
        HighscoreTable scores({},{});scores.setConfiguration(configuration);
        PlayableMenuDialog dialog(PlayableMenuPage::preferences,assets,NativePointerCapability::unavailable,
            NativeDetailCapability::fullRange,NativeMenuFeatures::fullGraphicDetail);
        NativeMenuInput down;down.down=true;
        dialog.advance(down,configuration,scores,nullptr);dialog.advance({},configuration,scores,nullptr);
        dialog.advance(down,configuration,scores,nullptr);dialog.advance({},configuration,scores,nullptr);
        const auto expected=preferencePointer(detail>40?PreferenceControl::joystick:PreferenceControl::mouse);
        require(dialog.pointerPosition()==expected,"full title detail DOWN ignored source40/41 boundary");
    }
    CockpitSettings configuration{82,{0,0,1,1,0,255}};
    HighscoreTable scores({},{});scores.setConfiguration(configuration);
    PlayableMenuDialog legacy(PlayableMenuPage::preferences,assets,NativePointerCapability::unavailable,
        NativeDetailCapability::bounded51To81);
    NativeMenuInput down;down.down=true;
    legacy.advance(down,configuration,scores,nullptr);legacy.advance({},configuration,scores,nullptr);
    legacy.advance(down,configuration,scores,nullptr);
    require(legacy.pointerPosition()==preferencePointer(PreferenceControl::mouse),"legacy navigation contract changed");
    PlayableMenuDialog game(PlayableMenuPage::preferences,assets,NativePointerCapability::unavailable,
        NativeDetailCapability::fullRange,NativeMenuFeatures::fullGraphicDetail,PreferenceLayout::inGame);
    game.advance(down,configuration,scores,nullptr);game.advance({},configuration,scores,nullptr);
    game.advance(down,configuration,scores,nullptr);
    require(game.pointerPosition()==preferencePointer(PreferenceControl::mouse,PreferenceLayout::inGame),
        "in-game detail inherited the title-only DOWN-link change");
    PlayableMenuDialog briefing(PlayableMenuPage::briefing,assets);
    NativeMenuInput left;left.left=true;
    briefing.advance(left,configuration,scores,nullptr);
    require(briefing.pointerPosition()==ProjectedPoint{67,172},"Briefing LEFT was replaced by layout comma expression");
}
void briefingDescriptor(const GameplayAssets& assets) {
    bool sharedChanged=false;
    for(std::size_t page=1;page<=15;++page) {
        BriefingViewer cold(assets),explicitFull(assets,PlayableModelDetail{}),retained(assets,nativeModelDetail(0));
        cold.select(page,assets);explicitFull.select(page,assets);retained.select(page,assets);
        cold.control(BriefingViewerControl::stop);explicitFull.control(BriefingViewerControl::stop);
        retained.control(BriefingViewerControl::stop);
        const auto initial=pixels(cold.poll(assets,0xa800));
        require(initial==pixels(explicitFull.poll(assets,0xa800)),"default cold Briefing256 profile changed");
        const auto low=pixels(retained.poll(assets,0xa800));
        if(page==14) {
            require(initial==low,"retained shared descriptor changed local Guardian bank1");
        } else {sharedChanged=sharedChanged||initial!=low;}
    }
    require(sharedChanged,"actual shared Briefing meshes ignored retained descriptor0 scale/cap");
}
}
int main(int argc,char** argv) {
    if(argc!=2 && argc!=3) {return 2;}
    try {
        const auto assets=naturalRoute::assets(argv[1]);
        preferenceInsets(assets,argc==3?std::filesystem::path(argv[2]):std::filesystem::path{});
        navigation(assets);briefingDescriptor(assets);
        std::cout<<"Full title40/41 navigation, Briefing arrows and retained descriptor0/local-bank separation PASS\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
