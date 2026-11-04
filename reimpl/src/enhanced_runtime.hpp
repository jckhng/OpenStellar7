#pragma once
#include <cstdlib>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>

namespace stellar7 {
enum class EnhancedGraphics {cpu,opengl,gles};
inline bool enhancedGamepadMode() {
    const auto* value=std::getenv("STELLAR7_GAMEPAD");
    if(!value||std::strcmp(value,"0")==0) {return false;}
    if(std::strcmp(value,"1")==0) {return true;}
    throw std::invalid_argument("STELLAR7_GAMEPAD requires 0 or 1");
}
inline bool enhancedSafeMode() {
    const auto* value=std::getenv("STELLAR7_SAFE_MODE");
    if(value) {return std::strcmp(value,"0")!=0;}
#ifdef _WIN32
    // Contain the reported desktop lockup until hardware startup is qualified.
    return true;
#else
    return false;
#endif
}
inline std::optional<float> enhancedXrScale() {
    if(enhancedSafeMode()) {return std::nullopt;}
    const auto* enabled=std::getenv("STELLAR7_XR");
    if(!enabled||std::strcmp(enabled,"0")==0) {return std::nullopt;}
    if(std::strcmp(enabled,"1")!=0) {throw std::invalid_argument("STELLAR7_XR requires 0 or 1");}
    const auto* value=std::getenv("STELLAR7_XR_UNITS_PER_METRE");
    if(!value||!*value) {throw std::invalid_argument("OpenXR requires explicit STELLAR7_XR_UNITS_PER_METRE");}
    char* end=nullptr;errno=0;const auto scale=std::strtof(value,&end);
    if(errno||end==value||*end||!std::isfinite(scale)||scale<=0) {throw std::invalid_argument("Invalid STELLAR7_XR_UNITS_PER_METRE");}
    return scale;
}
inline bool enhancedControllersEnabled() {
    if(enhancedSafeMode()) {return false;}
    const auto* value=std::getenv("STELLAR7_HOST_CONTROLLERS");
    if(!value) {return true;}
    if(std::strcmp(value,"0")==0) {return false;}
    if(std::strcmp(value,"1")==0) {return true;}
    throw std::invalid_argument("STELLAR7_HOST_CONTROLLERS requires 0 or 1");
}
inline bool enhancedXrControllersEnabled() {
    const auto* value=std::getenv("STELLAR7_XR_CONTROLLERS");
    if(!value||std::strcmp(value,"1")==0) {return true;}
    if(std::strcmp(value,"0")==0) {return false;}
    throw std::invalid_argument("STELLAR7_XR_CONTROLLERS requires 0 or 1");
}
inline EnhancedGraphics enhancedGraphics() {
    if(enhancedSafeMode()) {return EnhancedGraphics::cpu;}
    if(enhancedGamepadMode()) {return EnhancedGraphics::cpu;}
    const auto* value=std::getenv("STELLAR7_GRAPHICS");
    if(!value||std::strcmp(value,"cpu")==0) {return EnhancedGraphics::cpu;}
    if(std::strcmp(value,"opengl")==0) {return EnhancedGraphics::opengl;}
    if(std::strcmp(value,"gles")==0) {return EnhancedGraphics::gles;}
    throw std::invalid_argument("STELLAR7_GRAPHICS requires cpu, opengl or gles");
}
inline const char* enhancedHostRenderer() {
    if(enhancedSafeMode()) {return "software";}
    if(enhancedGamepadMode()) {return "software";}
    const auto* value=std::getenv("STELLAR7_HOST_RENDERER");
    if(enhancedXrScale()) {
        if(value&&std::strcmp(value,"software")!=0) {throw std::invalid_argument("OpenXR currently requires a software desktop mirror");}
        return "software";
    }
    if(value&&(*value=='\0'||std::strlen(value)>63)) {
        throw std::invalid_argument("STELLAR7_HOST_RENDERER requires an exact SDL renderer name");
    }
    return value;
}
inline bool enhancedHostOverrides() {
    return std::getenv("STELLAR7_HOST_RENDERER")||std::getenv("STELLAR7_HOST_CONTROLLERS")||std::getenv("STELLAR7_GRAPHICS")
        ||std::getenv("STELLAR7_XR")||std::getenv("STELLAR7_XR_UNITS_PER_METRE");
}
inline bool enhancedHostDiagnosticsEnabled() {
    const auto* value=std::getenv("STELLAR7_HOST_DIAGNOSTICS");
    return value&&std::strcmp(value,"1")==0;
}
inline void enhancedHostTrace(const char* stage) {
    if(!enhancedHostDiagnosticsEnabled()) {return;}
    static std::mutex output;
    const std::lock_guard<std::mutex> lock(output);
    std::cerr<<"Stellar7 SDL host: "<<stage<<std::endl;
}
}
