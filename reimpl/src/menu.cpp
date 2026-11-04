#include "stellar7/menu.hpp"
#include "stellar7/menu_model.hpp"

#include <istream>
#include <ostream>
#include <string>

namespace stellar7 {
namespace {

void printMenu(std::ostream& output, const MenuModel& menu) {
    output << "\nSTELLAR 7\n";
    MenuModel row;
    for (std::size_t index = 0; index < MenuModel::itemCount; ++index) {
        output << (index == menu.selection() ? "> " : "  ") << row.label() << '\n';
        row.moveDown();
    }
    output << "selection=" << menu.label()
           << " cursor=(" << menu.cursorX() << ',' << menu.cursorY() << ")\n"
           << "w=up s=down Enter=select q=quit\n";
}

}  // namespace

int runMenu(std::istream& input, std::ostream& output,
    const std::function<int()>& beginGameplay) {
    MenuModel menu;
    std::string command;
    while (true) {
        printMenu(output, menu);
        if (!std::getline(input, command) || command == "q") {
            return 0;
        }
        if (command == "w") {
            menu.moveUp();
            continue;
        }
        if (command == "s") {
            menu.moveDown();
            continue;
        }
        if (!command.empty()) {
            continue;
        }
        if (menu.label() == "EXIT") {
            return 0;
        }
        if (menu.label() != "BEGIN") {
            output << menu.label() << " is not implemented.\n";
            continue;
        }
        if (!beginGameplay) {
            output << "Launch with --menu DATA_DIRECTORY to begin gameplay.\n";
            continue;
        }
        return beginGameplay();
    }
}

}  // namespace stellar7
