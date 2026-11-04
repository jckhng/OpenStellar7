#pragma once

#include "stellar7/bitmap_cache.hpp"
#include "stellar7/radar_work.hpp"

namespace stellar7 {

struct FailureEffectWork;

enum class Level1EffectProjectionDomain { ordinaryCombat, guardianCombat, level2Combat };

struct RadarStateActor {
    std::size_t allocation;
    WorldActorWorkInput actor;
    std::uint32_t radius;
    std::int16_t type;
    std::int16_t category;
    std::uint16_t callback;
    WorldActorRenderProfile renderProfile;
};

class RadarState {
public:
    explicit RadarState(const std::vector<IndexedBitmap>& panelGroup);
    void resetArmorStrip() noexcept;
    //7D18 invalidates only the retained5232 counter after the modal radar redraw.
    void clearRetainedArmorStrip() noexcept { armorStrip_ = 0; }
    void initializeAllocation(std::size_t allocation);
    RadarWork initialize(std::int16_t armor, FixedPosition playerPosition,
        std::uint16_t playerYaw, const RadarPages& pages,
        RadarPageLayout layout = RadarPageLayout::compatibility);
    RadarWork draw(std::int16_t armor, FixedPosition playerPosition, std::uint16_t playerYaw,
        const std::vector<RadarStateActor>& actors, const RadarPages& pages,
        RadarTraversalProfile profile, RadarPageLayout layout = RadarPageLayout::compatibility,
        RadarPixelDomain domain = RadarPixelDomain::retained);
    RadarWork drawLevel2Constructor(std::int16_t armor, FixedPosition playerPosition,
        std::uint16_t playerYaw, const std::vector<RadarStateActor>& actors,
        const RadarPages& pages, RadarTraversalProfile profile);
    RadarWork drawLevel2Combat(std::int16_t armor, FixedPosition playerPosition,
        std::uint16_t playerYaw, const std::vector<RadarStateActor>& actors,
        const RadarPages& pages, RadarTraversalProfile profile,
        Level2RadarDomain domain = Level2RadarDomain::combat);
    RadarWork initializeLevel1(std::int16_t armor, FixedPosition playerPosition,
        std::uint16_t playerYaw, const RadarPages& pages);
    Level1RadarWork drawLevel1(std::int16_t armor, FixedPosition playerPosition, std::uint16_t playerYaw,
        const std::vector<RadarStateActor>& actors, const RadarPages& pages,
        RadarTraversalProfile profile, const Model3d& type11Model, Level1AnimationState animation);
    Level1RadarWork drawLevel1Projectile(std::int16_t armor, FixedPosition playerPosition, std::uint16_t playerYaw,
        const std::vector<RadarStateActor>& actors, const RadarPages& pages,
        RadarTraversalProfile profile, const Model3d& type11Model, Level1AnimationState animation);
    Level1RadarWork drawLevel1Combat(std::int16_t armor, FixedPosition playerPosition, std::uint16_t playerYaw,
        const std::vector<RadarStateActor>& actors, const RadarPages& pages,
        RadarTraversalProfile profile, const Model3d& type11Model, Level1AnimationState animation);
    Level1DynamicRadarWork drawLevel1Type3Combat(std::int16_t armor,FixedPosition playerPosition,
        std::uint16_t playerYaw,const std::vector<RadarStateActor>& actors,const RadarPages& pages,
        RadarTraversalProfile profile,const Model3d& type11Model,const Model3d* guardianModel,
        std::vector<std::optional<Level1AnimationState>> animations,Level1OrdinaryRenderFamily family);
    Level1DynamicRadarWork drawLevel1DynamicCombat(std::int16_t armor, FixedPosition playerPosition,
        std::uint16_t playerYaw, const std::vector<RadarStateActor>& actors, const RadarPages& pages,
        RadarTraversalProfile profile, const Model3d& type11Model,
        std::vector<std::optional<Level1AnimationState>> animations);
    Level1DynamicRadarWork drawLevel1CombatEffects(std::int16_t armor, FixedPosition playerPosition,
        std::uint16_t playerYaw, const std::vector<RadarStateActor>& actors, const RadarPages& pages,
        RadarTraversalProfile profile, const Model3d& type11Model,
        std::vector<std::optional<Level1AnimationState>> animations);
    Level1DynamicRadarWork drawLevel1BonusCombat(std::int16_t armor, FixedPosition playerPosition,
        std::uint16_t playerYaw, const std::vector<RadarStateActor>& actors, const RadarPages& pages,
        RadarTraversalProfile profile, const Model3d& type11Model,
        std::vector<std::optional<Level1AnimationState>> animations);
    Level1DynamicRadarWork drawLevel1ArrivalCombat(std::int16_t armor, FixedPosition playerPosition,
        std::uint16_t playerYaw, const std::vector<RadarStateActor>& actors, const RadarPages& pages,
        RadarTraversalProfile profile, const Model3d& type11Model,
        std::vector<std::optional<Level1AnimationState>> animations);
    Level1DynamicRadarWork drawLevel1DeploymentCombat(std::int16_t armor,FixedPosition playerPosition,
        std::uint16_t playerYaw,const std::vector<RadarStateActor>& actors,const RadarPages& pages,
        RadarTraversalProfile profile,const Model3d& type11Model,const Model3d& guardianModel,
        std::vector<std::optional<Level1AnimationState>> animations);
    //Publish actual main-view FC observations; no projection/callback/reset/work.
    void retainLevel1CombatEffectProjection(std::size_t allocation, const FailureEffectWork& work);
    void retainLevel1CombatEffectProjection(std::size_t allocation, const FailureEffectWork& work,
        Level1EffectProjectionDomain domain);
    const BitmapCache& cache() const noexcept { return cache_; }
    const WorldViewSetupInput& view() const noexcept { return view_; }
    std::int16_t armorStrip() const noexcept { return armorStrip_; }
    const RadarActorOutput& projection(std::size_t allocation) const;
    RadarActorOutput projectBitmapEffect(std::size_t allocation, const WorldActorWorkInput& actor,
        const ViewTransform& view, RadarPixelDomain domain = RadarPixelDomain::retained);
    void retainCompletionBeam(std::size_t allocation, const CompletionBeamPresentation& beam);

private:
    enum class Level1List { entities, primaryProjectile, combatProjectiles };
    Level1RadarWork drawLevel1List(std::int16_t armor, FixedPosition playerPosition, std::uint16_t playerYaw,
        const std::vector<RadarStateActor>& actors, const RadarPages& pages,
        RadarTraversalProfile profile, const Model3d& type11Model, Level1AnimationState animation, Level1List list);
    std::vector<RadarActorInput> level1Inputs(const std::vector<RadarStateActor>& actors) const;
    void retainLevel1Draw(const RadarWork& work, const std::vector<RadarStateActor>& actors,
        FixedPosition playerPosition, std::uint16_t playerYaw);
    // Source constructPools:16 ordinary,16 projectiles,8 effects,8 fixed records.
    static constexpr std::size_t allocationCapacity = 48;
    BitmapCache cache_;
    WorldViewSetupInput view_;
    std::array<std::optional<RadarActorOutput>, allocationCapacity> projections_{};
    std::int16_t armorStrip_ = 0;
    bool initialized_ = false;
};

}
