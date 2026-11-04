#pragma once
#include "stellar7/native_install.hpp"
#include <vector>

// Private implementation seam: production entry always supplies its fixed18 table.
namespace stellar7::install_detail {
std::string sha256(const std::vector<std::uint8_t>& bytes);
NativeInstallResult validate(const std::filesystem::path&,
    const std::vector<NativeDataRequirement>&);
NativeDataFiles resolve(const std::filesystem::path&,
    const std::vector<NativeDataRequirement>&);
NativeInstallResult import(const std::filesystem::path&,const std::filesystem::path&,
    const std::vector<NativeDataRequirement>&);
}
