#pragma once
#include "stellar7/native_menu_features.hpp"

#include "stellar7/gameplay.hpp"
#include "stellar7/frame_copy_work.hpp"
#include "stellar7/native_startup.hpp"

#include <string>
#include <filesystem>
#include <memory>
#include <vector>

namespace stellar7 {

struct GameplayRunOptions {
    std::string inputRecordingPath;
    bool startAtLevel2 = false;
    bool startAtLevel3 = false;
    std::shared_ptr<HighscoreStore> highscoreStore;
    NativeMenuFeatures menuFeatures = NativeMenuFeatures::legacy;
    unsigned nativeGameRate = 0;
    std::filesystem::path nativeOptionsPath;
};

int runGameplay(
    const GameplayAssets& assets,
    const GameplayRunOptions& options = {}
);
int runGameplay(
    const GameplayAssets& assets,
    GameplayModel initialModel,
    const GameplayRunOptions& options
);

// Explicit native intro-skip route; original graphical menu/input timing is unqualified.
int runNativeEntry(const GameplayAssets& assets, const NativeStartupAssets& startup,
    const GameplayRunOptions& options = {});

// Recovered work-clock entry; native keyboard default Begin and ordinary Level1.
int runRecoveredNativeEntry(const GameplayAssets& assets, const NativeStartupAssets& startup,
    FrameCopyProfile copyProfile, const GameplayRunOptions& options = {});

int runPlayableNativeEntry(const GameplayAssets& assets, const NativeStartupAssets& startup,
    FrameCopyProfile copyProfile, const GameplayRunOptions& options = {});

int runSdlPlayableNativeEntry(const GameplayAssets& assets,const NativeStartupAssets& startup,
    FrameCopyProfile copyProfile,const GameplayRunOptions& options = {});

inline constexpr unsigned maximumGameplaySmokeFrames = 120;
inline constexpr unsigned maximumNativeEntrySmokeFrames = 10000;

struct GameplaySmokePhase {
    unsigned frame;
    GameplayPhase phase;
    std::uint64_t sampleEnd;
    std::size_t oplWriteEnd;
    std::uint64_t gdiBlits;
    IndexedScreen indexed;
};

struct GameplaySmokeResult {
    bool cancelled = true;
    unsigned frames = 0;
    unsigned retainedLevel1Frames = 0;
    std::uint64_t gdiBlits = 0;
    std::uint64_t generatedSamples = 0;
    std::uint64_t queuedSamples = 0;
    std::uint64_t completedSamples = 0;
    std::size_t audioEvents = 0;
    std::size_t oplWrites = 0;
    unsigned startupBeginRequests = 0;
    std::vector<GameplaySmokePhase> phases;
    IndexedScreen finalFrame;
};

GameplaySmokeResult runGameplaySmoke(
    const GameplayAssets& assets,
    const std::vector<GameplayInput>& inputs
);

GameplaySmokeResult runNativeEntrySmoke(const GameplayAssets& assets,
    const NativeStartupAssets& startup, const std::vector<GameplayInput>& inputs);

}  // namespace stellar7
