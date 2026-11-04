#include "native_player_platform.hpp"
#include <SDL.h>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace stellar7::player_platform {
namespace {
#ifdef STELLAR7_ENHANCED
constexpr char profileName[]="Stellar7Enhanced";
#else
constexpr char profileName[]="Stellar7";
#endif
}
std::vector<std::filesystem::path> commandArguments(const int argc,const char* const argv[]) {
    std::vector<std::filesystem::path> result;
    for(int index=1;index<argc;++index) {result.push_back(std::filesystem::u8path(argv[index]));}
    return result;
}
std::filesystem::path defaultProfile() {
#ifdef _WIN32
    const auto local=SDL_getenv("LOCALAPPDATA");
    if(!local||!*local) {throw std::runtime_error("Cannot find Local AppData. Supply --user-dir PROFILE.");}
    const auto root=std::filesystem::u8path(local);
    if(!root.is_absolute()) {throw std::runtime_error("Local AppData is not absolute. Supply --user-dir PROFILE.");}
    return root/profileName;
#else
    const std::unique_ptr<char,decltype(&SDL_free)> path(SDL_GetPrefPath("dos-re-harness",profileName),SDL_free);
    if(!path) {throw std::runtime_error(std::string("Cannot find user data: ")+SDL_GetError()+". Supply --user-dir PROFILE.");}
    return std::filesystem::u8path(path.get());
#endif
}
std::filesystem::path executableDirectory() {
    const std::unique_ptr<char,decltype(&SDL_free)> owned(SDL_GetBasePath(),SDL_free);
    const auto base=owned.get();
    if(!base||!*base) {throw std::runtime_error("Cannot locate application datafile folder. Supply --data-dir PATH.");}
    const auto path=std::filesystem::u8path(base);
    if(!path.is_absolute()) {throw std::runtime_error("Application folder is not absolute. Supply --data-dir PATH.");}
    return path;
}
std::optional<std::filesystem::path> chooseOriginalFolder() {
    throw std::runtime_error("Folder selection is unavailable in the SDL2 host. Use datafile, --data-dir PATH or --import-data ORIGINAL_FOLDER.");
}
void showError(const std::string& message) {
    const auto result=SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Stellar 7",message.c_str(),nullptr);
    const bool failed=result<0;
    if(failed) {
        std::cerr<<"Cannot display error dialog: "<<SDL_GetError()<<'\n';
    }
}
}
