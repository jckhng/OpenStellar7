#pragma once

#include "stellar7/bonus_prefix_work.hpp"
#include "stellar7/type42_pickup_work.hpp"

namespace stellar7 {

struct Level1BonusPickupContext {
    std::array<std::int8_t,Level1State::systemCount> systems;
    std::array<std::int16_t,Level1State::systemCount> timers;
    std::int16_t selectedSystem;
    const std::vector<IndexedBitmap>& artGroup4;
};

struct Level1PrefixDrawingStep {
    //Publish after this many flat prefix operations; child CPU work is already
    //flattened, while7970 remains an existing separate ordered score step.
    std::size_t operationOffset;
    std::vector<IndexedPageEffect> effects;
};

struct Level1PrefixPickupObservation {
    std::size_t entityIndex;
    std::uint8_t payload;
    std::size_t operationBegin;
    std::size_t operationEnd;
    Level1TargetHitRemoval removal;
};

struct Level1BonusPickupPresentation {
    CockpitPageState pages;
    std::uint8_t fillStyle;
    std::uint8_t fillColor;
    std::uint16_t selectedFont;
};

struct Level1BonusPickupPrefixWork {
    Level1BonusTargetPrefixWork bonus;
    std::array<std::int8_t,Level1State::systemCount> systems;
    std::array<std::int16_t,Level1State::systemCount> timers;
    std::int16_t selectedSystem;
    std::vector<Level1PrefixPickupObservation> pickups;
    std::vector<Level1PrefixDrawingStep> drawingSteps;
    std::optional<Level1BonusPickupPresentation> presentation;
};

//Explicit initial systems3/timers0/selection-1 pickup domain, payload3/7. Whole
//082C..0848 child is composed at the selected player contact; terminal FE42
//still runs old45D2 spin before3768. Returned records/systems/work already own
//all mutation. Commit once; publish drawing/score children once at their offsets.
Level1BonusPickupPrefixWork level1BonusPickupPrefixOperations(const Level1TargetPrefixInput& input,
    const Level1TargetPrefixProfile& profile, const BitmapFont& scoreFont,
    const CockpitPageState& pages, std::uint16_t firstBlockCapacity, const Model3d& model42,
    const Level1BonusPickupContext& context);

} // namespace stellar7
