#pragma once

#include "stellar7/native_player.hpp"

namespace stellar7 {

int runInstalledPlayer(const NativeProfilePaths& paths, const HighscoreTable& table,
    NativePlayerAction action, unsigned gameRate);

} // namespace stellar7
