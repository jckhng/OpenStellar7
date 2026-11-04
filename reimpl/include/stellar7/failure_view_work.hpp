#pragma once
#include "stellar7/render_work.hpp"

namespace stellar7 {
struct FixedMatrixWork {
    FixedMatrixQ14 matrix;
    std::vector<NormalWorkOperation> operations;
};
// Source34AE or38FF body through RETF; excludes caller. Angles retain low-bit table quantization.
FixedMatrixWork fixedMatrixBuildOperations(std::array<std::uint16_t, 3> angles);
FixedMatrixWork fixedMatrixComposeOperations(const FixedMatrixQ14& first,
    const FixedMatrixQ14& second, std::uint16_t incomingCX);

// Direct6BA6 entry through RET. Caller7379/SAR6 and ordinary6B5A/7C08 excluded.
// Full-screen Level1 gradient; hardware clip is initial0..319 or prior model0..318.
HorizonExecutionWork failureHorizonExecution(const HorizonWorkState& state,
    const std::vector<IndexedBitmap>& frames, Viewport incomingHardwareClip);

// B7AC through B994 RETF. Zero local view angles, full camera modes0..3, flags88/shift7.
WorldViewSetupWork failureViewSetupOperations(const WorldViewSetupInput& state);
// B9A5 through BA88, no global callbacks, non-camera actor and no flag40 proxy.
WorldFirstCenterWork failureFirstCenterOperations(const ViewTransform& view, const WorldActorWorkInput& actor);
WorldFirstCenterWork failureNextCenterOperations(const ViewTransform& view, const WorldActorWorkInput& actor);
} // namespace stellar7

namespace stellar7 {
HorizonExecutionWork level2FailureHorizonExecution(const HorizonWorkState&,
    const std::vector<IndexedBitmap>&,Viewport);
} // namespace stellar7
