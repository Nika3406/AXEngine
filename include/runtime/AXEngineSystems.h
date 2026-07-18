#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace ax {

struct AssetHeader {
    std::string type;
    std::string assetId;
    int version = 1;
};

std::string makeStableAssetId(const std::string& type, const std::string& pathOrName);

struct MaterialDefinition {
    AssetHeader header{"AXMaterial", "", 1};
    std::string renderMode = "Stylized"; // Realistic, Stylized, Cartoon, VFX Preview
    std::string vertexShader = "assets/shaders/stylized_lit.vert";
    std::string fragmentShader = "assets/shaders/stylized_lit.frag";
    float baseColor[4] = {1,1,1,1};
    float emission[3] = {0,0,0};
    float roughness = 0.55f;
    float metallic = 0.0f;
    bool cel = false;
    bool outline = false;
    bool bloom = false;
};

struct InputBinding { std::string action; std::string keyboard; std::string gamepad; };
struct InputMapDefinition {
    AssetHeader header{"AXInput", "", 1};
    bool bufferEnabled = true;
    float bufferSeconds = 0.18f;
    std::vector<InputBinding> bindings;
};

struct TimelineEvent { float timeSeconds = 0.0f; std::string type; std::string payload; };
struct AnimationTimelineDefinition {
    AssetHeader header{"AXAnimationTimeline", "", 1};
    std::string clip = "Attack_01";
    float fps = 60.0f;
    float duration = 1.0f;
    std::vector<TimelineEvent> events;
};

struct VFXDefinition {
    AssetHeader header{"AXVFX", "", 1};
    std::string kind = "SlashMeshVFX";
    std::string mesh;
    std::string material;
    float lifetime = 0.25f;
    bool billboard = false;
    bool bloom = true;
};

struct EnemyAttackDefinition {
    std::string name = "BasicAttack";
    float range = 1.55f;
    float cooldown = 1.25f;
    float windup = 0.32f;
    float activeEnd = 0.52f;
    int damage = 10;
};

struct EnemyDefinition {
    AssetHeader header{"AXEnemy", "", 1};
    std::string archetype = "Basic";
    std::string model = "assets/Man1_Cinema.glb";
    int hp = 160;
    float radius = 0.48f;
    float height = 1.85f;
    float detectionRange = 18.0f;
    float preferredRange = 9.0f;
    float attackCooldown = 1.4f;
    float moveSpeed = 2.65f;
    std::vector<EnemyAttackDefinition> attacks;
};

struct PlatformInfo {
    std::string os;
    std::string compiler;
    std::string buildType;
};

PlatformInfo detectPlatformInfo();

bool loadInputMapFile(const std::string& path, InputMapDefinition& out, std::string* error = nullptr);
bool loadEnemyFile(const std::string& path, EnemyDefinition& out, std::string* error = nullptr);
bool loadVFXFile(const std::string& path, VFXDefinition& out, std::string* error = nullptr);
std::string firstKeyboardToken(const std::string& keyboardSpec);

struct PerfCounters {
    double cpuMs = 0.0;
    double gpuMs = 0.0;
    int drawCalls = 0;
    int visibleObjects = 0;
    int activeAttackVolumes = 0;
    int activeVfx = 0;
};

} // namespace ax
