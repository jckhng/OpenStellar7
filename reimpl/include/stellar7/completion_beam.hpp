#pragma once

#include "stellar7/level1.hpp"

namespace stellar7 {

enum class CompletionBeamView { main, radar };

void generateCompletionBeam(CompletionBeamPresentation& beam, StellarRng& rng,
    CompletionBeamView context);

// B996 main cache projection and B0B4, admitted VGA indexed-color profile only.
CompletionBeamPresentation presentCompletionBeam(const Level1Entity& actor,
    const ViewTransform& view, StellarRng& rng, CompletionBeamView context);

struct CompletionBeamPixel {
    ProjectedPoint point;
    std::uint8_t color;
};

// Pixel-only vertical21B4 clipping; no CPU/device work qualification.
std::vector<CompletionBeamPixel> completionBeamPixels(const CompletionBeamPresentation& beam,
    Viewport viewport);

}
