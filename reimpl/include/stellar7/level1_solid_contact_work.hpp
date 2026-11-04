#pragma once
#include "stellar7/level1.hpp"
#include "stellar7/normal_clock.hpp"
#include <optional>
#include <vector>
namespace stellar7 {
enum class Level1ContactStep { momentumEntry, scanWrite, playerDispatch, playerBackout, playerRebound, playerAudio, actorPlayerOverlap, actorResponse };
struct Level1ContactReceipt {
    Level1ContactStep step;
    std::uint16_t sourceBegin;
    std::uint16_t sourceEnd;
    std::size_t operationBegin;
    std::size_t operationEnd;
    //Absent subject/target denotes the canonical player, never another record.
    std::optional<std::size_t> subject;
    std::optional<std::size_t> target;
};
struct Level1SolidContactWork {
    Level1Entity player;
    std::int16_t impulse;
    std::vector<NormalWorkOperation> operations;
    std::vector<Level1ContactReceipt> receipts;
};
//0224..024B. Excludes024E CALL; consumes and halves the existing0074 owner.
Level1SolidContactWork level1PlayerImpulseOperations(const Level1Entity& player,std::int16_t impulse);
//Selected solid contact after0C0A RET:07A3..090C/0845..0848.
//Targets are source Type1/class1,Type11/class3 or fixedFE Type29/class0.
//The event marker owns0905/0908/0909,090C JMP and0845 argument cleanup.
Level1SolidContactWork level1SolidContactOperations(const Level1Entity& player,
    const Level1Entity& target,std::size_t targetIndex);
}
