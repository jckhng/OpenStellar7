#pragma once

#include "stellar7/score_work.hpp"
#include "stellar7/target_hit_work.hpp"

#include <variant>

namespace stellar7 {

struct Level1Type42PickupInput {
    Level1Entity pickup;
    std::uint16_t drawCallback = 0;
    std::uint32_t score = 0;
    std::array<std::int8_t, Level1State::systemCount> systems{};
    std::array<std::int16_t, Level1State::systemCount> timers{};
    std::int16_t selectedSystem = -1;
    std::uint16_t difficulty = 0;
    std::vector<Level1ObjectRef> allocationOrder;
};
struct Level1Type42PickupDrawing {
    std::vector<NormalWorkOperation> operations;
    std::vector<IndexedPageEffect> effects;
};
struct Level1Type42PickupAudio {
    std::uint16_t event = 24;
    std::uint16_t callerBegin = 0x9103;
    std::uint16_t callerEnd = 0x910d;
    std::uint16_t callerDispatches = 4;
};
using Level1Type42PickupStep = std::variant<Level1Type42PickupDrawing, ScoreWork, Level1Type42PickupAudio>;
struct Level1Type42PickupWork {
    Level1Entity pickup;
    std::uint32_t score;
    std::array<std::int8_t, Level1State::systemCount> systems;
    Level1TargetHitRemoval removal;
    std::vector<Level1ObjectRef> allocationOrder;
    bool refreshRequested = true;
    std::uint16_t drawPage;
    std::uint16_t copyPage;
    std::uint8_t fillStyle = 1;
    std::uint8_t fillColor = 137;
    std::uint16_t selectedFont = 0;
    std::vector<Level1Type42PickupStep> steps;
};

//082C..0848 inclusive: selected player-contact Type42, then full90F2/8ED2/8F40/
//481A. Stop before094F's following old-list traversal. Collision selection and
//all other prefix/updater/main/radar work remain caller owned.
//Explicit initial system profile: payload3/7, FE,health1,class0,callback0;
//all7 statuses3/timers0/selectedSystem-1, difficulty0. Other statuses reject.
//Step order is authoritative: source caller, complete7970 score100, source
//cleanup, event24 marker, repair/draw/copy/retirement. ScoreWork retains both
//untimed font-provider boundaries. Root gameplayBonusPickupEvent owns allfour
//9103/9106/9107/910A dispatches AND completeB3E6/driver; none occur in Drawing.
//Apply observations once to existing state owners; no semantic tick/replay.
//Timers/selection/RNG and old45D2 do not change. Final DRAW=BACK,COPY=FRONT,
//style1,color137,font0. All cache/page roles otherwise remain caller owned.
Level1Type42PickupWork level1Type42PickupOperations(const Level1Type42PickupInput& input,
    const BitmapFont& scoreFont, const std::vector<IndexedBitmap>& artGroup4,
    const CockpitPageState& pages);

} // namespace stellar7
