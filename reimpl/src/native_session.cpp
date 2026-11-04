#include "stellar7/native_session.hpp"
#include "failure_page.hpp"
#include "stellar7/palette_clock.hpp"
#include "stellar7/blackout_work.hpp"
#include "stellar7/raster_point_work.hpp"
#include "stellar7/sight_work.hpp"
#include "stellar7/highscore_render.hpp"
#include "stellar7/campaign_loader.hpp"
#include "level1_reward_model_bank.hpp"
#include "playable_menu_pages.hpp"
#include "playable_detail_render.hpp"
#include "playable_system_pages.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::size_t panelGroup = 3;
constexpr std::size_t overlayGroup = 4;
constexpr std::size_t introductionOption = 5;
constexpr std::size_t stripIndex = 1;
constexpr std::size_t backgroundIndex = 2;
constexpr std::size_t indicatorIndex = 6;
constexpr std::size_t retainedActorCount = 7;
constexpr std::int16_t maximumSteeringVelocity = 512;
constexpr std::int16_t maximumForwardVelocity = 200;
constexpr std::int32_t maximumRetainedRadarAltitude = 1000;
constexpr std::int16_t completionCraftType = 7;
constexpr std::int32_t completionCraftAltitude = 5000;
constexpr std::int32_t maximumCompletionCraftBank = 0x5000;
constexpr std::uint8_t lastCompletionCraftNoCallbackState = 2;
constexpr std::uint16_t completionBeamCallback = 0xb0b4;
constexpr std::int16_t guardianType = 44;
constexpr std::int16_t openGuardianType = 45;
constexpr std::int16_t guardianFixedType = 46;
constexpr std::int16_t chasingActorType = 47;
constexpr std::int16_t descendingActorType = 48;
constexpr std::uint32_t inlineActorRadius = 559;
constexpr std::uint32_t inlineModelRadius = 291;
constexpr std::int32_t inlineLaunchAltitude = 500;
constexpr std::int32_t inlineDescentStep = 50;
constexpr std::uint8_t chasingAnimationLimit = 4;
constexpr std::uint8_t descendingAnimationLimit = 14;
constexpr std::uint8_t chasingTimer = 60;
constexpr std::int16_t chasingForwardSpeed = 100;
constexpr std::uint8_t inlineLodFlags = 0x80;
constexpr std::uint16_t guardianAnimationCallback = 0x130c;
constexpr std::uint8_t guardianAnimationLimit = 6;
constexpr std::uint8_t openGuardianAnimationLimit = 5;
constexpr std::uint8_t guardianAnimationInterval = 2;
constexpr std::uint32_t guardianRadius = 1147;
constexpr std::uint32_t openGuardianModelRadius = 1145;
constexpr std::uint32_t guardianFixedRadius = 2260;
constexpr std::uint8_t lastPostBeamTimer = 29;
constexpr std::uint32_t maximumRetainedRadarRadius = 32767;
constexpr std::int8_t ordinaryRadarCategory = -1;
constexpr std::int8_t fixedRadarCategory = -2;
constexpr std::int8_t projectileRadarCategory = -3;
constexpr std::int16_t airborneProjectileType = 20;
constexpr std::size_t projectilePoolBase = 32;
constexpr std::size_t fixedPoolSize = 16;
constexpr std::size_t fixedPoolBase = projectilePoolBase - fixedPoolSize;
constexpr std::size_t resetFixedSlotCount = 8;
constexpr std::size_t projectilePoolSize = 8;
constexpr std::size_t effectPoolBase = projectilePoolBase + projectilePoolSize;
constexpr std::size_t effectPoolSize = 8;
constexpr std::size_t retainedFixedCount = 2;
constexpr std::uint16_t effectCallback = 0x0ac8;
constexpr std::size_t effectBitmapGroup = 5;
constexpr std::uint8_t nonlethalEffectTerminal = 12;
constexpr std::uint8_t firstLethalEffectTerminal = 17;
constexpr std::uint8_t lethalEffectFrameCount = 3;
constexpr std::uint8_t maximumCompleteLethalBucket = 4;
constexpr std::array<std::int16_t, 4> retainedProjectileTypes{17, 19, 20, 23};
constexpr std::uint8_t maximumWeaponCooldown = 8;
constexpr std::uint8_t maximumCannonSlotTimer = 31;
constexpr std::uint8_t level2Index = 1;
constexpr std::uint8_t level3Index = 2;
constexpr std::uint8_t titleColor = 239;
constexpr std::int16_t cockpitOverlayStage = 70;
constexpr std::uint16_t screenWidth = 320;
constexpr std::uint16_t screenHeight = 200;
constexpr std::uint8_t finalBlackoutIntensity = 3;
constexpr Viewport worldViewport{24, 295, 24, 145};
constexpr std::int16_t maximumArmor = 680;
constexpr std::int16_t minimumLethalArmor = -128;
// With origin200000, signed16 altitude keeps the source divisor in its two-shift range.
constexpr std::int32_t maximumRadarAltitude = std::numeric_limits<std::int16_t>::max();
constexpr std::int64_t maximumRadarRelativeCoordinate = 1000000;
constexpr std::int32_t minimumProjectionDepth = 0x20000;
constexpr std::int32_t maximumProjectionDepth = 0x40000;
constexpr std::int32_t projectionScale = 128;
constexpr std::int8_t firstRadarCategory = -4;
constexpr std::int8_t lastRadarCategory = -1;
constexpr std::array<std::uint8_t, 4> radarColors{78, 238, 138, 228};
constexpr std::int16_t completionPickupType = 25;
constexpr std::uint8_t completionPickupColor = 234;
constexpr std::uint32_t completionPickupRadius = 897;
constexpr std::uint8_t completionPickupAnimationLimit = 4;
constexpr std::size_t completionPickupGroupCount = 6;
constexpr std::array<std::int16_t, 4> completionPickupSpins{2560, -2816, 3072, -3328};
constexpr std::size_t unavailableLamp = 39;
constexpr std::size_t readyLamp = 40;
constexpr std::uint16_t lampCopyWidth = 24;
constexpr std::uint16_t lampCopyHeight = 10;
constexpr ProjectedPoint continueDialogOrigin{59, 29};
constexpr std::uint16_t continueDialogWidth = 209;
constexpr std::uint16_t continueDialogHeight = 114;
constexpr std::int16_t continuePointerX = 161;
constexpr std::int16_t continueYesPointerY = 74;
constexpr std::int16_t continueNoPointerY = 118;
constexpr std::uint16_t continuePointerWidth = 16;
constexpr std::uint16_t continuePointerHeight = 21;

constexpr std::array<const char*, 7> campaignTitles{
    "SOL", "ANTARES", "RIGEL", "DENEB", "SIRIUS", "REGULUS", "ARCTURUS"};

std::size_t campaignStartIndex(const NativeSessionStart start) {
    switch (start) {
    case NativeSessionStart::level1: return 0;
    case NativeSessionStart::level2: return 1;
    case NativeSessionStart::level3: return 2;
    case NativeSessionStart::level4: return 3;
    case NativeSessionStart::level5: return 4;
    case NativeSessionStart::level6: return 5;
    case NativeSessionStart::level7: return 6;
    }
    throw std::invalid_argument("invalid native campaign page start");
}


bool supportsRetainedModel(const Model3d& model) {
    constexpr std::size_t selectorBytes = Level1Entity{}.stateBytes4dTo52.size() - 2;
    if (model.lods.empty()) {
        return false;
    }
    for (const auto& lod : model.lods) {
        if (lod.groups.empty()) {
            return false;
        }
        for (const auto& group : lod.groups) {
            if (group.animationOffset < -1
                || (group.animationOffset >= 0 && static_cast<std::size_t>(group.animationOffset) >= selectorBytes)
                || group.vertices.empty() || group.originVertex >= group.vertices.size() || group.primitives.empty()
                || std::any_of(group.primitives.begin(), group.primitives.end(), [](const auto& primitive) {
                    return primitive.kind != 0;
                })) {
                return false;
            }
        }
    }
    return true;
}

bool supportsRetainedModel(const GameplayAssets& assets, const std::size_t type) {
    return assets.models.size() > type && supportsRetainedModel(assets.models[type]);
}

bool supportsCompletionCraft(const GameplayAssets& assets, const Level1Entity& actor,
    const CompletionBeamPresentation* presentation) {
    const auto bank = static_cast<std::int16_t>(actor.roll);
    const bool approach = actor.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback
        && actor.flags == 0 && actor.stateBytes4dTo52[5] <= lastCompletionCraftNoCallbackState
        && presentation == nullptr;
    const bool postBeam = actor.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback
        && (actor.flags == 0 || actor.flags == 4) && actor.stateBytes4dTo52[5] == 3
        && actor.behaviorTimer > 0 && actor.behaviorTimer <= lastPostBeamTimer && presentation == nullptr;
    const bool beam = actor.renderProfile == WorldActorRenderProfile::inlineSelectorBeamCallback
        && actor.stateBytes4dTo52[5] == 3 && actor.behaviorTimer >= 30 && actor.behaviorTimer <= 70
        && presentation && presentation->timer == actor.behaviorTimer
        && actor.flags == (presentation->projected ? 12 : 8);
    const bool departure = actor.renderProfile == WorldActorRenderProfile::inlineSelectorDeparture
        && (actor.flags == 0 || actor.flags == 4) && actor.stateBytes4dTo52[5] == 4
        && actor.behaviorTimer == 0 && actor.stateBytes4dTo52[2] == 0 && presentation == nullptr
        && actor.position.z >= 0 && actor.position.z <= maximumRadarRelativeCoordinate;
    if (actor.type != completionCraftType
        || (!approach && !beam && !postBeam && !departure) || actor.stateBytes4dTo52[0] != 0xff
        || (!departure && (actor.pitch != 0 || actor.position.z != completionCraftAltitude))
        || bank < -maximumCompletionCraftBank || bank > maximumCompletionCraftBank
        || !supportsRetainedModel(assets, completionCraftType)
        || actor.collisionRadius != assets.models[completionCraftType].collisionRadius) {
        return false;
    }
    for (const auto& lod : assets.models[completionCraftType].lods) {
        for (const auto& group : lod.groups) {
            if (group.animationOffset >= 0
                && actor.stateBytes4dTo52[group.animationOffset + 2] >= group.primitives.size()) {
                return false;
            }
        }
    }
    return true;
}

bool supportsType11(const GameplayAssets& assets, const Level1Entity& actor) {
    constexpr std::size_t type = 11;
    constexpr std::uint8_t animationLimit = 8;
    constexpr std::uint8_t animationInterval = 5;
    constexpr std::int32_t minimumAltitude = 150;
    constexpr std::int32_t altitudeStep = 50;
    constexpr std::int32_t maximumBank = 0x2000;
    const auto roll = static_cast<std::int16_t>(actor.roll);
    if (actor.type != type || actor.renderProfile != WorldActorRenderProfile::heapSelectorDeferredAnimation
        || actor.flags != 0 || actor.stateBytes4dTo52[0] != 0xff || actor.stateBytes4dTo52[1] != 1
        || actor.stateBytes4dTo52[2] != animationInterval || actor.stateBytes4dTo52[3] > animationInterval
        || actor.stateBytes4dTo52[4] != 3 || actor.stateBytes4dTo52[5] > 5
        || !actor.animatedModelPresenterEnabled || actor.modelAnimationVariant >= animationLimit
        || actor.presentationModelAnimationVariant >= animationLimit
        || actor.pitch != 0 || roll < -maximumBank || roll > maximumBank
        || actor.position.z < minimumAltitude || actor.position.z > maximumRetainedRadarAltitude
        || actor.position.z % altitudeStep != 0 || actor.collisionRadius != 748
        || !supportsRetainedModel(assets, type)) {
        return false;
    }
    const auto& model = assets.models[type];
    return model.collisionRadius == actor.collisionRadius && model.flags == 1
        && model.animationLimits.size() == 1 && model.animationLimits[0] == animationLimit
        && std::all_of(model.lods.begin(), model.lods.end(), [animationLimit](const auto& lod) {
            return std::all_of(lod.groups.begin(), lod.groups.end(), [animationLimit](const auto& group) {
                return (group.animationOffset == -1 && group.primitives.size() == 1)
                    || (group.animationOffset == 0 && group.primitives.size() == animationLimit);
            });
        });
}

bool supportsGuardian(const GameplayAssets& assets, const Level1Entity& actor, const std::size_t level) {
    if (level == 4) {
        constexpr std::uint32_t radius = 1508;
        constexpr std::uint8_t animationLimit = 6;
        const bool callback = actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation;
        if (actor.type != guardianType
            || (!callback && actor.renderProfile != WorldActorRenderProfile::heapSelectorNoCallback)
            || actor.animatedModelPresenterEnabled != callback
            || actor.flags != 0 || actor.stateBytes4dTo52[0] != 0xff
            || actor.stateBytes4dTo52[1] > 50 || actor.stateBytes4dTo52[4] != 5
            || actor.stateBytes4dTo52[5] != 0 || actor.pitch != 0 || actor.roll != 0 || actor.position.z != 0
            || actor.collisionRadius != radius || actor.modelAnimationVariant >= animationLimit
            || actor.presentationModelAnimationVariant >= animationLimit
            || actor.stateBytes4dTo52[2] < 1 || actor.stateBytes4dTo52[2] > 2
            || actor.stateBytes4dTo52[3] > 2 || assets.campaignLevels.size() <= level
            || assets.campaignLevels[level].resourceName != "LEVEL5.RES"
            || assets.campaignLevels[level].models.size() != 1) {
            return false;
        }
        const auto& model = assets.campaignLevels[level].models.front();
        if (!supportsRetainedModel(model) || model.collisionRadius != radius || model.flags != 1
            || model.animationLimits != std::vector<std::uint8_t>{animationLimit}
            || model.scaleShift != 0 || model.origin != std::array<std::int16_t, 3>{0, 0, 0}
            || model.lods.size() != 1 || model.lods.front().flags != 0
            || model.lods.front().threshold != 0 || model.lods.front().groups.size() != 20) {
            return false;
        }
        std::size_t staticGroups = 0;
        std::size_t animatedGroups = 0;
        for (const auto& group : model.lods.front().groups) {
            if (group.animationOffset == -1 && group.primitives.size() == 1) {
                ++staticGroups;
            } else if (group.animationOffset == 0 && group.primitives.size() == animationLimit) {
                ++animatedGroups;
            } else {
                return false;
            }
        }
        return staticGroups == 14 && animatedGroups == 6;
    }
    if (level != 1) {
        return false;
    }
    const bool open = actor.type == openGuardianType;
    const auto limit = open ? openGuardianAnimationLimit : guardianAnimationLimit;
    const auto behavior = actor.stateBytes4dTo52[5];
    const bool supportedBehavior = open ? (behavior == 1 && actor.behaviorTimer >= 1 && actor.behaviorTimer <= 35)
            || (behavior == 0 && actor.behaviorTimer == 0)
        : behavior == 0 || (behavior == 1 && actor.behaviorTimer == 36);
    const auto localModel = open ? 1U : 0U;
    if ((actor.type != guardianType && !open) || actor.renderProfile != WorldActorRenderProfile::heapSelectorDeferredAnimation
        || actor.flags != 0 || actor.stateBytes4dTo52[0] != 0xff || actor.stateBytes4dTo52[1] > 20
        || actor.pitch != 0 || actor.roll != 0 || actor.position.z != 0 || !actor.animatedModelPresenterEnabled
        || actor.modelAnimationVariant >= limit
        || actor.presentationModelAnimationVariant >= limit
        || actor.stateBytes4dTo52[2] != guardianAnimationInterval
        || actor.stateBytes4dTo52[3] > guardianAnimationInterval
        || !supportedBehavior || actor.collisionRadius != guardianRadius
        || assets.level2Models.size() <= localModel || !supportsRetainedModel(assets.level2Models[localModel])) {
        return false;
    }
    const auto& model = assets.level2Models[localModel];
    if (model.collisionRadius != (open ? openGuardianModelRadius : guardianRadius)
        || model.flags != 1 || model.animationLimits.size() != 1 || model.animationLimits[0] != limit) {
        return false;
    }
    for (const auto& lod : model.lods) {
        for (const auto& group : lod.groups) {
            if (group.animationOffset != 0
                || (group.primitives.size() != limit && (open || group.primitives.size() != 1))) {
                return false;
            }
        }
    }
    return true;
}

bool supportsGuardianFixed(const GameplayAssets& assets, const Level1Entity& actor) {
    constexpr std::size_t localModel = guardianFixedType - guardianType;
    if (actor.type != guardianFixedType || actor.renderProfile != WorldActorRenderProfile::noHandles
        || actor.flags != 0 || actor.stateBytes4dTo52[0] != 0xfe
        || actor.pitch != 0 || actor.roll != 0 || actor.yaw != 0 || actor.position.z != 0
        || actor.collisionRadius != guardianFixedRadius || assets.level2Models.size() <= localModel
        || !supportsRetainedModel(assets.level2Models[localModel])) {
        return false;
    }
    const auto& model = assets.level2Models[localModel];
    return model.flags == 0 && model.collisionRadius == guardianFixedRadius
        && std::all_of(model.lods.begin(), model.lods.end(), [](const auto& lod) {
            return std::all_of(lod.groups.begin(), lod.groups.end(), [](const auto& group) {
                return group.animationOffset == -1 && group.primitives.size() == 1;
            });
        });
}

