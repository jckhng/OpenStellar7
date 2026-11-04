#pragma once
#include <cstddef>

namespace stellar7 {
enum class NativeAttractStage { menu, briefing, intro, credits, postCredits };
struct NativeAttractProgress {
    NativeAttractStage stage=NativeAttractStage::menu;
    std::size_t page=0;
    std::size_t tours=0;
};
}
