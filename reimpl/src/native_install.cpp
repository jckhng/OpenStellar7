#include "stellar7/native_install.hpp"
#include "stellar7/native_profile.hpp"
#include "native_install_detail.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace stellar7 {
namespace {
constexpr std::size_t maximumAttempts=8;
constexpr std::uintmax_t maximumResourceBytes=1024*1024;
constexpr char markerName[]=".stellar7-native-data";
constexpr char markerText[]="stellar7-native-data-v1\nreleaseMenus15-supported-dos-resources\n";
constexpr char lockName[]=".native-import-lock";
const std::array<NativeDataRequirement,20> requirements{{
    {"COCKPIT.RES",115411,"3c17beaa6fdde8498e00bb3b47ce7a87e860b4cd4405726329c3d6ca254283e3"},
    {"STELLAR.RES",69783,"fea419b8da7f3e96d5ed56495552d4740e8a2b598f1ed5c3c66b5e24a7b8e278"},
    {"STELART.RES",93605,"e19bde1ee7707614882982d92b91c81ef271e0092242c7507cd64b3d2821e125"},
    {"LEVEL1.RES",23130,"2d4ac1489706d32d011336244c55d63c8a8fd42869ded3848c038fde62f0214d"},
    {"LEVEL2.RES",32146,"3200c4649302e3f83354b2407416566b5b0d77e23e07c191ef8586b2ab97c1ec"},
    {"LEVEL3.RES",28912,"c7e0d8875b0656bbd7d8402f1b07adc9ef0fea5ef9078a374c6a266757cb4ec0"},
    {"LEVEL4.RES",30895,"31fc6a138da3c6d0d100c6ac9154ab42c4970307dc0d7146307468bc14386420"},
    {"LEVEL5.RES",13514,"ed2715a0414b3aacb0472587d6dca35ad6c5e44a59c889e0715085243dda9e9f"},
    {"LEVEL6.RES",18499,"a962d42a19a617ef224203e1c6147ff25e75466a0a3529c2ef1085fb3b22aa32"},
    {"LEVEL7.RES",11104,"b5d9c05bc1e1824a202b25bcab04048c51c6489b97cbcedd545747484a3dc2cb"},
    {"SCENEA.RES",47423,"250c12cbf9c2c3a873647e07664fe3d9e8760ad5905fa809408830bd1c5bdcdc"},
    {"SCENEB1.RES",82250,"235f5569bc31d5f25a8c82af4c315a4e31b8f1d8ee161328be98fd06cfeb4055"},
    {"SCENEB2.RES",1679,"85deee27b72272f1e66a36be18b784a0759ecf67ce7c38784fc13cb9d036417d"},
    {"SCENEB3.RES",1857,"0f4459b07d1e1f6bc2c53ba48ceca16103afa57f42de54047361743b7cda9798"},
    {"SCENEB5.RES",2882,"589ae39d254bd333b7ebd79277a4c8ca2b87cec25b020bd0ddb25ba16d9beff1"},
    {"SCENEC.RES",35438,"474435daace30d0dea3b80338e75b9add121c78c5eb4220b606bf878f2514433"},
    {"SCENED.RES",39107,"9161cb3df0b005b10db95bc446bba9bef3d8309e2f1200c5dacf928b8ab1e3b7"},
    {"SCENEE.RES",45860,"127e0282d31e8317e5dd5274eb113c7a4dd0e4b34cb398a65b8e3c5a8ea01985"},
    {"DRAXON.RES",443,"fabb8993559f051ec0929a45644de0495efed9da3e58ec8ce5942fbf38ad7240"},
    {"VOICE.RES",332264,"74fa40dc17ad06bc4bcee001fd15f4b6d522557369478bf80e0900297175febc"},
}};

NativeInstallResult fail(const std::filesystem::path& path,const std::string& reason) {
    return {false,path.u8string()+": "+reason};
}
std::string folded(std::string value) {
    for(auto& c:value) {if(c>='a'&&c<='z') {c=static_cast<char>(c-'a'+'A');}}
    return value;
}
bool plainFile(const std::filesystem::path& path) {
    std::error_code error;const auto status=std::filesystem::symlink_status(path,error);
    if(error||!std::filesystem::is_regular_file(status)) {return false;}
#ifdef _WIN32
    const auto attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT)!=0) {return false;}
#endif
    return true;
}
bool plainDirectory(const std::filesystem::path& path) {
    std::error_code error;const auto status=std::filesystem::symlink_status(path,error);
    if(error||!std::filesystem::is_directory(status)) {return false;}
#ifdef _WIN32
    const auto attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT)!=0) {return false;}
