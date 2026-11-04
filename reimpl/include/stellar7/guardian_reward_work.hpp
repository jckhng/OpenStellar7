#pragma once

#include "stellar7/type42_bonus_work.hpp"
#include "stellar7/simulation_prefix_work.hpp"
#include <variant>

namespace stellar7 {
//Call-time projections of canonical physical records, including retired actors.
//FD/FC participate in14B0 but are filtered inside0C0A, before collision.
struct Level1RewardObject {
    Level1ObjectRef identity;
    Level1Entity actor;
};
struct Level1RewardContactWrite {
    Level1ObjectRef candidate;
    std::size_t rewardEntityIndex;
};
struct Level1GuardianRewardInput {
    Level1Entity source;
    std::optional<std::size_t> existingReward;
    std::array<Level1BonusFixedSlot,level1BonusFixedPoolSize> fixedSlots{};
    std::size_t nextEntityIndex;
    std::vector<Level1ObjectRef> allocationOrder;
    std::vector<Level1RewardObject> inherited;
    StellarRng rng;
    std::uint16_t firstBlockCapacity = 44;
};
struct Level1GuardianRewardWork {
    StellarRng rng;
    std::optional<Level1Type42BonusReceipt> birth;
    std::vector<Level1ObjectRef> allocationOrder;
    std::vector<Level1RewardContactWrite> contactWrites;
    std::vector<NormalWorkOperation> operations;
};
//Whole45C6(victim,0)..RET including4506, continuingB39E and all displacement.
//No initial3 selector repair: caller's0784 epilogue owns0A9A.
//No fallback allocation, unknown fixed bytes or nonzero draw callback admitted.
Level1GuardianRewardWork level1GuardianRewardOperations(const Level1GuardianRewardInput& input,
    const Model3d& model25);

struct Level1GuardianDeathAudio {
    std::uint16_t event = 12;
    std::uint16_t callerBegin = 0x11f1;
    std::uint16_t callerEnd = 0x11fb;
    std::uint16_t callerDispatches = 4;
};
using Level1GuardianDeathStep = std::variant<std::vector<NormalWorkOperation>,
    Level1GuardianDeathAudio,ScoreWork>;
struct Level1GuardianDeathInput {
    Level1TargetHitInput hit;
    Level1GuardianRewardInput reward;
    FixedPosition camera;
    std::uint32_t score;
    std::uint16_t difficulty = 0;
};
struct Level1GuardianDeathWork {
    Level1TargetHitWork hit;
    Level1GuardianRewardWork reward;
    std::vector<Level1RewardObject> inherited;
    std::uint32_t score;
    std::vector<Level1GuardianDeathStep> steps;
};
//Whole lethal1176 forType44/FF/class5,damage1/5,Level1(index0).
//Ordered90A4/FC/481A/count--/event12/7970(1000)/14B0/45C6/AX1.
//1292 is retained. Returned old-list sort is authoritative, not a3768 rebuild.
Level1GuardianDeathWork level1GuardianDeathOperations(const Level1GuardianDeathInput& input,
    const Model3d& model25,const BitmapFont& scoreFont,const CockpitPageState& pages);

struct Level1RewardOscillatorWork {
    std::optional<Level1Entity> actor;
    std::vector<NormalWorkOperation> operations;
};
//Whole0A9A..RET, independently of membership/health; no RNG or renderer work.
Level1RewardOscillatorWork level1RewardOscillatorOperations(const std::optional<Level1Entity>& actor);
//Whole1176 Type25 impact path; signed lethal test resetshealth100 and stays live.
//No bonus/removal/count/score/reward allocation. Caller still retires its shot.
Level1TargetHitWork level1RewardHitOperations(const Level1TargetHitInput& input);

struct Level1RewardSuccessChild {
    std::uint16_t sourceEntry = 0x6f52;
    std::uint16_t argument = 0;
    std::uint16_t callerBegin = 0x0802;
    std::uint16_t callerEnd = 0x0809;
    std::uint16_t callerDispatches = 3;
};
struct Level1RewardContactWork {
    std::uint16_t campaignIndex;
    bool active = false;
    std::vector<NormalWorkOperation> beforeSuccess;
    Level1RewardSuccessChild success;
    std::vector<NormalWorkOperation> afterSuccess;
};
//Selected player-contact Type25:07ED..0817 through jump094F. No retirement,
//no128E clear, no extra oscillator. Whole blocking6F52(0) is a required child;
//afterSuccess cannot execute until that child's qualified return.
Level1RewardContactWork level1RewardContactOperations(const Level1Entity& selected,
    std::uint16_t campaignIndex);
} //namespace stellar7
