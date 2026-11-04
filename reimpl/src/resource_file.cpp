#include "resource_file.hpp"
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace stellar7 {

std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open input file: " + path.u8string());
    }
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>(),
    };
}

} // namespace stellar7