#endif
    return true;
}
bool boundedRead(const std::filesystem::path& path,const std::uintmax_t expected,
    std::vector<std::uint8_t>& bytes) {
    if(expected>maximumResourceBytes||!plainFile(path)) {return false;}
    std::ifstream input(path,std::ios::binary);if(!input) {return false;}
    bytes.resize(static_cast<std::size_t>(expected));
    if(expected!=0) {input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(expected));}
    if(!input||input.peek()!=std::char_traits<char>::eof()||input.bad()) {return false;}
    return true;
}
struct ValidatedFiles {
    NativeInstallResult result{true,{}};
    std::vector<std::vector<std::uint8_t>> bytes;
    std::vector<std::filesystem::path> paths;
};
ValidatedFiles readData(const std::filesystem::path& root,const std::vector<NativeDataRequirement>& needed) {
    ValidatedFiles result;result.paths.resize(needed.size());auto& selected=result.paths;
    if(!plainDirectory(root)) {result.result=fail(root,"data must be a regular directory, not a link/reparse point");return result;}
    std::error_code error;std::filesystem::directory_iterator iterator(root,error),end;
    if(error) {result.result=fail(root,"cannot read data directory; choose an accessible original installation");return result;}
    for(;iterator!=end;iterator.increment(error)) {
        if(error) {break;}
        const auto name=folded(iterator->path().filename().u8string());
        for(std::size_t i=0;i<needed.size();++i) {
            if(name!=folded(needed[i].filename)) {continue;}
            if(!selected[i].empty()) {result.result=fail(root,"ambiguous filename casing for "+name);return result;}
            selected[i]=iterator->path();
        }
    }
    if(error) {result.result=fail(root,"directory enumeration failed");return result;}
    result.bytes.resize(needed.size());
    for(std::size_t i=0;i<needed.size();++i) {
        const auto& item=needed[i];const auto expected=root/item.filename;
        if(selected[i].empty()) {result.result=fail(expected,"missing required original file");return result;}
        if(!plainFile(selected[i])) {result.result=fail(selected[i],"must be a regular file, not a link/reparse point");return result;}
        const auto size=std::filesystem::file_size(selected[i],error);
        if(error) {result.result=fail(selected[i],"cannot read file size");return result;}
        if(size!=item.size) {result.result=fail(selected[i],"unsupported size; use the supported original data fingerprint");return result;}
        if(!boundedRead(selected[i],item.size,result.bytes[i])) {result.result=fail(selected[i],"cannot read exact bounded file contents");return result;}
        if(install_detail::sha256(result.bytes[i])!=item.sha256) {
            result.result=fail(selected[i],"unsupported SHA256; original data is not the supported fingerprint");return result;
        }
    }
    return result;
}
bool overlaps(const std::filesystem::path& first,const std::filesystem::path& second) {
    auto a=first.begin(),b=second.begin();
    for(;a!=first.end()&&b!=second.end();++a,++b) {
#ifdef _WIN32
        const auto left=a->native(),right=b->native();
        if(CompareStringOrdinal(left.c_str(),static_cast<int>(left.size()),right.c_str(),static_cast<int>(right.size()),TRUE)!=CSTR_EQUAL) {return false;}
#else
        if(*a!=*b) {return false;}
#endif
    }
    return true;
}
bool renameAbsent(const std::filesystem::path& from,const std::filesystem::path& to) {
#ifdef _WIN32
    return MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_WRITE_THROUGH)!=0;
#else
    // Cooperative importers hold the profile lock; nonempty directory targets
    // also cannot be replaced by POSIX rename. Never intentionally replace one.
    std::error_code error;if(std::filesystem::exists(to,error)||error) {return false;}
    std::filesystem::rename(from,to,error);return !error;
#endif
}
class OwnedDirectory {
public:
    ~OwnedDirectory() {
        if(!owned_) {return;}
        std::error_code ignored;
        for(const auto& file:files_) {std::filesystem::remove(file,ignored);}
        std::filesystem::remove(path_,ignored);
    }
    bool exact(const std::filesystem::path& path) {
        std::error_code error;owned_=std::filesystem::create_directory(path,error);path_=path;return owned_&&!error;
    }
    bool temporary(const std::filesystem::path& root,const char* label) {
        for(std::size_t i=0;i<maximumAttempts;++i) {if(exact(root/(std::string(label)+std::to_string(i)))) {return true;}}
        return false;
    }
    bool write(const std::string& name,const std::vector<std::uint8_t>& bytes) {
        const auto file=path_/name;files_.push_back(file);
        std::ofstream output(file,std::ios::binary|std::ios::trunc);if(!output) {return false;}
        output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));output.close();return bool(output);
    }
    const std::filesystem::path& path() const {return path_;}
    void release() {owned_=false;}
