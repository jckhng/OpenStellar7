#pragma once

#include "stellar7/gameplay.hpp"
#include "stellar7/native_install.hpp"

namespace stellar7 {

stellar7::GameplayAssets loadGameplayAssets(
    const std::filesystem::path& dataDirectory,
    const std::string& palettePath = {},
    const std::string& level3SuccessSequencePath = {},
    const std::optional<stellar7::HighscoreTable>& profileTable = {},
    const stellar7::NativeDataFiles* resourceFiles = nullptr
);

} // namespace stellar7
