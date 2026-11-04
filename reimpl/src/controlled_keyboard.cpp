#include "stellar7/controlled_keyboard.hpp"

#include <stdexcept>

namespace stellar7 {

void ControlledForwardQueue::transition(const ControlledForwardState state) {
    if (state != ControlledForwardState::released && state != ControlledForwardState::held) {
        throw std::invalid_argument("unknown host Up state");
    }
    if (state == received_) { return; }
    pending_.push_back(state == ControlledForwardState::held ? KeyboardEdge::press : KeyboardEdge::release);
    received_ = state;
}

std::optional<KeyboardEdge> ControlledForwardQueue::forwardAtRenderReturn(const ControlledKeyboardBoundary& boundary) {
    if (pending_.empty()) { return std::nullopt; }
    const auto edge = pending_.front();
    const auto expected = edge == KeyboardEdge::press ? ControlledForwardState::released : ControlledForwardState::held;
    if (boundary.forward != expected) {
        throw std::invalid_argument("host Up edge disagrees with retained controller");
    }
    pending_.pop_front();
    return edge;
}

} // namespace stellar7
