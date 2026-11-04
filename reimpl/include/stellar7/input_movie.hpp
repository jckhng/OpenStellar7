#pragma once

#include "stellar7/gameplay.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace stellar7 {

struct RecordedGameplayInputEvent {
    std::uint32_t tick = 0;
    bool pressed = false;
    std::vector<std::string> keys;
};

class GameplayInputRecorder {
public:
    void observe(std::uint32_t tick, const GameplayInput& input);
    std::string finish(std::uint32_t terminalTick);

    const std::vector<RecordedGameplayInputEvent>& events() const noexcept;

private:
    GameplayInput previous_{};
    std::vector<RecordedGameplayInputEvent> events_;
    bool finished_ = false;
};

void writeGameplayInputMovie(
    const std::string& path,
    const std::string& content
);

}  // namespace stellar7