bool supportsInlineFamily(const GameplayAssets& assets, const Level1Entity& actor) {
    const bool descending = actor.type == descendingActorType;
    if ((actor.type != chasingActorType && !descending)
        || actor.renderProfile != WorldActorRenderProfile::inlineSelectorNoCallback
        || actor.flags != 0 || actor.stateBytes4dTo52[0] != 0xff || actor.stateBytes4dTo52[1] != 1
        || actor.stateBytes4dTo52[3] != 0 || actor.stateBytes4dTo52[4] != chasingAnimationLimit
        || actor.stateBytes4dTo52[5] != 0
        || actor.pitch != 0 || actor.roll != 0 || actor.yawVelocity != 0
        || actor.position.z < 0 || actor.position.z > inlineLaunchAltitude
        || actor.position.z % inlineDescentStep != 0 || actor.collisionRadius != inlineActorRadius
        || actor.forwardVelocity != (descending ? 0 : chasingForwardSpeed)
        || (!descending && actor.behaviorTimer > chasingTimer)) {
        return false;
    }
    const auto local = static_cast<std::size_t>(actor.type - guardianType);
    const auto limit = descending ? descendingAnimationLimit : chasingAnimationLimit;
    if (assets.level2Models.size() <= local || !supportsRetainedModel(assets.level2Models[local])) {
        return false;
    }
    const auto& model = assets.level2Models[local];
    if (model.collisionRadius != inlineModelRadius || model.flags != 1
        || model.animationLimits.size() != 1 || model.animationLimits[0] != limit
        || model.lods.size() != 1 || model.lods[0].flags != inlineLodFlags) {
        return false;
    }
    // CF26..CF30 uses alternative0 for an out-of-range source byte, including first47 selector13.
    return std::all_of(model.lods[0].groups.begin(), model.lods[0].groups.end(), [limit](const auto& group) {
        return group.animationOffset == 0 && group.primitives.size() == limit;
    });
}

bool supportsCompletionPickup(const GameplayAssets& assets, const Level1Entity& actor) {
    if (actor.type != completionPickupType || actor.renderProfile != WorldActorRenderProfile::inlineSelectorNoCallback
        || actor.flags != 0 || actor.stateBytes4dTo52[0] != 0xfe || actor.stateBytes4dTo52[1] != 100
        || actor.stateBytes4dTo52[2] >= completionPickupAnimationLimit
        || actor.stateBytes4dTo52[3] != 0 || actor.stateBytes4dTo52[4] != 0
        || (actor.stateBytes4dTo52[5] != 1 && actor.stateBytes4dTo52[5] != 0xff)
        || actor.pitch != 0 || actor.roll != 0 || actor.position.z != 0
        || actor.pitchVelocity != 0 || actor.rollVelocity != 0 || actor.forwardVelocity != 0
        || actor.xVelocity != 0 || actor.yVelocity != 0 || actor.zVelocity != 0
        || std::find(completionPickupSpins.begin(), completionPickupSpins.end(), actor.yawVelocity)
            == completionPickupSpins.end()
        || actor.collisionRadius != completionPickupRadius || !supportsRetainedModel(assets, actor.type)) {
        return false;
    }
    const auto& model = assets.models[completionPickupType];
    return model.flags == 1 && model.animationLimits.size() == 1
        && model.animationLimits[0] == completionPickupAnimationLimit
        && model.collisionRadius == completionPickupRadius
        && model.lods.size() == 1 && model.lods[0].flags == inlineLodFlags
        && model.lods[0].groups.size() == completionPickupGroupCount
        && std::all_of(model.lods[0].groups.begin(), model.lods[0].groups.end(), [](const auto& group) {
            return group.animationOffset == 0 && group.primitives.size() == completionPickupAnimationLimit;
        });
}

bool supportsSystemBonus(const GameplayAssets& assets, const Level1Entity& actor) {
    constexpr std::size_t type = 42;
    constexpr std::uint32_t radius = 330;
    constexpr std::int32_t altitude = 100;
    constexpr std::int16_t spin = 0x0800;
    const auto payload = actor.stateBytes4dTo52[5];
    if (actor.type != type || actor.renderProfile != WorldActorRenderProfile::inlineSelectorNoCallback
        || actor.flags != 0 || actor.stateBytes4dTo52[0] != 0xfe || actor.stateBytes4dTo52[1] != 1
        || payload < 1 || payload > Level1State::systemCount || actor.stateBytes4dTo52[2] != payload
        || actor.stateBytes4dTo52[3] != 0 || actor.stateBytes4dTo52[4] != 0
        || actor.position.z != altitude || actor.collisionRadius != radius
        || actor.pitch != 0 || actor.pitchVelocity != 0 || actor.forwardVelocity != 0
        || actor.xVelocity != 0 || actor.yVelocity != 0 || actor.zVelocity != 0
        || actor.yawVelocity != spin || actor.rollVelocity != spin || actor.weaponSlotCount != 0
        || actor.animatedModelPresenterEnabled
        || actor.collisionContactAllocationIndex != static_cast<std::size_t>(-1)
        || !supportsRetainedModel(assets, type)) {
        return false;
    }
    const auto& model = assets.models[type];
    if (model.flags != 1 || model.collisionRadius != radius || model.lods.size() != 1
        || model.lods[0].flags != inlineLodFlags || model.lods[0].groups.size() != 5) {
        return false;
    }
    const auto& groups = model.lods[0].groups;
    return groups[0].animationOffset == 0 && groups[0].primitives.size() == 5
        && std::all_of(groups.begin() + 1, groups.end(), [](const auto& group) {
            return group.animationOffset == -1 && group.primitives.size() == 1;
        });
}

RadarPages radarRoles(const CockpitPageState& pages) {
    return {pages.cachePage(), pages.backPage(), pages.frontPage(), pages.drawPage(), pages.copyPage()};
}

CockpitPageLayout retainedPageLayout(const CockpitPageState& pages) {
    return pages.cachePage()==CockpitPageState::fromRecoveredMenu().cachePage()
        ? CockpitPageLayout::recoveredStartup : CockpitPageLayout::compatibility;
}

RadarPageLayout retainedRadarLayout(const CockpitPageState& pages) {
    return retainedPageLayout(pages)==CockpitPageLayout::recoveredStartup
        ? RadarPageLayout::recoveredStartup : RadarPageLayout::compatibility;
}

bool supportsLevel3Actor(const GameplayAssets& assets, const Level1Entity& actor) {
    constexpr std::int16_t animatedFixed = 24;
    constexpr std::int16_t fixedCompanion = 31;
    constexpr std::uint8_t fixedInterval = 2;
    if ((actor.type != 2 && actor.type != 10 && actor.type != animatedFixed && actor.type != fixedCompanion)
        || !supportsRetainedModel(assets, actor.type) || actor.position.z != 0
        || actor.pitch != 0 || actor.roll != 0 || actor.pitchVelocity != 0 || actor.rollVelocity != 0
        || actor.xVelocity != 0 || actor.yVelocity != 0 || actor.zVelocity != 0 || actor.flags != 0
        || actor.collisionRadius != campaignModelRadius(assets.models, {}, actor.type)) {
        return false;
    }
    if (actor.type == 2 || actor.type == 10) {
        return actor.stateBytes4dTo52[0] == 0xff && actor.stateBytes4dTo52[1] != 0
            && actor.renderProfile == WorldActorRenderProfile::noHandles && !actor.animatedModelPresenterEnabled;
    }
    const auto& model = assets.models[actor.type];
    if (model.animationLimits.size() != 1 || model.animationLimits[0] == 0 || model.flags != 1) {
        return false;
    }
    if (actor.type == animatedFixed) {
        return actor.stateBytes4dTo52[0] == 0xfc && actor.stateBytes4dTo52[1] == 100
            && actor.stateBytes4dTo52[2] == fixedInterval && actor.stateBytes4dTo52[3] <= fixedInterval
            && actor.animatedModelPresenterEnabled
            && actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation
            && actor.modelAnimationVariant < model.animationLimits[0]
            && actor.presentationModelAnimationVariant < model.animationLimits[0];
    }
    return actor.stateBytes4dTo52[0] == 0xfe && actor.stateBytes4dTo52[1] == 100
        && actor.stateBytes4dTo52[2] == 0 && !actor.animatedModelPresenterEnabled
        && actor.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback;
}
}

CockpitSettings NativeGameplaySession::renderSettings() const {
    if (!assets_.cockpitSettings) {
        throw std::invalid_argument("native rendering requires loaded cockpit settings");
    }
    auto settings = *assets_.cockpitSettings;
    if (audio_->playablePreferences_) {
        if (!audio_->playableDetailProfile()) {
            throw std::invalid_argument("native detail requires its canonical playable profile");
        }
        settings.viewSize = audio_->playableDetail();
    }
    return settings;
}

void NativeGameplaySession::restorePlayablePreferencesPages() {
    if (!playableCampaign_ || !audio_->playablePreferences_ || !audio_->playableDetailProfile()
        || !hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::fullGraphicDetail)
        || !pixels_ || !radar_ || level1Pages_ || !campaignResources_
        || campaignResources_->level != model_.campaignLevel()) {
        throw std::invalid_argument("preferences restore requires its retained playable page owners");
    }
    const auto detail = audio_->playableDetail();
    const auto layout = retainedPageLayout(pages_);
    auto stagedModel = model_;
    auto stagedPixels = *pixels_;
    auto stagedRadar = *radar_;
    auto stagedIncarnations = actorIncarnations_;
    auto stagedTypes = actorTypes_;
    const auto states = stagedModel.resumePlayablePreferences(detail);
    const auto matching = playableFullCockpitOperations({states[0].x, states[0].velocity, detail,
        CockpitPageRelation::matching}, cockpitBitmaps_);
    const auto ordinary = playableFullCockpitOperations({states[1].x, states[1].velocity, detail,
        CockpitPageRelation::differing}, cockpitBitmaps_);
    if (matching.state.indicatorX != states[1].x || matching.state.indicatorVelocity != states[1].velocity
        || ordinary.state.indicatorX != states[2].x || ordinary.state.indicatorVelocity != states[2].velocity) {
        throw std::logic_error("preferences redraw advanced a different cockpit indicator");
    }
    //50C4 reuses the existing cache. Skipped detail branches intentionally retain its pixels.
    stagedPixels.apply(cockpitCachePageEffects(matching, ordinary, cockpitBitmaps_, assets_.titleFont,
        campaignTitles.at(model_.campaignLevel()), titleColor, pages_.cachePage(), pages_.backPage(),
        layout, CockpitCacheDomain::retainedGameplay));
    std::vector<RadarStateActor> actors;
    for (const auto& input : stagedModel.playablePreferencesRadarActors()) {
        const auto& actor = input.actor;
        if (input.allocation < ordinaryPoolSize_ && input.incarnation) {
            if (stagedIncarnations[input.allocation] != *input.incarnation) {
                stagedRadar.initializeAllocation(input.allocation);
                stagedIncarnations[input.allocation] = *input.incarnation;
            }
            stagedTypes[input.allocation] = actor.type;
        }
        const auto effect = actor.renderProfile == WorldActorRenderProfile::bitmapEffectCallback;
        const auto flags = effect ? stagedRadar.projection(input.allocation).flags : actor.flags;
        const auto callback = actor.renderProfile == WorldActorRenderProfile::inlineSelectorBeamCallback ? completionBeamCallback
            : actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation ? guardianAnimationCallback
            : effect && actor.stateBytes4dTo52[1] < actor.stateBytes4dTo52[4] ? effectCallback : 0;
        actors.push_back({input.allocation, {flags, actor.position}, actor.collisionRadius, actor.type,
            static_cast<std::int8_t>(actor.stateBytes4dTo52[0]), static_cast<std::uint16_t>(callback), actor.renderProfile});
    }
    auto roles = radarRoles(pages_);
    roles.draw = roles.back; roles.copy = roles.cached;
    const auto& player = stagedModel.level2Player();
    stagedPixels.apply(stagedRadar.draw(stagedModel.playerArmor(), player.position, player.yaw, actors,
        roles, {0, 0}, retainedRadarLayout(pages_), RadarPixelDomain::playableCampaign)); //5101/698A.
    stagedPixels.apply(std::vector<IndexedPageEffect>{RadarPageCopy{pages_.backPage(), pages_.frontPage(),
        {0, 0}, screenWidth, screenHeight}}); //5110.
    const auto& systems = stagedModel.level1State();
    stagedPixels.apply(statusPageEffects(systems.systemStatusLevels(), systems.systemTimers(), -1,
        assets_.artGroups.at(overlayGroup), pages_.backPage(), pages_.frontPage(), layout));
    if (systems.selectedSystem() >= 0) {
        const auto selected = static_cast<std::size_t>(systems.selectedSystem());
        stagedPixels.apply(playableSystemPageEffects(SystemBorder{selected, true}, assets_.artGroups.at(overlayGroup),
            pages_.backPage(), pages_.frontPage(), layout));
        stagedPixels.apply(playableSystemPageEffects(SystemCopy{selected}, assets_.artGroups.at(overlayGroup),
            pages_.backPage(), pages_.frontPage(), layout));
    }
    stagedRadar.clearRetainedArmorStrip(); //7D18, after50C4/698A and before7970.
    stagedPixels.apply(scorePageEffects(stagedModel.score(), assets_.scoreFont,
        pages_.backPage(), pages_.frontPage(), layout)); //7970(0).
    model_ = std::move(stagedModel);
    *pixels_ = std::move(stagedPixels);
    *radar_ = std::move(stagedRadar);
    actorIncarnations_ = stagedIncarnations;
    actorTypes_ = stagedTypes;
    pages_.selectScoreCopy();
    incomingIndicator_ = states.back();
    pageScore_ = model_.score();
    pageStatus_ = model_.level1State().systemStatusLevels();
    frame_.pixels = pixels_->snapshot(pages_.frontPage());
}

