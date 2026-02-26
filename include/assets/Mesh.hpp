// include/assets/Mesh.hpp
#pragma once

#include <bgfx/bgfx.h>
#include <vector>
#include <string>

namespace assets {

// ============================================================================
// Vertex Format
// ============================================================================
struct PosNormalVertex {
    float x, y, z;
    float nx, ny, nz;

    static bgfx::VertexLayout getLayout();
};

struct PosNormalTexVertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;

    static bgfx::VertexLayout getLayout();
};

// ============================================================================
// Mesh Data
// ============================================================================
struct MeshData {
    std::vector<PosNormalVertex> vertices;
    std::vector<uint16_t> indices;
    
    // Bounding box
    float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
    float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
    
    void calculateBounds();
};

// ============================================================================
// Mesh Resource (GPU-side)
// ============================================================================
struct Mesh {
    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
    uint32_t numIndices = 0;
    
    // Metadata
    std::string name;
    float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
    float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
    
    bool isValid() const {
        return bgfx::isValid(vbh) && bgfx::isValid(ibh);
    }
    
    void destroy();
};

// ============================================================================
// Primitive Mesh Generators
// ============================================================================
namespace primitives {
    MeshData createCube(float size = 1.0f);
    MeshData createSphere(float radius = 1.0f, int segments = 32);
    MeshData createPlane(float width = 1.0f, float height = 1.0f, int segmentsX = 1, int segmentsY = 1);
    MeshData createCylinder(float radius = 1.0f, float height = 2.0f, int segments = 32);
    MeshData createCone(float radius = 1.0f, float height = 2.0f, int segments = 32);
} // namespace primitives

} // namespace assets