#include "runtime/AXProceduralAnimation.h"

#include <algorithm>
#include <cmath>

namespace ax {
namespace {
static constexpr float PI = 3.14159265358979323846f;
static float wrapDegrees(float v) {
    while (v > 180.0f) v -= 360.0f;
    while (v < -180.0f) v += 360.0f;
    return v;
}
static float smoothToward(float current, float target, float dt, float speed) {
    float t = std::clamp(dt * speed, 0.0f, 1.0f);
    return current + (target - current) * t;
}
}

ProceduralAimDefinition ProceduralAnimationSystem::aimFromComponent(const ComponentDefinition& component) {
    ProceduralAimDefinition a;
    a.aimRoot = getProperty(component, "aimRoot", a.aimRoot);
    a.muzzleSocket = getProperty(component, "muzzleSocket", getProperty(component, "muzzleBone", a.muzzleSocket));
    a.handBone = getProperty(component, "handBone", a.handBone);
    a.maxPitchUp = getFloatProperty(component, "maxPitchUp", getFloatProperty(component, "aimPitchUp", a.maxPitchUp));
    a.maxPitchDown = getFloatProperty(component, "maxPitchDown", getFloatProperty(component, "aimPitchDown", a.maxPitchDown));
    a.maxYaw = getFloatProperty(component, "maxYaw", getFloatProperty(component, "aimYaw", a.maxYaw));
    a.blendSpeed = getFloatProperty(component, "blendSpeed", a.blendSpeed);
    return a;
}

SwordTrailDefinition ProceduralAnimationSystem::trailFromComponent(const ComponentDefinition& component) {
    SwordTrailDefinition t;
    t.name = getProperty(component, "name", t.name);
    t.startSocket = getProperty(component, "startSocket", t.startSocket);
    t.endSocket = getProperty(component, "endSocket", t.endSocket);
    t.material = getProperty(component, "material", t.material);
    t.lifetime = getFloatProperty(component, "lifetime", t.lifetime);
    t.width = getFloatProperty(component, "width", t.width);
    t.enabledByTimeline = getBoolProperty(component, "enabledByTimeline", t.enabledByTimeline);
    return t;
}

SocketDefinition ProceduralAnimationSystem::socketFromComponent(const ComponentDefinition& component) {
    SocketDefinition s;
    s.name = getProperty(component, "name", s.name);
    s.bone = getProperty(component, "bone", s.bone);
    s.shape = getProperty(component, "shape", s.shape);
    s.position.x = getFloatProperty(component, "posX", 0.0f);
    s.position.y = getFloatProperty(component, "posY", 0.0f);
    s.position.z = getFloatProperty(component, "posZ", 0.0f);
    s.rotation.x = getFloatProperty(component, "rotX", 0.0f);
    s.rotation.y = getFloatProperty(component, "rotY", 0.0f);
    s.rotation.z = getFloatProperty(component, "rotZ", 0.0f);
    return s;
}

void ProceduralAnimationSystem::updateAimState(ProceduralPoseState& state,
                                               const Vec3& actorPosition,
                                               float actorYawDeg,
                                               const Vec3& targetPosition,
                                               const ProceduralAimDefinition& aim,
                                               float dt) {
    float dx = targetPosition.x - actorPosition.x;
    float dy = targetPosition.y - actorPosition.y;
    float dz = targetPosition.z - actorPosition.z;
    float horizontal = std::sqrt(dx * dx + dz * dz);
    float targetYaw = std::atan2(dx, dz) * 180.0f / PI;
    float localYaw = std::clamp(wrapDegrees(targetYaw - actorYawDeg), -aim.maxYaw, aim.maxYaw);
    float pitch = std::atan2(dy, std::max(horizontal, 0.001f)) * 180.0f / PI;
    pitch = std::clamp(pitch, aim.maxPitchDown, aim.maxPitchUp);
    state.aimYaw = smoothToward(state.aimYaw, localYaw, dt, aim.blendSpeed);
    state.aimPitch = smoothToward(state.aimPitch, pitch, dt, aim.blendSpeed);
    state.aimWeight = smoothToward(state.aimWeight, 1.0f, dt, aim.blendSpeed);
}

} // namespace ax
