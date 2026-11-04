#include "stellar7/native_profile.hpp"
#include "stellar7/cockpit_state.hpp"
#include <algorithm>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
using namespace stellar7;
namespace fs=std::filesystem;
HighscoreTable defaultTable() {
    HighscoreTable table({},{});table.setConfiguration({81,{1,0,1,1,0,255}});return table;
}
void require(bool value,const char* expression,int line) {
    if(!value) {throw std::runtime_error("profile test line="+std::to_string(line)+" expression="+expression);}
}
#define REQUIRE(...) require(bool((__VA_ARGS__)),#__VA_ARGS__,__LINE__)
std::vector<std::uint8_t> read(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);REQUIRE(bool(file));return {std::istreambuf_iterator<char>(file),{}};
}
void write(const fs::path& path,const std::vector<std::uint8_t>& bytes) {
    fs::create_directories(path.parent_path());std::ofstream file(path,std::ios::binary|std::ios::trunc);REQUIRE(bool(file));
    file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));file.close();REQUIRE(bool(file));
}
fs::path privateOutput(const fs::path& requested) {
    const auto base=fs::weakly_canonical(fs::absolute(requested));
    fs::create_directories(base);
    constexpr unsigned maximumRuns=64;
    for(unsigned run=0;run<maximumRuns;++run) {const auto output=base/("save-profile-"+std::to_string(run));if(fs::create_directory(output)) {return output;}}
    throw std::runtime_error("private profile test output slots exhausted");
}
class ReadonlyFixture {
public:
    explicit ReadonlyFixture(fs::path path):path_(std::move(path)) {
#ifdef _WIN32
        previous_=GetFileAttributesW(path_.c_str());REQUIRE(previous_!=INVALID_FILE_ATTRIBUTES);
        REQUIRE(SetFileAttributesW(path_.c_str(),previous_|FILE_ATTRIBUTE_READONLY)!=0);
#else
        previous_=fs::status(path_).permissions();fs::permissions(path_,fs::perms::owner_write|fs::perms::group_write|fs::perms::others_write,fs::perm_options::remove);
#endif
    }
    ~ReadonlyFixture() {
#ifdef _WIN32
        SetFileAttributesW(path_.c_str(),previous_);
#else
        std::error_code ignored;fs::permissions(path_,previous_,fs::perm_options::replace,ignored);
#endif
    }
private:
    fs::path path_;
#ifdef _WIN32
    DWORD previous_=0;
#else
    fs::perms previous_=fs::perms::unknown;
#endif
};
std::vector<std::uint8_t> zeroScoreSeed() {
    auto bytes=encodeHighscoreTable(defaultTable());
    constexpr std::uint32_t scoreMask=0xa1b2c3d4U;
    for(unsigned byte=0;byte<sizeof(scoreMask);++byte) {bytes.push_back(static_cast<std::uint8_t>(scoreMask>>(byte*8)));}
    for(const unsigned char character:std::string("ZERO")) {bytes.push_back(character^0x7f);}
    bytes.push_back(0);return bytes;
}
void run(const fs::path& output) {
    const auto root=output/"profile",seed=output/"source"/highscoreFileName;
    const auto seedBytes=zeroScoreSeed();write(seed,seedBytes);
    REQUIRE(decodeHighscoreTable(seedBytes)&&encodeHighscoreTable(*decodeHighscoreTable(seedBytes))!=seedBytes);
    const auto paths=nativeProfilePaths(root);
    REQUIRE(paths.profileDirectory==root&&paths.resourceDirectory==root/"data"&&paths.highscorePath==root/highscoreFileName);
    const auto initialized=openNativeSaveProfile(root,seed);
    REQUIRE(initialized.status==NativeProfileStatus::initialized&&initialized.table&&read(paths.highscorePath)==seedBytes&&read(seed)==seedBytes);
    REQUIRE(!fs::exists(paths.resourceDirectory)); //Importer alone owns immutable data publication.
    REQUIRE(preflightNativeProfile(root)==NativeProfileStatus::ready&&read(paths.highscorePath)==seedBytes);
    const auto stale=root/".fame-write-check-0";fs::create_directory(stale);write(stale/"user-file",{1,2,3});
    REQUIRE(preflightNativeProfile(root)==NativeProfileStatus::ready&&read(stale/"user-file")==std::vector<std::uint8_t>({1,2,3})
        &&!fs::exists(root/".fame-write-check-1")&&read(paths.highscorePath)==seedBytes);
    const auto invalidSeed=output/"bad-seed";write(invalidSeed,{1,2,3});
    const auto retained=openNativeSaveProfile(root,invalidSeed);
    REQUIRE(retained.status==NativeProfileStatus::ready&&retained.table&&read(paths.highscorePath)==seedBytes&&read(invalidSeed)==std::vector<std::uint8_t>({1,2,3}));
    const auto probeOnly=output/"probe-only";
    REQUIRE(preflightNativeProfile(probeOnly)==NativeProfileStatus::ready&&fs::is_empty(probeOnly));
    const auto collisionProfile=output/"init-collision";const auto initStale=collisionProfile/".fame-init-0";
    write(initStale/"user-file",{4,5,6});
    REQUIRE(openNativeSaveProfile(collisionProfile,seed).status==NativeProfileStatus::initialized
        &&read(initStale/"user-file")==std::vector<std::uint8_t>({4,5,6})&&!fs::exists(collisionProfile/".fame-init-1")
        &&read(collisionProfile/highscoreFileName)==seedBytes);
    {
        ReadonlyFixture readonlySeed(seed);const auto readonlySeedProfile=output/"readonly-seed-profile";
        REQUIRE(openNativeSaveProfile(readonlySeedProfile,seed).status==NativeProfileStatus::initialized
            &&preflightNativeProfile(readonlySeedProfile)==NativeProfileStatus::ready&&read(seed)==seedBytes);
    }
    const auto badNew=output/"bad-new";
    REQUIRE(openNativeSaveProfile(badNew,invalidSeed).status==NativeProfileStatus::invalidSeed&&!fs::exists(badNew));
    for(const auto profile:{output/"default",output/"missing-seed"}) {
        const auto fresh=profile.filename()=="default"?openNativeSaveProfile(profile):openNativeSaveProfile(profile,output/"missing-source-fame");
        REQUIRE(fresh.status==NativeProfileStatus::initialized&&fresh.table);
        const auto settings=decodeCockpitSettings(std::vector<std::uint8_t>(fresh.table->header().begin(),fresh.table->header().end()));
        REQUIRE(settings.viewSize==81&&settings.options==std::array<std::int16_t,6>{1,0,1,1,0,255});
        for(const auto& entry:fresh.table->entries()) {REQUIRE(entry.score==0&&entry.name.empty());}
        REQUIRE(read(nativeProfilePaths(profile).highscorePath)==encodeHighscoreTable(*fresh.table));
    }
    const auto corrupt=output/"corrupt";write(corrupt/highscoreFileName,{9,8,7});
    REQUIRE(openNativeSaveProfile(corrupt,seed).status==NativeProfileStatus::invalidExisting
        &&preflightNativeProfile(corrupt)==NativeProfileStatus::invalidExisting&&read(corrupt/highscoreFileName)==std::vector<std::uint8_t>({9,8,7}));
    auto replacement=defaultTable();REQUIRE(replacement.insert(1234,"PLAYER",HighscoreNameMark::ordinary));
    NativeProfileHighscoreStore corruptStore(corrupt);
    REQUIRE(corruptStore.saveAndReload(replacement).status==HighscoreSaveStatus::reloadFailed&&read(corrupt/highscoreFileName)==std::vector<std::uint8_t>({9,8,7}));
    NativeProfileHighscoreStore store(root);
    {
        ReadonlyFixture readonly(paths.highscorePath);
        REQUIRE(preflightNativeProfile(root)==NativeProfileStatus::readonlyExisting);
        REQUIRE(store.saveAndReload(replacement).status==HighscoreSaveStatus::writeFailed&&read(paths.highscorePath)==seedBytes);
    }
#ifdef _WIN32
    const auto locked=CreateFileW(paths.highscorePath.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    REQUIRE(locked!=INVALID_HANDLE_VALUE);
    const auto blockedPreflight=preflightNativeProfile(root);const auto blockedSave=store.saveAndReload(replacement);CloseHandle(locked);
    REQUIRE(blockedPreflight==NativeProfileStatus::writeFailed&&blockedSave.status==HighscoreSaveStatus::writeFailed&&read(paths.highscorePath)==seedBytes);
    const auto unreadable=CreateFileW(paths.highscorePath.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    REQUIRE(unreadable!=INVALID_HANDLE_VALUE);const auto blockedOpen=openNativeSaveProfile(root,seed);CloseHandle(unreadable);
    REQUIRE(blockedOpen.status==NativeProfileStatus::readFailed&&!blockedOpen.table&&read(paths.highscorePath)==seedBytes);
#endif
    auto invalid=replacement;CockpitSettings invalidSettings{81,{1,0,1,1,0,2}};invalid.setConfiguration(invalidSettings);
    REQUIRE(store.saveAndReload(invalid).status==HighscoreSaveStatus::writeFailed&&read(paths.highscorePath)==seedBytes);
    REQUIRE(store.saveAndReload(replacement).status==HighscoreSaveStatus::saved&&read(paths.highscorePath)==encodeHighscoreTable(replacement)&&read(seed)==seedBytes);
    REQUIRE(openNativeSaveProfile(root,invalidSeed).status==NativeProfileStatus::ready&&read(paths.highscorePath)==encodeHighscoreTable(replacement));
    const auto admissionTable=[&](const std::uint32_t score,std::string name) {
        auto entries=defaultTable().entries();entries[0]={score,std::move(name)};
        return HighscoreTable(defaultTable().header(),std::move(entries));
    };
    std::string markedName(1,static_cast<char>('A'|0x80));markedName+=" _";
    const auto marked=admissionTable(123,markedName);const auto markedSeed=output/"marked-seed";
    write(markedSeed,encodeHighscoreTable(marked));const auto markedProfile=output/"marked-profile";
    REQUIRE(openNativeSaveProfile(markedProfile,markedSeed).status==NativeProfileStatus::initialized);
    NativeProfileHighscoreStore markedStore(markedProfile);REQUIRE(markedStore.saveAndReload(marked).status==HighscoreSaveStatus::saved);
    const std::vector<HighscoreTable> unsupported{
        admissionTable(0x80000000U,"NEGATIVE"),admissionTable(123,"LOWERa"),admissionTable(123,"`"),
        admissionTable(123,std::string(1,static_cast<char>(31))),admissionTable(123,std::string(1,static_cast<char>(0x80))),
        admissionTable(123,std::string("A")+static_cast<char>('B'|0x80))};
    for(std::size_t index=0;index<unsupported.size();++index) {
        const auto bytes=encodeHighscoreTable(unsupported[index]);REQUIRE(decodeHighscoreTable(bytes)); //Legacy codec admission unchanged.
        const auto source=output/("unsupported-seed-"+std::to_string(index));write(source,bytes);
        const auto fresh=output/("unsupported-new-"+std::to_string(index));
        REQUIRE(openNativeSaveProfile(fresh,source).status==NativeProfileStatus::invalidSeed&&!fs::exists(fresh)&&read(source)==bytes);
        const auto existing=output/("unsupported-existing-"+std::to_string(index));write(existing/highscoreFileName,bytes);
        REQUIRE(openNativeSaveProfile(existing,seed).status==NativeProfileStatus::invalidExisting
            &&preflightNativeProfile(existing)==NativeProfileStatus::invalidExisting&&read(existing/highscoreFileName)==bytes);
        const auto prior=read(paths.highscorePath);
        REQUIRE(store.saveAndReload(unsupported[index]).status==HighscoreSaveStatus::writeFailed&&read(paths.highscorePath)==prior);
    }
    const auto oversized=output/"oversized";write(oversized/highscoreFileName,std::vector<std::uint8_t>(1024,0));
    REQUIRE(openNativeSaveProfile(oversized,seed).status==NativeProfileStatus::invalidExisting&&read(oversized/highscoreFileName).size()==1024);
    const auto symlinkProfile=output/"symlink";fs::create_directory(symlinkProfile);std::error_code linkError;
    fs::create_symlink(seed,symlinkProfile/highscoreFileName,linkError);
    if(!linkError) {REQUIRE(openNativeSaveProfile(symlinkProfile,seed).status==NativeProfileStatus::invalidExisting&&fs::is_symlink(symlinkProfile/highscoreFileName)&&read(seed)==seedBytes);}
    std::cout<<"symlinkFixture="<<(!linkError?"checked":"unavailable")<<'\n';
    constexpr unsigned raceCases=8;
    for(unsigned index=0;index<raceCases;++index) {
        const auto race=output/("race-"+std::to_string(index));
        auto left=std::async(std::launch::async,[&] {return openNativeSaveProfile(race,seed);});
        auto right=std::async(std::launch::async,[&] {return openNativeSaveProfile(race);});
        const auto a=left.get(),b=right.get();const auto bytes=read(nativeProfilePaths(race).highscorePath);
        REQUIRE(a.table&&b.table&&((a.status==NativeProfileStatus::initialized&&b.status==NativeProfileStatus::ready)
            ||(b.status==NativeProfileStatus::initialized&&a.status==NativeProfileStatus::ready)));
        REQUIRE(bytes==seedBytes||bytes==encodeHighscoreTable(defaultTable()));
    }
    REQUIRE(openNativeSaveProfile({}).status==NativeProfileStatus::invalidRoot&&preflightNativeProfile({})==NativeProfileStatus::invalidRoot);
    for(const auto& entry:fs::directory_iterator(root)) {REQUIRE(entry.path().filename().string().find(".fame-init-")!=0);}
    std::cout<<"Native profile raw seed/default/preservation/invalid-readonly-locked refusal/staged save/owned preflight/concurrent no-replace PASS\n";
}
}
int main(int argc,char** argv) {
    try {if(argc!=2) {throw std::invalid_argument("requires an explicit test output directory");}run(privateOutput(argv[1]));}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
