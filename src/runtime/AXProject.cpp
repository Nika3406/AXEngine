#include "runtime/AXProject.h"

#include <SDL3/SDL.h>

#include <fstream>
#include <sstream>

namespace {

static std::string readAll(const std::string& path) {
    std::ifstream in(path);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static bool findStringValue(const std::string& block, const std::string& key, std::string& out) {
    const std::string needle = "\"" + key + "\"";
    size_t p = block.find(needle);
    if (p == std::string::npos) return false;
    p = block.find(':', p);
    if (p == std::string::npos) return false;
    p = block.find('"', p + 1);
    if (p == std::string::npos) return false;

    std::string value;
    bool esc = false;
    for (size_t i = p + 1; i < block.size(); ++i) {
        char c = block[i];
        if (esc) {
            if (c == 'n') value += '\n';
            else if (c == 'r') value += '\r';
            else if (c == 't') value += '\t';
            else value += c;
            esc = false;
            continue;
        }
        if (c == '\\') { esc = true; continue; }
        if (c == '"') { out = value; return true; }
        value += c;
    }
    return false;
}

static std::string parentDir(const std::string& path) {
    size_t p = path.find_last_of("/\\");
    if (p == std::string::npos) return ".";
    return path.substr(0, p);
}

} // namespace

namespace AXProject {

bool load(const std::string& path, AXProjectData& outProject, std::string* error) {
    std::string text = readAll(path);

    if (text.empty()) {
        const char* base = SDL_GetBasePath();
        if (base) {
            text = readAll(std::string(base) + path);
        }
    }

    if (text.empty()) {
        if (error) *error = "Could not read AXProject file: " + path;
        return false;
    }

    AXProjectData p;
    findStringValue(text, "name", p.name);
    findStringValue(text, "startupScene", p.startupScene);
    findStringValue(text, "assetRoot", p.assetRoot);
    findStringValue(text, "scriptRoot", p.scriptRoot);
    findStringValue(text, "generatedRoot", p.generatedRoot);

    outProject = p;
    return true;
}

bool saveDefault(const std::string& path, const AXProjectData& project, std::string* error) {
    std::ofstream out(path);
    if (!out) {
        if (error) *error = "Could not write AXProject file: " + path;
        return false;
    }

    out << "{\n";
    out << "  \"format\": \"AXProject\",\n";
    out << "  \"version\": 1,\n";
    out << "  \"name\": \"" << project.name << "\",\n";
    out << "  \"startupScene\": \"" << project.startupScene << "\",\n";
    out << "  \"assetRoot\": \"" << project.assetRoot << "\",\n";
    out << "  \"scriptRoot\": \"" << project.scriptRoot << "\",\n";
    out << "  \"generatedRoot\": \"" << project.generatedRoot << "\"\n";
    out << "}\n";
    return true;
}

std::string resolveProjectPath(const std::string& projectFile, const std::string& relativePath) {
    if (relativePath.empty()) return relativePath;
    if (relativePath[0] == '/') return relativePath;
    return parentDir(projectFile) + "/" + relativePath;
}

} // namespace AXProject
