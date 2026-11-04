#include "stellar7/level2_preloop_prefix_work.hpp"
#include "stellar7/level2_projectile_prefix_work.hpp"
#include "stellar7/level2_contact_work.hpp"
#include "stellar7/campaign_loader.hpp"
#include "stellar7/deployment_prefix_work.hpp"
#include "stellar7/failure_control_work.hpp"
#include "level1_effect_constructor.hpp"
#include "level1_rebuild_abs.hpp"
#include "stellar7/level1_type7_work.hpp"

#include <algorithm>
#include <utility>
#include <limits>
#include <optional>
#include <stdexcept>

namespace stellar7 {
namespace {

constexpr std::int16_t sourceForwardLimit200 = 200;
constexpr std::int16_t sourceYawLimit512 = 512;
constexpr std::int64_t listRefreshDistance1024 = 1024;

void instructions(std::vector<NormalWorkOperation>& operations,
    const std::uint32_t count) {
    operations.push_back({NormalWorkKind::instructions, count});
}

std::uint32_t absoluteDifference(const std::int32_t first,
    const std::int32_t second, std::vector<NormalWorkOperation>& operations) {
    const auto difference = static_cast<std::uint32_t>(second)
        - static_cast<std::uint32_t>(first);
    instructions(operations, 6); // MOV/MOV/SUB/SBB/OR/JGE.
    if ((difference & 0x80000000U) == 0) {
        instructions(operations, 2);
        return difference;
    }
    instructions(operations, 2); // CMP low word/JNZ.
    if ((difference & 0xffffU) == 0x8000U) {
        instructions(operations, 2); // CMP high word/JNZ.
        if (difference == 0xffff8000U) {
            instructions(operations, 3); // Source -32768 clamp and jump.
            return 32767;
        }
    }
    instructions(operations, 5); // NEG/ADC/NEG and two stores.
    return 0U - difference;
}

bool withinRadius(const std::uint32_t difference, const std::uint32_t radius,
    const bool firstAxis, std::vector<NormalWorkOperation>& operations) {
    const auto high = static_cast<std::int16_t>(difference >> 16);
    const auto radiusHigh = static_cast<std::int16_t>(radius >> 16);
    instructions(operations, 4); // Load radius, compare high, JL.
    if (high < radiusHigh) {
        return true;
    }
    instructions(operations, 1);
    if (high > radiusHigh) {
        if (firstAxis) {
            instructions(operations, 1);
        }
        return false;
    }
    instructions(operations, 2);
    const auto within = (difference & 0xffffU) <= (radius & 0xffffU);
    if (!within && firstAxis) {
        instructions(operations, 1);
    }
    return within;
}

} // namespace

KinematicsWork flatKinematicsOperations(const Level1Entity& actor) {
    const auto pitch = static_cast<std::uint16_t>(actor.pitch + actor.pitchVelocity);
    const auto roll = static_cast<std::uint16_t>(actor.roll + actor.rollVelocity);
    if (pitch != 0 || roll != 0) {
        throw std::invalid_argument("flat kinematics requires zero updated pitch/roll");
    }
    KinematicsWork result{actor, {}};
    result.actor.position = {};
    advanceEntityKinematics(result.actor);
    result.actor.position.x = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(actor.position.x)
        + static_cast<std::uint32_t>(result.actor.position.x));
    result.actor.position.y = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(actor.position.y)
        + static_cast<std::uint32_t>(result.actor.position.y));
    result.actor.position.z = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(actor.position.z)
        + static_cast<std::uint32_t>(result.actor.position.z));
    instructions(result.operations, 15); // 3b3a..3b5e, flat branch.
    instructions(result.operations, 2); // Yaw guard.
    if (result.actor.yaw == 0) {
        instructions(result.operations, 4); // Direct forward y addition.
    } else {
        instructions(result.operations, 2); // Sine argument/CALLF.
        const auto sineIndex = static_cast<std::uint16_t>(result.actor.yaw + 0xc000U) >> 4;
        instructions(result.operations, (sineIndex & 0x800U) != 0 ? 17 : 15);
        instructions(result.operations, 24); // 3c80..3cb3, including cosine CALLF.
        const auto cosineIndex = result.actor.yaw >> 4;
        instructions(result.operations, (cosineIndex & 0x800U) != 0 ? 16 : 14);
        instructions(result.operations, 22); // 3cb8..3ce0 and y addition.
    }
    instructions(result.operations, 16); // Direct velocities and epilogue.
    return result;
}

namespace {
struct CollisionBody {
    std::int16_t type;
    std::uint32_t collisionRadius;
    FixedPosition position;
};

template<class Actor>
CollisionBody collisionBody(const Actor& actor) {
    return {actor.type, actor.collisionRadius, actor.position};
}

ActorOverlapWork overlapOperations(const CollisionBody first, const CollisionBody second) {
    ActorOverlapWork result{false, {}};
    instructions(result.operations, 7);
    if (first.type == 7) {
        instructions(result.operations, 6);
        return result;
    }
    instructions(result.operations, 3);
    if (second.type == 7) {
        instructions(result.operations, 6);
        return result;
    }
    const auto sum = first.collisionRadius + second.collisionRadius;
    const auto radius = (sum >> 1) | (sum & 0x80000000U);
    instructions(result.operations, 8);
    const std::array<std::int32_t, 3> firstPosition{
        first.position.x, first.position.y, first.position.z};
    const std::array<std::int32_t, 3> secondPosition{
        second.position.x, second.position.y, second.position.z};
    for (std::size_t axis = 0; axis < firstPosition.size(); ++axis) {
        const auto difference = absoluteDifference(firstPosition[axis],
            secondPosition[axis], result.operations);
        if (!withinRadius(difference, radius, axis == 0, result.operations)) {
            instructions(result.operations, 5);
            return result;
        }
    }
    result.overlaps = true;
    instructions(result.operations, 5);
    return result;
}
} // namespace

ActorOverlapWork actorOverlapOperations(const Level1Entity& first,
    const Level1Entity& second) {
    return overlapOperations(collisionBody(first), collisionBody(second));
}

ActorContactWork actorContactOperations(const std::vector<Level1Entity>& actors,
    const std::vector<std::size_t>& activeOrder, const std::size_t subjectIndex) {
    if (subjectIndex >= actors.size() || activeOrder.size() > 32767) {
        throw std::invalid_argument("invalid contact subject or active count");
    }
    std::vector<bool> seen(actors.size());
    for (const auto index : activeOrder) {
        if (index >= actors.size() || seen[index]) {
            throw std::invalid_argument("invalid contact active order");
        }
        seen[index] = true;
    }
    ActorContactWork result{std::numeric_limits<std::size_t>::max(), {}, {}};
    instructions(result.operations, 9);
    if (!activeOrder.empty()) {
        instructions(result.operations, 2);
    }
    for (const auto index : activeOrder) {
        instructions(result.operations, 4);
        if (index != subjectIndex) {
            instructions(result.operations, 2);
            if (actors[index].stateBytes4dTo52[0] != 253) {
                instructions(result.operations, 2);
                if (actors[index].stateBytes4dTo52[0] != 252) {
                    instructions(result.operations, 3);
                    const auto overlap = actorOverlapOperations(actors[index], actors[subjectIndex]);
                    result.operations.insert(result.operations.end(),
                        overlap.operations.begin(), overlap.operations.end());
                    instructions(result.operations, 3);
                    if (overlap.overlaps) {
                        instructions(result.operations, 2);
                        result.contactIndex = index;
                        result.contactWrites.push_back({index, subjectIndex});
                    }
                }
            }
        }
        instructions(result.operations, 5);
    }
    instructions(result.operations, 6);
    return result;
}

namespace {

void append(std::vector<NormalWorkOperation>& operations,
    const std::vector<NormalWorkOperation>& child) {
    operations.insert(operations.end(), child.begin(), child.end());
}

void requireVisiblePrefixPosition(const FixedPosition actor,
    const FixedPosition player) {
    constexpr std::int64_t sourceVisibilityLimit = 65000;
    const auto x = static_cast<std::int64_t>(actor.x) - player.x;
    const auto y = static_cast<std::int64_t>(actor.y) - player.y;
    if (x <= -sourceVisibilityLimit || x >= sourceVisibilityLimit
        || y <= -sourceVisibilityLimit || y >= sourceVisibilityLimit) {
        throw std::invalid_argument("stationary prefix excludes visibility-boundary positions");
    }
}

std::uint32_t distanceAbsolute(const std::uint32_t difference,
    std::vector<NormalWorkOperation>& operations) {
    instructions(operations, 8);
    if ((difference & 0x80000000U) == 0) {
        return difference;
    }
    instructions(operations, 2);
    if ((difference & 0xffffU) == 0x8000U) {
        instructions(operations, 2);
        if (difference == 0xffff8000U) {
            instructions(operations, 3);
            return 32767;
        }
    }
    instructions(operations, 3);
    return 0U - difference;
}

std::uint32_t distanceOperations(const FixedPosition first,
    const FixedPosition second, std::vector<NormalWorkOperation>& operations) {
    instructions(operations, 6);
    auto x = distanceAbsolute(static_cast<std::uint32_t>(second.x)
        - static_cast<std::uint32_t>(first.x), operations);
    auto y = distanceAbsolute(static_cast<std::uint32_t>(second.y)
        - static_cast<std::uint32_t>(first.y), operations);
    instructions(operations, 4);
    const auto xHigh = static_cast<std::int16_t>(x >> 16);
    const auto yHigh = static_cast<std::int16_t>(y >> 16);
    auto swap = false;
    if (yHigh >= xHigh) {
        instructions(operations, 1);
        if (yHigh > xHigh) {
            swap = true;
        } else {
            instructions(operations, 2);
            swap = (y & 0xffffU) > (x & 0xffffU);
        }
    }
    if (swap) {
        instructions(operations, 10);
        std::swap(x, y);
    }
    instructions(operations, 20);
    const auto product = y * 3U;
    const auto shifted = (product >> 3)
        | ((product & 0x80000000U) != 0 ? 0xe0000000U : 0U);
    return x + shifted;
}

std::uint16_t headingOperations(const FixedPosition actor,
    const FixedPosition player, std::vector<NormalWorkOperation>& operations) {
    const auto signedX = static_cast<std::int16_t>(
        static_cast<std::uint32_t>(player.x) - static_cast<std::uint32_t>(actor.x));
    const auto signedY = static_cast<std::int16_t>(
        static_cast<std::uint32_t>(player.y) - static_cast<std::uint32_t>(actor.y));
    const auto x = static_cast<std::uint32_t>(signedX < 0 ? -static_cast<int>(signedX) : signedX);
    const auto y = static_cast<std::uint32_t>(signedY < 0 ? -static_cast<int>(signedY) : signedY);
    instructions(operations, 4);
    instructions(operations, signedX < 0 ? 10 : 6);
    instructions(operations, signedY < 0 ? 10 : 6);
    instructions(operations, 3);
    if (x == 0) {
        instructions(operations, 2);
    } else {
        instructions(operations, 3);
        if (y == 0) {
            instructions(operations, 2);
        } else {
            instructions(operations, 4);
            if (x == y) {
                instructions(operations, 4);
            } else {
                if ((x & 0xffffU) == (y & 0xffffU)) {
                    instructions(operations, 2);
                }
                instructions(operations, 5);
                instructions(operations, y > x ? 11 : 13);
                instructions(operations, 30); // E0D0 positive16-bit denominator.
                instructions(operations, 2); // PUSH ratio and CALLF atan.
                instructions(operations, 5); // 16e6:34a1 table accessor through RETF.
                instructions(operations, y > x ? 4 : 2);
            }
        }
    }
    instructions(operations, signedX < 0 ? 5 : 2);
    instructions(operations, signedY < 0 ? 5 : 2);
    instructions(operations, 7);
    return presenterTargetHeading(actor, player);
}

struct EnemyFireContext;
bool level1FireGuardOperations(Level1Entity& actor,
    std::vector<NormalWorkOperation>& operations, EnemyFireContext* enemy);

void type8Operations(Level1Entity& actor, std::vector<NormalWorkOperation>& operations,
    EnemyFireContext* enemy = nullptr) {
    constexpr std::uint16_t steeringLimit = (1U << 7) + 15;
    instructions(operations, 7); // 94da type8 dispatch.
    instructions(operations, 2); // Steering CALL.
    const auto difference = static_cast<std::uint16_t>(actor.targetHeading - actor.yaw);
    const auto signedDifference = static_cast<std::int16_t>(difference);
    const auto magnitude = static_cast<std::uint16_t>(signedDifference < 0
        ? -static_cast<int>(signedDifference) : signedDifference);
    instructions(operations, 23); // 9890 through magnitude clamp branch.
    instructions(operations, magnitude < steeringLimit ? 1 : 2);
    instructions(operations, 2);
    instructions(operations, 1); // Negative NEG or positive JMP.
    actor.yawVelocity = static_cast<std::int16_t>(
        difference >= 0x8000U ? -std::min(magnitude, steeringLimit)
                            : std::min(magnitude, steeringLimit));
    instructions(operations, 6); // Store and steering epilogue.
    instructions(operations, 3); // Cleanup, argument, range CALL.
    instructions(operations, 6); // 98dc high-word range test.
    const auto high = static_cast<std::int16_t>(actor.presenterDistance >> 16);
    auto inRange = high < 0;
    if (high == 0) {
        instructions(operations, 2);
        inRange = (actor.presenterDistance & 0xffffU) <= 0x6000U;
    }
    instructions(operations, 3); // Range return.
    instructions(operations, 3); // 94ff cleanup/test/branch.
    if (inRange) {
        instructions(operations, 3); // Jump and98fa argument/CALL.
        if (enemy) {
            level1FireGuardOperations(actor,operations,enemy);
        } else {
            instructions(operations, 9); // Fire guard prologue through cooldown branch.
            if (actor.weaponCooldown != 0) {
                --actor.weaponCooldown;
                instructions(operations, 2);
            } else {
                instructions(operations, 7);
                if (static_cast<std::int16_t>(magnitude) < 0x200) {
                    instructions(operations, 3);
                    if (actor.weaponSlotCount != 0) {
                        throw std::invalid_argument("stationary prefix does not admit firing branch");
                    }
                }
            }
            instructions(operations, 6); // Fire return.
        }
        instructions(operations, 2); // Jump and argument cleanup.
    }
    instructions(operations, 4); // Callback return.
}

enum class Type13VelocityDomain { distantOnly, headingChase };

void type13Operations(Level1Entity& actor, const Level1Entity& player,
    const std::vector<Level1Entity>& actors,
    std::vector<NormalWorkOperation>& operations,
    const Type13VelocityDomain domain = Type13VelocityDomain::distantOnly) {
    constexpr std::int16_t planarSpeed = 112;
    constexpr std::int32_t altitudeStep = 45;
    constexpr std::int32_t maximumAltitude = 1000;
    if (actor.position.z < 0 || actor.position.z > maximumAltitude) {
        throw std::invalid_argument("unsupported retained type13 contact/altitude");
    }
    if (actor.collisionContactIsPlayer) {
        if (domain!=Type13VelocityDomain::headingChase || actor.active()) {
            throw std::invalid_argument("Type13 player contact requires its retired source record");
        }
        instructions(operations,19); //968E..96D7 then97EA: retained player contact returns.
        return;
    }
    const auto contact = actor.collisionContactAllocationIndex;
    const bool touching = contact != std::numeric_limits<std::size_t>::max();
    if (touching && (contact >= actors.size() || actors[contact].type != 30
        || actors[contact].stateBytes4dTo52[0] != 254 || actors[contact].stateBytes4dTo52[4] != 0)) {
        throw std::invalid_argument("type13 prefix requires a fixed type30 contact");
    }
    const auto x = static_cast<std::int32_t>(static_cast<std::uint32_t>(actor.position.x)
        - static_cast<std::uint32_t>(player.position.x));
    const auto y = static_cast<std::int32_t>(static_cast<std::uint32_t>(actor.position.y)
        - static_cast<std::uint32_t>(player.position.y));
    // 9a34's special absolute-value clamp is not part of the qualified far path.
    if (x == -32768 || y == -32768) {
        throw std::invalid_argument("stationary prefix excludes type13 absolute-value clamp");
    }
    constexpr std::int32_t chaseAxisLimit = 2000;
    const auto nearX = x > -chaseAxisLimit && x < chaseAxisLimit;
    const auto nearY = y > -chaseAxisLimit && y < chaseAxisLimit;
    if (domain == Type13VelocityDomain::distantOnly && (nearX || nearY)) {
        throw std::invalid_argument("stationary prefix excludes near type13 velocity branch");
    }
    if (touching) {
        actor.position.z += altitudeStep;
        actor.collisionContactAllocationIndex = std::numeric_limits<std::size_t>::max();
        if (actor.position.z > maximumAltitude) {
            instructions(operations, 32); // Contact lift, source height cap and return; no velocity CALL.
            actor.position.z = maximumAltitude;
            return;
        }
        instructions(operations, 29); // Contact lift/clear, height guard and9792 velocity CALL.
    } else {
        instructions(operations, 10); // 968e type/contact guard, argument/CALL.
    }
    instructions(operations, 23); // 9a34 through x velocity sign branch.
    instructions(operations, x < 0 ? 2 : 3);
    actor.xVelocity = x < 0 ? planarSpeed : -planarSpeed;
    instructions(operations, 2);
    instructions(operations, y < 0 ? 2 : 3);
    actor.yVelocity = y < 0 ? planarSpeed : -planarSpeed;
    instructions(operations, 2);
    if (x < 0) {
        instructions(operations, 2);
        if ((static_cast<std::uint32_t>(x) & 0xffffU) == 0x8000U) {
            instructions(operations, 2);
        }
        instructions(operations, 7);
    } else {
        instructions(operations, 5);
    }
    const auto absX = static_cast<std::uint32_t>(x < 0 ? -static_cast<std::int64_t>(x) : x);
    instructions(operations, absX >= 65536 ? 3 : 5);
    if (!nearX) { //9ADC skips the entire Y absolute/threshold walk for near X.
        instructions(operations, 2);
        if (y < 0) {
            instructions(operations, 2);
            if ((static_cast<std::uint32_t>(y) & 0xffffU) == 0x8000U) {
                instructions(operations, 2);
            }
            instructions(operations, 7);
        } else {
            instructions(operations, 5);
        }
        const auto absY = static_cast<std::uint32_t>(y < 0 ? -static_cast<std::int64_t>(y) : y);
        instructions(operations, absY >= 65536 ? 3 : nearY ? 5 : 6);
    }
    if (nearX || nearY) {
        const auto heading = actor.targetHeading;
        const auto matrix = makeFixedMatrixQ14(0,0,heading);
        const auto negativeSine = -static_cast<int>(matrix.values[1]);
        const auto cosine = matrix.values[0];
        instructions(operations, 10); //9B2E..9B40: doubled speed, saved args, sine CALLF.
        instructions(operations, (static_cast<std::uint16_t>(heading+0xc000U) & 0x8000U) ? 17 : 15);
        instructions(operations, 6); //9B45..9B4D: cleanup/negative sine/E16A CALL.
        instructions(operations, negativeSine < 0 ? 19 : 11);
        instructions(operations, 18+4); //9B50..9B7E: shift/store X, cosine args/CALLF.
        instructions(operations, (heading & 0x8000U) ? 16 : 14);
        instructions(operations, 5); //9B83..9B89: cosine/E16A CALL.
        instructions(operations, cosine < 0 ? 19 : 11);
        instructions(operations, 18); //9B8C..9BAE: shift/store Y.
        Level1Entity velocity{};
        velocity.yaw = heading;
        velocity.forwardVelocity = 2 * planarSpeed;
        advanceEntityKinematics(velocity);
        actor.xVelocity = static_cast<std::int16_t>(velocity.position.x);
        actor.yVelocity = static_cast<std::int16_t>(velocity.position.y);
    }
    instructions(operations, 5); // Velocity helper return.
    if (touching) {
        instructions(operations, 5); // 9796 cleanup and callback return.
        return;
    }
    instructions(operations, 3); // Callercleanup/z test.
    if (actor.position.z != 0) {
        instructions(operations, 11); // Positiveheight branch, subtractstep and signedhigh guard.
        actor.position.z -= altitudeStep;
        if (actor.position.z < 0) {
            instructions(operations, 3); // 9748 zero both coordinate words.
            actor.position.z = 0;
        } else {
            instructions(operations, 1); // 9745 epilogue jump.
        }
        instructions(operations, 4);
        return;
    }
    instructions(operations, 4); // Zero z guard and epilogue jump.
    instructions(operations, 4); // Callback return.
}

} // namespace

enum class Level2PrefixPlayerDomain { forward, flatControls };

static StationaryLevel2PrefixWork level2PrefixOperations(
    const StationaryLevel2PrefixInput& input, const Level2PrefixPlayerDomain domain) {
    const auto& incomingPlayer = input.player;
    if (input.actors.size() != 7 || input.activeOrder.size() != 7
        || input.refreshRequested
        || input.fadeTicks != 0 || input.specialActorPresent || input.impulse != 0
        || !input.active || input.projectileHandlePresent[0] || input.projectileHandlePresent[1]
        || incomingPlayer.pitch != 0 || incomingPlayer.roll != 0
        || (domain == Level2PrefixPlayerDomain::forward
            && (incomingPlayer.yaw != 0 || incomingPlayer.yawVelocity != 0))
        || incomingPlayer.yawVelocity < -sourceYawLimit512 || incomingPlayer.yawVelocity > sourceYawLimit512
        || incomingPlayer.pitchVelocity != 0 || incomingPlayer.rollVelocity != 0
        || incomingPlayer.xVelocity != 0
        || incomingPlayer.yVelocity != 0 || incomingPlayer.zVelocity != 0
        || (incomingPlayer.forwardVelocity != 0 && incomingPlayer.position.z != 0)
        || incomingPlayer.forwardVelocity < -sourceForwardLimit200
        || incomingPlayer.forwardVelocity > sourceForwardLimit200) {
        throw std::invalid_argument("unsupported retained Level2 prefix profile");
    }
    for (const auto& actor : input.actors) {
        requireVisiblePrefixPosition(actor.position, incomingPlayer.position);
        if (!actor.active() || (actor.type != 8 && actor.type != 13 && actor.type != 30)
            || (actor.type == 30 && (actor.stateBytes4dTo52[0] != 254
                || actor.stateBytes4dTo52[4] != 0))
            || (actor.type != 30 && (actor.stateBytes4dTo52[0] != 255
                || actor.stateBytes4dTo52[4] != (actor.type == 8 ? 2 : 4)))) {
            throw std::invalid_argument("unsupported stationary constructor actor");
        }
    }
    StationaryLevel2PrefixWork result{incomingPlayer, input.actors, input.activeOrder, {}};
    instructions(result.operations, 7); // Three source guards then0784 CALL.
    instructions(result.operations, 7); // 0784 through player kinematics CALL.
    const auto playerKinematics = flatKinematicsOperations(incomingPlayer);
    result.player = playerKinematics.actor;
    append(result.operations, playerKinematics.operations);
    instructions(result.operations, 4);

    for (const auto& actor : result.actors) {
        requireVisiblePrefixPosition(actor.position, result.player.position);
    }
    auto storage = result.actors;
    storage.push_back(result.player);
    const auto playerIndex = result.actors.size();
    const auto initialContact = actorContactOperations(storage, result.activeOrder, playerIndex);
    if (initialContact.contactIndex != std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("retained prefix excludes player contact");
    }
    append(result.operations, initialContact.operations);
    instructions(result.operations, 9); // No contact branch and actor loop setup.
    for (std::size_t position = 0; position < result.activeOrder.size(); ++position) {
        const auto index = result.activeOrder[position];
        auto& actor = storage[index];
        instructions(result.operations, 8); // Actor state guards.
        if (actor.stateBytes4dTo52[0] == 254) {
            instructions(result.operations, 3); // Non42 skips integration.
        } else {
            instructions(result.operations, 3); // DIclear/argument/CALL.
            const auto kinematics = flatKinematicsOperations(actor);
            actor = kinematics.actor;
            append(result.operations, kinematics.operations);
            instructions(result.operations, 4); // Cleanup/playerargument/CALL.
            const auto overlap = actorOverlapOperations(actor, result.player);
            append(result.operations, overlap.operations);
            if (overlap.overlaps) {
                throw std::invalid_argument("stationary prefix excludes actor/player collision");
            }
            instructions(result.operations, 3);
            instructions(result.operations, 4); // DIguard/actorargument/CALL.
            const auto contacts = actorContactOperations(storage, result.activeOrder, index);
            const bool touching = contacts.contactIndex != std::numeric_limits<std::size_t>::max();
            if (touching && (actor.type != 13 || contacts.contactWrites.size() != 1
                || storage[contacts.contactIndex].type != 30
                || storage[contacts.contactIndex].stateBytes4dTo52[0] != 254)) {
                throw std::invalid_argument("stationary prefix excludes actor collision");
            }
            append(result.operations, contacts.operations);
            for (const auto& write : contacts.contactWrites) {
                storage[write.actorIndex].collisionContactAllocationIndex = write.subjectIndex;
                storage[write.actorIndex].collisionContactIsPlayer = false;
            }
            instructions(result.operations, 6); // Cleanup/DIcopy/callback/contact guards.
            if (touching) {
                actor.collisionContactAllocationIndex = contacts.contactIndex;
                actor.collisionContactIsPlayer = false;
                instructions(result.operations, 3); // Subjectcontactstore, argument and0e50 CALL.
                instructions(result.operations, 12); // State4 ignores state1/state5 changes; RET.
                instructions(result.operations, 9); // Cleanup, target/type/level guards; no backout/damage.
            }
        }
        instructions(result.operations, position + 1 == result.activeOrder.size() ? 5 : 6);
    }
    instructions(result.operations, 2); // First0a9a argument/CALL.
    instructions(result.operations, 10); // Nullhandle0a9a throughRET.
    instructions(result.operations, 3); // Cleanup/secondargument/CALL.
    instructions(result.operations, 10);
    instructions(result.operations, 6); // Cleanup/0784return.
    result.actors.assign(storage.begin(), storage.begin() + playerIndex);
    instructions(result.operations, 8); // Style/draw/activeguard/viewselect/9298CALL.
    instructions(result.operations, 13); // 9298 playerorigin/profile and loopsetup.
    for (const auto index : result.activeOrder) {
        auto& actor = result.actors[index];
        requireVisiblePrefixPosition(actor.position, result.player.position);
        instructions(result.operations, 8);
        actor.presenterDistance = distanceOperations(result.player.position, actor.position, result.operations);
        instructions(result.operations, 16);
        actor.targetHeading = headingOperations(actor.position, result.player.position, result.operations);
        instructions(result.operations, 4);
        if (actor.stateBytes4dTo52[0] == 255) {
            instructions(result.operations, 8);
            if (actor.type == 8) {
                type8Operations(actor, result.operations);
            } else {
                type13Operations(actor, result.player, result.actors, result.operations);
            }
            instructions(result.operations, 1);
        }
        instructions(result.operations, 6);
    }
    instructions(result.operations, 5); // 9298return.
    instructions(result.operations, 1); // 14b0CALL.
    instructions(result.operations, 8); // Sortprologue.
    for (std::size_t destination = 0; destination < result.activeOrder.size(); ++destination) {
        instructions(result.operations, 9); // Zero maximum and scansetup.
        std::uint32_t maximum = 0;
        auto selected = destination;
        for (std::size_t candidate = destination; candidate < result.activeOrder.size(); ++candidate) {
            instructions(result.operations, 9);
            const auto distance = distanceOperations(result.player.position,
                result.actors[result.activeOrder[candidate]].position, result.operations);
            instructions(result.operations, 4); // Cleanup/stores/levelcompare.
            instructions(result.operations, 4); // Level2 type46 override test fails.
            instructions(result.operations, 4); // Maximum highcompare/JL.
            const auto maximumHigh = static_cast<std::int16_t>(maximum >> 16);
            const auto distanceHigh = static_cast<std::int16_t>(distance >> 16);
            if (distanceHigh >= maximumHigh) {
                instructions(result.operations, 1);
                if (distanceHigh == maximumHigh) {
                    instructions(result.operations, 2);
                }
            }
            if (distance > maximum) {
                instructions(result.operations, 5);
                maximum = distance;
                selected = candidate;
            }
            instructions(result.operations, 5);
        }
        instructions(result.operations, 3);
        if (selected != destination) {
            instructions(result.operations, 8);
            std::swap(result.activeOrder[selected], result.activeOrder[destination]);
        }
        instructions(result.operations, destination + 1 == result.activeOrder.size() ? 4 : 5);
    }
    instructions(result.operations, 5); // Sortreturn.
    const auto displacementX = static_cast<std::int64_t>(result.player.position.x)
        - input.lastListCamera.x;
    const auto displacementY = static_cast<std::int64_t>(result.player.position.y)
        - input.lastListCamera.y;
    if (displacementX <= -listRefreshDistance1024
        || displacementX >= listRefreshDistance1024
        || displacementY <= -listRefreshDistance1024
        || displacementY >= listRefreshDistance1024) {
        throw std::invalid_argument("retained prefix exceeds list refresh displacement");
    }
    instructions(result.operations, 10); // Sevenargumentwords and3768CALL.
    instructions(result.operations, 10); // 3768refreshguard.
    instructions(result.operations, 14); // Coordinate differences and x signguard.
    if (displacementX < 0) {
        instructions(result.operations, 2); // Negative x CMP/JNZ.
        instructions(result.operations, 3); // Negative x NEG/ADC/NEG.
    }
    instructions(result.operations, 2); // x stores.
    instructions(result.operations, 5); // x thresholdtest.
    instructions(result.operations, displacementY < 0 ? 9 : 5); // y sign/absolute value/load/jump.
    instructions(result.operations, 2); // y stores.
    instructions(result.operations, 6); // y threshold and retainlist jump.
    instructions(result.operations, 6); // 3768return.
    instructions(result.operations, 5); // Callercleanup/countstore/equalitybranch.
    return result;
}