bool NativeGameplaySession::supportsRetainedPages() const noexcept {
    const bool finalDisplay = model_.pendingLevel2FailureTail();
    if ((model_.phase() != GameplayPhase::level2Handoff && model_.phase() != GameplayPhase::level2
            && model_.phase() != GameplayPhase::level2SuccessPresentation
            && model_.phase() != GameplayPhase::level5 && model_.phase() != GameplayPhase::level3Native && !finalDisplay)
        || !assets_.cockpitSettings || (audio_->playablePreferences_ ? !audio_->playableDetailProfile()
            : assets_.cockpitSettings->viewSize <= cockpitOverlayStage)) {
        return false;
    }
    if (finalDisplay && (!pixels_ || !radar_ || level1Pages_
        || model_.playerArmor() < minimumLethalArmor || model_.playerArmor() >= 0
        || assets_.cockpitDacPalette.size() != vgaPaletteBytes
        || std::any_of(assets_.cockpitDacPalette.begin(), assets_.cockpitDacPalette.end(),
            [](const auto component) { return component > vgaComponentMaximum; }))) {
        return false;
    }
    const auto& player = model_.level2Player();
    const auto& systems = model_.level1State();
    if (player.pitch != 0 || player.roll != 0 || player.position.z != 0
        || player.yawVelocity < -maximumSteeringVelocity || player.yawVelocity > maximumSteeringVelocity
        || player.forwardVelocity < -maximumForwardVelocity || player.forwardVelocity > maximumForwardVelocity
        || player.weaponCooldown > maximumWeaponCooldown || systems.selectedSystem() != -1
        || player.weaponSlotTimers[2] != 0
        || std::any_of(player.weaponSlotTimers.begin(), player.weaponSlotTimers.begin() + 2,
            [](const auto timer) { return timer > maximumCannonSlotTimer; })
        || std::any_of(systems.systemTimers().begin(), systems.systemTimers().end(),
            [](const auto timer) { return timer != 0; })) {
        return false;
    }
    const auto& actors = model_.level2PresentationEntities();
    if ((!pixels_ && actors.size() != retainedActorCount)
        || (model_.campaignLevel() != 4 && actors.size() < retainedFixedCount)
        || actors.size() > ordinaryPoolSize_ + resetFixedSlotCount + projectilePoolSize + effectPoolSize
        || model_.level2PresentationActiveListOrder().size() != actors.size()
        || std::any_of(actors.begin(), actors.end(), [this, &player, &actors](const auto& actor) {
            const auto relativeX = static_cast<std::int64_t>(actor.position.x) - player.position.x;
            const auto relativeY = static_cast<std::int64_t>(actor.position.y) - player.position.y;
            const auto category = static_cast<std::int8_t>(actor.stateBytes4dTo52[0]);
            const bool ordinary = actor.renderProfile == WorldActorRenderProfile::noHandles
                && (((actor.type == 8 || actor.type == 13 || actor.type == 30)
                        && (category == ordinaryRadarCategory || category == fixedRadarCategory))
                    || ((actor.type == 1 || actor.type == 4
                            || (model_.campaignLevel() == 4 && actor.type == 5)) && category == ordinaryRadarCategory
                        && supportsRetainedModel(assets_, actor.type)
                        && actor.collisionRadius == assets_.models[actor.type].collisionRadius
                        && (actor.type != 5 || (actor.stateBytes4dTo52[1] <= 3
                            && actor.stateBytes4dTo52[4] == 1 && actor.stateBytes4dTo52[5] <= 5
                            && actor.weaponSlotCount == 3 && !actor.animatedModelPresenterEnabled))));
            const bool projectile = actor.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback
                && (std::find(retainedProjectileTypes.begin(), retainedProjectileTypes.end(), actor.type)
                    != retainedProjectileTypes.end()
                    || (model_.campaignLevel() == 4 && actor.type == 22 && actor.position.z == 0
                        && actor.stateBytes4dTo52[1] <= 84
                        && (actor.stateBytes4dTo52[2] == 0
                            || (actor.stateBytes4dTo52[2] == 1 && actor.stateBytes4dTo52[1] <= 64))
                        && actor.stateBytes4dTo52[3] == 0 && actor.stateBytes4dTo52[4] == 0
                        && actor.stateBytes4dTo52[5] == 0 && actor.weaponSlotCount == 0))
                && category == projectileRadarCategory && supportsRetainedModel(assets_, actor.type)
                && actor.collisionRadius == assets_.models[actor.type].collisionRadius;
            const bool airborneProjectile = projectile && actor.type == airborneProjectileType;
            const bool craft = supportsCompletionCraft(assets_, actor,
                model_.level2CompletionBeam(static_cast<std::size_t>(&actor - actors.data())));
            const bool type11 = model_.campaignLevel() == 4 && supportsType11(assets_, actor);
            const bool guardian = supportsGuardian(assets_, actor, model_.campaignLevel());
            const bool guardianFixed = supportsGuardianFixed(assets_, actor);
            const bool inlineFamily = supportsInlineFamily(assets_, actor);
            const bool pickup = supportsCompletionPickup(assets_, actor);
            const bool bonus = model_.campaignLevel() == 4 && supportsSystemBonus(assets_, actor);
            const bool level3 = model_.phase() == GameplayPhase::level3Native && supportsLevel3Actor(assets_, actor);
            const auto frame = actor.stateBytes4dTo52[1];
            const auto terminal = actor.stateBytes4dTo52[4];
            const auto lethalOffset = static_cast<int>(terminal) - firstLethalEffectTerminal;
            const bool normalLethal = lethalOffset >= 0
                && lethalOffset % lethalEffectFrameCount == 0
                && lethalOffset / lethalEffectFrameCount <= maximumCompleteLethalBucket
                && frame >= terminal - (lethalEffectFrameCount - 1);
            const bool nonlethal = terminal == nonlethalEffectTerminal && frame >= 2;
            const bool effect = actor.type == -1 && category == -4
                && actor.renderProfile == WorldActorRenderProfile::bitmapEffectCallback
                && (actor.flags == 0x88 || actor.flags == 0x8c) && actor.collisionRadius == 0
                && (nonlethal || normalLethal) && frame <= terminal
                && actor.position.z >= 0 && actor.position.z <= inlineLaunchAltitude
                && actor.position.z % inlineDescentStep == 0
                && assets_.artGroups.size() > effectBitmapGroup
                && assets_.artGroups[effectBitmapGroup].size() >= terminal;
            return !actor.active() || (!ordinary && !projectile && !effect && !craft && !guardian && !guardianFixed && !inlineFamily && !pickup && !type11 && !bonus && !level3)
                || (!effect && !craft && actor.flags != 0)
                || (!effect && actor.collisionRadius == 0) || actor.collisionRadius > maximumRetainedRadarRadius
                || (!craft && (actor.position.z < 0 || actor.position.z > maximumRetainedRadarAltitude
                    || (actor.position.z != 0 && actor.type != 13 && !inlineFamily && !effect && !type11 && !airborneProjectile && !bonus)))
                || relativeX < -maximumRadarRelativeCoordinate || relativeX > maximumRadarRelativeCoordinate
                || relativeY < -maximumRadarRelativeCoordinate || relativeY > maximumRadarRelativeCoordinate;
        })) {
        return false;
    }
    if (!pixels_ || (level1Pages_ && model_.phase() == GameplayPhase::level2Handoff)) {
        // The explicit campaign loader replaces allocation identities, retaining the pages.
        return true;
    }
    std::array<bool, retainedActorCount> seen{};
    std::array<bool, ordinaryPoolSize_> ordinarySlots{};
    std::array<bool, projectilePoolSize> projectiles{};
    std::array<bool, effectPoolSize> effects{};
    std::array<bool, fixedPoolSize> fixed{};
    for (std::size_t index = 0; index < actors.size(); ++index) {
        const auto slot = model_.level2PresentationPoolSlot(index);
        if (!slot) {
            return false;
        }
        if (*slot >= projectilePoolBase && *slot < projectilePoolBase + projectilePoolSize) {
            if (actors[index].renderProfile != WorldActorRenderProfile::inlineSelectorNoCallback
                || projectiles[*slot - projectilePoolBase]) {
                return false;
            }
            projectiles[*slot - projectilePoolBase] = true;
            continue;
        }
        if (*slot >= effectPoolBase && *slot < effectPoolBase + effectPoolSize) {
            if (actors[index].renderProfile != WorldActorRenderProfile::bitmapEffectCallback
                || effects[*slot - effectPoolBase]) {
                return false;
            }
            effects[*slot - effectPoolBase] = true;
            continue;
        }
        if (*slot < ordinaryPoolSize_) {
            const auto incarnation = model_.level2PresentationActorIncarnation(index);
            if (!incarnation || *incarnation == 0 || ordinarySlots[*slot]
                || *incarnation < actorIncarnations_[*slot]) {
                return false;
            }
            ordinarySlots[*slot] = true;
            if (*incarnation == actorIncarnations_[*slot]) {
                if (actors[index].type != actorTypes_[*slot]
                    && !((actorTypes_[*slot] == guardianType || actorTypes_[*slot] == openGuardianType)
                        && supportsGuardian(assets_, actors[index], model_.campaignLevel()))
                    && !(actorTypes_[*slot] == descendingActorType && actors[index].type == chasingActorType
                        && supportsInlineFamily(assets_, actors[index]))) {
                    return false;
                }
            } else if (!supportsCompletionCraft(assets_, actors[index], model_.level2CompletionBeam(index))
                && !(model_.campaignLevel() == 4 && supportsType11(assets_, actors[index]))
                && !supportsGuardian(assets_, actors[index], model_.campaignLevel())
                && !supportsInlineFamily(assets_, actors[index])
                && ((actors[index].type != 1 && actors[index].type != 4
                    && !(model_.campaignLevel() == 4 && actors[index].type == 5)
                    && actors[index].type != 8 && actors[index].type != 13)
                || actors[index].renderProfile != WorldActorRenderProfile::noHandles
                || actors[index].stateBytes4dTo52[0] != 0xff
                || !supportsRetainedModel(assets_, actors[index].type)
                || actors[index].collisionRadius != assets_.models[actors[index].type].collisionRadius)) {
                return false;
            }
            const auto initial = std::find(initialPoolSlots_.begin(), initialPoolSlots_.end(), *slot);
            if (initial != initialPoolSlots_.end()) {
                seen[static_cast<std::size_t>(initial - initialPoolSlots_.begin())] = true;
            }
            continue;
        }
        if (*slot < fixedPoolBase || *slot >= fixedPoolBase + resetFixedSlotCount
            || fixed[*slot - fixedPoolBase]) {
            return false;
        }
        fixed[*slot - fixedPoolBase] = true;
        const auto position = std::find(initialPoolSlots_.begin(), initialPoolSlots_.end(), *slot);
        if (position == initialPoolSlots_.end()) {
            if (!supportsGuardianFixed(assets_, actors[index]) && !supportsCompletionPickup(assets_, actors[index])
                && !(model_.campaignLevel() == 4 && supportsSystemBonus(assets_, actors[index]))) {
                return false;
            }
            continue;
        }
        const auto identity = static_cast<std::size_t>(position - initialPoolSlots_.begin());
        if (seen[identity] || actors[index].type != initialActorTypes_[identity]) {
            return false;
        }
        seen[identity] = true;
    }
    for (std::size_t index = 0; index < initialActorCount_; ++index) {
        if (!seen[index] && initialActorTypes_[index] != 8 && initialActorTypes_[index] != 13
            && !(model_.campaignLevel() == 4 && initialActorTypes_[index] == 11)) {
            return false;
        }
    }
    for (std::size_t index = 0; index < pageStatus_.size(); ++index) {
        const auto status = systems.systemStatusLevels()[index];
        if (status != pageStatus_[index] && (model_.campaignLevel() != 4
            || status != 3 || pageStatus_[index] < 0)) {
            return false;
        }
    }
    return true;
}

bool NativeGameplaySession::supportsLevel1Pages() const noexcept {
    return supportsLevel1Pages(model_);
}

bool NativeGameplaySession::supportsLevel1Pages(const GameplayModel& model) const noexcept {
    if (model.phase() != GameplayPhase::level1 || !assets_.cockpitSettings
        || (audio_->playablePreferences_ ? !audio_->playableDetailProfile()
            : assets_.cockpitSettings->viewSize <= cockpitOverlayStage) || assets_.scoreFont.glyphs.empty()) {
        return false;
    }
    const auto& state = model.level1State();
    const auto& player = state.player();
    if ((player.yaw & 0x000fU) != 0 || player.pitch != 0 || player.roll != 0 || player.position.z != 0
        || state.selectedSystem() != -1 || model.playerArmor() > maximumArmor
        || (model.playerArmor() < 0 && (!pixels_ || !level1Pages_ || !model.pendingLevel1FailureTail()))
        || (model.playerArmor() < 0 && (assets_.cockpitDacPalette.size() != vgaPaletteBytes
            || std::any_of(assets_.cockpitDacPalette.begin(), assets_.cockpitDacPalette.end(),
                [](const auto component) { return component > vgaComponentMaximum; })))
        || std::any_of(state.systemTimers().begin(), state.systemTimers().end(),
            [](const auto timer) { return timer != 0; })) {
        return false;
    }
    for (const auto& actor : state.radarPresentationObjects()) {
        const auto relativeX = static_cast<std::int64_t>(actor.position.x) - player.position.x;
        const auto relativeY = static_cast<std::int64_t>(actor.position.y) - player.position.y;
        if (actor.category < firstRadarCategory || actor.category > lastRadarCategory
            || actor.position.z < -maximumRadarAltitude || actor.position.z > maximumRadarAltitude
            || relativeX < -maximumRadarRelativeCoordinate || relativeX > maximumRadarRelativeCoordinate
            || relativeY < -maximumRadarRelativeCoordinate || relativeY > maximumRadarRelativeCoordinate) {
            return false;
        }
    }
    return true;
}

void NativeGameplaySession::initializePages(const NativeSessionStart level) {
    const auto layout=retainedPageLayout(pages_);
    const auto& record = model_.lastCockpitInitialization();
    const auto playableLoader = model_.phase() == GameplayPhase::playableLoaderReturn;
    const auto level1 = level == NativeSessionStart::level1 && !playableLoader;
    const auto level3 = level == NativeSessionStart::level3;
    const auto level5 = level == NativeSessionStart::level5;
    const auto levelIndex = campaignStartIndex(level);
    const auto loaderOnly = playableLoader || level3 || level5 || (level1 && (model_.phase() == GameplayPhase::level1BeginHandoff
        || model_.phase() == GameplayPhase::level1RetryHandoff));
    if (record.levelIndex != levelIndex
        || record.displaySteps != (loaderOnly ? 2 : 3) || record.states.back().x != model_.level2CockpitIndicatorX()
        || record.states.back().velocity != model_.level2CockpitIndicatorVelocity()) {
        throw std::logic_error("native campaign cockpit initialization mismatch");
    }
    cockpitBitmaps_.strip = assets_.artGroups.at(panelGroup).at(stripIndex);
    cockpitBitmaps_.indicator = assets_.artGroups.at(overlayGroup).at(indicatorIndex);
    cockpitBitmaps_.matchingPageBackground = assets_.artGroups.at(panelGroup).at(backgroundIndex);
    for (std::size_t index = 0; index < cockpitBitmaps_.overlays.size(); ++index) {
        cockpitBitmaps_.overlays[index] = assets_.artGroups.at(overlayGroup).at(index);
    }
    // The model records loader calls and, where admitted, the first ordinary draw.
    std::array<CockpitWork, 3> draws;
    const auto settings = renderSettings();
    const auto cockpit = hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::fullGraphicDetail)
        ? playableFullCockpitOperations : audio_->playablePreferences_ ? playableCockpitOperations : cockpitOperations;
    for (std::size_t index = 0; index < record.displaySteps; ++index) {
        const auto incoming = record.states[index];
        draws[index] = cockpit({incoming.x, incoming.velocity, settings.viewSize,
            index == 0 ? CockpitPageRelation::matching : CockpitPageRelation::differing}, cockpitBitmaps_);
        if (draws[index].state.indicatorX != record.states[index + 1].x
            || draws[index].state.indicatorVelocity != record.states[index + 1].velocity) {
            throw std::logic_error("native cockpit projection advanced a different indicator");
        }
    }
    if (!pixels_) {
        pixels_.emplace();
    }
    radar_.emplace(assets_.artGroups.at(panelGroup));
    pixels_->apply(cockpitResourcePageEffects(assets_.cockpit, pages_.cachePage(),layout));
    //4E06 selects12CC. Every reached5C74 exit, including Escape, restores12CA.
    const bool storyProfile=hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::storyPanels);
    const bool primaryTitle=storyProfile
        ? audio_->playableStoryEnabled() && campaignStoryScene(levelIndex).has_value()
        : !level1 && !loaderOnly && assets_.cockpitSettings->options[introductionOption]!=0;
    const auto& font=primaryTitle ? assets_.titleFont : assets_.scoreFont;
    pixels_->apply(cockpitCachePageEffects(draws[0], draws[1], cockpitBitmaps_, font,
        campaignTitles.at(levelIndex), titleColor,
        pages_.cachePage(), pages_.backPage(),layout,playableCampaign_
            ? CockpitCacheDomain::retainedGameplay : CockpitCacheDomain::initialLoader));
    radar_->resetArmorStrip();
    actorIncarnations_.fill(0);
    actorTypes_.fill(0);
    initialActorCount_ = level1 ? 0 : model_.level2PresentationEntities().size();
    if (initialActorCount_ > initialPoolSlots_.size()) {
        throw std::logic_error("native initial campaign allocation capacity exceeded");
    }
    if (playableLoader) {
        initialPoolSlots_.fill(static_cast<std::size_t>(-1));
        initialActorTypes_.fill(-1);
    }
    for (std::size_t index = 0; !level1 && index < model_.level2PresentationEntities().size(); ++index) {
        const auto slot = model_.level2PresentationPoolSlot(index);
        if (!slot) {
            throw std::logic_error("native initial radar allocation has no source identity");
        }
        initialPoolSlots_[index] = *slot;
        initialActorTypes_[index] = model_.level2PresentationEntities()[index].type;
        if (*slot < ordinaryPoolSize_) {
            const auto incarnation = model_.level2PresentationActorIncarnation(index);
            if (!incarnation || *incarnation == 0) {
                throw std::logic_error("native initial ordinary allocation has no incarnation");
            }
            actorIncarnations_[*slot] = *incarnation;
            actorTypes_[*slot] = initialActorTypes_[index];
        }
        radar_->initializeAllocation(*slot);
    }
    for (std::size_t slot = level5 || playableLoader ? 0 : retainedFixedCount; !level1 && slot < resetFixedSlotCount; ++slot) {
        radar_->initializeAllocation(fixedPoolBase + slot);
    }
    //48d4 cleared every projectile record before44ba's later partial activations.
    for (std::size_t slot = 0; !level1 && slot < projectilePoolSize; ++slot) {
        radar_->initializeAllocation(projectilePoolBase + slot);
    }
    for (std::size_t slot = 0; !level1 && slot < effectPoolSize; ++slot) {
        radar_->initializeAllocation(effectPoolBase + slot);
    }
    pages_.beginFrame();
    pages_.selectCockpitCopy();
    const auto& player = level1 ? model_.level1State().player() : model_.level2Player();
    const auto startup = radar_->initialize(model_.playerArmor(), player.position, player.yaw, radarRoles(pages_),retainedRadarLayout(pages_));
    pixels_->apply(startup);
    if (startup.pageCopy) {
        pages_.selectArmorCopy();
    }
    pixels_->apply(std::vector<IndexedPageEffect>{RadarPageCopy{pages_.backPage(), pages_.frontPage(),
        {0, 0}, screenWidth, screenHeight}});
    const auto& systems = model_.level1State();
    if (!loaderOnly) {
        pixels_->apply(scorePageEffects(model_.score(), assets_.scoreFont, pages_.backPage(), pages_.frontPage(),layout));
        pixels_->apply(statusPageEffects(systems.systemStatusLevels(), systems.systemTimers(), systems.selectedSystem(),
            assets_.artGroups.at(overlayGroup), pages_.backPage(), pages_.frontPage(),layout));
    }
    pageScore_ = level5 || level3 || playableLoader ? 0 : model_.score();
    pageStatus_ = systems.systemStatusLevels();
    incomingIndicator_ = record.states[2];
    level1Pages_ = level1;
    failurePages_ = false;
    continuePages_ = false;
    //7bae's dirty word survives a cockpit/page reload.
}

void NativeGameplaySession::renderLevel1Pages() {
    auto viewport = renderLevel1ViewportIndexed(assets_, model_);
    pages_.beginFrame();
    pages_.selectCockpitCopy();
    for (auto y = worldViewport.top; y <= worldViewport.bottom; ++y) {
        for (auto x = worldViewport.left; x <= worldViewport.right; ++x) {
            pixels_->write(pages_.backPage(), {x, y}, viewport.pixels[y * screenWidth + x]);
        }
    }
    const auto drawCockpit = hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::fullGraphicDetail)
        ? playableFullCockpitOperations : audio_->playablePreferences_ ? playableCockpitOperations : cockpitOperations;
    const auto cockpit = drawCockpit({incomingIndicator_.x, incomingIndicator_.velocity,
        renderSettings().viewSize, CockpitPageRelation::differing}, cockpitBitmaps_);
    if (cockpit.state.indicatorX != model_.level2CockpitIndicatorX()
        || cockpit.state.indicatorVelocity != model_.level2CockpitIndicatorVelocity()) {
        throw std::logic_error("native Level1 cockpit projection differs from its model");
    }
    pixels_->apply(cockpitPageEffects(cockpit, cockpitBitmaps_, pages_.cachePage(), pages_.backPage()));
    renderLevel1Radar();
    const auto& state = model_.level1State();
    if (model_.score() != pageScore_) {
        pixels_->apply(scorePageEffects(model_.score(), assets_.scoreFont, pages_.backPage(), pages_.frontPage()));
        pageScore_ = model_.score();
    }
    if (state.systemStatusLevels() != pageStatus_) {
        pixels_->apply(statusPageEffects(state.systemStatusLevels(), state.systemTimers(), state.selectedSystem(),
            assets_.artGroups.at(overlayGroup), pages_.backPage(), pages_.frontPage()));
        pageStatus_ = state.systemStatusLevels();
    }
    pages_.flip();
    renderWeaponLamp();
    for (std::int16_t y = 0; y < screenHeight; ++y) {
        for (std::int16_t x = 0; x < screenWidth; ++x) {
            viewport.pixels[y * screenWidth + x] = pixels_->read(pages_.frontPage(), {x, y});
        }
    }
    frame_ = std::move(viewport);
    pageProjection_ = NativePageProjection::retainedNativeLevel1;
}

