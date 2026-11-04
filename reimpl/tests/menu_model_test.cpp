#include "stellar7/menu_model.hpp"
#include "stellar7/menu.hpp"
#include "stellar7/gameplay.hpp"

#include <cassert>
#include <array>
#include <sstream>
#include <string_view>

void testMenuModel() {
    stellar7::MenuModel menu;
    assert(menu.selection() == 0);
    assert(menu.label() == std::string_view("BEGIN"));
    assert(menu.cursorX() == 163);
    assert(menu.cursorY() == 69);

    menu.moveDown();
    assert(menu.selection() == 1);
    assert(menu.label() == std::string_view("BRIEFING"));
    assert(menu.cursorX() == 163);
    assert(menu.cursorY() == 101);

    menu.moveUp();
    assert(menu.selection() == 0);
    menu.moveUp();
    assert(menu.selection() == 4);
    assert(menu.cursorY() == 176);
    menu.moveDown();
    assert(menu.selection() == 0);
    assert(menu.cursorY() == 69);
    constexpr std::array<int, stellar7::MenuModel::itemCount> sourceRows{69, 101, 126, 151, 176};
    for (const auto row : sourceRows) {
        assert(menu.cursorY() == row);
        menu.moveDown();
    }
    assert(menu.selection() == 0);

    std::ostringstream output;
    std::istringstream beginInput("\n\n");
    unsigned launches = 0;
    const auto result = stellar7::runMenu(beginInput, output, [&]() {
        ++launches;
        stellar7::GameplayModel game;
        const auto initialTick = game.ticks();
        game.tick({false, false, true, false, false});
        game.tick({false, false, true, false, false});
        assert(game.ticks() == initialTick + 2);
        return 7;
    });
    assert(result == 7);
    assert(launches == 1);

    std::istringstream exitInput("s\ns\ns\ns\n\n");
    assert(stellar7::runMenu(exitInput, output, [&]() { ++launches; return 1; }) == 0);
    assert(launches == 1);

    std::istringstream unsupportedInput("s\n\nw\n\n");
    assert(stellar7::runMenu(unsupportedInput, output, [&]() { ++launches; return 0; }) == 0);
    assert(launches == 2);
    assert(output.str().find("BRIEFING is not implemented") != std::string::npos);

    std::istringstream demoInput("\nq\n");
    assert(stellar7::runMenu(demoInput, output) == 0);
    assert(output.str().find("--menu DATA_DIRECTORY") != std::string::npos);
}
