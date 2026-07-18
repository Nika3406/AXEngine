#pragma once
#include "ecs/Component.h"
#include "ecs/TransformComponent.h"
#include <functional>

struct AABB {
    Vec3 min;
    Vec3 max;

    bool intersects(const AABB& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
        (min.y <= other.max.y && max.y >= other.min.y) &&
        (min.z <= other.max.z && max.z >= other.min.z);
    }

    // World-space AABB from a half-extent and a position
    static AABB fromCenter(const Vec3& center, const Vec3& halfExtents) {
        return {
            {center.x - halfExtents.x, center.y - halfExtents.y, center.z - halfExtents.z},
            {center.x + halfExtents.x, center.y + halfExtents.y, center.z + halfExtents.z}
        };
    }
};

class ColliderComponent : public Component {
public:
    Vec3 halfExtents = {0.5f, 0.5f, 0.5f}; // box size in each direction
    bool isTrigger   = false; // true = overlap event only, no resolution

    // Fired when this collider overlaps another — set this from game code
    std::function<void(Entity*)> onCollision;

    AABB getWorldAABB() const;
};
