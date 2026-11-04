#include "enhanced_options.hpp"
#include "stellar7/native_game_rate.hpp"
#include <charconv>
#include <fstream>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace stellar7 {
namespace {
constexpr std::uintmax_t maximumSettingsBytes=16384;
constexpr char currentOptionsFormat[]="5";
constexpr unsigned maximumSaveAttempts=8;
std::string trim(const std::string& text) {
    const auto begin=text.find_first_not_of(" \t\r");
    if(begin==std::string::npos) {return {};}
    return text.substr(begin,text.find_last_not_of(" \t\r")-begin+1);
}
int number(const std::string& text,int minimum,int maximum) {
    int result=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result);
    if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||result<minimum||result>maximum) {
        throw std::invalid_argument("Enhanced option value outside supported range: "+text);
    }
    return result;
}
void plainFile(const std::filesystem::path& path) {
    std::error_code error;const auto status=std::filesystem::symlink_status(path,error);
    if(error==std::errc::no_such_file_or_directory||status.type()==std::filesystem::file_type::not_found) {return;}
    if(error||!std::filesystem::is_regular_file(status)) {throw std::runtime_error("Enhanced options must be a regular file: "+path.u8string());}
#ifdef _WIN32
    const auto attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT)!=0) {throw std::runtime_error("Enhanced options cannot be a reparse point");}
