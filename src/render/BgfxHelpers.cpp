// src/render/BgfxHelpers.cpp
#include "render/BgfxHelpers.hpp"
#include <fstream>
#include <vector>
#include <iostream>
#include <cmath>
#include <cstring>  // For memcpy

namespace render {

// ============================================================================
// Matrix Helpers
// ============================================================================

void createLookAtMatrix(float* outView, 
                       const bx::Vec3& eye, 
                       const bx::Vec3& at, 
                       const bx::Vec3& up) {
    bx::mtxLookAt(outView, eye, at, up);
}

void createPerspectiveMatrix(float* outProj,
                            float fov,
                            float aspect,
                            float nearPlane,
                            float farPlane,
                            bool homogeneousDepth) {
    bx::mtxProj(outProj, fov, aspect, nearPlane, farPlane, homogeneousDepth);
}

void createOrthographicMatrix(float* outProj,
                             float left,
                             float right,
                             float bottom,
                             float top,
                             float nearPlane,
                             float farPlane,
                             float offset,
                             bool homogeneousDepth) {
    bx::mtxOrtho(outProj, left, right, bottom, top, nearPlane, farPlane, offset, homogeneousDepth);
}

void createTransformMatrix(float* outMatrix,
                          const bx::Vec3& position,
                          const bx::Vec3& rotation,
                          const bx::Vec3& scale) {
    // Create rotation matrix from Euler angles
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
// Shader Helpers
// ============================================================================

bgfx::ShaderHandle loadShader(const char* filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open shader: " << filepath << "\n";
        return BGFX_INVALID_HANDLE;
    }
    
    file.seekg(0, std::ios::end);
    const size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    
    const bgfx::Memory* mem = bgfx::copy(data.data(), static_cast<uint32_t>(size));
    return bgfx::createShader(mem);
}

bgfx::ProgramHandle loadProgram(const char* vsPath, const char* fsPath) {
    bgfx::ShaderHandle vs = loadShader(vsPath);
    bgfx::ShaderHandle fs = loadShader(fsPath);
    
    if (!bgfx::isValid(vs) || !bgfx::isValid(fs)) {
        if (bgfx::isValid(vs)) bgfx::destroy(vs);
        if (bgfx::isValid(fs)) bgfx::destroy(fs);
        return BGFX_INVALID_HANDLE;
    }
    
    return bgfx::createProgram(vs, fs, true);
}

// ============================================================================
// Uniform Helpers
// ============================================================================

void setUniformMat4(bgfx::UniformHandle uniform, const float* matrix) {
    bgfx::setUniform(uniform, matrix, 1);
}

void setUniformVec4(bgfx::UniformHandle uniform, const bx::Vec3& vec, float w) {
    float data[4] = { vec.x, vec.y, vec.z, w };
    bgfx::setUniform(uniform, data, 1);
}

void setUniformVec3(bgfx::UniformHandle uniform, const bx::Vec3& vec) {
    setUniformVec4(uniform, vec, 0.0f);
}

// ============================================================================
// Mesh Rendering Helpers
// ============================================================================

void submitMesh(const assets::Mesh& mesh, 
               bgfx::ProgramHandle program,
               const float* transform,
               bgfx::ViewId viewId,
               uint64_t state) {
    if (!mesh.isValid() || !bgfx::isValid(program)) {
        return;
    }
    
    // Set transform
    bgfx::setTransform(transform);
    
    // Set vertex and index buffers
    bgfx::setVertexBuffer(0, mesh.vbh);
    bgfx::setIndexBuffer(mesh.ibh);
    
    // Set render state
    bgfx::setState(state);
    
    // Submit
    bgfx::submit(viewId, program);
}

void submitMeshWithUniforms(const assets::Mesh& mesh,
                           bgfx::ProgramHandle program,
                           bgfx::UniformHandle* uniforms,
                           const void** uniformData,
                           uint32_t numUniforms,
                           const float* transform,
                           bgfx::ViewId viewId,
                           uint64_t state) {
    if (!mesh.isValid() || !bgfx::isValid(program)) {
        return;
    }
    
    // Set transform
    bgfx::setTransform(transform);
    
    // Set uniforms
    for (uint32_t i = 0; i < numUniforms; ++i) {
        if (bgfx::isValid(uniforms[i])) {
            bgfx::setUniform(uniforms[i], uniformData[i]);
        }
    }
    
    // Set vertex and index buffers
    bgfx::setVertexBuffer(0, mesh.vbh);
    bgfx::setIndexBuffer(mesh.ibh);
    
    // Set render state
    bgfx::setState(state);
    
    // Submit
    bgfx::submit(viewId, program);
}

// ============================================================================
// Debug Rendering
// ============================================================================

void drawDebugAxes(const bx::Vec3& position, float size) {
    // TODO: Implement debug line rendering
    // This would require a separate line rendering system
}

void drawDebugGrid(float size, int divisions) {
    // TODO: Implement debug grid rendering
    // This would require a separate line rendering system
}

} // namespace render