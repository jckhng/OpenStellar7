#pragma once
#include "gl_scene_renderer.hpp"
#include "xr_view.hpp"
#include "controller_input.hpp"
#include <openxr/openxr.h>
#include <functional>

namespace stellar7 {
enum class OpenXrBinding {platform,egl};
struct OpenXrGraphics {
    GlProfile profile;
    const void* binding;
    XrVersion contextVersion;
    OpenXrBinding api=OpenXrBinding::platform;
};
using OpenXrGraphicsFactory=std::function<OpenXrGraphics(XrVersion minimum,XrVersion maximum)>;
struct OpenXrEye {
    unsigned eye=0;
    std::uint32_t texture=0;
    int width=0,height=0;
    SpatialMatrix clipFromNative{};
};
enum class OpenXrFrame {idle,empty,submitted,exiting};
enum class OpenXrLayer {projection,none};
enum class OpenXrInput {none,controllers};
// Optional runtime boundary. The host supplies its loader and a current graphics
// context/binding; this class never opens a window, starts audio or advances a tick.
class OpenXrSession {
public:
    OpenXrSession(PFN_xrGetInstanceProcAddr loader,const OpenXrGraphics& graphics,const XrViewScale& scale,
        OpenXrInput input=OpenXrInput::none);
    OpenXrSession(PFN_xrGetInstanceProcAddr loader,GlProfile profile,OpenXrBinding api,
        const XrViewScale& scale,const OpenXrGraphicsFactory& createGraphics,OpenXrInput input=OpenXrInput::none);
    ~OpenXrSession();
    OpenXrSession(const OpenXrSession&)=delete;
    OpenXrSession& operator=(const OpenXrSession&)=delete;
    OpenXrFrame frame(const std::function<void(const OpenXrEye&)>& draw,OpenXrLayer layer=OpenXrLayer::projection);
    OpenXrFrame frameFromLatest(const std::function<OpenXrLayer()>& select,const std::function<void(const OpenXrEye&)>& draw);
    void recenter();
    bool pollShutdown();
    bool focused() const;
    ControllerInput controls() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// Prepare exactly one immutable scene, then use the runtime poses/FOV/texture for
// each eye. No full-screen indexed cockpit overlay is submitted to the headset.
OpenXrFrame presentOpenXrScene(OpenXrSession& session,GlSceneRenderer& renderer,
    const SpatialScene& scene,const std::vector<Rgb>& palette);
} // namespace stellar7
