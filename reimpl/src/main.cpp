#include <SDL_main.h>
#include "native_player_entry.hpp"
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (const auto result = stellar7::runNativePlayerCommand(argc, argv, stellar7::runInstalledPlayer)) {
            return *result;
        }
        std::cerr << "Unsupported player option. Use --help. Developer diagnostics are in stellar7-tools.\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "stellar7: " << error.what() << '\n';
        return 1;
    }
}
