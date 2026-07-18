#pragma once

#include "AXEngine/Core.h"

#include <cstdint>

namespace ax::vfx {

// AXVFXKit is intentionally closer to old Unreal Cascade than Niagara:
// readable asset definitions, simple emitters, Blender-made mesh/sprite inputs,
// and gameplay-event spawning.

enum class VFXKind {
    DebugSpark,
    Mesh,
    Sprite,
    Trail,
    ParticleBurst,
    ScreenFlash,
    UI
};

enum class VFXFacingMode {
    World,
    FollowDirection,
    BillboardCamera,
    AttachToSocket
};

struct Curve3 {
    Vec3 start{1.0f, 1.0f, 1.0f};
    Vec3 mid{1.0f, 1.0f, 1.0f};
    Vec3 end{1.0f, 1.0f, 1.0f};

    Vec3 sample(float t) const;
};

struct Curve1 {
    float start = 1.0f;
    float mid = 1.0f;
    float end = 0.0f;

    float sample(float t) const;
};

struct VFXDefinition {
    String name;
    VFXKind kind = VFXKind::DebugSpark;

    // Blender-first VFX: mesh can be a .glb/.gltf object authored in Blender.
    // AXEngine owns lifetime, transform, tint, event spawning and playback.
    String meshPath;
    String spritePath;
    String materialPath;

    float lifetime = 0.25f;
    bool loop = false;
    bool spawnOnce = true;
    bool debugFallback = true;

    VFXFacingMode facing = VFXFacingMode::FollowDirection;
    Vec3 localOffset{};
    Vec3 localRotationDeg{};
    Vec3 baseScale{1.0f, 1.0f, 1.0f};
    Curve3 scaleOverLife{};
    Curve1 alphaOverLife{};
    Color tint{1.0f, 0.85f, 0.25f, 1.0f};

    // Optional socket/bone attachment name. The gameplay side decides how to map
    // this to an actual skeleton socket; runtime stores the intent clearly.
    String attachTo;
};

struct SpawnRequest {
    String asset;       // path to .axvfx, usually assets/vfx/Foo.axvfx
    String name;        // fallback/easy name when no asset is supplied
    Vec3 position{};
    Vec3 rotationDeg{};
    Vec3 direction{0.0f, 0.0f, 1.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};
    Color tint{1.0f, 1.0f, 1.0f, 1.0f};
    String socket;
    String ownerId;
    float intensity = 1.0f;
};

struct VFXInstance {
    uint64_t id = 0;
    VFXDefinition definition{};
    String ownerId;
    String socket;
    Vec3 position{};
    Vec3 direction{0.0f, 0.0f, 1.0f};
    Vec3 rotationDeg{};
    Vec3 requestScale{1.0f, 1.0f, 1.0f};
    Color requestTint{1.0f, 1.0f, 1.0f, 1.0f};
    float age = 0.0f;
    float intensity = 1.0f;
    bool alive = true;
};

struct VFXRenderCommand {
    uint64_t id = 0;
    String name;
    String meshPath;
    String spritePath;
    String materialPath;
    Transform transform{};
    Color tint{1.0f, 1.0f, 1.0f, 1.0f};
    float normalizedTime = 0.0f;
    bool meshBacked = false;
    bool spriteBacked = false;
};

struct VFXDebugLine {
    Vec3 a{};
    Vec3 b{};
    Color color{1.0f, 1.0f, 1.0f, 1.0f};
};

class VFXRuntime {
public:
    bool loadDefinition(const String& path, VFXDefinition& outDefinition) const;
    uint64_t spawn(const SpawnRequest& request);
    void update(float dt);
    void clear();

    const Array<VFXInstance>& instances() const { return instances_; }
    Array<VFXRenderCommand> renderCommands() const;
    Array<VFXDebugLine> debugLines() const;

private:
    VFXDefinition fallbackDefinition(const SpawnRequest& request) const;

    Array<VFXInstance> instances_;
    uint64_t nextId_ = 1;
};

VFXRuntime& runtime();

// Convenience helpers for user scripts.
uint64_t spawnMeshVFX(const String& axvfxPath, const Vec3& position, const Vec3& direction, float scale = 1.0f);
uint64_t spawnHitSpark(const Vec3& position, const Vec3& direction, const Color& tint, float radius = 1.0f);

namespace Events {
    inline constexpr const char* VFXSpawned = "OnVFXSpawned";
    inline constexpr const char* VFXFinished = "OnVFXFinished";
}

} // namespace ax::vfx
