#include "stellar7/native_player.hpp"
#include "stellar7/native_install.hpp"
#include "stellar7/native_profile.hpp"
#include "native_player_platform.hpp"
#include "stellar7/native_game_rate.hpp"
#ifdef STELLAR7_ENHANCED
#include "enhanced_options.hpp"
#endif
#include <charconv>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace stellar7 {
namespace {
constexpr int invalidInstallation = 2;
constexpr const char* help =
#ifdef STELLAR7_ENHANCED
    "Stellar 7 Enhanced (SDL2) - F12 native options and controls\n"
#else
    "Stellar 7 portable reimplementation\n"
#endif
    "  stellar7 [--play] [--data-dir PATH] [--user-dir PROFILE] [--game-rate 0|25|35|50|70]\n"
    "  stellar7 --import-data ORIGINAL_FOLDER [--user-dir PROFILE]\n"
    "  stellar7 --verify-install [--data-dir PATH] [--user-dir PROFILE]\n"
    "  stellar7 --supported-data\n"
    "Place supported original resources in datafile beside the executable, or use --data-dir PATH.\n"
    "Previous imported profile/data installations remain supported. No folder picker opens automatically.\n"
#ifdef STELLAR7_ENHANCED
    "Default profile: platform user data (Windows: %LOCALAPPDATA%\\Stellar7Enhanced).\n"
#else
    "Default profile: platform user data (Windows: %LOCALAPPDATA%\\Stellar7).\n"
#endif
    "Import never overwrites an existing save. Unsupported data is rejected.\n";

using player_platform::commandArguments;
using player_platform::defaultProfile;
bool savesInsideResources(const NativeProfilePaths& paths) {
    std::error_code error;auto parent=std::filesystem::weakly_canonical(paths.profileDirectory,error);
    if(error) {throw std::runtime_error("Cannot resolve writable profile. Supply --user-dir PROFILE.");}
    while(!parent.empty()) {
        if(std::filesystem::equivalent(parent,paths.resourceDirectory,error)&&!error) {return true;}
        const auto next=parent.parent_path();if(next==parent) {break;}parent=next;
    }
    return false;
}
int reportFailure(const std::string& message,const bool interactive) {
    std::cerr<<message<<'\n';
    if(interactive) {player_platform::showError(message);}
    return invalidInstallation;
}
} // namespace

std::optional<int> runNativePlayerCommand(const int argc,const char* const argv[],const NativePlayerEntry& entry) {
    const auto args=commandArguments(argc,argv);
    const auto first=args.empty()?std::filesystem::path{}:args.front();
    const bool known=args.empty()||first=="--play"||first=="--user-dir"||first=="--game-rate"||first=="--data-dir"||first=="--import-data"
        ||first=="--verify-install"||first=="--supported-data"||first=="--help";
    if(!known) {return std::nullopt;}
    // Preserve the explicitly supplied legacy diagnostic --play DIR [PALETTE].
    if(first=="--play"&&args.size()>1&&!args[1].empty()
        &&args[1].native().front()!=std::filesystem::path("-").native().front()) {
        return std::nullopt;
    }
    if(first=="--help"||first=="--supported-data") {
        if(args.size()!=1) {return reportFailure("Unexpected arguments. Use --help.",false);}
        std::cout<<(first=="--help"?std::string(help):nativeSupportedDataJson())<<'\n';return 0;
    }
    std::optional<std::filesystem::path> profile,source,dataDirectory;
    unsigned gameRate=defaultNativeGameRate;
    bool rateSpecified=false;
    const bool importing=first=="--import-data",verify=first=="--verify-install";
    std::size_t next=(args.empty()||first=="--user-dir"||first=="--game-rate"||first=="--data-dir")?0:1;
    if(importing) {
        if(next>=args.size()||args[next].empty()||args[next]=="--user-dir") {
            return reportFailure("Supply --import-data ORIGINAL_FOLDER [--user-dir PROFILE].",false);
        }
        source=args[next++];
    }
    while(next<args.size()) {
        if(next+1>=args.size()||args[next+1].empty()) {
            return reportFailure("Invalid player arguments. Use --help.",false);
        }
        if(args[next]=="--user-dir"&&!profile) {profile=args[next+1];}
        else if(args[next]=="--data-dir"&&!dataDirectory&&!importing) {dataDirectory=args[next+1];}
        else if(args[next]=="--game-rate"&&!rateSpecified&&!importing&&!verify) {
            const auto text=args[next+1].u8string();
            const auto parsed=std::from_chars(text.data(),text.data()+text.size(),gameRate);
            if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||!validNativeGameRate(gameRate)) {
                return reportFailure("Use --game-rate 0,25,35,50 or70 (0 means uncapped native speed).",false);
            }
            rateSpecified=true;
        } else {return reportFailure("Invalid or duplicate player option. Use --help.",false);}
        next+=2;
    }
    const bool interactive=!importing&&!verify;
    try {
        const auto root=profile?*profile:defaultProfile();
        if(importing) {
            const auto imported=importNativeData(*source,root);
            if(!imported.ok) {return reportFailure(imported.message,false);}
            std::cout<<imported.message<<'\n';return 0;
        }
        auto paths=nativeProfilePaths(root);
        if(dataDirectory) {paths.resourceDirectory=*dataDirectory;}
        else {
            const auto adjacent=player_platform::executableDirectory()/"datafile";
            std::error_code error;const auto status=std::filesystem::symlink_status(adjacent,error);
            if(error&&error!=std::errc::no_such_file_or_directory) {
                return reportFailure("Cannot inspect "+adjacent.u8string()+". Supply --data-dir PATH.",interactive);
            }
            if(!error&&std::filesystem::exists(status)) {paths.resourceDirectory=adjacent;}
        }
        const auto data=validateNativeData(paths.resourceDirectory);
        if(!data.ok) {
            return reportFailure(data.message+"\nPlace supported original resources in datafile beside the executable, or supply --data-dir PATH.",interactive);
        }
        if(savesInsideResources(paths)) {
            return reportFailure("Choose --user-dir PROFILE outside the resource directory so original data remains read-only.",interactive);
        }
        // Verification must never initialize or reset a missing save.
        if(verify&&!std::filesystem::is_regular_file(paths.highscorePath)) {
            return reportFailure("Missing profile FAME.HAL. Run --play with the same --data-dir/--user-dir to initialize a writable profile, or use --import-data.",false);
        }
        const auto opened=openNativeSaveProfile(root);
        if(!opened.table) {return reportFailure(std::string(nativeProfileStatusMessage(opened.status)),interactive);}
        const auto writable=preflightNativeProfile(root);
        if(writable!=NativeProfileStatus::ready) {return reportFailure(std::string(nativeProfileStatusMessage(writable)),interactive);}
#ifdef STELLAR7_ENHANCED
        if(!verify&&!rateSpecified) {
            try {gameRate=EnhancedOptionsStore(root/"enhanced.cfg").load().gameRate;}
            catch(const std::exception& error) {std::cerr<<"Enhanced options: "<<error.what()<<'\n';}
        }
#endif
        return entry(paths,*opened.table,verify?NativePlayerAction::verify:NativePlayerAction::play,gameRate);
    } catch(const std::exception& error) {return reportFailure(error.what(),interactive);}
}
} // namespace stellar7
