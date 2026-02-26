// src/ecs/Components.cpp
#include "ecs/Components.hpp"
#include <bx/math.h>
#include <cstring>  // For memcpy

namespace ecs {

// ============================================================================
// Transform Implementation
// ============================================================================

void Transform::getMatrix(float* outMatrix) const {
    // Create rotation matrix
    float rotMatrix[16];
    bx::mtxRotateXYZ(rotMatrix, rotation.x, rotation.y, rotation.z);
    
    // Create scale matrix
    float scaleMatrix[16];
    bx::mtxScale(scaleMatrix, scale.x, scale.y, scale.z);
    
    // Combine: Scale * Rotation
    float temp[16];
    bx::mtxMul(temp, scaleMatrix, rotMatrix);
    
    // Add translation
    temp[12] = position.x;
    temp[13] = position.y;
    temp[14] = position.z;
    
    // Copy to output
    std::memcpy(outMatrix, temp, sizeof(float) * 16);
}

// ============================================================================
// Camera Implementation
// ============================================================================

void Camera::getViewMatrix(const bx::Vec3& position, float* outView) const {
    // Calculate look-at point
    bx::Vec3 lookAt = {
        position.x + target.x,
        position.y + target.y,
        position.z + target.z
    };
    
    bx::mtxLookAt(outView, position, lookAt, up);
}

void Camera::getProjectionMatrix(float aspect, float* outProj) const {
    if (type == Type::Perspective) {
        bx::mtxProj(outProj, fov, aspect, nearPlane, farPlane, 
                   bgfx::getCaps()->homogeneousDepth);
    } else {
        const float halfWidth = orthoSize * aspect * 0.5f;
        const float halfHeight = orthoSize * 0.5f;
        bx::mtxOrtho(outProj, -halfWidth, halfWidth, -halfHeight, halfHeight,
                    nearPlane, farPlane, 0.0f, bgfx::getCaps()->homogeneousDepth);
    }
}

} // namespace ecs