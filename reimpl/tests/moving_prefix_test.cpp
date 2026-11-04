#include "stellar7/simulation_prefix_work.hpp"
#include <array>
#include <cassert>
#include <stdexcept>

namespace {
using namespace stellar7;
StationaryLevel2PrefixInput fixture() {
    StationaryLevel2PrefixInput input{};
    input.active = true;
    input.player.type = 0;
    input.player.collisionRadius = 723;
    const std::array<std::array<std::int64_t, 20>, 7> rows{{{13,559,-22896,26992,0,0,0,33536,0,0,2048,112,-112,0,0,255,4,0,35578,40096},{8,860,12288,24576,0,0,0,2145,0,0,143,0,0,0,0,255,2,2,29184,27936},{8,860,24576,-8192,0,0,0,51297,0,0,143,0,0,0,0,255,2,2,27648,13056},{13,559,10608,-22896,0,0,0,34560,0,0,-2048,-112,112,0,0,255,4,0,26874,4512},{30,868,-8192,20480,0,0,0,0,0,0,0,0,0,0,0,254,0,0,23552,36720},{30,868,8192,-20480,0,0,0,0,0,0,0,0,0,0,0,254,0,0,23552,3952},{8,860,-20480,4096,0,0,0,18529,0,0,143,0,0,0,0,255,2,2,22016,47104}}};
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        Level1Entity v{};
        v.type = static_cast<std::int16_t>(r[0]);
        v.collisionRadius = static_cast<std::uint32_t>(r[1]);
        v.position = {static_cast<std::int32_t>(r[2]), static_cast<std::int32_t>(r[3]), static_cast<std::int32_t>(r[4])};
        v.pitch = static_cast<std::uint16_t>(r[5]);
        v.roll = static_cast<std::uint16_t>(r[6]);
        v.yaw = static_cast<std::uint16_t>(r[7]);
        v.pitchVelocity = static_cast<std::int16_t>(r[8]);
        v.rollVelocity = static_cast<std::int16_t>(r[9]);
        v.yawVelocity = static_cast<std::int16_t>(r[10]);
        v.xVelocity = static_cast<std::int16_t>(r[11]);
        v.yVelocity = static_cast<std::int16_t>(r[12]);
        v.zVelocity = static_cast<std::int16_t>(r[13]);
        v.forwardVelocity = static_cast<std::int16_t>(r[14]);
        v.stateBytes4dTo52[0] = static_cast<std::uint8_t>(r[15]);
        v.stateBytes4dTo52[1] = 1;
        v.stateBytes4dTo52[4] = static_cast<std::uint8_t>(r[16]);
        v.weaponSlotCount = static_cast<std::uint8_t>(r[17]);
        v.presenterDistance = static_cast<std::uint32_t>(r[18]);
        v.targetHeading = static_cast<std::uint16_t>(r[19]);
        input.actors.push_back(v);
        input.activeOrder.push_back(i);
    }
    return input;
}
std::array<std::int64_t, 15> fields(const Level1Entity& v) {
    return {v.position.x,v.position.y,v.position.z,v.pitch,v.roll,v.yaw,
        v.pitchVelocity,v.rollVelocity,v.yawVelocity,v.xVelocity,v.yVelocity,v.zVelocity,
        v.forwardVelocity,v.presenterDistance,v.targetHeading};
}
bool rejects(const StationaryLevel2PrefixInput& input) {
    try { retainedLevel2PrefixOperations(input); }
    catch (const std::invalid_argument&) { return true; }
    return false;
}
}
int main() {
    using namespace stellar7;
    // Twelve device-free original CPU results; second input is always the first returned native state.
    {
        auto input = fixture();
        input.player.forwardVelocity = 10;
        input.lastListCamera = {0,0,0};
        const auto step0 = retainedLevel2PrefixOperations(input);
        std::uint64_t count0 = 0;
        for (const auto operation : step0.operations) {
            assert(operation.kind == NormalWorkKind::instructions);
            count0 += operation.count;
        }
        assert(count0 == 8023);
        assert((step0.player.position == FixedPosition{0,10,0}));
        assert(step0.player.forwardVelocity == 10);
        const std::array<std::array<std::int64_t,15>,7> expected0{{{-22784,26880,0,0,0,35584,0,0,2048,112,-112,0,0,35414,40096},{12288,24576,0,0,0,2288,0,0,143,0,0,0,0,29174,27936},{24576,-8192,0,0,0,51440,0,0,143,0,0,0,0,27651,13056},{10496,-22784,0,0,0,32512,0,0,-2048,-112,112,0,0,26730,4480},{-8192,20480,0,0,0,0,0,0,0,0,0,0,0,23542,36720},{8192,-20480,0,0,0,0,0,0,0,0,0,0,0,23562,3952},{-20480,4096,0,0,0,18672,0,0,143,0,0,0,0,22012,47104}}};
        for (std::size_t i = 0; i < expected0.size(); ++i) {
            assert(fields(step0.actors[i]) == expected0[i]);
        }
        assert((step0.activeOrder == std::vector<std::size_t>{0,1,2,3,5,4,6}));
        input.player = step0.player;
        input.actors = step0.actors;
        input.activeOrder = step0.activeOrder;
        const auto step1 = retainedLevel2PrefixOperations(input);
        std::uint64_t count1 = 0;
        for (const auto operation : step1.operations) {
            assert(operation.kind == NormalWorkKind::instructions);
            count1 += operation.count;
        }
        assert(count1 == 8010);
        assert((step1.player.position == FixedPosition{0,20,0}));
        assert(step1.player.forwardVelocity == 10);
        const std::array<std::array<std::int64_t,15>,7> expected1{{{-22672,26768,0,0,0,37632,0,0,2048,112,-112,0,0,35250,40080},{12288,24576,0,0,0,2431,0,0,143,0,0,0,0,29164,27936},{24576,-8192,0,0,0,51583,0,0,143,0,0,0,0,27655,13024},{10384,-22672,0,0,0,30464,0,0,-2048,-112,112,0,0,26586,4464},{-8192,20480,0,0,0,0,0,0,0,0,0,0,0,23532,36736},{8192,-20480,0,0,0,0,0,0,0,0,0,0,0,23572,3952},{-20480,4096,0,0,0,18815,0,0,143,0,0,0,0,22008,47136}}};
        for (std::size_t i = 0; i < expected1.size(); ++i) {
            assert(fields(step1.actors[i]) == expected1[i]);
        }
        assert((step1.activeOrder == std::vector<std::size_t>{0,1,2,3,5,4,6}));
        input.player = step1.player;
        input.actors = step1.actors;
        input.activeOrder = step1.activeOrder;
    }
    {
        auto input = fixture();
        input.player.forwardVelocity = 200;
        input.lastListCamera = {0,0,0};
        const auto step0 = retainedLevel2PrefixOperations(input);
        std::uint64_t count0 = 0;
        for (const auto operation : step0.operations) {
            assert(operation.kind == NormalWorkKind::instructions);
            count0 += operation.count;
        }
        assert(count0 == 8023);
        assert((step0.player.position == FixedPosition{0,200,0}));
        assert(step0.player.forwardVelocity == 200);
        const std::array<std::array<std::int64_t,15>,7> expected0{{{-22784,26880,0,0,0,35584,0,0,2048,112,-112,0,0,35224,40128},{12288,24576,0,0,0,2288,0,0,143,0,0,0,0,28984,27904},{24576,-8192,0,0,0,51440,0,0,143,0,0,0,0,27723,12976},{10496,-22784,0,0,0,32512,0,0,-2048,-112,112,0,0,26920,4448},{-8192,20480,0,0,0,0,0,0,0,0,0,0,0,23352,36752},{8192,-20480,0,0,0,0,0,0,0,0,0,0,0,23752,3904},{-20480,4096,0,0,0,18672,0,0,143,0,0,0,0,21941,47200}}};
        for (std::size_t i = 0; i < expected0.size(); ++i) {
            assert(fields(step0.actors[i]) == expected0[i]);
        }
        assert((step0.activeOrder == std::vector<std::size_t>{0,1,2,3,5,4,6}));
        input.player = step0.player;
        input.actors = step0.actors;
        input.activeOrder = step0.activeOrder;
        const auto step1 = retainedLevel2PrefixOperations(input);
        std::uint64_t count1 = 0;
        for (const auto operation : step1.operations) {
            assert(operation.kind == NormalWorkKind::instructions);
            count1 += operation.count;
        }
        assert(count1 == 8010);
        assert((step1.player.position == FixedPosition{0,400,0}));
        assert(step1.player.forwardVelocity == 200);
        const std::array<std::array<std::int64_t,15>,7> expected1{{{-22672,26768,0,0,0,37632,0,0,2048,112,-112,0,0,34870,40160},{12288,24576,0,0,0,2431,0,0,143,0,0,0,0,28784,27872},{24576,-8192,0,0,0,51583,0,0,143,0,0,0,0,27798,12880},{10384,-22672,0,0,0,30464,0,0,-2048,-112,112,0,0,26966,4400},{-8192,20480,0,0,0,0,0,0,0,0,0,0,0,23152,36784},{8192,-20480,0,0,0,0,0,0,0,0,0,0,0,23952,3872},{-20480,4096,0,0,0,18815,0,0,143,0,0,0,0,21866,47312}}};
        for (std::size_t i = 0; i < expected1.size(); ++i) {
            assert(fields(step1.actors[i]) == expected1[i]);
        }
        assert((step1.activeOrder == std::vector<std::size_t>{0,1,2,3,5,4,6}));
        input.player = step1.player;
        input.actors = step1.actors;
        input.activeOrder = step1.activeOrder;
    }
    {
        auto input = fixture();
        input.player.forwardVelocity = -10;
        input.lastListCamera = {0,0,0};
        const auto step0 = retainedLevel2PrefixOperations(input);
        std::uint64_t count0 = 0;
        for (const auto operation : step0.operations) {
            assert(operation.kind == NormalWorkKind::instructions);
            count0 += operation.count;
        }
        assert(count0 == 8014);
        assert((step0.player.position == FixedPosition{0,-10,0}));
        assert(step0.player.forwardVelocity == -10);
        const std::array<std::array<std::int64_t,15>,7> expected0{{{-22784,26880,0,0,0,35584,0,0,2048,112,-112,0,0,35434,40080},{12288,24576,0,0,0,2288,0,0,143,0,0,0,0,29194,27952},{24576,-8192,0,0,0,51440,0,0,143,0,0,0,0,27644,13056},{10496,-22784,0,0,0,32512,0,0,-2048,-112,112,0,0,26710,4480},{-8192,20480,0,0,0,0,0,0,0,0,0,0,0,23562,36720},{8192,-20480,0,0,0,0,0,0,0,0,0,0,0,23542,3952},{-20480,4096,0,0,0,18672,0,0,143,0,0,0,0,22019,47104}}};
        for (std::size_t i = 0; i < expected0.size(); ++i) {
            assert(fields(step0.actors[i]) == expected0[i]);
        }
        assert((step0.activeOrder == std::vector<std::size_t>{0,1,2,3,4,5,6}));
        input.player = step0.player;
        input.actors = step0.actors;
        input.activeOrder = step0.activeOrder;
        const auto step1 = retainedLevel2PrefixOperations(input);
        std::uint64_t count1 = 0;
        for (const auto operation : step1.operations) {
            assert(operation.kind == NormalWorkKind::instructions);
            count1 += operation.count;
        }
        assert(count1 == 8014);
        assert((step1.player.position == FixedPosition{0,-20,0}));
        assert(step1.player.forwardVelocity == -10);
        const std::array<std::array<std::int64_t,15>,7> expected1{{{-22672,26768,0,0,0,37632,0,0,2048,112,-112,0,0,35290,40080},{12288,24576,0,0,0,2431,0,0,143,0,0,0,0,29204,27952},{24576,-8192,0,0,0,51583,0,0,143,0,0,0,0,27640,13056},{10384,-22672,0,0,0,30464,0,0,-2048,-112,112,0,0,26546,4464},{-8192,20480,0,0,0,0,0,0,0,0,0,0,0,23572,36720},{8192,-20480,0,0,0,0,0,0,0,0,0,0,0,23532,3968},{-20480,4096,0,0,0,18815,0,0,143,0,0,0,0,22023,47104}}};
        for (std::size_t i = 0; i < expected1.size(); ++i) {
            assert(fields(step1.actors[i]) == expected1[i]);
        }
        assert((step1.activeOrder == std::vector<std::size_t>{0,1,2,3,4,5,6}));
        input.player = step1.player;
        input.actors = step1.actors;
        input.activeOrder = step1.activeOrder;
    }
    {
        auto input = fixture();
        input.player.forwardVelocity = -200;
        input.lastListCamera = {0,0,0};
        const auto step0 = retainedLevel2PrefixOperations(input);
        std::uint64_t count0 = 0;
        for (const auto operation : step0.operations) {
            assert(operation.kind == NormalWorkKind::instructions);
            count0 += operation.count;
        }
        assert(count0 == 8014);
        assert((step0.player.position == FixedPosition{0,-200,0}));
        assert(step0.player.forwardVelocity == -200);
        const std::array<std::array<std::int64_t,15>,7> expected0{{{-22784,26880,0,0,0,35584,0,0,2048,112,-112,0,0,35624,40048},{12288,24576,0,0,0,2288,0,0,143,0,0,0,0,29384,27984},{24576,-8192,0,0,0,51440,0,0,143,0,0,0,0,27573,13120},{10496,-22784,0,0,0,32512,0,0,-2048,-112,112,0,0,26520,4512},{-8192,20480,0,0,0,0,0,0,0,0,0,0,0,23752,36672},{8192,-20480,0,0,0,0,0,0,0,0,0,0,0,23352,3984},{-20480,4096,0,0,0,18672,0,0,143,0,0,0,0,22091,47008}}};
        for (std::size_t i = 0; i < expected0.size(); ++i) {
            assert(fields(step0.actors[i]) == expected0[i]);
        }
        assert((step0.activeOrder == std::vector<std::size_t>{0,1,2,3,4,5,6}));
        input.player = step0.player;
        input.actors = step0.actors;
        input.activeOrder = step0.activeOrder;
        const auto step1 = retainedLevel2PrefixOperations(input);
        std::uint64_t count1 = 0;
        for (const auto operation : step1.operations) {
            assert(operation.kind == NormalWorkKind::instructions);
            count1 += operation.count;
        }
        assert(count1 == 8014);
        assert((step1.player.position == FixedPosition{0,-400,0}));
        assert(step1.player.forwardVelocity == -200);
        const std::array<std::array<std::int64_t,15>,7> expected1{{{-22672,26768,0,0,0,37632,0,0,2048,112,-112,0,0,35670,40016},{12288,24576,0,0,0,2431,0,0,143,0,0,0,0,29584,28016},{24576,-8192,0,0,0,51583,0,0,143,0,0,0,0,27498,13200},{10384,-22672,0,0,0,30464,0,0,-2048,-112,112,0,0,26166,4528},{-8192,20480,0,0,0,0,0,0,0,0,0,0,0,23952,36640},{8192,-20480,0,0,0,0,0,0,0,0,0,0,0,23152,4016},{-20480,4096,0,0,0,18815,0,0,143,0,0,0,0,22166,46912}}};
        for (std::size_t i = 0; i < expected1.size(); ++i) {
            assert(fields(step1.actors[i]) == expected1[i]);
        }
        assert((step1.activeOrder == std::vector<std::size_t>{0,1,2,3,4,5,6}));
        input.player = step1.player;
        input.actors = step1.actors;
        input.activeOrder = step1.activeOrder;
    }
    {
        auto input = fixture();
        input.player.forwardVelocity = 10;
        input.lastListCamera = {8,0,0};
        const auto step0 = retainedLevel2PrefixOperations(input);
        std::uint64_t count0 = 0;
        for (const auto operation : step0.operations) {
            assert(operation.kind == NormalWorkKind::instructions);
            count0 += operation.count;
        }
        assert(count0 == 8028);
        assert((step0.player.position == FixedPosition{0,10,0}));
        assert(step0.player.forwardVelocity == 10);
        const std::array<std::array<std::int64_t,15>,7> expected0{{{-22784,26880,0,0,0,35584,0,0,2048,112,-112,0,0,35414,40096},{12288,24576,0,0,0,2288,0,0,143,0,0,0,0,29174,27936},{24576,-8192,0,0,0,51440,0,0,143,0,0,0,0,27651,13056},{10496,-22784,0,0,0,32512,0,0,-2048,-112,112,0,0,26730,4480},{-8192,20480,0,0,0,0,0,0,0,0,0,0,0,23542,36720},{8192,-20480,0,0,0,0,0,0,0,0,0,0,0,23562,3952},{-20480,4096,0,0,0,18672,0,0,143,0,0,0,0,22012,47104}}};
        for (std::size_t i = 0; i < expected0.size(); ++i) {
            assert(fields(step0.actors[i]) == expected0[i]);
        }
        assert((step0.activeOrder == std::vector<std::size_t>{0,1,2,3,5,4,6}));
        input.player = step0.player;
        input.actors = step0.actors;
        input.activeOrder = step0.activeOrder;
        const auto step1 = retainedLevel2PrefixOperations(input);
        std::uint64_t count1 = 0;
        for (const auto operation : step1.operations) {
            assert(operation.kind == NormalWorkKind::instructions);
            count1 += operation.count;
        }
        assert(count1 == 8015);
        assert((step1.player.position == FixedPosition{0,20,0}));
        assert(step1.player.forwardVelocity == 10);
        const std::array<std::array<std::int64_t,15>,7> expected1{{{-22672,26768,0,0,0,37632,0,0,2048,112,-112,0,0,35250,40080},{12288,24576,0,0,0,2431,0,0,143,0,0,0,0,29164,27936},{24576,-8192,0,0,0,51583,0,0,143,0,0,0,0,27655,13024},{10384,-22672,0,0,0,30464,0,0,-2048,-112,112,0,0,26586,4464},{-8192,20480,0,0,0,0,0,0,0,0,0,0,0,23532,36736},{8192,-20480,0,0,0,0,0,0,0,0,0,0,0,23572,3952},{-20480,4096,0,0,0,18815,0,0,143,0,0,0,0,22008,47136}}};
        for (std::size_t i = 0; i < expected1.size(); ++i) {
            assert(fields(step1.actors[i]) == expected1[i]);
        }
        assert((step1.activeOrder == std::vector<std::size_t>{0,1,2,3,5,4,6}));
        input.player = step1.player;
        input.actors = step1.actors;
        input.activeOrder = step1.activeOrder;
    }
    {
        auto input = fixture();
        input.player.forwardVelocity = 10;
        input.lastListCamera = {-8,0,0};
        const auto step0 = retainedLevel2PrefixOperations(input);
        std::uint64_t count0 = 0;
        for (const auto operation : step0.operations) {
            assert(operation.kind == NormalWorkKind::instructions);
            count0 += operation.count;
        }
        assert(count0 == 8023);
        assert((step0.player.position == FixedPosition{0,10,0}));
        assert(step0.player.forwardVelocity == 10);
        const std::array<std::array<std::int64_t,15>,7> expected0{{{-22784,26880,0,0,0,35584,0,0,2048,112,-112,0,0,35414,40096},{12288,24576,0,0,0,2288,0,0,143,0,0,0,0,29174,27936},{24576,-8192,0,0,0,51440,0,0,143,0,0,0,0,27651,13056},{10496,-22784,0,0,0,32512,0,0,-2048,-112,112,0,0,26730,4480},{-8192,20480,0,0,0,0,0,0,0,0,0,0,0,23542,36720},{8192,-20480,0,0,0,0,0,0,0,0,0,0,0,23562,3952},{-20480,4096,0,0,0,18672,0,0,143,0,0,0,0,22012,47104}}};
        for (std::size_t i = 0; i < expected0.size(); ++i) {
            assert(fields(step0.actors[i]) == expected0[i]);
        }
        assert((step0.activeOrder == std::vector<std::size_t>{0,1,2,3,5,4,6}));
        input.player = step0.player;
        input.actors = step0.actors;
        input.activeOrder = step0.activeOrder;
        const auto step1 = retainedLevel2PrefixOperations(input);
        std::uint64_t count1 = 0;
        for (const auto operation : step1.operations) {
            assert(operation.kind == NormalWorkKind::instructions);
            count1 += operation.count;
        }
        assert(count1 == 8010);
        assert((step1.player.position == FixedPosition{0,20,0}));
        assert(step1.player.forwardVelocity == 10);
        const std::array<std::array<std::int64_t,15>,7> expected1{{{-22672,26768,0,0,0,37632,0,0,2048,112,-112,0,0,35250,40080},{12288,24576,0,0,0,2431,0,0,143,0,0,0,0,29164,27936},{24576,-8192,0,0,0,51583,0,0,143,0,0,0,0,27655,13024},{10384,-22672,0,0,0,30464,0,0,-2048,-112,112,0,0,26586,4464},{-8192,20480,0,0,0,0,0,0,0,0,0,0,0,23532,36736},{8192,-20480,0,0,0,0,0,0,0,0,0,0,0,23572,3952},{-20480,4096,0,0,0,18815,0,0,143,0,0,0,0,22008,47136}}};
        for (std::size_t i = 0; i < expected1.size(); ++i) {
            assert(fields(step1.actors[i]) == expected1[i]);
        }
        assert((step1.activeOrder == std::vector<std::size_t>{0,1,2,3,5,4,6}));
        input.player = step1.player;
        input.actors = step1.actors;
        input.activeOrder = step1.activeOrder;
    }
    auto invalid = fixture();
    invalid.player.forwardVelocity = 201;
    assert(rejects(invalid));
    invalid.player.forwardVelocity = -201;
    assert(rejects(invalid));
    invalid = fixture();
    invalid.player.forwardVelocity = 200;
    invalid.lastListCamera.y = -824; // Updated camera displacement is exactly 1024.
    assert(rejects(invalid));
    invalid.lastListCamera.y = 1224; // Updated displacement is exactly -1024.
    assert(rejects(invalid));
    invalid = fixture();
    invalid.refreshRequested = true;
    assert(rejects(invalid));
    invalid = fixture();
    invalid.player.forwardVelocity = 200;
    invalid.actors[1].position = {0,850,0}; // Only the updated player overlaps this actor.
    assert(rejects(invalid));
    invalid = fixture();
    invalid.player.forwardVelocity = 10;
    invalid.player.position.z = 1;
    assert(rejects(invalid));
    invalid = fixture();
    invalid.player.forwardVelocity = 10;
    invalid.player.yaw = 1;
    assert(rejects(invalid));
    invalid = fixture();
    invalid.activeOrder[1] = invalid.activeOrder[0];
    assert(rejects(invalid));
}
