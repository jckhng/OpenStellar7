#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace stellar7 {

std::vector<std::uint8_t> readFile(const std::filesystem::path& path);

} // namespace stellar7
