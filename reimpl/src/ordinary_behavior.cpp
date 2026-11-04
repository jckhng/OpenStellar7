#include "ordinary_behavior.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace stellar7 {
namespace {

constexpr std::uint32_t attackDistance = 0x6000U;
enum class TankRange { legacyAltitude, cachedDistance };
struct TankProfile {
    std::uint8_t choice2Lower, choice3Lower;
    std::int16_t cruiseSpeed, reverseSpeed, turnFactor;
    std::uint8_t behavior2Reset, behavior3Reset;
    std::uint16_t aggressiveLevelNumber;
    TankRange range;
};
constexpr TankProfile type5Profile{16,110,135,67,2,4,4,5,TankRange::legacyAltitude};
constexpr TankProfile type6Profile{48,128,180,81,3,4,3,6,TankRange::cachedDistance};
constexpr std::uint8_t cloakedFlag = 0x80;
constexpr std::uint16_t cloakCycleTicks = 200;
constexpr std::int16_t cloakStartBlinkThreshold = 195;
constexpr std::int16_t cloakEndBlinkThreshold = 5;

bool withinAttackRange(const Level1Entity& actor) {
    constexpr auto maximumSignedDistance = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
    return actor.presenterDistance <= attackDistance || actor.presenterDistance > maximumSignedDistance;
}

void aimAtPlayer(Level1Entity& actor, const std::int32_t turnLimit = 143) {
    const auto difference = static_cast<std::int16_t>(actor.targetHeading - actor.yaw);
    actor.yawVelocity = static_cast<std::int16_t>(
        std::clamp<std::int32_t>(difference, -turnLimit, turnLimit));
}

void chooseCruiseTurn(Level1Entity& actor, StellarRng& rng, const TankProfile& profile) {
    actor.forwardVelocity = profile.cruiseSpeed;
    const auto choice = rng.next();
    if (static_cast<std::uint8_t>(choice) < 90) {
        actor.yawVelocity = 0;
    } else {
        auto turn = static_cast<std::int16_t>(
            static_cast<std::int16_t>(choice)
                % (profile.turnFactor << 7)
            + 256
        );
        if ((choice & 0x0100U) != 0) {
            turn = static_cast<std::int16_t>(-turn);
        }
        actor.yawVelocity = turn;
    }
    actor.behaviorTimer = static_cast<std::uint8_t>(
        (choice >> 8U) & 0x7FU
    );
}

}  // namespace

static void advanceTankBehavior(Level1Entity& actor, StellarRng& rng, const std::uint16_t levelNumber,
    const TankProfile& profile,
    const std::function<bool(Level1Entity&)>& fire) {
    auto behavior = actor.stateBytes4dTo52[5];
    const auto insideAttackRange = profile.range == TankRange::cachedDistance ? withinAttackRange(actor)
        : actor.position.z < 0 || actor.presenterDistance <= attackDistance;
    const auto turnLimit = profile.turnFactor * 128 + 15;
    if (behavior == 0) {
        actor.behaviorTimer = 0;
        const auto choice = static_cast<std::uint8_t>(rng.next());
        behavior = choice < profile.choice2Lower
            ? 1
            : (choice < profile.choice3Lower ? 2 : 3);
    }
    if (behavior == 1) {
        if (actor.behaviorTimer == 0) {
            chooseCruiseTurn(actor, rng, profile);
        } else {
            --actor.behaviorTimer;
            if (actor.behaviorTimer == 0) {
                behavior = 0;
            }
            if (insideAttackRange) {
                fire(actor);
            }
        }
    } else if (behavior == 2) {
        actor.forwardVelocity = 0;
        const auto difference = static_cast<std::int16_t>(
            actor.targetHeading - actor.yaw
        );
        actor.yawVelocity = static_cast<std::int16_t>(
            std::clamp<std::int32_t>(difference, -turnLimit, turnLimit)
        );
        if (!insideAttackRange || fire(actor)) {
            behavior = 0;
        }
        if (static_cast<std::uint8_t>(rng.next()) < profile.behavior2Reset) {
            behavior = 0;
        }
    } else if (behavior == 3) {
        const auto difference = static_cast<std::int16_t>(
            actor.targetHeading - actor.yaw
        );
        actor.yawVelocity = static_cast<std::int16_t>(
            std::clamp<std::int32_t>(difference, -turnLimit, turnLimit)
        );
        if (insideAttackRange) {
            fire(actor);
        }
        const auto resetThreshold = levelNumber >= profile.aggressiveLevelNumber
            ? profile.behavior3Reset * 2 : profile.behavior3Reset;
        if (static_cast<std::uint8_t>(rng.next()) < resetThreshold) {
            behavior = 0;
        }
    } else if (behavior == 4) {
        actor.collisionContactAllocationIndex = static_cast<std::size_t>(-1);
        actor.collisionContactIsPlayer = false;
        if (actor.behaviorTimer == 0) {
            const auto choice = rng.next();
            actor.forwardVelocity = actor.forwardVelocity < 1
                ? profile.cruiseSpeed
                : static_cast<std::int16_t>(-profile.reverseSpeed);
            const auto turn = static_cast<std::int16_t>(profile.turnFactor << 8);
            actor.yawVelocity = (choice & 0x0200U) != 0
                ? static_cast<std::int16_t>(-turn)
                : turn;
            actor.behaviorTimer = 0x3F;
        } else {
            --actor.behaviorTimer;
            if (actor.behaviorTimer == 0) {
                behavior = 0;
                actor.forwardVelocity = profile.cruiseSpeed;
            }
            if (insideAttackRange) {
                fire(actor);
            }
        }
    } else if (behavior == 5) {
        chooseCruiseTurn(actor, rng, profile);
    }
    actor.stateBytes4dTo52[5] = behavior;
}

