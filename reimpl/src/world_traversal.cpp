#include "stellar7/world_traversal.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {

WorldModelCallerWork worldModelCallerOperations(const FixedPosition center,
    const std::uint32_t collisionRadius, const std::uint8_t flags, const std::uint16_t callback) {
    return worldModelCallerOperations(center, collisionRadius, flags, callback, WorldSourceFlagsDomain::ordinary);
}

WorldModelCallerWork worldModelCallerOperations(const FixedPosition center,
    const std::uint32_t collisionRadius, const std::uint8_t flags, const std::uint16_t callback,
    const WorldSourceFlagsDomain domain) {
    constexpr std::int64_t nearPlane = 128;
    constexpr std::int64_t projectionScale = 128;
    constexpr std::int64_t shortDivideLimit = 32768;
    constexpr std::uint16_t maximumRadius = 256;
    const auto depth = static_cast<std::int64_t>(center.y) - collisionRadius;
    const auto minimumDepth = std::max(nearPlane, depth);
    if ((domain != WorldSourceFlagsDomain::ordinary && domain != WorldSourceFlagsDomain::flagsWithoutCallback)
        || (flags != 0 && (domain != WorldSourceFlagsDomain::flagsWithoutCallback || flags != 4))
        || callback != 0 || collisionRadius == 0 || collisionRadius >= shortDivideLimit
        || minimumDepth >= shortDivideLimit || depth < -0x80000000LL) {
        throw std::invalid_argument("unsupported world model caller path");
    }

    // Signed high-word comparison skips the low-word test for negative depths.
    constexpr std::uint64_t depthPrefix = 22;
    const std::uint64_t depthBranch = depth < 0 ? 4 : depth < nearPlane ? 6 : 3;
    constexpr std::uint64_t radiusCall = 5;
    constexpr std::uint64_t radiusBody = 111; // 4b57 + 5413 + 5392, each once.
    constexpr std::uint64_t modelCallSuffix = 17;
    const auto radius = static_cast<std::uint64_t>(collisionRadius) * projectionScale / minimumDepth;
    const std::uint64_t radiusClamp = radius > maximumRadius ? 2 : 0;
    return {static_cast<std::int32_t>(minimumDepth),
        static_cast<std::uint16_t>(std::min<std::uint64_t>(maximumRadius, radius)),
        {{NormalWorkKind::instructions, depthPrefix + depthBranch + radiusCall},
         {NormalWorkKind::instructions, radiusBody},
         {NormalWorkKind::instructions, modelCallSuffix + radiusClamp}}};
}

std::vector<NormalWorkOperation> worldModelReturnOperations(const WorldActorContinuation continuation) {
    constexpr std::uint64_t modelCleanup = 4;
    constexpr std::uint64_t listAdvance = 5;
    constexpr std::uint64_t nextActorJump = 1;
    constexpr std::uint64_t worldReturn = 7;
    return {{NormalWorkKind::instructions, modelCleanup + listAdvance
        + (continuation == WorldActorContinuation::nextActor ? nextActorJump : worldReturn)}};
}

WorldFirstCenterWork worldNextCenterOperations(const ViewTransform& view, const WorldActorWorkInput& actor) {
    return worldNextCenterOperations(view, actor, WorldSourceFlagsDomain::ordinary);
}

WorldFirstCenterWork worldNextCenterOperations(const ViewTransform& view, const WorldActorWorkInput& actor,
    const WorldSourceFlagsDomain domain) {
    auto result = worldFirstCenterOperations(view, actor, domain);
    constexpr std::uint64_t firstActorSetup = 8;
    result.operations.front().count -= firstActorSetup;
    return result;
}

}
