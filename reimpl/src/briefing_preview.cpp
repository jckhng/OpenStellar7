#include "briefing_preview.hpp"
#include "stellar7/failure_model_work.hpp"
#include "stellar7/level1_owned_view_work.hpp"
#include "stellar7/level1_deployment_render_work.hpp"
#include "stellar7/raster_line_work.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <sstream>

namespace stellar7 {
namespace {
constexpr Viewport previewViewport{50, 294, 35, 109};
constexpr Viewport rasterViewport{0, 318, 0, 199};
constexpr ProjectedPoint geometryOffset{13, -12}; //159,84 ->172,72.
constexpr std::int16_t sourceYawStep = 256;
constexpr std::int16_t sourceYawIncrease = 64;
constexpr std::int16_t maximumYawStep = 2560;
constexpr std::int16_t referenceType = 24;
constexpr std::uint8_t referenceDelay = 5;

const Model3d& modelFor(const GameplayAssets& assets, const std::int16_t type) {
    if (type == 44 && !assets.campaignLevels.empty() && !assets.campaignLevels[0].models.empty()) {
        return assets.campaignLevels[0].models[0];
    }
    if (type >= 0 && static_cast<std::size_t>(type) < assets.models.size()) {
        return assets.models[type];
    }
    throw std::invalid_argument("briefing preview requires its real shared or Level1 local model");
}

WorldModelSetupWork setupPixels(const Model3d& model, const Level1Entity& actor, const PlayableModelDetail detail) {
    if (model.lods.empty() || model.scaleShift != 0
        || model.origin != std::array<std::int16_t, 3>{}) {
        throw std::invalid_argument("briefing model requires its native precision0 descriptor");
    }
    //BA88/CB7A radius controls LOD; positions in this source menu stay precision0.
    const auto minimumDepth = std::max<std::int64_t>(128,
        std::int64_t{actor.position.y} - actor.collisionRadius);
    auto projectedRadius = static_cast<std::uint16_t>(
        (std::uint64_t{actor.collisionRadius} << 7) / minimumDepth);
    if (detail.scale != 256) {
        //C69A uses descriptor0's retained scale/cap, not the current menu slider value.
        projectedRadius = static_cast<std::uint16_t>(std::min<std::uint32_t>(
            std::uint32_t{std::min<std::uint16_t>(projectedRadius, 256)} * detail.scale / 256, detail.cap));
    }
    auto selected = std::find_if(model.lods.begin(), model.lods.end(), [projectedRadius](const auto& lod) {
        return projectedRadius <= lod.threshold;
    });
    if (selected == model.lods.end()) {
        selected = model.lods.end() - 1;
    }
    WorldModelSetupWork setup{};
    setup.matrix = makeFixedMatrixQ14(actor.pitch, actor.roll, actor.yaw);
    setup.translation = actor.position;
    setup.selectedLodIndex = static_cast<std::size_t>(selected - model.lods.begin());
    setup.smallRadius = 1;
    for (std::size_t index = 0; index < selected->groups.size(); ++index) {
        const auto& group = selected->groups[index];
        if (group.originVertex >= group.vertices.size()) {
            throw std::invalid_argument("briefing group requires its source sort origin");
        }
        const auto& vertex = group.vertices[group.originVertex];
        const auto rotated = transformToView({vertex[0], vertex[1], vertex[2]},
            {{}, setup.matrix, {}, 0});
        const auto depth = static_cast<std::uint32_t>(std::int64_t{rotated.x} * actor.position.x
            + std::int64_t{rotated.y} * actor.position.y + std::int64_t{rotated.z} * actor.position.z);
        setup.groupOrder.push_back(index);
        setup.groupDepths.push_back(static_cast<std::int32_t>(depth));
    }
    //C9DE selection sort: descending signed32 depth; equality keeps selected index.
    for (std::size_t destination = 0; destination < setup.groupOrder.size(); ++destination) {
        auto selectedIndex = destination;
        for (std::size_t candidate = destination + 1; candidate < setup.groupOrder.size(); ++candidate) {
            if (setup.groupDepths[setup.groupOrder[selectedIndex]] < setup.groupDepths[setup.groupOrder[candidate]]) {
                selectedIndex = candidate;
            }
        }
        std::swap(setup.groupOrder[destination], setup.groupOrder[selectedIndex]);
    }
    return setup;
}

void draw(std::vector<IndexedPageEffect>& effects, const GameplayAssets& assets,
    const Level1Entity& actor, const std::vector<std::uint8_t>& selectors, const std::uint16_t page,
    const PlayableModelDetail sharedDetail) {
    const auto& model = modelFor(assets, actor.type);
    const auto setup = setupPixels(model, actor, actor.type < 44 ? sharedDetail : PlayableModelDetail{});
    auto input = worldModelGeometryInput(assets, setup, actor.position, PolygonClipMode::Enabled);
    input.animationSelectors = selectors;
    if(actor.type==44) {input.matrixProfile=WorldModelMatrixProfile::level1Deployment;}
    const std::array<std::uint16_t,3> angles{actor.pitch,actor.roll,actor.yaw};
    const WorldModelViewInput view{makeFixedMatrixQ14(0,0,0),actor.position};
    const auto& lod=model.lods.at(setup.selectedLodIndex);
    WorldModelGeometryWork geometry;
    try {
        geometry = actor.type==44 ? level1DeploymentModelGeometryOperations(lod,input,angles,view,44)
            : level1OwnedModelGeometryOperations(lod,input,angles,view);
    } catch(const std::invalid_argument& error) {
        std::ostringstream detail;
        detail<<error.what()<<" briefing type="<<actor.type<<" yaw="<<actor.yaw
            <<" xyz="<<actor.position.x<<','<<actor.position.y<<','<<actor.position.z
            <<" lod="<<setup.selectedLodIndex<<" flags="<<static_cast<unsigned>(model.lods.at(setup.selectedLodIndex).flags)
            <<" matrix="<<input.matrix.mode<<" profile="<<static_cast<unsigned>(input.matrixProfile)
            <<" precision="<<input.vertexPrecision;
        throw std::invalid_argument(detail.str());
    }
    for (const auto& polygon : geometry.polygons) {
        auto points = polygon.points;
        for (auto& point : points) {
            point.x = static_cast<std::int16_t>(point.x + geometryOffset.x);
            point.y = static_cast<std::int16_t>(point.y + geometryOffset.y);
        }
        //Reuse actual native color/style raster; crop its pixel output to source preview.
        //The helper emits no CPU operations and claims no viewport-edge certification.
        const auto raster = ownedPolygonOperations(points, rasterViewport, PolygonClipMode::Enabled,
            polygon.filled ? FailurePolygonStyle::filled : FailurePolygonStyle::outline,
            polygon.primaryColor, polygon.secondaryColor);
        for (const auto& pixel : raster.pixels) {
            if (pixel.point.x < previewViewport.left || pixel.point.x > previewViewport.right
                || pixel.point.y < previewViewport.top || pixel.point.y > previewViewport.bottom) {
                continue;
            }
            effects.emplace_back(RadarPixelWrite{pixel.point, pixel.color, page});
        }
    }
}
}

BriefingViewer::BriefingViewer(const GameplayAssets& assets, const PlayableModelDetail sharedDetail)
    : sharedDetail_(sharedDetail) {
    if (sharedDetail.scale < 64 || sharedDetail.scale > 256 || sharedDetail.cap == 0 || sharedDetail.cap > 256) {
        throw std::invalid_argument("briefing requires a retained native model descriptor");
    }
    //1E0E..1E5C source-local template, then2776(secondary,5).
    reference_.type = referenceType;
    reference_.position = {0, 6000, -200};
    reference_.yawVelocity = sourceYawStep;
    const auto& model = modelFor(assets, referenceType);
    reference_.collisionRadius = model.collisionRadius;
    referenceSelectors_.assign(model.flags, 0); //3956 zero-filled allocation.
}

void BriefingViewer::select(const std::size_t textPage, const GameplayAssets& assets) {
    const auto selected = briefingPreview(textPage);
    if (!selected) {
        preview_.reset();
        return;
    }
    const auto yaw = actor_.yaw;
    Level1Entity staged{};
    staged.type = selected->model;
    staged.position = {0, 2000, 0};
    staged.yaw = yaw;
    staged.yawVelocity = sourceYawStep;
    std::int16_t step = 100;
    switch (staged.type) {
    case 1: staged.position.z = 100; break;
    case 4: staged.position = {0, 2150, 25}; break;
    case 5: staged.position.y = 2200; break;
    case 8: staged.position = {0, 1900, -50}; break;
    case 10: staged.position = {0, 2500, -50}; break;
    case 11: staged.position.z = 50; break;
    case 13: staged.position = {0, 2500, -100}; break;
    case 25: staged.position = {0, 2200, -50}; break;
    case 29: staged.position = {0, 3500, -250}; break;
    case 31: staged.position = {0, 6000, -200}; step = 200; break;
    case 44: staged.position = {0, 6000, -300}; step = 400; break;
    default: break;
    }
    if (staged.type != 250) {
        staged.collisionRadius = modelFor(assets, staged.type).collisionRadius;
        if (staged.type == 31) {
            staged.collisionRadius *= 2;
        }
    }
    actor_ = staged;
    baseline_ = staged.position.y;
    zoomStep_ = step;
    stripIndex_ = 0;
    preview_ = selected;
}

bool BriefingViewer::control(const BriefingViewerControl control) {
    if (!preview_) {
        return false;
    }
    switch (control) {
    case BriefingViewerControl::stop:
        actor_.yawVelocity = 0;
        return false;
    case BriefingViewerControl::faster:
        actor_.yawVelocity = std::min<std::int16_t>(maximumYawStep, actor_.yawVelocity + sourceYawIncrease);
        return false;
    case BriefingViewerControl::zoomIn:
        if (actor_.position.y - zoomStep_ >= baseline_ - 10 * zoomStep_) {
            actor_.position.y -= zoomStep_;
            return false;
        }
        stripIndex_ = static_cast<std::int16_t>(static_cast<std::uint16_t>(stripIndex_) + 1U);
        return true;
    case BriefingViewerControl::zoomOut:
        if (actor_.position.y + zoomStep_ <= baseline_ + 10 * zoomStep_) {
            actor_.position.y += zoomStep_;
            return false;
        }
        stripIndex_ = static_cast<std::int16_t>(static_cast<std::uint16_t>(stripIndex_) - 1U);
        return true;
    }
    throw std::invalid_argument("unknown briefing viewer control");
}

std::vector<IndexedPageEffect> BriefingViewer::poll(const GameplayAssets& assets, const std::uint16_t page) {
    if (!preview_) {
        return {};
    }
    auto staged = *this;
    if (staged.preview_->model == 29) {
        staged.actor_.type = static_cast<std::int16_t>(29 + (staged.obstaclePhase_ >> 7));
        ++staged.obstaclePhase_;
        if (staged.obstaclePhase_ == 256) {
            staged.obstaclePhase_ = 384;
        }
        if (staged.obstaclePhase_ == 896) {
            staged.obstaclePhase_ = 0;
        }
    }
    advanceEntityKinematics(staged.actor_);
    staged.reference_.position.y = staged.actor_.position.y;
    std::vector<IndexedPageEffect> effects;
    if (staged.actor_.type != 250) {
        const auto selectors = staged.preview_->model == 0 || staged.preview_->model == 31
            ? std::vector<std::uint8_t>{} : std::vector<std::uint8_t>(modelFor(assets, staged.actor_.type).flags, 0);
        draw(effects, assets, staged.actor_, selectors, page, staged.sharedDetail_);
    }
    if (staged.actor_.type == 31) {
        draw(effects, assets, staged.reference_, staged.referenceSelectors_, page, staged.sharedDetail_);
        if (staged.referenceWait_ == 0) {
            const auto& limits = modelFor(assets, referenceType).animationLimits;
            if (limits.size() != staged.referenceSelectors_.size()) {
                throw std::invalid_argument("briefing reference model requires its source animation limits");
            }
            for (std::size_t index = 0; index < limits.size(); ++index) {
                if (limits[index] != 0) {
                    ++staged.referenceSelectors_[index];
                    if (staged.referenceSelectors_[index] >= limits[index]) {
                        staged.referenceSelectors_[index] = 0;
                    }
                }
            }
            staged.referenceWait_ = referenceDelay;
        } else {
            --staged.referenceWait_;
        }
    }
    *this = std::move(staged);
    return effects;
}
} // namespace stellar7
