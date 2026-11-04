#pragma once

#include <cstddef>
#include <string_view>

namespace stellar7 {

enum class PlayableMenuPage { main, scores, briefing, preferences };
class PlayableMenuDialog;

class MenuModel {
public:
    static constexpr std::size_t itemCount = 5;

    std::size_t selection() const noexcept;
    std::string_view label() const noexcept;
    int cursorX() const noexcept;
    int cursorY() const noexcept;
    void moveUp() noexcept;
    void moveDown() noexcept;

private:
    std::size_t selection_ = 0;
};

}  // namespace stellar7