StationaryLevel2PrefixWork retainedLevel2PrefixOperations(const StationaryLevel2PrefixInput& input) {
    return level2PrefixOperations(input,Level2PrefixPlayerDomain::forward);
}

StationaryLevel2PrefixWork controlledLevel2PrefixOperations(const StationaryLevel2PrefixInput& input) {
    return level2PrefixOperations(input,Level2PrefixPlayerDomain::flatControls);
}

StationaryLevel2PrefixWork stationaryLevel2PrefixOperations(
    const StationaryLevel2PrefixInput& input) {
    if (input.player.forwardVelocity != 0
        || !(input.lastListCamera == input.player.position)) {
        throw std::invalid_argument("unsupported stationary Level2 prefix profile");
    }
    return retainedLevel2PrefixOperations(input);
}

namespace {
constexpr std::size_t level1ActorCount = 6;
constexpr std::size_t initialOrdinaryActorCount = 2;
constexpr std::size_t maximumLevel1ActorCount = 14; //Ten ordinary and four fixed records.

bool ordinaryStorageIndex(const std::size_t index) {
    return index < initialOrdinaryActorCount || index >= level1ActorCount;
}
constexpr std::uint16_t level1TankTurnLimit = 143;
constexpr std::int16_t level1LiftTurnLimit = 271;
constexpr std::int16_t level1TankCruiseSpeed = 225;
constexpr std::int16_t level1LiftCruiseSpeed = 270;
constexpr std::int32_t level1LiftCruiseAltitude = 1000;

struct Class1Tables {
    std::uint16_t choice2, choice3;
    std::int16_t cruise, reverse, factor;
    std::uint16_t reset2;
    std::uint8_t reset3;
};
//DS0AF6/0B06/0A00/0A22/0A44/0AD6/0AE6, indexed by actual source type.
constexpr Class1Tables tankTables{50,102,225,45,1,12,5};
constexpr Class1Tables type3Tables{48,128,216,72,3,10,6};
constexpr Class1Tables type4Tables{50,96,180,90,2,8,6};
constexpr std::int16_t type3MaximumTurn=3*256; //State4 factor<<8; bounds9842/9890 too.
bool type3Family(const Level1OrdinaryFamily family) {
    return family==Level1OrdinaryFamily::type1And3;
}
bool airborneAllocationDomain(const Level1ProjectileAllocationDomain domain) {
    return domain==Level1ProjectileAllocationDomain::retainedAirborne || domain==Level1ProjectileAllocationDomain::retainedRoster;
}
bool airborneOwner(const Level1Entity& owner,const Level1OrdinaryFamily family) {
    return owner.type==11 || (type3Family(family) && owner.type==3);
}


KinematicsWork level1KinematicsOperations(const Level1Entity& actor) {
    const auto updatedPitch = static_cast<std::uint16_t>(actor.pitch + actor.pitchVelocity);
    const auto updatedRoll = static_cast<std::uint16_t>(actor.roll + actor.rollVelocity);
    if (updatedPitch == 0 && updatedRoll == 0) {
        return flatKinematicsOperations(actor);
    }
    //3B61 takes the longer forward-vector route for bank alone, with pitch zero.
    KinematicsWork result{actor, {}};
    result.actor.position = {};
    advanceEntityKinematics(result.actor);
    result.actor.position.x = static_cast<std::int32_t>(static_cast<std::uint32_t>(actor.position.x)
        + static_cast<std::uint32_t>(result.actor.position.x));
    result.actor.position.y = static_cast<std::int32_t>(static_cast<std::uint32_t>(actor.position.y)
        + static_cast<std::uint32_t>(result.actor.position.y));
    result.actor.position.z = static_cast<std::int32_t>(static_cast<std::uint32_t>(actor.position.z)
        + static_cast<std::uint32_t>(result.actor.position.z));
    const auto yawIndex = result.actor.yaw >> 4;
    const auto sineIndex = static_cast<std::uint16_t>(result.actor.yaw + 0xc000U) >> 4;
    const auto sineNegative = yawIndex > 0x800;
    const auto cosineNegative = yawIndex > 0x400 && yawIndex < 0xc00;
    instructions(result.operations, 14 + 2 + 14); //3B3A..3B64 and pitch cosine.
    instructions(result.operations, 24);
    instructions(result.operations, (sineIndex & 0x800U) != 0 ? 17 : 15);
    instructions(result.operations, 5);
    instructions(result.operations, actor.forwardVelocity > 0 || sineNegative ? 19 : 11); //E16A.
    instructions(result.operations, 21 + 14 + 23);
    instructions(result.operations, (yawIndex & 0x800U) != 0 ? 16 : 14);
    instructions(result.operations, 5);
    instructions(result.operations, actor.forwardVelocity < 0 || cosineNegative ? 19 : 11);
    instructions(result.operations, 21 + 17 + 22 + 16); //Pitch-zero sine follows the reflected table path.
    return result;
}

void flatBackoutOperations(Level1Entity& actor, std::vector<NormalWorkOperation>& operations) {
    if (actor.pitch != 0 || actor.roll != 0) {
        throw std::invalid_argument("ordinary actor backout requires flat motion");
    }
    auto translation = actor;
    translation.position = {};
    reverseEntityTranslation(translation);
    actor.position.x = static_cast<std::int32_t>(static_cast<std::uint32_t>(actor.position.x)
        + static_cast<std::uint32_t>(translation.position.x));
    actor.position.y = static_cast<std::int32_t>(static_cast<std::uint32_t>(actor.position.y)
        + static_cast<std::uint32_t>(translation.position.y));
    actor.position.z = static_cast<std::int32_t>(static_cast<std::uint32_t>(actor.position.z)
        + static_cast<std::uint32_t>(translation.position.z));
    instructions(operations, 21 + 2); //3D10..3D40;3E56/3E5A flat yaw guard.
    if (actor.yaw == 0) {
        instructions(operations, 2); //3EC6/3EC9 direct forward value.
    } else {
        instructions(operations, 2); //3E5C/3E5F sine argument/CALLF.
        const auto sineIndex = static_cast<std::uint16_t>(actor.yaw + 0xc000U) >> 4;
        instructions(operations, (sineIndex & 0x800U) != 0 ? 17 : 15);
        instructions(operations, 24); //3E64..3E97 including cosine CALLF.
        instructions(operations, ((actor.yaw >> 4) & 0x800U) != 0 ? 16 : 14);
        instructions(operations, 20); //3E9C..3EC4 signed product/shift and jump.
    }
    instructions(operations, 6); //3ECA..3ED4 y subtraction and return.
}

} // namespace

Level1SolidContactWork level1PlayerImpulseOperations(const Level1Entity& player,const std::int16_t impulse) {
    if (impulse < -sourceForwardLimit200 || impulse > sourceForwardLimit200) {
        throw std::invalid_argument("solid-contact momentum exceeds admitted player velocity");
    }
    Level1SolidContactWork result{player,impulse,{},{}};
    instructions(result.operations,2); //0224 CMP/0229 JZ.
    if (impulse != 0) {
        result.player.forwardVelocity=impulse;
        instructions(result.operations,5); //022B..023A player store and sign branch.
        result.impulse=static_cast<std::int16_t>(impulse / 2);
        instructions(result.operations,impulse > 0 ? 2 : 5); //023C/0240 or0242..024B.
    }
    result.receipts.push_back({Level1ContactStep::momentumEntry,0x0224,0x024e,0,result.operations.size(),{}, {}});
    return result;
}

namespace {
bool ordinarySolidTarget(const Level1Entity& target,
    const Level1OrdinaryFamily family=Level1OrdinaryFamily::type1) {
    const auto category=target.stateBytes4dTo52[0];
    const auto callback=target.stateBytes4dTo52[4];
    return ((target.type==1 || (type3Family(family) && target.type==3)) && category==255 && callback==1)
        || (target.type==11 && category==255 && callback==3)
        || (target.type==29 && category==254 && callback==0);
}
}

enum class SolidContactDomain { level1, level2 };
static bool level2SolidTarget(const Level1Entity& target) {
    const auto ordinary=target.stateBytes4dTo52[0]==255;
    return (ordinary && ((target.type==1 || target.type==4) && target.stateBytes4dTo52[4]==1
        || target.type==8 && target.stateBytes4dTo52[4]==2 || target.type==13 && target.stateBytes4dTo52[4]==4))
        || (target.type==30 && target.stateBytes4dTo52[0]==254 && target.stateBytes4dTo52[4]==0);
}
static Level1SolidContactWork solidContactOperations(const Level1Entity& player,
    const Level1Entity& target,const std::size_t targetIndex,const Level1OrdinaryFamily family,
    const SolidContactDomain domain=SolidContactDomain::level1,const std::uint32_t ownedRadius=0,
    std::optional<std::size_t>* damageChildOffset=nullptr) {
    const auto level2=domain==SolidContactDomain::level2;
    if (!(level2 ? level2SolidTarget(target) : ordinarySolidTarget(target,family)) || player.type!=0 || player.position.z!=0 || player.pitch!=0 || player.roll!=0
        || player.xVelocity!=0 || player.yVelocity!=0 || player.zVelocity!=0
        || player.forwardVelocity < -sourceForwardLimit200 || player.forwardVelocity > sourceForwardLimit200
        || player.collisionRadius!=723 || target.collisionRadius!=(level2 ? ownedRadius
            : target.type==1 ? 1342U : target.type==3 ? 770U : target.type==11 ? 748U : 862U)
        || (level2 && (ownedRadius==0 || !damageChildOffset))
        || !actorOverlapOperations(target,player).overlaps) {
        throw std::invalid_argument("unsupported Level1 solid-contact caller");
    }
    Level1SolidContactWork result{player,0,{},{}};
    instructions(result.operations,6); //07A3..07B4 ability0 contact.
    instructions(result.operations,(target.type==29 || target.type==30) ? 22 : 18); //07ED..08C0 exact skipped special-type guards.
    if (level2 && target.type==4) {*damageChildOffset=result.operations.size();}
    result.receipts.push_back({Level1ContactStep::playerDispatch,0x07a3,0x08cc,0,result.operations.size(),{},targetIndex});
    instructions(result.operations,3); //08CC load/CMP/JNZ.
    if (result.player.forwardVelocity==0) {
        result.player.forwardVelocity=10;
        instructions(result.operations,1); //08D6 stationary escape velocity.
    }
    bool overlaps;
    do {
        const auto begin=result.operations.size();
        const auto before=result.player.position;
        instructions(result.operations,2); //08DB PUSH/08DF CALL.
        flatBackoutOperations(result.player,result.operations);
        if (result.player.position==before) {
            throw std::invalid_argument("source backout has no translation");
        }
        instructions(result.operations,4); //08E2..08EA cleanup and overlap caller.
        const auto overlap=actorOverlapOperations(target,result.player);
        append(result.operations,overlap.operations);
        instructions(result.operations,3); //08ED cleanup/OR/JNZ.
        overlaps=overlap.overlaps;
        result.receipts.push_back({Level1ContactStep::playerBackout,0x08db,0x08f4,begin,result.operations.size(),{},targetIndex});
    } while (overlaps);
    const auto rebound=result.operations.size();
    result.player.forwardVelocity=static_cast<std::int16_t>(-result.player.forwardVelocity);
    result.impulse=result.player.forwardVelocity;
    instructions(result.operations,5); //08F4..0902: negate then copy to0074.
    result.receipts.push_back({Level1ContactStep::playerRebound,0x08f4,0x0905,rebound,result.operations.size(),{},targetIndex});
    const auto audio=result.operations.size();
    result.operations.push_back({level2 ? NormalWorkKind::gameplayLevel2SolidContactEvent
        : NormalWorkKind::gameplaySolidContactEvent,1,9});
    result.receipts.push_back({Level1ContactStep::playerAudio,0x0905,0x0848,audio,result.operations.size(),{},targetIndex});
    instructions(result.operations,1); //0848 JMP094F; marker already owns090C/0845.
    return result;
}

Level1SolidContactWork level1SolidContactOperations(const Level1Entity& player,
    const Level1Entity& target,const std::size_t targetIndex) {
    return solidContactOperations(player,target,targetIndex,Level1OrdinaryFamily::type1);
}

Level2SolidContactWork level2SolidContactOperations(const Level1Entity& player,
    const Level1Entity& target,const std::size_t targetIndex,const std::uint32_t ownedRadius) {
    Level2SolidContactWork result;
    result.contact=solidContactOperations(player,target,targetIndex,Level1OrdinaryFamily::type1,
        SolidContactDomain::level2,ownedRadius,&result.damageChildOffset);
    return result;
}
Level2Type4DamageWork level2Type4DamageOperations(const std::int16_t armor,const Level1PlayerCombatDomain domain) {
    constexpr std::int16_t scaledNegative25Damage=100;
    const auto lifecycle=domain==Level1PlayerCombatDomain::expiryAndLoss;
    if ((!lifecycle && domain!=Level1PlayerCombatDomain::survivingOnly)
        || armor<(lifecycle ? -level2MaximumFrameDamage+scaledNegative25Damage : scaledNegative25Damage)
        || armor>680) {
        throw std::invalid_argument("Type4 negative damage exceeds the retained Level2 armor domain");
    }
    Level2Type4DamageWork result{static_cast<std::int16_t>(armor-scaledNegative25Damage),8,{},{}};
    instructions(result.operations,2); //0E7C/0E7D.
    result.operations.push_back({NormalWorkKind::gameplayLevel2TargetHitEvent,1,6});
    instructions(result.operations,2+3+5); //0E88..0EA0, negative ADD-100 branch.
    instructions(result.operations,2+1+8); //0EB1..0ECB;132C=8 and palette caller.
    instructions(result.operations,11); //Resident1B38..1B50, loaded leaf remains explicit.
    result.palette={result.operations.size(),{128,128,238,15}};
    instructions(result.operations,3+3); //Resident return;0ED0..0ED3.
    return result;
}

namespace {

std::uint16_t prefixRandom(StellarRng& rng, std::vector<NormalWorkOperation>& operations) {
    instructions(operations, 20 + (rng.indexA() == 0 ? 1 : 0) + (rng.indexB() == 0 ? 1 : 0));
    return rng.next();
}

std::uint16_t level1SteeringOperations(Level1Entity& actor, const std::uint16_t limit,
    std::vector<NormalWorkOperation>& operations) {
    const auto difference = static_cast<std::int16_t>(actor.targetHeading - actor.yaw);
    const auto magnitude = static_cast<std::uint16_t>(difference < 0 ? -static_cast<int>(difference) : difference);
    instructions(operations, 23);
    instructions(operations, magnitude < limit ? 1 : 2);
    instructions(operations, 2 + 1 + 6);
    actor.yawVelocity = static_cast<std::int16_t>(difference < 0
        ? -std::min(magnitude, limit) : std::min(magnitude, limit));
    return magnitude;
}

bool level1RangeOperations(const Level1Entity& actor,
    std::vector<NormalWorkOperation>& operations) {
    const auto high = static_cast<std::int16_t>(actor.presenterDistance >> 16);
    instructions(operations, high < 0 ? 5 : 6);
    if (high == 0) {
        instructions(operations, 2);
    }
    instructions(operations, 3);
    return high < 0 || (high == 0 && (actor.presenterDistance & 0xffffU) <= 0x6000U);
}

struct PlayerDamageContext {
    std::int16_t& armor;
    std::uint16_t& fadeTicks;
    std::vector<Level1PrefixPaletteStep>& paletteSteps;
    Level1PlayerCombatDomain domain = Level1PlayerCombatDomain::survivingOnly;
};

struct BonusPickupExecution {
    const Level1BonusPickupContext& input;
    Level1BonusPickupPrefixWork& output;
};

struct TargetPrefixContext {
    Level1TargetPrefixWork& result;
    const BitmapFont& scoreFont;
    const CockpitPageState& pages;
    std::uint16_t difficulty;
    Level1TargetHitDomain targetDomain;
    const Model3d* bonusModel = nullptr;
    std::uint16_t firstBlockCapacity = 0;
    std::vector<Level1PrefixBonusBirth>* bonusBirths = nullptr;
    BonusPickupExecution* pickup = nullptr;
    const Level1ArrivalPrefixContext* arrival = nullptr;
    const Level1DeploymentPrefixContext* deployment = nullptr;
    Level1DeploymentPrefixWork* deploymentOutput = nullptr;
};

struct EnemyFireContext {
    Level1ProjectilePrefixWork& work;
    const Level1EnemyPrefixProfile& profile;
    std::vector<Level1AudioEvent>& events;
    PlayerDamageContext* damage = nullptr;
    TargetPrefixContext* target = nullptr;
    const Level2HostileProjectileModel* level2Hostile = nullptr;
    const Level2PeriodicOrdinaryProfile* level2Periodic = nullptr;
};

bool enemyLaunchOperations(EnemyFireContext& context, Level1Entity& owner, std::size_t weaponSlot);

bool level1FireGuardOperations(Level1Entity& actor,
    std::vector<NormalWorkOperation>& operations, EnemyFireContext* enemy) {
    bool launched = false;
    instructions(operations, 9); //98FA through cooldown branch.
    if (actor.weaponCooldown != 0) {
        --actor.weaponCooldown;
        instructions(operations, 2);
    } else {
        const auto difference = static_cast<std::int16_t>(actor.targetHeading - actor.yaw);
        const auto magnitude = static_cast<std::uint16_t>(difference < 0
            ? -static_cast<int>(difference) : difference);
        instructions(operations, 7);
        //The source uses signed JNL even for the wrapped absolute value 8000h.
        if (static_cast<std::int16_t>(magnitude) < 512) {
            instructions(operations, 3);
            for (std::size_t slot = 0; slot < actor.weaponSlotCount; ++slot) {
                instructions(operations, 4);
                if (actor.weaponSlotTimers[slot] == 0) {
                    if (!enemy) {
                        throw std::invalid_argument("Level1 prefix reached projectile creation at995E");
                    }
                    instructions(operations, 2); //9934/9936, one success per98FA call.
                    if (!launched) {
                        instructions(operations, 3); //9938..993A.
                        launched = enemyLaunchOperations(*enemy, actor, slot);
                        instructions(operations, 3); //993D..9942.
                    }
                } else {
                    --actor.weaponSlotTimers[slot];
                    instructions(operations, 3);
                }
                instructions(operations, 5);
            }
        }
    }
    instructions(operations, 6);
    return launched;
}

void level1RandomSteeringOperations(Level1Entity& actor, StellarRng& rng,
    std::vector<NormalWorkOperation>& operations) {
    instructions(operations, 5); //9842 through RNG CALL.
    const auto choice = prefixRandom(rng, operations);
    instructions(operations, 3);
    if (static_cast<std::uint8_t>(choice) < 90) {
        actor.yawVelocity = 0;
        instructions(operations, 3);
    } else {
        const auto denominator = actor.type == 3 ? type3Tables.factor*128 : actor.type == 4 ? type4Tables.factor*128 : actor.type == 1 ? 128 : 256;
        auto turn = static_cast<std::int16_t>(static_cast<std::int16_t>(choice) % denominator + 256);
        instructions(operations, 12); //9860..987D, signed remainder and high-byte increment.
        if ((choice & 0x100U) != 0) {
            turn = static_cast<std::int16_t>(-turn);
            instructions(operations, 1);
        }
        actor.yawVelocity = turn;
    }
    actor.behaviorTimer = static_cast<std::uint8_t>((choice >> 8) & 0x7fU);
    instructions(operations, 7);
}

void level1TankOperations(Level1Entity& actor, StellarRng& rng,
    std::vector<NormalWorkOperation>& operations, EnemyFireContext* enemy = nullptr) {
    const auto& tables=actor.type==3 ? type3Tables : actor.type==4 ? type4Tables : tankTables;
    auto& behavior = actor.stateBytes4dTo52[5];
    instructions(operations, 7);
    if (behavior == 0) {
        actor.behaviorTimer = 0;
        instructions(operations, 3 + 5);
        const auto choice = static_cast<std::uint8_t>(prefixRandom(rng, operations));
        instructions(operations, 10);
        if (choice < tables.choice2) {
            behavior = 1;
        } else {
            instructions(operations, 3);
            behavior = choice < tables.choice3 ? 2 : 3;
        }
        instructions(operations, 5 + 2);
    }
    instructions(operations, 2); //Non-Type6 guard.
    instructions(operations, behavior < 4 ? 2 + 2 * behavior : behavior == 4 ? 11 : 13);
    if (behavior == 1 && actor.behaviorTimer == 0) {
        instructions(operations, 2 + 1);
        actor.forwardVelocity = tables.cruise;
        instructions(operations, 6);
        level1RandomSteeringOperations(actor, rng, operations);
        instructions(operations, 1 + 4);
        return;
    }
    if (behavior == 5) {
        actor.forwardVelocity = tables.cruise;
        instructions(operations, 6);
        level1RandomSteeringOperations(actor, rng, operations);
        instructions(operations, 1 + 4);
        return;
    }
    if (behavior == 2 || behavior == 3) {
        const auto stopped = behavior == 2;
        if (stopped) {
            actor.forwardVelocity = 0;
        }
        instructions(operations, stopped ? 3 : 2);
        level1SteeringOperations(actor, static_cast<std::uint16_t>(tables.factor*128+15), operations);
        instructions(operations, 3);
        const auto inRange = level1RangeOperations(actor, operations);
        instructions(operations, 3);
        if (inRange) {
            instructions(operations, 2);
            const auto launched = level1FireGuardOperations(actor, operations, enemy);
            instructions(operations, stopped ? 3 : 1);
            if (stopped && launched) {
                behavior = 0;
                instructions(operations, 1); //93E6, successful fire leaves stopped behavior.
            }
        } else if (stopped) {
            actor.stateBytes4dTo52[5] = 0;
            instructions(operations, 1);
        }
        instructions(operations, 1);
        const auto choice = static_cast<std::uint8_t>(prefixRandom(rng, operations));
        instructions(operations, stopped ? 7 : 10);
        if (choice < (stopped ? tables.reset2 : tables.reset3)) {
            actor.stateBytes4dTo52[5] = 0;
            instructions(operations, stopped ? 5 : 6);
        } else {
            instructions(operations, 5);
        }
        return;
    }
    if (behavior == 1) {
        instructions(operations, 2 + 2);
        --actor.behaviorTimer;
        if (actor.behaviorTimer == 0) {
            behavior = 0;
            instructions(operations, 2);
        } else {
            instructions(operations, 1);
        }
    } else { //944E behavior4 clears the retained source contact.
        actor.collisionContactAllocationIndex = std::numeric_limits<std::size_t>::max();
        actor.collisionContactIsPlayer = false;
        instructions(operations, 3);
        if (actor.behaviorTimer == 0) {
            instructions(operations, 1);
            const auto choice = prefixRandom(rng, operations);
            instructions(operations, 3);
            if (actor.forwardVelocity > 0) {
                actor.forwardVelocity = static_cast<std::int16_t>(-tables.reverse);
                instructions(operations, 5);
            } else {
                actor.forwardVelocity = tables.cruise;
                instructions(operations, 3);
            }
            actor.yawVelocity = static_cast<std::int16_t>((choice & 0x200U) != 0
                ? -tables.factor*256 : tables.factor*256);
            instructions(operations, 7 + ((choice & 0x200U) != 0 ? 1 : 0));
            actor.behaviorTimer = 63;
            instructions(operations, 1 + 4);
            return;
        }
        --actor.behaviorTimer;
        instructions(operations, 2);
        if (actor.behaviorTimer == 0) {
            behavior = 0;
            actor.forwardVelocity = tables.cruise;
            instructions(operations, 5);
        }
    }
    instructions(operations, 2);
    const auto inRange = level1RangeOperations(actor, operations);
    instructions(operations, 3);
    if (inRange) {
        instructions(operations, 2);
        level1FireGuardOperations(actor, operations, enemy);
        instructions(operations, 1 + 1);
    }
    instructions(operations, 4);
}

void level1LiftOperations(Level1Entity& actor, const Level1Entity& player, StellarRng& rng,
    std::vector<NormalWorkOperation>& operations, EnemyFireContext* enemy = nullptr) {
    auto& behavior = actor.stateBytes4dTo52[5];
    instructions(operations, 7); //952C..953A includes the contact guard.
    if (actor.collisionContactAllocationIndex != std::numeric_limits<std::size_t>::max()
        || actor.collisionContactIsPlayer) {
        actor.collisionContactAllocationIndex = std::numeric_limits<std::size_t>::max();
        actor.collisionContactIsPlayer = false;
        behavior = 5;
        instructions(operations, 2); //953C/9541 clear contact and select climb.
    }
    instructions(operations, 7); //9545..9554 behavior dispatch.
    if (behavior == 0) {
        instructions(operations, 2);
        if (actor.behaviorTimer == 0) {
            actor.forwardVelocity = level1LiftCruiseSpeed;
            instructions(operations, 6);
            level1RandomSteeringOperations(actor, rng, operations);
            instructions(operations, 2);
        } else {
            --actor.behaviorTimer;
            instructions(operations, 2);
            if (actor.behaviorTimer == 0) {
                behavior = 1;
                instructions(operations, 2);
            } else {
                instructions(operations, 1);
            }
        }
    } else if (behavior == 1) {
        instructions(operations, 2);
        level1SteeringOperations(actor, level1LiftTurnLimit, operations);
        instructions(operations, 1 + 1);
        const auto choice = static_cast<std::uint8_t>(prefixRandom(rng, operations));
        instructions(operations, 3);
        if (choice < 8) {
            behavior = 2;
            instructions(operations, 2);
        } else {
            instructions(operations, 1);
        }
    } else if (behavior == 2) {
        actor.position.z -= 50;
        instructions(operations, 4);
        if (actor.position.z < 0) {
            instructions(operations, 1 + 2);
            behavior = 3;
        } else {
            instructions(operations, 1 + 2);
            if (actor.position.z < 200) {
                behavior = 3;
                instructions(operations, 2);
            } else {
                instructions(operations, 1);
            }
        }
    } else if (behavior == 3) {
        instructions(operations, 2);
        const auto magnitude = level1SteeringOperations(actor, level1LiftTurnLimit, operations);
        instructions(operations, 3);
        const auto inRange = level1RangeOperations(actor, operations);
        instructions(operations, 3);
        if (!inRange) {
            instructions(operations, 1);
        } else {
            instructions(operations, 2);
            if (actor.weaponSlotTimers[0] == 0) {
                instructions(operations, 7);
                if (static_cast<std::int16_t>(magnitude) < 256) {
                    instructions(operations, 2);
                    const auto launched=level1FireGuardOperations(actor, operations, enemy);
                    instructions(operations, 3);
                    if (launched) {
                        behavior=4;
                        actor.behaviorTimer=63;
                        instructions(operations,3); //960A/960E/9612 successful launch continuation.
                    }
                } else {
                    const auto x = static_cast<std::uint32_t>(player.position.x)
                        - static_cast<std::uint32_t>(actor.position.x);
                    const auto absoluteX = x == 0xffff8000U ? 32767U
                        : (x & 0x80000000U) != 0 ? 0U - x : x;
                    instructions(operations, 2);
                    if (static_cast<std::uint8_t>(absoluteX) < 2) {
                        behavior = 5;
                        instructions(operations, 2);
                    }
                }
            }
        }
    } else if (behavior == 4) {
        instructions(operations, 2);
        if (actor.behaviorTimer == 0) {
            behavior = 5;
        } else {
            --actor.behaviorTimer;
        }
        instructions(operations, 2);
    } else {
        actor.forwardVelocity = level1LiftCruiseSpeed;
        instructions(operations, 4 + 2 + 1 + 2);
        if (actor.position.z <= level1LiftCruiseAltitude) {
            actor.position.z += 100;
            instructions(operations, 4 + 1 + 2);
            if (actor.position.z >= level1LiftCruiseAltitude) {
                actor.position.z = level1LiftCruiseAltitude;
                behavior = 0;
                instructions(operations, 3);
            }
        }
    }
    instructions(operations, 2);
    if (actor.weaponSlotTimers[0] != 0) {
        --actor.weaponSlotTimers[0];
        instructions(operations, 1);
    }
    const auto damping = static_cast<std::int16_t>(0U - static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(actor.yawVelocity) << 4) + actor.roll));
    const auto shifted = damping >= 0 ? damping / 16 : -((-static_cast<int>(damping) + 15) / 16);
    actor.roll = static_cast<std::uint16_t>(actor.roll + shifted);
    instructions(operations, 11);
}

