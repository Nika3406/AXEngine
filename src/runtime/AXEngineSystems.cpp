#include "runtime/AXEngineSystems.h"
#include <sstream>
#include <functional>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace ax {

std::string makeStableAssetId(const std::string& type, const std::string& pathOrName) {
    std::hash<std::string> h;
    const uint64_t v = static_cast<uint64_t>(h(type + ":" + pathOrName));
    std::ostringstream oss;
    oss << type << "-" << std::hex << v;
    return oss.str();
}

PlatformInfo detectPlatformInfo() {
    PlatformInfo info;
#if defined(_WIN32)
    info.os = "Windows";
#elif defined(__APPLE__)
    info.os = "macOS";
#elif defined(__linux__)
    info.os = "Linux";
#else
    info.os = "Unknown";
#endif

#if defined(__clang__)
    info.compiler = "Clang";
#elif defined(__GNUC__)
    info.compiler = "GCC";
#elif defined(_MSC_VER)
    info.compiler = "MSVC";
#else
    info.compiler = "UnknownCompiler";
#endif

#if defined(NDEBUG)
    info.buildType = "Release";
#else
    info.buildType = "Debug";
#endif
    return info;
}

namespace {
static std::string axLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}
static std::string readAllText(const std::string& path) {
    std::ifstream in(path);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
static std::string readStringKey(const std::string& text, const std::string& key, const std::string& fallback = {}) {
    const std::string needle = "\"" + key + "\"";
    size_t p = text.find(needle);
    if (p == std::string::npos) return fallback;
    p = text.find(':', p);
    if (p == std::string::npos) return fallback;
    p = text.find('"', p + 1);
    if (p == std::string::npos) return fallback;
    size_t q = text.find('"', p + 1);
    if (q == std::string::npos) return fallback;
    return text.substr(p + 1, q - p - 1);
}
static float readFloatKey(const std::string& text, const std::string& key, float fallback) {
    const std::string needle = "\"" + key + "\"";
    size_t p = text.find(needle);
    if (p == std::string::npos) return fallback;
    p = text.find(':', p);
    if (p == std::string::npos) return fallback;
    char* end = nullptr;
    float v = std::strtof(text.c_str() + p + 1, &end);
    return end == text.c_str() + p + 1 ? fallback : v;
}
static std::vector<std::string> objectBlocksInArray2(const std::string& text, const std::string& arrayKey) {
    std::vector<std::string> out;
    const std::string needle = "\"" + arrayKey + "\"";
    size_t p = text.find(needle);
    if (p == std::string::npos) return out;
    p = text.find('[', p);
    if (p == std::string::npos) return out;
    int bracketDepth = 0;
    int braceDepth = 0;
    size_t blockStart = std::string::npos;
    bool inString = false;
    bool esc = false;
    for (size_t i = p; i < text.size(); ++i) {
        char c = text[i];
        if (inString) {
            if (esc) { esc = false; continue; }
            if (c == '\\') { esc = true; continue; }
            if (c == '"') inString = false;
            continue;
        }
        if (c == '"') { inString = true; continue; }
        if (c == '[') ++bracketDepth;
        else if (c == ']') { --bracketDepth; if (bracketDepth <= 0) break; }
        else if (c == '{') { if (braceDepth == 0) blockStart = i; ++braceDepth; }
        else if (c == '}') { --braceDepth; if (braceDepth == 0 && blockStart != std::string::npos) { out.push_back(text.substr(blockStart, i - blockStart + 1)); blockStart = std::string::npos; } }
    }
    return out;
}
}

std::string firstKeyboardToken(const std::string& keyboardSpec) {
    std::string token;
    for (char c : keyboardSpec) {
        if (c == '/' || c == ',' || c == '|' || c == ';') break;
        token += c;
    }
    while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front()))) token.erase(token.begin());
    while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back()))) token.pop_back();
    return token;
}

bool loadInputMapFile(const std::string& path, InputMapDefinition& out, std::string* error) {
    std::string text = readAllText(path);
    if (text.empty()) {
        if (error) *error = "Could not read input map: " + path;
        return false;
    }
    out = InputMapDefinition{};
    out.header.type = readStringKey(text, "type", "AXInput");
    out.header.assetId = readStringKey(text, "assetId", makeStableAssetId("AXInput", path));
    out.bufferSeconds = readFloatKey(text, "bufferSeconds", out.bufferSeconds);
    for (const std::string& block : objectBlocksInArray2(text, "actions")) {
        InputBinding b;
        b.action = readStringKey(block, "name", "");
        b.keyboard = readStringKey(block, "keyboard", "");
        b.gamepad = readStringKey(block, "gamepad", "");
        if (!b.action.empty()) out.bindings.push_back(b);
    }
    return !out.bindings.empty();
}


bool loadVFXFile(const std::string& path, VFXDefinition& out, std::string* error) {
    std::string text = readAllText(path);
    if (text.empty()) {
        if (error) *error = "Could not read VFX file: " + path;
        return false;
    }
    out = VFXDefinition{};
    out.header.type = readStringKey(text, "type", "AXVFX");
    out.header.assetId = readStringKey(text, "assetId", makeStableAssetId("AXVFX", path));
    out.kind = readStringKey(text, "kind", out.kind);
    out.mesh = readStringKey(text, "mesh", out.mesh);
    out.material = readStringKey(text, "material", out.material);
    out.lifetime = readFloatKey(text, "lifetime", out.lifetime);
    std::string bloomText = axLowerCopy(readStringKey(text, "bloom", ""));
    if (!bloomText.empty()) out.bloom = (bloomText == "true" || bloomText == "1");
    return true;
}

bool loadEnemyFile(const std::string& path, EnemyDefinition& out, std::string* error) {
    std::string text = readAllText(path);
    if (text.empty()) {
        if (error) *error = "Could not read enemy file: " + path;
        return false;
    }
    out = EnemyDefinition{};
    out.header.type = readStringKey(text, "type", "AXEnemy");
    out.header.assetId = readStringKey(text, "assetId", makeStableAssetId("AXEnemy", path));
    out.archetype = readStringKey(text, "archetype", readStringKey(text, "type", out.archetype));
    out.model = readStringKey(text, "model", out.model);
    out.hp = static_cast<int>(readFloatKey(text, "hp", static_cast<float>(out.hp)));
    out.radius = readFloatKey(text, "radius", out.radius);
    out.height = readFloatKey(text, "height", out.height);
    out.detectionRange = readFloatKey(text, "detectionRange", out.detectionRange);
    out.preferredRange = readFloatKey(text, "preferredRange", out.preferredRange);
    out.attackCooldown = readFloatKey(text, "attackCooldown", out.attackCooldown);
    out.moveSpeed = readFloatKey(text, "moveSpeed", out.moveSpeed);

    for (const std::string& block : objectBlocksInArray2(text, "attacks")) {
        EnemyAttackDefinition atk;
        atk.name = readStringKey(block, "name", atk.name);
        atk.range = readFloatKey(block, "range", atk.range);
        atk.cooldown = readFloatKey(block, "cooldown", atk.cooldown);
        atk.windup = readFloatKey(block, "windup", atk.windup);
        atk.activeEnd = readFloatKey(block, "activeEnd", atk.activeEnd);
        atk.damage = static_cast<int>(readFloatKey(block, "damage", static_cast<float>(atk.damage)));
        out.attacks.push_back(atk);
    }
    if (out.attacks.empty()) out.attacks.push_back(EnemyAttackDefinition{});
    return true;
}

} // namespace ax
