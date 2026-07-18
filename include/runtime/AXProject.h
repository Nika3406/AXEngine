#pragma once

#include <string>
#include <vector>

struct AXProjectData {
    std::string name = "Untitled";
    std::string startupScene = "assets/scenes/Empty.axscene";
    std::string assetRoot = "assets";
    std::string scriptRoot = "scripts";
    std::string generatedRoot = "generated";
};

namespace AXProject {
    bool load(const std::string& path, AXProjectData& outProject, std::string* error = nullptr);
    bool saveDefault(const std::string& path, const AXProjectData& project, std::string* error = nullptr);
    std::string resolveProjectPath(const std::string& projectFile, const std::string& relativePath);
}
