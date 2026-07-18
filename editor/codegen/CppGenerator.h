#pragma once
#include "../EditorState.h"
#include <filesystem>
#include <vector>

class CppGenerator {
public:
    static bool writeRegistries(const std::filesystem::path& outputPath, const std::vector<EditorObject>& objects);
};
