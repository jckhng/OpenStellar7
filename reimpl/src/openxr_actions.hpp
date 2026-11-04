#pragma once
#include "controller_input.hpp"
#include <openxr/openxr.h>
#include <memory>

namespace stellar7 {
class OpenXrActions {
public:
    OpenXrActions(PFN_xrGetInstanceProcAddr loader,XrInstance instance,XrSession session,bool picoProfile);
    ~OpenXrActions();
    OpenXrActions(const OpenXrActions&)=delete;
    OpenXrActions& operator=(const OpenXrActions&)=delete;
    ControllerInput poll(bool focused);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
