#pragma once
#include "stellar7/highscore.hpp"
#include <filesystem>
#include <optional>
#include <string_view>

namespace stellar7 {
struct NativeProfilePaths {
    std::filesystem::path profileDirectory;
    std::filesystem::path resourceDirectory;
    std::filesystem::path highscorePath;
};
NativeProfilePaths nativeProfilePaths(const std::filesystem::path& profileRoot);

enum class NativeProfileStatus {
    ready,initialized,invalidRoot,invalidExisting,invalidSeed,readFailed,readonlyExisting,
    createFailed,writeFailed,publishFailed,cleanupFailed
};
struct NativeProfileOpenResult {
    NativeProfileStatus status;
    std::optional<HighscoreTable> table;
};
std::string_view nativeProfileStatusMessage(NativeProfileStatus status) noexcept;
//Existing FAME is validated/preserved; missing seed uses the native empty table.
NativeProfileOpenResult openNativeSaveProfile(const std::filesystem::path& profileRoot,
    const std::optional<std::filesystem::path>& seedFamePath=std::nullopt);
//Only exclusive owned temporary files/directories are written and removed.
NativeProfileStatus preflightNativeProfile(const std::filesystem::path& profileRoot);

class NativeProfileHighscoreStore final:public HighscoreStore {
public:
    explicit NativeProfileHighscoreStore(std::filesystem::path profileRoot);
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override;
private:
    std::filesystem::path profileRoot_;
};
} //namespace stellar7
