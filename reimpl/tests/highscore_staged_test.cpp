#include "stellar7/highscore.hpp"
#include "stellar7/cockpit_state.hpp"

#include <fstream>
#include <iostream>
#include <chrono>
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
void require(const bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);require(bool(file),"cannot read private driver fixture");
    return {std::istreambuf_iterator<char>(file),{}};
}
constexpr std::size_t maximumStagingAttempts=8;
std::filesystem::path temporary(const std::filesystem::path& target,const std::size_t slot=0) {
    auto directory=target;directory+=".save-tmp-"+std::to_string(slot);return directory;
}
FileHighscoreStore staged(const std::filesystem::path& path) {
#ifdef STELLAR7_TEST_RECOVERED_DRIVER
    return FileHighscoreStore(path); //Baseline adapter: demonstrate the missing preservation contract.
#else
    return FileHighscoreStore(path,HighscoreWritePolicy::nativeStagedReplace);
#endif
}
HighscoreTable table(const std::uint32_t score,const char* name,const std::int16_t detail) {
    HighscoreTable::Entries entries{};entries[0]={score,name};
    HighscoreTable value({},entries);value.setConfiguration({detail,{0,0,1,1,0,0}});return value;
}
void run(const std::filesystem::path& output) {
    require(!std::filesystem::exists(output),"driver test needs a fresh private output directory");
    std::filesystem::create_directory(output);
    const auto path=output/highscoreFileName;
    const auto first=table(100,"FIRST",81),second=table(200,"SECOND",70),third=table(300,"THIRD",51);
    const auto firstBytes=encodeHighscoreTable(first),secondBytes=encodeHighscoreTable(second),thirdBytes=encodeHighscoreTable(third);
    require(saveHighscoreTable(path,first).status==HighscoreSaveStatus::saved,"cannot seed private recovered file");
    auto driver=staged(path);
    const auto replaced=driver.saveAndReload(second);
    require(replaced.status==HighscoreSaveStatus::saved&&replaced.reloaded
        &&encodeHighscoreTable(*replaced.reloaded)==secondBytes&&read(path)==secondBytes
        &&!std::filesystem::exists(temporary(path)),"existing-target replacement/header reload or owned cleanup failed");
#ifdef _WIN32
    const auto locked=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(locked!=INVALID_HANDLE_VALUE,"cannot acquire replacement-denial fixture");
    HighscoreSaveResult denied{HighscoreSaveStatus::saved,std::nullopt};
    try {denied=driver.saveAndReload(third);}catch(...) {CloseHandle(locked);throw;}
    CloseHandle(locked);
    require(denied.status==HighscoreSaveStatus::writeFailed&&!denied.reloaded&&read(path)==secondBytes
        &&!std::filesystem::exists(temporary(path)),
        "native replacement denial must preserve prior bytes and clean owned staging");
    const auto retry=driver.saveAndReload(third);
    require(retry.status==HighscoreSaveStatus::saved&&retry.reloaded&&read(path)==thirdBytes
        &&!std::filesystem::exists(temporary(path)),"native replacement did not recover after target unlock");
#else
    require(driver.saveAndReload(third).status==HighscoreSaveStatus::saved&&read(path)==thirdBytes,
        "POSIX existing-target rename did not replace exact bytes");
#endif
    const auto collision=temporary(path);
    std::filesystem::create_directory(collision);
    const auto sentinel=collision/"player-file";
    {std::ofstream file(sentinel,std::ios::binary);file<<"PRESERVE";}
    const auto sentinelBytes=read(sentinel);
    const auto nextSlot=driver.saveAndReload(first);
    require(nextSlot.status==HighscoreSaveStatus::saved&&nextSlot.reloaded&&read(path)==firstBytes
        &&read(sentinel)==sentinelBytes&&!std::filesystem::exists(temporary(path,1)),
        "stale staging directory blocked save or was reused/removed");
    std::filesystem::remove(sentinel);std::filesystem::remove(collision); //Only this test's exact fixtures.
    {std::ofstream file(collision,std::ios::binary);file<<"PRESERVE FILE";}
    const auto collisionBytes=read(collision);
    const auto fileCollision=driver.saveAndReload(second);
    require(fileCollision.status==HighscoreSaveStatus::saved&&fileCollision.reloaded
        &&read(collision)==collisionBytes&&read(path)==secondBytes&&!std::filesystem::exists(temporary(path,1)),
        "existing staging file blocked save or was overwritten");
    std::filesystem::remove(collision);

    for(std::size_t slot=0;slot<maximumStagingAttempts;++slot) {
        const auto directory=temporary(path,slot);std::filesystem::create_directory(directory);
        std::ofstream file(directory/"player-file",std::ios::binary);file<<"PRESERVE";
    }
    const auto exhausted=driver.saveAndReload(third);
    require(exhausted.status==HighscoreSaveStatus::openFailed&&!exhausted.reloaded&&read(path)==secondBytes
        &&!std::filesystem::exists(temporary(path,maximumStagingAttempts)),
        "exhausted bounded staging slots modified the prior target or claimed success");
    for(std::size_t slot=0;slot<maximumStagingAttempts;++slot) {
        const auto directory=temporary(path,slot);const auto playerFile=directory/"player-file";
        require(read(playerFile)==sentinelBytes,"exhaustion removed or modified a pre-existing staging artifact");
        std::filesystem::remove(playerFile);std::filesystem::remove(directory);
    }
    auto absent=staged(output/"absent"/highscoreFileName);
    const auto missing=absent.saveAndReload(first);
    require(missing.status==HighscoreSaveStatus::openFailed&&!missing.reloaded
        &&!std::filesystem::exists(output/"absent"),"missing parent was created or misreported");
    const auto freshPath=output/"new.hal";auto fresh=staged(freshPath);
    const auto created=fresh.saveAndReload(first);
    require(created.status==HighscoreSaveStatus::saved&&created.reloaded&&read(freshPath)==firstBytes
        &&!std::filesystem::exists(temporary(freshPath)),"fresh native target/header reload or cleanup failed");

    const auto sourcePath=output/"recovered.hal";
    FileHighscoreStore recovered(sourcePath);
    require(recovered.saveAndReload(second).status==HighscoreSaveStatus::saved&&read(sourcePath)==secondBytes,
        "default FileHighscoreStore recovered behavior changed");
    require(saveHighscoreTable(sourcePath,first).status==HighscoreSaveStatus::saved&&read(sourcePath)==firstBytes,
        "strict saveHighscoreTable behavior changed");
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {
        const auto output=std::filesystem::path(argv[1])/("highscore-staged-"
            +std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        run(output);std::cout<<"Native staged replacement/preservation/collision/cleanup and recovered default PASS\n";}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
