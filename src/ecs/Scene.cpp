#include "ecs/Scene.h"
#include <algorithm>

Entity* Scene::createEntity(const std::string& name) {
    auto e  = std::make_unique<Entity>();
    e->name = name;
    Entity* raw = e.get();
    entities_.push_back(std::move(e));
    return raw;
}

void Scene::destroyEntity(Entity* entity) {
    pendingDestroy_.push_back(entity);
}

void Scene::onStart() {
    for (auto& e : entities_) e->onStart();
}

void Scene::onUpdate(float dt) {
    for (auto& e : entities_) e->onUpdate(dt);
    resolveCollisions();
    flushDestroyQueue();
}

void Scene::resolveCollisions() {
    // Collect entities that have a collider
    std::vector<Entity*> collidables;
    for (auto& e : entities_) {
        if (e->active && e->hasComponent<ColliderComponent>())
            collidables.push_back(e.get());
    }

    // O(n²) broadphase — fine for small entity counts
    for (size_t i = 0; i < collidables.size(); ++i) {
        for (size_t j = i + 1; j < collidables.size(); ++j) {
            Entity* a = collidables[i];
            Entity* b = collidables[j];

            auto* ca = a->getComponent<ColliderComponent>();
            auto* cb = b->getComponent<ColliderComponent>();

            AABB boxA = ca->getWorldAABB();
            AABB boxB = cb->getWorldAABB();

            if (!boxA.intersects(boxB)) continue;

            // Fire callbacks on both sides
            if (ca->onCollision) ca->onCollision(b);
            if (cb->onCollision) cb->onCollision(a);

            // Solid collision resolution (non-trigger only)
            if (!ca->isTrigger && !cb->isTrigger) {
                // Find the shallowest penetration axis and push apart
                Vec3 posA = a->transform()->position;
                Vec3 posB = b->transform()->position;

                float overlapX = std::min(boxA.max.x, boxB.max.x) - std::max(boxA.min.x, boxB.min.x);
                float overlapY = std::min(boxA.max.y, boxB.max.y) - std::max(boxA.min.y, boxB.min.y);
                float overlapZ = std::min(boxA.max.z, boxB.max.z) - std::max(boxA.min.z, boxB.min.z);

                float half = 0.5f;

                if (overlapX <= overlapY && overlapX <= overlapZ) {
                    float dir = (posA.x < posB.x) ? -1.0f : 1.0f;
                    a->transform()->position.x += dir * overlapX * half;
                    b->transform()->position.x -= dir * overlapX * half;
                } else if (overlapY <= overlapX && overlapY <= overlapZ) {
                    float dir = (posA.y < posB.y) ? -1.0f : 1.0f;
                    a->transform()->position.y += dir * overlapY * half;
                    b->transform()->position.y -= dir * overlapY * half;
                } else {
                    float dir = (posA.z < posB.z) ? -1.0f : 1.0f;
                    a->transform()->position.z += dir * overlapZ * half;
                    b->transform()->position.z -= dir * overlapZ * half;
                }
            }
        }
    }
}

void Scene::flushDestroyQueue() {
    for (Entity* e : pendingDestroy_) {
        entities_.erase(
            std::remove_if(entities_.begin(), entities_.end(),
                           [e](const std::unique_ptr<Entity>& ptr) { return ptr.get() == e; }),
                        entities_.end()
        );
    }
    pendingDestroy_.clear();
}
