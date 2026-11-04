#pragma once
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