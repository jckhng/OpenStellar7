#pragma once
#include "stellar7/controlled_session.hpp"
#include <cstdint>

namespace stellar7 {
enum class ControlledFrontendVisibility { shown, hidden };
struct ControlledFrontendKeyEdge {
    std::uint16_t loopTick;
    KeyboardEdge edge;
};
struct ControlledFrontendResult {
    bool cancelled = false;
    unsigned frames = 0;
    std::uint64_t samplesSubmitted = 0;
    std::uint64_t samplesPlayed = 0;
    std::uint64_t scansPresented = 0;
    std::uint64_t gdiBlits = 0;
    std::uint64_t keyboardReturns = 0;
    std::uint64_t keyboardEdges = 0;
    std::vector<ControlledFrontendKeyEdge> keyboardEvents;
};
ControlledFrontendResult runControlledSession(ControlledLevel2Session& session,
    unsigned completeFrames, ControlledFrontendVisibility visibility);
ControlledFrontendResult runControlledInput(GameplayAssets assets, ControlledBootstrapRequest request,
    unsigned completeFrames, ControlledFrontendVisibility visibility);
}
