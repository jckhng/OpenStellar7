#pragma once
#include "openxr_session.hpp"
#include "xr_presentation.hpp"
namespace stellar7 {
class OpenXrPump {
public:
    OpenXrFrame step(OpenXrSession& session,GlSceneRenderer& renderer,const XrPresentationMailbox& mailbox);
private:
    std::shared_ptr<const XrPresentation> prepared_;
};
}
