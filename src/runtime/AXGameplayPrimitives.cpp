#include "runtime/AXGameplayPrimitives.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace ax {
namespace {

static std::string lowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

static std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

static std::string readAllText(const std::string& path) {
    std::ifstream in(path);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static bool writeAllText(const std::string& path, const std::string& text, std::string* error) {
    std::ofstream out(path);
    if (!out) {
        if (error) *error = "Could not write file: " + path;
        return false;
    }
    out << text;
    return true;
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

static int readIntKey(const std::string& text, const std::string& key, int fallback) {
    return static_cast<int>(readFloatKey(text, key, static_cast<float>(fallback)));
}

static std::vector<std::string> objectBlocksInArray(const std::string& text, const std::string& arrayKey) {
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
        else if (c == '}') {
            --braceDepth;
            if (braceDepth == 0 && blockStart != std::string::npos) {
                out.push_back(text.substr(blockStart, i - blockStart + 1));
                blockStart = std::string::npos;
            }
        }
    }
    return out;
}

static std::string stableRuntimeId(uint64_t number) {
    std::ostringstream oss;
    oss << "runtime-" << std::setw(8) << std::setfill('0') << number;
    return oss.str();
}

static void writeVec3(std::ostringstream& out, const Vec3f& v) {
    out << "[" << v.x << ", " << v.y << ", " << v.z << "]";
}

static Vec3f readVec3Keys(const std::string& text, const std::string& prefix, const Vec3f& fallback) {
    return {
        readFloatKey(text, prefix + "X", fallback.x),
        readFloatKey(text, prefix + "Y", fallback.y),
        readFloatKey(text, prefix + "Z", fallback.z)
    };
}

} // namespace

// -----------------------------------------------------------------------------
// GameplayEventBus
// -----------------------------------------------------------------------------

int GameplayEventBus::subscribe(const std::string& type, Listener listener) {
    if (!listener) return 0;
    const int id = nextListenerId_++;
    listeners_[type].push_back({id, std::move(listener)});
    return id;
}

void GameplayEventBus::unsubscribe(const std::string& type, int subscriptionId) {
    auto it = listeners_.find(type);
    if (it == listeners_.end()) return;
    auto& list = it->second;
    list.erase(std::remove_if(list.begin(), list.end(), [&](const ListenerEntry& e) {
        return e.id == subscriptionId;
    }), list.end());
}

void GameplayEventBus::emit(const GameplayEvent& event) {
    auto callList = [&](const std::string& type) {
        auto it = listeners_.find(type);
        if (it == listeners_.end()) return;
        for (const ListenerEntry& entry : it->second) {
            if (entry.fn) entry.fn(event);
        }
    };
    callList(event.type);
    callList("*");
}

void GameplayEventBus::queue(const GameplayEvent& event) {
    queued_.push_back(event);
}

void GameplayEventBus::flushQueued() {
    const std::vector<GameplayEvent> events = queued_;
    queued_.clear();
    for (const GameplayEvent& e : events) emit(e);
}

void GameplayEventBus::clear() {
    listeners_.clear();
    queued_.clear();
    nextListenerId_ = 1;
}

size_t GameplayEventBus::listenerCount(const std::string& type) const {
    auto it = listeners_.find(type);
    return it == listeners_.end() ? 0u : it->second.size();
}

// -----------------------------------------------------------------------------
// RuntimeObjectRegistry
// -----------------------------------------------------------------------------

RuntimeObjectRecord& RuntimeObjectRegistry::create(const std::string& archetype, const std::string& name) {
    RuntimeObjectRecord rec;
    rec.id = stableRuntimeId(nextId_++);
    rec.name = name.empty() ? rec.id : name;
    rec.archetype = archetype;
    objects_.push_back(rec);
    return objects_.back();
}

bool RuntimeObjectRegistry::destroy(const std::string& id) {
    const size_t oldSize = objects_.size();
    objects_.erase(std::remove_if(objects_.begin(), objects_.end(), [&](const RuntimeObjectRecord& o) {
        return o.id == id;
    }), objects_.end());
    return objects_.size() != oldSize;
}

RuntimeObjectRecord* RuntimeObjectRegistry::find(const std::string& id) {
    for (RuntimeObjectRecord& o : objects_) if (o.id == id) return &o;
    return nullptr;
}

const RuntimeObjectRecord* RuntimeObjectRegistry::find(const std::string& id) const {
    for (const RuntimeObjectRecord& o : objects_) if (o.id == id) return &o;
    return nullptr;
}

std::vector<RuntimeObjectRecord*> RuntimeObjectRegistry::findByTag(const std::string& tag) {
    std::vector<RuntimeObjectRecord*> out;
    for (RuntimeObjectRecord& o : objects_) {
        if (std::find(o.tags.begin(), o.tags.end(), tag) != o.tags.end()) out.push_back(&o);
    }
    return out;
}

std::vector<RuntimeObjectRecord*> RuntimeObjectRegistry::findByArchetype(const std::string& archetype) {
    std::vector<RuntimeObjectRecord*> out;
    for (RuntimeObjectRecord& o : objects_) if (o.archetype == archetype) out.push_back(&o);
    return out;
}

void RuntimeObjectRegistry::clear() {
    objects_.clear();
    nextId_ = 1;
}

// -----------------------------------------------------------------------------
// SaveGameStore
// -----------------------------------------------------------------------------

void SaveGameStore::setGlobal(const std::string& key, const std::string& value) {
    for (SaveValue& v : data_.globals) {
        if (v.key == key) { v.value = value; return; }
    }
    data_.globals.push_back({key, value});
}

std::string SaveGameStore::getGlobal(const std::string& key, const std::string& fallback) const {
    for (const SaveValue& v : data_.globals) if (v.key == key) return v.value;
    return fallback;
}

SaveObjectState& SaveGameStore::upsertObject(const std::string& objectId, const std::string& archetype) {
    for (SaveObjectState& o : data_.objects) {
        if (o.objectId == objectId) return o;
    }
    SaveObjectState state;
    state.objectId = objectId;
    state.archetype = archetype;
    data_.objects.push_back(state);
    return data_.objects.back();
}

bool SaveGameStore::saveToFile(const std::string& path, std::string* error) const {
    return saveDefaultSaveGame(path, data_, error);
}

bool SaveGameStore::loadFromFile(const std::string& path, std::string* error) {
    return loadDefaultSaveGame(path, data_, error);
}

void SaveGameStore::clear() {
    data_ = SaveGameData{};
}

// -----------------------------------------------------------------------------
// String conversions
// -----------------------------------------------------------------------------

std::string uiAnchorToString(UIAnchor anchor) {
    switch (anchor) {
        case UIAnchor::TopLeft: return "TopLeft";
        case UIAnchor::TopRight: return "TopRight";
        case UIAnchor::BottomLeft: return "BottomLeft";
        case UIAnchor::BottomRight: return "BottomRight";
        case UIAnchor::Center: return "Center";
        case UIAnchor::WorldSpace: return "WorldSpace";
    }
    return "TopLeft";
}

UIAnchor uiAnchorFromString(const std::string& value) {
    const std::string v = lowerCopy(value);
    if (v == "topright") return UIAnchor::TopRight;
    if (v == "bottomleft") return UIAnchor::BottomLeft;
    if (v == "bottomright") return UIAnchor::BottomRight;
    if (v == "center") return UIAnchor::Center;
    if (v == "worldspace") return UIAnchor::WorldSpace;
    return UIAnchor::TopLeft;
}

std::string voxelBlendModeToString(VoxelBlendMode mode) {
    switch (mode) {
        case VoxelBlendMode::Stamp: return "Stamp";
        case VoxelBlendMode::TerrainBlend: return "TerrainBlend";
        case VoxelBlendMode::RuinBlend: return "RuinBlend";
        case VoxelBlendMode::CaveBlend: return "CaveBlend";
    }
    return "TerrainBlend";
}

VoxelBlendMode voxelBlendModeFromString(const std::string& value) {
    const std::string v = lowerCopy(value);
    if (v == "stamp") return VoxelBlendMode::Stamp;
    if (v == "ruinblend") return VoxelBlendMode::RuinBlend;
    if (v == "caveblend") return VoxelBlendMode::CaveBlend;
    return VoxelBlendMode::TerrainBlend;
}

// -----------------------------------------------------------------------------
// Data-file helpers
// -----------------------------------------------------------------------------

bool saveGameplayEventsTemplate(const std::string& path, std::string* error) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"type\": \"AXGameplayEvents\",\n";
    out << "  \"version\": 1,\n";
    out << "  \"events\": [\n";
    out << "    { \"name\": \"OnAttackHit\", \"description\": \"Emitted when an authored attack volume hits a hurtbox.\" },\n";
    out << "    { \"name\": \"OnTriggerEnter\", \"description\": \"Emitted when a runtime object enters a trigger volume.\" },\n";
    out << "    { \"name\": \"OnProjectileHit\", \"description\": \"Use this for coin ricochet, railgun redirection, bullets, arrows, magic, etc.\" },\n";
    out << "    { \"name\": \"OnAnimationEvent\", \"description\": \"Emitted by timeline frame/time markers.\" },\n";
    out << "    { \"name\": \"OnSceneLoaded\", \"description\": \"Emitted after the runtime scene source-of-truth is loaded.\" }\n";
    out << "  ]\n";
    out << "}\n";
    return writeAllText(path, out.str(), error);
}

