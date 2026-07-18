#include "EditorApp.h"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    std::filesystem::path projectRoot = std::filesystem::current_path();

    if (argc > 1) {
        projectRoot = std::filesystem::absolute(argv[1]);
    }

    EditorApp app(projectRoot);
    if (!app.initialize()) {
        std::cerr << "[AXEditor] Failed to initialize.\n";
        return 1;
    }

    app.run();
    app.shutdown();
    return 0;
}
