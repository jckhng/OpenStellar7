#include "stellar7/frontend.hpp"

#include "stellar7/gameplay.hpp"
#include "stellar7/input_movie.hpp"
#include "stellar7/native_session.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace stellar7 {

int runGameplay(
    const GameplayAssets& assets,
    const GameplayRunOptions& options
) {
    GameplayModel model;
    if (options.startAtLevel3) {
        model.startLevel3();
    } else if (options.startAtLevel2) {
        model.startLevel2();
    }
    return runGameplay(assets, std::move(model), options);
}

int runGameplay(
    const GameplayAssets& assets,
    GameplayModel initialModel,
    const GameplayRunOptions& options
) {
    NativeGameplaySession session(assets, std::move(initialModel), options.startAtLevel3 ? NativeSessionStart::level3
        : options.startAtLevel2 ? NativeSessionStart::level2 : NativeSessionStart::level1,
        std::unique_ptr<NativeAudioOwner>{}, options.highscoreStore);
    session.initialize();
    const auto& model = session.model();
    GameplayInputRecorder inputRecorder;
    std::string command;
    std::cout
        << "Portable gameplay controls: a=left d=right w=forward "
           "s=reverse f=fire q=quit\n"
        << "Continue prompt: w/s toggle, Enter confirms, q declines.\n";
    while (true) {
        if (model.phase() == GameplayPhase::campaignEnded) {
            break;
        }
        const bool continuePrompt = model.phase() == GameplayPhase::continuePrompt;
        const bool nameEntry = model.phase() == GameplayPhase::highscoreNameEntry;
        const bool hall = model.phase() == GameplayPhase::highscoreHall;
        if (continuePrompt) {
            if (!options.inputRecordingPath.empty()) {
                inputRecorder.observe(static_cast<std::uint32_t>(model.ticks() + 1), {});
            }
            session.advance({});
            std::cout << "CONTINUE? "
                << (model.continueChoice() == ContinueChoice::yes ? "[YES] NO" : "YES [NO]")
                << '\n';
        }
        const auto& frame = session.frame();
        std::cout << "tick=" << model.ticks()
                  << " heading=" << model.heading()
                  << " travel=" << model.travel()
                  << " shot=" << (model.shotActive() ? "active" : "idle")
                  << " enemy=" << (model.enemyActive() ? "active" : "destroyed")
                  << " bearing=" << model.enemyBearing()
                  << " range=" << model.enemyRange()
                  << " score=" << model.score()
                  << " frame_pixels=" << frame.pixels.size() << '\n';
        if (!std::getline(std::cin, command)) {
            if (!options.inputRecordingPath.empty() && model.ticks() != 0) {
                writeGameplayInputMovie(
                    options.inputRecordingPath,
                    inputRecorder.finish(
                        static_cast<std::uint32_t>(model.ticks())
                    )
                );
            }
            return 0;
        }
        if (nameEntry) {
            if (!options.inputRecordingPath.empty()) {
                throw std::invalid_argument("native text input recording is not yet supported");
            }
            NameEntryInput text;
            for (const unsigned char character : command) {
                text.characters.push_back(character);
            }
            session.advance({}, text);
            if (command.empty()) {
                GameplayInput confirm;
                confirm.confirm = true;
                session.advance(confirm);
                session.advance({});
            }
            continue;
        }
        const bool exitRequested = command == "q";
        if (exitRequested && !continuePrompt && !hall) {
            if (!options.inputRecordingPath.empty() && model.ticks() != 0) {
                writeGameplayInputMovie(
                    options.inputRecordingPath,
                    inputRecorder.finish(
                        static_cast<std::uint32_t>(model.ticks())
                    )
                );
            }
            return 0;
        }
        const GameplayInput input{
            command.find('a') != std::string::npos,
            command.find('d') != std::string::npos,
            command.find('w') != std::string::npos,
            command.find('s') != std::string::npos,
            command.find('f') != std::string::npos,
            (continuePrompt || hall) && exitRequested,
            (continuePrompt || hall) && command.empty(),
        };
        if (!options.inputRecordingPath.empty()) {
            inputRecorder.observe(
                static_cast<std::uint32_t>(model.ticks() + 1),
                input
            );
        }
        session.advance(input);
        if ((continuePrompt && model.phase() == GameplayPhase::continuePrompt)
            || (hall && model.phase() == GameplayPhase::highscoreHall)) {
            if (!options.inputRecordingPath.empty()) {
                inputRecorder.observe(static_cast<std::uint32_t>(model.ticks() + 1), {});
            }
            session.advance({});
        }
    }
    if (!options.inputRecordingPath.empty() && model.ticks() != 0) {
        writeGameplayInputMovie(options.inputRecordingPath,
            inputRecorder.finish(static_cast<std::uint32_t>(model.ticks())));
    }
    return 0;
}

GameplaySmokeResult runGameplaySmoke(
    const GameplayAssets&,
    const std::vector<GameplayInput>&
) {
    throw std::runtime_error("native device smoke requires the Windows frontend");
}

int runNativeEntry(const GameplayAssets&, const NativeStartupAssets&, const GameplayRunOptions&) {
    throw std::runtime_error("continuous startup audio delivery requires the Windows frontend");
}

int runRecoveredNativeEntry(const GameplayAssets&, const NativeStartupAssets&,
    FrameCopyProfile, const GameplayRunOptions&) {
    throw std::runtime_error("recovered device delivery requires the Windows frontend");
}

int runPlayableNativeEntry(const GameplayAssets&, const NativeStartupAssets&,
    FrameCopyProfile, const GameplayRunOptions&) {
    throw std::runtime_error("playable device delivery requires the Windows frontend");
}

GameplaySmokeResult runNativeEntrySmoke(const GameplayAssets&, const NativeStartupAssets&,
    const std::vector<GameplayInput>&) {
    throw std::runtime_error("native entry device smoke requires the Windows frontend");
}

}  // namespace stellar7
