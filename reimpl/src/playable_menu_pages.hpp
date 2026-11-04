#pragma once

#include "stellar7/highscore_render.hpp"
#include "stellar7/menu_model.hpp"

namespace stellar7 {

void paintPlayableMenuPointer(IndexedPages& pages,const IndexedBitmap& pointer,
    ProjectedPoint origin,std::uint16_t back,std::uint16_t front);
void paintPlayableMainMenu(IndexedPages& pages,const std::vector<IndexedBitmap>& art,
    const BitmapFont& font,const MenuModel& menu,std::optional<std::size_t> pressed,
    std::uint16_t back,std::uint16_t front,std::optional<ProjectedPoint> pointer=std::nullopt);
void paintPlayableScores(IndexedPages& pages,const std::vector<IndexedBitmap>& art,
    const BitmapFont& font,const HighscoreTable& table,MenuWidgetState state,
    std::uint16_t back,std::uint16_t front,std::optional<ProjectedPoint> pointer=std::nullopt);

}
