#pragma once

#include "stellar7/normal_clock.hpp"
#include "stellar7/resource_codec.hpp"

#include <optional>

namespace stellar7 {

// Stable fragment lifetime identity; never a DOS offset or a second model owner.
struct FailureHeapIdentity {
    std::uint64_t generation;
    std::uint16_t slot;
};

enum class FailureHeapProviderKind { allocate, release };

struct FailureHeapProviderCall {
    FailureHeapProviderKind kind;
    FailureHeapIdentity identity;
    std::uint16_t requestedBytes;
    std::uint16_t sourceEntry;
    bool guestElapsedQualified;
};

struct FailureHeapWork {
    std::vector<NormalWorkOperation> beforeProvider;
    FailureHeapProviderCall provider;
    std::vector<NormalWorkOperation> afterProvider;
    std::optional<std::uint32_t> collisionRadius;
};

//39EE/3956/C05C/D03C, excluding positiveDCC6 CRT provider and caller7244 CALL.
// Provider returns stable non-null native storage or terminates on failure.
// The source-owned REP99 clear follows the provider before type/radius stores.
FailureHeapWork failureDebrisCreateOperations(FailureHeapIdentity identity,
    std::int16_t type, std::uint16_t firstBlockCapacity, const Model3d& model);

//398A, excludingDCCA CRT provider and caller74FE CALL. Includes actualDCC6(FFFF)
// size rejection. Release each owned identity once in creation order; source
// allocation/presenter list aliases remain stale until their ordinary rebuild.
FailureHeapWork failureDebrisReleaseOperations(FailureHeapIdentity identity);

} // namespace stellar7
