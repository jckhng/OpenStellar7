#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>

namespace stellar7 {
struct NativeDataRequirement {
    const char* filename;
    std::uintmax_t size;
    const char* sha256;
};
struct NativeInstallResult {
    bool ok;
    std::string message;
};
struct NativeDataFiles {
    NativeDataFiles(NativeInstallResult validation,
        std::map<std::string,std::filesystem::path> paths);
    NativeInstallResult validation;
    const std::filesystem::path& file(std::string_view name) const;
private:
    std::map<std::string,std::filesystem::path> paths_;
};
const std::array<NativeDataRequirement,20>& nativeDataRequirements() noexcept;
NativeInstallResult validateNativeData(const std::filesystem::path& directory);
NativeDataFiles resolveNativeData(const std::filesystem::path& directory);
NativeInstallResult importNativeData(const std::filesystem::path& source,
    const std::filesystem::path& profile);
std::string nativeSupportedDataJson();
}
