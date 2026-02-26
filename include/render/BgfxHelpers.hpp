// include/render/BgfxHelpers.hpp
#pragma once

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include "assets/Mesh.hpp"

namespace render {

// ============================================================================
// Matrix Helpers
// ============================================================================

// Create look-at view matrix
void createLookAtMatrix(float* outView, 
                       const bx::Vec3& eye, 
                       const bx::Vec3& at, 
                       const bx::Vec3& up);

// Create perspective projection matrix
void createPerspectiveMatrix(float* outProj,
                            float fov,
                            float aspect,
                            float nearPlane,
                            float farPlane,
                            bool homogeneousDepth);

// Create orthographic projection matrix
void createOrthographicMatrix(float* outProj,
                             float left,
                             float right,
                             float bottom,
                             float top,
                             float nearPlane,
                             float farPlane,
                             float offset,
                             bool homogeneousDepth);

// Create transformation matrix from position, rotation (euler), and scale
void createTransformMatrix(float* outMatrix,
                          const bx::Vec3& position,
                          const bx::Vec3& rotation,
                          const bx::Vec3& scale);

// ============================================================================
// Shader Helpers
// ============================================================================

// Load shader from file
bgfx::ShaderHandle loadShader(const char* filepath);

// Load program from vertex and fragment shader files
bgfx::ProgramHandle loadProgram(const char* vsPath, const char* fsPath);

// ============================================================================
// Uniform Helpers
// ============================================================================

// Set mat4 uniform
void setUniformMat4(bgfx::UniformHandle uniform, const float* matrix);

// Set vec4 uniform
void setUniformVec4(bgfx::UniformHandle uniform, const bx::Vec3& vec, float w = 1.0f);

// Set vec3 uniform (as vec4 with w=0)
void setUniformVec3(bgfx::UniformHandle uniform, const bx::Vec3& vec);

// ============================================================================
// Mesh Rendering Helpers
// ============================================================================

// Submit mesh for rendering
void submitMesh(const assets::Mesh& mesh, 
               bgfx::ProgramHandle program,
               const float* transform,
               bgfx::ViewId viewId = 0,
               uint64_t state = BGFX_STATE_DEFAULT);

// Submit mesh with custom uniforms
void submitMeshWithUniforms(const assets::Mesh& mesh,
                           bgfx::ProgramHandle program,
                           bgfx::UniformHandle* uniforms,
                           const void** uniformData,
                           uint32_t numUniforms,
                           const float* transform,
                           bgfx::ViewId viewId = 0,
                           uint64_t state = BGFX_STATE_DEFAULT);

// ============================================================================
// Debug Rendering
// ============================================================================

// Draw debug axes at position
void drawDebugAxes(const bx::Vec3& position, float size = 1.0f);

// Draw debug grid
void drawDebugGrid(float size = 10.0f, int divisions = 10);

} // namespace render