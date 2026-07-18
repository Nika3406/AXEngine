#include "runtime/AXComponentModel.h"

#include <algorithm>
#include <cstdlib>

namespace ax {

void ComponentRegistry::registerComponentType(const std::string& type, const std::string& category, const std::string& description) {
    entries_[type] = Entry{category, description};
}

bool ComponentRegistry::hasComponentType(const std::string& type) const {
    return entries_.find(type) != entries_.end();
}

std::vector<std::string> ComponentRegistry::componentTypes() const {
    std::vector<std::string> out;
    out.reserve(entries_.size());
    for (const auto& kv : entries_) out.push_back(kv.first);
    std::sort(out.begin(), out.end());
    return out;
}

std::string ComponentRegistry::categoryFor(const std::string& type) const {
    auto it = entries_.find(type);
    return it == entries_.end() ? std::string() : it->second.category;
}

std::string ComponentRegistry::descriptionFor(const std::string& type) const {
    auto it = entries_.find(type);
    return it == entries_.end() ? std::string() : it->second.description;
}

ComponentRegistry ComponentRegistry::makeDefaultRuntimeRegistry() {
    ComponentRegistry r;
    r.registerComponentType("Transform", "Core", "Position, rotation, and scale. Always present on scene objects.");
    r.registerComponentType("StaticMeshRenderer", "Rendering", "Draws a static GLB mesh referenced by sourcePath or mesh property.");
    r.registerComponentType("SkinnedMeshRenderer", "Rendering", "Draws a rigged GLB and exposes animations/bones/sockets.");
    r.registerComponentType("Camera", "Rendering", "Runtime or editor camera component.");
    r.registerComponentType("DirectionalLight", "Rendering", "Sun-like light source for render profiles and shadows.");
    r.registerComponentType("PointLight", "Rendering", "Local light source.");
    r.registerComponentType("BoxCollider", "Physics", "Simple box collider.");
    r.registerComponentType("MeshCollider", "Physics", "Mesh collider, often imported from COLLIDER_* nodes.");
    r.registerComponentType("TriggerVolume", "Gameplay", "Non-solid volume for events, rooms, doors, cameras, or fights.");
    r.registerComponentType("EnemyAI", "Gameplay", "Enemy behavior selector and basic stats.");
    r.registerComponentType("GunnerAim", "Gameplay", "Upper body/gun aiming metadata using sockets/bones.");
    r.registerComponentType("ProjectileEmitter", "Gameplay", "Projectile spawn and cooldown metadata.");
    r.registerComponentType("ScriptComponent", "Gameplay", "User-authored script hook. Never generated over hand-written code.");
    r.registerComponentType("SocketBinding", "Animation", "Attaches a helper socket to a bone/node.");
    r.registerComponentType("SwordTrail", "Animation", "Trail generated between two sockets/helper bones.");
    r.registerComponentType("ProceduralAnimator", "Animation", "Runtime procedural animation layer: aim, look-at, recoil, additive pose.");
    r.registerComponentType("HitboxTimeline", "Combat", "Frame/time-based hitboxes and cancel windows for DMC-style moves.");
    r.registerComponentType("AttackCollider", "Combat", "One big authored attack volume: box/sphere/capsule/cylinder, active timing, damage, hitstop, knockback.");
    r.registerComponentType("Hurtbox", "Combat", "Target body volume that can be hit by attack colliders.");
    r.registerComponentType("CombatActor", "Combat", "Runtime combat state holder: faction, invulnerability, style, stagger, hit memory.");
    r.registerComponentType("ComboMove", "Combat", "Action move definition: animation, root motion, damage, VFX, trail events.");
    r.registerComponentType("LODGroup", "Optimization", "Chooses renderer/detail level by camera distance.");
    r.registerComponentType("VoxelTerrain", "Voxel", "Procedural voxel world settings and chunk/LOD configuration.");
    r.registerComponentType("ProceduralGenerator", "Procedural", "Generates objects/props/terrain from seed/rules.");
    r.registerComponentType("NavRegion", "AI", "Navigation/encounter region metadata.");
    return r;
}

std::string getProperty(const ComponentDefinition& component, const std::string& name, const std::string& fallback) {
    for (const auto& p : component.properties) {
        if (p.name == name) return p.value;
    }
    return fallback;
}

float getFloatProperty(const ComponentDefinition& component, const std::string& name, float fallback) {
    std::string s = getProperty(component, name, "");
    if (s.empty()) return fallback;
    char* end = nullptr;
    float v = std::strtof(s.c_str(), &end);
    return end == s.c_str() ? fallback : v;
}

bool getBoolProperty(const ComponentDefinition& component, const std::string& name, bool fallback) {
    std::string s = getProperty(component, name, "");
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    if (s == "true" || s == "1" || s == "yes") return true;
    if (s == "false" || s == "0" || s == "no") return false;
    return fallback;
}

} // namespace ax
