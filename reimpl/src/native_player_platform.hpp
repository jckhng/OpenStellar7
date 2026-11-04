#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace stellar7::player_platform {
std::vector<std::filesystem::path> commandArguments(int argc,const char* const argv[]);
std::filesystem::path defaultProfile();
std::filesystem::path executableDirectory();
std::optional<std::filesystem::path> chooseOriginalFolder();
void showError(const std::string& message);
}
