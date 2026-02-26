// include/ecs/EntityBuilder.hpp
#pragma once

#include "Components.hpp"
#include "assets/AssetSystem.hpp"
#include <entt/entt.hpp>
#include <string>
#include <iostream>
#include <algorithm>

namespace ecs {

// ============================================================================
// Entity Builder - Fluent API for Entity Creation
// ============================================================================
class Entity {
public:
    Entity(entt::registry& registry, assets::AssetSystem& assets, bgfx::ProgramHandle defaultProgram)
        : m_registry(registry)
        , m_assets(assets)
        , m_defaultProgram(defaultProgram)
        , m_entity(registry.create()) {
    }

    // ========================================================================
    // Identification
    // ========================================================================
    
    Entity& tag(const std::string& name) {
        m_registry.emplace<Tag>(m_entity, name);
        return *this;
    }

    // ========================================================================
    // Transform
    // ========================================================================
    
    Entity& position(float x, float y, float z) {
        getOrCreateTransform().position = {x, y, z};
        return *this;
    }

    Entity& position(const bx::Vec3& pos) {
        getOrCreateTransform().position = pos;
        return *this;
    }

    Entity& rotation(float x, float y, float z) {
        getOrCreateTransform().rotation = {x, y, z};
        return *this;
    }

    Entity& rotation(const bx::Vec3& rot) {
        getOrCreateTransform().rotation = rot;
        return *this;
    }

    Entity& scale(float x, float y, float z) {
        getOrCreateTransform().scale = {x, y, z};
        return *this;
    }

    Entity& scale(const bx::Vec3& s) {
        getOrCreateTransform().scale = s;
        return *this;
    }

    Entity& scale(float uniformScale) {
        getOrCreateTransform().scale = {uniformScale, uniformScale, uniformScale};
        return *this;
    }

    // ========================================================================
    // Mesh & Rendering
    // ========================================================================
    
    Entity& mesh(const std::string& meshName) {
        auto* meshPtr = m_assets.getMesh(meshName);
        if (!meshPtr) {
            std::cerr << "Warning: Mesh '" << meshName << "' not found\n";
            return *this;
        }

        m_currentMesh = meshPtr;

        auto& renderer = m_registry.emplace<MeshRenderer>(m_entity);
        renderer.meshName = meshName;
        renderer.vbh = meshPtr->vbh;
        renderer.ibh = meshPtr->ibh;
        renderer.program = m_defaultProgram;
        renderer.color = {1.0f, 1.0f, 1.0f};

        return *this;
    }

    Entity& color(float r, float g, float b) {
        if (auto* renderer = m_registry.try_get<MeshRenderer>(m_entity)) {
            renderer->color = {r, g, b};
        }
        return *this;
    }

    Entity& color(const bx::Vec3& c) {
        return color(c.x, c.y, c.z);
    }

    // ========================================================================
    // Collision
    // ========================================================================
    
    Entity& boxCollider(float halfX, float halfY, float halfZ, bool isStatic = true) {
        m_registry.emplace<ColliderAABB>(m_entity, ColliderAABB{
            {halfX, halfY, halfZ},
            isStatic
        });
        return *this;
    }

    Entity& boxCollider(const bx::Vec3& halfExtents, bool isStatic = true) {
        return boxCollider(halfExtents.x, halfExtents.y, halfExtents.z, isStatic);
    }

    Entity& boxCollider(float halfSize, bool isStatic = true) {
        return boxCollider(halfSize, halfSize, halfSize, isStatic);
    }

    Entity& sphereCollider(float radius) {
        m_registry.emplace<ColliderSphere>(m_entity, ColliderSphere{radius});
        return *this;
    }

    Entity& capsuleCollider(float radius, float height) {
        m_registry.emplace<ColliderCapsule>(m_entity, ColliderCapsule{radius, height});
        return *this;
    }

    // Auto-generate box collider from mesh bounds
    Entity& autoBoxCollider(bool isStatic = true) {
        if (!m_currentMesh) {
            std::cerr << "Warning: Cannot auto-generate collider - no mesh set\n";
            return *this;
        }

        // Get scale from transform
        float scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;
        if (auto* t = m_registry.try_get<Transform>(m_entity)) {
            scaleX = t->scale.x;
            scaleY = t->scale.y;
            scaleZ = t->scale.z;
        }

        float hx = (m_currentMesh->maxX - m_currentMesh->minX) * 0.5f * scaleX;
        float hy = (m_currentMesh->maxY - m_currentMesh->minY) * 0.5f * scaleY;
        float hz = (m_currentMesh->maxZ - m_currentMesh->minZ) * 0.5f * scaleZ;

        return boxCollider(hx, hy, hz, isStatic);
    }

