// include/ecs/Components.hpp
#pragma once

#include <bx/math.h>
#include <bgfx/bgfx.h>
#include <string>
#include <memory>

namespace ecs {

// ============================================================================
// Transform Component
// ============================================================================
struct Transform {
    bx::Vec3 position = {0.0f, 0.0f, 0.0f};
    bx::Vec3 rotation = {0.0f, 0.0f, 0.0f}; // Euler angles (radians)
    bx::Vec3 scale = {1.0f, 1.0f, 1.0f};

    // Get world transformation matrix
    void getMatrix(float* outMatrix) const;
};

// ============================================================================
// Camera Component
// ============================================================================
struct Camera {
    enum class Type {
        Perspective,
        Orthographic
    };

    Type type = Type::Perspective;

    // Perspective settings
    float fov = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    // Orthographic settings
    float orthoSize = 10.0f;

    // Target (look-at point) relative to entity position (direction vector)
    bx::Vec3 target = {0.0f, 0.0f, 0.0f};
    bx::Vec3 up = {0.0f, 1.0f, 0.0f};

    bool isActive = true; // Only one camera should be active

    void getViewMatrix(const bx::Vec3& position, float* outView) const;
    void getProjectionMatrix(float aspect, float* outProj) const;
};

// ============================================================================
// Mesh Renderer Component
// ============================================================================
struct MeshRenderer {
    std::string meshName;           // Asset name for the mesh
    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;

    // Material properties
    bx::Vec3 color = {1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;

    bool castShadows = true;
    bool receiveShadows = true;

    bool isValid() const {
        return bgfx::isValid(vbh) && bgfx::isValid(ibh) && bgfx::isValid(program);
    }
};

// ============================================================================
// Light Component
// ============================================================================
struct Light {
    enum class Type {
        Directional,
        Point,
        Spot
    };

    Type type = Type::Directional;
    bx::Vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;

    // Point/Spot light settings
    float range = 10.0f;
    float attenuation = 1.0f;

    // Spot light settings
    float innerConeAngle = 30.0f; // degrees
    float outerConeAngle = 45.0f; // degrees

    bool castShadows = true;
};

// ============================================================================
// Rigidbody Component (for future physics integration)
// ============================================================================
struct Rigidbody {
    bx::Vec3 velocity = {0.0f, 0.0f, 0.0f};
    bx::Vec3 angularVelocity = {0.0f, 0.0f, 0.0f};
    float mass = 1.0f;
    float drag = 0.0f;
    float angularDrag = 0.05f;
    bool useGravity = true;
    bool isKinematic = false;
};

// ============================================================================
// Tag Component
// ============================================================================
struct Tag {
    std::string name;

    explicit Tag(const std::string& n = "Entity") : name(n) {}
};

// ============================================================================
// Script Component (for attaching custom behavior)
// ============================================================================
struct Script {
    std::string scriptName;
    void* userData = nullptr; // Custom data pointer
};

// ============================================================================
// Collision Components (minimal starter set)
// ============================================================================

// Axis-aligned bounding box collider in local space.
// World AABB = Transform.position +/- halfExtents (scale not applied yet).
struct ColliderAABB {
    bx::Vec3 halfExtents = {0.5f, 0.5f, 0.5f};
    bool isStatic = true;
};

// Sphere collider (good for a player controller starter).
struct ColliderSphere {
    float radius = 0.35f;
};

// Capsule/Cylinder collider (best for character controllers - combines stability of cylinder with smooth edge handling).
struct ColliderCapsule {
    float radius = 0.35f;
    float height = 1.7f;  // Total height including hemispherical caps
    
    // Helper to get cylinder portion height (excludes caps)
    float getCylinderHeight() const {
        return height - (radius * 2.0f);
    }
};

} // namespace ecs