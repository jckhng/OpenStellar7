#pragma once

#include "stellar7/level1_object_ref.hpp"
#include "stellar7/normal_clock.hpp"

#include <array>
#include <optional>
#include <vector>

namespace stellar7 {

enum class Level1TargetHitDomain { ordinaryTank, initialOrdinary, guardianNonlethal, level2Constructor, periodicClass1, level2PeriodicOrdinary };

struct Level1TargetBonusHistory {
    std::int16_t lastType = 0;
    std::uint16_t consecutive = 0;
};

struct Level1TargetHitInput {
    Level1Entity target;
    std::size_t targetIndex;
    std::uint8_t damage = 1;
    Level1TargetBonusHistory bonus;
    std::int16_t activeEnemies;
    std::vector<Level1ObjectRef> allocationOrder;
    //Actual FC+4E bytes in47D8 order.4580 tests age, not callback/listed.
    std::array<std::uint8_t, Level1State::deathEffectPoolSize> effectAges{};
    bool refreshRequested = false;
    bool playerTarget = false;
    bool specialTarget = false; //Source1294 identity.
};

//Partial0BC8/4580 writes only. Preserve every unlisted field in the reused slot,
//including flags, radius, projections, cache, angles, velocities and selectors.
struct Level1TargetHitEffect {
    std::size_t poolSlot;
    std::size_t appendedAllocationIndex;
    std::int16_t type = -1;
    std::uint8_t category = 252;
    std::uint8_t age;
    FixedPosition position;
    std::uint8_t endAge;
    std::uint16_t callback = 0x0ac8;
};

struct Level1TargetHitRemoval {
    std::size_t removedIndex;
    std::size_t lastIndex;
    Level1ObjectRef replacement;
};

enum class Level1TargetHitSound : std::uint16_t { impact = 5, destroyed = 6 };
struct Level1TargetHitAudio {
    Level1TargetHitSound sound;
    std::uint16_t callerBegin;
    std::uint16_t callerEnd; //Exclusive return boundary.
    std::uint16_t callerDispatches = 4;
};
struct Level1TargetHitStep {
    std::vector<NormalWorkOperation> operations;
    std::optional<Level1TargetHitAudio> audio;
};

struct Level1TargetHitWork {
    std::uint8_t targetHealth;
    Level1TargetBonusHistory bonus;
    std::int16_t activeEnemies;
    std::optional<Level1TargetHitEffect> effect;
    std::optional<Level1TargetHitRemoval> removal;
    std::size_t allocationCount;
    bool refreshRequested;
    bool destroyed; //Source AX return:1 lethal,0 surviving.
    std::vector<Level1TargetHitStep> steps;
};

//1176 entry through RET:damage1, Type1/categoryFF/callback1 or Type29/FE/0,
//positive signed-byte health, no90A4 bonus emission. Includes90A4,4580/0BC8,
//positive32/16 E0D0 and481A. Audio children own their four caller dispatches
//and completeB3E6; those intervals are excluded from operations, never zero-cost.
//Caller supplies its timed audio executor before this can become executable.
//Exclude primary caller0552..058A, score7970 and all main/radar effect work.
//Apply observations once to the existing owner:health, bonus(if destroyed),
//FC append/partial writes, then target swap-last removal/enemy count. Old45D2
//refs remain unchanged. Initial FC age is firstFrame+1; main0AC8 advances later.
//Explicit guardianNonlethal accepts Type44/FF/class5, damage1/5 and strictly
//surviving health only. Lethal event12/score/sort/45C6 rejects atomically.
//Explicit initialOrdinary additionally admits Type11/FF/class3. Third same-type
//bonus emission remains rejected; returned terminal storage keeps its category.
//level2Constructor admits only actual Type8/FF/class2, Type13/FF/class4 and
//Type30/FE/class0 records, with at most five live constructor enemies.
//Type8 third same-type kill follows its actual zero bonus-table byte, retaining
//streak3. Type13 third-kill bonus creation remains outside this domain.
//level2PeriodicOrdinary additionally admits actual1/4 FF/class1 and source count<=12.
//Type1/4/13 third same-type bonus creation remains rejected; physical/logical
//mapping belongs to the caller and is not inferred from an initial slot index.
Level1TargetHitWork level1TargetHitOperations(const Level1TargetHitInput& input,
    Level1TargetHitDomain domain = Level1TargetHitDomain::ordinaryTank);

} // namespace stellar7
