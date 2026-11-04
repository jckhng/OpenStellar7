#include "stellar7/input_movie.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {

constexpr std::array<
    std::pair<const char*, bool GameplayInput::*>,
    7
> inputKeys{{
    {"left", &GameplayInput::left},
    {"right", &GameplayInput::right},
    {"up", &GameplayInput::forward},
    {"down", &GameplayInput::reverse},
    {"spc", &GameplayInput::fire},
    {"esc", &GameplayInput::exit},
    {"enter", &GameplayInput::confirm},
}};

void appendTransition(
    const std::uint32_t tick,
    const bool pressed,
    std::vector<std::string> keys,
    std::vector<RecordedGameplayInputEvent>& events
) {
    if (!keys.empty()) {
        events.push_back({tick, pressed, std::move(keys)});
    }
}

std::string joinedKeys(const std::vector<std::string>& keys) {
    std::ostringstream output;
    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index != 0) {
            output << '+';
        }
        output << keys[index];
    }
    return output.str();
}

}  // namespace

void GameplayInputRecorder::observe(
    const std::uint32_t tick,
    const GameplayInput& input
) {
    if (finished_) {
        throw std::logic_error("cannot append to a finished input movie");
    }
    if (tick == 0) {
        throw std::invalid_argument("input movie ticks are one-based");
    }
    if (input.systemAction != GameplaySystemAction::none) {
        throw std::invalid_argument("v1 input movies cannot represent queued system actions");
    }
    std::vector<std::string> released;
    std::vector<std::string> pressed;
    for (const auto& [name, field] : inputKeys) {
        if (previous_.*field && !(input.*field)) {
            released.emplace_back(name);
        } else if (!(previous_.*field) && input.*field) {
            pressed.emplace_back(name);
        }
    }
    appendTransition(tick, false, std::move(released), events_);
    appendTransition(tick, true, std::move(pressed), events_);
    previous_ = input;
}

std::string GameplayInputRecorder::finish(
    const std::uint32_t terminalTick
) {
    if (finished_) {
        throw std::logic_error("input movie is already finished");
    }
    if (terminalTick == 0) {
        throw std::invalid_argument(
            "input movie requires at least one simulated tick"
        );
    }
    GameplayInput released{};
    observe(terminalTick + 1, released);
    finished_ = true;

    std::ostringstream output;
    output
        << "dos-re-state-input-script-v1\n"
        << "# state_field=loop_tick\n"
        << "# controller=portable-interactive-recording-v1\n"
        << "# terminal_value=" << terminalTick << '\n'
        << "# release_value=" << (terminalTick + 1) << '\n';
    for (const auto& event : events_) {
        output
            << event.tick << '='
            << (event.pressed ? "down." : "up.")
            << joinedKeys(event.keys) << '\n';
    }
    return output.str();
}

const std::vector<RecordedGameplayInputEvent>&
GameplayInputRecorder::events() const noexcept {
    return events_;
}

void writeGameplayInputMovie(
    const std::string& path,
    const std::string& content
) {
    const std::filesystem::path outputPath(path);
    if (outputPath.has_parent_path()) {
        std::filesystem::create_directories(outputPath.parent_path());
    }
    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot open input movie: " + path);
    }
    output << content;
    if (!output) {
        throw std::runtime_error("failed while writing input movie: " + path);
    }
}

}  // namespace stellar7