template<class Work, class Reference, class Position>
void level1RebuildOperations(Work& work, const std::vector<Reference>& allocations, Position positionOf) {
    instructions(work.operations, 6); //382D camera copy setup; caller owns refresh/displacement tests.
    work.operations.push_back({NormalWorkKind::repeatString, 6});
    instructions(work.operations, 8); //Initialize count and allocation loop.
    work.activeOrder.clear();
    for (std::size_t position = 0; position < allocations.size(); ++position) {
        const auto index = allocations[position];
        const auto actor = positionOf(index);
        const auto x = detail::level1RebuildAbsolute(static_cast<std::uint32_t>(work.player.position.x)
            - static_cast<std::uint32_t>(actor.x));
        const auto y = detail::level1RebuildAbsolute(static_cast<std::uint32_t>(work.player.position.y)
            - static_cast<std::uint32_t>(actor.y));
        instructions(work.operations, 4 + 13 + 2); //Health, coordinate deltas and X sign.
        instructions(work.operations, x.branchDispatches);
        instructions(work.operations, x.storeDispatches + 5 + 2); //X stores/visibility and Y sign.
        instructions(work.operations, y.branchDispatches);
        instructions(work.operations, y.storeDispatches + 5 + 5); //Y stores/visibility and append pointer.
        instructions(work.operations, position + 1 == allocations.size() ? 4 : 5);
        work.activeOrder.push_back(index);
    }
    instructions(work.operations, 6);
    work.lastListCamera = work.player.position;
    work.refreshRequested = false;
}

template<class Work>
bool level1RefreshOperations(Work& work) {
    instructions(work.operations, 10);
    if (work.refreshRequested) {
        instructions(work.operations, 2);
        return true;
    }
    const auto x = static_cast<std::int64_t>(work.player.position.x) - work.lastListCamera.x;
    const auto y = static_cast<std::int64_t>(work.player.position.y) - work.lastListCamera.y;
    if (x <= -32768 || x >= 32768 || y <= -32768 || y >= 32768) {
        throw std::invalid_argument("Level1 list refresh excludes wide displacement/clamp");
    }
    instructions(work.operations, 14);
    if (x < 0) {
        instructions(work.operations, 2 + 3);
    }
    instructions(work.operations, 2 + 5);
    if (x <= -listRefreshDistance1024 || x >= listRefreshDistance1024) {
        return true;
    }
    instructions(work.operations, y < 0 ? 9 : 5);
    instructions(work.operations, 2 + 5);
    if (y <= -listRefreshDistance1024 || y >= listRefreshDistance1024) {
        return true;
    }
    instructions(work.operations, 1 + 6);
    return false;
}

enum class PrefixCampaignDomain { level1, level2Constructor };

template<class Work, class Position>
void level1SortOperations(Work& work, const bool first, Position positionOf,
    const PrefixCampaignDomain campaign = PrefixCampaignDomain::level1) {
    instructions(work.operations, first ? 9 : 8);
    std::size_t selected = 0; //Source1538 retains this local across zero-maximum passes.
    for (std::size_t destination = 0; destination < work.activeOrder.size(); ++destination) {
        instructions(work.operations, 9);
        std::uint32_t maximum = 0;
        for (std::size_t candidate = destination; candidate < work.activeOrder.size(); ++candidate) {
            instructions(work.operations, 9);
            const auto distance = distanceOperations(work.player.position,
                positionOf(work.activeOrder[candidate]), work.operations);
            //Level1 skips Type46; Level2 executes its failing type comparison.
            instructions(work.operations, 4 + (campaign == PrefixCampaignDomain::level1 ? 1 : 4) + 4);
            const auto maximumHigh = static_cast<std::int16_t>(maximum >> 16);
            const auto distanceHigh = static_cast<std::int16_t>(distance >> 16);
            if (distanceHigh >= maximumHigh) {
                instructions(work.operations, 1);
                if (distanceHigh == maximumHigh) {
                    instructions(work.operations, 2);
                }
            }
            if (distance > maximum) {
                instructions(work.operations, 5);
                maximum = distance;
                selected = candidate;
            }
            instructions(work.operations, 5);
        }
        instructions(work.operations, 3);
        if (selected != destination) {
            instructions(work.operations, 8);
            std::swap(work.activeOrder[selected], work.activeOrder[destination]);
        }
        instructions(work.operations, destination + 1 == work.activeOrder.size() ? 4 : 5);
    }
    instructions(work.operations, 5);
}

enum class Level1ActorStorageValidation { live, retainedCombat, retainedBonus, retainedPickup, retainedArrival, retainedDeployment, retainedReward };

constexpr std::size_t maximumBonusActorCount = 15;
constexpr std::int16_t systemBonusType = 42;

bool validGuardianReward(const Level1Entity& actor) {
    const auto spin=actor.yawVelocity;
    return actor.type==25 && actor.flags==0 && actor.collisionRadius==897
        && actor.stateBytes4dTo52[0]==254 && actor.stateBytes4dTo52[1]>=1 && actor.stateBytes4dTo52[1]<=100
        && actor.stateBytes4dTo52[2]<=3 && actor.stateBytes4dTo52[4]==0
        && (actor.stateBytes4dTo52[5]==1 || actor.stateBytes4dTo52[5]==255)
        && actor.pitch==0 && actor.roll==0 && actor.position.z==0
        && actor.pitchVelocity==0 && actor.rollVelocity==0 && actor.xVelocity==0 && actor.yVelocity==0
        && actor.zVelocity==0 && actor.forwardVelocity==0
        && (spin==2560 || spin==-2816 || spin==3072 || spin==-3328)
        && !actor.animatedModelPresenterEnabled && actor.renderProfile==WorldActorRenderProfile::inlineSelectorNoCallback
        && actor.modelAnimationVariant==actor.stateBytes4dTo52[2];
}

bool validSystemBonus(const Level1Entity& actor) {
    const auto payload = actor.stateBytes4dTo52[2];
    return actor.type == systemBonusType && actor.flags == 0 && actor.collisionRadius == 330
        && actor.stateBytes4dTo52[0] == 254 && actor.stateBytes4dTo52[1] <= 1
        && (payload == 3 || payload == 7) && actor.stateBytes4dTo52[3] == 0
        && actor.stateBytes4dTo52[4] == 0 && actor.stateBytes4dTo52[5] == payload
        && actor.pitch == 0 && actor.roll % 0x0800 == 0 && actor.roll == actor.yaw
        && actor.pitchVelocity == 0 && actor.rollVelocity == 0x0800 && actor.yawVelocity == 0x0800
        && actor.xVelocity == 0 && actor.yVelocity == 0 && actor.zVelocity == 0
        && actor.forwardVelocity == 0 && actor.position.z == 100
        && !actor.primaryType11AnimationSelector && !actor.animatedModelPresenterEnabled
        && actor.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback
        && actor.modelAnimationVariant == payload;
}

std::array<Level1BonusFixedSlot,level1BonusFixedPoolSize> fixedSlotProjection(
    const std::vector<Level1Entity>& entities,const bool reward=false) {
    std::array<Level1BonusFixedSlot,level1BonusFixedPoolSize> slots{};
    constexpr std::size_t resetSlots = 8;
    for (std::size_t slot = 0; slot < resetSlots; ++slot) {
        slots[slot].provenance = Level1BonusFixedProvenance::reset;
        slots[slot].entity.stateBytes4dTo52[0] = 254;
    }
    std::size_t slot = 0;
    for (std::size_t index = 0; index < entities.size(); ++index) {
        const auto& entity = entities[index];
        if (entity.stateBytes4dTo52[0] != 254) { continue; }
        if (slot == slots.size() || (slot < 4 && index != slot + 2)
            || (entity.type != 29 && !validSystemBonus(entity) && !(reward && validGuardianReward(entity)))) {
            throw std::invalid_argument("unsupported recovered fixed-slot identity projection");
        }
        slots[slot++] = {Level1BonusFixedProvenance::retained,index,entity,0};
    }
    if (slot < 4) { throw std::invalid_argument("bonus requires the four initial fixed identities"); }
    return slots;
}

void validateLevel1PrefixInput(const Level1PrefixInput& input,
    const Level1ActorContactDomain contactDomain = Level1ActorContactDomain::none,
    const Level1OrdinaryMembership membership = Level1OrdinaryMembership::initialSix,
    const Level1ActorStorageValidation storage = Level1ActorStorageValidation::live,
    const Level1OrdinaryFamily ordinaryFamily = Level1OrdinaryFamily::type1) {
    const auto dynamic = membership == Level1OrdinaryMembership::periodicSpawned;
    const auto rewardDomain = storage == Level1ActorStorageValidation::retainedReward;
    const auto deploymentDomain = storage == Level1ActorStorageValidation::retainedDeployment || rewardDomain;
    const auto arrivalDomain = storage == Level1ActorStorageValidation::retainedArrival || deploymentDomain;
    const auto pickupDomain = storage == Level1ActorStorageValidation::retainedPickup || arrivalDomain;
    const auto bonusDomain = storage == Level1ActorStorageValidation::retainedBonus || pickupDomain;
    const auto actorCount = input.actors.size();
    const auto first = input.activeOrder.empty();
    const auto& player = input.player;
    if ((ordinaryFamily!=Level1OrdinaryFamily::type1 && !type3Family(ordinaryFamily))
        || (type3Family(ordinaryFamily) && !dynamic)
        || (membership != Level1OrdinaryMembership::initialSix && !dynamic)
        || (dynamic ? actorCount < level1ActorCount || actorCount > (rewardDomain ? maximumBonusActorCount+3 : deploymentDomain ? maximumBonusActorCount+2 : arrivalDomain ? maximumBonusActorCount+1 : bonusDomain ? maximumBonusActorCount : maximumLevel1ActorCount)
            || input.allocationOrder.size() != actorCount || first
            || (input.activeOrder.size() != actorCount
                && (input.activeOrder.size() + 1 != actorCount || !input.refreshRequested))
            : actorCount != level1ActorCount || input.allocationOrder.size() != level1ActorCount
                || (first ? !input.refreshRequested : input.activeOrder.size() != level1ActorCount))
        || input.fadeTicks != 0 || (!arrivalDomain && input.specialActorPresent)
        || (input.impulse != 0 && contactDomain != Level1ActorContactDomain::ordinaryPlayer)
        || input.impulse < -sourceForwardLimit200 || input.impulse > sourceForwardLimit200 || !input.active
        || (!rewardDomain && input.projectileHandlePresent[0]) || input.projectileHandlePresent[1]
        || (contactDomain == Level1ActorContactDomain::ordinaryPlayer && (player.type != 0 || player.collisionRadius != 723))
        || player.position.z != 0 || player.pitch != 0 || player.roll != 0
        || player.pitchVelocity != 0 || player.rollVelocity != 0
        || player.yawVelocity < -512 || player.yawVelocity > 512
        || player.xVelocity != 0 || player.yVelocity != 0 || player.zVelocity != 0
        || player.forwardVelocity < -sourceForwardLimit200 || player.forwardVelocity > sourceForwardLimit200
        || input.rng.indexA() >= StellarRng::wordCount * 2 || input.rng.indexB() >= StellarRng::wordCount * 2
        || input.rng.indexA() % 2 != 0 || input.rng.indexB() % 2 != 0) {
        throw std::invalid_argument("unsupported Level1 prefix profile");
    }
    std::vector<bool> seen(actorCount);
    for (std::size_t position = 0; position < actorCount; ++position) {
        if (input.allocationOrder[position] != position) {
            throw std::invalid_argument("unsupported Level1 allocation identity");
        }
        const auto& actor = input.actors[position];
        if(arrivalDomain && actor.type==7) {
            if(!(deploymentDomain ? isLevel1Type7DeploymentState(actor) : isLevel1Type7ApproachState(actor)) || actor.allocationIndex!=position || actor.collisionContactIsPlayer
                || actor.collisionContactAllocationIndex!=std::numeric_limits<std::size_t>::max()) {
                throw std::invalid_argument("invalid retained arrival identity/contact");
            }
            requireVisiblePrefixPosition(actor.position,player.position);
            continue;
        }
        if(deploymentDomain && actor.type==44) {
            if(actor.allocationIndex!=position || actor.collisionRadius!=2792 || (actor.flags&~4U)!=0
                || actor.stateBytes4dTo52[0]!=255 || (!rewardDomain && !actor.active()) || actor.stateBytes4dTo52[1]>15
                || actor.stateBytes4dTo52[2]<1 || actor.stateBytes4dTo52[2]>2 || actor.stateBytes4dTo52[3]>2
                || actor.stateBytes4dTo52[4]!=5 || actor.stateBytes4dTo52[5]!=0 || actor.modelAnimationVariant>7
                || actor.pitch!=0 || actor.roll!=0 || actor.position.z!=0 || actor.pitchVelocity!=0 || actor.rollVelocity!=0
                || actor.xVelocity!=0 || actor.yVelocity!=0 || actor.zVelocity!=0 || actor.forwardVelocity!=0) {
                throw std::invalid_argument("invalid retained guardian identity/state");
            }
            requireVisiblePrefixPosition(actor.position,player.position);continue;
        }
        if(rewardDomain && actor.type==25) {
            if(actor.allocationIndex!=position || !validGuardianReward(actor)) {
                throw std::invalid_argument("invalid retained Type25 source record");
            }
            requireVisiblePrefixPosition(actor.position,player.position);continue;
        }
        const auto bonus = bonusDomain && actor.type == systemBonusType;
        const auto retainedOrdinary=dynamic && ordinaryStorageIndex(position);
        const auto type = bonus ? systemBonusType : retainedOrdinary ? actor.type
            : position < level1ActorCount ? (position == 0 ? 1 : position == 1 ? 11 : 29) : actor.type;
        if (bonus && (position < 2 || !validSystemBonus(actor))) {
            throw std::invalid_argument("unsupported retained Type42 record");
        }
        if ((position >= level1ActorCount || retainedOrdinary) && type != 1 && type != 11 && !(type3Family(ordinaryFamily) && type==3) && !bonus) {
            throw std::invalid_argument("unsupported spawned ordinary behavior family");
        }
        const auto radius = type == 1 ? 1342U : type==3 ? 770U : type == 11 ? 748U : bonus ? 330U : 862U;
        const auto category = type == 29 || bonus ? 254 : 255;
        const auto callback = type == 1 || type==3 ? 1 : type == 11 ? 3 : 0;
        const auto contact = actor.collisionContactAllocationIndex;
        const auto unsupportedContact = (actor.collisionContactIsPlayer
                && !((pickupDomain && bonus || contactDomain == Level1ActorContactDomain::ordinaryPlayer)
                    && contact == std::numeric_limits<std::size_t>::max()))
            || (contact != std::numeric_limits<std::size_t>::max()
                && (contactDomain != Level1ActorContactDomain::ordinaryActors
                    && contactDomain != Level1ActorContactDomain::ordinaryPlayer
                    || contact >= input.actors.size() || contact == position));
        requireVisiblePrefixPosition(actor.position, player.position);
        if (actor.allocationIndex != position || actor.type != type || actor.collisionRadius != radius
            || (!actor.active() && storage == Level1ActorStorageValidation::live)
            || actor.stateBytes4dTo52[0] != category
            || (type != 29 && actor.stateBytes4dTo52[4] != callback)
            || actor.pitch != 0 || (type != 11 && !bonus && actor.roll != 0)
            || actor.pitchVelocity != 0 || (!bonus && actor.rollVelocity != 0)
            || (!bonus && (actor.yawVelocity < -(type==3 ? type3MaximumTurn : 511)
                || actor.yawVelocity > (type==3 ? type3MaximumTurn : 511)))
            || actor.xVelocity != 0 || actor.yVelocity != 0 || actor.zVelocity != 0
            || (type == 1 && actor.forwardVelocity != -45 && actor.forwardVelocity != 0
                && actor.forwardVelocity != level1TankCruiseSpeed)
            || (type==3 && actor.forwardVelocity!=-type3Tables.reverse && actor.forwardVelocity!=0
                && actor.forwardVelocity!=type3Tables.cruise)
            || (type == 11 && actor.forwardVelocity != level1LiftCruiseSpeed)
            || (type == 29 && (actor.forwardVelocity != 0 || actor.yawVelocity != 0))
            || actor.position.z < 0 || actor.position.z > (type == 11 ? level1LiftCruiseAltitude : bonus ? 100 : 0)
            || unsupportedContact || (!bonus && actor.stateBytes4dTo52[5] > 5)
            || actor.weaponSlotCount > actor.weaponSlotTimers.size()
            || (type==3 && (actor.stateBytes4dTo52[1]>1 || actor.weaponSlotCount!=1
                || actor.behaviorTimer>127 || (actor.flags&~4U)!=0 || actor.animatedModelPresenterEnabled
                || actor.primaryType11AnimationSelector || actor.renderProfile!=WorldActorRenderProfile::noHandles))) {
            throw std::invalid_argument("unsupported Level1 constructor actor");
        }
    }
    for (const auto index : input.activeOrder) {
        if (index >= actorCount || seen[index]) {
            throw std::invalid_argument("invalid Level1 inherited active order");
        }
        seen[index] = true;
    }
}

template<class Actor>
void level1PresenterFieldsOperations(Actor& actor, const Level1Entity& player,
    std::vector<NormalWorkOperation>& operations) {
    requireVisiblePrefixPosition(actor.position, player.position);
    instructions(operations, 8);
    actor.presenterDistance = distanceOperations(player.position, actor.position, operations);
    instructions(operations, 16);
    actor.targetHeading = headingOperations(actor.position, player.position, operations);
    instructions(operations, 4);
}
} // namespace

