#include "core/GameEntry.hpp"
#include "core/App.hpp"   

#include <iostream>

int main() {
    try {
        auto game = CreateGame();
        if (!game) {
            std::cerr << "CreateGame() returned nullptr.\n";
            return 1;
        }

        game->run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
