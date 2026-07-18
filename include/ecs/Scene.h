#pragma once
#include "ecs/Entity.h"
#include "ecs/ColliderComponent.h"
#include <vector>
#include <memory>
#include <string>

class Scene {
public:
    // Create and own an entity
    Entity* createEntity(const std::string& name = "Entity");

    // Remove by pointer (safe to call during update)
    void destroyEntity(Entity* entity);

    void onStart();
    void onUpdate(float dt);

    const std::vector<std::unique_ptr<Entity>>& entities() const { return entities_; }

private:
    std::vector<std::unique_ptr<Entity>> entities_;
    std::vector<Entity*>                 pendingDestroy_;

    void resolveCollisions();
    void flushDestroyQueue();
};
