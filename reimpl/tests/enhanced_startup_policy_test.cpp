#include "enhanced_runtime.hpp"
#include <optional>
#include <stdexcept>
#include <string>

namespace {
constexpr char modeVariable[]="STELLAR7_SAFE_MODE";
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
void environment(const char* value) {
#ifdef _WIN32
    require(_putenv_s(modeVariable,value?value:"")==0,"cannot set child environment");
#else
    require((value?setenv(modeVariable,value,1):unsetenv(modeVariable))==0,"cannot set child environment");
#endif
}
void hostEnvironment(const char* name,const char* value) {
#ifdef _WIN32
    require(_putenv_s(name,value)==0,"cannot set host environment");
#else
    require(setenv(name,value,1)==0,"cannot set host environment");
#endif
}
class RestoreEnvironment {
    std::optional<std::string> value_;
public:
    RestoreEnvironment() {const auto* current=std::getenv(modeVariable);if(current) {value_=current;}}
    ~RestoreEnvironment() {environment(value_?value_->c_str():nullptr);}
};
}
int main() try {
    hostEnvironment("STELLAR7_GAMEPAD","0");
    RestoreEnvironment restore;environment(nullptr);
#ifdef _WIN32
    require(stellar7::enhancedSafeMode(),"Windows enhanced startup must default to software/no-controller mode");
#else
    require(!stellar7::enhancedSafeMode(),"other platforms must retain their existing default");
#endif
    environment("1");require(stellar7::enhancedSafeMode(),"explicit safe mode was ignored");
    hostEnvironment("STELLAR7_GRAPHICS","opengl");
    require(stellar7::enhancedGraphics()==stellar7::EnhancedGraphics::cpu,"GL selector escaped safe mode");
    environment("0");require(!stellar7::enhancedSafeMode(),"explicit diagnostic opt-in was ignored");
    require(stellar7::enhancedGraphics()==stellar7::EnhancedGraphics::opengl,"desktop GL selector ignored");
    hostEnvironment("STELLAR7_GRAPHICS","gles");
    require(stellar7::enhancedGraphics()==stellar7::EnhancedGraphics::gles,"GLES selector ignored");
    hostEnvironment("STELLAR7_GRAPHICS","cpu");
    environment("unexpected");require(stellar7::enhancedSafeMode(),"invalid mode must fail closed");
    hostEnvironment("STELLAR7_HOST_RENDERER","direct3d11");hostEnvironment("STELLAR7_HOST_CONTROLLERS","1");
    require(!stellar7::enhancedControllersEnabled(),"host override escaped safe input guard");
    require(std::strcmp(stellar7::enhancedHostRenderer(),"software")==0,"host override escaped safe renderer guard");
    environment("0");
    require(stellar7::enhancedControllersEnabled(),"independent controller enable was ignored");
    require(std::strcmp(stellar7::enhancedHostRenderer(),"direct3d11")==0,"exact renderer override was ignored");
    hostEnvironment("STELLAR7_HOST_CONTROLLERS","0");
    require(!stellar7::enhancedControllersEnabled(),"independent controller disable was ignored");
    hostEnvironment("STELLAR7_HOST_RENDERER","software");hostEnvironment("STELLAR7_HOST_CONTROLLERS","1");
    require(stellar7::enhancedControllersEnabled(),"software rendering also disabled controllers");
    hostEnvironment("STELLAR7_HOST_CONTROLLERS","invalid");
    bool rejected=false;try {stellar7::enhancedControllersEnabled();} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"malformed host input policy was silently accepted");
    hostEnvironment("STELLAR7_GRAPHICS","invalid");rejected=false;
    try {stellar7::enhancedGraphics();} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"malformed graphics selector was silently accepted");
    hostEnvironment("STELLAR7_XR","1");hostEnvironment("STELLAR7_XR_UNITS_PER_METRE","invalid");
    environment("1");require(!stellar7::enhancedXrScale(),"XR request escaped safe mode");
    environment("0");rejected=false;
    try {stellar7::enhancedXrScale();} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"XR accepted an invalid physical scale");
    for(const auto* value:{"0","-1","nan","inf","1000junk",""}) {
        hostEnvironment("STELLAR7_XR_UNITS_PER_METRE",value);rejected=false;
        try {stellar7::enhancedXrScale();} catch(const std::invalid_argument&) {rejected=true;}
        require(rejected,"XR accepted an unsupported scale");
    }
    hostEnvironment("STELLAR7_XR_UNITS_PER_METRE","1000");hostEnvironment("STELLAR7_HOST_RENDERER","software");
    require(stellar7::enhancedXrScale()==1000&&stellar7::enhancedHostOverrides(),"XR explicit opt-in/no-save policy");
    require(std::strcmp(stellar7::enhancedHostRenderer(),"software")==0,"XR mirror did not select software");
    hostEnvironment("STELLAR7_HOST_RENDERER","opengl");rejected=false;
    try {stellar7::enhancedHostRenderer();} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"XR accepted competing desktop GL ownership");
    hostEnvironment("STELLAR7_XR","unexpected");rejected=false;
    try {stellar7::enhancedXrScale();} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"malformed XR selector silently accepted");
    hostEnvironment("STELLAR7_XR","0");require(!stellar7::enhancedXrScale(),"disabled XR requested a runtime");
    hostEnvironment("STELLAR7_GAMEPAD","1");
    require(stellar7::enhancedGamepadMode()&&stellar7::enhancedGraphics()==stellar7::EnhancedGraphics::cpu
        &&std::strcmp(stellar7::enhancedHostRenderer(),"software")==0,"gamepad opt-in enabled accelerated rendering");
    hostEnvironment("STELLAR7_XR_CONTROLLERS","0");require(!stellar7::enhancedXrControllersEnabled(),"XR keyboard fallback ignored");
    hostEnvironment("STELLAR7_XR_CONTROLLERS","1");require(stellar7::enhancedXrControllersEnabled(),"XR controllers not enabled");
    hostEnvironment("STELLAR7_XR_CONTROLLERS","invalid");rejected=false;
    try {stellar7::enhancedXrControllersEnabled();} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"malformed XR controller option accepted");
} catch(const std::exception& error) {
    std::cerr<<error.what()<<'\n';return 1;
}
