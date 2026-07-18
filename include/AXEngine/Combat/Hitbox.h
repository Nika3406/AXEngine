#pragma once

#include "AXEngine/Combat/DamagePacket.h"

namespace ax::combat {

struct Hitbox {
    String id;
    String ownerId;
    String attackName;
    HitShape shape = HitShape::Sphere;

    Vec3 center{};
    Vec3 halfExtents{0.5f, 0.5f, 0.5f};
    Vec3 forward{0.0f, 0.0f, 1.0f};

    float radius = 0.5f;
    float height = 1.0f;
    float range = 1.0f;
    float arcCos = 0.0f;

    float activeStart = 0.0f;
    float activeEnd = 1.0f;
    bool hitOncePerTarget = true;

    DamagePacket damage;

    bool activeAt(float normalizedTime) const {
        return normalizedTime >= activeStart && normalizedTime <= activeEnd;
    }
};

} // namespace ax::combat
