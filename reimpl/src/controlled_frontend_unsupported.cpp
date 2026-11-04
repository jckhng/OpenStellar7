#include "stellar7/controlled_frontend.hpp"
#include <stdexcept>

namespace stellar7 {
ControlledFrontendResult runControlledSession(ControlledLevel2Session&, unsigned, ControlledFrontendVisibility) {
    throw std::runtime_error("controlled graphical transport is currently supported on Windows only");
}
ControlledFrontendResult runControlledInput(GameplayAssets, ControlledBootstrapRequest, unsigned, ControlledFrontendVisibility) {
    throw std::runtime_error("controlled graphical transport is currently supported on Windows only");
}
}
