// include/ecs/Systems.hpp
#pragma once

#include <entt/entt.hpp>
#include <bgfx/bgfx.h>
#include <bx/math.h>

namespace ecs {

// Forward declarations
struct Transform;
struct Camera;
struct MeshRenderer;
struct Light;
struct Rigidbody;

struct ColliderAABB;
struct ColliderSphere;
struct ColliderCapsule;

// ============================================================================
// Transform System
// ============================================================================
class TransformSystem {
public:
    static void update(entt::registry& registry);

    // Helper functions
    static void updateMatrix(Transform& transform, float* outMatrix);
};

// ============================================================================
// Camera System
// ============================================================================
class CameraSystem {
public:
    static void update(entt::registry& registry, float aspect);

    // Get the active camera entity
    static entt::entity getActiveCamera(entt::registry& registry);

    // Get view and projection matrices for active camera
    static bool getActiveCameraMatrices(entt::registry& registry,
                                       float aspect,
                                       float* outView,
                                       float* outProj);
};

// ============================================================================
// Render System
// ============================================================================
class RenderSystem {
public:
    static void render(entt::registry& registry,
                      const float* view,
                      const float* proj,
                      bgfx::ViewId viewId = 0);

    static void renderEntity(const Transform& transform,
                            const MeshRenderer& renderer,
                            const float* view,
                            const float* proj,
                            bgfx::ViewId viewId);
};

// ============================================================================
// Collision System (minimal, practical starter)
// ============================================================================
class CollisionSystem {
public:
    // Resolves player (sphere or capsule) vs world (AABBs) by pushing the player out.
    static void resolvePlayerVsWorld(entt::registry& registry, entt::entity player);
};

// ============================================================================
// Physics System (placeholder for future implementation)
// ============================================================================
class PhysicsSystem {
public:
    static void update(entt::registry& registry, float deltaTime);

    static void applyForce(Rigidbody& rb, const bx::Vec3& force);
    static void applyImpulse(Rigidbody& rb, const bx::Vec3& impulse);
};

} // namespace ecs