#include "openxr_loader.hpp"
#include <SDL.h>
#include <stdexcept>
#include <string>
namespace stellar7 {
const char* openXrLoaderName() {
#ifdef _WIN32
    return "openxr_loader.dll";
#elif defined(__linux__)
    return "libopenxr_loader.so.1";
#else
    throw std::runtime_error("No OpenXR loader configured for this host");
#endif
}
void OpenXrLoader::Unload::operator()(void* value) const {SDL_UnloadObject(value);}
OpenXrLoader::OpenXrLoader(const char* path) {
    const auto* library=path?path:openXrLoaderName();object_.reset(SDL_LoadObject(library));
    if(!object_) {throw std::runtime_error(std::string("Cannot load OpenXR loader ")+library+": "+SDL_GetError());}
    entry_=reinterpret_cast<PFN_xrGetInstanceProcAddr>(SDL_LoadFunction(object_.get(),"xrGetInstanceProcAddr"));
    if(!entry_) {throw std::runtime_error(std::string("OpenXR loader lacks xrGetInstanceProcAddr: ")+SDL_GetError());}
}
PFN_xrGetInstanceProcAddr OpenXrLoader::entry() const {return entry_;}
}