Level1PrefixWork level1PrefixOperations(const Level1PrefixInput& input) {
    validateLevel1PrefixInput(input);
    const auto first = input.activeOrder.empty();
    const auto& player = input.player;
    Level1PrefixWork result{player, input.actors, input.activeOrder, input.lastListCamera,
        input.refreshRequested, input.rng, {}};
    instructions(result.operations, 7 + 7);
    const auto playerKinematics = flatKinematicsOperations(player);
    result.player = playerKinematics.actor;
    append(result.operations, playerKinematics.operations);
    instructions(result.operations, 4);
    auto storage = result.actors;
    storage.push_back(result.player);
    const auto playerIndex = result.actors.size();
    const auto contact = actorContactOperations(storage, result.activeOrder, playerIndex);
    if (contact.contactIndex != std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("Level1 prefix excludes player contact");
    }
    append(result.operations, contact.operations);
    instructions(result.operations, 9);
    for (std::size_t position = 0; position < result.activeOrder.size(); ++position) {
        const auto index = result.activeOrder[position];
        auto& actor = storage[index];
        instructions(result.operations, 8);
        if (actor.stateBytes4dTo52[0] == 254) {
            instructions(result.operations, 3);
        } else {
            instructions(result.operations, 3);
            const auto kinematics = level1KinematicsOperations(actor);
            actor = kinematics.actor;
            append(result.operations, kinematics.operations);
            instructions(result.operations, 4);
            const auto overlap = actorOverlapOperations(actor, result.player);
            if (overlap.overlaps) {
                throw std::invalid_argument("Level1 prefix excludes actor/player collision");
            }
            append(result.operations, overlap.operations);
            instructions(result.operations, 3 + 4);
            const auto contacts = actorContactOperations(storage, result.activeOrder, index);
            if (contacts.contactIndex != std::numeric_limits<std::size_t>::max()) {
                throw std::invalid_argument("Level1 prefix excludes actor collision");
            }
            append(result.operations, contacts.operations);
            //0A00 bypasses OR/JZ for behavior3 after the common contact cleanup.
            instructions(result.operations, actor.stateBytes4dTo52[4] == 3 ? 4 : 6);
        }
        instructions(result.operations, position + 1 == result.activeOrder.size() ? 5 : 6);
    }
    instructions(result.operations, 2 + 10 + 3 + 10 + 6);
    result.actors.assign(storage.begin(), storage.begin() + playerIndex);
    instructions(result.operations, 8);
    instructions(result.operations, first ? 12 : 13);
    for (const auto index : result.activeOrder) {
        auto& actor = result.actors[index];
        level1PresenterFieldsOperations(actor, result.player, result.operations);
        if (actor.stateBytes4dTo52[0] == 255) {
            instructions(result.operations, 8);
            if (actor.type == 1) {
                level1TankOperations(actor, result.rng, result.operations);
            } else {
                level1LiftOperations(actor, result.player, result.rng, result.operations);
            }
            instructions(result.operations, 1);
        }
        instructions(result.operations, 6);
    }
    instructions(result.operations, 5 + 1);
    level1SortOperations(result, first, [&result](const std::size_t index) { return result.actors[index].position; });
    instructions(result.operations, 10);
    if (level1RefreshOperations(result)) {
        level1RebuildOperations(result, input.allocationOrder,
            [&result](const std::size_t index) { return result.actors[index].position; });
    }
    instructions(result.operations, 5);
    return result;
}

