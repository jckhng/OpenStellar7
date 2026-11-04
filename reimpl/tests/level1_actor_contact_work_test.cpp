#include "stellar7/simulation_prefix_work.hpp"

#include <cassert>
#include <limits>
#include <memory>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;
constexpr auto noContact = std::numeric_limits<std::size_t>::max();

template<class Function>
void rejects(Function function) {
    bool rejected = false;
    try { function(); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

Level1ProjectilePrefixInput input() {
    auto state = std::make_unique<Level1State>();
    state->reloadAfterFailure(StellarRng{});
    Level1ProjectilePrefixInput result{state->player(),state->entities(),state->projectiles(),{}, {},{},
        false,state->rng(),0,false,0,true,{false,false}};
    for (std::size_t index = 0; index < Level1State::entityCount; ++index) {
        const Level1ObjectRef ref{Level1PresentationKind::entity,index};
        result.allocationOrder.push_back(ref);
        result.activeOrder.push_back(ref);
    }
    auto& tank = result.entities[0];
    tank.position = {-1841,1961,0};
    tank.yaw = 40608;
    tank.yawVelocity = 0;
    tank.forwardVelocity = 0;
    tank.stateBytes4dTo52[5] = 3;
    tank.behaviorTimer = 1;
    tank.weaponCooldown = 255;
    auto& lift = result.entities[1];
    lift.position = {-1526,3090,1000};
    lift.roll = 61779;
    lift.yaw = 26562;
    lift.yawVelocity = 271;
    lift.stateBytes4dTo52[5] = 2;
    return result;
}

Level1ProjectilePrefixInput carry(const Level1ProjectilePrefixWork& work) {
    return {work.player,work.entities,work.projectiles,work.allocationOrder,work.activeOrder,
        work.lastListCamera,work.refreshRequested,work.rng,0,false,0,true,{false,false}};
}

void instructions(const Level1ProjectilePrefixWork& work,const std::uint64_t expected) {
    std::uint64_t count = 0;
    for (const auto operation : work.operations) {
        assert(operation.kind == NormalWorkKind::instructions);
        count += operation.count;
    }
    assert(count == expected);
}
} // namespace

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    Level1EnemyPrefixProfile profile{1,256,{0,441},{0,134}};
    auto value = input();
    rejects([&]() { level1EnemyPrefixOperations(value,profile); });
    profile.contactDomain = Level1ActorContactDomain::ordinaryActors;
    const auto first = level1EnemyPrefixOperations(value,profile);
    assert(first.audioEvents.empty());
    assert(first.prefix.entities[0].collisionContactAllocationIndex == 1);
    assert(!first.prefix.entities[0].collisionContactIsPlayer);
    assert(first.prefix.entities[1].collisionContactAllocationIndex == noContact);
    assert(first.prefix.entities[1].position == (FixedPosition{-1672,2862,950}));
    assert(first.prefix.entities[1].yaw == 26833 && first.prefix.entities[1].stateBytes4dTo52[5] == 2);
    assert(first.prefix.allocationOrder == value.allocationOrder);
    //Original complete01D0..02A3 controlled constructor input; callback3 bypasses response.
    instructions(first.prefix,5001);

    const auto nextInput = carry(first.prefix);
    auto strict = profile;
    strict.contactDomain = Level1ActorContactDomain::none;
    rejects([&]() { level1EnemyPrefixOperations(nextInput,strict); });
    const auto next = level1EnemyPrefixOperations(nextInput,profile);
    assert(next.prefix.entities[0].position == value.entities[0].position);
    assert(next.prefix.entities[0].stateBytes4dTo52[5] == 4 && next.prefix.entities[0].behaviorTimer == 63);
    assert(next.prefix.entities[0].collisionContactAllocationIndex == noContact);
    assert(next.prefix.entities[1].collisionContactAllocationIndex == noContact);
    assert(next.prefix.entities[1].position == (FixedPosition{-1812,2630,1000}));
    assert(next.prefix.entities[1].stateBytes4dTo52[5] == 0);
    //953C forces lift climb;944E clears tank contact after0E50 and3D10.
    instructions(next.prefix,5034);

    for (const auto yaw : {0,16384}) {
        auto moving = value;
        moving.entities[0].yaw = static_cast<std::uint16_t>(yaw);
        moving.entities[0].forwardVelocity = 225;
        moving.entities[1].position.y = 2900;
        const auto result = level1EnemyPrefixOperations(moving,profile);
        const auto& tank = result.prefix.entities[0];
        assert(tank.position == moving.entities[0].position && tank.yaw == yaw);
        assert(tank.forwardVelocity == -45 && tank.stateBytes4dTo52[5] == 4 && tank.behaviorTimer == 63);
        assert(tank.collisionContactAllocationIndex == noContact);
        instructions(result.prefix,yaw == 0 ? 4961 : 5107);
    }

    auto multiple = value;
    for (const auto index : {2U,3U}) {
        multiple.entities[index].position = multiple.entities[0].position;
    }
    const auto allContacts = level1EnemyPrefixOperations(multiple,profile);
    assert(allContacts.prefix.entities[2].collisionContactAllocationIndex == 0);
    assert(allContacts.prefix.entities[3].collisionContactAllocationIndex == 0);
    assert(allContacts.prefix.entities[0].stateBytes4dTo52[5] == 4);

    auto invalid = value;
    invalid.entities[0].collisionContactAllocationIndex = 0;
    rejects([&]() { level1EnemyPrefixOperations(invalid,profile); });
    invalid.entities[0].collisionContactAllocationIndex = 6;
    rejects([&]() { level1EnemyPrefixOperations(invalid,profile); });
    invalid.entities[0].collisionContactAllocationIndex = noContact;
    invalid.entities[0].collisionContactIsPlayer = true;
    rejects([&]() { level1EnemyPrefixOperations(invalid,profile); });
    invalid = value;
    invalid.entities[0].stateBytes4dTo52[4] = 5;
    rejects([&]() { level1EnemyPrefixOperations(invalid,profile); });
    invalid = value;
    invalid.entities[1].position = {};
    rejects([&]() { level1EnemyPrefixOperations(invalid,profile); });

    //0C0A writes every overlapping target and returns the last old-list match.
    std::vector<Level1Entity> actors(3,value.entities[0]);
    for (std::size_t index = 0; index < actors.size(); ++index) {
        actors[index].allocationIndex = index;
    }
    const auto contacts = actorContactOperations(actors,{1,0,2},0);
    assert(contacts.contactIndex == 2 && contacts.contactWrites.size() == 2);
    assert(contacts.contactWrites[0].actorIndex == 1 && contacts.contactWrites[1].actorIndex == 2);
}
