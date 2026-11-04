#pragma once
#include <openxr/openxr.h>
#include <memory>
namespace stellar7 {
const char* openXrLoaderName();
class OpenXrLoader {
public:
    explicit OpenXrLoader(const char* path=nullptr);
    PFN_xrGetInstanceProcAddr entry() const;
private:
    struct Unload {void operator()(void* value) const;};
    std::unique_ptr<void,Unload> object_;
    PFN_xrGetInstanceProcAddr entry_=nullptr;
};
}
