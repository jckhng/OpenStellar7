#include "stellar7/simulation_prefix_work.hpp"

#include <cassert>
#include <limits>
#include <stdexcept>

int main() {
    using namespace stellar7;
    Level1Entity first{};
    first.type = 13;
    first.collisionRadius = 100;
    auto second = first;
    second.position.x = 100;
    assert(actorOverlapOperations(first, second).overlaps);
    second.position.x = 101;
    assert(!actorOverlapOperations(first, second).overlaps);
    second.position = {-32768, 0, 0};
    first.collisionRadius = second.collisionRadius = 32767;
    assert(actorOverlapOperations(first, second).overlaps);
    second.type = 7;
    assert(!actorOverlapOperations(first, second).overlaps);
    first.forwardVelocity = 104;
    assert(flatKinematicsOperations(first).actor.position.y == 104);
    first.position.y = std::numeric_limits<std::int32_t>::max();
    assert(flatKinematicsOperations(first).actor.position.y
        == std::numeric_limits<std::int32_t>::min() + 103);
    first.position = {};
    first.yaw = 0x2000;
    const auto yawWork = flatKinematicsOperations(first);
    std::uint32_t dispatches = 0;
    for (const auto operation : yawWork.operations) {
        dispatches += operation.count;
    }
    assert(dispatches == 112);
    first.pitch = 1;
    auto rejected = false;
    try {
        flatKinematicsOperations(first);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    first.pitch = 0;
    second = first;
    auto third = first;
    const auto contacts = actorContactOperations({first, second, third}, {2, 0, 1}, 0);
    assert(contacts.contactIndex == 1 && contacts.contactWrites.size() == 2);
    third.stateBytes4dTo52[0] = 253;
    second.stateBytes4dTo52[0] = 252;
    assert(actorContactOperations({first, second, third}, {2, 0, 1}, 0).contactIndex
        == std::numeric_limits<std::size_t>::max());
    rejected = false;
    try {
        actorContactOperations({first}, {0, 0}, 0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    StationaryLevel2PrefixInput prefix{};
    prefix.active = true;
    prefix.player.type = 31;
    prefix.player.collisionRadius = 740;
    const std::array<std::int16_t, 7> types{13, 8, 8, 13, 30, 30, 8};
    const std::array<FixedPosition, 7> positions{{
        {-22896, 26992, 0}, {12288, 24576, 0}, {24576, -8192, 0},
        {10608, -22896, 0}, {-8192, 20480, 0}, {8192, -20480, 0},
        {-20480, 4096, 0}}};
    const std::array<std::uint16_t, 7> yaws{33536, 2145, 51297, 34560, 0, 0, 18529};
    for (std::size_t index = 0; index < types.size(); ++index) {
        Level1Entity actor{};
        actor.type = types[index];
        actor.position = positions[index];
        actor.yaw = yaws[index];
        actor.collisionRadius = actor.type == 13 ? 559 : actor.type == 8 ? 860 : 868;
        actor.stateBytes4dTo52[0] = actor.type == 30 ? 254 : 255;
        actor.stateBytes4dTo52[1] = 1;
        actor.stateBytes4dTo52[4] = actor.type == 13 ? 4 : actor.type == 8 ? 2 : 0;
        if (actor.type == 8) {
            actor.yawVelocity = 143;
            actor.weaponSlotCount = 2;
        }
        prefix.actors.push_back(actor);
        prefix.activeOrder.push_back(index);
    }
    prefix.actors[0].yawVelocity = 2048;
    prefix.actors[0].xVelocity = 112;
    prefix.actors[0].yVelocity = -112;
    prefix.actors[3].yawVelocity = -2048;
    prefix.actors[3].xVelocity = -112;
    prefix.actors[3].yVelocity = 112;
    const auto complete = stationaryLevel2PrefixOperations(prefix);
    dispatches = 0;
    for (const auto operation : complete.operations) {
        dispatches += operation.count;
    }
    assert(dispatches == 8010);
    assert(complete.activeOrder == prefix.activeOrder);
    assert((complete.actors[0].position == FixedPosition{-22784, 26880, 0}));
    auto collision = prefix;
    collision.actors[3].position = {8928, -21216, 0};
    const auto lifted = stationaryLevel2PrefixOperations(collision);
    assert((lifted.actors[3].position == FixedPosition{8816, -21104, 45}));
    assert(lifted.actors[3].collisionContactAllocationIndex == std::numeric_limits<std::size_t>::max());
    assert(lifted.actors[5].collisionContactAllocationIndex == 3);
    assert(!lifted.actors[5].collisionContactIsPlayer);
    assert(lifted.actors[3].xVelocity == -112 && lifted.actors[3].yVelocity == 112);
    collision.actors = lifted.actors;
    collision.activeOrder = lifted.activeOrder;
    const auto adjacent = stationaryLevel2PrefixOperations(collision);
    assert(adjacent.actors[3].position.z == 90);
    assert(adjacent.actors[5].collisionContactAllocationIndex == 3);
    const auto rejectsPrefix = [](const StationaryLevel2PrefixInput& input) {
        try {
            stationaryLevel2PrefixOperations(input);
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    auto unsupported = prefix;
    unsupported.refreshRequested = true;
    assert(rejectsPrefix(unsupported));
    unsupported = prefix;
    unsupported.lastListCamera.x = 1;
    assert(rejectsPrefix(unsupported));
    unsupported = prefix;
    unsupported.fadeTicks = 1;
    assert(rejectsPrefix(unsupported));
    unsupported = prefix;
    unsupported.projectileHandlePresent[0] = true;
    assert(rejectsPrefix(unsupported));
    unsupported = prefix;
    unsupported.actors[0].stateBytes4dTo52[4] = 5;
    assert(rejectsPrefix(unsupported));
    unsupported = prefix;
    unsupported.actors[1].position = {};
    assert(rejectsPrefix(unsupported));
    unsupported = prefix;
    unsupported.actors[0].position.x = 1000;
    assert(rejectsPrefix(unsupported));
    unsupported = prefix;
    unsupported.player.forwardVelocity = 1;
    assert(rejectsPrefix(unsupported));
    unsupported = prefix;
    unsupported.actors[0].position.x = -32768 - unsupported.actors[0].xVelocity;
    assert(rejectsPrefix(unsupported));
    unsupported = prefix;
    unsupported.actors[0].position.y = -32768 - unsupported.actors[0].yVelocity;
    assert(rejectsPrefix(unsupported));
    unsupported = prefix;
    unsupported.actors[1].position.x = 65000;
    assert(rejectsPrefix(unsupported));
    unsupported = prefix;
    unsupported.actors[1].position.x = -65000;
    assert(rejectsPrefix(unsupported));
}
