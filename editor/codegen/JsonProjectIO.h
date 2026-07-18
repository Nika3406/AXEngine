#pragma once
#include "../EditorState.h"
#include <filesystem>
#include <vector>

class JsonProjectIO {
public:
    static bool writeScene(const std::filesystem::path& outputPath, const std::vector<EditorObject>& objects);
    static bool writeDefaultGunner(const std::filesystem::path& outputPath);
    static bool writeDefaultBloomBlade(const std::filesystem::path& outputPath);
};
