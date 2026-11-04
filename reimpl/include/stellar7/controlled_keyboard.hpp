#pragma once

#include "stellar7/keyboard_transport.hpp"

#include <deque>
#include <optional>

namespace stellar7 {

enum class ControlledForwardState { released, held };
struct ControlledKeyboardBoundary {
    std::uint16_t loopTick;
    ControlledForwardState forward;
};
class ControlledKeyboardSource {
public:
    virtual ~ControlledKeyboardSource() = default;
    virtual std::optional<KeyboardEdge> forwardAtRenderReturn(const ControlledKeyboardBoundary&) = 0;
};

// Host intent only. Resident metadata and packet timing belong to the guest owners.
class ControlledForwardQueue final : public ControlledKeyboardSource {
public:
    void transition(ControlledForwardState state);
    std::optional<KeyboardEdge> forwardAtRenderReturn(const ControlledKeyboardBoundary& boundary) override;

private:
    ControlledForwardState received_ = ControlledForwardState::released;
    std::deque<KeyboardEdge> pending_;
};

} // namespace stellar7
