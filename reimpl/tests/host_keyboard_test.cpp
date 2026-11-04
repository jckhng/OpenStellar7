#include "stellar7/controlled_keyboard.hpp"

#include <cassert>
#include <stdexcept>

using namespace stellar7;

int main() {
    ControlledForwardQueue keys;
    assert(!keys.forwardAtRenderReturn({4,ControlledForwardState::released}));
    keys.transition(ControlledForwardState::held);
    keys.transition(ControlledForwardState::held);
    keys.transition(ControlledForwardState::released);
    assert(keys.forwardAtRenderReturn({4,ControlledForwardState::released}) == KeyboardEdge::press);
    assert(keys.forwardAtRenderReturn({5,ControlledForwardState::held}) == KeyboardEdge::release);
    assert(!keys.forwardAtRenderReturn({6,ControlledForwardState::released}));
    keys.transition(ControlledForwardState::held);
    assert(keys.forwardAtRenderReturn({7,ControlledForwardState::released}) == KeyboardEdge::press);
    keys.transition(ControlledForwardState::released); // Focus loss releases an outstanding hold.
    assert(keys.forwardAtRenderReturn({8,ControlledForwardState::held}) == KeyboardEdge::release);
    keys.transition(ControlledForwardState::held);
    bool rejected = false;
    try { keys.forwardAtRenderReturn({9,ControlledForwardState::held}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    assert(keys.forwardAtRenderReturn({9,ControlledForwardState::released}) == KeyboardEdge::press);
}
