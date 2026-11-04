#pragma once
namespace stellar7 {
enum class CockpitStyle {original,spatial};
struct SpatialOptions {
    CockpitStyle cockpit=CockpitStyle::spatial;
    bool groundDetail=true,localLights=true;
    bool operator==(const SpatialOptions& other) const {
        return cockpit==other.cockpit&&groundDetail==other.groundDetail&&localLights==other.localLights;
    }
};
}