    // Auto-generate sphere collider from mesh bounds (uses largest dimension as radius)
    Entity& autoSphereCollider() {
        if (!m_currentMesh) {
            std::cerr << "Warning: Cannot auto-generate collider - no mesh set\n";
            return *this;
        }

        float dx = m_currentMesh->maxX - m_currentMesh->minX;
        float dy = m_currentMesh->maxY - m_currentMesh->minY;
        float dz = m_currentMesh->maxZ - m_currentMesh->minZ;
        float radius = std::max({dx, dy, dz}) * 0.5f;

        // Apply scale
        if (auto* t = m_registry.try_get<Transform>(m_entity)) {
            float maxScale = std::max({t->scale.x, t->scale.y, t->scale.z});
            radius *= maxScale;
        }

        return sphereCollider(radius);
    }

    // ========================================================================
    // Physics
    // ========================================================================
    
    Entity& rigidbody(float mass = 1.0f, bool useGravity = true) {
        auto& rb = m_registry.emplace<Rigidbody>(m_entity);
        rb.mass = mass;
        rb.useGravity = useGravity;
        return *this;
    }

    // ========================================================================
    // Camera
    // ========================================================================
    
    Entity& camera(float fov = 75.0f, float nearPlane = 0.1f, float farPlane = 1000.0f) {
        auto& cam = m_registry.emplace<Camera>(m_entity);
        cam.type = Camera::Type::Perspective;
        cam.fov = fov;
        cam.nearPlane = nearPlane;
        cam.farPlane = farPlane;
        cam.isActive = true;
        cam.target = {0.0f, 0.0f, -1.0f};
        cam.up = {0.0f, 1.0f, 0.0f};
        return *this;
    }

    Entity& lookAt(const bx::Vec3& direction) {
        if (auto* cam = m_registry.try_get<Camera>(m_entity)) {
            cam->target = direction;
        }
        return *this;
    }

    Entity& lookAt(float x, float y, float z) {
        return lookAt({x, y, z});
    }

    // ========================================================================
    // Lights
    // ========================================================================
    
    Entity& directionalLight(const bx::Vec3& color = {1.0f, 1.0f, 1.0f}, float intensity = 1.0f) {
        auto& light = m_registry.emplace<Light>(m_entity);
        light.type = Light::Type::Directional;
        light.color = color;
        light.intensity = intensity;
        return *this;
    }

    Entity& pointLight(const bx::Vec3& color = {1.0f, 1.0f, 1.0f}, float intensity = 1.0f, float range = 10.0f) {
        auto& light = m_registry.emplace<Light>(m_entity);
        light.type = Light::Type::Point;
        light.color = color;
        light.intensity = intensity;
        light.range = range;
        return *this;
    }

    // ========================================================================
    // Finalize
    // ========================================================================
    
    entt::entity get() const {
        return m_entity;
    }

    operator entt::entity() const {
        return m_entity;
    }

private:
    Transform& getOrCreateTransform() {
        if (auto* t = m_registry.try_get<Transform>(m_entity)) {
            return *t;
        }
        return m_registry.emplace<Transform>(m_entity);
    }

private:
    entt::registry& m_registry;
    assets::AssetSystem& m_assets;
    bgfx::ProgramHandle m_defaultProgram;
    entt::entity m_entity;
    const assets::Mesh* m_currentMesh = nullptr;
};

// ============================================================================
// Scene - High-Level Entity Management
// ============================================================================
class Scene {
public:
    Scene(entt::registry& registry, assets::AssetSystem& assets, bgfx::ProgramHandle defaultProgram)
        : m_registry(registry)
        , m_assets(assets)
        , m_defaultProgram(defaultProgram) {
    }

    // Create new entity with builder pattern
    Entity create() {
        return Entity(m_registry, m_assets, m_defaultProgram);
    }

    // ========================================================================
    // Query & Manipulation
    // ========================================================================
    
    entt::entity findByTag(const std::string& tagName) {
        auto view = m_registry.view<Tag>();
        for (auto e : view) {
            if (view.get<Tag>(e).name == tagName) {
                return e;
            }
        }
        return entt::null;
    }

    void destroy(entt::entity e) {
        if (m_registry.valid(e)) {
            m_registry.destroy(e);
        }
    }

    void setPosition(entt::entity e, const bx::Vec3& pos) {
        if (auto* t = m_registry.try_get<Transform>(e)) {
            t->position = pos;
        }
    }

    void setRotation(entt::entity e, const bx::Vec3& rot) {
        if (auto* t = m_registry.try_get<Transform>(e)) {
            t->rotation = rot;
        }
    }

    void setScale(entt::entity e, const bx::Vec3& s) {
        if (auto* t = m_registry.try_get<Transform>(e)) {
            t->scale = s;
        }
    }

    void setColor(entt::entity e, const bx::Vec3& color) {
        if (auto* mr = m_registry.try_get<MeshRenderer>(e)) {
            mr->color = color;
        }
    }

private:
    entt::registry& m_registry;
    assets::AssetSystem& m_assets;
    bgfx::ProgramHandle m_defaultProgram;
};

} // namespace ecs