void NativeGameplaySession::renderLevel1Radar() {
    const auto& state = model_.level1State();
    const auto& player = state.player();
    // Reuse source panel/armor effects; model-owned animation callbacks stay deferred.
    const auto radar = radar_->draw(model_.playerArmor(), player.position, player.yaw, {}, radarRoles(pages_), {0, 0});
    const auto& panel = radar_->cache().frames.front().bitmap;
    const auto panelPixels = static_cast<std::size_t>(panel.width) * panel.height;
    for (std::size_t index = 0; index < panelPixels; ++index) {
        const auto& pixel = radar.pixels[index];
        pixels_->write(pixel.page, pixel.point, pixel.color);
    }
    constexpr Viewport markerViewport{112, 206, 146, 179};
    const auto divide = [](const std::int32_t coordinate, const std::int32_t depth) {
        const auto numerator = static_cast<std::int64_t>(coordinate) * projectionScale;
        const auto magnitude = numerator < 0 ? -numerator : numerator;
        const auto quotient = (magnitude >> 2) / (depth >> 2);
        return numerator < 0 ? -quotient : quotient;
    };
    const auto& view = radar.setup.view;
    for (const auto& object : state.radarPresentationObjects()) {
        const auto center = transformToView(object.position, view);
        if (center.y < minimumProjectionDepth || center.y >= maximumProjectionDepth) {
            throw std::logic_error("Level1 radar depth outside admitted projection");
        }
        const auto x = view.viewport.left + (view.viewport.right - view.viewport.left) / 2 + divide(center.x, center.y);
        const auto y = view.viewport.top + (view.viewport.bottom - view.viewport.top) / 2 - divide(center.z, center.y);
        if (x < view.viewport.left || x > view.viewport.right || y < view.viewport.top || y > view.viewport.bottom) {
            continue;
        }
        const auto color = object.category == -2 && object.type == completionPickupType
            ? completionPickupColor : radarColors[object.category - firstRadarCategory];
        const auto marker = rasterPointOperations({static_cast<std::int16_t>(x - 4), static_cast<std::int16_t>(y)}, markerViewport);
        for (const auto point : marker.pixels) {
            pixels_->write(pages_.backPage(), point, color);
        }
    }
    for (std::size_t index = panelPixels; index < radar.pixels.size(); ++index) {
        const auto& pixel = radar.pixels[index];
        pixels_->write(pixel.page, pixel.point, pixel.color);
    }
    if (radar.pageCopy) {
        pixels_->apply(std::vector<IndexedPageEffect>{*radar.pageCopy});
        pages_.selectArmorCopy();
    }
}

void NativeGameplaySession::renderWeaponLamp() {
    // Level2 pages stop before the current fire/timer tail; the clean presenter
    // predicts that later tail. Retained pages consume the already executed one.
    const auto& player = model_.level2Player();
    const auto ready = level1Pages_ ? model_.weaponReadyIndicator()
        : player.weaponCooldown == 0 && (player.weaponSlotTimers[0] == 0 || player.weaponSlotTimers[1] == 0);
    if (model_.weaponIndicatorVisible() && (!pageLampVisible_ || ready != pageLampReady_)) {
        const auto& lamp = assets_.artGroups.at(overlayGroup).at(ready ? readyLamp : unavailableLamp);
        constexpr ProjectedPoint lampPosition{243, 170};
        for (std::uint16_t y = 0; y < lamp.height; ++y) {
            for (std::uint16_t x = 0; x < lamp.width; ++x) {
                const auto color = lamp.pixels[y * lamp.width + x];
                if (color != 0) {
                    pixels_->write(pages_.backPage(), {static_cast<std::int16_t>(lampPosition.x + x),
                        static_cast<std::int16_t>(lampPosition.y + y)}, color);
                }
            }
        }
        const auto copy = planarCopyWork(lampPosition.x, lampCopyWidth, lampCopyHeight);
        pixels_->apply(std::vector<IndexedPageEffect>{RadarPageCopy{pages_.backPage(), pages_.frontPage(),
            {static_cast<std::int16_t>(lampPosition.x - lampPosition.x % 4), lampPosition.y},
            static_cast<std::uint16_t>(4 * copy.stringIterations / lampCopyHeight), lampCopyHeight}});
        //7bae leaves the same back-copy/front-draw selection as the armor copy.
        pages_.selectArmorCopy();
        pageLampVisible_ = true;
        pageLampReady_ = ready;
        if (model_.phase() == GameplayPhase::level5) {
            //Level5 executes its fire/timer tail after refreshFrame. Deliver the
            //same physical7bae copy without repeating main/radar or a clock step.
            const auto left = static_cast<std::int16_t>(lampPosition.x - lampPosition.x % 4);
            const auto width = static_cast<std::uint16_t>(4 * copy.stringIterations / lampCopyHeight);
            for (std::int16_t y = lampPosition.y; y < lampPosition.y + lampCopyHeight; ++y) {
                for (std::int16_t x = left; x < left + width; ++x) {
                    frame_.pixels[y * screenWidth + x] = pixels_->read(pages_.frontPage(), {x, y});
                }
            }
        }
    }
}

std::vector<RadarStateActor> NativeGameplaySession::level2RadarActors() const {
    std::vector<RadarStateActor> actors;
    const auto sharedCount=playableCampaign_ ? level1RewardModelCapacity(assets_) : 0;
    for (const auto index : model_.level2PresentationActiveListOrder()) {
        const auto& actor = model_.level2PresentationEntities().at(index);
        if (playableCampaign_ && actor.type>=0) {
            if (!campaignResources_ || !campaignResources_->models) {
                throw std::logic_error("playable radar lost its decoded model bank");
            }
            const auto& local=*campaignResources_->models;
            const auto radiusFor=[&](const std::uint16_t type) {
                return type>=sharedCount ? campaignModelRadius({},local,static_cast<std::uint16_t>(type-sharedCount))
                    : campaignModelRadius(assets_.models,{},type);
            };
            auto radius=radiusFor(static_cast<std::uint16_t>(actor.type));
            // Opening the guardian and transforming Type14 retain the collision
            // body that their constructor allocated before changing the type.
            if (actor.type==openGuardianType) {
                radius=radiusFor(guardianType);
            } else if (actor.type==15 || actor.type==16) {
                radius=radiusFor(14);
            } else if (actor.type==chasingActorType || actor.type==descendingActorType) {
                radius=radiusFor(13);
            }
            if (actor.renderProfile==WorldActorRenderProfile::unknown || actor.collisionRadius!=radius) {
                throw std::invalid_argument("playable radar actor disagrees with its decoded collision model");
            }
        }
        const auto slot = model_.level2PresentationPoolSlot(index);
        if (!slot) {
            throw std::logic_error("native radar presentation lost its source identity");
        }
        const auto effect = actor.renderProfile == WorldActorRenderProfile::bitmapEffectCallback;
        const auto flags = effect ? radar_->projection(*slot).flags : actor.flags;
        const auto callback = actor.renderProfile == WorldActorRenderProfile::inlineSelectorBeamCallback
            ? completionBeamCallback
            : actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation ? guardianAnimationCallback
            : effect && actor.stateBytes4dTo52[1] < actor.stateBytes4dTo52[4] ? effectCallback : 0;
        actors.push_back({*slot, {flags, actor.position}, actor.collisionRadius,
            actor.type, static_cast<std::int8_t>(actor.stateBytes4dTo52[0]),
            static_cast<std::uint16_t>(callback), actor.renderProfile});
    }
    return actors;
}

bool NativeGameplaySession::renderLevel2Pages(std::vector<NormalWorkOperation>* const cockpitWork) {
    const auto layout=retainedPageLayout(pages_);
    //Deliver each full ordinary-record reset before either presenter uses its cache.
    for (std::size_t index = 0; index < model_.level2PresentationEntities().size(); ++index) {
        const auto slot = model_.level2PresentationPoolSlot(index);
        const auto incarnation = model_.level2PresentationActorIncarnation(index);
        if (slot && *slot < ordinaryPoolSize_ && incarnation && actorIncarnations_[*slot] != *incarnation) {
            radar_->initializeAllocation(*slot);
            actorIncarnations_[*slot] = *incarnation;
        }
        if (slot && *slot < ordinaryPoolSize_) {
            actorTypes_[*slot] = model_.level2PresentationEntities()[index].type;
        }
    }
    const auto renderViewport = [this](const GameplayAssets& assets, const GameplayModel& model,
        const EffectProjector& effectProjector, const BeamProjectionSink& beamProjection) {
        return playableCampaign_ && hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::fullGraphicDetail)
            ? renderFullDetailPlayableLevel2ViewportIndexed(assets,model,effectProjector,beamProjection,renderSettings().viewSize)
            : playableCampaign_ ? renderPlayableLevel2ViewportIndexed(assets,model,effectProjector,beamProjection,renderSettings().viewSize)
            : renderLevel2ViewportIndexed(assets,model,effectProjector,beamProjection);
    };
    auto viewport = renderViewport(assets_, model_, [this](const auto index, const auto& view)
        -> std::optional<ProjectedPoint> {
        const auto slot = model_.level2PresentationPoolSlot(index);
        if (!slot) {
            throw std::logic_error("bitmap effect has no physical identity");
        }
        const auto& actor = model_.level2PresentationEntities().at(index);
        const auto projected = radar_->projectBitmapEffect(*slot, {actor.flags, actor.position}, view,
            playableCampaign_ ? RadarPixelDomain::playableCampaign : RadarPixelDomain::retained);
        return projected.projected ? std::optional<ProjectedPoint>{{projected.projectedX, projected.projectedY}} : std::nullopt;
    }, [this](const auto index, const auto& beam) {
        const auto slot = model_.level2PresentationPoolSlot(index);
        if (!slot) {
            throw std::logic_error("completion beam has no physical identity");
        }
        radar_->retainCompletionBeam(*slot, beam);
    });

    const auto actors = level2RadarActors();
    const auto& player = model_.level2Player();
    auto drawRoles = radarRoles(pages_);
    drawRoles.draw = drawRoles.back;
    drawRoles.copy = drawRoles.cached;
    const auto radar = radar_->draw(model_.playerArmor(), player.position, player.yaw, actors, drawRoles, {0, 0},
        retainedRadarLayout(pages_),playableCampaign_ ? RadarPixelDomain::playableCampaign : RadarPixelDomain::retained);
    // Admitted main flags0 skip BB15..BBCB's actor cache stores. Radar owns the
    // retained X/radius on clipping;698a restores flags and clears Y afterward.

    pages_.beginFrame();
    pages_.selectCockpitCopy();
    for (auto y = worldViewport.top; y <= worldViewport.bottom; ++y) {
        for (auto x = worldViewport.left; x <= worldViewport.right; ++x) {
            pixels_->write(pages_.backPage(), {x, y}, viewport.pixels[y * screenWidth + x]);
        }
    }
    const auto drawCockpit = hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::fullGraphicDetail)
        ? playableFullCockpitOperations : audio_->playablePreferences_ ? playableCockpitOperations : cockpitOperations;
    const auto cockpit = drawCockpit({incomingIndicator_.x, incomingIndicator_.velocity,
        renderSettings().viewSize, CockpitPageRelation::differing}, cockpitBitmaps_);
    if (cockpit.state.indicatorX != model_.level2CockpitIndicatorX()
        || cockpit.state.indicatorVelocity != model_.level2CockpitIndicatorVelocity()) {
        throw std::logic_error("native cockpit draw does not match retained model");
    }
    if (cockpitWork) {
        *cockpitWork = cockpit.operations;
    }
    pixels_->apply(cockpitPageEffects(cockpit, cockpitBitmaps_, pages_.cachePage(), pages_.backPage()));
    pixels_->apply(radar);
    if (radar.pageCopy) {
        pages_.selectArmorCopy();
    }
    if (model_.score() != pageScore_) {
        pixels_->apply(scorePageEffects(model_.score(), assets_.scoreFont, pages_.backPage(), pages_.frontPage(),layout));
        pageScore_ = model_.score();
    }
    const auto& systems = model_.level1State();
    if (systems.systemStatusLevels() != pageStatus_) {
        for (std::size_t system = 0; system < pageStatus_.size(); ++system) {
            if (systems.systemStatusLevels()[system] != pageStatus_[system]) {
                pixels_->apply(statusRepairPageEffects(system, systems.systemStatusLevels(), systems.systemTimers(),
                    assets_.artGroups.at(overlayGroup), pages_.backPage(), pages_.frontPage(),layout));
            }
        }
        pageStatus_ = systems.systemStatusLevels();
    }
    pages_.flip();
    renderWeaponLamp();
    for (std::int16_t y = 0; y < screenHeight; ++y) {
        for (std::int16_t x = 0; x < screenWidth; ++x) {
            viewport.pixels[y * screenWidth + x] = pixels_->read(pages_.frontPage(), {x, y});
        }
    }
    frame_ = std::move(viewport);
    const bool translated = player.forwardVelocity != 0 || player.position.x != 0 || player.position.y != 0;
    pageProjection_ = model_.phase() == GameplayPhase::playable ? NativePageProjection::retainedPlayableCampaign
        : model_.phase() == GameplayPhase::level5 ? NativePageProjection::retainedLevel5
        : model_.phase() == GameplayPhase::level3Native ? NativePageProjection::retainedLevel3
        : translated ? NativePageProjection::retainedMovingLevel2
        : player.yaw == 0 && player.yawVelocity == 0 ? NativePageProjection::retainedStationaryLevel2
        : NativePageProjection::retainedSteeringLevel2;
    return true;
}

bool NativeGameplaySession::supportsSuccessPages() const noexcept {
    if (pixels_ && radar_ && level1Pages_ && model_.pendingLevel1SuccessContact()) {
        const auto& player = model_.level1State().player();
        const auto objects = model_.level1State().radarPresentationObjects();
        return assets_.cockpitSettings && (audio_->playablePreferences_ ? audio_->playableDetailProfile()
            : assets_.cockpitSettings->viewSize > cockpitOverlayStage)
            && !assets_.scoreFont.glyphs.empty() && (player.yaw & 0x000fU) == 0
            && player.pitch == 0 && player.roll == 0 && player.position.z == 0
            && std::all_of(objects.begin(), objects.end(), [&player](const auto& object) {
                    const auto relativeX = static_cast<std::int64_t>(object.position.x) - player.position.x;
                    const auto relativeY = static_cast<std::int64_t>(object.position.y) - player.position.y;
                    return object.category == fixedRadarCategory
                        && object.position.z >= -maximumRadarAltitude && object.position.z <= maximumRadarAltitude
                        && relativeX >= -maximumRadarRelativeCoordinate && relativeX <= maximumRadarRelativeCoordinate
                        && relativeY >= -maximumRadarRelativeCoordinate && relativeY <= maximumRadarRelativeCoordinate;
                });
    }
    return pixels_ && radar_ && !level1Pages_ && model_.phase() == GameplayPhase::level2SuccessPresentation
        && supportsRetainedPages()
        && std::count_if(model_.level2Entities().begin(), model_.level2Entities().end(),
            [](const auto& actor) { return actor.type == completionPickupType; }) == 1;
}

void NativeGameplaySession::renderSuccessPages() {
    const auto projection = level1Pages_ ? NativePageProjection::retainedLevel1Success
        : NativePageProjection::retainedLevel2Success;
    if (successPalette_) {
        pageProjection_ = projection;
        return;
    }
    //6fbb owns the one cockpit step already executed at pickup collision.
    pages_.selectCockpitDraw();
    const auto drawCockpit = hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::fullGraphicDetail)
        ? playableFullCockpitOperations : audio_->playablePreferences_ ? playableCockpitOperations : cockpitOperations;
    const auto cockpit = drawCockpit({incomingIndicator_.x, incomingIndicator_.velocity,
        renderSettings().viewSize, CockpitPageRelation::matching}, cockpitBitmaps_);
    if (cockpit.state.indicatorX != model_.level2CockpitIndicatorX()
        || cockpit.state.indicatorVelocity != model_.level2CockpitIndicatorVelocity()) {
        throw std::logic_error("success cockpit entry advanced a different indicator");
    }
    pixels_->apply(cockpitPageEffects(cockpit, cockpitBitmaps_, pages_.cachePage(), pages_.cachePage()));
    pages_.selectCockpitCopy();
    pages_.beginFrame();
    constexpr std::uint16_t successCopyHeight = 146;
    pixels_->apply(std::vector<IndexedPageEffect>{RadarPageCopy{
        pages_.cachePage(), pages_.backPage(), {0, 0}, screenWidth, successCopyHeight}});
    //7043/7046: existing list and camera; no ordinary world/callback execution.
    const auto& player = level1Pages_ ? model_.level1State().player() : model_.level2Player();
    const auto& order = level1Pages_ ? model_.level1State().activeListOrder() : model_.level2ActiveListOrder();
    const auto& entities = level1Pages_ ? model_.level1State().entities() : model_.level2Entities();
    const auto loopTick = level1Pages_ ? model_.level1State().loopTick() : model_.level2LoopTick();
    SightRefreshInput refresh{{}, player.yaw};
    for (const auto index : order) {
        const auto& actor = entities.at(index);
        refresh.targets.push_back({actor.type, actor.targetHeading});
    }
    constexpr std::uint8_t sightColor = 95;
    const auto sight = sightOperations(loopTick, model_.alternateSightPhase(),
        {24, 294, 24, 145}, sightColor, refresh);
    if (sight.alternatePhase != model_.alternateSightPhase()) {
        throw std::logic_error("success sight entry differs from model predicate");
    }
    for (const auto& pixel : sight.pixels) {
        pixels_->write(pages_.backPage(), pixel.point, pixel.color);
    }
    if (level1Pages_) {
        renderLevel1Radar();
    } else {
        const auto radar = radar_->draw(model_.playerArmor(), player.position, player.yaw,
            level2RadarActors(), radarRoles(pages_), {0, 0},retainedRadarLayout(pages_),
            playableCampaign_ ? RadarPixelDomain::playableCampaign : RadarPixelDomain::retained);
        pixels_->apply(radar);
    }
    //7049..706f/8484: full back->front copy; never22b1's swap.
    pages_.selectArmorCopy();
    pixels_->apply(std::vector<IndexedPageEffect>{RadarPageCopy{
        pages_.backPage(), pages_.frontPage(), {0, 0}, screenWidth, screenHeight}});
    for (std::int16_t y = 0; y < screenHeight; ++y) {
        for (std::int16_t x = 0; x < screenWidth; ++x) {
            frame_.pixels[y * screenWidth + x] = pixels_->read(pages_.frontPage(), {x, y});
        }
    }
    auto channel = SuccessPaletteChannel::green;
    if (!level1Pages_) {
        const auto contact = std::find_if(entities.begin(), entities.end(), [](const auto& actor) {
            return (actor.type == 25 || actor.type == 26) && actor.collisionContactIsPlayer;
        });
        if (contact == entities.end()) {
            throw std::logic_error("success palette requires its actual pickup contact");
        }
        channel = contact->type == 25 ? SuccessPaletteChannel::green : SuccessPaletteChannel::blue;
    }
    successPalette_ = initialSuccessPalette(channel);
    pageProjection_ = projection;
}

