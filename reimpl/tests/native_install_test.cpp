#include "stellar7/native_install.hpp"
#include "stellar7/native_profile.hpp"
#include "native_install_detail.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
using namespace stellar7;
namespace fs=std::filesystem;
void require(const bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
std::vector<std::uint8_t> bytes(const std::string& value) {return {value.begin(),value.end()};}
void write(const fs::path& path,const std::string& value) {
    std::ofstream output(path,std::ios::binary);output<<value;require(bool(output),"fixture write failed");
}
std::string read(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);require(bool(input),"fixture read failed");return {std::istreambuf_iterator<char>(input),{}};
}
const std::vector<NativeDataRequirement> fixtures{
    {"ONE.RES",3,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
    {"TWO.RES",0,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"}};
void digestTests() {
    require(install_detail::sha256({})==fixtures[1].sha256,"SHA256 empty vector");
    require(install_detail::sha256(bytes("abc"))==fixtures[0].sha256,"SHA256 abc vector");
    require(install_detail::sha256(bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"))
        =="248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1","SHA256 padding/two-block vector");
    require(install_detail::sha256(std::vector<std::uint8_t>(1000000,'a'))
        =="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0","SHA256 million-a vector");
    const auto& required=nativeDataRequirements();require(required.size()==20,"immutable profile must contain20 files");
    for(const auto& item:required) {
        require(item.size>0&&std::string(item.sha256).size()==64,"immutable requirement metadata");
        require(std::string(item.filename)!="FAME.HAL"&&std::string(item.filename)!="STELLAR7.EXE","mutable/original-only file required");
    }
    require(nativeSupportedDataJson().find("SCENEE.RES")!=std::string::npos,"game over missing from published eligibility");
    require(nativeSupportedDataJson().find("DRAXON.RES")!=std::string::npos,"bridge music missing from published eligibility");
    require(nativeSupportedDataJson().find("SCENED.RES")!=std::string::npos,"ending missing from published eligibility");
}
void run(const fs::path& root) {
    fs::create_directories(root);const auto source=root/"Original source",profile=root/"Native profile";
    fs::create_directory(source);write(source/"one.res","abc");write(source/"Two.Res","");write(source/"UNRELATED.TXT","KEEP");
    require(install_detail::validate(source,fixtures).ok,"case insensitive allowlist/extras");
    const auto resolved=install_detail::resolve(source,fixtures);
    require(resolved.validation.ok
        &&resolved.file("ONE.RES")==source/"one.res"&&resolved.file("two.res")==source/"Two.Res",
        "resolver must retain exact admitted on-disk filename casing");
    require(read(resolved.file("one.res"))=="abc"&&read(resolved.file("TWO.RES")).empty(),"resolved file paths must open without case folding by filesystem");
    bool rejected=false;try {resolved.file("../ONE.RES");}catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"resolver allowed a name outside admitted resources");
    write(source/"one.res","abd");const auto badHash=install_detail::validate(source,fixtures);
    const auto unresolved=install_detail::resolve(source,fixtures);
    rejected=false;try {unresolved.file("ONE.RES");}catch(const std::logic_error&) {rejected=true;}
    require(!unresolved.validation.ok&&rejected,"invalid dataset exposed partial resource paths");
    require(!badHash.ok&&badHash.message.find("SHA256")!=std::string::npos,"same-size hash rejection");
    write(source/"one.res","abcd");require(!install_detail::validate(source,fixtures).ok,"size mismatch");
    write(source/"one.res","abc");fs::rename(source/"Two.Res",source/"NOT-TWO");
    require(!install_detail::validate(source,fixtures).ok,"missing required file");fs::rename(source/"NOT-TWO",source/"Two.Res");
    fs::create_directory(source/"ONE-PLACEHOLDER");fs::rename(source/"one.res",source/"saved-one");fs::rename(source/"ONE-PLACEHOLDER",source/"one.res");
    require(!install_detail::validate(source,fixtures).ok,"required directory rejected");fs::remove(source/"one.res");fs::rename(source/"saved-one",source/"one.res");
#ifndef _WIN32
    write(source/"ONE.RES","abc");
    if(!fs::equivalent(source/"ONE.RES",source/"one.res")) {
        require(!install_detail::validate(source,fixtures).ok,"ambiguous case rejected");
        require(!install_detail::resolve(source,fixtures).validation.ok,"resolver admitted ambiguous casing");fs::remove(source/"ONE.RES");
    }
    fs::rename(source/"one.res",source/"saved-one");fs::create_symlink("saved-one",source/"one.res");
    require(!install_detail::validate(source,fixtures).ok,"required symlink rejected");
    require(!install_detail::resolve(source,fixtures).validation.ok,"resolver admitted resource symlink");fs::remove(source/"one.res");fs::rename(source/"saved-one",source/"one.res");
#endif
    require(!install_detail::import(source,source/"profile",fixtures).ok,"nested source/profile rejection");
    require(!install_detail::import(source,root,fixtures).ok,"containing source/profile rejection");
    const auto imported=install_detail::import(source,profile,fixtures);require(imported.ok,imported.message.c_str());
    const auto paths=nativeProfilePaths(profile);const auto fame=read(paths.highscorePath);
    require(read(source/"one.res")=="abc"&&read(source/"UNRELATED.TXT")=="KEEP","import mutated source");
    require(read(paths.resourceDirectory/"ONE.RES")=="abc"&&!fs::exists(paths.resourceDirectory/"UNRELATED.TXT"),"copy exceeded allowlist");
    require(install_detail::import(source,profile,fixtures).ok&&read(paths.highscorePath)==fame,"idempotent import preserves save");
    write(paths.highscorePath,"MALFORMED");require(!install_detail::import(source,profile,fixtures).ok&&read(paths.highscorePath)=="MALFORMED","malformed existing FAME reset");
    write(paths.highscorePath,fame);
    fs::create_directory(profile/".native-import-lock");write(profile/".native-import-lock"/"foreign","KEEP");
    require(!install_detail::import(source,profile,fixtures).ok&&read(profile/".native-import-lock"/"foreign")=="KEEP","import lock overwritten/removed");
    fs::remove(profile/".native-import-lock"/"foreign");fs::remove(profile/".native-import-lock");
    const auto unmanaged=root/"unmanaged";fs::create_directories(unmanaged/"data");write(unmanaged/"data"/"foreign","KEEP");
    require(!install_detail::import(source,unmanaged,fixtures).ok&&read(unmanaged/"data"/"foreign")=="KEEP","unmanaged data replaced");
    write(paths.resourceDirectory/"ONE.RES","BAD");write(paths.resourceDirectory/"PLAYER-NOTE","KEEP");
    fs::create_directory(profile/".native-data-backup-0");write(profile/".native-data-backup-0"/"foreign","KEEP");
    const auto repaired=install_detail::import(source,profile,fixtures);require(repaired.ok,repaired.message.c_str());
    require(read(profile/".native-data-backup-0"/"foreign")=="KEEP","foreign backup slot changed");
    require(read(profile/".native-data-backup-1"/"data"/"ONE.RES")=="BAD"
        &&read(profile/".native-data-backup-1"/"data"/"PLAYER-NOTE")=="KEEP","prior managed tree not retained");
    require(read(paths.highscorePath)==fame&&install_detail::validate(paths.resourceDirectory,fixtures).ok,"repair invalid/save changed");
    for(unsigned i=0;i<8;++i) {fs::create_directory(profile/(".native-data-stage-"+std::to_string(i)));}
    write(paths.resourceDirectory/"ONE.RES","BAD");require(!install_detail::import(source,profile,fixtures).ok&&read(paths.highscorePath)==fame,"staging exhaustion changed save");
    const auto seeded=root/"seeded";const auto seedBytes=fame+std::string("\xd4\xc3\xb2\xa1\0",5);write(source/"fame.hal",seedBytes);
    require(install_detail::import(source,seeded,fixtures).ok&&read(nativeProfilePaths(seeded).highscorePath)==seedBytes,"case-insensitive byte-exact zero-score seed");
    write(source/"fame.hal","MALFORMED");const auto invalidSeed=root/"invalid seed";
    require(!install_detail::import(source,invalidSeed,fixtures).ok&&!fs::exists(invalidSeed/"data")
        &&!fs::exists(nativeProfilePaths(invalidSeed).highscorePath),"invalid seed published data/save");
    require(install_detail::import(source,seeded,fixtures).ok&&read(nativeProfilePaths(seeded).highscorePath)==seedBytes,"existing save must win over invalid seed");
    // Leave this uniquely owned fixture directory for root review; no recursive deletion.
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {
        digestTests();run(fs::path(argv[1])/("native-install-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())));
        std::cout<<"Native import/digest/eligibility/repair/save-isolation PASS\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
