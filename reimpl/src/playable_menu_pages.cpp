#include "playable_menu_pages.hpp"
#include "stellar7/ui_bitmap_work.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::size_t pointerWidth=16,pointerHeight=21;
constexpr Viewport display{0,319,0,199};
}

void paintPlayableMenuPointer(IndexedPages& pages,const IndexedBitmap& pointer,
    ProjectedPoint origin,std::uint16_t back,std::uint16_t front) {
    if(pointer.width!=pointerWidth||pointer.height+1!=pointerHeight) {
        throw std::invalid_argument("menu pointer requires actual16x20 art");
    }
    if(origin.x<0||origin.y<0||origin.x>=320||origin.y>=200) {
        throw std::invalid_argument("pointer origin outside native display");
    }
    const auto width=static_cast<std::uint16_t>(std::min<int>(pointerWidth,320-origin.x));
    const auto height=static_cast<std::uint16_t>(std::min<int>(pointerHeight,200-origin.y));
    std::array<std::uint8_t,pointerWidth*pointerHeight> saved{};
    for(std::size_t y=0;y<height;++y) {
        for(std::size_t x=0;x<width;++x) {
            saved[y*pointerWidth+x]=pages.read(back,{static_cast<std::int16_t>(origin.x+x),
                static_cast<std::int16_t>(origin.y+y)});
        }
    }
    pages.apply(uiBitmapOperations(pointer,origin,display,back).effects);
    pages.apply(std::vector<IndexedPageEffect>{RadarPageCopy{back,front,origin,width,height}});
    for(std::size_t y=0;y<height;++y) {
        for(std::size_t x=0;x<width;++x) {
            pages.write(back,{static_cast<std::int16_t>(origin.x+x),static_cast<std::int16_t>(origin.y+y)},
                saved[y*pointerWidth+x]);
        }
    }
}

void paintPlayableMainMenu(IndexedPages& pages,const std::vector<IndexedBitmap>& art,
    const BitmapFont& font,const MenuModel& menu,std::optional<std::size_t> pressed,
    std::uint16_t back,std::uint16_t front,const std::optional<ProjectedPoint> pointer) {
    pages.apply(mainMenuPageEffects(art,font,back));
    if(pressed) {pages.apply(mainMenuWidgetEffects(art,font,*pressed,MenuWidgetState::pressed,back));}
    pages.apply(std::vector<IndexedPageEffect>{RadarPageCopy{back,front,{0,0},320,200}});
    paintPlayableMenuPointer(pages,art.at(0),pointer.value_or(ProjectedPoint{static_cast<std::int16_t>(menu.cursorX()),
        static_cast<std::int16_t>(menu.cursorY())}),back,front);
}

void paintPlayableScores(IndexedPages& pages,const std::vector<IndexedBitmap>& art,
    const BitmapFont& font,const HighscoreTable& table,MenuWidgetState state,
    std::uint16_t back,std::uint16_t front,const std::optional<ProjectedPoint> pointer) {
    pages.apply(highscoreHallPageEffects(art,font,table,back,front));
    if(state==MenuWidgetState::pressed) {pages.apply(highscoreHallWidgetEffects(art,font,state,back,front));}
    paintPlayableMenuPointer(pages,art.at(0),pointer.value_or(ProjectedPoint{164,173}),back,front);
}
}
