#pragma once
#include "chase_options.hpp"
#include "spatial_options.hpp"
#include <array>
#include <filesystem>
#include <string>
#include <utility>

namespace stellar7 {
enum class EnhancedAction { left,right,forward,reverse,fire,activate,cycle,decoy,bomb,jump,shield,scanner,superPrimary,turbo,menu,count };
inline constexpr std::size_t enhancedActionCount=static_cast<std::size_t>(EnhancedAction::count);
inline constexpr std::array<const char*,enhancedActionCount> enhancedActionNames{
    "left","right","forward","reverse","fire","activate","cycle","decoy","bomb","jump","shield","scanner","super","turbo","menu"};
enum class EnhancedAspect { display4by3,sourcePixels };
enum class EnhancedScaling { fit,integer };
enum class EnhancedFilter { sharp,smooth };
enum class EnhancedController { modern,original,off };
struct EnhancedBinding {std::string key,button;};
struct EnhancedOptions {
    ChaseOptions camera;
    SpatialOptions scene;
    bool fullscreen=false;
    EnhancedAspect aspect=EnhancedAspect::display4by3;
    EnhancedScaling scaling=EnhancedScaling::fit;
    EnhancedFilter filter=EnhancedFilter::sharp;
    EnhancedController controller=EnhancedController::modern;
    int width=960,height=720,deadzone=20;
    int volume=100;
    bool muted=false;
    unsigned gameRate=25;
    unsigned renderScale=4,presentationRate=60;
    std::array<EnhancedBinding,enhancedActionCount> bindings{{
        {"A","dpleft"},{"D","dpright"},{"W","dpup"},{"S","dpdown"},{"Space","a"},
        {"Return","x"},{"Tab","leftshoulder"},{"I","y"},{"B","b"},{"J","rightshoulder"},
        {"E","none"},{"C","none"},{"X","none"},{"T","none"},{"Escape","start"}}};
};
class EnhancedOptionsStore {
public:
    explicit EnhancedOptionsStore(std::filesystem::path path):path_(std::move(path)) {}
    EnhancedOptions load() const;
    void save(const EnhancedOptions& options) const;
private:
    std::filesystem::path path_;
};
} //namespace stellar7
