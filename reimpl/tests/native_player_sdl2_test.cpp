#include "native_player_platform.hpp"
#include <SDL.h>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {
std::string base,local=u8"C:/Isolated SDL Profile Ω";
unsigned freed=0,messages=0;
void require(bool value) {if(!value) {throw std::runtime_error("SDL2 platform contract failed");}}
char* copy(const std::string& text) {
    auto* result=static_cast<char*>(std::malloc(text.size()+1));std::memcpy(result,text.c_str(),text.size()+1);return result;
}
template<class Action> bool rejects(Action action) {try {action();return false;}catch(const std::exception&) {return true;}}
}
const char* SDL_GetError() {return "mock SDL2 error";}
char* SDL_getenv(const char* name) {require(std::string(name)=="LOCALAPPDATA");return local.empty()?nullptr:local.data();}
char* SDL_GetPrefPath(const char* organization,const char* application) {
    require(std::string(organization)=="dos-re-harness"&&std::string(application)=="Stellar7");
    return copy("/tmp/isolated-sdl-profile/");
}
char* SDL_GetBasePath() {return base.empty()?nullptr:copy(base);}
void SDL_free(void* value) {if(value) {++freed;}std::free(value);}
int SDL_ShowSimpleMessageBox(Uint32,const char*,const char*,SDL_Window*) {++messages;return 0;}
int main() {
    using namespace stellar7::player_platform;
    const char* args[]{"stellar7","--data-dir",u8"C:/Original Ω"};
    const auto converted=commandArguments(3,args);require(converted.size()==2&&converted[1]==std::filesystem::u8path(args[2]));
#ifdef _WIN32
    require(defaultProfile()==std::filesystem::u8path(local)/"Stellar7");
    local.clear();require(rejects([] {defaultProfile();}));
    base=u8"C:/Application Folder Ω/";
#else
    require(defaultProfile()=="/tmp/isolated-sdl-profile/");
    base=u8"/tmp/Application Folder Ω/";
#endif
    const auto previous=freed;
    require(executableDirectory()==std::filesystem::u8path(base)&&freed==previous+1);
    base.clear();require(rejects([] {executableDirectory();}));
    base="relative/";require(rejects([] {executableDirectory();}));
    require(rejects([] {chooseOriginalFolder();}));
    showError("test");require(messages==1);
}
