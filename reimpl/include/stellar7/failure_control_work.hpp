#pragma once

#include "stellar7/level1.hpp"
#include "stellar7/normal_clock.hpp"
#include "stellar7/simulation_prefix_work.hpp"

#include <array>
#include <optional>
#include <vector>

namespace stellar7 {

enum class FailureObjectKind { mission, projectile, player, debris, effect };
struct FailureObjectRef {
    FailureObjectKind kind;
    std::size_t index;
};
constexpr bool operator==(const FailureObjectRef a,const FailureObjectRef b) noexcept {
    return a.kind == b.kind && a.index == b.index;
}
constexpr bool operator!=(const FailureObjectRef a,const FailureObjectRef b) noexcept { return !(a == b); }

//Only source control/list identity absent from the semantic model is retained here.
struct FailureControlState {
    std::uint16_t display = 0;
    bool entered = false;
    bool finished = false;
    bool refreshRequested = false;
    FixedPosition lastListCamera{};
    std::vector<FailureObjectRef> allocationOrder;
    std::vector<FailureObjectRef> presenterOrder;
    std::vector<std::size_t> debrisCreationOrder;
};

//Call-time observation copied from existing owners; never a second persistent model.
struct FailureControlSnapshot {
    Level1Entity player;
    Level1Entity camera;
    std::vector<Level1Entity> mission;
    std::array<Level1Projectile,Level1State::projectilePoolSize> projectiles;
    std::vector<Level1Entity> debris;
    std::array<Level1DeathEffect,Level1State::deathEffectPoolSize> effects;
    StellarRng rng;
    std::uint16_t sourceLoopTick = 0;
};

enum class FailureControlChildKind {
    loadedPalette, blackout, pageClear, modelViewport, hardwareViewport,
    horizon, heapCreate, heapRelease, scene, pageFlip, frameClear,
    entryFade
};
struct FailureControlChild {
    FailureControlChildKind kind;
    std::uint16_t sourceEntry;
    std::size_t ordinal = 0;
    std::array<std::int32_t,6> arguments{};
};
struct FailureControlStep {
    std::vector<NormalWorkOperation> operations;
    std::optional<FailureControlChild> child;
};
struct FailureControlInput {
    FailureControlState control;
    FailureControlSnapshot snapshot;
    std::array<std::uint32_t,5> debrisRadii{171,107,88,107,157};
};
struct FailureControlWork {
    FailureControlState control;
    //Expected source writes BEFORE scene child; root compares/stages existing owners once.
    FailureControlSnapshot expected;
    std::vector<FailureControlStep> steps;
    std::vector<FailureObjectRef> retired;
    std::vector<FailureObjectRef> drawOrder;
};

//Complete3B3A arithmetic and work, including nonzero pitch/roll and E16A branches.
KinematicsWork failureKinematicsOperations(const Level1Entity& actor);
//03EB through72EC/77CE entry, stopping before first7307. Requires12C2=1/12CE=0.
FailureControlWork failureEnterOperations(const FailureControlInput& input);
//7307 through74E3/74E1, before either next7307 or74E6; scene/heap/device children explicit.
FailureControlWork failureDisplayOperations(const FailureControlInput& input);
//74E6 through7511; excludes0401 and retains stale presenter/allocation debris refs.
FailureControlWork failureLeaveOperations(const FailureControlInput& input);
//Apply B996-owned retirement receipts in source order; adds no source work.
FailureControlState applyFailureSceneRetirements(FailureControlState state,
    const std::vector<FailureObjectRef>& retired);

}
