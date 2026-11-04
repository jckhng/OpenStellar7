#pragma once

#include "stellar7/combat_effect_work.hpp"
#include "stellar7/level1_deployment_render_work.hpp"
#include "stellar7/level1_object_ref.hpp"
#include "stellar7/level1_ordinary_family.hpp"
#include "stellar7/world_scene_work.hpp"

namespace stellar7 {

//Transient view of the existing FC slot and canonical projection cache.
struct Level1CombatEffectView {
    std::size_t poolSlot;
    WorldActorWorkInput actor;
    std::uint32_t radius;
    FailureEffectState state;
    std::uint16_t targetHeading;
};
struct Level1CombatEffectObservation {
    std::size_t ordinal;
    std::size_t poolSlot;
    //Operations/pixels already belong to the scene. Apply only returned state
    //and removal observations to the existing model; do not replay the child.
    FailureEffectWork work;
};
struct Level1CombatSceneWork {
    WorldSceneWork scene;
    std::vector<Level1CombatEffectObservation> effects;
    std::vector<Level1ObjectRef> allocationOrderAfterMain;
    std::optional<StellarRng> rngAfterMain;
    std::vector<Level1BeamObservation> beams;
};

//Same B9A5..BC3B ownership as worldSceneOperations; each optional effect replaces
//one normal model body at BA88. Old45D2 order remains fixed through main/radar;
//481A applies to a separate ephemeral registry in callback order. No pools owned.
Level1CombatSceneWork level1CombatSceneOperations(const GameplayAssets& assets,
    const ViewTransform& view, const std::vector<WorldSceneActor>& actors,
    const std::vector<std::optional<Level1CombatEffectView>>& effects,
    WorldSceneProfile profile, std::array<std::int16_t,3> transformedLight,
    std::uint16_t page, const std::vector<Level1ObjectRef>& allocationOrder);

//Explicit Type42 inline/spin presentation; existing combat defaults remain strict.
Level1CombatSceneWork level1BonusCombatSceneOperations(const GameplayAssets& assets,
    const ViewTransform& view, const std::vector<WorldSceneActor>& actors,
    const std::vector<std::optional<Level1CombatEffectView>>& effects,
    WorldSceneProfile profile, std::array<std::int16_t,3> transformedLight,
    std::uint16_t page, const std::vector<Level1ObjectRef>& allocationOrder);

Level1CombatSceneWork level1ArrivalCombatSceneOperations(const GameplayAssets& assets,
    const ViewTransform& view, const std::vector<WorldSceneActor>& actors,
    const std::vector<std::optional<Level1CombatEffectView>>& effects,
    WorldSceneProfile profile, std::array<std::int16_t,3> transformedLight,
    std::uint16_t page, const std::vector<Level1ObjectRef>& allocationOrder);

Level1CombatSceneWork level1DeploymentCombatSceneOperations(const GameplayAssets& assets,
    const ViewTransform& view, const std::vector<WorldSceneActor>& actors,
    const std::vector<std::optional<Level1CombatEffectView>>& effects,
    WorldSceneProfile profile, std::array<std::int16_t,3> transformedLight,
    std::uint16_t page, const std::vector<Level1ObjectRef>& allocationOrder,const StellarRng& rng);


Level1CombatSceneWork level1Type3CombatSceneOperations(const GameplayAssets& assets,
    const ViewTransform& view,const std::vector<WorldSceneActor>& actors,
    const std::vector<std::optional<Level1CombatEffectView>>& effects,
    WorldSceneProfile profile,std::array<std::int16_t,3> light,std::uint16_t page,
    const std::vector<Level1ObjectRef>& allocations,Level1OrdinaryRenderFamily family,
    const StellarRng* rng=nullptr);

} // namespace stellar7

namespace stellar7 {
Level1CombatSceneWork level2CombatSceneOperations(const GameplayAssets& assets,
    const ViewTransform& view, const std::vector<WorldSceneActor>& actors,
    const std::vector<std::optional<Level1CombatEffectView>>& effects,
    WorldSceneProfile profile, std::array<std::int16_t,3> transformedLight,
    std::uint16_t page, const std::vector<Level1ObjectRef>& allocationOrder);

Level1CombatSceneWork level2PeriodicSceneOperations(const GameplayAssets& assets,
    const ViewTransform& view, const std::vector<WorldSceneActor>& actors,
    const std::vector<std::optional<Level1CombatEffectView>>& effects,
    WorldSceneProfile profile, std::array<std::int16_t,3> transformedLight,
    std::uint16_t page, const std::vector<Level1ObjectRef>& allocationOrder);
}
