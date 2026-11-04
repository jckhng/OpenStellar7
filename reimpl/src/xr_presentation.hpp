#pragma once
#include "spatial_view.hpp"
#include "xr_view.hpp"
#include <memory>
#include <mutex>

namespace stellar7 {
struct XrPresentation {SpatialScene scene;std::vector<Rgb> palette;};
inline constexpr float xrDefaultUnitsPerMetre=200;
XrViewScale xrWorldViewScale(float nativeUnitsPerMetre);
XrPresentation xrWorld(const SpatialScene& source,const IndexedScreen& indexed,EnhancedView view,const SpatialOptions& options,float nativeUnitsPerMetre);
XrPresentation xrPanel(const IndexedScreen& indexed,float nativeUnitsPerMetre);
IndexedScreen xrOptionsScreen(const std::vector<std::uint32_t>& argb);
// Latest-wins publication bounds latency and memory. A reader retains one const
// snapshot across both eyes; publication never waits for a runtime/GPU frame.
class XrPresentationMailbox {
public:
    void publish(std::shared_ptr<const XrPresentation> presentation);
    std::shared_ptr<const XrPresentation> latest() const;
private:
    mutable std::mutex mutex_;
    std::shared_ptr<const XrPresentation> latest_;
};
}
