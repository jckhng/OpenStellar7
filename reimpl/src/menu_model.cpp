#include "stellar7/menu_model.hpp"

#include <array>

namespace stellar7 {
namespace {

constexpr std::array<std::string_view, MenuModel::itemCount> labels{
    "BEGIN",
    "BRIEFING",
    "PREFS",
    "SCORES",
    "EXIT",
};

constexpr std::array<int, MenuModel::itemCount> cursorRows{
    69,
    101,
    126,
    151,
    176,
};

}  // namespace

std::size_t MenuModel::selection() const noexcept {
    return selection_;
}

std::string_view MenuModel::label() const noexcept {
    return labels[selection_];
}

int MenuModel::cursorX() const noexcept {
    return 163;
}

int MenuModel::cursorY() const noexcept {
    return cursorRows[selection_];
}

void MenuModel::moveUp() noexcept {
    selection_ = (selection_ + itemCount - 1) % itemCount;
}

void MenuModel::moveDown() noexcept {
    selection_ = (selection_ + 1) % itemCount;
}

}  // namespace stellar7
