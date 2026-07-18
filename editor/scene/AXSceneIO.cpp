#include "AXSceneIO.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <cstdlib>
#include <sstream>

namespace {
static void ensureParentDirectory(const std::filesystem::path& path) {
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
}

static std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

static std::string readAll(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static std::string trim(std::string s) {
    auto notSpace = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
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

static bool findBoolValue(const std::string& block, const std::string& key, bool& out) {
    const std::string needle = "\"" + key + "\"";
    size_t p = block.find(needle);
    if (p == std::string::npos) return false;
    p = block.find(':', p);
    if (p == std::string::npos) return false;
    std::string tail = block.substr(p + 1, 8);
    if (tail.find("true") != std::string::npos) { out = true; return true; }
    if (tail.find("false") != std::string::npos) { out = false; return true; }
    return false;
}

static bool findIntValue(const std::string& block, const std::string& key, int& out) {
    const std::string needle = "\"" + key + "\"";
    size_t p = block.find(needle);
    if (p == std::string::npos) return false;
    p = block.find(':', p);
    if (p == std::string::npos) return false;
    size_t e = p + 1;
    while (e < block.size() && (std::isspace((unsigned char)block[e]) || block[e] == ',')) ++e;
    char* end = nullptr;
    long v = std::strtol(block.c_str() + e, &end, 10);
    if (end == block.c_str() + e) return false;
    out = (int)v;
    return true;
}

static bool findFloatArray3(const std::string& block, const std::string& key, float out[3]) {
    const std::string needle = "\"" + key + "\"";
    size_t p = block.find(needle);
    if (p == std::string::npos) return false;
    p = block.find('[', p);
    if (p == std::string::npos) return false;
    for (int i = 0; i < 3; ++i) {
        ++p;
        while (p < block.size() && std::isspace((unsigned char)block[p])) ++p;
        char* end = nullptr;
        float v = std::strtof(block.c_str() + p, &end);
        if (end == block.c_str() + p) return false;
        out[i] = v;
        p = (size_t)(end - block.c_str());
        p = block.find(i == 2 ? ']' : ',', p);
        if (p == std::string::npos) return false;
    }
    return true;
}

static size_t findMatching(const std::string& s, size_t openPos, char open, char close) {
    int depth = 0;
    bool inString = false;
    bool esc = false;
    for (size_t i = openPos; i < s.size(); ++i) {
        char c = s[i];
        if (inString) {
            if (esc) { esc = false; continue; }
            if (c == '\\') { esc = true; continue; }
            if (c == '"') inString = false;
            continue;
        }
        if (c == '"') { inString = true; continue; }
        if (c == open) ++depth;
        else if (c == close) {
            --depth;
            if (depth == 0) return i;
        }
    }
    return std::string::npos;
}

static std::vector<std::string> objectBlocksFromArray(const std::string& text, const std::string& key) {
    std::vector<std::string> out;
    const std::string needle = "\"" + key + "\"";
    size_t p = text.find(needle);
    if (p == std::string::npos) return out;
    size_t arr = text.find('[', p);
    if (arr == std::string::npos) return out;
    size_t arrEnd = findMatching(text, arr, '[', ']');
    if (arrEnd == std::string::npos) return out;
    size_t i = arr + 1;
    while (i < arrEnd) {
        size_t objStart = text.find('{', i);
        if (objStart == std::string::npos || objStart >= arrEnd) break;
        size_t objEnd = findMatching(text, objStart, '{', '}');
        if (objEnd == std::string::npos || objEnd > arrEnd) break;
        out.push_back(text.substr(objStart, objEnd - objStart + 1));
        i = objEnd + 1;
    }
    return out;
}


static std::vector<EditorProperty> parsePropertiesObject(const std::string& block) {
    std::vector<EditorProperty> out;
    const std::string needle = "\"properties\"";
    size_t p = block.find(needle);
    if (p == std::string::npos) return out;
    p = block.find('{', p);
    if (p == std::string::npos) return out;
    size_t end = findMatching(block, p, '{', '}');
    if (end == std::string::npos) return out;
    size_t i = p + 1;
    while (i < end) {
        while (i < end && (std::isspace((unsigned char)block[i]) || block[i] == ',')) ++i;
        if (i >= end || block[i] != '"') break;
        std::string key;
        bool esc = false;
        ++i;
        for (; i < end; ++i) {
            char c = block[i];
            if (esc) { key += c; esc = false; continue; }
            if (c == '\\') { esc = true; continue; }
            if (c == '"') { ++i; break; }
            key += c;
        }
        size_t colon = block.find(':', i);
        if (colon == std::string::npos || colon >= end) break;
        i = colon + 1;
        while (i < end && std::isspace((unsigned char)block[i])) ++i;
        std::string value;
        if (i < end && block[i] == '"') {
            ++i; esc = false;
            for (; i < end; ++i) {
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
                if (c == '"') { ++i; break; }
                value += c;
            }
        } else {
            size_t vstart = i;
            while (i < end && block[i] != ',') ++i;
            value = trim(block.substr(vstart, i - vstart));
        }
        if (!key.empty()) out.push_back({key, value});
    }
    return out;
}

static void writeComponents(std::ostream& out, const std::vector<EditorComponent>& components, int indent) {
    const std::string pad(indent, ' ');
    const std::string pad2(indent + 2, ' ');
    out << pad << "\"components\": [\n";
    for (size_t i = 0; i < components.size(); ++i) {
        const EditorComponent& c = components[i];
        out << pad2 << "{\n";
        out << pad2 << "  \"type\": \"" << escapeJson(c.type) << "\",\n";
        out << pad2 << "  \"active\": " << (c.active ? "true" : "false") << ",\n";
        out << pad2 << "  \"dynamic\": " << (c.dynamic ? "true" : "false") << ",\n";
        out << pad2 << "  \"properties\": {";
        for (size_t p = 0; p < c.properties.size(); ++p) {
            const EditorProperty& prop = c.properties[p];
            if (p > 0) out << ", ";
            out << "\"" << escapeJson(prop.name) << "\": \"" << escapeJson(prop.value) << "\"";
        }
        out << "}\n";
        out << pad2 << "}" << (i + 1 == components.size() ? "" : ",") << "\n";
    }
    out << pad << "]\n";
}
}

bool AXSceneIO::writeSceneFile(const std::filesystem::path& outputPath, const EditorState& state, std::string* error) {
    ensureParentDirectory(outputPath);
    std::ofstream out(outputPath);
    if (!out) {
        if (error) *error = "Could not open for writing: " + outputPath.string();
        return false;
    }

    out << "{\n";
    out << "  \"format\": \"AXScene\",\n";
    out << "  \"version\": 1,\n";
    out << "  \"name\": \"" << escapeJson(state.sceneName) << "\",\n";
    out << "  \"renderProfile\": \"" << escapeJson(state.sceneRenderProfile) << "\",\n";
    out << "  \"previewGlb\": \"" << escapeJson(state.sceneGlbPath) << "\",\n";
    out << "  \"world\": {\n";
    out << "    \"gravity\": " << state.sceneGravity << ",\n";
    out << "    \"unitScale\": " << state.sceneUnitScale << "\n";
    out << "  },\n";
    out << "  \"editor\": {\n";
    out << "    \"showGrid\": " << (state.showGrid ? "true" : "false") << ",\n";
    out << "    \"sceneCameraPosition\": [" << state.sceneCameraFocus[0] << ", " << state.sceneCameraFocus[1] << ", " << state.sceneCameraFocus[2] << "],\n";
    out << "    \"sceneCameraYawPitchDistance\": [" << state.sceneCameraYaw << ", " << state.sceneCameraPitch << ", " << state.sceneCameraDistance << "]\n";
    out << "  },\n";
    out << "  \"objects\": [\n";

    for (size_t i = 0; i < state.hierarchy.size(); ++i) {
        const EditorObject& obj = state.hierarchy[i];
        out << "    {\n";
        out << "      \"id\": \"obj_" << i << "\",\n";
        out << "      \"name\": \"" << escapeJson(obj.name) << "\",\n";
        out << "      \"type\": \"" << escapeJson(obj.type) << "\",\n";
        out << "      \"sourcePath\": \"" << escapeJson(obj.sourcePath) << "\",\n";
        out << "      \"active\": " << (obj.active ? "true" : "false") << ",\n";
        out << "      \"static\": " << (obj.isStatic ? "true" : "false") << ",\n";
        out << "      \"parentIndex\": " << obj.parentIndex << ",\n";
        out << "      \"transform\": {\n";
        out << "        \"position\": [" << obj.position[0] << ", " << obj.position[1] << ", " << obj.position[2] << "],\n";
        out << "        \"rotation\": [" << obj.rotation[0] << ", " << obj.rotation[1] << ", " << obj.rotation[2] << "],\n";
        out << "        \"scale\": [" << obj.scale[0] << ", " << obj.scale[1] << ", " << obj.scale[2] << "]\n";
        out << "      },\n";
        writeComponents(out, obj.components, 6);
        out << "    }" << (i + 1 == state.hierarchy.size() ? "" : ",") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return true;
}

bool AXSceneIO::readSceneFile(const std::filesystem::path& inputPath, EditorState& state, std::string* error) {
    std::string text = readAll(inputPath);
    if (text.empty()) {
        if (error) *error = "Could not read or empty file: " + inputPath.string();
        return false;
    }

    std::string format;
    if (!findStringValue(text, "format", format) || format != "AXScene") {
        if (error) *error = "Not an AXScene file: " + inputPath.string();
        return false;
    }

    findStringValue(text, "name", state.sceneName);
    findStringValue(text, "renderProfile", state.sceneRenderProfile);
    findStringValue(text, "previewGlb", state.sceneGlbPath);

    std::vector<EditorObject> objects;
    for (const std::string& block : objectBlocksFromArray(text, "objects")) {
        EditorObject obj;
        findStringValue(block, "name", obj.name);
        findStringValue(block, "type", obj.type);
        findStringValue(block, "sourcePath", obj.sourcePath);
        findBoolValue(block, "active", obj.active);
        findBoolValue(block, "static", obj.isStatic);
        findIntValue(block, "parentIndex", obj.parentIndex);
        findFloatArray3(block, "position", obj.position);
        findFloatArray3(block, "rotation", obj.rotation);
        findFloatArray3(block, "scale", obj.scale);

        // Minimal component recovery: keep component types so the editor still shows useful data.
        for (const std::string& cblock : objectBlocksFromArray(block, "components")) {
            EditorComponent c;
            findStringValue(cblock, "type", c.type);
            findBoolValue(cblock, "active", c.active);
            findBoolValue(cblock, "dynamic", c.dynamic);
            c.properties = parsePropertiesObject(cblock);
            obj.components.push_back(c);
        }

        if (!obj.name.empty()) objects.push_back(obj);
    }

    // Empty scenes are valid in a real engine/editor.
    state.hierarchy = std::move(objects);
    clearSelection(state);
    state.currentScenePath = inputPath;
    state.sceneDirty = false;
    state.showSceneView = true;
    return true;
}

bool AXSceneIO::writeDefaultSceneFile(const std::filesystem::path& outputPath) {
    EditorState s;
    s.sceneName = outputPath.stem().string();
    s.currentScenePath = outputPath;

    EditorObject environment;
    environment.name = "Environment";
    environment.type = "StaticScene";
    environment.sourcePath = "";
    environment.isStatic = true;
    environment.scale[0] = environment.scale[1] = environment.scale[2] = 1.0f;
    environment.components.push_back({"StaticMeshRenderer", true, false, {{"model", ""}}});
    environment.components.push_back({"MeshCollider", true, false, {{"source", "COLLIDER_*"}}});
    s.hierarchy.push_back(environment);

    EditorObject playerStart;
    playerStart.name = "PlayerStart";
    playerStart.type = "PlayerStart";
    playerStart.components.push_back({"PlayerStart", true, false, {{"spawnCameraYaw", "180"}}});
    s.hierarchy.push_back(playerStart);

    return writeSceneFile(outputPath, s, nullptr);
}