private:
    std::filesystem::path path_;std::vector<std::filesystem::path> files_;bool owned_=false;
};
bool managed(const std::filesystem::path& data) {
    std::vector<std::uint8_t> bytes;
    return boundedRead(data/markerName,sizeof(markerText)-1,bytes)
        &&std::string(bytes.begin(),bytes.end())==markerText;
}
NativeInstallResult seedSave(const std::filesystem::path& source,const std::filesystem::path& profile) {
    std::optional<std::filesystem::path> seed;
    std::error_code error;
    if(!std::filesystem::exists(nativeProfilePaths(profile).highscorePath,error)&&!error) {
        std::filesystem::directory_iterator iterator(source,error),end;
        for(;!error&&iterator!=end;iterator.increment(error)) {
            if(folded(iterator->path().filename().u8string())!=highscoreFileName) {continue;}
            if(seed) {return fail(source,"ambiguous FAME.HAL casing; select one source save");}
            seed=iterator->path();
        }
        if(error) {return fail(source,"cannot inspect optional FAME.HAL seed");}
    }
    const auto result=openNativeSaveProfile(profile,seed);
    if(result.status!=NativeProfileStatus::ready&&result.status!=NativeProfileStatus::initialized) {
        return fail(nativeProfilePaths(profile).highscorePath,std::string(nativeProfileStatusMessage(result.status)));
    }
    return {true,{}};
}
}

