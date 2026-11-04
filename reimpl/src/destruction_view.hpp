#pragma once
#include "world_presentation.hpp"

namespace stellar7 {
std::vector<PresentationFace> destructionFaces(const DestructionCue& cue,std::uint64_t worldTick,
    const ViewTransform& view,const std::vector<Rgb>& palette);
}