bool saveDefaultSaveGame(const std::string& path, const SaveGameData& data, std::string* error) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"type\": \"AXSaveGame\",\n";
    out << "  \"version\": " << data.version << ",\n";
    out << "  \"saveId\": \"" << escapeJson(data.saveId) << "\",\n";
    out << "  \"scenePath\": \"" << escapeJson(data.scenePath) << "\",\n";
    out << "  \"globals\": [\n";
    for (size_t i = 0; i < data.globals.size(); ++i) {
        const SaveValue& v = data.globals[i];
        out << "    { \"key\": \"" << escapeJson(v.key) << "\", \"value\": \"" << escapeJson(v.value) << "\" }";
        out << (i + 1 < data.globals.size() ? "," : "") << "\n";
    }
    out << "  ],\n";
    out << "  \"objects\": [\n";
    for (size_t i = 0; i < data.objects.size(); ++i) {
        const SaveObjectState& o = data.objects[i];
        out << "    {\n";
        out << "      \"objectId\": \"" << escapeJson(o.objectId) << "\",\n";
        out << "      \"archetype\": \"" << escapeJson(o.archetype) << "\",\n";
        out << "      \"posX\": " << o.transform.position.x << ", \"posY\": " << o.transform.position.y << ", \"posZ\": " << o.transform.position.z << ",\n";
        out << "      \"rotX\": " << o.transform.rotationDeg.x << ", \"rotY\": " << o.transform.rotationDeg.y << ", \"rotZ\": " << o.transform.rotationDeg.z << ",\n";
        out << "      \"scaleX\": " << o.transform.scale.x << ", \"scaleY\": " << o.transform.scale.y << ", \"scaleZ\": " << o.transform.scale.z << "\n";
        out << "    }" << (i + 1 < data.objects.size() ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return writeAllText(path, out.str(), error);
}

bool loadDefaultSaveGame(const std::string& path, SaveGameData& data, std::string* error) {
    const std::string text = readAllText(path);
    if (text.empty()) {
        if (error) *error = "Could not read save file: " + path;
        return false;
    }
    data = SaveGameData{};
    data.version = readIntKey(text, "version", 1);
    data.saveId = readStringKey(text, "saveId", "DefaultSave");
    data.scenePath = readStringKey(text, "scenePath", "");

    for (const std::string& block : objectBlocksInArray(text, "objects")) {
        SaveObjectState o;
        o.objectId = readStringKey(block, "objectId", "");
        o.archetype = readStringKey(block, "archetype", "");
        o.transform.position = readVec3Keys(block, "pos", {0,0,0});
        o.transform.rotationDeg = readVec3Keys(block, "rot", {0,0,0});
        o.transform.scale = readVec3Keys(block, "scale", {1,1,1});
        if (!o.objectId.empty()) data.objects.push_back(o);
    }
    return true;
}

bool saveVoxelStructure(const std::string& path, const VoxelStructureDefinition& structure, std::string* error) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"type\": \"AXVoxelStructure\",\n";
    out << "  \"version\": 1,\n";
    out << "  \"id\": \"" << escapeJson(structure.id) << "\",\n";
    out << "  \"name\": \"" << escapeJson(structure.name) << "\",\n";
    out << "  \"blendMode\": \"" << voxelBlendModeToString(structure.blendMode) << "\",\n";
    out << "  \"blendRadius\": " << structure.blendRadius << ",\n";
    out << "  \"spawnWeight\": " << structure.spawnWeight << ",\n";
    out << "  \"blocks\": [\n";
    for (size_t i = 0; i < structure.blocks.size(); ++i) {
        const VoxelBlockEntry& b = structure.blocks[i];
        out << "    { \"x\": " << b.x << ", \"y\": " << b.y << ", \"z\": " << b.z << ", \"blockId\": " << b.blockId << " }";
        out << (i + 1 < structure.blocks.size() ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return writeAllText(path, out.str(), error);
}

bool loadVoxelStructure(const std::string& path, VoxelStructureDefinition& structure, std::string* error) {
    const std::string text = readAllText(path);
    if (text.empty()) {
        if (error) *error = "Could not read voxel structure: " + path;
        return false;
    }
    structure = VoxelStructureDefinition{};
    structure.id = readStringKey(text, "id", path);
    structure.name = readStringKey(text, "name", "VoxelStructure");
    structure.blendMode = voxelBlendModeFromString(readStringKey(text, "blendMode", "TerrainBlend"));
    structure.blendRadius = readFloatKey(text, "blendRadius", 4.0f);
    structure.spawnWeight = readFloatKey(text, "spawnWeight", 1.0f);

    for (const std::string& block : objectBlocksInArray(text, "blocks")) {
        VoxelBlockEntry b;
        b.x = readIntKey(block, "x", 0);
        b.y = readIntKey(block, "y", 0);
        b.z = readIntKey(block, "z", 0);
        b.blockId = readIntKey(block, "blockId", 1);
        structure.blocks.push_back(b);
    }
    return true;
}

} // namespace ax
