#include "openxr_pump.hpp"
namespace stellar7 {
OpenXrFrame OpenXrPump::step(OpenXrSession& session,GlSceneRenderer& renderer,const XrPresentationMailbox& mailbox) {
    std::shared_ptr<const XrPresentation> current;
    return session.frameFromLatest([&] {
        current=mailbox.latest();return current?OpenXrLayer::projection:OpenXrLayer::none;
    },[&](const auto& eye) {
        if(prepared_!=current) {renderer.prepare(current->scene,current->palette);prepared_=current;}
        renderer.renderTexture(eye.clipFromNative,eye.texture,eye.width,eye.height);
    });
}
}