namespace install_detail {
std::string sha256(const std::vector<std::uint8_t>& input) {
    constexpr std::array<std::uint32_t,64> k{{
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2}};
    std::array<std::uint32_t,8> h{{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
    const auto rotate=[](const std::uint32_t x,const unsigned n) {return (x>>n)|(x<<(32-n));};
    auto bytes=input;bytes.push_back(0x80);while(bytes.size()%64!=56) {bytes.push_back(0);}
    const auto bits=static_cast<std::uint64_t>(input.size())*8;
    for(int i=7;i>=0;--i) {bytes.push_back(static_cast<std::uint8_t>(bits>>(i*8)));}
    for(std::size_t offset=0;offset<bytes.size();offset+=64) {
        std::array<std::uint32_t,64> w{};
        for(std::size_t i=0;i<16;++i) {for(std::size_t j=0;j<4;++j) {w[i]=(w[i]<<8)|bytes[offset+i*4+j];}}
        for(std::size_t i=16;i<w.size();++i) {
            const auto a=w[i-15],b=w[i-2];
            w[i]=w[i-16]+(rotate(a,7)^rotate(a,18)^(a>>3))+w[i-7]+(rotate(b,17)^rotate(b,19)^(b>>10));
        }
        auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
        for(std::size_t i=0;i<w.size();++i) {
            const auto t1=v+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+k[i]+w[i];
            const auto t2=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));
            v=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
    }
    std::ostringstream output;output<<std::hex<<std::setfill('0');for(const auto word:h) {output<<std::setw(8)<<word;}return output.str();
}
NativeInstallResult validate(const std::filesystem::path& directory,const std::vector<NativeDataRequirement>& needed) {
    return readData(directory,needed).result;
}
NativeDataFiles resolve(const std::filesystem::path& directory,const std::vector<NativeDataRequirement>& needed) {
    auto validated=readData(directory,needed);std::map<std::string,std::filesystem::path> paths;
    if(validated.result.ok) {
        for(std::size_t i=0;i<needed.size();++i) {paths.emplace(folded(needed[i].filename),std::move(validated.paths[i]));}
    }
    return {std::move(validated.result),std::move(paths)};
}
NativeInstallResult import(const std::filesystem::path& source,const std::filesystem::path& profile,
    const std::vector<NativeDataRequirement>& needed) {
    if(source.empty()||profile.empty()) {return {false,"Choose both an original data directory and a separate writable profile"};}
    std::error_code error;const auto sourceRoot=std::filesystem::canonical(source,error);
    if(error) {return fail(source,"cannot resolve original data directory");}
    const auto profileRoot=std::filesystem::weakly_canonical(std::filesystem::absolute(profile),error);
    if(error||overlaps(sourceRoot,profileRoot)) {return fail(profile,"profile and original installation must be disjoint directories");}
    auto validated=readData(sourceRoot,needed);if(!validated.result.ok) {return validated.result;}
    std::filesystem::create_directories(profileRoot,error);if(error) {return fail(profileRoot,"cannot create writable profile directory");}
    OwnedDirectory lock;if(!lock.exact(profileRoot/lockName)) {return fail(profileRoot/lockName,"import already active or stale lock present; inspect before retrying");}
    const auto data=nativeProfilePaths(profileRoot).resourceDirectory;
    const bool exists=std::filesystem::exists(data,error);if(error) {return fail(data,"cannot inspect installed data");}
    if(exists&&!plainDirectory(data)) {return fail(data,"installed data must be a regular directory, not a link/reparse point");}
    if(exists&&validate(data,needed).ok) {
        const auto saved=seedSave(sourceRoot,profileRoot);if(!saved.ok) {return saved;}
        return {true,"Supported data already installed; existing FAME.HAL preserved"};
    }
    if(exists&&!managed(data)) {return fail(data,"invalid unmanaged data; choose a new profile (existing files preserved)");}
    OwnedDirectory staging;if(!staging.temporary(profileRoot,".native-data-stage-")) {return fail(profileRoot,"cannot reserve import staging directory");}
    for(std::size_t i=0;i<needed.size();++i) {
        if(!staging.write(needed[i].filename,validated.bytes[i])) {return fail(staging.path()/needed[i].filename,"cannot write staged original file");}
    }
    if(!staging.write(markerName,std::vector<std::uint8_t>(markerText,markerText+sizeof(markerText)-1))) {return fail(staging.path(),"cannot write managed-data marker");}
    const auto verified=validate(staging.path(),needed);if(!verified.ok) {return verified;}
    const auto saved=seedSave(sourceRoot,profileRoot);if(!saved.ok) {return saved;}
    std::filesystem::path backup;OwnedDirectory backupDirectory;
    if(exists) {
        if(!backupDirectory.temporary(profileRoot,".native-data-backup-")) {return fail(data,"cannot reserve a prior-data backup directory");}
        backup=backupDirectory.path()/"data";
        if(!renameAbsent(data,backup)) {return fail(data,"cannot retain prior managed data backup; close users of this directory and retry");}
        backupDirectory.release(); //Never remove a directory containing prior player files.
    }
    if(!renameAbsent(staging.path(),data)) {
        if(!backup.empty()) {renameAbsent(backup,data);}
        return fail(data,"cannot publish validated data; prior data retained at target or .native-data-backup-N/data; inspect before retrying");
    }
    staging.release();
    return {true,backup.empty()?"Imported supported original data; FAME.HAL initialized or preserved"
        :"Repaired supported data; retained prior files at "+backup.u8string()+"; FAME.HAL preserved"};
}
}
const std::array<NativeDataRequirement,20>& nativeDataRequirements() noexcept {return requirements;}
NativeDataFiles::NativeDataFiles(NativeInstallResult result,std::map<std::string,std::filesystem::path> paths)
    :validation(std::move(result)),paths_(std::move(paths)) {}
const std::filesystem::path& NativeDataFiles::file(const std::string_view name) const {
    if(!validation.ok) {throw std::logic_error("Native resource paths require successful data validation");}
    const auto found=paths_.find(folded(std::string(name)));
    if(found==paths_.end()) {throw std::invalid_argument("Resource is outside the admitted native dataset: "+std::string(name));}
    return found->second;
}
NativeDataFiles resolveNativeData(const std::filesystem::path& directory) {
    return install_detail::resolve(directory,{requirements.begin(),requirements.end()});
}
NativeInstallResult validateNativeData(const std::filesystem::path& directory) {
    return install_detail::validate(directory,{requirements.begin(),requirements.end()});
}
NativeInstallResult importNativeData(const std::filesystem::path& source,const std::filesystem::path& profile) {
    return install_detail::import(source,profile,{requirements.begin(),requirements.end()});
}
std::string nativeSupportedDataJson() {
    std::ostringstream output;output<<"{\"profile\":\"releaseMenus15-supported-dos-resources\",\"version\":\"unidentified DOS release\",\"files\":[";
    bool first=true;for(const auto& item:requirements) {
        if(!first) {output<<',';}first=false;
        output<<"{\"filename\":\""<<item.filename<<"\",\"size\":"<<item.size<<",\"sha256\":\""<<item.sha256<<"\"}";
    }
    output<<"],\"save\":\"FAME.HAL is mutable; no required hash\"}";return output.str();
}
}

