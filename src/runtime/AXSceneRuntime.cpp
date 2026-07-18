#include "runtime/AXSceneRuntime.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

static std::string lowerCopy(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static bool iequals(const std::string& a, const std::string& b) {
    return lowerCopy(a) == lowerCopy(b);
}

static bool startsWithIgnoreCase(const std::string& value, const std::string& token) {
    std::string v = lowerCopy(value);
    std::string t = lowerCopy(token);
    return v.rfind(t, 0) == 0;
}

static bool containsIgnoreCase(const std::string& value, const std::string& token) {
    return lowerCopy(value).find(lowerCopy(token)) != std::string::npos;
}

static std::string readAll(const std::string& path) {
    std::ifstream in(path);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static std::string resolveRuntimeFile(const std::string& path) {
    // Try as typed first.
    std::ifstream direct(path);
    if (direct.good()) return path;

    // Try relative to executable base path.
    const char* base = SDL_GetBasePath();
    if (base) {
        std::string p = std::string(base) + path;
        std::ifstream f(p);
        if (f.good()) return p;
    }

    // Try converted editor asset path.
    std::string converted = AXSceneRuntime::runtimeAssetPath(path);
    std::ifstream conv(converted);
    if (conv.good()) return converted;
    if (base) {
        std::string p = std::string(base) + converted;
        std::ifstream f(p);
        if (f.good()) return p;
    }

    return path;
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
    while (e < block.size() && std::isspace(static_cast<unsigned char>(block[e]))) ++e;
    char* end = nullptr;
    long v = std::strtol(block.c_str() + e, &end, 10);
    if (end == block.c_str() + e) return false;
    out = static_cast<int>(v);
    return true;
}

static bool findFloatValue(const std::string& block, const std::string& key, float& out) {
    const std::string needle = "\"" + key + "\"";
    size_t p = block.find(needle);
    if (p == std::string::npos) return false;
    p = block.find(':', p);
    if (p == std::string::npos) return false;
    size_t e = p + 1;
    while (e < block.size() && std::isspace(static_cast<unsigned char>(block[e]))) ++e;
    char* end = nullptr;
    float v = std::strtof(block.c_str() + e, &end);
    if (end == block.c_str() + e) return false;
    out = v;
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
        while (p < block.size() && std::isspace(static_cast<unsigned char>(block[p]))) ++p;
        char* end = nullptr;
        float v = std::strtof(block.c_str() + p, &end);
        if (end == block.c_str() + p) return false;
        out[i] = v;
        p = static_cast<size_t>(end - block.c_str());
        p = block.find(i == 2 ? ']' : ',', p);
        if (p == std::string::npos) return false;
    }
    return true;
}

static std::string objectTypeFromName(const std::string& name) {
    if (startsWithIgnoreCase(name, "PLAYER_START")) return "PlayerStart";
    if (startsWithIgnoreCase(name, "ENEMY") || startsWithIgnoreCase(name, "AI")) return "Enemy";
    if (startsWithIgnoreCase(name, "CAMERA")) return "Camera";
    if (startsWithIgnoreCase(name, "LIGHT")) return "Light";
    if (startsWithIgnoreCase(name, "VFX")) return "VFX";
    return "GameObject";
}

} // namespace

std::string AXSceneRuntimeComponent::property(const std::string& name, const std::string& fallback) const {
    for (const auto& p : properties) {
        if (iequals(p.name, name)) return p.value;
    }
    return fallback;
}

const AXSceneRuntimeComponent* AXSceneRuntimeObject::component(const std::string& typeName) const {
    for (const auto& c : components) {
        if (iequals(c.type, typeName)) return &c;
    }
    return nullptr;
}

const AXSceneRuntimeObject* AXSceneRuntimeData::firstObjectOfType(const std::string& typeName) const {
    for (const auto& obj : objects) {
        if (iequals(obj.type, typeName)) return &obj;
    }
    return nullptr;
}

