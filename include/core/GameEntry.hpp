#pragma once
#include <memory>

namespace core {
class App;
}

// User/game module entry point.
// Implemented in scripts/*.cpp (e.g., scripts/MyGame.cpp)
std::unique_ptr<core::App> CreateGame();
