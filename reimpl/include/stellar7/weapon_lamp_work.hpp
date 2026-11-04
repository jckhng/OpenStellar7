#pragma once

#include "stellar7/cockpit_state.hpp"
#include "stellar7/indexed_pages.hpp"

namespace stellar7 {

enum class WeaponLampState : std::uint16_t { unavailable, ready };

struct WeaponLampWork {
    std::uint16_t dirty;
    CockpitPageState pages;
    std::vector<NormalWorkOperation> operations;
    std::vector<IndexedPageEffect> effects;
};

//7BAE entry through RET, excluding its caller. Selected art4[39/40], recovered
//A000/A800 physical roles after display; FFFF is the initial dirty word.
WeaponLampWork weaponLampOperations(std::uint16_t dirty, WeaponLampState requested,
    const IndexedBitmap& bitmap, const CockpitPageState& pages);

} // namespace stellar7
