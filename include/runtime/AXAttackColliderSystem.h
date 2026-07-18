#pragma once

#include <string>
#include <vector>
#include <unordered_set>

namespace ax {

struct AttackVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

enum class AttackColliderShape {
    Box,
    Sphere,
    Capsule,
    Cylinder
};

enum class AttackColliderDirection {
    Forward,
    Up,
    Down,
    Left,
    Right,
    Back,
    Custom
};

struct AttackColliderVolume {
    std::string name = "Main";
    AttackColliderShape shape = AttackColliderShape::Box;
    AttackColliderDirection direction = AttackColliderDirection::Forward;

    // Authored relative to the attacking character. Z+ is forward by engine convention.
    AttackVec3 offset = {0.0f, 1.0f, 1.8f};
    AttackVec3 rotation = {0.0f, 0.0f, 0.0f};
    AttackVec3 size = {3.0f, 1.4f, 2.0f};
    float radius = 1.0f;
    float height = 2.0f;

    float activeFrom = 0.10f;
    float activeTo = 0.25f;

    float damage = 10.0f;
    float hitstop = 0.04f;
    AttackVec3 knockback = {0.0f, 1.0f, 4.0f};
    bool launch = false;
    bool knockdown = false;
    bool hitOncePerTarget = true;
};

struct AttackDefinition {
    std::string name = "UnnamedAttack";
    float duration = 0.60f;
    std::vector<AttackColliderVolume> colliders;
};

struct Hurtbox {
    std::string targetId;
    AttackVec3 center;
    AttackVec3 halfExtents = {0.5f, 1.0f, 0.5f};
    bool active = true;
};

struct AttackHitResult {
    std::string attackName;
    std::string colliderName;
    std::string targetId;
    float damage = 0.0f;
    float hitstop = 0.0f;
    AttackVec3 knockback;
    bool launch = false;
    bool knockdown = false;
};

class AttackColliderSystem {
public:
    bool loadAttackFile(const std::string& path, AttackDefinition& outAttack, std::string* error = nullptr) const;
    bool saveAttackFile(const std::string& path, const AttackDefinition& attack, std::string* error = nullptr) const;

    std::vector<const AttackColliderVolume*> activeColliders(const AttackDefinition& attack, float timeSeconds) const;

    // Simple action-game volume evaluation. This intentionally uses authored big volumes,
    // not exact sword-blade tracing. Good enough for DMC-style prototype combat.
    std::vector<AttackHitResult> evaluate(
        const AttackDefinition& attack,
        float timeSeconds,
        const AttackVec3& attackerPosition,
        float attackerYawDegrees,
        const std::vector<Hurtbox>& hurtboxes
    );

    void resetHitMemory();

    static const char* shapeToString(AttackColliderShape shape);
    static AttackColliderShape shapeFromString(const std::string& shape);
    static const char* directionToString(AttackColliderDirection direction);
    static AttackColliderDirection directionFromString(const std::string& direction);

private:
    std::unordered_set<std::string> hitMemory_;
};

AttackVec3 authoredOffsetToWorld(const AttackVec3& attackerPosition, float attackerYawDegrees, const AttackVec3& localOffset);
bool pointInsideAttackVolume(const AttackColliderVolume& collider, const AttackVec3& attackerPosition, float attackerYawDegrees, const AttackVec3& point);

} // namespace ax