void NativeGameplaySession::finishLevel2FailureDisplay() {
    //The final snapshot was drawn before its deferred input/timer/lamp tail.
    model_.finishLevel2FailureTail();
    renderWeaponLamp();
    for (std::int16_t y = 0; y < screenHeight; ++y) {
        for (std::int16_t x = 0; x < screenWidth; ++x) {
            frame_.pixels[y * screenWidth + x] = pixels_->read(pages_.frontPage(), {x, y});
        }
    }
    setSourcePaletteIntensity(vgaComponentMaximum);
    failurePages_ = true;
    pageProjection_ = NativePageProjection::retainedLevel2FailureRestore;
}

void NativeGameplaySession::finishLevel1FailureDisplay() {
    model_.finishLevel1FailureTail();
    renderWeaponLamp();
    for (std::int16_t y = 0; y < screenHeight; ++y) {
        for (std::int16_t x = 0; x < screenWidth; ++x) {
            frame_.pixels[y * screenWidth + x] = pixels_->read(pages_.frontPage(), {x, y});
        }
    }
    setSourcePaletteIntensity(vgaComponentMaximum);
    failurePages_ = true;
    pageProjection_ = NativePageProjection::retainedLevel1FailureRestore;
}

void NativeGameplaySession::setSourcePaletteIntensity(const std::uint8_t intensity) {
    PaletteWorkState palette{};
    std::copy(assets_.cockpitDacPalette.begin(), assets_.cockpitDacPalette.end(), palette.source.begin());
    const auto restored = transformPalette(palette, {0, vgaPaletteColors, 0, intensity});
    const auto expanded = [&restored](const std::size_t color, const std::size_t component) {
        const auto value = restored.output[color * vgaPaletteComponents + component];
        return static_cast<std::uint8_t>((value << 2) | (value >> 4));
    };
    for (std::size_t color = 0; color < vgaPaletteColors; ++color) {
        frame_.palette[color] = {expanded(color, 0), expanded(color, 1), expanded(color, 2)};
    }
}

bool NativeGameplaySession::supportsFailurePages() const noexcept {
    const auto phase = model_.phase();
    const auto& view = model_.failureView().viewport;
    const auto originMatches = level1Pages_ ? !model_.failureFromLevel2() : model_.failureFromLevel2();
    return failurePages_ && pixels_ && radar_ && originMatches
        && (phase == GameplayPhase::failureExplosion || phase == GameplayPhase::failurePostExplosionBlackout
            || (level1Pages_ && phase == GameplayPhase::failurePreExplosionBlackout))
        && ((level1Pages_ && phase == GameplayPhase::failurePreExplosionBlackout)
            || (view.left == 0 && view.right == screenWidth - 2 && view.top == 0 && view.bottom == screenHeight - 1));
}

void NativeGameplaySession::renderFailurePages() {
    const auto layout=retainedPageLayout(pages_)==CockpitPageLayout::recoveredStartup
        ? BlackoutPageLayout::recoveredStartup : BlackoutPageLayout::compatibility;
    if (model_.phase() == GameplayPhase::failurePreExplosionBlackout) {
        pageProjection_ = NativePageProjection::retainedLevel1FailureRestore;
        return;
    }
    if (model_.phase() == GameplayPhase::failurePostExplosionBlackout) {
        pages_.selectFront();
        applyBlackoutClear(*pixels_, pages_.frontPage(),layout);
        std::fill(frame_.pixels.begin(), frame_.pixels.end(), 0);
        setSourcePaletteIntensity(finalBlackoutIntensity);
        pageProjection_ = level1Pages_ ? NativePageProjection::retainedLevel1DestructionClear
            : NativePageProjection::retainedLevel2DestructionClear;
        return;
    }
    if (model_.presentationFrame() == 0) {
        //77ce:9d4c front clear, then explicit front/back6622; cache survives.
        pages_.selectFront();
        applyBlackoutClear(*pixels_, pages_.frontPage(),layout);
        applyBlackoutClear(*pixels_, pages_.frontPage(),layout);
        pages_.beginFrame();
        applyBlackoutClear(*pixels_, pages_.backPage(),layout);
        //5bfe installs full hardware bounds before the first horizon.
        failureBackgroundClip_ = {0, screenWidth - 1, 0, screenHeight - 1};
    }
    auto destination = frame_;
    for (std::int16_t y = 0; y < screenHeight; ++y) {
        for (std::int16_t x = 0; x < screenWidth; ++x) {
            destination.pixels[y * screenWidth + x] = pixels_->read(pages_.backPage(), {x, y});
        }
    }
    auto rendered = playableCampaign_
        ? (hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::fullGraphicDetail)
            ? renderFullDetailPlayableFailurePageIndexed(assets_, model_, destination, failureBackgroundClip_, renderSettings().viewSize)
            : renderPlayableFailurePageIndexed(assets_, model_, destination, failureBackgroundClip_, renderSettings().viewSize))
        : renderFailurePageIndexed(assets_, model_, destination, failureBackgroundClip_);
    pages_.beginFrame();
    for (std::int16_t y = 0; y < screenHeight; ++y) {
        for (std::int16_t x = 0; x < screenWidth; ++x) {
            pixels_->write(pages_.backPage(), {x, y}, rendered.pixels[y * screenWidth + x]);
        }
    }
    //B7ac installs model bounds for the next horizon; driver9 performs no copy.
    failureBackgroundClip_ = model_.failureView().viewport;
    pages_.flip();
    frame_ = std::move(rendered);
    pageProjection_ = level1Pages_ ? NativePageProjection::retainedLevel1Destruction
        : NativePageProjection::retainedLevel2Destruction;
}

bool NativeGameplaySession::supportsContinuePages() const noexcept {
    return failurePages_ && pixels_ && radar_
        && (level1Pages_ ? !model_.failureFromLevel2() : model_.failureFromLevel2())
        && model_.phase() == GameplayPhase::continuePrompt;
}

bool NativeGameplaySession::supportsNameEntryPages() const noexcept {
    return pixels_ && radar_ && !level1Pages_ && failurePages_ && nameEntry_
        && model_.campaignLevel() == 4 && model_.phase() == GameplayPhase::highscoreNameEntry;
}

void NativeGameplaySession::renderNameEntryPages(const NameEntryPoll poll) {
    const auto& art = assets_.artGroups.at(0);
    pages_.beginModal();
    if (!nameEntryPages_) {
        if(playableCampaign_) {modalPointer_.reset();}
        pixels_->apply(highscoreEntryPageEffects(art, assets_.titleFont, pages_.backPage(), pages_.frontPage()));
        setSourcePaletteIntensity(vgaComponentMaximum);
    } else {
        if (nameOkPressed_ != incomingNameOkPressed_) {
            pixels_->apply(highscoreOkWidgetEffects(art, assets_.titleFont,
                incomingNameOkPressed_ ? MenuWidgetState::pressed : MenuWidgetState::released,
                pages_.backPage(), pages_.frontPage()));
        }
        const auto edit = [this](const std::uint8_t character) {
            const auto result = nameEntry_->edit(character);
            pixels_->apply(highscoreNameEditEffects(assets_.titleFont, *nameEntry_, result,
                pages_.backPage(), pages_.frontPage()));
        };
        //31d0 skips33e6 when5936 returns the accepted OK result.
        if (poll == NameEntryPoll::editing) {
            if (pendingNameInput_.characters.empty()) {
                edit(0);
            } else {
                for (const auto character : pendingNameInput_.characters) {
                    edit(character);
                }
            }
        }
    }
    pendingNameInput_.characters.clear();
    nameCaret_ = nameEntry_->caret();
    nameOkPressed_ = incomingNameOkPressed_;
    //5936's pointer is transient on back, copied to front, then restored on back.
    constexpr ProjectedPoint origin{161, 139};
    renderHighscorePointer(origin, highscorePointerPageEffects(art.at(0), pages_.backPage()));
    nameEntryPages_ = true;
    pageProjection_ = NativePageProjection::retainedHighscoreEntry;
}

void NativeGameplaySession::renderHighscorePointer(const ProjectedPoint origin,
    const std::vector<IndexedPageEffect>& effects) {
    if(playableCampaign_&&audio_->pointerCapability_==NativePointerCapability::portablePointer
        && (model_.phase()==GameplayPhase::highscoreNameEntry||model_.phase()==GameplayPhase::highscoreHall)) {
        pixels_->apply(std::vector<IndexedPageEffect>{RadarPageCopy{pages_.backPage(),pages_.frontPage(),{0,0},320,200}});
        paintPlayableMenuPointer(*pixels_,assets_.artGroups.at(0).at(0),modalPointer_.value_or(origin),
            pages_.backPage(),pages_.frontPage());
        frame_.pixels=pixels_->snapshot(pages_.frontPage());
        return;
    }
    std::array<std::uint8_t, continuePointerWidth * continuePointerHeight> background{};
    for (std::uint16_t y = 0; y < continuePointerHeight; ++y) {
        for (std::uint16_t x = 0; x < continuePointerWidth; ++x) {
            background[y * continuePointerWidth + x] = pixels_->read(pages_.backPage(),
                {static_cast<std::int16_t>(origin.x + x), static_cast<std::int16_t>(origin.y + y)});
        }
    }
    pixels_->apply(effects);
    pages_.selectFront();
    pixels_->apply(std::vector<IndexedPageEffect>{RadarPageCopy{pages_.backPage(), pages_.frontPage(),
        origin, continuePointerWidth, continuePointerHeight}});
    pages_.beginModal();
    for (std::uint16_t y = 0; y < continuePointerHeight; ++y) {
        for (std::uint16_t x = 0; x < continuePointerWidth; ++x) {
            pixels_->write(pages_.backPage(),
                {static_cast<std::int16_t>(origin.x + x), static_cast<std::int16_t>(origin.y + y)},
                background[y * continuePointerWidth + x]);
        }
    }
    for (std::int16_t y = 0; y < screenHeight; ++y) {
        for (std::int16_t x = 0; x < screenWidth; ++x) {
            frame_.pixels[y * screenWidth + x] = pixels_->read(pages_.frontPage(), {x, y});
        }
    }
}

bool NativeGameplaySession::supportsHallPages() const noexcept {
    return pixels_ && radar_ && !level1Pages_ && failurePages_ && nameEntry_
        && !nameEntry_->name().empty() && highscoreTable_
        && model_.campaignLevel() == 4 && model_.phase() == GameplayPhase::highscoreHall;
}

void NativeGameplaySession::renderHallPages() {
    const auto& art = assets_.artGroups.at(0);
    pages_.beginModal();
    if (!hallPages_) {
        if(playableCampaign_) {modalPointer_.reset();}
        pixels_->apply(highscoreHallPageEffects(art, assets_.titleFont, *highscoreTable_,
            pages_.backPage(), pages_.frontPage()));
        setSourcePaletteIntensity(vgaComponentMaximum);
    } else if (hallContinuePressed_ != incomingHallContinuePressed_) {
        pixels_->apply(highscoreHallWidgetEffects(art, assets_.titleFont,
            incomingHallContinuePressed_ ? MenuWidgetState::pressed : MenuWidgetState::released,
            pages_.backPage(), pages_.frontPage()));
    }
    hallContinuePressed_ = incomingHallContinuePressed_;
    hallPages_ = true;
    renderHighscorePointer({164, 173}, highscoreHallPointerPageEffects(art.at(0), pages_.backPage()));
    pageProjection_ = NativePageProjection::retainedHighscoreHall;
}

bool NativeGameplaySession::supportsLevel2Retry(const GameplayInput& input) const noexcept {
    //330a returns without changing the score table only below every entry.
    return !level1Pages_ && supportsContinuePages() && continuePages_ && assets_.hallOfFameScores
        && model_.pressedContinueChoice() == ContinueChoice::yes
        && !input.confirm && !input.fire && !input.exit
        && !input.forward && !input.reverse && !input.left && !input.right
        && model_.score() <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
        && (!highscoreTable_ || !highscoreTable_->rank(model_.score()))
        && std::all_of(assets_.hallOfFameScores->begin(), assets_.hallOfFameScores->end(),
            [this](const auto score) { return model_.score() <= score; });
}

void NativeGameplaySession::renderContinuePages() {
    const auto readBack = [this]() {
        auto result = frame_;
        for (std::int16_t y = 0; y < screenHeight; ++y) {
            for (std::int16_t x = 0; x < screenWidth; ++x) {
                result.pixels[y * screenWidth + x] = pixels_->read(pages_.backPage(), {x, y});
            }
        }
        return result;
    };
    const auto writeBack = [this](const IndexedScreen& image, const ProjectedPoint origin,
        const std::uint16_t rectangleWidth, const std::uint16_t rectangleHeight) {
        for (std::int16_t y = origin.y; y < origin.y + rectangleHeight; ++y) {
            for (std::int16_t x = origin.x; x < origin.x + rectangleWidth; ++x) {
                pixels_->write(pages_.backPage(), {x, y}, image.pixels[y * screenWidth + x]);
            }
        }
    };
    const auto copyToFront = [this](const ProjectedPoint origin,
        const std::uint16_t rectangleWidth, const std::uint16_t rectangleHeight) {
        pages_.selectFront();
        pixels_->apply(std::vector<IndexedPageEffect>{RadarPageCopy{
            pages_.backPage(), pages_.frontPage(), origin, rectangleWidth, rectangleHeight}});
    };
    const auto pointerOrigin = [](const ContinueChoice choice) {
        return ProjectedPoint{continuePointerX,
            choice == ContinueChoice::yes ? continueYesPointerY : continueNoPointerY};
    };
    if (assets_.artGroups.empty() || assets_.artGroups[0].empty()
        || assets_.artGroups[0][0].width != continuePointerWidth
        || assets_.artGroups[0][0].height + 1 != continuePointerHeight) {
        throw std::invalid_argument("retained Continue requires the source pointer bitmap");
    }
    if (!continuePages_) {
        //A0ee/8484: backdrop on retained back, then full copy without a flip.
        pages_.beginModal();
        auto backdrop = renderContinuePageIndexed(assets_, model_, readBack(), ContinuePagePart::backdrop);
        writeBack(backdrop, {0, 0}, screenWidth, screenHeight);
        copyToFront({0, 0}, screenWidth, screenHeight);
        //8586: restore the owned source DAC, compose on back, copy only the panel.
        setSourcePaletteIntensity(vgaComponentMaximum);
        pages_.beginModal();
        auto dialog = renderContinuePageIndexed(assets_, model_, readBack(), ContinuePagePart::dialog);
        writeBack(dialog, {0, 0}, screenWidth, screenHeight);
        copyToFront(continueDialogOrigin, continueDialogWidth, continueDialogHeight);
    } else if (continuePressedChoice_ != model_.pressedContinueChoice()) {
        const auto choice = continuePressedChoice_ ? *continuePressedChoice_ : *model_.pressedContinueChoice();
        const auto yes = choice == ContinueChoice::yes;
        const ProjectedPoint origin{100, static_cast<std::int16_t>(yes ? 63 : 99)};
        const auto rectangleWidth = static_cast<std::uint16_t>(yes ? 116 : 117);
        const auto rectangleHeight = static_cast<std::uint16_t>(yes ? 30 : 33);
        pages_.beginModal();
        auto widget = renderContinuePageIndexed(assets_, model_, readBack(),
            yes ? ContinuePagePart::yesWidget : ContinuePagePart::noWidget);
        writeBack(widget, origin, rectangleWidth, rectangleHeight);
        copyToFront(origin, rectangleWidth, rectangleHeight);
    }
    if(playableCampaign_&&audio_->pointerCapability_==NativePointerCapability::portablePointer) {
        if(!continuePages_) {modalPointer_.reset();}
        pixels_->apply(std::vector<IndexedPageEffect>{RadarPageCopy{pages_.backPage(),pages_.frontPage(),{0,0},320,200}});
        paintPlayableMenuPointer(*pixels_,assets_.artGroups.at(0).at(0),
            modalPointer_.value_or(pointerOrigin(model_.continueChoice())),pages_.backPage(),pages_.frontPage());
        frame_.pixels=pixels_->snapshot(pages_.frontPage());
        continuePointerChoice_=model_.continueChoice();continuePressedChoice_=model_.pressedContinueChoice();
        continuePages_=true;
        pageProjection_=level1Pages_?NativePageProjection::retainedLevel1Continue:NativePageProjection::retainedLevel2Continue;
        return;
    }
    //82ea: transient back-page pointer, old/new rectangle copies, then restore back.
    const auto origin = pointerOrigin(model_.continueChoice());
    const auto background = readBack();
    const auto pointer = renderContinuePageIndexed(assets_, model_, background, ContinuePagePart::pointer);
    pages_.beginModal();
    writeBack(pointer, origin, continuePointerWidth, continuePointerHeight);
    if (continuePages_) {
        copyToFront(pointerOrigin(continuePointerChoice_), continuePointerWidth, continuePointerHeight);
    }
    copyToFront(origin, continuePointerWidth, continuePointerHeight);
    pages_.beginModal();
    writeBack(background, origin, continuePointerWidth, continuePointerHeight);
    for (std::int16_t y = 0; y < screenHeight; ++y) {
        for (std::int16_t x = 0; x < screenWidth; ++x) {
            frame_.pixels[y * screenWidth + x] = pixels_->read(pages_.frontPage(), {x, y});
        }
    }
    continuePointerChoice_ = model_.continueChoice();
    continuePressedChoice_ = model_.pressedContinueChoice();
    continuePages_ = true;
    pageProjection_ = level1Pages_ ? NativePageProjection::retainedLevel1Continue
        : NativePageProjection::retainedLevel2Continue;
}

