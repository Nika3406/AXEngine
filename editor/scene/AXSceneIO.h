#pragma once

#include "../EditorState.h"
#include <filesystem>

class AXSceneIO {
public:
    static bool writeSceneFile(const std::filesystem::path& outputPath, const EditorState& state, std::string* error = nullptr);
    static bool readSceneFile(const std::filesystem::path& inputPath, EditorState& state, std::string* error = nullptr);
    static bool writeDefaultSceneFile(const std::filesystem::path& outputPath);
};
