#pragma once
#include "stellar7/level1_loader_work.hpp"

namespace stellar7 {
struct Level2LoaderPageInput {
    CockpitInitializationRecord cockpit;
    Level1Entity player;
    std::int16_t armor;
};
//Destination50C4(0), two cockpit calls, source ANTARES title, reset empty-list
//698A and full back->front copy. Resource cache decode remains the same explicit
//native provider as the Level1 loader. Existing radar/cache/page owners supplied
//by caller; this plan does not initialize actor allocation caches again.
Level1LoaderPages planLevel2LoaderPages(const GameplayAssets& assets,const Level2LoaderPageInput& input,
    const CockpitPageState& pages,const IndexedPages& pixels,const RadarState& radar);
}
