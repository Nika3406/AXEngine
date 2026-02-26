// src/ecs/Systems.cpp
#include "ecs/Systems.hpp"
#include "ecs/Components.hpp"

#include <iostream>
#include <algorithm>
#include <cmath>

namespace ecs {

// ============================================================================
// Transform System
// ============================================================================

void TransformSystem::update(entt::registry& registry) {
    // Currently transforms are passive - they're used by other systems
    // Future: implement transform hierarchy (parent-child relationships)
    (void)registry;
}

void TransformSystem::updateMatrix(Transform& transform, float* outMatrix) {
    transform.getMatrix(outMatrix);
}

// ============================================================================
// Camera System
// ============================================================================

void CameraSystem::update(entt::registry& registry, float aspect) {
    (void)aspect;

    // Ensure only one camera is active
    int activeCameras = 0;
    entt::entity firstCamera = entt::null;

    auto view = registry.view<Camera>();
    for (auto entity : view) {
        auto& camera = view.get<Camera>(entity);
        if (camera.isActive) {
            activeCameras++;
            if (firstCamera == entt::null) {
                firstCamera = entity;
            }
        }
    }

    // If multiple cameras are active, deactivate all but the first
    if (activeCameras > 1) {
        bool foundFirst = false;
        for (auto entity : view) {
            auto& camera = view.get<Camera>(entity);
            if (camera.isActive) {
                if (!foundFirst) {
                    foundFirst = true;
                } else {
                    camera.isActive = false;
                    std::cerr << "Warning: Multiple active cameras found. Deactivating extras.\n";
                }
            }
        }
    }
}

entt::entity CameraSystem::getActiveCamera(entt::registry& registry) {
    auto view = registry.view<Camera>();
    for (auto entity : view) {
        const auto& camera = view.get<Camera>(entity);
        if (camera.isActive) {
            return entity;
        }
    }
    return entt::null;
}

bool CameraSystem::getActiveCameraMatrices(entt::registry& registry,
                                          float aspect,
                                          float* outView,
                                          float* outProj) {
    entt::entity cameraEntity = getActiveCamera(registry);
    if (cameraEntity == entt::null) {
        return false;
    }

    const auto& camera = registry.get<Camera>(cameraEntity);
    const auto* transform = registry.try_get<Transform>(cameraEntity);

    bx::Vec3 position = {0.0f, 0.0f, 0.0f};
    if (transform) {
        position = transform->position;
    }

    camera.getViewMatrix(position, outView);
    camera.getProjectionMatrix(aspect, outProj);

    return true;
}

// ============================================================================
// Render System
// ============================================================================

// Static uniform handle for color
static bgfx::UniformHandle s_colorUniform = BGFX_INVALID_HANDLE;

void RenderSystem::render(entt::registry& registry,
                          const float* view,
                          const float* proj,
                          bgfx::ViewId viewId) {
    // Create color uniform if needed
    if (!bgfx::isValid(s_colorUniform)) {
        s_colorUniform = bgfx::createUniform("u_color", bgfx::UniformType::Vec4);
    }

    // Set view transform for this view
    bgfx::setViewTransform(viewId, view, proj);

    // Touch view to ensure it renders even if nothing is submitted
    bgfx::touch(viewId);

    // Render all entities with Transform and MeshRenderer
    auto renderView = registry.view<Transform, MeshRenderer>();
    for (auto entity : renderView) {
        const auto& transform = renderView.get<Transform>(entity);
        const auto& renderer = renderView.get<MeshRenderer>(entity);

        if (renderer.isValid()) {
            renderEntity(transform, renderer, view, proj, viewId);
        }
    }
}

void RenderSystem::renderEntity(const Transform& transform,
                                const MeshRenderer& renderer,
                                const float* view,
                                const float* proj,
                                bgfx::ViewId viewId) {
    (void)view;
    (void)proj;

    // Get transform matrix
    float modelMatrix[16];
    transform.getMatrix(modelMatrix);

    // Set transform
    bgfx::setTransform(modelMatrix);

    // Set vertex and index buffers
    bgfx::setVertexBuffer(0, renderer.vbh);
    bgfx::setIndexBuffer(renderer.ibh);

    // Set color uniform (IMPORTANT: This makes colors visible!)
    if (bgfx::isValid(s_colorUniform)) {
        float colorVec4[4] = {
            renderer.color.x,
            renderer.color.y,
            renderer.color.z,
            1.0f
        };
        bgfx::setUniform(s_colorUniform, colorVec4);
    }

    // Set render state - enable depth test and writing
    uint64_t state = 0
        | BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_A
        | BGFX_STATE_WRITE_Z
        | BGFX_STATE_DEPTH_TEST_LESS
        | BGFX_STATE_CULL_CCW
        | BGFX_STATE_MSAA
        ;

    bgfx::setState(state);

    // Submit
    bgfx::submit(viewId, renderer.program);
}

// ============================================================================
// Collision System (sphere, capsule vs AABB, push-out resolution)
// ============================================================================

static bx::Vec3 v3Clamp(const bx::Vec3& v, const bx::Vec3& mn, const bx::Vec3& mx) {
    return {
        std::max(mn.x, std::min(v.x, mx.x)),
        std::max(mn.y, std::min(v.y, mx.y)),
        std::max(mn.z, std::min(v.z, mx.z)),
    };
}

static float v3Dot(const bx::Vec3& a, const bx::Vec3& b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

static float v3Len(const bx::Vec3& v) {
    return std::sqrt(v3Dot(v, v));
}

// Test capsule vs AABB - returns penetration depth and push direction
static bool testCapsuleVsAABB(const bx::Vec3& capsulePos, 
                              float capsuleRadius, 
                              float capsuleHeight,
                              const bx::Vec3& aabbMin,
                              const bx::Vec3& aabbMax,
                              float& outPenetration,
                              bx::Vec3& outPushDir) {
    // Capsule is defined by a line segment (bottom to top) + radius
    float halfCylinderHeight = (capsuleHeight - capsuleRadius * 2.0f) * 0.5f;
    
    bx::Vec3 capsuleBottom = {capsulePos.x, capsulePos.y - halfCylinderHeight, capsulePos.z};
    bx::Vec3 capsuleTop = {capsulePos.x, capsulePos.y + halfCylinderHeight, capsulePos.z};
    
    // Find closest point on AABB to the capsule line segment
    float bestDist = std::numeric_limits<float>::max();
    bx::Vec3 bestPoint = {0.0f, 0.0f, 0.0f};
    
    // Sample points along the capsule centerline
    const int samples = 5;
    for (int i = 0; i <= samples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(samples);
        bx::Vec3 p = {
            capsuleBottom.x + t * (capsuleTop.x - capsuleBottom.x),
            capsuleBottom.y + t * (capsuleTop.y - capsuleBottom.y),
            capsuleBottom.z + t * (capsuleTop.z - capsuleBottom.z)
        };
        
        // Closest point on AABB to this sample point
        bx::Vec3 closest = v3Clamp(p, aabbMin, aabbMax);
        bx::Vec3 d = {p.x - closest.x, p.y - closest.y, p.z - closest.z};
        float dist = v3Len(d);
        
        if (dist < bestDist) {
            bestDist = dist;
            bestPoint = closest;
        }
    }
    
    // Check if we're colliding
    if (bestDist >= capsuleRadius) {
        return false;  // No collision
    }
    
    outPenetration = capsuleRadius - bestDist;
    
    // Calculate push direction
    // Find closest point on capsule line to bestPoint
    bx::Vec3 capsuleDir = {
        capsuleTop.x - capsuleBottom.x,
        capsuleTop.y - capsuleBottom.y,
        capsuleTop.z - capsuleBottom.z
    };
    float capsuleLen = v3Len(capsuleDir);
    
    if (capsuleLen > 0.0001f) {
        capsuleDir.x /= capsuleLen;
        capsuleDir.y /= capsuleLen;
        capsuleDir.z /= capsuleLen;
    }
    
    bx::Vec3 toPoint = {
        bestPoint.x - capsuleBottom.x,
        bestPoint.y - capsuleBottom.y,
        bestPoint.z - capsuleBottom.z
    };
    
    float t = v3Dot(toPoint, capsuleDir);
    t = std::max(0.0f, std::min(t, capsuleLen));
    
    bx::Vec3 closestOnCapsule = {
        capsuleBottom.x + capsuleDir.x * t,
        capsuleBottom.y + capsuleDir.y * t,
        capsuleBottom.z + capsuleDir.z * t
    };
    
    bx::Vec3 pushVector = {
        closestOnCapsule.x - bestPoint.x,
        closestOnCapsule.y - bestPoint.y,
        closestOnCapsule.z - bestPoint.z
    };
    
    float pushLen = v3Len(pushVector);
    if (pushLen < 0.0001f) {
        // Capsule center is inside AABB - push up
        outPushDir = {0.0f, 1.0f, 0.0f};
    } else {
        outPushDir = {
            pushVector.x / pushLen,
            pushVector.y / pushLen,
            pushVector.z / pushLen
        };
    }
    
    return true;
}

void CollisionSystem::resolvePlayerVsWorld(entt::registry& registry, entt::entity player) {
    if (!registry.valid(player)) return;
    
    auto* pT = registry.try_get<Transform>(player);
    if (!pT) return;
    
    bx::Vec3 p = pT->position;
    
    // Check if player has capsule or sphere collider
    auto* capsule = registry.try_get<ColliderCapsule>(player);
    auto* sphere = registry.try_get<ColliderSphere>(player);
    
    if (!capsule && !sphere) return;
    
    // Iterate AABBs (world)
    auto aabbView = registry.view<Transform, ColliderAABB>();
    for (auto e : aabbView) {
        if (e == player) continue;

        const auto& t = aabbView.get<Transform>(e);
        const auto& b = aabbView.get<ColliderAABB>(e);

        // World AABB bounds
        bx::Vec3 mn { t.position.x - b.halfExtents.x, t.position.y - b.halfExtents.y, t.position.z - b.halfExtents.z };
        bx::Vec3 mx { t.position.x + b.halfExtents.x, t.position.y + b.halfExtents.y, t.position.z + b.halfExtents.z };

        float penetration = 0.0f;
        bx::Vec3 pushDir = {0.0f, 1.0f, 0.0f};
        bool colliding = false;
        
        if (capsule) {
            // Capsule collision
            colliding = testCapsuleVsAABB(p, capsule->radius, capsule->height, mn, mx, penetration, pushDir);
        } else if (sphere) {
            // Sphere collision (original code)
            bx::Vec3 c = v3Clamp(p, mn, mx);
            bx::Vec3 d { p.x - c.x, p.y - c.y, p.z - c.z };
            float dist = v3Len(d);
            
            if (dist < sphere->radius) {
                colliding = true;
                penetration = sphere->radius - dist;
                
                if (dist >= 1e-6f) {
                    pushDir = { d.x/dist, d.y/dist, d.z/dist };
                }
            }
        }
        
        if (colliding) {
            // Push player out
            p.x += pushDir.x * penetration;
            p.y += pushDir.y * penetration;
            p.z += pushDir.z * penetration;
        }
    }

    pT->position = p;
}

// ============================================================================
// Physics System
// ============================================================================

void PhysicsSystem::update(entt::registry& registry, float deltaTime) {
    auto view = registry.view<Transform, Rigidbody>();

    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        auto& rb = view.get<Rigidbody>(entity);

        if (rb.isKinematic) {
            continue;
        }

        // Apply gravity
        if (rb.useGravity) {
            bx::Vec3 gravity = {0.0f, -9.81f * rb.mass, 0.0f};
            rb.velocity.x += gravity.x * deltaTime;
            rb.velocity.y += gravity.y * deltaTime;
            rb.velocity.z += gravity.z * deltaTime;
        }

        // Apply drag
        float dragFactor = 1.0f - (rb.drag * deltaTime);
        if (dragFactor < 0.0f) dragFactor = 0.0f;

        rb.velocity.x *= dragFactor;
        rb.velocity.y *= dragFactor;
        rb.velocity.z *= dragFactor;

        // Apply angular drag
        float angularDragFactor = 1.0f - (rb.angularDrag * deltaTime);
        if (angularDragFactor < 0.0f) angularDragFactor = 0.0f;

        rb.angularVelocity.x *= angularDragFactor;
        rb.angularVelocity.y *= angularDragFactor;
        rb.angularVelocity.z *= angularDragFactor;

        // Update position
        transform.position.x += rb.velocity.x * deltaTime;
        transform.position.y += rb.velocity.y * deltaTime;
        transform.position.z += rb.velocity.z * deltaTime;

        // Update rotation
        transform.rotation.x += rb.angularVelocity.x * deltaTime;
        transform.rotation.y += rb.angularVelocity.y * deltaTime;
        transform.rotation.z += rb.angularVelocity.z * deltaTime;
    }
}

void PhysicsSystem::applyForce(Rigidbody& rb, const bx::Vec3& force) {
    if (rb.mass > 0.0f) {
        rb.velocity.x += force.x / rb.mass;
        rb.velocity.y += force.y / rb.mass;
        rb.velocity.z += force.z / rb.mass;
    }
}

void PhysicsSystem::applyImpulse(Rigidbody& rb, const bx::Vec3& impulse) {
    if (rb.mass > 0.0f) {
        rb.velocity.x += impulse.x / rb.mass;
        rb.velocity.y += impulse.y / rb.mass;
        rb.velocity.z += impulse.z / rb.mass;
    }
}

} // namespace ecs