#pragma once

#include "stellar7/completion_beam.hpp"
#include "stellar7/level1_world_work.hpp"
#include "stellar7/radar_work.hpp"
#include "stellar7/world_model_work.hpp"
#include "stellar7/world_traversal.hpp"

namespace stellar7 {

struct Level1GuardianSelectorWrite {
    std::uint8_t index;
    std::uint8_t value;
    std::uint16_t sourceInstruction;
    std::uint64_t instructionOffset;
};
struct Level1GuardianAnimationWork {
    Level1AnimationState state;
    std::vector<NormalWorkOperation> operations;
    //Ordered physical-byte observations; completed canonical state remains scalar.
    std::vector<Level1GuardianSelectorWrite> selectorWrites;
};

//130C callee, shared44 then Level1-local0 registry; no constructor/provider work.
Level1GuardianAnimationWork level1GuardianAnimationOperations(Level1AnimationState state,
    const Model3d& model);

struct Level1BeamProjectionWork {
    RadarActorOutput projection;
    std::uint16_t modelRadius;
    std::vector<NormalWorkOperation> operations;
};
//BA88 through BBE9 model CALL, replacing the ordinary flags0 caller exactly once.
Level1BeamProjectionWork level1BeamProjectionOperations(FixedPosition center,
    std::uint8_t flags, ProjectedPoint retainedAnchor, std::uint16_t retainedRadius);

struct Level1CompletionBeamWork {
    CompletionBeamPresentation beam;
    StellarRng rng;
    std::vector<NormalWorkOperation> operations;
    std::vector<CompletionBeamPixel> pixels;
};
//B0B4..B128 only. Consumes the prior BA88 observation, never projects again.
Level1CompletionBeamWork level1CompletionBeamOperations(const RadarActorOutput& projection,
    std::uint8_t timer, StellarRng rng, CompletionBeamView view,
    PolygonClipMode clipping = PolygonClipMode::Disabled);

struct Level1BeamObservation {
    std::size_t ordinal;
    RadarActorOutput projection;
    CompletionBeamPresentation beam;
};

WorldModelCullWork level1DeploymentModelCullOperations(FixedPosition center,std::int16_t type);
WorldModelCallerWork level1DeploymentModelCallerOperations(FixedPosition center,std::int16_t type);
WorldModelSetupWork level1DeploymentModelSetupOperations(FixedPosition center,
    std::array<std::uint16_t,3> angles, std::uint16_t projectedRadius,std::int16_t type,
    const Model3d& model, std::uint16_t registryScale, std::uint16_t registryCap,
    const WorldModelViewInput& view);
WorldModelGeometryWork level1DeploymentModelGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input, std::array<std::uint16_t,3> angles,
    const WorldModelViewInput& view,std::int16_t type);

} // namespace stellar7