namespace {
constexpr std::int16_t ordinaryPrimaryType = 23;
constexpr std::uint32_t ordinaryPrimaryRadius = 134;
constexpr std::uint8_t ordinaryPrimaryCategory = 253;
constexpr std::uint8_t ordinaryPrimaryLifetime = 64;
constexpr std::int16_t ordinaryPrimarySpeed = 500;

bool freshEnemyRecord(const Level1Projectile& shot) {
    return shot.flags == 0 && shot.type == 0 && shot.collisionRadius == 0
        && shot.position == FixedPosition{} && shot.pitch == 0 && shot.roll == 0 && shot.yaw == 0
        && shot.pitchVelocity == 0 && shot.rollVelocity == 0 && shot.yawVelocity == 0
        && shot.xVelocity == 0 && shot.yVelocity == 0 && shot.zVelocity == 0 && shot.forwardVelocity == 0
        && shot.stateBytes4dTo54 == std::array<std::uint8_t, 8>{ordinaryPrimaryCategory, 0, 0, 0, 0, 0, 0, 0}
        && !shot.ownerIsPlayer && shot.ownerAllocationIndex == std::numeric_limits<std::size_t>::max()
        && shot.presenterDistance == 0 && shot.targetHeading == 0;
}

bool retainedProjectileRecord(const Level1Projectile& shot,
    const std::vector<Level1Entity>* entities = nullptr, const Level1ProjectileAllocationDomain allocationDomain = Level1ProjectileAllocationDomain::freshOnly,
    const Level1OrdinaryFamily family=Level1OrdinaryFamily::type1) {
    const auto roster=allocationDomain==Level1ProjectileAllocationDomain::retainedRoster;
    const auto ownerIdentity=entities && level1RetainedEmitterIdentity(*entities,shot.ownerAllocationIndex);
    const auto primary = shot.type == ordinaryPrimaryType && shot.collisionRadius == ordinaryPrimaryRadius
        && shot.ownerIsPlayer && shot.ownerAllocationIndex == std::numeric_limits<std::size_t>::max();
    const auto hostile = shot.type == 17 && shot.collisionRadius == 441
        && !shot.ownerIsPlayer && (roster ? ownerIdentity : (shot.ownerAllocationIndex == 0
            || (entities && shot.ownerAllocationIndex < entities->size()
                && (*entities)[shot.ownerAllocationIndex].type == 1)));
    const auto airborne=airborneAllocationDomain(allocationDomain) && shot.type==20 && shot.collisionRadius==134 && !shot.ownerIsPlayer
        && entities && shot.ownerAllocationIndex<entities->size() && (roster ? ownerIdentity : airborneOwner((*entities)[shot.ownerAllocationIndex],family));
    const auto type3Owned=!roster && airborne && (*entities)[shot.ownerAllocationIndex].type==3;
    return (primary || hostile || airborne) && (!type3Owned || (shot.position.z==0 && shot.roll==0)) && shot.flags == 0
        && (airborne ? shot.position.z>=0 && shot.position.z<=level1LiftCruiseAltitude : shot.position.z==0)
        && shot.pitch == 0 && (airborne || shot.roll == 0)
        && shot.pitchVelocity == 0 && shot.rollVelocity == 0 && shot.yawVelocity == 0
        && shot.xVelocity == 0 && shot.yVelocity == 0 && shot.zVelocity == 0 && shot.forwardVelocity == ordinaryPrimarySpeed
        && shot.stateBytes4dTo54 == std::array<std::uint8_t, 8>{ordinaryPrimaryCategory, 0, 0, 0, 0, 0, 0, 0};
}

bool level2ReusableProjectile(const Level1Projectile& shot) {
    //48D4 ancestry or a retired admitted FD; 44BA/995E overwrite the other fields.
    const auto knownType = shot.type==0 || shot.type==17 || shot.type==20 || shot.type==22
        || shot.type==ordinaryPrimaryType || shot.type==Level2HostileProjectileModel::type;
    return knownType && (shot.flags&~4U)==0 && shot.stateBytes4dTo54[0]==ordinaryPrimaryCategory
        && shot.stateBytes4dTo54[1]==0 && shot.pitchVelocity==0 && shot.rollVelocity==0;
}

bool enemyLaunchOperations(EnemyFireContext& context, Level1Entity& owner, const std::size_t weaponSlot) {
    auto& work = context.work;
    auto& operations = work.operations;
    const auto periodic = context.level2Periodic != nullptr;
    const auto level2 = context.level2Hostile != nullptr || periodic;
    const auto class1 = periodic && (owner.type==1 || owner.type==4) && owner.stateBytes4dTo52[4]==1;
    const auto class2 = context.level2Hostile && owner.type==8 && owner.stateBytes4dTo52[4]==2;
    if (level2 ? owner.stateBytes4dTo52[0]!=255 || (!class1 && !class2)
        : owner.type != 1 && !(airborneAllocationDomain(context.profile.allocationDomain)
            && airborneOwner(owner,context.profile.ordinaryFamily))) {
        throw std::invalid_argument("enemy launch requires its actual source type/class owner");
    }
    const auto type = class1 ? static_cast<std::int16_t>(owner.type==1 ? 17 : 20)
        : level2 ? Level2HostileProjectileModel::type : static_cast<std::int16_t>(owner.type==1 ? 17 : 20);
    const auto lifetime = type==19 ? Level2HostileProjectileModel::lifetime
        : static_cast<std::uint8_t>(type==17 ? 54 : 60);
    const auto model = class1 ? (type==17 ? context.level2Periodic->model17 : context.level2Periodic->model20)
        : level2 ? context.level2Hostile->model19 : type==17 ? context.profile.model17 : context.profile.model20;
    instructions(operations, 14 + 1); //995E..9986 normal owner, nonzero type;998B CALL44BA.
    instructions(operations, 6);
    auto selected = work.projectiles.size();
    for (std::size_t slot = 0; slot < work.projectiles.size(); ++slot) {
        instructions(operations, 3);
        if (!work.projectiles[slot].active()) {
            selected = slot;
            break;
        }
        instructions(operations, 4);
    }
    if (selected == work.projectiles.size()) {
        instructions(operations, 1 + 5 + 4 + 6); //44D7/44FE return;998E failed result;9A2C return0.
        return false;
    }
    auto& shot = work.projectiles[selected];
    const auto admitted = level2 ? level2ReusableProjectile(shot) : freshEnemyRecord(shot)
        || (context.profile.allocationDomain != Level1ProjectileAllocationDomain::freshOnly && retainedProjectileRecord(shot,
            context.profile.membership == Level1OrdinaryMembership::periodicSpawned
                || airborneAllocationDomain(context.profile.allocationDomain) ? &work.entities : nullptr,
            context.profile.allocationDomain,context.profile.ordinaryFamily));
    if (!admitted
        || std::find(work.allocationOrder.begin(), work.allocationOrder.end(),
            Level1ObjectRef{Level1PresentationKind::projectile, selected}) != work.allocationOrder.end()) {
        throw std::invalid_argument("enemy fire selected a reused projectile record");
    }
    const auto registryCapacity = periodic ? context.level2Periodic->firstRegistryCapacity
        : level2 ? context.level2Hostile->firstRegistryCapacity : context.profile.firstRegistryCapacity;
    if (registryCapacity <= type || model.scaleShift != 0
        || model.baseRadius != (type == 17 || type==19 ? 441 : 134)) {
        throw std::invalid_argument("unsupported enemy projectile model lookup");
    }
    instructions(operations, 12); //44DA..44FC partial writes,1296 and49F8 append.
    shot.stateBytes4dTo54[4] = 0;
    shot.zVelocity = 0;
    work.allocationOrder.push_back({Level1PresentationKind::projectile, selected});
    work.refreshRequested = true;
    instructions(operations, 3 + 9); //998E..9993 and9998..99AC position setup.
    operations.push_back({NormalWorkKind::repeatString, 6});
    shot.stateBytes4dTo54[2] = 0;
    shot.position = owner.position;
    instructions(operations, 4);
    for (std::size_t angle = 0; angle < 3; ++angle) {
        operations.push_back({NormalWorkKind::string, 1});
    }
    shot.pitch = owner.pitch;
    shot.roll = owner.roll;
    shot.yaw = owner.yaw;
    shot.type = type;
    instructions(operations, 5 + 6 + 27 + 15); //99BC..99C4 and residentD03C/C05C lookup.
    instructions(operations, 8 + 14); //99C9..9A0A, normal callback1/3 owner path.
    shot.collisionRadius = model.baseRadius;
    shot.forwardVelocity = ordinaryPrimarySpeed;
    shot.ownerIsPlayer = false;
    shot.ownerAllocationIndex = owner.allocationIndex;
    shot.stateBytes4dTo54[1] = lifetime;
    owner.behaviorTimer = 1;
    owner.weaponCooldown = type==19 ? Level2HostileProjectileModel::cooldown : owner.type==1 ? 8 : 0;
    owner.weaponSlotTimers[weaponSlot] = lifetime;
    const auto event = static_cast<std::uint16_t>(type < 20 ? 2 : 15);
    instructions(operations, type < 20 ? 3 : 4); //9A0D..9A18 event selection.
    operations.push_back({level2 ? (type==20 ? NormalWorkKind::gameplayLevel2EnemyType20Event
        : NormalWorkKind::gameplayLevel2EnemyFireEvent) : NormalWorkKind::gameplayEnemyFireEvent, 1, event});
    context.events.push_back({context.profile.loopTick, event});
    instructions(operations, 6); //9A22..9A2A result and epilogue.
    return true;
}

void type4PlayerDamageOperations(EnemyFireContext& enemy) {
    auto& damage=*enemy.damage;
    auto child=level2Type4DamageOperations(damage.armor,damage.domain);
    const auto offset=enemy.work.operations.size();
    append(enemy.work.operations,child.operations);
    child.palette.operationOffset+=offset;
    damage.paletteSteps.push_back(child.palette);
    damage.armor=child.armor;damage.fadeTicks=child.fadeTicks;
    enemy.events.push_back({enemy.profile.loopTick,6});
}

CollisionBody mixedBody(const Level1ProjectilePrefixWork& work, const Level1ObjectRef reference,
    const TargetPrefixContext* target = nullptr) {
    if (reference.kind == Level1PresentationKind::deathEffect) {
        if (!target) { throw std::invalid_argument("FC identity requires target prefix context"); }
        return collisionBody(target->result.effects.at(reference.index).entity);
    }
    if (reference.kind == Level1PresentationKind::entity) {
        return collisionBody(work.entities[reference.index]);
    }
    return collisionBody(work.projectiles[reference.index]);
}

std::uint8_t mixedCategory(const Level1ProjectilePrefixWork& work, const Level1ObjectRef reference,
    const TargetPrefixContext* target = nullptr) {
    if (reference.kind == Level1PresentationKind::deathEffect) {
        if (!target) { throw std::invalid_argument("FC identity requires target prefix context"); }
        return target->result.effects.at(reference.index).entity.stateBytes4dTo52[0];
    }
    return reference.kind == Level1PresentationKind::entity
        ? work.entities[reference.index].stateBytes4dTo52[0]
        : work.projectiles[reference.index].stateBytes4dTo54[0];
}

std::optional<Level1ObjectRef> mixedContactOperations(Level1ProjectilePrefixWork& work,
    const CollisionBody subject, const std::optional<Level1ObjectRef> subjectReference,
    const Level1ActorContactDomain contactDomain = Level1ActorContactDomain::none,
    TargetPrefixContext* target = nullptr,
    const Level1OrdinaryFamily ordinaryFamily=Level1OrdinaryFamily::type1) {
    std::optional<Level1ObjectRef> selected;
    instructions(work.operations, 9);
    if (!work.activeOrder.empty()) {
        instructions(work.operations, 2);
    }
    for (const auto reference : work.activeOrder) {
        instructions(work.operations, 4);
        if (!subjectReference || reference != *subjectReference) {
            instructions(work.operations, 2);
            if (mixedCategory(work, reference, target) != 253) {
                instructions(work.operations, 2);
                if (mixedCategory(work, reference, target) != 252) {
                    instructions(work.operations, 3);
                    const auto overlap = overlapOperations(mixedBody(work, reference, target), subject);
                    append(work.operations, overlap.operations);
                    instructions(work.operations, 3);
                    if (overlap.overlaps) {
                        const auto pickupContact = !subjectReference && target && target->pickup
                            && reference.kind == Level1PresentationKind::entity
                            && (work.entities[reference.index].type == systemBonusType
                                || (target->deployment && target->deployment->reward && work.entities[reference.index].type==25));
                        const auto ordinaryPlayer=contactDomain==Level1ActorContactDomain::ordinaryPlayer;
                        const auto periodic = target && target->targetDomain==Level1TargetHitDomain::level2PeriodicOrdinary;
                        if (periodic && reference.kind==Level1PresentationKind::entity
                            && (!subjectReference || subjectReference->kind==Level1PresentationKind::entity)) {
                            auto& actor=work.entities[reference.index];
                            actor.collisionContactAllocationIndex=subjectReference ? subjectReference->index : std::numeric_limits<std::size_t>::max();
                            actor.collisionContactIsPlayer=!subjectReference;
                            work.contacts.push_back({Level1ContactStep::scanWrite,0x0c4c,0x0c52,
                                work.operations.size(),work.operations.size()+1,
                                subjectReference ? std::optional<std::size_t>{subjectReference->index} : std::nullopt,reference.index});
                        } else if (ordinaryPlayer && reference.kind==Level1PresentationKind::entity
                            && ordinarySolidTarget(work.entities[reference.index],ordinaryFamily)
                            && (!subjectReference || (subjectReference->kind==Level1PresentationKind::entity
                                && ordinarySolidTarget(work.entities[subjectReference->index],ordinaryFamily)))) {
                            auto& actor=work.entities[reference.index];
                            actor.collisionContactAllocationIndex=subjectReference ? subjectReference->index : std::numeric_limits<std::size_t>::max();
                            actor.collisionContactIsPlayer=!subjectReference;
                            work.contacts.push_back({Level1ContactStep::scanWrite,0x0c4c,0x0c52,
                                work.operations.size(),work.operations.size()+1,
                                subjectReference ? std::optional<std::size_t>{subjectReference->index} : std::nullopt,reference.index});
                        } else if (pickupContact) {
                            auto& actor = work.entities[reference.index];
                            actor.collisionContactAllocationIndex = std::numeric_limits<std::size_t>::max();
                            actor.collisionContactIsPlayer = true;
                        } else {
                            const auto level2 = target && (target->targetDomain == Level1TargetHitDomain::level2Constructor
                                || target->targetDomain == Level1TargetHitDomain::level2PeriodicOrdinary);
                            if (contactDomain != Level1ActorContactDomain::ordinaryActors || !subjectReference
                                || subjectReference->kind != Level1PresentationKind::entity
                                || reference.kind != Level1PresentationKind::entity
                                || (level2 && (work.entities[subjectReference->index].type != 13
                                    || work.entities[reference.index].type != 30 || selected))) {
                                throw std::invalid_argument("mixed Level1 prefix reached contact at0C4C");
                            }
                            auto& actor = work.entities[reference.index];
                            actor.collisionContactAllocationIndex = subjectReference->index;
                            actor.collisionContactIsPlayer = false;
                        }
                        selected = reference;
                        instructions(work.operations, 2); //0C4C target+5F and0C4F last target.
                    }
                }
            }
        }
        instructions(work.operations, 5);
    }
    instructions(work.operations, 6);
    return selected;
}

std::optional<Level1ObjectRef> primaryTargetOperations(Level1ProjectilePrefixWork& work,
    const Level1Projectile& shot, TargetPrefixContext* target = nullptr) {
    instructions(work.operations, 8); //072A..0738.
    if (!work.activeOrder.empty()) {
        instructions(work.operations, 1); //073A old-list pointer.
        for (const auto reference : work.activeOrder) {
            instructions(work.operations, 4); //073F..0748, FC test precedes FD.
            if (mixedCategory(work, reference, target) != 252) {
                instructions(work.operations, 2);
                if (mixedCategory(work, reference, target) != ordinaryPrimaryCategory) {
                    instructions(work.operations, 3);
                    const auto overlap = overlapOperations(collisionBody(shot), mixedBody(work, reference, target));
                    append(work.operations, overlap.operations);
                    instructions(work.operations, 3);
                    if (overlap.overlaps) {
                        instructions(work.operations, 3); //075E MOV/0761 CMP/0764 JNZ: ignore the owner.
                        if (shot.ownerIsPlayer || reference.kind != Level1PresentationKind::entity
                            || work.entities[reference.index].allocationIndex != shot.ownerAllocationIndex) {
                            if (!target) { throw std::invalid_argument("projectile reached a non-owner target at075E"); }
                            instructions(work.operations, 6); //0774..077B first non-owner result.
                            return reference;
                        }
                    }
                }
            }
            instructions(work.operations, 4); //0766..076F next ref and list bound.
        }
        instructions(work.operations, 1); //0771 JMP zero result.
    }
    instructions(work.operations, 6); //077C..0783.
    return {};
}

void removeAllocationOperations(Level1ProjectilePrefixWork& work,const Level1ObjectRef reference) {
    instructions(work.operations, 6 + 2); //481A..4829, nonempty allocation list.
    auto found = work.allocationOrder.size();
    for (std::size_t index = 0; index < work.allocationOrder.size(); ++index) {
        instructions(work.operations, 2); //482C/482E.
        if (work.allocationOrder[index] == reference) {
            found = index;
            break;
        }
        instructions(work.operations, 5); //4830..4837.
    }
    if (found == work.allocationOrder.size()) {
        throw std::invalid_argument("retired object is absent from49F8");
    }
    instructions(work.operations, 3); //483E..4846.
    if (found + 1 != work.allocationOrder.size()) {
        work.allocationOrder[found] = work.allocationOrder.back();
        instructions(work.operations, 5); //4848..4854 swap last ref into the removed slot.
    }
    work.allocationOrder.pop_back();
    work.refreshRequested = true;
    instructions(work.operations, 1 + 4); //4858 refresh;485D..4861 return.
}

void retireProjectileOperations(Level1ProjectilePrefixWork& work,const std::size_t slot) {
    removeAllocationOperations(work,{Level1PresentationKind::projectile,slot});
}

void guardianDeathOperations(EnemyFireContext& enemy,Level1Projectile& shot,const Level1ObjectRef reference,
    const Level1TargetHitInput& input) {
    auto& work=enemy.work;auto& context=*enemy.target;auto& result=context.result;
    auto& output=*context.deploymentOutput;
    std::vector<Level1RewardObject> inherited;
    for(const auto ref:work.activeOrder) {
        Level1Entity actor{};
        if(ref.kind==Level1PresentationKind::entity) {actor=work.entities.at(ref.index);} else {
            const auto body=mixedBody(work,ref,&context);actor.type=body.type;actor.position=body.position;
            actor.collisionRadius=body.collisionRadius;actor.stateBytes4dTo52[0]=mixedCategory(work,ref,&context);
        }
        inherited.push_back({ref,actor});
    }
    auto child=level1GuardianDeathOperations({input,{input.target,output.rewardEntityIndex,
        fixedSlotProjection(work.entities,true),work.entities.size(),work.allocationOrder,inherited,work.rng,
        context.firstBlockCapacity},work.player.position,result.score,context.difficulty},
        context.deployment->models.at(25),context.scoreFont,context.pages);
    instructions(work.operations,8+3+14); //052A..054A;AC9C wrapper before1176.
    const auto offset=work.operations.size();
    for(const auto& step:child.steps) {
        if(const auto* ops=std::get_if<std::vector<NormalWorkOperation>>(&step)) {append(work.operations,*ops);}
        else if(const auto* audio=std::get_if<Level1GuardianDeathAudio>(&step)) {
            output.children.push_back({work.operations.size(),*audio});enemy.events.push_back({enemy.profile.loopTick,audio->event});
        } else {result.scoreSteps.push_back({work.operations.size(),std::get<ScoreWork>(step)});}
    }
    work.entities.at(reference.index).stateBytes4dTo52[1]=0;
    result.bonus=child.hit.bonus;result.activeEnemies=child.hit.activeEnemies;result.score=child.score;
    if(child.hit.effect) {
        const auto& write=*child.hit.effect;auto& effect=result.effects.at(write.poolSlot);effect.listed=true;
        effect.entity.type=write.type;effect.entity.position=write.position;effect.entity.stateBytes4dTo52[0]=write.category;
        effect.entity.stateBytes4dTo52[1]=write.age;effect.entity.stateBytes4dTo52[4]=write.endAge;
    }
    if(child.reward.birth) {
        const auto& birth=*child.reward.birth;
        if(birth.entityIndex>work.entities.size() || (birth.materializeEntity && birth.poolSlot>=8)) {
            throw std::invalid_argument("reward materialization lacks known fixed reset ancestry");
        }
        if(birth.entityIndex==work.entities.size()) {work.entities.push_back(birth.entity);} else {work.entities.at(birth.entityIndex)=birth.entity;}
        output.rewardBirths.push_back(birth);output.rewardEntityIndex=birth.entityIndex;
    }
    for(const auto& write:child.reward.contactWrites) {
        if(write.candidate.kind!=Level1PresentationKind::entity) {throw std::logic_error("0C0A wrote filteredFD/FC contact");}
        auto& candidate=work.entities.at(write.candidate.index);
        candidate.collisionContactAllocationIndex=write.rewardEntityIndex;candidate.collisionContactIsPlayer=false;
    }
    work.activeOrder.clear();for(const auto& object:child.inherited) {work.activeOrder.push_back(object.identity);}
    work.allocationOrder=child.reward.allocationOrder;work.rng=child.reward.rng;work.refreshRequested=true;
    instructions(work.operations,6+2+1);shot.stateBytes4dTo54[1]=0;
    result.targetHits.push_back({shot.poolSlot,reference.index,offset,std::move(child.hit)});
}

void primaryHitOperations(EnemyFireContext& enemy, Level1Projectile& shot, const Level1ObjectRef hit) {
    auto& work = enemy.work;
    auto& context = *enemy.target;
    auto& result = context.result;
    if (hit.kind != Level1PresentationKind::entity) {
        throw std::invalid_argument("primary target is outside ordinary entity storage");
    }
    auto& actor = work.entities.at(hit.index);
    if(context.deployment && ((actor.stateBytes4dTo52[4]==5 && actor.type==44)
        || (context.deployment->reward && actor.type==25))) {
        const auto damage=static_cast<std::uint8_t>(actor.type==25 || shot.stateBytes4dTo54[2]<1?1:5);
        std::array<std::uint8_t,Level1State::deathEffectPoolSize> ages{};
        for(std::size_t i=0;i<ages.size();++i) {ages[i]=result.effects[i].entity.stateBytes4dTo52[1];}
        if(context.deployment->reward && actor.type==44 && actor.stateBytes4dTo52[1]<=damage) {
            guardianDeathOperations(enemy,shot,hit,{actor,hit.index,damage,result.bonus,result.activeEnemies,
                work.allocationOrder,ages,work.refreshRequested,false,false});return;
        }
        const Level1TargetHitInput hitInput{actor,hit.index,damage,result.bonus,result.activeEnemies,
            work.allocationOrder,ages,work.refreshRequested,false,false};
        auto child=actor.type==25 ? level1RewardHitOperations(hitInput)
            : level1TargetHitOperations(hitInput,Level1TargetHitDomain::guardianNonlethal);
        instructions(work.operations,actor.type==25?8+7:8+3+14); //Ordinary0552 or guardianAC9C caller.
        const auto offset=work.operations.size();
        for(const auto& step:child.steps) {
            append(work.operations,step.operations);
            if(step.audio) {work.operations.push_back({NormalWorkKind::gameplayTargetHitEvent,1,5});enemy.events.push_back({enemy.profile.loopTick,5});}
        }
        actor.stateBytes4dTo52[1]=child.targetHealth;
        if(child.effect) {
            const auto& write=*child.effect;auto& effect=result.effects[write.poolSlot];
            effect.listed=true;effect.entity.type=write.type;effect.entity.position=write.position;
            effect.entity.stateBytes4dTo52[0]=write.category;effect.entity.stateBytes4dTo52[1]=write.age;
            effect.entity.stateBytes4dTo52[4]=write.endAge;work.allocationOrder.push_back({Level1PresentationKind::deathEffect,write.poolSlot});
        }
        work.refreshRequested=child.refreshRequested;instructions(work.operations,actor.type==25?4:6+2+1);
        shot.stateBytes4dTo54[1]=0;result.targetHits.push_back({shot.poolSlot,hit.index,offset,std::move(child)});return;
    }
    if (actor.type == 46 || actor.stateBytes4dTo52[4] == 5) {
        throw std::invalid_argument("unsupported primary target special/class5 path");
    }
    instructions(work.operations, 8 + 7); //052A..0546;0552..055F ordinary damage1 CALL.
    std::array<std::uint8_t,Level1State::deathEffectPoolSize> ages{};
    for (std::size_t slot = 0; slot < ages.size(); ++slot) {
        ages[slot] = result.effects[slot].entity.stateBytes4dTo52[1];
    }
    const Level1TargetHitInput hitInput{actor,hit.index,1,result.bonus,result.activeEnemies,
        work.allocationOrder,ages,work.refreshRequested,false,false};
    Level1BonusTargetHitWork planned{};
    if (context.bonusModel && actor.type!=3) {
        const auto slots = fixedSlotProjection(work.entities,context.deployment && context.deployment->reward);
        planned = level1BonusTargetHitOperations(hitInput,
            {slots,work.entities.size(),context.firstBlockCapacity,*context.bonusModel});
    } else {
        planned.hit = level1TargetHitOperations(hitInput,context.targetDomain);
    }
    auto child = std::move(planned.hit);
    const auto offset = work.operations.size();
    for (const auto& step : child.steps) {
        append(work.operations,step.operations);
        if (step.audio) {
            const auto event = static_cast<std::uint16_t>(step.audio->sound);
            const auto marker = (context.targetDomain == Level1TargetHitDomain::level2Constructor
                || context.targetDomain == Level1TargetHitDomain::level2PeriodicOrdinary)
                ? NormalWorkKind::gameplayLevel2TargetHitEvent : NormalWorkKind::gameplayTargetHitEvent;
            work.operations.push_back({marker,1,event});
            enemy.events.push_back({enemy.profile.loopTick,event});
        }
    }
    actor.stateBytes4dTo52[1] = child.targetHealth;
    if (planned.bonus && planned.bonus->birth) {
        const auto& birth = *planned.bonus->birth;
        const auto index = birth.entityIndex;
        if (birth.appendedAllocationIndex != work.allocationOrder.size()
            || index > work.entities.size() || !context.bonusBirths) {
            throw std::logic_error("bonus receipt lost its canonical append identity");
        }
        const auto otherBonus = std::find_if(work.entities.begin(),work.entities.end(),
            [index](const auto& value) { return value.type == systemBonusType && value.allocationIndex != index; });
        if (otherBonus != work.entities.end()) {
            throw std::invalid_argument("bonus prefix admits one Type42 storage identity");
        }
        std::optional<Level1BonusResetProjection> reset;
        if (birth.materializeEntity) {
            if (index != work.entities.size() || birth.poolSlot >= 8
                || work.entities.size() == maximumBonusActorCount) {
                throw std::invalid_argument("fresh bonus requires known first-eight reset storage");
            }
            work.entities.push_back(birth.entity);
            reset = Level1BonusResetProjection{index,birth.poolSlot};
        } else {
            work.entities.at(index) = birth.entity;
        }
        work.allocationOrder = planned.bonus->allocationOrder;
        context.bonusBirths->push_back({offset+planned.bonusOperationOffset,birth,reset});
    }
    result.bonus = child.bonus;
    result.activeEnemies = child.activeEnemies;
    if (child.effect) {
        const auto& write = *child.effect;
        auto& effect = result.effects.at(write.poolSlot);
        if (work.allocationOrder.size() != write.appendedAllocationIndex) {
            throw std::logic_error("target FC append receipt lost allocation order");
        }
        effect.listed = true;
        effect.entity.type = write.type;
        effect.entity.position = write.position;
        effect.entity.stateBytes4dTo52[0] = write.category;
        effect.entity.stateBytes4dTo52[1] = write.age;
        effect.entity.stateBytes4dTo52[4] = write.endAge;
        work.allocationOrder.push_back({Level1PresentationKind::deathEffect,write.poolSlot});
    }
    if (child.removal) {
        const auto& removal = *child.removal;
        if (work.allocationOrder.at(removal.removedIndex) != hit
            || work.allocationOrder.size() != removal.lastIndex+1
            || work.allocationOrder.back() != removal.replacement) {
            throw std::logic_error("target swap-last receipt lost allocation identity");
        }
        work.allocationOrder[removal.removedIndex] = work.allocationOrder.back();
        work.allocationOrder.pop_back();
    }
    if (work.allocationOrder.size() != child.allocationCount) {
        throw std::logic_error("target child allocation receipt count mismatch");
    }
    work.refreshRequested = child.refreshRequested;
    instructions(work.operations, 3); //0562..0567:AX result test.
    if (child.destroyed) {
        instructions(work.operations, 2); //0569/056D type score selection.
        const auto targetType = work.entities.at(hit.index).type;
        const auto periodic = context.targetDomain == Level1TargetHitDomain::level2PeriodicOrdinary;
        const auto level2 = (context.targetDomain == Level1TargetHitDomain::level2Constructor
                || context.targetDomain == Level1TargetHitDomain::level2PeriodicOrdinary);
        if (targetType == 1 || (periodic && targetType == 4) || (context.targetDomain==Level1TargetHitDomain::periodicClass1 && targetType==3)
            || ((context.targetDomain == Level1TargetHitDomain::initialOrdinary
                || context.targetDomain==Level1TargetHitDomain::periodicClass1) && targetType == 11)
            || (level2 && (targetType == 8 || targetType == 13))) {
            instructions(work.operations, 4+1); //056F..0578 and0584 CALL7970.
            constexpr std::int16_t tankAward = 20;
            constexpr std::int16_t liftAward = 50;
            constexpr std::int16_t level2Type4Award = 30;
            constexpr std::int16_t level2TankAward = 25;
            constexpr std::int16_t level2LiftAward = 10;
            const auto award = periodic && targetType == 1 ? tankAward
                : periodic && targetType == 4 ? level2Type4Award
                : level2 ? (targetType == 8 ? level2TankAward : level2LiftAward)
                : targetType == 1 || targetType==3 ? tankAward : liftAward;
            const auto scoreDomain = periodic ? ScoreAwardDomain::level2PeriodicOrdinary
                : level2 ? ScoreAwardDomain::level2Constructor
                : context.targetDomain == Level1TargetHitDomain::initialOrdinary
                    || context.targetDomain==Level1TargetHitDomain::periodicClass1
                ? ScoreAwardDomain::initialOrdinary : ScoreAwardDomain::ordinaryTank;
            auto score = scoreOperations(result.score,award,context.difficulty,context.scoreFont,context.pages,scoreDomain);
            result.score = score.score;
            result.scoreSteps.push_back({work.operations.size(),std::move(score)});
            instructions(work.operations, 2); //0587 cleanup/058A JMP05BD.
        } else {
            instructions(work.operations, 2); //057A/057E Type29 skips7970.
        }
    }
    instructions(work.operations, 1); //05BD clears primary lifetime; stale record remains.
    shot.stateBytes4dTo54[1] = 0;
    result.targetHits.push_back({shot.poolSlot,hit.index,offset,std::move(child)});
}

void playerHitOperations(EnemyFireContext& enemy,const std::int16_t armorDamage=40) {
    auto& operations = enemy.work.operations;
    auto& damage = *enemy.damage;
    const auto tankShotArmorDamage=armorDamage;
    if (damage.domain == Level1PlayerCombatDomain::survivingOnly && damage.armor < tankShotArmorDamage) {
        throw std::invalid_argument("hostile hit would reach lethal player armor");
    }
    instructions(operations, 2); //0E7C PUSH BP/0E7D MOV BP,SP.
    const auto level2=enemy.target && (enemy.target->targetDomain==Level1TargetHitDomain::level2Constructor
        || enemy.target->targetDomain==Level1TargetHitDomain::level2PeriodicOrdinary);
    operations.push_back({level2 ? NormalWorkKind::gameplayLevel2TargetHitEvent : NormalWorkKind::gameplayPlayerHitEvent, 1, 6});
    enemy.events.push_back({enemy.profile.loopTick, 6});
    instructions(operations, 2 + 3 + 2 + 3); //0E88..0EAD ordinary positive damage/no shield.
    damage.armor = static_cast<std::int16_t>(damage.armor - tankShotArmorDamage);
    instructions(operations, 2 + 1 + 8); //0EB1..0ECB VGA palette flag,132C=8,args/CALL.
    damage.fadeTicks = 8;
    instructions(operations, 11); //Resident1B38..1B50 including loaded-leaf CALLF.
    damage.paletteSteps.push_back({operations.size(), {128,128,238,15}});
    instructions(operations, 3 + 3); //Resident1B54..1B57;0ED0..0ED3.
}

void guardianGroundEffectOperations(EnemyFireContext& enemy,const Level1Projectile& shot) {
    auto& work=enemy.work;auto& result=enemy.target->result;
    std::array<std::uint8_t,8> ages{};
    for(std::size_t i=0;i<ages.size();++i) {ages[i]=result.effects[i].entity.stateBytes4dTo52[1];}
    auto child=detail::effectConstructorOperations(ages,shot.position,0,12,work.allocationOrder.size());
    if(child.effect) {
        auto& effect=result.effects[child.effect->poolSlot];
        if(work.allocationOrder.size()>=32 || effect.listed) {throw std::invalid_argument("guardian ground FC lacks append capacity/identity");}
        effect.listed=true;effect.entity.type=-1;effect.entity.position=shot.position;
        effect.entity.stateBytes4dTo52[0]=252;effect.entity.stateBytes4dTo52[1]=1;effect.entity.stateBytes4dTo52[4]=12;
        work.allocationOrder.push_back({Level1PresentationKind::deathEffect,child.effect->poolSlot});work.refreshRequested=true;
    }
    append(work.operations,child.operations);
}

void primaryUpdateOperations(Level1ProjectilePrefixWork& work, Level1Projectile& shot,
    EnemyFireContext* enemy) {
    instructions(work.operations, 7); //04D8..04E4 active guard.
    --shot.stateBytes4dTo54[1];
    instructions(work.operations, 3); //04E9..04F0.
    if(shot.type==22) {
        const auto high=static_cast<std::int16_t>(static_cast<std::uint32_t>(shot.position.z)>>16U);
        if(high<=-1) {
            instructions(work.operations,1);
            bool below=high<-1;
            if(high==-1) {instructions(work.operations,2);below=(static_cast<std::uint32_t>(shot.position.z)&65535U)<0xfed4;}
            if(below) {
                if(!enemy || !enemy->target || !enemy->target->deployment) {throw std::invalid_argument("guardian ground impact requires deployment context");}
                instructions(work.operations,6);guardianGroundEffectOperations(*enemy,shot);instructions(work.operations,2);shot.stateBytes4dTo54[1]=0;
            }
        }
    }
    instructions(work.operations, 2); //050D/050E kinematics CALL.
    //A temporary kinematic argument has no object identity or list membership.
    Level1Entity motion{};
    motion.position = shot.position;
    motion.pitch = shot.pitch;
    motion.roll = shot.roll;
    motion.yaw = shot.yaw;
    motion.forwardVelocity = shot.forwardVelocity;
    motion.pitchVelocity=shot.pitchVelocity;motion.rollVelocity=shot.rollVelocity;motion.yawVelocity=shot.yawVelocity;
    motion.xVelocity=shot.xVelocity;motion.yVelocity=shot.yVelocity;motion.zVelocity=shot.zVelocity;
    const auto kinematics = shot.type==22 ? failureKinematicsOperations(motion) : level1KinematicsOperations(motion);
    shot.position = kinematics.actor.position;
    shot.yaw = kinematics.actor.yaw;
    shot.pitch=kinematics.actor.pitch;shot.roll=kinematics.actor.roll;
    append(work.operations, kinematics.operations);
    instructions(work.operations, 3); //0511..0518.
    if (shot.type == ordinaryPrimaryType) {
        instructions(work.operations, 2); //051A/051B.
        const auto hit = primaryTargetOperations(work, shot, enemy ? enemy->target : nullptr);
        if (hit) {
            instructions(work.operations, 4); //051E..0525 hit skips0527.
            if(enemy && enemy->target && enemy->target->deployment && hit->kind==Level1PresentationKind::entity
                && enemy->target->deployment->craftEntityIndex==std::optional<std::size_t>{hit->index}) {
                instructions(work.operations,6); //052A/052E,0533/0537,053C/0540 skip craft.
            } else {primaryHitOperations(*enemy,shot,*hit);}
        } else {
            instructions(work.operations, 5); //051E..0527.
        }
    } else {
        instructions(work.operations, 3); //058C..0591 player overlap CALL.
        const auto overlap = overlapOperations(collisionBody(shot), collisionBody(work.player));
        append(work.operations, overlap.operations);
        instructions(work.operations, 3); //0594..0599.
        if (overlap.overlaps) {
            if (!enemy || !enemy->damage) {
                throw std::invalid_argument("hostile projectile reached the player at059B");
            }
            shot.stateBytes4dTo54[1] = 0;
            instructions(work.operations, 6); //059B..05A9: clear life,load damage byte,push/CALL.
            playerHitOperations(*enemy,shot.type==Level2HostileProjectileModel::type
                ? Level2HostileProjectileModel::armorDamage : shot.type==22 ? 104 : shot.type==20 ? 56 : 40);
            instructions(work.operations, 2); //05AC cleanup/05AF JMP5C1.
        } else {
            instructions(work.operations, 2); //05B2/05B3 no-player-hit target scan.
            const auto hit = primaryTargetOperations(work, shot, enemy ? enemy->target : nullptr);
            instructions(work.operations, 3); //05B6..05BB.
            if (hit) {
                shot.stateBytes4dTo54[1] = 0;
                instructions(work.operations, 1); //05BD hostile non-player impact has no1176.
            }
        }
    }
    instructions(work.operations, 2); //05C1/05C5.
    if (!shot.active()) {
        instructions(work.operations, 2); //05C7 PUSH/05C8 CALL481A.
        retireProjectileOperations(work, shot.poolSlot);
        instructions(work.operations, 1); //05CB cleanup.
    }
    instructions(work.operations, 5); //05CE..05D3.
}

void validateDynamicProjectilePrefix(const Level1ProjectilePrefixInput& input,
    const bool enemy, const bool damage, const Level1ProjectileAllocationDomain allocationDomain,
    const Level1ActorContactDomain contactDomain, const Level1PlayerCombatDomain combatDomain,
    const Level1OrdinaryFamily ordinaryFamily) {
    const auto roster=allocationDomain==Level1ProjectileAllocationDomain::retainedRoster;
    if (!enemy || input.entities.size() < level1ActorCount || input.entities.size() > maximumLevel1ActorCount
        || input.allocationOrder.size() > input.entities.size() + input.projectiles.size()
        || input.activeOrder.empty() || input.activeOrder.size() > input.allocationOrder.size()) {
        throw std::invalid_argument("unsupported dynamic Level1 mixed membership");
    }
    std::vector<bool> allocatedEntities(input.entities.size()), listedEntities(input.entities.size());
    std::array<bool, Level1State::projectilePoolSize> allocatedShots{}, listedShots{};
    std::vector<std::size_t> entityAllocationOrder, entityActiveOrder;
    for (const auto ref : input.allocationOrder) {
        if (ref.kind == Level1PresentationKind::entity && ref.index < allocatedEntities.size() && !allocatedEntities[ref.index]) {
            allocatedEntities[ref.index] = true; entityAllocationOrder.push_back(ref.index);
        } else if (ref.kind == Level1PresentationKind::projectile && ref.index < allocatedShots.size()
            && input.projectiles[ref.index].active() && !allocatedShots[ref.index]) {
            allocatedShots[ref.index] = true;
        } else {
            throw std::invalid_argument("invalid dynamic allocation identity");
        }
    }
    if (std::find(allocatedEntities.begin(), allocatedEntities.end(), false) != allocatedEntities.end()) {
        throw std::invalid_argument("dynamic allocation list omits a live ordinary entity");
    }
    for (const auto ref : input.activeOrder) {
        if (ref.kind == Level1PresentationKind::entity && ref.index < listedEntities.size()
            && allocatedEntities[ref.index] && !listedEntities[ref.index]) {
            listedEntities[ref.index] = true; entityActiveOrder.push_back(ref.index);
        } else if (ref.kind == Level1PresentationKind::projectile && ref.index < listedShots.size()
            && allocatedShots[ref.index] && !listedShots[ref.index]) {
            listedShots[ref.index] = true;
        } else {
            throw std::invalid_argument("invalid dynamic active identity");
        }
    }
    //032F/9ED4 primary append precedes033C/0DDC ordinary birth. Neither
    //new record enters the old45D2 traversal before the late3768 rebuild.
    std::vector<Level1ObjectRef> pending;
    for (const auto ref : input.allocationOrder) {
        const auto listed = ref.kind == Level1PresentationKind::entity
            ? listedEntities[ref.index] : listedShots[ref.index];
        if (!listed) {
            pending.push_back(ref);
        } else if (!pending.empty()) {
            throw std::invalid_argument("pending Level1 appends must form the allocation suffix");
        }
    }
    if (pending.size() > 2 || (!pending.empty() && !input.refreshRequested)
        || (pending.size() == 2 && (pending[0].kind != Level1PresentationKind::projectile
            || pending[1].kind != Level1PresentationKind::entity))) {
        throw std::invalid_argument("unsupported Level1 pending append order");
    }
    std::size_t missingEntities = 0;
    for (std::size_t i = 0; i < listedEntities.size(); ++i) {
        if (listedEntities[i]) { continue; }
        const auto& actor = input.entities[i];
        if (++missingEntities != 1 || !input.refreshRequested || !ordinaryStorageIndex(i)
            || actor.stateBytes4dTo52[5] != 0 || actor.stateBytes4dTo52[3] != 0
            || actor.behaviorTimer != 0 || actor.modelAnimationVariant != 0
            || actor.weaponCooldown != 0 || actor.weaponSlotTimers != std::array<std::uint8_t,3>{}) {
            throw std::invalid_argument("unlisted ordinary actor requires its fresh append refresh");
        }
    }
    std::size_t missingShots = 0;
    for (std::size_t slot = 0; slot < input.projectiles.size(); ++slot) {
        const auto& shot = input.projectiles[slot];
        if (shot.poolSlot != slot || allocatedShots[slot] != shot.active()) {
            throw std::invalid_argument("dynamic projectile pool identity/membership mismatch");
        }
        if (!shot.active()) { continue; }
        const auto primary = shot.type == ordinaryPrimaryType;
        const auto airborne=airborneAllocationDomain(allocationDomain)
            && shot.type==20 && !shot.ownerIsPlayer && shot.ownerAllocationIndex<input.entities.size()
            && (roster ? level1RetainedEmitterIdentity(input.entities,shot.ownerAllocationIndex)
                : airborneOwner(input.entities[shot.ownerAllocationIndex],ordinaryFamily));
        if (!roster && airborne && input.entities[shot.ownerAllocationIndex].type==3 && (shot.roll!=0 || shot.position.z!=0)) {
            throw std::invalid_argument("Type3-owned20 retains its flat source birth motion");
        }
        const auto hostile = shot.type == 17 && !shot.ownerIsPlayer
            && shot.ownerAllocationIndex < input.entities.size()
            && (roster ? level1RetainedEmitterIdentity(input.entities,shot.ownerAllocationIndex)
                : input.entities[shot.ownerAllocationIndex].type == 1);
        if ((!primary && !hostile && !airborne) || shot.collisionRadius != (primary || airborne ? ordinaryPrimaryRadius : 441)
            || shot.stateBytes4dTo54[0] != ordinaryPrimaryCategory
            || shot.stateBytes4dTo54[1] < (combatDomain == Level1PlayerCombatDomain::expiryAndLoss ? 1 : 2)
            || shot.stateBytes4dTo54[1] > (primary ? ordinaryPrimaryLifetime : airborne ? 60 : 54)
            || std::any_of(shot.stateBytes4dTo54.begin()+2, shot.stateBytes4dTo54.end(), [](auto value) { return value != 0; })
            || (primary && (!shot.ownerIsPlayer || shot.ownerAllocationIndex != std::numeric_limits<std::size_t>::max()))
            || shot.pitch != 0 || (!airborne && shot.roll != 0) || shot.pitchVelocity != 0 || shot.rollVelocity != 0 || shot.yawVelocity != 0
            || shot.xVelocity != 0 || shot.yVelocity != 0 || shot.zVelocity != 0
            || shot.forwardVelocity != ordinaryPrimarySpeed || shot.position.z < 0
            || shot.position.z > (airborne ? level1LiftCruiseAltitude : 0)) {
            throw std::invalid_argument("unsupported dynamic ordinary projectile record");
        }
        requireVisiblePrefixPosition(shot.position, input.player.position);
        if (listedShots[slot]) { continue; }
        if (++missingShots != 1 || !input.refreshRequested || !primary || shot.stateBytes4dTo54[1] != ordinaryPrimaryLifetime
            || (allocationDomain == Level1ProjectileAllocationDomain::freshOnly && (shot.presenterDistance != 0 || shot.targetHeading != 0))) {
            throw std::invalid_argument("unlisted dynamic shot requires its pending primary append");
        }
    }
    validateLevel1PrefixInput({input.player, input.entities, entityAllocationOrder, entityActiveOrder,
        input.lastListCamera, input.refreshRequested, input.rng, damage ? std::uint16_t{0} : input.fadeTicks,
        input.specialActorPresent, input.impulse, input.active, input.projectileHandlePresent},
        contactDomain, Level1OrdinaryMembership::periodicSpawned,Level1ActorStorageValidation::live,ordinaryFamily);
}

void validateProjectilePrefix(const Level1ProjectilePrefixInput& input, const bool enemy = false,
    const bool damage = false,
    const Level1ProjectileAllocationDomain allocationDomain = Level1ProjectileAllocationDomain::freshOnly,
    const Level1ActorContactDomain contactDomain = Level1ActorContactDomain::none,
    const Level1PlayerCombatDomain combatDomain = Level1PlayerCombatDomain::survivingOnly,
    const Level1OrdinaryMembership membership = Level1OrdinaryMembership::initialSix,
    const Level1OrdinaryFamily ordinaryFamily=Level1OrdinaryFamily::type1) {
    if ((ordinaryFamily!=Level1OrdinaryFamily::type1 && !type3Family(ordinaryFamily))
        || (type3Family(ordinaryFamily) && membership!=Level1OrdinaryMembership::periodicSpawned)
        || (membership != Level1OrdinaryMembership::initialSix && membership != Level1OrdinaryMembership::periodicSpawned)) {
        throw std::invalid_argument("unsupported ordinary membership domain");
    }
    if (allocationDomain != Level1ProjectileAllocationDomain::freshOnly
        && allocationDomain != Level1ProjectileAllocationDomain::retained
        && allocationDomain != Level1ProjectileAllocationDomain::retainedAirborne
        && allocationDomain != Level1ProjectileAllocationDomain::retainedRoster) {
        throw std::invalid_argument("unsupported Level1 projectile allocation domain");
    }
    if (allocationDomain==Level1ProjectileAllocationDomain::retainedRoster && membership!=Level1OrdinaryMembership::periodicSpawned) {
        throw std::invalid_argument("retained emitter identity requires the explicit periodic roster");
    }
    if (contactDomain != Level1ActorContactDomain::none && contactDomain != Level1ActorContactDomain::ordinaryActors
        && contactDomain != Level1ActorContactDomain::ordinaryPlayer) {
        throw std::invalid_argument("unsupported Level1 actor contact domain");
    }
    if (membership == Level1OrdinaryMembership::periodicSpawned) {
        validateDynamicProjectilePrefix(input, enemy, damage, allocationDomain, contactDomain, combatDomain,ordinaryFamily);
        return;
    }
    if ((enemy ? input.allocationOrder.size() < level1ActorCount : input.allocationOrder.size() <= level1ActorCount)
        || input.allocationOrder.size() > level1ActorCount + input.projectiles.size()
        || input.activeOrder.size() < level1ActorCount
        || input.activeOrder.size() > input.allocationOrder.size()) {
        throw std::invalid_argument("mixed Level1 prefix requires six entities and live primary shots");
    }
    std::size_t liveCount = 0;
    for (std::size_t slot = 0; slot < input.projectiles.size(); ++slot) {
        const auto& shot = input.projectiles[slot];
        if (shot.poolSlot != slot) {
            throw std::invalid_argument("mixed Level1 prefix requires stable pool identities");
        }
        if (!shot.active()) {
            continue;
        }
        ++liveCount;
        const auto primary = shot.type == ordinaryPrimaryType;
        const auto airborne=enemy && allocationDomain==Level1ProjectileAllocationDomain::retainedAirborne
            && shot.type==20 && !shot.ownerIsPlayer && shot.ownerAllocationIndex==1;
        const auto hostile = enemy && !shot.ownerIsPlayer
            && shot.type == 17 && shot.ownerAllocationIndex == 0;
        const auto radius = shot.type == 17 ? 441U : ordinaryPrimaryRadius;
        const auto lifetime = shot.type == 17 ? 54 : shot.type == 20 ? 60 : ordinaryPrimaryLifetime;
        if ((!primary && !hostile && !airborne) || shot.collisionRadius != radius
            || shot.stateBytes4dTo54[0] != ordinaryPrimaryCategory
            || shot.stateBytes4dTo54[1] < (combatDomain == Level1PlayerCombatDomain::expiryAndLoss ? 1 : 2)
            || shot.stateBytes4dTo54[1] > lifetime
            || std::any_of(shot.stateBytes4dTo54.begin() + 2, shot.stateBytes4dTo54.end(),
                [](const std::uint8_t value) { return value != 0; })
            || (primary && (!shot.ownerIsPlayer || shot.ownerAllocationIndex != std::numeric_limits<std::size_t>::max()))
            || shot.pitch != 0 || (shot.type != 20 && shot.roll != 0) || shot.pitchVelocity != 0 || shot.rollVelocity != 0
            || shot.yawVelocity != 0 || shot.xVelocity != 0 || shot.yVelocity != 0 || shot.zVelocity != 0
            || shot.forwardVelocity != ordinaryPrimarySpeed || shot.position.z < 0
            || shot.position.z > (shot.type == 20 ? level1LiftCruiseAltitude : 0)) {
            throw std::invalid_argument("unsupported ordinary primary projectile prefix");
        }
        requireVisiblePrefixPosition(shot.position, input.player.position);
    }
    if ((!enemy && liveCount == 0) || input.allocationOrder.size() != level1ActorCount + liveCount) {
        throw std::invalid_argument("mixed Level1 allocations must retain every live primary shot");
    }
    for (std::size_t index = 0; index < level1ActorCount; ++index) {
        if (input.allocationOrder[index] != Level1ObjectRef{Level1PresentationKind::entity, index}) {
            throw std::invalid_argument("mixed Level1 prefix changed entity allocations");
        }
    }
    std::array<bool, Level1State::projectilePoolSize> allocated{};
    for (std::size_t index = level1ActorCount; index < input.allocationOrder.size(); ++index) {
        const auto reference = input.allocationOrder[index];
        if (reference.kind != Level1PresentationKind::projectile || reference.index >= input.projectiles.size()
            || !input.projectiles[reference.index].active() || allocated[reference.index]) {
            throw std::invalid_argument("mixed Level1 prefix requires unique allocated pool identities");
        }
        allocated[reference.index] = true;
    }
    std::vector<std::size_t> entityOrder;
    std::array<bool, Level1State::projectilePoolSize> listed{};
    for (const auto reference : input.activeOrder) {
        if (reference.kind == Level1PresentationKind::entity) {
            entityOrder.push_back(reference.index);
        } else if (reference.kind == Level1PresentationKind::projectile
            && reference.index < listed.size() && allocated[reference.index] && !listed[reference.index]) {
            listed[reference.index] = true;
        } else {
            throw std::invalid_argument("invalid mixed Level1 active reference");
        }
    }
    for (std::size_t slot = 0; slot < allocated.size(); ++slot) {
        if (!allocated[slot] || listed[slot]) {
            continue;
        }
        const auto& shot = input.projectiles[slot];
        if (input.allocationOrder.back() != Level1ObjectRef{Level1PresentationKind::projectile, slot}
            || !input.refreshRequested || shot.type != ordinaryPrimaryType || shot.stateBytes4dTo54[1] != ordinaryPrimaryLifetime
            || (allocationDomain == Level1ProjectileAllocationDomain::freshOnly
                && (shot.presenterDistance != 0 || shot.targetHeading != 0))) {
            throw std::invalid_argument("unlisted primary projectile requires its pending append refresh");
        }
    }
    validateLevel1PrefixInput({input.player, input.entities, {0, 1, 2, 3, 4, 5}, entityOrder,
        input.lastListCamera, input.refreshRequested, input.rng,
        damage ? std::uint16_t{0} : input.fadeTicks, input.specialActorPresent,
        input.impulse, input.active, input.projectileHandlePresent}, contactDomain);
}
} // namespace

