#pragma once

#include "runtime/AXComponentModel.h"
#include <string>
#include <vector>

namespace ax {

struct SocketDefinition {
    std::string name;
    std::string bone;
    Vec3 position;
    Vec3 rotation;
    Vec3 scale = {1.0f, 1.0f, 1.0f};
    std::string shape = "Diamond";
};

struct SwordTrailDefinition {
    std::string name = "SwordTrail";
    std::string startSocket = "Blade_Base";
    std::string endSocket = "Blade_Tip";
    std::string material = "BLOOM_Trail";
    float lifetime = 0.18f;
    float width = 0.28f;
    bool enabledByTimeline = true;
};

struct ProceduralAimDefinition {
    std::string aimRoot = "Chest";
    std::string muzzleSocket = "Muzzle";
    std::string handBone = "RightHand";
    float maxPitchUp = 65.0f;
    float maxPitchDown = -45.0f;
    float maxYaw = 75.0f;
    float blendSpeed = 8.0f;
};

struct ProceduralPoseState {
    float aimYaw = 0.0f;
    float aimPitch = 0.0f;
    float aimWeight = 0.0f;
};

class ProceduralAnimationSystem {
public:
    static ProceduralAimDefinition aimFromComponent(const ComponentDefinition& component);
    static SwordTrailDefinition trailFromComponent(const ComponentDefinition& component);
    static SocketDefinition socketFromComponent(const ComponentDefinition& component);

    static void updateAimState(ProceduralPoseState& state,
                               const Vec3& actorPosition,
                               float actorYawDeg,
                               const Vec3& targetPosition,
                               const ProceduralAimDefinition& aim,
                               float dt);
};

} // namespace ax
