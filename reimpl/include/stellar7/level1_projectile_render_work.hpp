#pragma once

#include "stellar7/render_work.hpp"

namespace stellar7 {

// Type23/radius134 flat primary-shot sphere, including center-axis/near clamping.
WorldModelCullWork level1ProjectileModelCullOperations(FixedPosition center);

// CBCA's one-LOD/one-group inline-selector path; excludes the preceding sphere.
WorldModelSetupWork level1ProjectileModelSetupOperations(FixedPosition center,
    std::array<std::uint16_t, 3> actorAngles, std::uint16_t projectedRadius,
    const Model3d& model, std::uint16_t registryScale, std::uint16_t registryCap,
    const WorldModelViewInput& view);

// Combat lift's raised sphere crossing the view axis; legacy cull remains strict.
WorldModelCullWork level1CombatLiftModelCullOperations(FixedPosition center);
WorldModelSetupWork level1CombatLiftModelSetupOperations(FixedPosition center,
    std::array<std::uint16_t, 3> actorAngles, std::uint16_t projectedRadius,
    const Model3d& model, std::uint16_t registryScale, std::uint16_t registryCap,
    const WorldModelViewInput& view);

// Adds central upper-viewport clipping; the preceding lift API stays strict.
WorldModelCullWork level1CombatClippedLiftModelCullOperations(FixedPosition center);
WorldModelSetupWork level1CombatClippedLiftModelSetupOperations(FixedPosition center,
    std::array<std::uint16_t, 3> actorAngles, std::uint16_t projectedRadius,
    const Model3d& model, std::uint16_t registryScale, std::uint16_t registryCap,
    const WorldModelViewInput& view);

//Explicit Type11/pitch0/roll0, altitude0..1000, precision0 and flat camera.
//Shares CBCA's identity/yaw body; legacy and banked setup admission is unchanged.
WorldModelSetupWork level1CombatYawLiftModelSetupOperations(FixedPosition center,
    std::array<std::uint16_t, 3> actorAngles, std::uint16_t projectedRadius,
    const Model3d& model, std::uint16_t registryScale, std::uint16_t registryCap,
    const WorldModelViewInput& view);

// Separately qualified tank Type17/radius441; Type23 retains its primary domain.
WorldModelCullWork level1CombatProjectileModelCullOperations(FixedPosition center, std::int16_t type);
WorldModelSetupWork level1CombatProjectileModelSetupOperations(FixedPosition center,
    std::array<std::uint16_t, 3> actorAngles, std::uint16_t projectedRadius, std::int16_t type,
    const Model3d& model, std::uint16_t registryScale, std::uint16_t registryCap,
    const WorldModelViewInput& view);

//Existing Type17/23 setup metadata with explicit shared-descriptor sphere admission.
WorldModelSetupWork level1SharedCombatProjectileModelSetupOperations(FixedPosition center,
    std::array<std::uint16_t, 3> actorAngles, std::uint16_t projectedRadius, std::int16_t type,
    const Model3d& model, std::uint16_t registryScale, std::uint16_t registryCap,
    const WorldModelViewInput& view);

}
