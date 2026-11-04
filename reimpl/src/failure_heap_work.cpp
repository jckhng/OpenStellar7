#include "stellar7/failure_heap_work.hpp"

#include <array>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::uint16_t fragmentCapacity = 30;
constexpr std::uint16_t fragmentBytes = 99;
constexpr std::int16_t firstFragmentType = 36;
constexpr std::int16_t lastFragmentType = 40;
constexpr std::uint16_t sharedModelCapacity = 44;
constexpr std::array<std::uint16_t,5> fragmentRadii{171,107,88,107,157};

void validate(const FailureHeapIdentity identity) {
    if (identity.slot >= fragmentCapacity) {
        throw std::invalid_argument("unsupported failure fragment identity");
    }
}
}

FailureHeapWork failureDebrisCreateOperations(const FailureHeapIdentity identity,
    const std::int16_t type, const std::uint16_t firstBlockCapacity, const Model3d& model) {
    validate(identity);
    if (type < firstFragmentType || type > lastFragmentType
        || firstBlockCapacity != sharedModelCapacity || model.scaleShift != 0
        || model.collisionRadius != fragmentRadii[static_cast<std::size_t>(type-firstFragmentType)]) {
        throw std::invalid_argument("unsupported failure fragment model lookup");
    }
    return {
        {{NormalWorkKind::instructions,14}}, //39EE..39F9 and3956..395F CALLDCC6.
        {FailureHeapProviderKind::allocate,identity,fragmentBytes,0xdcc6,false},
        {{NormalWorkKind::instructions,9}, //3962..397E successful return/clear setup.
            {NormalWorkKind::repeatString,fragmentBytes}, //397F REPSTOSB.
            {NormalWorkKind::instructions,12+6+27+15+8}}, //3956 return,39EE/D03C/C05C and epilogues.
        model.collisionRadius};
}

FailureHeapWork failureDebrisReleaseOperations(const FailureHeapIdentity identity) {
    validate(identity);
    return {
        {{NormalWorkKind::instructions,4}}, //398A..3990 CALLDCCA.
        {FailureHeapProviderKind::release,identity,fragmentBytes,0xdcca,false},
        {{NormalWorkKind::instructions,4+15+3}}, //3993..399F, includingDCC6/F051(FFFF) rejection.
        std::nullopt};
}

} // namespace stellar7
