#include "stellar7/native_player.hpp"
#include "stellar7/cockpit_state.hpp"
#include "stellar7/native_install.hpp"
#include "stellar7/native_game_rate.hpp"
#include "native_player_platform.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
namespace fs=std::filesystem;
using namespace stellar7;
fs::path application;int baseQueries=0,imports=0,pickers=0,entries=0;
NativeProfilePaths captured;NativePlayerAction capturedAction=NativePlayerAction::play;
HighscoreTable capturedTable({},{});
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
void write(const fs::path& path,const std::string& bytes) {std::ofstream file(path,std::ios::binary);file<<bytes;require(bool(file),"fixture write");}
std::string read(const fs::path& path) {std::ifstream file(path,std::ios::binary);require(bool(file),"fixture read");return {std::istreambuf_iterator<char>(file),{}};}
void resources(const fs::path& path) {fs::create_directories(path);write(path/"valid-test-resources", "VALID");}
std::optional<int> launch(std::vector<std::string> args) {
    std::vector<const char*> pointers;for(const auto& arg:args) {pointers.push_back(arg.c_str());}
    return runNativePlayerCommand(static_cast<int>(pointers.size()),pointers.data(),
        [](const NativeProfilePaths& paths,const HighscoreTable& table,NativePlayerAction action,unsigned rate) {
            ++entries;captured=paths;capturedTable=table;capturedAction=action;
            require(rate==defaultNativeGameRate,"default rate changed");return 17;
        });
}
std::vector<std::string> play(const fs::path& profile) {return {"untrusted-argv0.exe","--play","--user-dir",profile.u8string()};}
}
namespace stellar7 {
NativeInstallResult validateNativeData(const fs::path& path) {
    // Routing seam only; real18-file hash validation retains its separate tests.
    return {fs::is_regular_file(path/"valid-test-resources"),"fixture resources unavailable: "+path.u8string()};
}
NativeInstallResult importNativeData(const fs::path&,const fs::path&) {++imports;return {true,"unchanged explicit import route"};}
std::string nativeSupportedDataJson() {return "{}";}
}
namespace stellar7::player_platform {
std::vector<fs::path> commandArguments(int argc,const char* const argv[]) {
    std::vector<fs::path> result;for(int i=1;i<argc;++i) {result.push_back(fs::u8path(argv[i]));}return result;
}
fs::path defaultProfile() {throw std::runtime_error("Tests require explicit user-dir");}
fs::path executableDirectory() {++baseQueries;return application;}
std::optional<fs::path> chooseOriginalFolder() {++pickers;throw std::runtime_error("Automatic picker forbidden");}
void showError(const std::string&) {}
}
namespace {
void run(const fs::path& root) {
    fs::create_directories(root);application=root/fs::u8path(u8"Application \u03a9");fs::create_directory(application);
    const auto external=root/fs::u8path(u8"Direct resources \u03a9");resources(external);
    const auto foreign=root/"Foreign CWD";fs::create_directory(foreign);
    const auto originalCwd=fs::current_path();
    struct RestoreCwd {fs::path path;~RestoreCwd() {std::error_code ignored;fs::current_path(path,ignored);}} restore{originalCwd};
    fs::current_path(foreign);
    const auto directProfile=root/"Direct profile";auto direct=play(directProfile);
    direct.insert(direct.end(),{"--data-dir",external.u8string()});
    require(launch(direct)==17&&captured.resourceDirectory==external&&captured.profileDirectory==directProfile,"explicit direct path/context");
    require(baseQueries==0&&!fs::exists(directProfile/"data")&&!fs::exists(external/"FAME.HAL"),"direct path copied/imported resources or wrote source");
    require(launch({"unused","--play","--data-dir",external.u8string(),"--user-dir",(external/"nested-save").u8string()})==2
        &&!fs::exists(external/"nested-save"),"profile nested inside resources wrote original data");
    const auto saveBefore=read(directProfile/"FAME.HAL");
    require(launch({"unused","--data-dir",external.u8string(),"--user-dir",directProfile.u8string()})==17,"implicit play with leading data-dir");
    resources(application/"datafile");resources(directProfile/"data");
    require(launch(direct)==17&&captured.resourceDirectory==external,"explicit did not override adjacent/profile");
    const auto adjacentProfile=root/"Adjacent profile";resources(adjacentProfile/"data");
    require(launch(play(adjacentProfile))==17&&captured.resourceDirectory==application/"datafile","SDL adjacent base did not override old import/foreign CWD");
    auto badExplicit=play(directProfile);badExplicit.insert(badExplicit.end(),{"--data-dir",(root/"Missing explicit").u8string()});
    const auto calls=entries;require(launch(badExplicit)==2&&entries==calls,"invalid explicit fell back");
    fs::rename(application/"datafile"/"valid-test-resources",application/"datafile"/"bad");
    require(launch(play(adjacentProfile))==2&&entries==calls,"invalid adjacent fell back to valid profile");
    fs::rename(application/"datafile",application/"parked-resources");
    require(launch(play(adjacentProfile))==17&&captured.resourceDirectory==adjacentProfile/"data","old profile/data fallback");
    const auto missingProfile=root/"Missing profile";
    require(launch(play(missingProfile))==2&&!fs::exists(missingProfile),"missing data initialized save/profile");
    auto verify=direct;verify[1]="--verify-install";
    require(launch(verify)==17&&capturedAction==NativePlayerAction::verify&&captured.resourceDirectory==external,"verify explicit path");
    require(read(directProfile/"FAME.HAL")==saveBefore,"existing save changed by selection/verify");
    auto verifyMissing=std::vector<std::string>{"unused","--verify-install","--data-dir",external.u8string(),"--user-dir",missingProfile.u8string()};
    require(launch(verifyMissing)==2&&!fs::exists(missingProfile),"verify initialized missing save");
    HighscoreTable::Entries rows{};rows[0]={12345,"PLAYER"};HighscoreTable custom({},rows);custom.setConfiguration({70,{1,0,0,1,2,0}});
    const auto customBytes=encodeHighscoreTable(custom);write(directProfile/"FAME.HAL",std::string(customBytes.begin(),customBytes.end()));
    require(launch(direct)==17&&capturedTable.entries()[0].score==12345&&capturedTable.header()==custom.header(),"direct data replaced player save/settings");
    write(directProfile/"FAME.HAL","MALFORMED");const auto beforeBad=entries;
    require(launch(direct)==2&&entries==beforeBad&&read(directProfile/"FAME.HAL")=="MALFORMED","malformed existing save reset");
    const auto originalImports=imports;
    require(!launch({"unused","--play",external.u8string(),"palette"}),"legacy --play DIR captured");
    require(launch({"unused","--import-data",external.u8string(),"--user-dir",missingProfile.u8string()})==0&&imports==originalImports+1,"explicit import changed");
    require(launch({"unused","--import-data",external.u8string(),"--data-dir",external.u8string(),"--user-dir",missingProfile.u8string()})==2,"import accepted data-dir");
    auto duplicate=direct;duplicate.insert(duplicate.end(),{"--data-dir",external.u8string()});require(launch(duplicate)==2,"duplicate data-dir accepted");
    require(launch({"unused","--play","--data-dir"})==2,"missing data-dir value accepted");
    require(pickers==0&&imports==originalImports+1,"direct launch invoked picker/import");
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(fs::absolute(fs::path(argv[1]))/("datafile-launch-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())));
        std::cout<<"Direct datafile/path precedence/save isolation/legacy routing PASS\n";return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