std::optional<std::vector<std::uint8_t>> NativeGameplaySession::level2BackPagePixels() const {
    return level2PagePixels(pages_.backPage());
}

std::optional<std::vector<std::uint8_t>> NativeGameplaySession::level2CachePagePixels() const {
    return level2PagePixels(pages_.cachePage());
}

std::optional<std::vector<std::uint8_t>> NativeGameplaySession::level2PagePixels(const std::uint16_t page) const {
    if (!pixels_ || level1Pages_) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> result;
    result.reserve(screenWidth * screenHeight);
    for (std::int16_t y = 0; y < screenHeight; ++y) {
        for (std::int16_t x = 0; x < screenWidth; ++x) {
            result.push_back(pixels_->read(page, {x, y}));
        }
    }
    return result;
}

std::optional<std::vector<std::uint8_t>> NativeGameplaySession::retainedPagePixels(const NativePageRole role) const {
    if (!pixels_) {
        return std::nullopt;
    }
    const auto page = role == NativePageRole::cache ? pages_.cachePage()
        : role == NativePageRole::back ? pages_.backPage() : pages_.frontPage();
    std::vector<std::uint8_t> result;
    result.reserve(screenWidth * screenHeight);
    for (std::int16_t y = 0; y < screenHeight; ++y) {
        for (std::int16_t x = 0; x < screenWidth; ++x) {
            result.push_back(pixels_->read(page, {x, y}));
        }
    }
    return result;
}

NativeGameplaySession::NativeGameplaySession(const GameplayAssets& assets,
    GameplayModel initialModel, const NativeSessionStart start, std::unique_ptr<NativeAudioOwner> audio,
    std::shared_ptr<HighscoreStore> highscoreStore)
    : assets_(assets), model_(std::move(initialModel)),
      audio_(audio && !audio->ready_ ? throw std::invalid_argument("startup owner is not ready")
          : audio ? std::move(audio) : std::make_unique<NativeAudioOwner>(assets)),
      synth_(audio_->synth_), clock_(audio_->clock_), highscoreTable_(audio_->currentScores_),
      highscoreStore_(std::move(highscoreStore)) {
    if (audio_->ready_) {
        const std::array<std::int8_t, 7> defaultDifficulty{3, 3, 3, 3, 3, 3, 3};
        if (audio_->adopted_ || start != NativeSessionStart::level1 || model_.phase() != GameplayPhase::level1
            || model_.ticks() != 0 || model_.level1State().systemStatusLevels() != defaultDifficulty) {
            throw std::invalid_argument("startup owner requires fresh default-difficulty Level1");
        }
        audio_->adopted_ = true;
        return;
    }
    if (synth_.startupPhase() != SoundBlasterStartupPhase::idle || clock_.elapsedSamples() != 0) {
        throw std::invalid_argument("incomplete startup owner cannot enter gameplay");
    }
    switch (start) {
    case NativeSessionStart::level1:
        synth_.startLevel1();
        break;
    case NativeSessionStart::level2:
        synth_.startLevel2();
        break;
    case NativeSessionStart::level3:
        synth_.startLevel3();
        break;
    default:
        throw std::invalid_argument("unsupported native session start");
    }
}

void NativeGameplaySession::enter(NativeSessionDelivery& delivery) {
    const auto frozen = frame_;
    const auto schedule = clock_.entrySchedule();
    delivery.entry.reserve(schedule.size());
    for (unsigned index = 0; index < schedule.size(); ++index) {
        delivery.entry.push_back({renderLevelEntryIndexed(frozen, assets_.cockpitDacPalette, index),
            clock_.renderEntryStep(schedule[index]), schedule[index].elapsedNanoseconds});
    }
}

void NativeGameplaySession::refreshFrame() {
    const auto phase = model_.phase();
    pageProjection_ = NativePageProjection::cleanUnqualified;
    if (supportsHallPages()) {
        renderHallPages();
        return;
    }
    if (supportsNameEntryPages()) {
        renderNameEntryPages();
        return;
    }
    if (supportsSuccessPages()) {
        renderSuccessPages();
        return;
    }
    successPalette_.reset();
    if (phase == GameplayPhase::successPresentation || phase == GameplayPhase::level2SuccessPresentation) {
        if (!hasFrozenSuccess_) {
            frozenSuccess_ = renderGameplayIndexed(assets_, model_);
            hasFrozenSuccess_ = true;
        }
        frame_ = renderLevel1SuccessIndexed(frozenSuccess_, model_.presentationFrame());
        return;
    }
    hasFrozenSuccess_ = false;
    if (supportsContinuePages()) {
        renderContinuePages();
        return;
    }
    if (supportsFailurePages()) {
        renderFailurePages();
        return;
    }
    if (pixels_ && level1Pages_ && supportsLevel1Pages()) {
        renderLevel1Pages();
        if (model_.pendingLevel1FailureTail()) {
            finishLevel1FailureDisplay();
        }
        return;
    }
    if (pixels_ && !level1Pages_ && supportsRetainedPages() && renderLevel2Pages()) {
        if (model_.pendingLevel2FailureTail()) {
            finishLevel2FailureDisplay();
        }
        return;
    }
    pixels_.reset();
    radar_.reset();
    level1Pages_ = false;
    failurePages_ = false;
    continuePages_ = false;
    frame_ = renderGameplayIndexed(assets_, model_);
}

NativeSessionDelivery NativeGameplaySession::initialize() {
    if (recoveredStartupPalette_) {
        throw std::logic_error("recovered startup requires the work-clock Level1 loader");
    }
    if (initialized_) {
        throw std::logic_error("native session already initialized");
    }
    NativeSessionDelivery delivery;
    delivery.firstSample = clock_.elapsedSamples();
    if (model_.phase() == GameplayPhase::level1) {
        if (model_.ticks() == 0 && supportsLevel1Pages()) {
            initializePages(NativeSessionStart::level1);
        } else {
            pages_.beginFrame();
            pages_.flip();
        }
    }
    refreshFrame();
    if (model_.ticks() == 0) {
        enter(delivery);
    }
    initialized_ = true;
    return delivery;
}

void NativeGameplaySession::dispatch(const std::vector<Level1AudioEvent>& events,
    std::size_t& cursor, NativeSessionDelivery& delivery) {
    if (cursor > events.size()) {
        throw std::logic_error("native audio cursor lost its event history");
    }
    while (cursor < events.size()) {
        const auto& event = events[cursor++];
        synth_.triggerLevel1Event(event.eventId);
        delivery.events.push_back(event);
    }
}

void NativeGameplaySession::finishCampaignLoad(const NativeSessionStart level) {
    if (level != NativeSessionStart::level3 && level != NativeSessionStart::level5) {
        throw std::invalid_argument("native campaign loader requires Level3 or Level5");
    }
    const std::size_t destination = level == NativeSessionStart::level3 ? level3Index : 4;
    initializePages(level);
    synth_.loadCampaignMusic(assets_.musicForLevel(destination));
    level2Cursor_ = 0;
    failureCursor_ = 0;
    successPalette_.reset();
    nameEntry_.reset();
    nameEntryPages_ = false;
    nameOkPressed_ = false;
    incomingNameOkPressed_ = false;
    hallPages_ = false;
    hallContinuePressed_ = false;
    incomingHallContinuePressed_ = false;
    for (std::int16_t y = 0; y < screenHeight; ++y) {
        for (std::int16_t x = 0; x < screenWidth; ++x) {
            frame_.pixels[y * screenWidth + x] = pixels_->read(pages_.frontPage(), {x, y});
        }
    }
    setSourcePaletteIntensity(0);
    pageProjection_ = level == NativeSessionStart::level3 ? NativePageProjection::retainedLevel3Loader
        : NativePageProjection::retainedLevel5Loader;
    const auto& resources = assets_.campaignLevels.at(destination);
    campaignResources_ = CampaignResourceBorrow{destination, &resources.bitmapGroups,
        &resources.models, &resources.musicSequence};
}

void NativeGameplaySession::enterEnding(const GameplayInput& input, const NameEntryInput& text) {
    constexpr std::size_t loadedLevel = 4;
    if (input.left || input.right || input.forward || input.reverse || input.fire || input.exit || input.confirm
        || !text.characters.empty() || !pixels_ || !radar_ || level1Pages_ || successPalette_
        || !campaignResources_ || campaignResources_->level != loadedLevel
        || assets_.campaignLevels.size() <= loadedLevel || !assets_.endingScene
        || !assets_.cockpitSettings || assets_.scoreFont.height != 8 || assets_.scoreFont.glyphs.size() != 96
        || synth_.successActive() || synth_.driverState().sequenceActive
        || synth_.sampledVoiceObservation().phase != SampledVoicePhase::unloaded) {
        throw std::invalid_argument("ending entry requires retained Level5 resources, SCENED/font and neutral stopped audio");
    }
    const auto& level = assets_.campaignLevels[loadedLevel];
    if (campaignResources_->bitmaps != &level.bitmapGroups || campaignResources_->models != &level.models
        || campaignResources_->music != &level.musicSequence || level.resourceName != "LEVEL5.RES") {
        throw std::invalid_argument("ending cleanup changed its loaded Level5 resource identity");
    }
    model_.validateCampaignSuccessCleanup();
    synth_.validateCampaignMusicRelease();
    validateEndingSceneAssets(*assets_.endingScene);
    auto owner = assets_.endingScene;
    ScenePageInterpreter staged(owner->scene, radarRoles(pages_), 0, 0);

    //0428..0453 release the native borrows; immutable decoded cache bytes outlive them.
    campaignResources_->bitmaps = nullptr;
    campaignResources_->models = nullptr;
    synth_.releaseCampaignMusic();
    campaignResources_->music = nullptr;
    pages_.selectFront();
    //7204 already cleared B94:0464 must not dispatch another driver stop.
    model_.finishCampaignSuccessCleanup();
    campaignResources_.reset();
    endingAssets_ = std::move(owner);
    ending_.emplace(std::move(staged));
    pageProjection_ = NativePageProjection::retainedEndingEntry;
}

void NativeGameplaySession::presentEndingFirstPage(const GameplayInput& input, const NameEntryInput& text) {
    constexpr std::size_t musicOption = 2; //12B4, also gates B50E's speech load.
    constexpr std::uint16_t endingVoiceGroup = 3;
    constexpr std::size_t firstPageOperations = 6;
    if (input.left || input.right || input.forward || input.reverse || input.fire || input.exit || input.confirm
        || !text.characters.empty() || !ending_ || !endingAssets_ || endingPalette_ || !pixels_ || !radar_
        || campaignResources_ || level1Pages_ || successPalette_ || !assets_.cockpitSettings
        || model_.campaignLevel() != 7 || model_.presentationFrame() != 0
        || assets_.scoreFont.firstCharacter != 32 || assets_.scoreFont.height != 8
        || assets_.scoreFont.glyphs.size() != 96 || assets_.cockpitDacPalette.size() != vgaPaletteBytes
        || frame_.pixels.size() != screenWidth * screenHeight || frame_.palette.size() != vgaPaletteColors
        || synth_.successActive() || synth_.driverState().sequenceActive
        || synth_.sampledVoiceObservation().phase != SampledVoicePhase::unloaded) {
        throw std::invalid_argument("SCENED first page requires neutral retained entry, source font/palette and inactive audio");
    }
    model_.validateEndingFirstPage();
    auto interpreter = *ending_;
    const auto work = interpreter.execute(0);
    if (!work.present || work.delayIrqCounts != 0 || work.operations.size() != firstPageOperations) {
        throw std::invalid_argument("SCENED first page requires its complete zero-delay operation domain");
    }
    const auto* select = std::get_if<ScenePaletteChange>(&work.operations[0]);
    const auto* load = std::get_if<ScenePaletteChange>(&work.operations[1]);
    const auto* background = std::get_if<SceneBackgroundDraw>(&work.operations[2]);
    const auto* cache = std::get_if<RadarPageCopy>(&work.operations[3]);
    const auto* voice = std::get_if<SceneSoundRequest>(&work.operations[4]);
    const auto* present = std::get_if<RadarPageCopy>(&work.operations[5]);
    const auto fullCopy = [](const RadarPageCopy* copy, const std::uint16_t from, const std::uint16_t to) {
        return copy && copy->sourcePage == from && copy->destinationPage == to
            && copy->position.x == 0 && copy->position.y == 0
            && copy->coveredWidth == screenWidth && copy->height == screenHeight;
    };
    if (!select || select->operation != ScenePaletteOperation::selectBank || select->asset
        || !load || load->operation != ScenePaletteOperation::load || load->asset != std::optional<std::size_t>{0}
        || !background || background->asset != 2 || !voice
        || voice->operation != SceneSoundOperation::loadVoice || voice->argument != endingVoiceGroup
        || !fullCopy(cache, pages_.backPage(), pages_.cachePage())
        || !fullCopy(present, pages_.backPage(), pages_.frontPage())) {
        throw std::invalid_argument("SCENED first page changed its admitted palette/background/cache/voice ordering");
    }

    //Stage all fallible lowering before1061 can write the inherited front page.
    validateEndingSceneAssets(*endingAssets_);
    const auto& scene = endingAssets_->scene;
    auto backgroundEffects = scenePagePixelEffects(work.operations[2], scene, assets_.scoreFont, pages_.backPage());
    const auto cacheEffects = scenePagePixelEffects(work.operations[3], scene, assets_.scoreFont, pages_.backPage());
    const auto presentEffects = scenePagePixelEffects(work.operations[5], scene, assets_.scoreFont, pages_.backPage());
    PaletteWorkState inherited{};
    std::copy(assets_.cockpitDacPalette.begin(), assets_.cockpitDacPalette.end(), inherited.source.begin());
    const PaletteRequest blackout{0, vgaPaletteColors, 0, 0};
    const auto black = completedFullPaletteUpload(inherited, blackout);
    const auto loaded = selectScenePalette(black, std::get<0>(scene.assets[0]));
    const auto voiceEnabled = assets_.cockpitSettings->options[musicOption] != 0;
    std::optional<PaletteWorkState> temporary;
    std::optional<PaletteWorkState> restored;
    if (voiceEnabled) {
        synth_.validateSampledVoiceLoad(endingAssets_->sampledVoice);
        temporary = completedFullPaletteUpload(inherited, {0, vgaPaletteColors, 0, vgaComponentMaximum});
        restored = selectScenePalette(*temporary, std::get<0>(scene.assets[0]));
    }
    const auto publishPalette = [this](const PaletteWorkState& state) {
        endingPalette_ = state;
        for (std::size_t color = 0; color < vgaPaletteColors; ++color) {
            const auto component = [&state, color](const std::size_t index) {
                const auto value = state.dac.colors[color * vgaPaletteComponents + index];
                return static_cast<std::uint8_t>((value << 2) | (value >> 4));
            };
            frame_.palette[color] = {component(0), component(1), component(2)};
        }
    };

    pages_.beginModal();
    pages_.selectCockpitCopy(); //0272/D30; page0 has no prior dirty rectangles.
    pages_.beginModal();
    pages_.selectFront();
    applySceneFrontWipe(*pixels_, pages_.frontPage());
    endingPaletteRequest_ = blackout;
    publishPalette(black); //1A22(0) only queries the inherited selected handle.
    pages_.beginModal();
    publishPalette(loaded); //F05F's nonzero1A22 uploads raw PAL, not intensity0.
    pages_.beginModal();
    applyBlackoutClear(*pixels_, pages_.backPage()); //F01F clears before drawing SCR.
    pixels_->apply(backgroundEffects);
    pages_.selectCockpitDraw();
    pixels_->apply(cacheEffects);
    pages_.beginModal(); //E86 restores its incoming back/back selectors.
    pages_.beginModal();
    if (voiceEnabled) {
        publishPalette(*temporary);
        synth_.loadSampledVoice(endingAssets_->sampledVoice);
        publishPalette(*restored);
        pages_.selectCockpitCopy();
        pixels_->apply(std::vector<IndexedPageEffect>{RadarPageCopy{
            pages_.cachePage(), pages_.backPage(), {0, 0}, screenWidth, screenHeight}});
    }
    pages_.selectArmorCopy();
    pixels_->apply(presentEffects);
    *ending_ = std::move(interpreter);
    model_.finishEndingFirstPage();
    pageProjection_ = NativePageProjection::retainedEndingPage;
    //No page0 IRQ wait. This native endpoint does not assign CPU/DAC work time.
    for (std::int16_t y = 0; y < screenHeight; ++y) {
        for (std::int16_t x = 0; x < screenWidth; ++x) {
            frame_.pixels[y * screenWidth + x] = pixels_->read(pages_.frontPage(), {x, y});
        }
    }
}

