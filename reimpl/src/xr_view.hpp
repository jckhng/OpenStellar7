#pragma once
#include "spatial_scene.hpp"

namespace stellar7 {
struct XrOrientation {float x=0,y=0,z=0,w=1;};
struct XrPose {XrOrientation orientation;SpatialPoint position;};
struct XrFov {float left=0,right=0,up=0,down=0;};
struct XrView {XrPose pose;XrFov fov;};
struct XrViewScale {
    float nativeUnitsPerMetre;
    float nearMetres=0.05f,farMetres=500;
};
// Poses use OpenXR's right/up/back axes and metres. Scene vertices already use
// those axes in the GL device; their native distance scale is converted here.
SpatialMatrix xrClipFromNative(const XrView& eye,const XrPose& referenceFromSeat,const XrViewScale& scale);
XrPose xrSeatedOrigin(const XrPose& head);
XrPose xrRebasedPose(const XrPose& previousPose,const XrPose& previousFromNew);
} // namespace stellar7
