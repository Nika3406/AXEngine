#include "ecs/ColliderComponent.h"
#include "ecs/Entity.h"

AABB ColliderComponent::getWorldAABB() const {
    // Pull position from the entity's transform
    Vec3 pos = {0, 0, 0};
    if (owner) {
        pos = owner->transform()->position;
    }
    return AABB::fromCenter(pos, halfExtents);
}
