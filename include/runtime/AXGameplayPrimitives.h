#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ax {

// -----------------------------------------------------------------------------
// AXGameplayPrimitives
// -----------------------------------------------------------------------------
// This file is intentionally genre-neutral.  It does not implement a coin flip,
// RPG quest, RTS command, racing checkpoint, or boss phase directly.  It gives
// user scripts and runtime systems the common primitives those mechanics need:
// events, runtime objects, save data, 2D/world UI objects, and voxel structures.
// -----------------------------------------------------------------------------

struct Vec3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec4f {
    float x = 1.0f;
    float y = 1.0f;
    float z = 1.0f;
    float w = 1.0f;
};

struct Transform3D {
    Vec3f position{0.0f, 0.0f, 0.0f};
    Vec3f rotationDeg{0.0f, 0.0f, 0.0f};
    Vec3f scale{1.0f, 1.0f, 1.0f};
};

// -----------------------------------------------------------------------------
// Event/callback system
// -----------------------------------------------------------------------------

struct GameplayEvent {
    std::string type;                 // OnAttackHit, OnTriggerEnter, OnProjectileHit, etc.
    std::string senderId;             // object/component that emitted the event
    std::string targetId;             // optional target object
    std::string stringPayload;        // simple payload for scripts/data tools
    float numberPayload = 0.0f;       // common scalar payload: damage, score, amount
    Vec3f position{};                 // world-space event point
    Vec3f direction{};                // direction/normal/forward
    uint64_t frame = 0;               // frame event was emitted
};

class GameplayEventBus {
public:
    using Listener = std::function<void(const GameplayEvent&)>;

    int subscribe(const std::string& type, Listener listener);
    void unsubscribe(const std::string& type, int subscriptionId);

    void emit(const GameplayEvent& event);
    void queue(const GameplayEvent& event);
    void flushQueued();
    void clear();

    size_t queuedCount() const { return queued_.size(); }
    size_t listenerCount(const std::string& type) const;

private:
    struct ListenerEntry {
        int id = 0;
        Listener fn;
    };

    int nextListenerId_ = 1;
    std::unordered_map<std::string, std::vector<ListenerEntry>> listeners_;
    std::vector<GameplayEvent> queued_;
};

// -----------------------------------------------------------------------------
// Runtime object/spawn primitives
// -----------------------------------------------------------------------------

struct RuntimeComponentRecord {
    std::string type;                 // ScriptComponent, SpriteRenderer, Inventory, AIController, etc.
    std::unordered_map<std::string, std::string> properties;
};

struct RuntimeObjectRecord {
    std::string id;
    std::string name;
    std::string archetype;            // Player, Enemy, Projectile, Pickup, UIObject, VoxelStructure
    std::string assetPath;            // GLB, AXUI object, sprite, etc.
    Transform3D transform;
    bool active = true;
    std::vector<std::string> tags;
    std::vector<RuntimeComponentRecord> components;
};

class RuntimeObjectRegistry {
public:
    RuntimeObjectRecord& create(const std::string& archetype, const std::string& name);
    bool destroy(const std::string& id);
    RuntimeObjectRecord* find(const std::string& id);
    const RuntimeObjectRecord* find(const std::string& id) const;

    std::vector<RuntimeObjectRecord*> findByTag(const std::string& tag);
    std::vector<RuntimeObjectRecord*> findByArchetype(const std::string& archetype);

    const std::vector<RuntimeObjectRecord>& objects() const { return objects_; }
    void clear();

private:
    uint64_t nextId_ = 1;
    std::vector<RuntimeObjectRecord> objects_;
};

// -----------------------------------------------------------------------------
// Save/load primitives
// -----------------------------------------------------------------------------

struct SaveValue {
    std::string key;
    std::string value;
};

struct SaveObjectState {
    std::string objectId;
    std::string archetype;
    Transform3D transform;
    std::vector<SaveValue> values;
};

struct SaveGameData {
    std::string saveId = "DefaultSave";
    int version = 1;
    std::string scenePath;
    std::vector<SaveValue> globals;
    std::vector<SaveObjectState> objects;
};

class SaveGameStore {
public:
    void setGlobal(const std::string& key, const std::string& value);
    std::string getGlobal(const std::string& key, const std::string& fallback = "") const;

    SaveObjectState& upsertObject(const std::string& objectId, const std::string& archetype);
    SaveGameData& data() { return data_; }
    const SaveGameData& data() const { return data_; }

    bool saveToFile(const std::string& path, std::string* error = nullptr) const;
    bool loadFromFile(const std::string& path, std::string* error = nullptr);
    void clear();

private:
    SaveGameData data_;
};

// -----------------------------------------------------------------------------
// 2D/world UI as object components, not DAW/Blender-style authoring
// -----------------------------------------------------------------------------

enum class UIAnchor {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Center,
    WorldSpace
};

struct UIObjectDefinition {
    std::string id;
    std::string name;
    std::string kind = "Sprite";      // Sprite, Text, Bar, Panel, DamageNumber
    UIAnchor anchor = UIAnchor::TopLeft;
    Vec3f position{};                  // x/y for screen, x/y/z for world-space
    Vec3f size{128.0f, 32.0f, 0.0f};
    Vec4f color{1.0f, 1.0f, 1.0f, 1.0f};
    std::string texturePath;
    std::string text;
    bool visible = true;
};

// -----------------------------------------------------------------------------
// Voxel structure/procedural placement primitives
// -----------------------------------------------------------------------------

enum class VoxelBlendMode {
    Stamp,
    TerrainBlend,
    RuinBlend,
    CaveBlend
};

struct VoxelBlockEntry {
    int x = 0;
    int y = 0;
    int z = 0;
    int blockId = 1;
};

struct VoxelStructureDefinition {
    std::string id;
    std::string name;
    Vec3f origin{};
    Vec3f size{};
    VoxelBlendMode blendMode = VoxelBlendMode::TerrainBlend;
    float blendRadius = 4.0f;
    float spawnWeight = 1.0f;
    std::vector<std::string> allowedBiomes;
    std::vector<VoxelBlockEntry> blocks;
};

// -----------------------------------------------------------------------------
// Small data-file helpers for the new primitive files.
// These are lightweight and dependency-free. They are not meant to replace a
// full JSON library later, but they are stable enough for AX text assets.
// -----------------------------------------------------------------------------

bool saveGameplayEventsTemplate(const std::string& path, std::string* error = nullptr);
bool saveDefaultSaveGame(const std::string& path, const SaveGameData& data, std::string* error = nullptr);
bool loadDefaultSaveGame(const std::string& path, SaveGameData& data, std::string* error = nullptr);
bool saveVoxelStructure(const std::string& path, const VoxelStructureDefinition& structure, std::string* error = nullptr);
bool loadVoxelStructure(const std::string& path, VoxelStructureDefinition& structure, std::string* error = nullptr);

std::string uiAnchorToString(UIAnchor anchor);
UIAnchor uiAnchorFromString(const std::string& value);
std::string voxelBlendModeToString(VoxelBlendMode mode);
VoxelBlendMode voxelBlendModeFromString(const std::string& value);

} // namespace ax
