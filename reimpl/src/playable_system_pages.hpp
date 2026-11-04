#pragma once

#include "playable_jump.hpp"
#include "stellar7/indexed_render.hpp"

namespace stellar7 {
//Source-ordered transient UI intents; canonical systems remain in Level1State.
std::vector<IndexedPageEffect> playableSystemPageEffects(const PlayableSystemEffect& effect,
    const std::vector<IndexedBitmap>& artGroup4, std::uint16_t backPage, std::uint16_t frontPage,
    CockpitPageLayout layout);
//Only normalized grounded reset/preloop; active runtime uses the ordered intents.
} //namespace stellar7
