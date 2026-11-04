#pragma once
#include "openxr_session.hpp"
#include <SDL.h>
namespace stellar7 {
// Queries an already-current SDL context; never creates a display/context or
// changes drivers. WGL on Windows, EGL+XR_MNDX_egl_enable on Linux.
class OpenXrNativeBinding {
public:
    OpenXrNativeBinding(SDL_Window* window,GlProfile profile);
    ~OpenXrNativeBinding();
    OpenXrGraphics graphics() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