#endif
}
}
EnhancedOptions EnhancedOptionsStore::load() const {
    EnhancedOptions result;plainFile(path_);
    std::error_code error;if(!std::filesystem::exists(path_,error)&&!error) {return result;}
    if(error||std::filesystem::file_size(path_,error)>maximumSettingsBytes||error) {throw std::runtime_error("Cannot read bounded enhanced options");}
    std::ifstream input(path_);if(!input) {throw std::runtime_error("Cannot open enhanced options");}
    std::set<std::string> seen;std::string line;
    while(std::getline(input,line)) {
        line=trim(line);if(line.empty()||line.front()=='#') {continue;}
        const auto equal=line.find('=');if(equal==std::string::npos) {throw std::invalid_argument("Enhanced options require name=value");}
        const auto key=trim(line.substr(0,equal)),value=trim(line.substr(equal+1));
        if(!seen.insert(key).second) {throw std::invalid_argument("Duplicate enhanced option: "+key);}
        if(key=="format") {if(value!="1"&&value!="2"&&value!="3"&&value!="4"&&value!=currentOptionsFormat) {throw std::invalid_argument("Unsupported enhanced options format");}}
        else if(key=="fullscreen") {result.fullscreen=number(value,0,1)!=0;}
        else if(key=="aspect") {result.aspect=static_cast<EnhancedAspect>(number(value,0,1));}
        else if(key=="integer_scale") {result.scaling=static_cast<EnhancedScaling>(number(value,0,1));}
        else if(key=="smooth") {result.filter=static_cast<EnhancedFilter>(number(value,0,1));}
        else if(key=="controller") {result.controller=static_cast<EnhancedController>(number(value,0,2));}
        else if(key=="width") {result.width=number(value,320,7680);}
        else if(key=="height") {result.height=number(value,200,4320);}
        else if(key=="deadzone") {result.deadzone=number(value,5,95);}
        else if(key=="volume") {result.volume=number(value,0,100);}
        else if(key=="muted") {result.muted=number(value,0,1)!=0;}
        else if(key=="chase_distance") {result.camera.distance=number(value,ChaseOptions::minimumDistance,ChaseOptions::maximumDistance);}
        else if(key=="chase_height") {result.camera.height=number(value,ChaseOptions::minimumHeight,ChaseOptions::maximumHeight);}
        else if(key=="cockpit_style") {result.scene.cockpit=static_cast<CockpitStyle>(number(value,0,1));}
        else if(key=="ground_detail") {result.scene.groundDetail=number(value,0,1)!=0;}
        else if(key=="local_lights") {result.scene.localLights=number(value,0,1)!=0;}
        else if(key=="render_scale") {
            result.renderScale=static_cast<unsigned>(number(value,1,4));
            if(result.renderScale!=1&&result.renderScale!=2&&result.renderScale!=4) {throw std::invalid_argument("Unsupported world resolution");}
        }
        else if(key=="presentation_rate") {
            result.presentationRate=static_cast<unsigned>(number(value,0,120));
            if(result.presentationRate!=0&&result.presentationRate!=60&&result.presentationRate!=120) {throw std::invalid_argument("Unsupported display rate");}
        }
        else if(key=="game_rate") {
            result.gameRate=static_cast<unsigned>(number(value,0,70));
            if(!validNativeGameRate(result.gameRate)) {throw std::invalid_argument("Unsupported enhanced game rate");}
        } else {
            bool found=false;
            for(std::size_t index=0;index<enhancedActionCount;++index) {
                const auto name=std::string(enhancedActionNames[index]);
                if(key=="key."+name) {result.bindings[index].key=value;found=true;break;}
                if(key=="pad."+name) {result.bindings[index].button=value;found=true;break;}
            }
            if(!found) {throw std::invalid_argument("Unknown enhanced option: "+key);}
        }
    }
    if(!input.eof()) {throw std::runtime_error("Incomplete enhanced options read");}
    return result;
}
void EnhancedOptionsStore::save(const EnhancedOptions& options) const {
    if(path_.empty()) {return;}
    plainFile(path_);
    struct Stage {
        std::filesystem::path directory,file;
        ~Stage() {std::error_code ignored;if(!file.empty()) {std::filesystem::remove(file,ignored);}if(!directory.empty()) {std::filesystem::remove(directory,ignored);}}
    } stage;
    for(unsigned attempt=0;attempt<maximumSaveAttempts;++attempt) {
        const auto directory=path_.parent_path()/(".enhanced-save-"+std::to_string(attempt));
        std::error_code error;
        if(std::filesystem::create_directory(directory,error)) {stage.directory=directory;stage.file=directory/"options";break;}
        if(error&&error!=std::errc::file_exists) {throw std::runtime_error("Cannot stage enhanced options");}
    }
    if(stage.file.empty()) {throw std::runtime_error("No free enhanced options staging slot");}
    std::ofstream output(stage.file,std::ios::binary);
    output<<"# Stellar 7 Enhanced native options; original FAME.HAL is separate.\nformat="<<currentOptionsFormat<<'\n'
        <<"fullscreen="<<options.fullscreen<<"\naspect="<<static_cast<int>(options.aspect)
        <<"\ninteger_scale="<<static_cast<int>(options.scaling)<<"\nsmooth="<<static_cast<int>(options.filter)
        <<"\ncontroller="<<static_cast<int>(options.controller)<<"\nwidth="<<options.width<<"\nheight="<<options.height
        <<"\ndeadzone="<<options.deadzone<<"\nvolume="<<options.volume<<"\nmuted="<<options.muted
        <<"\ngame_rate="<<options.gameRate<<"\nrender_scale="<<options.renderScale
        <<"\npresentation_rate="<<options.presentationRate
        <<"\nchase_distance="<<options.camera.distance<<"\nchase_height="<<options.camera.height
        <<"\ncockpit_style="<<static_cast<int>(options.scene.cockpit)<<"\nground_detail="<<options.scene.groundDetail
        <<"\nlocal_lights="<<options.scene.localLights<<'\n';
    for(std::size_t index=0;index<enhancedActionCount;++index) {
        output<<"key."<<enhancedActionNames[index]<<'='<<options.bindings[index].key<<'\n'
            <<"pad."<<enhancedActionNames[index]<<'='<<options.bindings[index].button<<'\n';
    }
    output.close();if(!output) {throw std::runtime_error("Cannot write enhanced options");}
#ifdef _WIN32
    if(!MoveFileExW(stage.file.c_str(),path_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {throw std::runtime_error("Cannot publish enhanced options");}
#else
    std::error_code error;std::filesystem::rename(stage.file,path_,error);
    if(error) {throw std::runtime_error("Cannot publish enhanced options: "+error.message());}
#endif
}
} //namespace stellar7