void advanceType5Behavior(Level1Entity& actor, StellarRng& rng, const std::uint16_t levelNumber,
    const std::function<bool(Level1Entity&)>& fire) {
    advanceTankBehavior(actor, rng, levelNumber, type5Profile, fire);
}

void advanceType8Behavior(Level1Entity& actor,
    const std::function<bool(Level1Entity&)>& fire) {
    aimAtPlayer(actor);
    if (actor.position.z < 0 || actor.presenterDistance <= attackDistance) {
        fire(actor);
    }
}

void advanceType9Behavior(Level1Entity& actor,
    const std::function<bool(Level1Entity&)>& fire) {
    //98DC tests the signed cached distance before steering or98FA's cooldown.
    if (!withinAttackRange(actor)) {
        return;
    }

    aimAtPlayer(actor);
    fire(actor);
}

void advanceType6Behavior(Level1Entity& actor, StellarRng& rng,
    const Type6BehaviorContext context, const std::function<bool(Level1Entity&)>& fire) {
    if (actor.stateBytes4dTo52[5] == 0) {
        actor.behaviorTimer = 0;
        const auto choice = static_cast<std::uint8_t>(rng.next());
        actor.stateBytes4dTo52[5] = choice < type6Profile.choice2Lower
            ? 1 : choice < type6Profile.choice3Lower ? 2 : 3;
    }

    actor.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
    actor.animatedModelPresenterEnabled = false;
    if (context.cloakTimer == 0) {
        context.cloakTimer = cloakCycleTicks;
    } else {
        const auto signedTimer = context.cloakTimer <= std::numeric_limits<std::int16_t>::max()
            ? static_cast<std::int32_t>(context.cloakTimer) : static_cast<std::int32_t>(context.cloakTimer) - 0x10000;
        if (signedTimer > cloakStartBlinkThreshold || signedTimer < cloakEndBlinkThreshold) {
            if (context.scanner == Type6ScannerState::inactive) {
                actor.flags ^= cloakedFlag;
                actor.stateBytes4dTo52[2] = 0;
            } else {
                actor.flags &= static_cast<std::uint8_t>(~cloakedFlag);
                actor.stateBytes4dTo52[2] ^= 1;
            }
        }
        --context.cloakTimer;
    }

    advanceTankBehavior(actor, rng, static_cast<std::uint16_t>(context.levelIndex + 1), type6Profile, fire);
}

void applyType6Scanner(Level1Entity& actor, const Type6ScannerState scanner) {
    if (actor.type != 6) {
        return;
    }
    if (scanner == Type6ScannerState::active) {
        actor.flags &= static_cast<std::uint8_t>(~cloakedFlag);
        actor.stateBytes4dTo52[2] = 1;
    } else {
        actor.flags |= cloakedFlag;
        actor.stateBytes4dTo52[2] = 0;
    }
}

}  // namespace stellar7
