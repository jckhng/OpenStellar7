#include "stellar7/level1_world_work.hpp"

#include <stdexcept>

namespace stellar7 {

Level1AnimationWork level1AnimationOperations(Level1AnimationState state, const Model3d& model) {
    constexpr std::uint8_t animationReload = 5;
    constexpr std::uint8_t animationLimit = 8;
    constexpr std::uint64_t decrementDispatches = 15;
    if (state.reload != animationReload || state.countdown > animationReload
        || state.variant >= animationLimit || model.flags != 1
        || model.animationLimits != std::vector<std::uint8_t>{animationLimit}) {
        throw std::invalid_argument("unsupported initial Level1 animation callback");
    }
    if (state.countdown != 0) {
        --state.countdown;
        return {state, {{NormalWorkKind::instructions, decrementDispatches}}};
    }
    constexpr std::uint64_t expiryCallerDispatches = 42;
    constexpr std::uint64_t metadataCountDispatches = 18;
    constexpr std::uint64_t metadataLimitDispatches = 30;
    constexpr std::uint64_t sharedLookupDispatches = 27;
    state.countdown = state.reload;
    ++state.variant;
    const auto wrapped = state.variant >= animationLimit;
    if (wrapped) {
        state.variant = 0;
    }
    return {state, {{NormalWorkKind::instructions, expiryCallerDispatches + metadataCountDispatches
        + metadataLimitDispatches + 2 * sharedLookupDispatches + (wrapped ? 1 : 0)}}};
}

}