std::vector<std::int16_t> NativeGameplaySession::presentEndingPage(const GameplayInput& input, const NameEntryInput& text) {
    constexpr std::size_t musicOption = 2;
    constexpr std::uint16_t sceneDelayIrqs = 24;
    if (input.left || input.right || input.forward || input.reverse || input.fire || input.exit || input.confirm
        || !text.characters.empty() || !ending_ || !endingAssets_ || !endingPalette_ || !pixels_ || !radar_
        || !assets_.cockpitSettings || assets_.cockpitSettings->options[musicOption] == 0
        || endingWait_.remaining != 0 || campaignResources_ || level1Pages_ || successPalette_
        || frame_.pixels.size() != screenWidth * screenHeight || frame_.palette.size() != vgaPaletteColors
        || synth_.successActive() || synth_.driverState().sequenceActive) {
        throw std::invalid_argument("timed SCENED pages require neutral retained native SBL speech, palette/pages and no pending wait");
    }
    model_.validateEndingPage();
    clock_.validateNativeIrqWait();
    const auto page = model_.presentationFrame();
    const auto voice = synth_.sampledVoiceObservation();
    if (page == 1 ? (sceneVoiceSegment_ != initialSceneVoiceSegment || voice.phase != SampledVoicePhase::loaded)
        : (sceneVoiceSegment_ != 0 || voice.phase != SampledVoicePhase::playing)) {
        throw std::invalid_argument("timed SCENED voice lost its inherited selector or retained segment0 lifetime");
    }
    for (const auto physical : {pages_.cachePage(), pages_.backPage(), pages_.frontPage()}) {
        for (std::int16_t y = 0; y < screenHeight; ++y) {
            for (std::int16_t x = 0; x < screenWidth; ++x) {
                if (!pixels_->known(physical, {x, y})) {
                    throw std::invalid_argument("timed SCENED pages require inherited indexed write ownership");
                }
            }
        }
    }
    auto interpreter = *ending_;
    const auto work = interpreter.execute(page);
    if (work.present != (page < endingScenePageCount) || work.delayIrqCounts != sceneDelayIrqs) {
        throw std::invalid_argument("timed SCENED page changed its source return or raw IRQ countdown");
    }
    constexpr std::uint64_t dspInputClock = 1'000'000;
    const auto waitSamples = clock_.nativeIrqWaitSamples(work.delayIrqCounts);
    if (voice.clockPeriod == 0 || voice.transferred > voice.sampleCount
        || (voice.clockPhase + waitSamples * dspInputClock) / voice.clockPeriod >= voice.sampleCount - voice.transferred) {
        throw std::invalid_argument("timed SCENED page would reach an unowned sampled-voice DMA completion IRQ");
    }
    std::vector<std::vector<IndexedPageEffect>> effects(work.operations.size());
    std::size_t voiceRequests = 0;
    for (std::size_t index = 0; index < work.operations.size(); ++index) {
        const auto& operation = work.operations[index];
        if (const auto* sound = std::get_if<SceneSoundRequest>(&operation)) {
            if (page != 1 || sound->operation != SceneSoundOperation::playSequence || sound->argument != 0
                || ++voiceRequests != 1) {
                throw std::invalid_argument("timed SCENED admits its one ordered page1 segment0 request");
            }
            synth_.validateSampledVoiceStart();
            continue;
        }
        if (std::holds_alternative<ScenePaletteChange>(operation)
            || std::holds_alternative<SceneBackgroundDraw>(operation)) {
            throw std::invalid_argument("timed SCENED pages exclude new palette/background ownership");
        }
        effects[index] = scenePagePixelEffects(operation, endingAssets_->scene, assets_.scoreFont, pages_.backPage());
    }
    if (voiceRequests != (page == 1 ? 1U : 0U)) {
        throw std::invalid_argument("timed SCENED page lost its source voice request");
    }

    //Whole-page lowering is qualified before any pixel, voice or clock mutation.
    pages_.beginModal();
    pages_.selectCockpitCopy();
    for (std::size_t index = 0; index < work.operations.size(); ++index) {
        const auto& operation = work.operations[index];
        if (std::holds_alternative<SceneSoundRequest>(operation)) {
            pages_.beginModal();
            //B94 is clear and1BD8 absent:51A6 adds no stop before op6.
            synth_.startSampledVoice();
            sceneVoiceSegment_ = 0;
            continue;
        }
        if (const auto* copy = std::get_if<RadarPageCopy>(&operation)) {
            pages_.beginModal();
            if (copy->sourcePage == pages_.cachePage() && copy->destinationPage == pages_.backPage()) {
                pages_.selectCockpitCopy();
            } else if (copy->sourcePage == pages_.backPage() && copy->destinationPage == pages_.cachePage()) {
                pages_.selectCockpitDraw();
            } else {
                pages_.selectArmorCopy();
            }
            pixels_->apply(effects[index]);
            if (copy->destinationPage == pages_.cachePage()) {
                pages_.beginModal();
            }
            continue;
        }
        pages_.beginModal();
        pixels_->apply(effects[index]);
    }
    pages_.selectArmorCopy(); //D6E leaves this pair even for the absent page43.
    *ending_ = std::move(interpreter);
    endingWait_.remaining = work.delayIrqCounts; //Already416E*4; never multiply twice.
    auto samples = clock_.renderIrqWait(endingWait_, std::numeric_limits<std::size_t>::max());
    model_.finishEndingPage(work.present ? EndingPageResult::present : EndingPageResult::exhausted);
    for (std::int16_t y = 0; y < screenHeight; ++y) {
        for (std::int16_t x = 0; x < screenWidth; ++x) {
            frame_.pixels[y * screenWidth + x] = pixels_->read(pages_.frontPage(), {x, y});
        }
    }
    return samples;
}

void NativeGameplaySession::finishEnding(NativeSessionDelivery& delivery,
    const GameplayInput& input, const NameEntryInput& text) {
    constexpr std::size_t musicOption = 2;
    if (input.left || input.right || input.forward || input.reverse || input.fire || input.exit || input.confirm
        || !text.characters.empty() || !ending_ || !endingAssets_ || !endingPalette_ || !pixels_ || !radar_
        || !assets_.cockpitSettings || assets_.cockpitSettings->options[musicOption] == 0
        || endingWait_.remaining != 0 || sceneVoiceSegment_ != 0 || campaignResources_ || successPalette_
        || level1Pages_ || assets_.cockpitDacPalette.size() != vgaPaletteBytes
        || frame_.pixels.size() != screenWidth * screenHeight || frame_.palette.size() != vgaPaletteColors
        || synth_.successActive() || synth_.driverState().sequenceActive
        || synth_.sampledVoiceObservation().phase != SampledVoicePhase::playing) {
        throw std::invalid_argument("SCENED teardown requires neutral normal EOF, retained pages/palettes and busy native speech");
    }
    model_.validateEndingTeardown();
    clock_.validateNativeIrqWait();
    //Qualify both banks and source operations before freeing script or voice storage.
    blackoutSourcePlan({}, pages_.frontPage());
    if (endingPalette_->dac.mode != PaletteDacMode::write || endingPalette_->dac.pelIndex != 0) {
        throw std::invalid_argument("SCENED teardown requires a completed scene DAC upload");
    }
    auto firstPalette = *endingPalette_;
    PaletteWorkState inherited = firstPalette;
    std::copy(assets_.cockpitDacPalette.begin(), assets_.cockpitDacPalette.end(), inherited.source.begin());
    inherited = completedFullPaletteUpload(inherited, {0, vgaPaletteColors, 0, vgaComponentMaximum});
    for (std::size_t index = 0; index < blackoutPaletteSteps; ++index) {
        const PaletteRequest request{0, vgaPaletteColors, 0,
            static_cast<std::uint8_t>(vgaComponentMaximum - index * 4)};
        firstPalette = completedFullPaletteUpload(firstPalette, request);
        completedFullPaletteUpload(inherited, request);
    }
    for (const auto physical : {pages_.cachePage(), pages_.backPage(), pages_.frontPage()}) {
        for (std::int16_t y = 0; y < screenHeight; ++y) {
            for (std::int16_t x = 0; x < screenWidth; ++x) {
                if (!pixels_->known(physical, {x, y})) {
                    throw std::invalid_argument("SCENED teardown requires retained indexed page ownership");
                }
            }
        }
    }
    const auto publishPalette = [this](const PaletteWorkState& palette) {
        *endingPalette_ = palette;
        for (std::size_t color = 0; color < vgaPaletteColors; ++color) {
            const auto component = [&palette, color](const std::size_t index) {
                const auto value = palette.dac.colors[color * vgaPaletteComponents + index];
                return static_cast<std::uint8_t>((value << 2) | (value >> 4));
            };
            frame_.palette[color] = {component(0), component(1), component(2)};
        }
    };
    const auto readFront = [this]() {
        for (std::int16_t y = 0; y < screenHeight; ++y) {
            for (std::int16_t x = 0; x < screenWidth; ++x) {
                frame_.pixels[y * screenWidth + x] = pixels_->read(pages_.frontPage(), {x, y});
            }
        }
    };
    std::uint64_t elapsed = 0;
    const auto fade = [&]() {
        auto work = clock_.renderNativeBlackout({*endingPalette_, {}}, pages_, *pixels_);
        for (std::size_t index = 0; index < work.steps.size(); ++index) {
            auto& step = work.steps[index];
            endingPaletteRequest_ = {0, vgaPaletteColors, 0,
                static_cast<std::uint8_t>(vgaComponentMaximum - index * 4)};
            publishPalette(step.palette);
            delivery.entry.push_back({frame_, std::move(step.samples), elapsed + step.elapsedNanoseconds});
        }
        readFront();
        publishPalette(work.state.palette);
        elapsed += work.elapsedNanoseconds;
        delivery.entry.push_back({frame_, std::move(work.clearSamples), elapsed});
    };

    //C20/E14 clear handles, not the copied bank-zero driver palette needed by9D4C.
    ending_.reset();
    endingAssets_.reset();
    //03EE's released-key gate is admitted by neutral input;1330/1335 are absent.
    synth_.stopSampledVoice();
    synth_.releaseSampledVoice();
    sceneVoiceSegment_ = initialSceneVoiceSegment;
    fade();
    pages_.beginModal();
    applyBlackoutClear(*pixels_, pages_.backPage());
    pages_.flip();
    readFront();
    publishPalette(inherited); //1A22(12C6) uploads raw PAL[2], before the second fade.
    delivery.entry.push_back({frame_, {}, elapsed});
    fade();
    //Native painters borrow explicit shared fonts; releasing the VM ends its scene font binding.
    model_.finishEndingTeardown();
    pageProjection_ = NativePageProjection::retainedEndingPage;
}

NativeSessionDelivery NativeGameplaySession::advanceLevel3(const GameplayInput& input, const NameEntryInput& text) {
    const bool first = model_.phase() == GameplayPhase::level3PreloopReturn;
    if (input.left || input.right || input.forward || input.reverse || input.fire || input.exit || input.confirm
        || !text.characters.empty() || !pixels_ || !radar_ || level1Pages_ || !assets_.cockpitSettings
        || (audio_->playablePreferences_ ? !audio_->playableDetailProfile()
            : assets_.cockpitSettings->viewSize <= cockpitOverlayStage) || successPalette_
        || !campaignResources_ || campaignResources_->level != level3Index
        || assets_.campaignLevels.size() <= level3Index
        || pageProjection_ != (first ? NativePageProjection::retainedLevel3Preloop : NativePageProjection::retainedLevel3)
        || synth_.driverState().clockSource != SoundBlasterClockSource::samples
        || !std::all_of(model_.level2Entities().begin(), model_.level2Entities().end(),
            [this](const auto& actor) { return supportsLevel3Actor(assets_, actor); })) {
        throw std::invalid_argument("Level3 frames require retained neutral actors, pages, resources and native sample clock");
    }
    const auto& resources = assets_.campaignLevels[level3Index];
    if (campaignResources_->bitmaps != &resources.bitmapGroups || campaignResources_->models != &resources.models
        || campaignResources_->music != &resources.musicSequence || resources.resourceName != "LEVEL3.RES") {
        throw std::invalid_argument("Level3 frame changed its loaded resource identity");
    }
    NativeSessionDelivery delivery;
    delivery.firstSample = clock_.elapsedSamples();
    incomingIndicator_ = {static_cast<std::int16_t>(model_.level2CockpitIndicatorX()),
        static_cast<std::int16_t>(model_.level2CockpitIndicatorVelocity())};
    model_.advanceRetainedLevel3(input);
    if (!supportsRetainedPages()) {
        throw std::invalid_argument("unsupported retained Level3 actor or presentation domain");
    }
    renderLevel2Pages();
    model_.finishLevel3Radar();
    dispatch(model_.level2AudioEvents(), level2Cursor_, delivery);
    if (first) {
        enter(delivery); //032C/9D9E follows display; the first source tail is still pending.
    }
    model_.finishLevel3Frame();
    dispatch(model_.level2AudioEvents(), level2Cursor_, delivery);
    delivery.samples = clock_.renderFrame();
    return delivery;
}

NativeSessionDelivery NativeGameplaySession::advance(const GameplayInput& input, const NameEntryInput& text) {
    if (completeLevel1Frames_) {
        throw std::invalid_argument("completed Level1 frames cannot switch to display-boundary execution");
    }
    return advanceImpl(input, text, Level1Boundary::display);
}

NativeSessionDelivery NativeGameplaySession::advanceLevel1Frame() {
    const auto& state = model_.level1State();
    if (!initialized_ || !pixels_ || !radar_ || !level1Pages_
        || model_.phase() != GameplayPhase::level1 || (!completeLevel1Frames_ && model_.ticks() != 0)
        || state.playerArmor() < 0 || state.selectedSystem() != -1 || state.playerWeaponCooldown() != 0
        || std::any_of(state.playerProjectileTimers().begin(), state.playerProjectileTimers().end(),
            [](const auto timer) { return timer != 0; })
        || std::any_of(state.systemTimers().begin(), state.systemTimers().end(),
            [](const auto timer) { return timer != 0; })) {
        throw std::invalid_argument("complete Level1 frame requires retained neutral ordinary ownership");
    }
    completeLevel1Frames_ = true;
    return advanceImpl({}, {}, Level1Boundary::completedTail);
}