namespace {
void bonusPickupOperations(EnemyFireContext& enemy,const Level1ObjectRef reference) {
    auto& work = enemy.work;
    auto& context = *enemy.target;
    auto& result = context.result;
    auto& execution = *context.pickup;
    auto& output = execution.output;
    const auto& actor = work.entities.at(reference.index);
    const auto payload = actor.stateBytes4dTo52[5];
    const auto child = level1Type42PickupOperations({actor,0,result.score,output.systems,
        output.timers,output.selectedSystem,context.difficulty,work.allocationOrder},
        context.scoreFont,execution.input.artGroup4,context.pages);
    const auto begin = work.operations.size();
    for (const auto& step : child.steps) {
        if (const auto* drawing = std::get_if<Level1Type42PickupDrawing>(&step)) {
            append(work.operations,drawing->operations);
            if (!drawing->effects.empty()) {
                output.drawingSteps.push_back({work.operations.size(),drawing->effects});
            }
        } else if (const auto* score = std::get_if<ScoreWork>(&step)) {
            result.scoreSteps.push_back({work.operations.size(),*score});
        } else {
            const auto& audio = std::get<Level1Type42PickupAudio>(step);
            if (audio.event != 24 || audio.callerBegin != 0x9103 || audio.callerEnd != 0x910d
                || audio.callerDispatches != 4) {
                throw std::logic_error("pickup audio child lost source caller ownership");
            }
            work.operations.push_back({NormalWorkKind::gameplayBonusPickupEvent,1,audio.event});
            enemy.events.push_back({enemy.profile.loopTick,audio.event});
        }
    }
    work.entities.at(reference.index) = child.pickup;
    work.allocationOrder = child.allocationOrder;
    work.refreshRequested = child.refreshRequested;
    result.score = child.score;
    output.systems = child.systems;
    auto pages = context.pages;
    pages.selectScoreCopy();
    if (pages.drawPage() != child.drawPage || pages.copyPage() != child.copyPage) {
        throw std::logic_error("pickup final source selectors disagree with physical roles");
    }
    output.presentation = Level1BonusPickupPresentation{pages,child.fillStyle,child.fillColor,child.selectedFont};
    output.pickups.push_back({reference.index,payload,begin,work.operations.size(),child.removal});
}

void appendDeploymentSteps(EnemyFireContext& enemy,const std::vector<Level1DeploymentStep>& steps) {
    auto& output=*enemy.target->deploymentOutput;
    for(const auto& step:steps) {
        if(const auto* plain=std::get_if<std::vector<NormalWorkOperation>>(&step)) {append(enemy.work.operations,*plain);}
        else if(const auto* heap=std::get_if<Level1DeploymentHeapChild>(&step)) {output.children.push_back({enemy.work.operations.size(),*heap});}
        else {
            const auto audio=std::get<Level1DeploymentAudioChild>(step);
            output.children.push_back({enemy.work.operations.size(),audio});
            if(audio.kind==Level1DeploymentAudioKind::directEvent6) {enemy.events.push_back({enemy.profile.loopTick,audio.eventId});}
        }
    }
}
Level1PeriodicSpawnInput deploymentPool(const EnemyFireContext& enemy) {
    auto pool=enemy.target->deployment->pool;pool.activeEnemies=enemy.target->result.activeEnemies;
    pool.allocationCount=static_cast<std::uint16_t>(enemy.work.allocationOrder.size());pool.nextEntityIndex=enemy.work.entities.size();
    for(auto& slot:pool.ordinaryPool) {
        if(slot.entityIndex) {
            const auto& actor=enemy.work.entities.at(*slot.entityIndex);slot.health=actor.stateBytes4dTo52[1];
            slot.hasAnimation=actor.primaryType11AnimationSelector
                || actor.renderProfile==WorldActorRenderProfile::heapSelectorDeferredAnimation
                || actor.renderProfile==WorldActorRenderProfile::heapSelectorNoCallback;
        }
    }
    return pool;
}
void deploymentBehaviorOperations(EnemyFireContext& enemy,const std::size_t index) {
    auto& actor=enemy.work.entities.at(index);const auto& context=*enemy.target->deployment;
    if(actor.type==7) {
        if(context.craftEntityIndex!=std::optional<std::size_t>{index}) {throw std::invalid_argument("deployment craft identity mismatch");}
        auto child=level1Type7DeploymentOperations({actor,context.guardianEntityIndex,deploymentPool(enemy),enemy.work.rng,
            enemy.work.allocationOrder},context.models);
        actor=child.actor;enemy.work.rng=child.rng;enemy.work.allocationOrder=child.allocationOrder;
        if(child.refreshRequested) {enemy.work.refreshRequested=true;}
        appendDeploymentSteps(enemy,child.steps);
        if(child.guardianBirth) {
            const auto& birth=*child.guardianBirth;const auto id=birth.actor.allocationIndex;
            if(id>enemy.work.entities.size()) {throw std::invalid_argument("guardian birth lost canonical entity identity");}
            if(id==enemy.work.entities.size()) {enemy.work.entities.push_back(birth.actor);}else {enemy.work.entities[id]=birth.actor;}
            enemy.target->result.activeEnemies=birth.activeEnemies;
        }
        enemy.target->deploymentOutput->observations.push_back({index,child.guardianBirth,{},child.craftRetired});
    } else {
        if(context.guardianEntityIndex!=std::optional<std::size_t>{index}) {throw std::invalid_argument("deployment guardian identity mismatch");}
        auto child=level1GuardianBehaviorOperations({actor,enemy.work.player,enemy.profile.loopTick,enemy.work.projectiles,
            enemy.work.allocationOrder,context.reward && !actor.active()
                ? Level1GuardianBehaviorDomain::inheritedRetired : Level1GuardianBehaviorDomain::live},context.models.at(22));
        actor=child.actor;enemy.work.projectiles=child.projectiles;enemy.work.allocationOrder=child.allocationOrder;
        if(child.refreshRequested) {enemy.work.refreshRequested=true;}
        appendDeploymentSteps(enemy,child.steps);enemy.target->deploymentOutput->observations.push_back({index,{},child.launchedSlot,false});
    }
}

enum class PrefixExecutionStart { frameEntry, successReturn };
void executeProjectilePrefix(Level1ProjectilePrefixWork& result, EnemyFireContext* enemy,
    const PrefixExecutionStart start=PrefixExecutionStart::frameEntry,
    const PrefixCampaignDomain campaign = PrefixCampaignDomain::level1) {
    const auto level2 = campaign == PrefixCampaignDomain::level2Constructor;
    const auto periodic=level2 && enemy && enemy->level2Periodic;
    const auto ordinaryPlayer=periodic || (!level2 && enemy && enemy->profile.contactDomain==Level1ActorContactDomain::ordinaryPlayer);
    if(start==PrefixExecutionStart::successReturn) {
        auto& context=*enemy->target;auto& output=*context.deploymentOutput;
        output.rewardContact=level1RewardContactOperations(result.entities.at(*output.rewardEntityIndex),
            context.deployment->reward->campaignIndex);
        append(result.operations,output.rewardContact->afterSuccess);
        instructions(result.operations,4); //094F..095E inherited cursor0.
    } else {
    if (enemy && enemy->damage && enemy->damage->fadeTicks != 0) {
        auto& damage = *enemy->damage;
        --damage.fadeTicks;
        instructions(result.operations, 5 + 13 + 11); //01D0..0212 ability0 caller;resident1B38 prefix.
        damage.paletteSteps.push_back({result.operations.size(),
            {0,256,238,static_cast<std::uint8_t>(63 - damage.fadeTicks * 4)}});
        if (ordinaryPlayer) {
            instructions(result.operations,3+1+2); //Resident return;0217;021A/021F.
        } else {
            instructions(result.operations, 3 + 1 + 5 + 7);
        }
    } else if (ordinaryPlayer) {
        instructions(result.operations,2+2); //01D0/01D5;021A/021F.
    } else {
        instructions(result.operations, 7 + 7);
    }
    if (ordinaryPlayer) {
        auto child=level1PlayerImpulseOperations(result.player,result.impulse);
        const auto offset=result.operations.size();
        result.player=child.player;result.impulse=child.impulse;
        append(result.operations,child.operations);
        for (auto receipt:child.receipts) {
            receipt.operationBegin+=offset;receipt.operationEnd+=offset;result.contacts.push_back(receipt);
        }
        instructions(result.operations,1+7); //024E CALL and0784..0790.
    }
    const auto playerKinematics = flatKinematicsOperations(result.player);
    result.player = playerKinematics.actor;
    append(result.operations, playerKinematics.operations);
    instructions(result.operations, 4);
    const auto playerContact = mixedContactOperations(result, collisionBody(result.player), std::nullopt,
        ordinaryPlayer ? Level1ActorContactDomain::ordinaryPlayer : Level1ActorContactDomain::none, enemy ? enemy->target : nullptr,
        enemy ? enemy->profile.ordinaryFamily : Level1OrdinaryFamily::type1);
    if (playerContact) {
        const auto rewardContact=enemy && enemy->target && enemy->target->deployment
            && enemy->target->deployment->reward && playerContact->kind==Level1PresentationKind::entity
            && result.entities[playerContact->index].type==25;
        if (periodic && playerContact->kind==Level1PresentationKind::entity) {
            const auto& target=result.entities[playerContact->index];
            auto child=level2SolidContactOperations(result.player,target,playerContact->index,target.collisionRadius);
            auto& contact=child.contact;
            const auto offset=result.operations.size();
            std::size_t inserted=0;
            if (child.damageChildOffset) {
                const auto boundary=*child.damageChildOffset;
                result.operations.insert(result.operations.end(),contact.operations.begin(),contact.operations.begin()+boundary);
                const auto before=result.operations.size();
                instructions(result.operations,3); //08C2/08C5/08C6.
                type4PlayerDamageOperations(*enemy);
                instructions(result.operations,1); //08C9 cleanup.
                inserted=result.operations.size()-before;
                result.operations.insert(result.operations.end(),contact.operations.begin()+boundary,contact.operations.end());
            } else {append(result.operations,contact.operations);}
            result.player=contact.player;result.impulse=contact.impulse;
            for (auto receipt:contact.receipts) {
                if (child.damageChildOffset) {
                    if (receipt.operationBegin>=*child.damageChildOffset) {receipt.operationBegin+=inserted;}
                    if (receipt.operationEnd>=*child.damageChildOffset) {receipt.operationEnd+=inserted;}
                }
                receipt.operationBegin+=offset;receipt.operationEnd+=offset;result.contacts.push_back(receipt);
            }
            enemy->events.push_back({enemy->profile.loopTick,9});
        } else if (ordinaryPlayer && playerContact->kind==Level1PresentationKind::entity
            && ordinarySolidTarget(result.entities[playerContact->index],enemy->profile.ordinaryFamily)) {
            auto child=solidContactOperations(result.player,result.entities[playerContact->index],playerContact->index,enemy->profile.ordinaryFamily);
            const auto offset=result.operations.size();
            result.player=child.player;result.impulse=child.impulse;
            append(result.operations,child.operations);
            for (auto receipt:child.receipts) {
                receipt.operationBegin+=offset;receipt.operationEnd+=offset;result.contacts.push_back(receipt);
            }
            enemy->events.push_back({enemy->profile.loopTick,9});
        } else if(rewardContact) {
            auto& context=*enemy->target;auto& output=*context.deploymentOutput;
            if(output.rewardEntityIndex!=std::optional<std::size_t>{playerContact->index}) {
                throw std::invalid_argument("Type25 contact lost128E canonical identity");
            }
            auto child=level1RewardContactOperations(result.entities[playerContact->index],context.deployment->reward->campaignIndex);
            instructions(result.operations,6); //07A3..07B4 ability0 selected contact.
            append(result.operations,child.beforeSuccess);
            output.children.push_back({result.operations.size(),child.success});
            output.successPause=Level1RewardSuccessPause{playerContact->index,*output.rewardEntityIndex,enemy->profile.loopTick};
            return; //0802: canonical output is the pre-child entry projection.
        } else {
        if (!enemy || !enemy->target || !enemy->target->pickup
            || playerContact->kind != Level1PresentationKind::entity
            || result.entities[playerContact->index].type != systemBonusType) {
            throw std::invalid_argument("player contact requires the explicit Type42 pickup owner");
        }
        instructions(result.operations,14); //07A3..0827, ability0 and both Type25/26 guards.
        bonusPickupOperations(*enemy,*playerContact);
        }
        instructions(result.operations,4); //094F/0954/0959/095E old-list setup.
    } else {
        instructions(result.operations,9);
    }
    }
    for (std::size_t ordinal = 0; ordinal < result.activeOrder.size(); ++ordinal) {
        const auto reference = result.activeOrder[ordinal];
        if (reference.kind == Level1PresentationKind::deathEffect) {
            instructions(result.operations, 5); //0963..096E skipsFC physics/callback.
        } else if (reference.kind == Level1PresentationKind::projectile) {
            instructions(result.operations, 8);
            primaryUpdateOperations(result, result.projectiles[reference.index], enemy);
            instructions(result.operations, 2); //097B/097E cleanup and loop-tail JMP.
        } else {
            auto& actor = result.entities[reference.index];
            const auto bonus = actor.stateBytes4dTo52[0] == 254 && actor.type == systemBonusType;
            instructions(result.operations, bonus ? 6 : 8);
            if (actor.stateBytes4dTo52[0] == 254) {
                if (actor.type == systemBonusType) {
                    if (!enemy || !enemy->target || !enemy->target->bonusModel) {
                        throw std::invalid_argument("Type42 update requires its explicit bonus context");
                    }
                    const auto kinematics = level1BonusFixedUpdateOperations(actor);
                    actor = kinematics.actor;
                    append(result.operations,kinematics.operations);
                } else {
                    instructions(result.operations, 3);
                }
            } else {
                instructions(result.operations, 3);
                const auto kinematics = level2 ? flatKinematicsOperations(actor)
                    : enemy && enemy->target && enemy->target->deployment && actor.type==7
                        ? failureKinematicsOperations(actor) : level1KinematicsOperations(actor);
                actor = kinematics.actor;
                append(result.operations, kinematics.operations);
                instructions(result.operations, 4);
                const auto overlap = actorOverlapOperations(actor, result.player);
                append(result.operations, overlap.operations);
                std::optional<Level1ObjectRef> contact;
                const auto contactIsPlayer=overlap.overlaps;
                if (contactIsPlayer) {
                    const auto level2Lift=level2 && actor.type==13 && enemy && enemy->damage;
                    const auto level2Class1=periodic && (actor.type==1 || actor.type==4);
                    if (!level2Lift && !level2Class1 && !(periodic && actor.type==8)
                        && (!ordinaryPlayer || !ordinarySolidTarget(actor,enemy->profile.ordinaryFamily) || actor.type==29)) {
                        throw std::invalid_argument("mixed Level1 prefix reached actor/player collision");
                    }
                    const auto begin=result.operations.size();
                    instructions(result.operations,3); //09A9..09AE overlap result.
                    if (level2Lift) {
                        instructions(result.operations,5+4); //09B0..09C4;09CC..09D5.
                        actor.stateBytes4dTo52[1]=0;--enemy->target->result.activeEnemies;
                        removeAllocationOperations(result,reference);
                        instructions(result.operations,1); //09D8 cleanup.
                        result.operations.push_back({NormalWorkKind::gameplayLevel2TargetHitEvent,1,6});
                        enemy->events.push_back({enemy->profile.loopTick,6});
                        instructions(result.operations,3); //09E5 raw argument32;09E8/09E9.
                        constexpr std::int16_t rawContactDamage=32;
                        constexpr std::int16_t unshieldedDamageScale=4;
                        playerHitOperations(*enemy,rawContactDamage*unshieldedDamageScale);
                        instructions(result.operations,1); //09EC cleanup.
                    } else if (periodic && actor.type==4) {
                        instructions(result.operations,5+2); //09B0..09BD,09E8/09E9.
                        type4PlayerDamageOperations(*enemy);
                        instructions(result.operations,1); //09EC cleanup.
                    } else {instructions(result.operations,7);} //09B0..09CA ordinary damage bypass.
                    instructions(result.operations,2); //09EF/09F1 skips0C0A.
                    instructions(result.operations,actor.stateBytes4dTo52[4]==3 ? 2 : 4); //09FC..0A04, no09F7 cleanup.
                    result.contacts.push_back({Level1ContactStep::actorPlayerOverlap,0x09a9,
                        static_cast<std::uint16_t>(actor.stateBytes4dTo52[4]==3 ? 0x0a6d : 0x0a06),
                        begin,result.operations.size(),reference.index,{}});
                } else {
                    instructions(result.operations, 3 + 4);
                    contact = mixedContactOperations(result, collisionBody(actor), reference,
                        enemy ? enemy->profile.contactDomain : Level1ActorContactDomain::none,
                        enemy ? enemy->target : nullptr,
                        enemy ? enemy->profile.ordinaryFamily : Level1OrdinaryFamily::type1);
                    instructions(result.operations, !level2 && actor.stateBytes4dTo52[4] == 3 ? 4 : 6);
                }
                if (level2 && (!periodic || actor.stateBytes4dTo52[4]!=1) && (contact || contactIsPlayer)) {
                    actor.collisionContactAllocationIndex = contactIsPlayer ? std::numeric_limits<std::size_t>::max() : contact->index;
                    actor.collisionContactIsPlayer = contactIsPlayer;
                    instructions(result.operations,3 + 12 + 9); //Class4 contact; Level2 skips backout/damage.
                } else if ((contact || contactIsPlayer) && actor.stateBytes4dTo52[4] != 3) {
                    const auto responseBegin=result.operations.size();
                    actor.collisionContactAllocationIndex = contactIsPlayer ? std::numeric_limits<std::size_t>::max() : contact->index;
                    actor.collisionContactIsPlayer = contactIsPlayer;
                    actor.stateBytes4dTo52[5] = 4;
                    actor.behaviorTimer = 0;
                    instructions(result.operations, 3 + 12); //0A06..0A0A;0E50 callback1 leaf.
                    instructions(result.operations, 3 + 6 + 2); //0A0D..0A14;0A34..0A44;0A4C/0A4D.
                    flatBackoutOperations(actor, result.operations);
                    instructions(result.operations, 1 + 2); //0A50 JMP097B;097B cleanup/097E JMP.
                    if (ordinaryPlayer) {
                        result.contacts.push_back({Level1ContactStep::actorResponse,0x0a06,0x0a6d,
                            responseBegin,result.operations.size(),reference.index,
                            contactIsPlayer ? std::nullopt : std::optional<std::size_t>{contact->index}});
                    }
                }
            }
        }
        instructions(result.operations, ordinal + 1 == result.activeOrder.size() ? 5 : 6);
    }
    if(enemy && enemy->target && enemy->target->deployment && enemy->target->deployment->reward) {
        auto& output=*enemy->target->deploymentOutput;
        instructions(result.operations,2);
        const auto oscillator=level1RewardOscillatorOperations(output.rewardEntityIndex
            ? std::optional<Level1Entity>{result.entities.at(*output.rewardEntityIndex)} : std::nullopt);
        append(result.operations,oscillator.operations);
        if(oscillator.actor) {result.entities.at(*output.rewardEntityIndex)=*oscillator.actor;}
        instructions(result.operations,3+10+6);
        if(output.rewardContact) {
            instructions(result.operations,6); //0251..0263 inactive jump0312.
            return;
        }
    } else {instructions(result.operations, 2 + 10 + 3 + 10 + 6);}
    instructions(result.operations, 8 + 13);
    for (const auto reference : result.activeOrder) {
        if (reference.kind == Level1PresentationKind::projectile) {
            level1PresenterFieldsOperations(result.projectiles[reference.index], result.player, result.operations);
        } else if (reference.kind == Level1PresentationKind::deathEffect) {
            level1PresenterFieldsOperations(enemy->target->result.effects[reference.index].entity,
                result.player,result.operations);
        } else {
            auto& actor = result.entities[reference.index];
            level1PresenterFieldsOperations(actor, result.player, result.operations);
            if (actor.stateBytes4dTo52[0] == 255) {
                instructions(result.operations, 8);
                if (level2) {
                    if (periodic && (actor.type==1 || actor.type==4)) {
                        level1TankOperations(actor,result.rng,result.operations,enemy);
                    } else if (actor.type == 8) {
                        type8Operations(actor,result.operations,enemy ? (enemy->level2Hostile ? enemy : nullptr) : nullptr);
                    } else {
                        type13Operations(actor,result.player,result.entities,result.operations,Type13VelocityDomain::headingChase);
                    }
                } else if ((actor.type==7 || actor.type==44) && enemy && enemy->target && enemy->target->deployment) {
                    if(actor.type==44) {instructions(result.operations,15);} //A418 non-Type7 dispatch and cleanup.
                    deploymentBehaviorOperations(*enemy,reference.index);
                } else if (actor.type == 7) {
                    if(!enemy || !enemy->target || !enemy->target->arrival
                        || enemy->target->arrival->craftEntityIndex!=reference.index) {
                        throw std::invalid_argument("Type7 requires its explicit arrival prefix context");
                    }
                    auto child=level1Type7Operations(actor,enemy->target->arrival->guardianEntityIndex);
                    actor=child.actor;append(result.operations,child.operations);
                } else if (actor.type == 1 || actor.type==3) {
                    level1TankOperations(actor, result.rng, result.operations, enemy);
                } else {
                    level1LiftOperations(actor, result.player, result.rng, result.operations, enemy);
                }
                instructions(result.operations, 1);
            }
        }
        instructions(result.operations, 6);
    }
    instructions(result.operations, 5 + 1);
    const auto positionOf = [&result,enemy](const Level1ObjectRef reference) {
        return mixedBody(result, reference,enemy ? enemy->target : nullptr).position;
    };
    level1SortOperations(result, false, positionOf, campaign);
    instructions(result.operations, 10);
    if (level1RefreshOperations(result)) {
        level1RebuildOperations(result, result.allocationOrder, [&result,enemy](const Level1ObjectRef reference) {
            const auto position = mixedBody(result, reference,enemy ? enemy->target : nullptr).position;
            requireVisiblePrefixPosition(position, result.player.position);
            return position;
        });
    }
    instructions(result.operations, 5);
}
} // namespace