namespace AXSceneRuntime {

std::string runtimeAssetPath(const std::string& path) {
    std::string out = path;
    std::replace(out.begin(), out.end(), '\\', '/');

    const std::string prefix = "scripts/assets/";
    if (out.rfind(prefix, 0) == 0) {
        return "assets/" + out.substr(prefix.size());
    }

    // Also accept editor project paths that already point at top-level assets.
    return out;
}

std::string archetypeFromEnemyObject(const AXSceneRuntimeObject& object) {
    if (const AXSceneRuntimeComponent* ai = object.component("EnemyAI")) {
        std::string a = ai->property("archetype");
        if (!a.empty()) return a;
    }

    std::string name = object.name;
    if (startsWithIgnoreCase(name, "ENEMY")) {
        size_t p = name.find('_');
        if (p != std::string::npos && p + 1 < name.size()) return name.substr(p + 1);
        return "Basic";
    }
    if (startsWithIgnoreCase(name, "AI")) {
        size_t p = name.find('_');
        if (p != std::string::npos && p + 1 < name.size()) return name.substr(p + 1);
        return "Basic";
    }
    return "Basic";
}

bool load(const std::string& path, AXSceneRuntimeData& outScene, std::string* error) {
    const std::string resolved = resolveRuntimeFile(path);
    const std::string text = readAll(resolved);
    if (text.empty()) {
        if (error) *error = "Could not read AXScene: " + path + " resolved=" + resolved;
        return false;
    }

    std::string format;
    if (!findStringValue(text, "format", format) || format != "AXScene") {
        if (error) *error = "File is not AXScene: " + resolved;
        return false;
    }

    AXSceneRuntimeData scene;
    findStringValue(text, "name", scene.name);
    findStringValue(text, "renderProfile", scene.renderProfile);
    findStringValue(text, "previewGlb", scene.previewGlb);
    scene.previewGlb = runtimeAssetPath(scene.previewGlb);

    // world block values are unique enough for the simple parser.
    findFloatValue(text, "gravity", scene.gravity);
    findFloatValue(text, "unitScale", scene.unitScale);

    for (const std::string& block : objectBlocksFromArray(text, "objects")) {
        AXSceneRuntimeObject obj;
        findStringValue(block, "id", obj.id);
        findStringValue(block, "name", obj.name);
        findStringValue(block, "type", obj.type);
        if (obj.type.empty()) obj.type = objectTypeFromName(obj.name);
        findStringValue(block, "sourcePath", obj.sourcePath);
        obj.sourcePath = runtimeAssetPath(obj.sourcePath);
        findBoolValue(block, "active", obj.active);
        findBoolValue(block, "static", obj.isStatic);
        findIntValue(block, "parentIndex", obj.parentIndex);
        findFloatArray3(block, "position", obj.position);
        findFloatArray3(block, "rotation", obj.rotation);
        findFloatArray3(block, "scale", obj.scale);

        for (const std::string& cblock : objectBlocksFromArray(block, "components")) {
            AXSceneRuntimeComponent c;
            findStringValue(cblock, "type", c.type);
            findBoolValue(cblock, "active", c.active);
            findBoolValue(cblock, "dynamic", c.dynamic);

            const std::string propNeedle = "\"properties\"";
            size_t p = cblock.find(propNeedle);
            if (p != std::string::npos) {
                size_t start = cblock.find('{', p);
                size_t end = start == std::string::npos ? std::string::npos : findMatching(cblock, start, '{', '}');
                if (start != std::string::npos && end != std::string::npos) {
                    std::string props = cblock.substr(start + 1, end - start - 1);
                    size_t i = 0;
                    while (i < props.size()) {
                        size_t k0 = props.find('"', i);
                        if (k0 == std::string::npos) break;
                        size_t k1 = props.find('"', k0 + 1);
                        if (k1 == std::string::npos) break;
                        size_t colon = props.find(':', k1 + 1);
                        if (colon == std::string::npos) break;
                        size_t v0 = props.find('"', colon + 1);
                        if (v0 == std::string::npos) break;
                        size_t v1 = props.find('"', v0 + 1);
                        if (v1 == std::string::npos) break;
                        c.properties.push_back({props.substr(k0 + 1, k1 - k0 - 1), props.substr(v0 + 1, v1 - v0 - 1)});
                        i = v1 + 1;
                    }
                }
            }

            if (!c.type.empty()) obj.components.push_back(c);
        }

        if (!obj.name.empty() && obj.active) {
            scene.objects.push_back(obj);
        }
    }

    if (scene.objects.empty()) {
        if (error) *error = "AXScene had no active objects: " + resolved;
        return false;
    }

    std::cout << "[AXSceneRuntime] Loaded " << resolved << "\n"
              << "  name=" << scene.name << "\n"
              << "  renderProfile=" << scene.renderProfile << "\n"
              << "  previewGlb=" << scene.previewGlb << "\n"
              << "  objects=" << scene.objects.size() << "\n";

    outScene = std::move(scene);
    return true;
}

} // namespace AXSceneRuntime
