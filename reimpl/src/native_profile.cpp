#include "stellar7/native_profile.hpp"
#include "stellar7/cockpit_state.hpp"
#include <fstream>
#include <limits>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace stellar7 {
namespace {
constexpr std::size_t maximumOwnedAttempts=8;
constexpr std::size_t maximumHighscoreBytes=highscoreHeaderBytes
    +highscoreEntryCount*(sizeof(std::uint32_t)+highscoreNameCapacity);
constexpr std::int16_t maximumViewSize=82;
constexpr std::size_t booleanOptionCount=4,difficultyOption=4,storyOption=5;
constexpr std::int16_t maximumDifficulty=2,storyActive=255;
constexpr CockpitSettings nativeDefaults{81,{1,0,1,1,0,storyActive}};

//Native empty-player policy, not recovered proprietary scores/configuration.
HighscoreTable defaultProfileTable() {
    HighscoreTable table({},{});table.setConfiguration(nativeDefaults);return table;
}
enum class ReadStatus {absent,valid,invalid,failed};
struct ProfileRead {ReadStatus status;std::vector<std::uint8_t> bytes;std::optional<HighscoreTable> table;};
bool validConfiguration(const HighscoreTable& table) {
    const auto& header=table.header();
    const auto word=[&](const std::size_t index) {
        const auto offset=index*sizeof(std::uint16_t);
        return static_cast<std::int16_t>(static_cast<std::uint16_t>(header[offset]|(std::uint16_t{header[offset+1]}<<8)));
    };
    const auto view=word(0);if(view<0||view>maximumViewSize) {return false;}
    for(std::size_t option=0;option<booleanOptionCount;++option) {
        const auto value=word(option+1);if(value!=0&&value!=1) {return false;}
    }
    const auto difficulty=word(difficultyOption+1),story=word(storyOption+1);
    return difficulty>=0&&difficulty<=maximumDifficulty&&(story==0||story==storyActive);
}
bool validProfileTable(const HighscoreTable& table) {
    if(!validConfiguration(table)) {return false;}
    constexpr std::uint8_t completionMark=0x80,minimumDisplayCharacter=32,maximumDisplayCharacter=95;
    for(const auto& entry:table.entries()) {
        if(entry.score>static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {return false;}
        for(std::size_t index=0;index<entry.name.size();++index) {
            const auto stored=static_cast<std::uint8_t>(entry.name[index]);
            const auto displayed=static_cast<std::uint8_t>(index==0?stored&static_cast<std::uint8_t>(~completionMark):stored);
            if((index!=0&&(stored&completionMark)!=0)||displayed<minimumDisplayCharacter||displayed>maximumDisplayCharacter) {return false;}
        }
    }
    return true;
}
ProfileRead readProfile(const std::filesystem::path& path) {
    std::error_code error;const auto status=std::filesystem::symlink_status(path,error);
    if(error==std::errc::no_such_file_or_directory||(!error&&!std::filesystem::exists(status))) {return {ReadStatus::absent,{},{}};}
    if(error) {return {ReadStatus::failed,{},{}};}
    if(!std::filesystem::is_regular_file(status)) {return {ReadStatus::invalid,{},{}};}
    std::ifstream input(path,std::ios::binary);if(!input) {return {ReadStatus::failed,{},{}};}
    std::vector<std::uint8_t> bytes(maximumHighscoreBytes+1);
    input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    const auto count=input.gcount();if(input.bad()) {return {ReadStatus::failed,{},{}};}
    bytes.resize(static_cast<std::size_t>(count));
    if(bytes.size()>maximumHighscoreBytes) {return {ReadStatus::invalid,{},{}};}
    auto table=decodeHighscoreTable(bytes);
    if(!table||!validProfileTable(*table)) {return {ReadStatus::invalid,{},{}};}
    return {ReadStatus::valid,std::move(bytes),std::move(table)};
}
NativeProfileOpenResult existingResult(ProfileRead read) {
    if(read.status==ReadStatus::valid) {return {NativeProfileStatus::ready,std::move(read.table)};}
    return {read.status==ReadStatus::invalid?NativeProfileStatus::invalidExisting:NativeProfileStatus::readFailed,std::nullopt};
}
NativeProfileStatus checkExistingWritable(const std::filesystem::path& path) {
#ifdef _WIN32
    const auto attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES) {return NativeProfileStatus::readFailed;}
    if((attributes&FILE_ATTRIBUTE_READONLY)!=0) {return NativeProfileStatus::readonlyExisting;}
    const auto access=CreateFileW(path.c_str(),GENERIC_WRITE|DELETE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(access==INVALID_HANDLE_VALUE) {return NativeProfileStatus::writeFailed;}
    CloseHandle(access); //Access check only: no bytes/length/attributes are changed.
#else
    std::error_code error;const auto permissions=std::filesystem::status(path,error).permissions();
    if(error) {return NativeProfileStatus::readFailed;}
    constexpr auto writable=std::filesystem::perms::owner_write|std::filesystem::perms::group_write|std::filesystem::perms::others_write;
    if((permissions&writable)==std::filesystem::perms::none) {return NativeProfileStatus::readonlyExisting;}
    std::fstream access(path,std::ios::binary|std::ios::in|std::ios::out);
    if(!access) {return NativeProfileStatus::writeFailed;}
#endif
    return NativeProfileStatus::ready;
}
bool createProfileDirectory(const std::filesystem::path& root) {
    std::error_code error;std::filesystem::create_directories(root,error);
    if(error) {return false;}
    return std::filesystem::is_directory(root,error)&&!error;
}
class OwnedProfileTemporary {
public:
    ~OwnedProfileTemporary() {
        if(!owned_) {return;}
        std::error_code ignored;std::filesystem::remove(file_,ignored);std::filesystem::remove(directory_,ignored);
    }
    bool create(const std::filesystem::path& root,const char* label) {
        for(std::size_t attempt=0;attempt<maximumOwnedAttempts;++attempt) {
            directory_=root/(std::string(label)+std::to_string(attempt));
            std::error_code error;owned_=std::filesystem::create_directory(directory_,error);
            if(owned_) {file_=directory_/highscoreFileName;return true;}
            if(error&&error!=std::errc::file_exists) {return false;}
        }
        return false;
    }
    bool write(const std::vector<std::uint8_t>& bytes) {
        std::ofstream output(file_,std::ios::binary|std::ios::trunc);if(!output) {return false;}
        output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));output.close();
        return bool(output);
    }
    bool publish(const std::filesystem::path& target) {
#ifdef _WIN32
        return MoveFileExW(file_.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH)!=0; //No REPLACE_EXISTING.
#else
        std::error_code error;std::filesystem::create_hard_link(file_,target,error);return !error;
#endif
    }
    bool cleanup() {
        if(!owned_) {return true;}
        std::error_code error;std::filesystem::remove(file_,error);if(error) {return false;}
        std::filesystem::remove(directory_,error);if(error) {return false;}
        owned_=false;return true;
    }
    const std::filesystem::path& file() const noexcept {return file_;}
private:
    std::filesystem::path directory_,file_;bool owned_=false;
};
}
NativeProfilePaths nativeProfilePaths(const std::filesystem::path& root) {
    if(root.empty()) {return {};}
    return {root,root/"data",root/highscoreFileName};
}
std::string_view nativeProfileStatusMessage(const NativeProfileStatus status) noexcept {
    switch(status) {
    case NativeProfileStatus::ready:return "Writable player profile is ready.";
    case NativeProfileStatus::initialized:return "Writable player profile was initialized.";
    case NativeProfileStatus::invalidRoot:return "Choose an explicit player profile directory.";
    case NativeProfileStatus::invalidExisting:return "Existing FAME.HAL is invalid; preserve it and restore a valid backup before retrying.";
    case NativeProfileStatus::invalidSeed:return "Source FAME.HAL is invalid; fix or omit that seed before importing.";
    case NativeProfileStatus::readFailed:return "Cannot read FAME.HAL; check file access and retry.";
    case NativeProfileStatus::readonlyExisting:return "Existing FAME.HAL is read-only; use a writable profile or remove its read-only attribute.";
    case NativeProfileStatus::createFailed:return "Cannot create the player profile directory; choose a writable location.";
    case NativeProfileStatus::writeFailed:return "Cannot write verified player data; close file locks and check free space/access, then retry.";
    case NativeProfileStatus::publishFailed:return "Cannot publish initial FAME.HAL; check file locks and retry.";
    case NativeProfileStatus::cleanupFailed:return "Cannot remove the owned profile write-check artifact; close file locks and retry.";
    }
    return "Unknown player profile status.";
}
NativeProfileOpenResult openNativeSaveProfile(const std::filesystem::path& root,
    const std::optional<std::filesystem::path>& seedFamePath) {
    if(root.empty()) {return {NativeProfileStatus::invalidRoot,std::nullopt};}
    const auto paths=nativeProfilePaths(root);auto existing=readProfile(paths.highscorePath);
    if(existing.status!=ReadStatus::absent) {return existingResult(std::move(existing));}
    auto table=defaultProfileTable();auto bytes=encodeHighscoreTable(table);
    if(seedFamePath) {
        auto seed=readProfile(*seedFamePath);
        if(seed.status==ReadStatus::invalid) {return {NativeProfileStatus::invalidSeed,std::nullopt};}
        if(seed.status==ReadStatus::failed) {return {NativeProfileStatus::readFailed,std::nullopt};}
        if(seed.status==ReadStatus::valid) {bytes=std::move(seed.bytes);table=std::move(*seed.table);}
    }
    if(!createProfileDirectory(root)) {return {NativeProfileStatus::createFailed,std::nullopt};}
    OwnedProfileTemporary temporary;
    if(!temporary.create(root,".fame-init-")) {return {NativeProfileStatus::createFailed,std::nullopt};}
    if(!temporary.write(bytes)) {return {NativeProfileStatus::writeFailed,std::nullopt};}
    auto verified=readProfile(temporary.file());
    if(verified.status!=ReadStatus::valid||verified.bytes!=bytes) {return {NativeProfileStatus::writeFailed,std::nullopt};}
    if(!temporary.publish(paths.highscorePath)) {
        auto winner=readProfile(paths.highscorePath);
        if(winner.status!=ReadStatus::absent) {return existingResult(std::move(winner));} //Another initializer wins; never replace it.
        return {NativeProfileStatus::publishFailed,std::nullopt};
    }
    return {NativeProfileStatus::initialized,std::move(verified.table)};
}
NativeProfileStatus preflightNativeProfile(const std::filesystem::path& root) {
    if(root.empty()) {return NativeProfileStatus::invalidRoot;}
    const auto path=nativeProfilePaths(root).highscorePath;const auto existing=readProfile(path);
    if(existing.status==ReadStatus::invalid) {return NativeProfileStatus::invalidExisting;}
    if(existing.status==ReadStatus::failed) {return NativeProfileStatus::readFailed;}
    if(existing.status==ReadStatus::valid) {
        const auto writable=checkExistingWritable(path);if(writable!=NativeProfileStatus::ready) {return writable;}
    }
    if(!createProfileDirectory(root)) {return NativeProfileStatus::createFailed;}
    OwnedProfileTemporary temporary;if(!temporary.create(root,".fame-write-check-")) {return NativeProfileStatus::createFailed;}
    const auto bytes=encodeHighscoreTable(defaultProfileTable());
    if(!temporary.write(bytes)) {return NativeProfileStatus::writeFailed;}
    const auto verified=readProfile(temporary.file());
    if(verified.status!=ReadStatus::valid||verified.bytes!=bytes) {return NativeProfileStatus::writeFailed;}
    return temporary.cleanup()?NativeProfileStatus::ready:NativeProfileStatus::cleanupFailed;
}
NativeProfileHighscoreStore::NativeProfileHighscoreStore(std::filesystem::path root):profileRoot_(std::move(root)) {}
HighscoreSaveResult NativeProfileHighscoreStore::saveAndReload(const HighscoreTable& table) {
    if(profileRoot_.empty()) {return {HighscoreSaveStatus::openFailed,std::nullopt};}
    if(!validProfileTable(table)) {return {HighscoreSaveStatus::writeFailed,std::nullopt};}
    const auto path=nativeProfilePaths(profileRoot_).highscorePath;const auto existing=readProfile(path);
    if(existing.status==ReadStatus::invalid) {return {HighscoreSaveStatus::reloadFailed,std::nullopt};}
    if(existing.status==ReadStatus::failed) {return {HighscoreSaveStatus::openFailed,std::nullopt};}
    if(existing.status==ReadStatus::valid&&checkExistingWritable(path)!=NativeProfileStatus::ready) {
        return {HighscoreSaveStatus::writeFailed,std::nullopt};
    }
    return FileHighscoreStore(path,HighscoreWritePolicy::nativeStagedReplace).saveAndReload(table);
}
} //namespace stellar7
