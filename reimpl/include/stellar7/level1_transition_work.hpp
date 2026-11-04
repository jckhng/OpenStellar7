#pragma once

#include "stellar7/retry_work.hpp"
#include "stellar7/campaign_loader.hpp"
#include "stellar7/level1.hpp"
#include "stellar7/resource_codec.hpp"
#include <variant>

namespace stellar7 {
struct Level1TransitionProfile {
    std::uint16_t levelIndex = 0;
    std::uint32_t score = 0;
    std::int16_t armor = 0;
    std::uint16_t introduction = 0;
    std::int16_t selectedSystem = -1;
    std::array<std::int8_t,7> systemLevels{};
    std::array<std::int16_t,7> systemTimers{};
    std::uint16_t frontPage = 0;
    std::uint16_t backPage = 0;
    std::uint16_t cachePage = 0;
    bool successReturned = false;
    bool missionActive = false;
    bool deathRequested = false;
    bool exitRequested = false;
};
struct Level1TransitionObservation {
    std::uint16_t levelIndex;
    std::uint32_t score;
    std::int16_t armor;
    std::array<std::int16_t,7> systemTimers;
    std::uint16_t loopTick = 0;
    std::uint16_t fadeTicks = 0;
    bool firstEntryDisplay = true;
    bool oldPresenterListEmpty = true;
    bool listRefreshRequested = true;
    bool missionActive = true;
};
struct Level1TransitionWork {
    std::vector<RetryWorkSegment> segments;
    Level1TransitionObservation observation;
    std::size_t poolResetSegment;
    std::size_t constructorsSegment;
    std::size_t loaderPagesSegment;
    std::size_t groundSegment;
};
//03EB AFTER inactive0312..03E6 through BEFORE newLevel2 01D0. No0784/0A9A.
//Uses existing retry child/provider contracts; Level2 audio/pages bindings are
//required explicit destination-aware children, never unchecked Level1 reuse.
//No model/RNG/pixel/audio/clock mutation; root executes/commits each stage once.
Level1TransitionWork level1SuccessTransitionOperations(const Level1TransitionProfile& profile);

enum class Level2LoaderPool { ordinary, fixed };
struct Level2LoaderActorReceipt {
    Level2LoaderPool pool;
    std::size_t slot;
    Level1Entity actor;
};
struct Level2LoaderActors {
    StellarRng rng;
    std::vector<Level2LoaderActorReceipt> actors;
    std::int16_t activeEnemies;
    std::int16_t spawnedEnemies;
    std::vector<NormalWorkOperation> operations;
};
enum class Level2ResetPool { ordinary, fixed, effect };
struct Level2SelectorReleaseReceipt {
    Level2ResetPool pool;
    std::size_t slot;
};
using Level2PoolResetStep = std::variant<std::vector<NormalWorkOperation>,Level2SelectorReleaseReceipt>;
struct Level2PoolResetInput {
    std::array<Level1Entity,16> ordinary;
    std::array<Level1Entity,16> fixed;
    std::array<Level1Entity,8> projectiles;
    std::array<Level1Entity,8> effects;
    Level1Entity player;
    std::int16_t armor;
    std::array<std::int16_t,7> systemTimers;
};
struct Level2PoolResetWork {
    Level2PoolResetInput state;
    std::vector<Level2SelectorReleaseReceipt> selectorReleases;
    std::vector<Level2PoolResetStep> steps;
};
//Whole48D4 projection/work into the existing physical pools. First8 fixed reset;
//upper8 remain untouched. Heap releases are provider receipts, not skipped time.
//Source48D4 handles/list/count/transient clears are stage commits described by
//the transition observation; no allocation, RNG, clock or radar reset here.
Level2PoolResetWork level2PoolReset(const Level2PoolResetInput& input,const Model3d& playerModel);
//Whole4F72..508F using immutable source table and existing ordinary
//constructor/loader abstraction. Reset48D4 precedes this call. Fixed first-free
//slots0/1 and ordinary0..4; no heap selector or transient record construction.
//Model resources must be the owned decoded destination bank. Includes source
//D03C/C05C/49F4/4862/417A/B39E and all allocator scans; no heap provider child.
Level2LoaderActors level2LoaderActors(const StellarRng& currentRng,
    const std::vector<Model3d>& shared,const std::vector<Model3d>& local);
struct Level2GroundInitializationWork {
    StellarRng rng;
    Level1GroundState ground;
    std::vector<NormalWorkOperation> operations;
};
//Whole4C16 with the existing nonnull0730 ground allocation. Includes six4DC8
//resets/twelve continuingB39E calls; excludes caller01BD. Caller must retain
//allocation provenance from the existing Level1 owner. No second ground reset.
Level2GroundInitializationWork level2RetainedGroundInitializationOperations(const StellarRng& currentRng);
} //namespace stellar7