Level1ProjectilePrefixWork level1ProjectilePrefixOperations(const Level1ProjectilePrefixInput& input) {
    validateProjectilePrefix(input);
    Level1ProjectilePrefixWork result{input.player, input.entities, input.projectiles, input.allocationOrder,
        input.activeOrder, input.lastListCamera, input.refreshRequested, input.rng, {}};
    executeProjectilePrefix(result, nullptr);
    return result;
}

Level1EnemyPrefixWork level1EnemyPrefixOperations(const Level1ProjectilePrefixInput& input,
    const Level1EnemyPrefixProfile& profile) {
    validateProjectilePrefix(input, true, false, profile.allocationDomain, profile.contactDomain,
        Level1PlayerCombatDomain::survivingOnly, profile.membership,profile.ordinaryFamily);
    Level1EnemyPrefixWork result{{input.player, input.entities, input.projectiles, input.allocationOrder,
        input.activeOrder, input.lastListCamera, input.refreshRequested, input.rng, {}}, {}};
    EnemyFireContext context{result.prefix, profile, result.audioEvents};
    result.prefix.impulse=input.impulse;
    executeProjectilePrefix(result.prefix, &context);
    return result;
}

Level1DamagePrefixWork level1DamagePrefixOperations(const Level1ProjectilePrefixInput& input,
    const Level1DamagePrefixProfile& profile) {
    if ((profile.combatDomain != Level1PlayerCombatDomain::survivingOnly
            && profile.combatDomain != Level1PlayerCombatDomain::expiryAndLoss)
        || profile.armor < (profile.combatDomain == Level1PlayerCombatDomain::expiryAndLoss ? 0 : 40)
        || profile.armor > 680 || input.fadeTicks > 8
        || profile.missionComplete != 0 || profile.shieldTicks != 0 || profile.abilityTicks != 0
        || profile.paletteMode != Level1DamagePaletteMode::residentVga) {
        throw std::invalid_argument("unsupported ordinary surviving player damage profile");
    }
    validateProjectilePrefix(input, true, true, profile.enemy.allocationDomain, profile.enemy.contactDomain, profile.combatDomain, profile.enemy.membership,profile.enemy.ordinaryFamily);
    Level1DamagePrefixWork result{{{input.player, input.entities, input.projectiles, input.allocationOrder,
        input.activeOrder, input.lastListCamera, input.refreshRequested, input.rng, {}}, {}},
        profile.armor, input.fadeTicks, {}};
    PlayerDamageContext damage{result.armor, result.fadeTicks, result.paletteSteps, profile.combatDomain};
    EnemyFireContext enemy{result.combat.prefix, profile.enemy, result.combat.audioEvents, &damage};
    result.combat.prefix.impulse=input.impulse;
    executeProjectilePrefix(result.combat.prefix, &enemy);
    return result;
}

namespace {
enum class TargetPrefixDomain { ordinary, systemBonus, systemBonusPickup, arrival, deployment, guardianReward };

void validateTargetPrefixInput(const Level1TargetPrefixInput& input, const Level1TargetPrefixProfile& profile,
    const TargetPrefixDomain domain = TargetPrefixDomain::ordinary) {
    const auto rewardDomain = domain == TargetPrefixDomain::guardianReward;
    const auto deploymentDomain = domain == TargetPrefixDomain::deployment || rewardDomain;
    const auto arrivalDomain = domain == TargetPrefixDomain::arrival || deploymentDomain;
    const auto pickupDomain = domain == TargetPrefixDomain::systemBonusPickup || arrivalDomain;
    const auto bonusDomain = domain == TargetPrefixDomain::systemBonus || pickupDomain;
    const auto& prefix = input.prefix;
    const auto& damage = profile.damage;
    const auto& enemy = damage.enemy;
    const auto roster=enemy.allocationDomain==Level1ProjectileAllocationDomain::retainedRoster;
    if ((profile.targetDomain != Level1TargetHitDomain::ordinaryTank
            && profile.targetDomain != Level1TargetHitDomain::initialOrdinary
            && profile.targetDomain != Level1TargetHitDomain::periodicClass1)
        || (type3Family(enemy.ordinaryFamily)!=(profile.targetDomain==Level1TargetHitDomain::periodicClass1))
        || profile.difficulty != 0 || input.score > 2147483627U
        || enemy.membership != Level1OrdinaryMembership::periodicSpawned
        || (enemy.allocationDomain != Level1ProjectileAllocationDomain::freshOnly
            && enemy.allocationDomain != Level1ProjectileAllocationDomain::retained
            && enemy.allocationDomain != Level1ProjectileAllocationDomain::retainedAirborne && !roster)
        || (enemy.contactDomain != Level1ActorContactDomain::none
            && enemy.contactDomain != Level1ActorContactDomain::ordinaryActors
            && enemy.contactDomain != Level1ActorContactDomain::ordinaryPlayer)
        || (damage.combatDomain != Level1PlayerCombatDomain::survivingOnly
            && damage.combatDomain != Level1PlayerCombatDomain::expiryAndLoss)
        || damage.armor < (damage.combatDomain == Level1PlayerCombatDomain::expiryAndLoss ? 0 : 40)
        || damage.armor > 680 || prefix.fadeTicks > 8 || damage.missionComplete != 0
        || damage.shieldTicks != 0 || damage.abilityTicks != 0
        || damage.paletteMode != Level1DamagePaletteMode::residentVga
        || prefix.entities.size() < level1ActorCount
        || prefix.entities.size() > (rewardDomain ? maximumBonusActorCount+3 : deploymentDomain ? maximumBonusActorCount+2 : arrivalDomain ? maximumBonusActorCount+1 : bonusDomain ? maximumBonusActorCount : maximumLevel1ActorCount)
        || (bonusDomain && profile.targetDomain != Level1TargetHitDomain::initialOrdinary
            && profile.targetDomain != Level1TargetHitDomain::periodicClass1)
        || prefix.activeOrder.empty() || input.activeEnemies < 0 || input.activeEnemies > (deploymentDomain?12:arrivalDomain?11:10)) {
        throw std::invalid_argument("unsupported retained ordinary target prefix profile");
    }
    //Validate record/player/RNG shape independently of membership. These local
    //indices enumerate actual storage; they never replace49F8/45D2 or run work.
    std::vector<std::size_t> storage;
    for (std::size_t index = 0; index < prefix.entities.size(); ++index) { storage.push_back(index); }
    validateLevel1PrefixInput({prefix.player,prefix.entities,storage,storage,prefix.lastListCamera,
        prefix.refreshRequested,prefix.rng,0,prefix.specialActorPresent,prefix.impulse,prefix.active,
        prefix.projectileHandlePresent},enemy.contactDomain,Level1OrdinaryMembership::periodicSpawned,
        rewardDomain ? Level1ActorStorageValidation::retainedReward
            : deploymentDomain ? Level1ActorStorageValidation::retainedDeployment
            : arrivalDomain ? Level1ActorStorageValidation::retainedArrival
            : pickupDomain ? Level1ActorStorageValidation::retainedPickup
            : bonusDomain ? Level1ActorStorageValidation::retainedBonus : Level1ActorStorageValidation::retainedCombat,enemy.ordinaryFamily);
    if (bonusDomain) {
        std::size_t ordinary = 0;
        std::size_t fixed = 0;
        std::size_t bonus = 0;
        for (const auto& actor : prefix.entities) {
            if (actor.stateBytes4dTo52[0] == 255) { ++ordinary; }
            if (actor.stateBytes4dTo52[0] == 254) { ++fixed; }
            if (actor.type == systemBonusType) { ++bonus; }
        }
        if (ordinary > (deploymentDomain?12U:arrivalDomain?11U:10U) || fixed > (rewardDomain?6U:5U) || bonus > 1) {
            throw std::invalid_argument("unsupported bonus source pool membership");
        }
        fixedSlotProjection(prefix.entities,rewardDomain);
    }
    const auto valid = [&](const Level1ObjectRef ref) {
        if (ref.kind == Level1PresentationKind::entity) { return ref.index < prefix.entities.size(); }
        if (ref.kind == Level1PresentationKind::projectile) { return ref.index < prefix.projectiles.size(); }
        return ref.kind == Level1PresentationKind::deathEffect && ref.index < input.effects.size();
    };
    const auto live = [&](const Level1ObjectRef ref) {
        if (ref.kind == Level1PresentationKind::entity) { return prefix.entities[ref.index].active(); }
        if (ref.kind == Level1PresentationKind::projectile) { return prefix.projectiles[ref.index].active(); }
        return input.effects[ref.index].listed && input.effects[ref.index].entity.active();
    };
    const auto contains = [](const auto& list,const auto ref) {
        return std::find(list.begin(),list.end(),ref) != list.end();
    };
    for (std::size_t index = 0; index < prefix.allocationOrder.size(); ++index) {
        const auto ref = prefix.allocationOrder[index];
        if (!valid(ref) || !live(ref)
            || std::find(prefix.allocationOrder.begin(),prefix.allocationOrder.begin()+index,ref)
                != prefix.allocationOrder.begin()+index) {
            throw std::invalid_argument("invalid target-prefix live allocation identity");
        }
    }
    for (std::size_t index = 0; index < prefix.activeOrder.size(); ++index) {
        const auto ref = prefix.activeOrder[index];
        if (!valid(ref) || (!contains(prefix.allocationOrder,ref) && !prefix.refreshRequested)
            || std::find(prefix.activeOrder.begin(),prefix.activeOrder.begin()+index,ref)
                != prefix.activeOrder.begin()+index) {
            throw std::invalid_argument("invalid target-prefix old-list identity");
        }
        if (ref.kind == Level1PresentationKind::projectile && !prefix.projectiles[ref.index].active()) {
            throw std::invalid_argument("inactive FD entry is outside target-prefix updater domain");
        }
        if (ref.kind == Level1PresentationKind::deathEffect) {
            const auto& effect = input.effects[ref.index];
            if (effect.entity.type != -1 || effect.entity.stateBytes4dTo52[0] != 252
                || (!effect.listed && effect.entity.stateBytes4dTo52[4] != 0)) {
                throw std::invalid_argument("terminal old-list FC requires its source retirement shape");
            }
        }
    }
    std::int16_t activeEnemies = 0;
    for (std::size_t index = 0; index < prefix.entities.size(); ++index) {
        const auto& actor = prefix.entities[index];
        if (contains(prefix.allocationOrder,Level1ObjectRef{Level1PresentationKind::entity,index}) != actor.active()
            || (!actor.active() && actor.type != 1 && actor.type != 29
                && !((profile.targetDomain == Level1TargetHitDomain::initialOrdinary
                    || profile.targetDomain==Level1TargetHitDomain::periodicClass1) && actor.type == 11)
                && !(type3Family(enemy.ordinaryFamily) && actor.type==3)
                && !(bonusDomain && actor.type == systemBonusType) && !(deploymentDomain && actor.type==7)
                && !(rewardDomain && actor.type==44))) {
            throw std::invalid_argument("target-prefix entity storage/liveness mismatch");
        }
        if (actor.active() && actor.stateBytes4dTo52[0] == 255) { ++activeEnemies; }
    }
    const auto retainedCraftCount=deploymentDomain ? std::count_if(prefix.entities.begin(),prefix.entities.end(),
        [](const auto& a){return a.type==7 && !a.active();}) : 0;
    if (activeEnemies+retainedCraftCount != input.activeEnemies) {
        throw std::invalid_argument("target-prefix active enemy count mismatch");
    }
    for (std::size_t slot = 0; slot < input.effects.size(); ++slot) {
        const auto& effect = input.effects[slot];
        const auto& bytes = effect.entity.stateBytes4dTo52;
        if (contains(prefix.allocationOrder,Level1ObjectRef{Level1PresentationKind::deathEffect,slot}) != effect.listed
            || (effect.listed && (effect.entity.type != -1 || bytes[0] != 252 || bytes[1] == 0
                || bytes[1] > bytes[4] || bytes[4] > 32))
            || (!effect.listed && bytes[1] != 0)) {
            throw std::invalid_argument("target-prefix FC storage/liveness mismatch");
        }
    }
    for (std::size_t slot = 0; slot < prefix.projectiles.size(); ++slot) {
        const auto& shot = prefix.projectiles[slot];
        if (shot.poolSlot != slot
            || contains(prefix.allocationOrder,Level1ObjectRef{Level1PresentationKind::projectile,slot}) != shot.active()) {
            throw std::invalid_argument("target-prefix projectile storage/liveness mismatch");
        }
        if (!shot.active()) { continue; }
        const auto primary = shot.type == ordinaryPrimaryType;
        const auto airborne=airborneAllocationDomain(enemy.allocationDomain)
            && shot.type==20 && !shot.ownerIsPlayer && shot.ownerAllocationIndex<prefix.entities.size()
            && (roster ? level1RetainedEmitterIdentity(prefix.entities,shot.ownerAllocationIndex)
                : airborneOwner(prefix.entities[shot.ownerAllocationIndex],enemy.ordinaryFamily));
        if (!roster && airborne && prefix.entities[shot.ownerAllocationIndex].type==3 && (shot.roll!=0 || shot.position.z!=0)) {
            throw std::invalid_argument("Type3-owned20 retains its flat source birth motion");
        }
        const auto guardianShot=deploymentDomain && shot.type==22;
        const auto hostile = (shot.type == 17 || guardianShot) && !shot.ownerIsPlayer
            && shot.ownerAllocationIndex < prefix.entities.size()
            && (roster && shot.type==17 ? level1RetainedEmitterIdentity(prefix.entities,shot.ownerAllocationIndex)
                : (prefix.entities[shot.ownerAllocationIndex].type == 1
                || (arrivalDomain && prefix.entities[shot.ownerAllocationIndex].type == 7)
                || (deploymentDomain && prefix.entities[shot.ownerAllocationIndex].type==44)));
        if(guardianShot) {
            if(!hostile || shot.collisionRadius!=134 || shot.stateBytes4dTo54[0]!=253
                || shot.stateBytes4dTo54[1]<1 || shot.stateBytes4dTo54[1]>64 || shot.stateBytes4dTo54[2]!=1
                || shot.stateBytes4dTo54[4]!=0 || shot.forwardVelocity!=500 || shot.zVelocity!=0) {
                throw std::invalid_argument("unsupported retained Type22 guardian projectile");
            }
            requireVisiblePrefixPosition(shot.position,prefix.player.position);continue;
        }
        if ((!primary && !hostile && !airborne) || shot.collisionRadius != (primary || airborne ? ordinaryPrimaryRadius : 441)
            || shot.stateBytes4dTo54[0] != ordinaryPrimaryCategory
            || shot.stateBytes4dTo54[1] < (damage.combatDomain == Level1PlayerCombatDomain::expiryAndLoss ? 1 : 2)
            || shot.stateBytes4dTo54[1] > (primary ? ordinaryPrimaryLifetime : airborne ? 60 : 54)
            || std::any_of(shot.stateBytes4dTo54.begin()+2,shot.stateBytes4dTo54.end(),[](auto value) { return value != 0; })
            || (primary && (!shot.ownerIsPlayer || shot.ownerAllocationIndex != std::numeric_limits<std::size_t>::max()))
            || shot.pitch != 0 || (!airborne && shot.roll != 0) || shot.pitchVelocity != 0 || shot.rollVelocity != 0 || shot.yawVelocity != 0
            || shot.xVelocity != 0 || shot.yVelocity != 0 || shot.zVelocity != 0
            || shot.forwardVelocity != ordinaryPrimarySpeed || shot.position.z < 0
            || shot.position.z > (airborne ? level1LiftCruiseAltitude : 0)) {
            throw std::invalid_argument("unsupported target-prefix ordinary projectile record");
        }
        requireVisiblePrefixPosition(shot.position,prefix.player.position);
    }
    std::vector<Level1ObjectRef> pending;
    for (const auto ref : prefix.allocationOrder) {
        if (!contains(prefix.activeOrder,ref)) { pending.push_back(ref); }
        else if (!pending.empty()) { throw std::invalid_argument("target-prefix pending appends require allocation suffix"); }
    }
    if (pending.size() > 2 || (!pending.empty() && !prefix.refreshRequested)
        || (pending.size() == 2 && (pending[0].kind != Level1PresentationKind::projectile
            || pending[1].kind != Level1PresentationKind::entity))) {
        throw std::invalid_argument("unsupported target-prefix pending append order");
    }
    for (const auto ref : pending) {
        if (ref.kind == Level1PresentationKind::projectile) {
            const auto& shot = prefix.projectiles[ref.index];
            if (shot.type != ordinaryPrimaryType || shot.stateBytes4dTo54[1] != ordinaryPrimaryLifetime
                || (enemy.allocationDomain == Level1ProjectileAllocationDomain::freshOnly
                    && (shot.presenterDistance != 0 || shot.targetHeading != 0))) {
                throw std::invalid_argument("unlisted target-prefix shot requires primary append ancestry");
            }
        } else if (ref.kind == Level1PresentationKind::entity) {
            const auto& actor = prefix.entities[ref.index];
            if ((!ordinaryStorageIndex(ref.index) && !(arrivalDomain && actor.type==7))
                || actor.stateBytes4dTo52[5] != 0 || actor.stateBytes4dTo52[3] != 0
                || actor.behaviorTimer != 0 || actor.modelAnimationVariant != 0 || actor.weaponCooldown != 0
                || actor.weaponSlotTimers != std::array<std::uint8_t,3>{}) {
                throw std::invalid_argument("unlisted target-prefix actor requires ordinary append ancestry");
            }
        } else { throw std::invalid_argument("unlisted FC append is outside retained target-prefix entry"); }
    }
}
}

namespace {
Level1BonusTargetPrefixWork targetPrefixPlan(const Level1TargetPrefixInput& input,
    const Level1TargetPrefixProfile& profile, const BitmapFont& scoreFont, const CockpitPageState& pages,
    const Model3d* bonusModel, const std::uint16_t firstBlockCapacity,
    BonusPickupExecution* pickup = nullptr,const Level1ArrivalPrefixContext* arrival = nullptr,
    const Level1DeploymentPrefixContext* deployment=nullptr,Level1DeploymentPrefixWork* deploymentOutput=nullptr,
    const PrefixExecutionStart start=PrefixExecutionStart::frameEntry) {
    const auto domain = deployment && deployment->reward ? TargetPrefixDomain::guardianReward : deployment ? TargetPrefixDomain::deployment : arrival ? TargetPrefixDomain::arrival : pickup ? TargetPrefixDomain::systemBonusPickup
        : bonusModel ? TargetPrefixDomain::systemBonus : TargetPrefixDomain::ordinary;
    validateTargetPrefixInput(input,profile,domain);
    const auto& prefix = input.prefix;
    Level1TargetPrefixWork result{{{{prefix.player,prefix.entities,prefix.projectiles,prefix.allocationOrder,
        prefix.activeOrder,prefix.lastListCamera,prefix.refreshRequested,prefix.rng,{}},{}},
        profile.damage.armor,prefix.fadeTicks,{}},input.effects,input.bonus,input.activeEnemies,input.score,{},{}};
    PlayerDamageContext damage{result.damage.armor,result.damage.fadeTicks,result.damage.paletteSteps,profile.damage.combatDomain};
    std::vector<Level1PrefixBonusBirth> births;
    TargetPrefixContext target{result,scoreFont,pages,profile.difficulty,profile.targetDomain,
        bonusModel,firstBlockCapacity,&births,pickup,arrival,deployment,deploymentOutput};
    EnemyFireContext enemy{result.damage.combat.prefix,profile.damage.enemy,result.damage.combat.audioEvents,&damage,&target};
    result.damage.combat.prefix.impulse=prefix.impulse;
    executeProjectilePrefix(result.damage.combat.prefix,&enemy,start);
    return {std::move(result),std::move(births)};
}
} // namespace

Level1TargetPrefixWork level1TargetPrefixOperations(const Level1TargetPrefixInput& input,
    const Level1TargetPrefixProfile& profile, const BitmapFont& scoreFont, const CockpitPageState& pages) {
    return targetPrefixPlan(input,profile,scoreFont,pages,nullptr,0).prefix;
}

Level1BonusTargetPrefixWork level1BonusTargetPrefixOperations(const Level1TargetPrefixInput& input,
    const Level1TargetPrefixProfile& profile, const BitmapFont& scoreFont, const CockpitPageState& pages,
    const std::uint16_t firstBlockCapacity, const Model3d& model42) {
    return targetPrefixPlan(input,profile,scoreFont,pages,&model42,firstBlockCapacity);
}

Level1BonusPickupPrefixWork level1BonusPickupPrefixOperations(const Level1TargetPrefixInput& input,
    const Level1TargetPrefixProfile& profile, const BitmapFont& scoreFont, const CockpitPageState& pages,
    const std::uint16_t firstBlockCapacity, const Model3d& model42, const Level1BonusPickupContext& context) {
    if (context.selectedSystem != -1
        || !std::all_of(context.systems.begin(),context.systems.end(),[](const auto value) { return value == 3; })
        || !std::all_of(context.timers.begin(),context.timers.end(),[](const auto value) { return value == 0; })) {
        throw std::invalid_argument("unsupported initial bonus pickup system state");
    }
    Level1BonusPickupPrefixWork result{{},context.systems,context.timers,context.selectedSystem,{},{},{}};
    BonusPickupExecution execution{context,result};
    result.bonus = targetPrefixPlan(input,profile,scoreFont,pages,&model42,firstBlockCapacity,&execution);
    return result;
}

KinematicsWork level1BonusFixedUpdateOperations(const Level1Entity& actor) {
    if (!validSystemBonus(actor)) {
        throw std::invalid_argument("unsupported Type42 source fixed-update profile");
    }
    auto result = level1KinematicsOperations(actor);
    result.operations.insert(result.operations.begin(),{NormalWorkKind::instructions,6});
    instructions(result.operations,3); //0995 JMP;097B cleanup/097E loop jump.
    return result;
}


Level1BonusPickupPrefixWork level1ArrivalPrefixOperations(const Level1TargetPrefixInput& input,
    const Level1TargetPrefixProfile& profile,const BitmapFont& scoreFont,const CockpitPageState& pages,
    const std::uint16_t firstBlockCapacity,const Model3d& model42,const Level1BonusPickupContext& pickup,
    const Level1ArrivalPrefixContext& arrival) {
    if(arrival.craftEntityIndex>=input.prefix.entities.size() || arrival.guardianEntityIndex
        || input.prefix.entities[arrival.craftEntityIndex].type!=7
        || std::count_if(input.prefix.entities.begin(),input.prefix.entities.end(),
            [](const auto& actor){return actor.type==7;})!=1
        || pickup.selectedSystem!=-1
        || !std::all_of(pickup.systems.begin(),pickup.systems.end(),[](auto v){return v==3;})
        || !std::all_of(pickup.timers.begin(),pickup.timers.end(),[](auto v){return v==0;})) {
        throw std::invalid_argument("unsupported arrival prefix owner/system state");
    }
    Level1BonusPickupPrefixWork result{{},pickup.systems,pickup.timers,pickup.selectedSystem,{},{},{}};
    BonusPickupExecution execution{pickup,result};
    result.bonus=targetPrefixPlan(input,profile,scoreFont,pages,&model42,firstBlockCapacity,&execution,&arrival);
    return result;
}

namespace {
enum class PreloopPlayerDomain { capturedType31, sourceReset };

Level2PreloopPrefixWork preloopPrefixOperations(
    const Level2PreloopPrefixInput& input,const PreloopPlayerDomain domain) {
    const auto& prefix = input.prefix;
    const auto& player = prefix.player;
    constexpr std::size_t actorCount = 7;
    constexpr std::array<std::int16_t, actorCount> types{8, 8, 8, 13, 13, 30, 30};
    const auto playerType = domain == PreloopPlayerDomain::sourceReset ? 0 : 31;
    if (prefix.actors.size() != actorCount || input.allocationOrder.size() != actorCount
        || !prefix.activeOrder.empty() || !prefix.refreshRequested
        || prefix.fadeTicks != 0 || prefix.specialActorPresent || prefix.impulse != 0
        || !prefix.active || prefix.projectileHandlePresent[0] || prefix.projectileHandlePresent[1]
        || player.type != playerType || player.collisionRadius != 723
        || player.stateBytes4dTo52[1] != 1 || !(player.position == FixedPosition{})
        || player.pitch != 0 || player.roll != 0 || player.yaw != 0
        || player.pitchVelocity != 0 || player.rollVelocity != 0 || player.yawVelocity != 0
        || player.xVelocity != 0 || player.yVelocity != 0 || player.zVelocity != 0
        || player.forwardVelocity != 0) {
        throw std::invalid_argument("unsupported first Level2 preloop prefix");
    }
    for (std::size_t index = 0; index < actorCount; ++index) {
        const auto& actor = prefix.actors[index];
        const auto fixed = actor.type == 30;
        const auto radius = actor.type == 8 ? 860U : actor.type == 13 ? 559U : 868U;
        const auto health = actor.type == 8 ? 2 : actor.type == 13 ? 1 : 100;
        if (input.allocationOrder[index] != index || actor.allocationIndex != index
            || actor.type != types[index] || actor.flags != 0 || actor.collisionRadius != radius
            || actor.stateBytes4dTo52[0] != (fixed ? 254 : 255)
            || actor.stateBytes4dTo52[1] != health
            || actor.stateBytes4dTo52[4] != (fixed ? 0 : actor.type == 8 ? 2 : 4)
            || actor.pitch != 0 || actor.roll != 0 || actor.position.z != 0) {
            throw std::invalid_argument("unsupported first Level2 constructor identity");
        }
        requireVisiblePrefixPosition(actor.position, player.position);
    }
    Level2PreloopPrefixWork result{player, prefix.actors, {}, prefix.lastListCamera,
        prefix.refreshRequested, {}, {prefix.lastListCamera, 0, 0}};
    instructions(result.operations, 7 + 7); //01D0 guards,0784 player kinematics CALL.
    const auto kinematics = flatKinematicsOperations(player);
    result.player = kinematics.actor;
    append(result.operations, kinematics.operations);
    instructions(result.operations, 4);
    auto storage = result.actors;
    storage.push_back(result.player);
    append(result.operations, actorContactOperations(storage, {}, actorCount).operations);
    instructions(result.operations, 9); //No contact and empty OLD actor loop.
    instructions(result.operations, 2 + 10 + 3 + 10 + 6); //Two null0A9A children,0784 RET.
    instructions(result.operations, 8); //Style1/back role/active/view/9298 CALL.
    instructions(result.operations, 12 + 5 + 1); //Empty9298,RET,14B0 CALL.
    level1SortOperations(result, true,
        [&result](const std::size_t index) { return result.actors[index].position; });
    instructions(result.operations, 10); //3768 arguments and CALL.
    result.visibility.operationBegin = result.operations.size();
    if (!level1RefreshOperations(result)) {
        throw std::logic_error("first Level2 prefix did not request its source rebuild");
    }
    level1RebuildOperations(result, input.allocationOrder,
        [&result](const std::size_t index) { return result.actors[index].position; });
    result.visibility.operationEnd = result.operations.size();
    instructions(result.operations, 5); //0291 cleanup/count store/equality branch to02A3.
    return result;
}
}

