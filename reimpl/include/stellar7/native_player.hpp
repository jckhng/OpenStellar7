#pragma once

#include "stellar7/native_profile.hpp"
#include <functional>

namespace stellar7 {

enum class NativePlayerAction { play, verify };
using NativePlayerEntry = std::function<int(const NativeProfilePaths&,
    const HighscoreTable&, NativePlayerAction,unsigned nativeGameRate)>;

// The installed player owns discovery/UI; the callback owns decoded game execution.
std::optional<int> runNativePlayerCommand(int argc, const char* const argv[],
    const NativePlayerEntry& entry);

} // namespace stellar7
