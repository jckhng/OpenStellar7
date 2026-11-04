#pragma once
#include "stellar7/frame_tail_work.hpp"

namespace stellar7 {
struct Level2FirstFrameTailWork {
    //tail.operations begin032F after the required9D9E child; state is after03E8.
    FrameTailWork tail;
    std::vector<NormalWorkOperation> beforeEntryFade;
};
//Genuine first0312/DI1: complete caller through032C, required shared9D9E child,
//then032F..03E8. Ordinary frameTailOperations retains firstFrame rejection.
//Only sourceLevel2 loop0/three8/two13/two30/released/armor200..680 is admitted.
//No palette/audio/clock execution; caller owns9D9E between the two segments.
Level2FirstFrameTailWork level2FirstFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards);
} //namespace stellar7