Level2PreloopPrefixWork level2PreloopPrefixOperations(const Level2PreloopPrefixInput& input) {
    return preloopPrefixOperations(input,PreloopPlayerDomain::capturedType31);
}

Level2PreloopPrefixWork level2ResetPreloopPrefixOperations(const Level2PreloopPrefixInput& input) {
    return preloopPrefixOperations(input,PreloopPlayerDomain::sourceReset);
}

} // namespace stellar7

namespace stellar7::detail {
std::uint16_t headingSourceOperations(const FixedPosition actor,const FixedPosition target,
    std::vector<NormalWorkOperation>& operations) { return headingOperations(actor,target,operations); }
}

namespace stellar7 {
Level1DeploymentPrefixWork level1DeploymentPrefixOperations(const Level1TargetPrefixInput& input,
    const Level1TargetPrefixProfile& profile,const BitmapFont& font,const CockpitPageState& pages,
    const std::uint16_t capacity,const Model3d& model42,const Level1BonusPickupContext& pickup,
    const Level1DeploymentPrefixContext& deployment) {
    if(deployment.reward || (!deployment.craftEntityIndex && !deployment.guardianEntityIndex) || deployment.models.size()<45
        || deployment.pool.spawnedEnemies>12 || pickup.selectedSystem!=-1
        || !std::all_of(pickup.systems.begin(),pickup.systems.end(),[](auto v){return v==3;})
        || !std::all_of(pickup.timers.begin(),pickup.timers.end(),[](auto v){return v==0;})) {
        throw std::invalid_argument("unsupported deployment prefix owner/profile");
    }
    Level1DeploymentPrefixWork result{{{},pickup.systems,pickup.timers,pickup.selectedSystem,{},{},{}},{},{}};
    BonusPickupExecution execution{pickup,result.prefix};
    result.prefix.bonus=targetPrefixPlan(input,profile,font,pages,&model42,capacity,&execution,nullptr,&deployment,&result);
    return result;
}
Level1DeploymentPrefixWork level1GuardianRewardPrefixOperations(const Level1TargetPrefixInput& input,
    const Level1TargetPrefixProfile& profile,const BitmapFont& font,const CockpitPageState& pages,
    const std::uint16_t capacity,const Model3d& model42,const Level1BonusPickupContext& pickup,
    const Level1DeploymentPrefixContext& deployment,const Level1GuardianRewardPrefixContext& reward) {
    if(deployment.reward || !deployment.guardianEntityIndex || deployment.models.size()<45
        || deployment.pool.spawnedEnemies>12 || capacity!=44 || reward.campaignIndex!=0
        || input.prefix.projectileHandlePresent[0]!=reward.rewardEntityIndex.has_value()
        || input.prefix.projectileHandlePresent[1] || pickup.selectedSystem!=-1
        || !std::all_of(pickup.systems.begin(),pickup.systems.end(),[](auto v){return v==3;})
        || !std::all_of(pickup.timers.begin(),pickup.timers.end(),[](auto v){return v==0;})) {
        throw std::invalid_argument("unsupported guardian reward prefix owner/profile");
    }
    const auto rewardCount=std::count_if(input.prefix.entities.begin(),input.prefix.entities.end(),
        [](const auto& actor){return actor.type==25;});
    if(rewardCount!=(reward.rewardEntityIndex?1:0)
        || (reward.rewardEntityIndex && (*reward.rewardEntityIndex>=input.prefix.entities.size()
            || input.prefix.entities[*reward.rewardEntityIndex].type!=25))) {
        throw std::invalid_argument("reward prefix128E pointer/storage mismatch");
    }
    auto context=deployment;context.reward=&reward;
    Level1DeploymentPrefixWork result{};
    result.prefix=Level1BonusPickupPrefixWork{{},pickup.systems,pickup.timers,pickup.selectedSystem,{},{},{}};
    result.rewardEntityIndex=reward.rewardEntityIndex;
    BonusPickupExecution execution{pickup,result.prefix};
    result.prefix.bonus=targetPrefixPlan(input,profile,font,pages,&model42,capacity,&execution,nullptr,&context,&result);
    return result;
}
Level1DeploymentPrefixWork level1GuardianRewardResumeOperations(const Level1TargetPrefixInput& postChild,
    const Level1TargetPrefixProfile& profile,const BitmapFont& font,const CockpitPageState& pages,
    const std::uint16_t capacity,const Model3d& model42,const Level1BonusPickupContext& pickup,
    const Level1DeploymentPrefixContext& deployment,const Level1GuardianRewardPrefixContext& reward,
    const Level1RewardSuccessPause& pause) {
    if(deployment.reward || !deployment.guardianEntityIndex || deployment.models.size()<45
        || deployment.pool.spawnedEnemies>12 || capacity!=44 || reward.campaignIndex!=0
        || !reward.rewardEntityIndex || *reward.rewardEntityIndex!=pause.rewardEntityIndex
        || pause.selectedEntityIndex!=pause.rewardEntityIndex || pause.sourceResume!=0x0809 || pause.oldListCursor!=0
        || pause.incomingLoopTick!=profile.damage.enemy.loopTick
        || pause.rewardEntityIndex>=postChild.prefix.entities.size()
        || postChild.prefix.entities[pause.rewardEntityIndex].type!=25
        || !postChild.prefix.projectileHandlePresent[0] || postChild.prefix.projectileHandlePresent[1]
        || pickup.selectedSystem!=-1
        || !std::all_of(pickup.systems.begin(),pickup.systems.end(),[](auto v){return v==3;})
        || !std::all_of(pickup.timers.begin(),pickup.timers.end(),[](auto v){return v==0;})) {
        throw std::invalid_argument("unsupported Type25 source success-return cursor/profile");
    }
    auto context=deployment;context.reward=&reward;
    Level1DeploymentPrefixWork result{};
    result.prefix=Level1BonusPickupPrefixWork{{},pickup.systems,pickup.timers,pickup.selectedSystem,{},{},{}};
    result.rewardEntityIndex=reward.rewardEntityIndex;
    BonusPickupExecution execution{pickup,result.prefix};
    result.prefix.bonus=targetPrefixPlan(postChild,profile,font,pages,&model42,capacity,&execution,nullptr,&context,&result,
        PrefixExecutionStart::successReturn);
    return result;
}
}

namespace stellar7 {
Level1TargetPrefixWork level1GuardianProjectileUpdateOperations(const Level1TargetPrefixInput& input,
    const Level1TargetPrefixProfile& profile,const std::size_t slot,const Level1DeploymentPrefixContext& deployment) {
    if(slot>=input.prefix.projectiles.size() || input.prefix.projectiles[slot].type!=22
        || !input.prefix.projectiles[slot].active() || input.prefix.projectiles[slot].stateBytes4dTo54[0]!=253
        || input.prefix.projectiles[slot].collisionRadius!=134 || !deployment.guardianEntityIndex
        || input.prefix.projectiles[slot].ownerAllocationIndex!=*deployment.guardianEntityIndex
        || input.prefix.entities.at(*deployment.guardianEntityIndex).type!=44
        || profile.damage.missionComplete || profile.damage.shieldTicks || profile.damage.abilityTicks
        || profile.damage.armor<0 || profile.damage.armor>680
        || profile.damage.paletteMode!=Level1DamagePaletteMode::residentVga) {
        throw std::invalid_argument("unsupported Type22 update component ABI");
    }
    const auto& p=input.prefix;
    Level1TargetPrefixWork result{{{{p.player,p.entities,p.projectiles,p.allocationOrder,p.activeOrder,
        p.lastListCamera,p.refreshRequested,p.rng,{}},{}},profile.damage.armor,p.fadeTicks,{}},
        input.effects,input.bonus,input.activeEnemies,input.score,{},{}};
    const BitmapFont font{};const CockpitPageState pages{};Level1DeploymentPrefixWork output{};
    PlayerDamageContext damage{result.damage.armor,result.damage.fadeTicks,result.damage.paletteSteps,profile.damage.combatDomain};
    TargetPrefixContext target{result,font,pages,0,Level1TargetHitDomain::initialOrdinary,nullptr,0,nullptr,nullptr,nullptr,&deployment,&output};
    EnemyFireContext enemy{result.damage.combat.prefix,profile.damage.enemy,result.damage.combat.audioEvents,&damage,&target};
    primaryUpdateOperations(result.damage.combat.prefix,result.damage.combat.prefix.projectiles[slot],&enemy);return result;
}
}


namespace stellar7 {
namespace {
void validateLevel2ProjectilePrefix(const Level1TargetPrefixInput& input,
    const Level2ProjectilePrefixProfile& profile) {
    const auto& prefix = input.prefix;
    const auto& player = prefix.player;
    if (profile.hostile && (profile.hostile->firstRegistryCapacity!=Level2HostileProjectileModel::sharedCount
        || profile.hostile->model19.scaleShift!=0 || profile.hostile->model19.baseRadius!=Level2HostileProjectileModel::radius)) {
        throw std::invalid_argument("Level2 hostile prefix requires decoded shared model19");
    }
    const auto lifecycle = profile.combatDomain==Level1PlayerCombatDomain::expiryAndLoss;
    if (!lifecycle && profile.combatDomain!=Level1PlayerCombatDomain::survivingOnly) {
        throw std::invalid_argument("unsupported Level2 player combat domain");
    }
    constexpr std::size_t constructorCount = 7;
    const auto periodic = profile.periodic.has_value();
    const auto maximumAllocations = periodic ? 32U : constructorCount + Level1State::projectilePoolSize
        + Level1State::deathEffectPoolSize;
    if ((periodic ? prefix.entities.size()<2 || prefix.entities.size()>18 : prefix.entities.size()!=constructorCount)
        || prefix.allocationOrder.size() > maximumAllocations
        || prefix.activeOrder.empty() || prefix.activeOrder.size() > maximumAllocations
        || prefix.fadeTicks > 8 || prefix.specialActorPresent
        || (periodic ? prefix.impulse < -sourceForwardLimit200 || prefix.impulse > sourceForwardLimit200 : prefix.impulse != 0) || !prefix.active
        || prefix.projectileHandlePresent[0] || prefix.projectileHandlePresent[1]
        || profile.armor < (lifecycle ? 0 : 200) || profile.armor > 680 || profile.difficulty != 0
        || player.pitch != 0 || player.roll != 0 || player.pitchVelocity != 0 || player.rollVelocity != 0
        || player.xVelocity != 0 || player.yVelocity != 0 || player.zVelocity != 0
        || player.yawVelocity < -sourceYawLimit512 || player.yawVelocity > sourceYawLimit512
        || player.forwardVelocity < -sourceForwardLimit200 || player.forwardVelocity > sourceForwardLimit200
        || (player.forwardVelocity != 0 && player.position.z != 0)) {
        throw std::invalid_argument("unsupported Level2 projectile prefix profile");
    }
    const auto contains = [](const auto& list, const Level1ObjectRef ref) {
        return std::find(list.begin(),list.end(),ref) != list.end();
    };
    const auto valid = [&](const Level1ObjectRef ref) {
        if (ref.kind == Level1PresentationKind::entity) { return ref.index < prefix.entities.size(); }
        if (ref.kind == Level1PresentationKind::projectile) { return ref.index < prefix.projectiles.size(); }
        return ref.kind == Level1PresentationKind::deathEffect && ref.index < input.effects.size();
    };
    const auto live = [&](const Level1ObjectRef ref) {
        if (ref.kind == Level1PresentationKind::entity) { return prefix.entities[ref.index].active(); }
        if (ref.kind == Level1PresentationKind::projectile) { return prefix.projectiles[ref.index].active(); }
        return input.effects[ref.index].listed;
    };
    for (std::size_t index = 0; index < prefix.allocationOrder.size(); ++index) {
        const auto ref = prefix.allocationOrder[index];
        if (!valid(ref) || !live(ref)
            || std::find(prefix.allocationOrder.begin(),prefix.allocationOrder.begin()+index,ref)
                != prefix.allocationOrder.begin()+index) {
            throw std::invalid_argument("invalid Level2 live allocation identity");
        }
    }
    for (std::size_t index = 0; index < prefix.activeOrder.size(); ++index) {
        const auto ref = prefix.activeOrder[index];
        if (!valid(ref) || (!contains(prefix.allocationOrder,ref) && !prefix.refreshRequested)
            || (ref.kind == Level1PresentationKind::projectile && !prefix.projectiles[ref.index].active())
            || std::find(prefix.activeOrder.begin(),prefix.activeOrder.begin()+index,ref)
                != prefix.activeOrder.begin()+index) {
            throw std::invalid_argument("invalid Level2 OLD identity");
        }
    }
    if (periodic) {
        const auto& descriptor=*profile.periodic;
        const auto validModel=[](const auto& model,const std::uint32_t radius) {
            return model.scaleShift==0 && model.baseRadius==radius;
        };
        if (descriptor.firstRegistryCapacity!=44 || descriptor.physicalKeys.size()!=prefix.entities.size()
            || !validModel(descriptor.model1,1342) || !validModel(descriptor.model4,928)
            || !validModel(descriptor.model17,441) || !validModel(descriptor.model20,134)
            || prefix.rng.indexA()>=StellarRng::wordCount*2 || prefix.rng.indexB()>=StellarRng::wordCount*2
            || prefix.rng.indexA()%2 || prefix.rng.indexB()%2) {
            throw std::invalid_argument("Level2 periodic prefix requires actual shared descriptors and physical aliases");
        }
    }
    std::array<std::size_t,3> typeCounts{};
    std::array<bool,18> selectedPhysical{};
    std::int16_t activeEnemies = 0;
    std::size_t pendingOrdinary = 0,liveLifts = 0;
    constexpr std::int32_t maximumLiftAltitude=1000;
    for (std::size_t index = 0; index < prefix.entities.size(); ++index) {
        const auto& actor = prefix.entities[index];
        const auto tank = actor.type == 8;
        const auto lift = actor.type == 13;
        const auto fixed = actor.type == 30;
        const auto class1 = periodic && (actor.type==1 || actor.type==4);
        if ((!tank && !lift && !fixed && !class1) || actor.allocationIndex != index
            || actor.stateBytes4dTo52[0] != (fixed ? 254 : 255)
            || actor.stateBytes4dTo52[4] != (class1 ? 1 : tank ? 2 : lift ? 4 : 0)
            || actor.stateBytes4dTo52[1] > 127 || (actor.flags != 0 && actor.flags != 4)
            || contains(prefix.allocationOrder,{Level1PresentationKind::entity,index}) != actor.active()) {
            throw std::invalid_argument("invalid Level2 ordinary/fixed storage");
        }
        if (periodic) {
            const auto key=profile.periodic->physicalKeys[index];
            const auto radius=actor.type==1 ? profile.periodic->model1.baseRadius
                : actor.type==4 ? profile.periodic->model4.baseRadius : tank ? 860U : lift ? 559U : 868U;
            if (key>=selectedPhysical.size() || selectedPhysical[key] || (fixed ? key<16 : key>=16)
                || actor.collisionRadius!=radius || actor.renderProfile!=WorldActorRenderProfile::noHandles
                || actor.animatedModelPresenterEnabled || actor.primaryType11AnimationSelector
                || (lift ? actor.position.z<0 || actor.position.z>maximumLiftAltitude || actor.zVelocity!=0 : actor.position.z!=0)
                || actor.pitch!=0 || actor.roll!=0 || actor.pitchVelocity!=0 || actor.rollVelocity!=0
                || (class1 && (actor.stateBytes4dTo52[5]>5 || actor.behaviorTimer>127
                    || actor.xVelocity!=0 || actor.yVelocity!=0 || actor.zVelocity!=0
                    || actor.stateBytes4dTo52[1]>(actor.type==1 ? 1 : 2)
                    || actor.weaponSlotCount>(actor.type==1 ? 2 : 1)
                    || actor.weaponCooldown>(actor.type==1 ? 8 : 0)
                    || actor.yawVelocity < -(actor.type==1 ? 256 : 512) || actor.yawVelocity > (actor.type==1 ? 256 : 512)
                    || (actor.forwardVelocity!=0 && actor.forwardVelocity!=(actor.type==1 ? 225 : 180)
                        && actor.forwardVelocity!=-(actor.type==1 ? 45 : 90))))) {
                throw std::invalid_argument("unsupported Level2 periodic physical actor projection");
            }
            selectedPhysical[key]=true;
            if (lift && actor.active()) {++liveLifts;}
            if (!fixed && actor.active() && !contains(prefix.activeOrder,{Level1PresentationKind::entity,index})) {
                const auto defaults=campaignOrdinaryDefaults(static_cast<std::uint8_t>(actor.type));
                const auto spin=std::abs(static_cast<int>(actor.yawVelocity));
                if (++pendingOrdinary!=1 || !prefix.refreshRequested || prefix.allocationOrder.empty()
                    || prefix.allocationOrder.back()!=Level1ObjectRef{Level1PresentationKind::entity,index}
                    || actor.flags!=0 || actor.position.z!=0 || actor.stateBytes4dTo52[1]!=defaults.health
                    || actor.stateBytes4dTo52[2]!=0 || actor.stateBytes4dTo52[3]!=0 || actor.stateBytes4dTo52[5]!=0
                    || actor.behaviorTimer!=0 || actor.weaponCooldown!=0 || actor.weaponSlotTimers!=std::array<std::uint8_t,3>{}
                    || actor.weaponSlotCount!=defaults.weaponSlots || actor.forwardVelocity!=defaults.speed.value_or(0)
                    || actor.xVelocity!=0 || actor.yVelocity!=0 || actor.zVelocity!=0
                    || (actor.yaw!=0 && actor.yaw!=0x8000)
                    || (lift ? spin<1792 || spin>2560 || spin%256 : actor.yawVelocity!=0)
                    || actor.collisionContactIsPlayer || actor.collisionContactAllocationIndex!=std::numeric_limits<std::size_t>::max()) {
                    throw std::invalid_argument("unlisted Level2 ordinary requires fresh4370 append ancestry");
                }
            }
        }
        if (profile.hostile && tank && (actor.position.z!=0 || actor.pitch!=0 || actor.roll!=0
            || actor.pitchVelocity!=0 || actor.rollVelocity!=0 || actor.weaponSlotCount>2
            || actor.weaponCooldown>Level2HostileProjectileModel::cooldown)) {
            throw std::invalid_argument("unsupported Level2 Type8 hostile owner");
        }
        ++typeCounts[tank || class1 ? 0 : lift ? 1 : 2];
        if (!fixed && actor.active()) { ++activeEnemies; }
        requireVisiblePrefixPosition(actor.position,player.position);
    }
    if ((periodic ? typeCounts[0]+typeCounts[1]>16 || typeCounts[2]!=2 || liveLifts>maximumLevel2PeriodicLiveType13 || activeEnemies>12
            : typeCounts != std::array<std::size_t,3>{3,2,2}) || input.activeEnemies != activeEnemies) {
        throw std::invalid_argument("Level2 ordinary inventory/counter mismatch");
    }
    std::size_t pending = 0;
    for (std::size_t slot = 0; slot < prefix.projectiles.size(); ++slot) {
        const auto& shot = prefix.projectiles[slot];
        const Level1ObjectRef ref{Level1PresentationKind::projectile,slot};
        if (shot.poolSlot != slot || contains(prefix.allocationOrder,ref) != shot.active()) {
            throw std::invalid_argument("Level2 FD pool identity mismatch");
        }
        if (!shot.active()) { continue; }
        const auto primary = shot.type==ordinaryPrimaryType && shot.collisionRadius==ordinaryPrimaryRadius
            && shot.ownerIsPlayer && shot.ownerAllocationIndex==std::numeric_limits<std::size_t>::max()
            && shot.stateBytes4dTo54[1]<=ordinaryPrimaryLifetime;
        const auto owner = shot.ownerAllocationIndex;
        const auto physicalOwner=owner<prefix.entities.size() && prefix.entities[owner].allocationIndex==owner
            && prefix.entities[owner].stateBytes4dTo52[0]==255
            && (periodic ? profile.periodic->physicalKeys[owner]<16
                : prefix.entities[owner].type==8 && prefix.entities[owner].stateBytes4dTo52[4]==2);
        const auto tankShot=periodic && (shot.type==17 || shot.type==20);
        const auto radius=shot.type==20 ? 134U : 441U;
        const auto life=shot.type==17 ? 54U : shot.type==20 ? 60U : Level2HostileProjectileModel::lifetime;
        const auto hostile = ((profile.hostile && shot.type==Level2HostileProjectileModel::type) || tankShot)
            && shot.collisionRadius==radius && !shot.ownerIsPlayer && shot.stateBytes4dTo54[1]<=life
            && physicalOwner && shot.position.z==0 && shot.zVelocity==0;
        if ((!primary && !hostile) || shot.stateBytes4dTo54[0]!=ordinaryPrimaryCategory
            || shot.stateBytes4dTo54[2]!=0 || shot.stateBytes4dTo54[4]!=0
            || (shot.flags!=0 && shot.flags!=4) || shot.forwardVelocity!=ordinaryPrimarySpeed
            || shot.pitch!=0 || shot.roll!=0 || shot.pitchVelocity!=0 || shot.rollVelocity!=0) {
            throw std::invalid_argument("unsupported Level2 FD physical record");
        }
        requireVisiblePrefixPosition(shot.position,player.position);
        if (!contains(prefix.activeOrder,ref)
            && (++pending != 1 || !primary || !prefix.refreshRequested || shot.stateBytes4dTo54[1] != ordinaryPrimaryLifetime
                || prefix.allocationOrder.back() != ref)) {
            throw std::invalid_argument("unlisted Level2 FD requires the pending primary append");
        }
    }
    for (std::size_t slot = 0; slot < input.effects.size(); ++slot) {
        const auto& effect = input.effects[slot];
        const auto& bytes = effect.entity.stateBytes4dTo52;
        const Level1ObjectRef ref{Level1PresentationKind::deathEffect,slot};
        if (contains(prefix.allocationOrder,ref) != effect.listed
            || (effect.listed && (effect.entity.type != -1 || bytes[0] != 252 || bytes[1] == 0
                || bytes[1] > bytes[4] || bytes[4] > 32))
            || (!effect.listed && bytes[1] != 0)
            || (contains(prefix.activeOrder,ref) && !effect.listed
                && (effect.entity.type != -1 || bytes[0] != 252 || bytes[4] != 0))) {
            throw std::invalid_argument("invalid Level2 FC physical record");
        }
        if (effect.listed || contains(prefix.activeOrder,ref)) {
            requireVisiblePrefixPosition(effect.entity.position,player.position);
        }
    }
}
} // namespace

Level1TargetPrefixWork level2ProjectilePrefixOperations(const Level1TargetPrefixInput& input,
    const Level2ProjectilePrefixProfile& profile, const BitmapFont& scoreFont,
    const CockpitPageState& pages) {
    validateLevel2ProjectilePrefix(input,profile);
    const auto& prefix = input.prefix;
    Level1TargetPrefixWork result{{{{prefix.player,prefix.entities,prefix.projectiles,prefix.allocationOrder,
        prefix.activeOrder,prefix.lastListCamera,prefix.refreshRequested,prefix.rng,{}},{}},
        profile.armor,prefix.fadeTicks,{}},input.effects,input.bonus,input.activeEnemies,input.score,{},{}};
    if (profile.periodic) {result.damage.combat.prefix.impulse=prefix.impulse;}
    //Level2 metadata stays separate from the Level1 membership/allocation profile.
    Level1EnemyPrefixProfile sourceEnemy{};
    sourceEnemy.loopTick = profile.loopTick;
    sourceEnemy.contactDomain = Level1ActorContactDomain::ordinaryActors;
    TargetPrefixContext target{result,scoreFont,pages,profile.difficulty,profile.periodic
        ? Level1TargetHitDomain::level2PeriodicOrdinary : Level1TargetHitDomain::level2Constructor};
    PlayerDamageContext damage{result.damage.armor,result.damage.fadeTicks,result.damage.paletteSteps,profile.combatDomain};
    EnemyFireContext enemy{result.damage.combat.prefix,sourceEnemy,result.damage.combat.audioEvents,&damage,&target,
        profile.hostile ? &*profile.hostile : nullptr,profile.periodic ? &*profile.periodic : nullptr};
    executeProjectilePrefix(result.damage.combat.prefix,&enemy,PrefixExecutionStart::frameEntry,
        PrefixCampaignDomain::level2Constructor);
    //Keep the qualified signed-HUD result domain for the expanded periodic roster.
    if (result.damage.armor < -level2MaximumFrameDamage) {
        throw std::logic_error("Level2 damage exceeds the admitted OLD inventory");
    }
    return result;
}
} // namespace stellar7
