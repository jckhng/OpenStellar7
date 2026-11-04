#pragma once
#include "spatial_view.hpp"
#include "controller_input.hpp"
#include <memory>
namespace stellar7 {
// Main thread owns the hidden SDL window and joins before deleting its context.
// The worker queries runtime requirements before creating GL, then renders latest
// immutable content at runtime cadence.
class OpenXrSdlHost {
public:
    explicit OpenXrSdlHost(float nativeUnitsPerMetre);
    ~OpenXrSdlHost();
    void world(std::shared_ptr<const SpatialScene> source,const IndexedScreen& indexed,EnhancedView view,const SpatialOptions& options);
    void panel(const IndexedScreen& indexed);
    void optionsPanel(const std::vector<std::uint32_t>& argb);
    void recenter();
    bool focused() const;
    bool exiting() const;
    ControllerInput controls();
    void check() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