NativeSessionDelivery NativeGameplaySession::advanceImpl(const GameplayInput& input, const NameEntryInput& text,
    const Level1Boundary boundary) {
    if (!initialized_ || model_.phase() == GameplayPhase::campaignEnded) {
        throw std::logic_error("native session cannot advance at this boundary");
    }
    if (model_.phase() == GameplayPhase::level2SuccessReturn) {
        if (input.left || input.right || input.forward || input.reverse || input.fire || input.exit || input.confirm
            || !text.characters.empty() || !pixels_ || !radar_ || level1Pages_ || successPalette_
            || pageProjection_ != NativePageProjection::retainedLevel2SuccessReturn
            || assets_.campaignLevels.size() <= level3Index
            || assets_.campaignLevels[level3Index].resourceName != "LEVEL3.RES"
            || assets_.musicForLevel(level3Index).size() < 36) {
            throw std::invalid_argument("Level3 loader requires retained neutral success and actual resources");
        }
        validateLevel3LoaderActors(assets_.models, assets_.campaignLevels[level3Index].models);
        synth_.validateCampaignMusicLoad(assets_.musicForLevel(level3Index));
        NativeSessionDelivery delivery;
        delivery.firstSample = clock_.elapsedSamples();
        pages_.selectFront(); //0456..0459, before the new loader selects its pages.
        model_.completeLevel3Load(assets_.models, assets_.campaignLevels[level3Index].models);
        finishCampaignLoad(NativeSessionStart::level3);
        return delivery;
    }
    if (model_.phase() == GameplayPhase::level3LoaderReturn) {
        if (input.left || input.right || input.forward || input.reverse || input.fire || input.exit || input.confirm
            || !text.characters.empty() || !pixels_ || !radar_ || level1Pages_ || successPalette_
            || pageProjection_ != NativePageProjection::retainedLevel3Loader
            || !campaignResources_ || campaignResources_->level != level3Index
            || assets_.campaignLevels.size() <= level3Index) {
            throw std::invalid_argument("Level3 preloop requires its retained loader owners and neutral input");
        }
        const auto& resources = assets_.campaignLevels[level3Index];
        if (campaignResources_->bitmaps != &resources.bitmapGroups || campaignResources_->models != &resources.models
            || campaignResources_->music != &resources.musicSequence || resources.resourceName != "LEVEL3.RES") {
            throw std::invalid_argument("Level3 preloop changed its resource identity");
        }
        model_.validateLevel3Preloop();
        const auto& systems = model_.level1State();
        const auto score = scorePageEffects(model_.score(), assets_.scoreFont, pages_.backPage(), pages_.frontPage());
        const auto status = statusPageEffects(systems.systemStatusLevels(), systems.systemTimers(), systems.selectedSystem(),
            assets_.artGroups.at(overlayGroup), pages_.backPage(), pages_.frontPage());
        NativeSessionDelivery delivery;
        delivery.firstSample = clock_.elapsedSamples();
        synth_.resetDirectEffects(); //01A8: byte dispatch16h(decimal22) ->206B, bookkeeping only.
        pixels_->apply(score); //7970(0) retains score and copies the field back->front.
        model_.completeLevel3Preloop(); //01BD/4C16, before8F1C's seven HUD widgets.
        pixels_->apply(status);
        pages_.selectArmorCopy();
        pageScore_ = model_.score();
        pageStatus_ = systems.systemStatusLevels();
        for (std::int16_t y = 0; y < screenHeight; ++y) {
            for (std::int16_t x = 0; x < screenWidth; ++x) {
                frame_.pixels[y * screenWidth + x] = pixels_->read(pages_.frontPage(), {x, y});
            }
        }
        pageProjection_ = NativePageProjection::retainedLevel3Preloop;
        return delivery;
    }
    if (model_.phase() == GameplayPhase::level3PreloopReturn || model_.phase() == GameplayPhase::level3Native) {
        return advanceLevel3(input, text);
    }
    if (model_.phase() == GameplayPhase::level5SuccessReturn) {
        NativeSessionDelivery delivery;
        delivery.firstSample = clock_.elapsedSamples();
        enterEnding(input, text);
        return delivery;
    }
    if (model_.phase() == GameplayPhase::endingEntry) {
        NativeSessionDelivery delivery;
        delivery.firstSample = clock_.elapsedSamples();
        presentEndingFirstPage(input, text);
        return delivery;
    }
    if (model_.phase() == GameplayPhase::endingPresentation) {
        NativeSessionDelivery delivery;
        delivery.firstSample = clock_.elapsedSamples();
        delivery.samples = presentEndingPage(input, text);
        return delivery;
    }
    if (model_.phase() == GameplayPhase::endingTeardown) {
        NativeSessionDelivery delivery;
        delivery.firstSample = clock_.elapsedSamples();
        finishEnding(delivery, input, text);
        return delivery;
    }
    if (model_.phase() == GameplayPhase::endingReturn) {
        NativeSessionDelivery delivery;
        delivery.firstSample = clock_.elapsedSamples();
        returnToMenu(delivery, input, text);
        return delivery;
    }
    if (model_.phase() == GameplayPhase::nativeMenu) {
        if (input.left || input.right || input.fire || input.exit || !text.characters.empty()) {
            throw std::invalid_argument("native menu admits its owned up/down/confirm input");
        }
        return advanceMenu({input.forward, input.reverse, input.confirm}, gameplayAudioSamplesPerFrame);
    }
    NativeSessionDelivery delivery;
    delivery.firstSample = clock_.elapsedSamples();
    const auto previous = model_.phase();
    if (!text.characters.empty() && previous != GameplayPhase::highscoreNameEntry) {
        throw std::invalid_argument("native text input requires the owned highscore editor");
    }
    if (previous == GameplayPhase::highscoreNameEntry && nameOkPressed_ && !input.confirm) {
        constexpr std::size_t destination = 4;
        if (!supportsNameEntryPages() || !text.characters.empty()
            || input.fire || input.left || input.right || input.forward || input.reverse || input.exit
            || assets_.campaignLevels.size() <= destination
            || assets_.campaignLevels[destination].resourceName != "LEVEL5.RES"
            || assets_.musicForLevel(destination).size() < 36) {
            throw std::invalid_argument("Highscore return requires neutral input and actual Level5 resources");
        }
        if (!nameEntry_->name().empty()) {
            if (!highscoreTable_ || !highscoreTable_->rank(model_.score()) || !highscoreStore_
                || !assets_.cockpitSettings) {
                throw std::invalid_argument("Highscore submission requires its owned table, live settings and persistence driver");
            }
            model_.validateHighscoreHall(assets_.models);
            auto candidate = *highscoreTable_;
            if (!candidate.insert(model_.score(), nameEntry_->name(), HighscoreNameMark::ordinary)) {
                throw std::invalid_argument("Highscore submission no longer has its source rank");
            }
            auto effects = highscoreHallPageEffects(assets_.artGroups.at(0), assets_.titleFont, candidate,
                pages_.backPage(), pages_.frontPage());
            auto configured = candidate;
            configured.setConfiguration(*assets_.cockpitSettings);
            incomingNameOkPressed_ = false;
            pendingNameInput_ = {};
            renderNameEntryPages(NameEntryPoll::accepted);
            const auto saved = highscoreStore_->saveAndReload(configured);
            if (saved.status == HighscoreSaveStatus::saved) {
                if (!saved.reloaded) {
                    throw std::runtime_error("Highscore saved result lacks its reloaded table");
                }
                const bool same = saved.reloaded->header() == configured.header()
                    && std::equal(candidate.entries().begin(), candidate.entries().end(), saved.reloaded->entries().begin(),
                        [](const auto& a, const auto& b) { return a.score == b.score && a.name == b.name; });
                if (!same) {
                    effects = highscoreHallPageEffects(assets_.artGroups.at(0), assets_.titleFont, *saved.reloaded,
                        pages_.backPage(), pages_.frontPage());
                }
                candidate = *saved.reloaded;
            } else if (saved.status != HighscoreSaveStatus::openFailed) {
                throw std::runtime_error("Highscore partial write or reload failure is outside the supported return domain");
            }
            highscoreTable_ = std::move(candidate);
            model_.beginHighscoreHall();
            model_.tick({});
            pixels_->apply(effects);
            setSourcePaletteIntensity(vgaComponentMaximum);
            hallPages_ = true;
            hallContinuePressed_ = false;
            incomingHallContinuePressed_ = false;
            renderHallPages();
            delivery.samples = clock_.renderFrame();
            return delivery;
        }
        model_.validateLevel5Retry(assets_.models, HighscoreReturn::emptyName);
        //5936 releases the widget before the genuine empty result returns from330a.
        pendingNameInput_ = {};
        incomingNameOkPressed_ = false;
        renderNameEntryPages(NameEntryPoll::accepted);
        model_.completeLevel5Retry(assets_.models, HighscoreReturn::emptyName, assets_.campaignLevels.at(4).models);
        synth_.stopMusic();
        finishCampaignLoad(NativeSessionStart::level5);
        return delivery;
    }
    if (previous == GameplayPhase::highscoreHall
        && (input.exit || (hallContinuePressed_ && !input.confirm))) {
        if (!supportsHallPages() || !text.characters.empty() || input.fire
            || input.left || input.right || input.forward || input.reverse) {
            throw std::invalid_argument("Highscore hall return requires its owned pages and neutral input");
        }
        model_.validateLevel5Retry(assets_.models, HighscoreReturn::recordedName);
        incomingHallContinuePressed_ = input.confirm;
        renderHallPages();
        model_.completeLevel5Retry(assets_.models, HighscoreReturn::recordedName, assets_.campaignLevels.at(4).models);
        synth_.stopMusic();
        finishCampaignLoad(NativeSessionStart::level5);
        return delivery;
    }
    if (model_.campaignLevel() == 4 && supportsContinuePages()
        && model_.pressedContinueChoice() == ContinueChoice::yes
        && !input.confirm && !input.fire && !input.exit) {
        if (!highscoreTable_) {
            throw std::invalid_argument("Level5 retry requires its source highscore table and same-level reload");
        }
        if (!highscoreTable_->rank(model_.score())) {
            constexpr std::size_t destination = 4;
            if (!continuePages_ || input.forward || input.reverse || input.left || input.right
                || model_.score() > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
                || assets_.campaignLevels.size() <= destination
                || assets_.campaignLevels[destination].resourceName != "LEVEL5.RES"
                || assets_.musicForLevel(destination).size() < 36) {
                throw std::invalid_argument("Level5 rank miss requires neutral release and actual Level5 resources");
            }
            model_.validateLevel5Retry(assets_.models, HighscoreReturn::rankMiss);
            model_.completeLevel5Retry(assets_.models, HighscoreReturn::rankMiss, assets_.campaignLevels[destination].models);
            // Only Continue choice/press feed this painter; release precedes loader pixels.
            renderContinuePages();
            synth_.stopMusic();
            finishCampaignLoad(NativeSessionStart::level5);
            return delivery;
        }
        model_.beginHighscoreNameEntry();
        nameEntry_.emplace(nameCaret_);
        nameEntryPages_ = false;
        nameOkPressed_ = false;
    }
    pendingNameInput_ = text;
    incomingNameOkPressed_ = model_.phase() == GameplayPhase::highscoreNameEntry && input.confirm;
    incomingHallContinuePressed_ = model_.phase() == GameplayPhase::highscoreHall && input.confirm;
    if (successPalette_ && previous == GameplayPhase::level2SuccessPresentation && !synth_.successActive()) {
        if (model_.campaignLevel() == 4) {
            if (!pixels_ || !radar_ || level1Pages_ || !supportsSuccessPages()) {
                throw std::invalid_argument("Level5 success return lost its retained presentation owners");
            }
            model_.validateLevel5SuccessReturn(input);
            synth_.stopSuccess(); //7204/51A6, before returning to0784.
            model_.setSuccessAudioActive(false);
            model_.tick(input);
            pages_.beginFrame(); //0251..0259 select back;1299 skips the main display/flip.
            successPalette_.reset();
            dispatch(model_.level2AudioEvents(), level2Cursor_, delivery);
            return delivery;
        }
        if (!pixels_ || !radar_ || level1Pages_ || !supportsSuccessPages()) {
            throw std::invalid_argument("Level2 success return lost its retained presentation owners");
        }
        model_.setSuccessAudioActive(false);
        model_.validateLevel2SuccessReturn(input);
        synth_.stopSuccess(); //7204/51A6, before returning to0784.
        model_.tick(input);
        pages_.beginFrame(); //1299 skips ordinary display and flip after0784.
        successPalette_.reset();
        pageProjection_ = NativePageProjection::retainedLevel2SuccessReturn;
        dispatch(model_.level2AudioEvents(), level2Cursor_, delivery);
        return delivery;
    }
    incomingIndicator_ = {static_cast<std::int16_t>(model_.level2CockpitIndicatorX()),
        static_cast<std::int16_t>(model_.level2CockpitIndicatorVelocity())};
    if (pixels_ && !level1Pages_ && input.exit && previous != GameplayPhase::highscoreNameEntry
        && previous != GameplayPhase::highscoreHall) {
        pixels_.reset();
        radar_.reset();
    }
    if (previous == GameplayPhase::level5LoaderReturn) {
        if (!pixels_ || !radar_ || level1Pages_) {
            throw std::invalid_argument("Level5 first frame lost its loader page owners");
        }
        const auto& systems = model_.level1State();
        if (systems.selectedSystem() != -1
            || std::any_of(systems.systemStatusLevels().begin(), systems.systemStatusLevels().end(),
                [](const auto status) { return status < 0; })
            || std::any_of(systems.systemTimers().begin(), systems.systemTimers().end(),
                [](const auto timer) { return timer != 0; })) {
            throw std::invalid_argument("Level5 HUD requires untimed unselected nonnegative systems");
        }
        synth_.resetDirectEffects(); //01ac dispatches byte operation16 -> SBL206b.
        //7970 and8f1c follow50be, before the first ordinary display.
        pixels_->apply(scorePageEffects(model_.score(), assets_.scoreFont, pages_.backPage(), pages_.frontPage()));
        pixels_->apply(statusPageEffects(systems.systemStatusLevels(), systems.systemTimers(), systems.selectedSystem(),
            assets_.artGroups.at(overlayGroup), pages_.backPage(), pages_.frontPage()));
        pageScore_ = model_.score();
        pageStatus_ = systems.systemStatusLevels();
    }
    model_.setSuccessAudioActive(synth_.successActive());
    const bool retainedRetry = supportsLevel2Retry(input);
    const bool retainedLevel1Retry = level1Pages_ && supportsContinuePages() && continuePages_
        && model_.pressedContinueChoice() == ContinueChoice::yes
        && !input.confirm && !input.fire && !input.exit
        && !input.forward && !input.reverse && !input.left && !input.right;
    const bool retainedLevel1Return = level1Pages_ && previous == GameplayPhase::successPresentation
        && !synth_.successActive();
    if (retainedLevel1Return) {
        model_.validateLevel1SuccessReturn(input);
        if (!supportsSuccessPages() || assets_.musicForLevel(level2Index).size() < 36) {
            throw std::invalid_argument("Level1 success return requires retained pages and Level2 music");
        }
        synth_.stopSuccess(); //7204 precedes the resumed0784 caller and loader.
    }
    if (level1Pages_ && previous == GameplayPhase::level1) {
        model_.tickRetainedLevel1(input);
    } else {
        model_.tick(input);
    }
    const auto current = model_.phase();
    if (retainedLevel1Retry && current == GameplayPhase::level1RetryHandoff) {
        pages_.selectFront();
        applyBlackoutClear(*pixels_, pages_.frontPage());
        pages_.beginFrame();
        applyBlackoutClear(*pixels_, pages_.backPage());
        pages_.flip();
        pages_.selectFront();
        applyBlackoutClear(*pixels_, pages_.frontPage());
        initializePages(NativeSessionStart::level1);
    }
    if (retainedRetry && current == GameplayPhase::level2Handoff) {
        if (assets_.cockpitSettings->options[introductionOption] != 0) {
            //5c74 exit: clear both pages, flip once, then clear the new front.
            pages_.selectFront();
            applyBlackoutClear(*pixels_, pages_.frontPage());
            pages_.beginFrame();
            applyBlackoutClear(*pixels_, pages_.backPage());
            pages_.flip();
            pages_.selectFront();
            applyBlackoutClear(*pixels_, pages_.frontPage());
        }
        //48d4 resets allocation/radar history; preserve the actual page owners.
        initializePages(NativeSessionStart::level2);
    }
    if ((previous == GameplayPhase::level5LoaderReturn || previous == GameplayPhase::level5)
        && ((current != GameplayPhase::level5 && current != GameplayPhase::level2SuccessPresentation
                && !model_.pendingLevel2FailureTail())
            || !supportsRetainedPages())) {
        throw std::invalid_argument("unsupported retained Level5 actor, UI or lifecycle domain");
    }
    if (model_.campaignLevel() == 4 && model_.failureFromLevel2() && current != GameplayPhase::campaignEnded
        && !supportsRetainedPages() && !supportsFailurePages() && !supportsContinuePages()
        && !supportsNameEntryPages() && !supportsHallPages()) {
        throw std::invalid_argument("unsupported retained Level5 failure domain");
    }
    if (pixels_ && (level1Pages_ ? !supportsLevel1Pages() && !supportsFailurePages() && !supportsContinuePages()
            && !supportsSuccessPages() && current != GameplayPhase::level1RetryHandoff
            && !(previous == GameplayPhase::successPresentation && current == GameplayPhase::level2Handoff)
        : !supportsRetainedPages() && !supportsFailurePages() && !supportsContinuePages()
            && !supportsSuccessPages() && !supportsNameEntryPages() && !supportsHallPages())) {
        pixels_.reset();
        radar_.reset();
        level1Pages_ = false;
    }
    if (current == GameplayPhase::campaignEnded) {
        synth_.stopMusic();
        pageProjection_ = NativePageProjection::cleanUnqualified;
        return delivery;
    }
    if (previous == GameplayPhase::continuePrompt && current == GameplayPhase::level1RetryHandoff) {
        synth_.stopMusic();
        synth_.startMusic();
        level1Cursor_ = 0;
        failureCursor_ = 0;
    }
    if (previous == GameplayPhase::continuePrompt && current == GameplayPhase::level2Handoff) {
        synth_.stopMusic();
        synth_.switchLevel2();
        level2Cursor_ = 0;
        failureCursor_ = 0;
    }
    if (previous == GameplayPhase::failurePreExplosionBlackout && current == GameplayPhase::failureExplosion) {
        failureCursor_ = 0;
    }
    const auto isSuccess = [](const GameplayPhase phase) {
        return phase == GameplayPhase::successPresentation || phase == GameplayPhase::level2SuccessPresentation
            || phase == GameplayPhase::level3SuccessPresentation;
    };
    if (current != previous && isSuccess(current)) {
        synth_.startSuccess();
    }
    if (previous == GameplayPhase::successPresentation && current == GameplayPhase::level2Handoff) {
        if (!retainedLevel1Return) {
            synth_.stopSuccess();
        }
        synth_.switchLevel2();
    }
    if (previous == GameplayPhase::level2SuccessPresentation && current == GameplayPhase::level3Handoff) {
        synth_.stopSuccess();
        synth_.switchLevel3();
        // Campaign pool initialization clears the shared Level2/3 event history.
        level2Cursor_ = 0;
    }
    if (previous == GameplayPhase::level3SuccessPresentation && current == GameplayPhase::level4Handoff) {
        synth_.stopSuccess();
    }
    if (previous == GameplayPhase::successPresentation && current == GameplayPhase::level2Handoff
        && supportsRetainedPages()) {
        initializePages(NativeSessionStart::level2);
    }
    if (previous == GameplayPhase::level1RetryHandoff && !level1Pages_ && supportsLevel1Pages()) {
        initializePages(NativeSessionStart::level1);
    }
    if ((!level1Pages_ && previous == GameplayPhase::level1 && current != GameplayPhase::successPresentation)
        || (!level1Pages_ && previous == GameplayPhase::level1RetryHandoff && current == GameplayPhase::level1)
        || (!pixels_ && (current == GameplayPhase::level2Handoff || current == GameplayPhase::level2))) {
        pages_.beginFrame();
        pages_.flip();
    }
    if (current == GameplayPhase::level1RetryHandoff && retainedLevel1Retry) {
        for (std::int16_t y = 0; y < screenHeight; ++y) {
            for (std::int16_t x = 0; x < screenWidth; ++x) {
                frame_.pixels[y * screenWidth + x] = pixels_->read(pages_.frontPage(), {x, y});
            }
        }
        pageProjection_ = NativePageProjection::retainedLevel1Loader;
    } else {
        refreshFrame();
    }
    if (boundary == Level1Boundary::completedTail) {
        if (pageProjection_ != NativePageProjection::retainedNativeLevel1 || !pixels_ || !radar_ || !level1Pages_) {
            throw std::invalid_argument("complete Level1 frame lost its physical display ownership");
        }
        model_.finishLevel1Frame();
    }
    dispatch(model_.level1State().audioEvents(), level1Cursor_, delivery);
    dispatch(model_.level2AudioEvents(), level2Cursor_, delivery);
    dispatch(model_.failureAudioEvents(), failureCursor_, delivery);
    if (startsLevelEntry(previous, current)) {
        enter(delivery);
        if (retainedLevel1Retry) {
            synth_.resetDirectEffects();
            pixels_->apply(scorePageEffects(model_.score(), assets_.scoreFont, pages_.backPage(), pages_.frontPage()));
            const auto& systems = model_.level1State();
            pixels_->apply(statusPageEffects(systems.systemStatusLevels(), systems.systemTimers(), systems.selectedSystem(),
                assets_.artGroups.at(overlayGroup), pages_.backPage(), pages_.frontPage()));
            for (std::int16_t y = 0; y < screenHeight; ++y) {
                for (std::int16_t x = 0; x < screenWidth; ++x) {
                    frame_.pixels[y * screenWidth + x] = pixels_->read(pages_.frontPage(), {x, y});
                }
            }
        }
    }
    if (current == GameplayPhase::level5) {
        model_.finishLevel5Frame();
        renderWeaponLamp();
        dispatch(model_.level2AudioEvents(), level2Cursor_, delivery);
    }
    if (successPalette_) {
        auto palette = clock_.renderSuccessPalette(*successPalette_, model_.presentationFrame() == 0
            ? SuccessPaletteExecution::initial : SuccessPaletteExecution::advance);
        successPalette_ = palette.state;
        for (const auto& commit : palette.commits) {
            const auto expand = [](const std::uint8_t value) {
                return static_cast<std::uint8_t>((value << 2) | (value >> 4));
            };
            frame_.palette[commit.index] = {expand(commit.color[0]), expand(commit.color[1]), expand(commit.color[2])};
        }
        delivery.samples = std::move(palette.samples);
    } else {
        delivery.samples = clock_.renderFrame();
    }
    return delivery;
}

}  // namespace stellar7
