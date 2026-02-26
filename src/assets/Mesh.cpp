// src/assets/Mesh.cpp
#include "assets/Mesh.hpp"
#include <cmath>
#include <algorithm>

namespace assets {

// ============================================================================
// Vertex Layout Implementations
// ============================================================================

bgfx::VertexLayout PosNormalVertex::getLayout() {
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .end();
    return layout;
}

bgfx::VertexLayout PosNormalTexVertex::getLayout() {
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
    return layout;
}

// ============================================================================
// MeshData Implementation
// ============================================================================

void MeshData::calculateBounds() {
    if (vertices.empty()) {
        minX = minY = minZ = 0.0f;
        maxX = maxY = maxZ = 0.0f;
        return;
    }
    
    minX = maxX = vertices[0].x;
    minY = maxY = vertices[0].y;
    minZ = maxZ = vertices[0].z;
    
    for (const auto& v : vertices) {
        minX = std::min(minX, v.x);
        minY = std::min(minY, v.y);
        minZ = std::min(minZ, v.z);
        maxX = std::max(maxX, v.x);
        maxY = std::max(maxY, v.y);
        maxZ = std::max(maxZ, v.z);
    }
}

// ============================================================================
// Mesh Implementation
// ============================================================================

void Mesh::destroy() {
    if (bgfx::isValid(vbh)) {
        bgfx::destroy(vbh);
        vbh = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(ibh)) {
        bgfx::destroy(ibh);
        ibh = BGFX_INVALID_HANDLE;
    }
}

// ============================================================================
// Primitive Generators
// ============================================================================

namespace primitives {

MeshData createCube(float size) {
    MeshData mesh;
    const float h = size * 0.5f;
    
    // Define 6 faces of the cube, each with 4 vertices
    const PosNormalVertex vertices[] = {
        // +X face
        { h, -h, -h,  1, 0, 0}, { h,  h, -h,  1, 0, 0}, 
        { h,  h,  h,  1, 0, 0}, { h, -h,  h,  1, 0, 0},
        // -X face
        {-h, -h,  h, -1, 0, 0}, {-h,  h,  h, -1, 0, 0}, 
        {-h,  h, -h, -1, 0, 0}, {-h, -h, -h, -1, 0, 0},
        // +Y face
        {-h,  h, -h,  0, 1, 0}, {-h,  h,  h,  0, 1, 0}, 
        { h,  h,  h,  0, 1, 0}, { h,  h, -h,  0, 1, 0},
        // -Y face
        {-h, -h,  h,  0,-1, 0}, {-h, -h, -h,  0,-1, 0}, 
        { h, -h, -h,  0,-1, 0}, { h, -h,  h,  0,-1, 0},
        // +Z face
        { h, -h,  h,  0, 0, 1}, { h,  h,  h,  0, 0, 1}, 
        {-h,  h,  h,  0, 0, 1}, {-h, -h,  h,  0, 0, 1},
        // -Z face
        {-h, -h, -h,  0, 0,-1}, {-h,  h, -h,  0, 0,-1}, 
        { h,  h, -h,  0, 0,-1}, { h, -h, -h,  0, 0,-1},
    };
    
    const uint16_t indices[] = {
        0,1,2,   0,2,3,   // +X
        4,5,6,   4,6,7,   // -X
        8,9,10,  8,10,11, // +Y
        12,13,14, 12,14,15, // -Y
        16,17,18, 16,18,19, // +Z
        20,21,22, 20,22,23  // -Z
    };
    
    mesh.vertices.assign(vertices, vertices + 24);
    mesh.indices.assign(indices, indices + 36);
    mesh.calculateBounds();
    
    return mesh;
}

MeshData createSphere(float radius, int segments) {
    MeshData mesh;
    
    const int rings = segments / 2;
    const float pi = 3.14159265359f;
    
    // Generate vertices
    for (int ring = 0; ring <= rings; ++ring) {
        const float theta = pi * static_cast<float>(ring) / static_cast<float>(rings);
        const float sinTheta = std::sin(theta);
        const float cosTheta = std::cos(theta);
        
        for (int seg = 0; seg <= segments; ++seg) {
            const float phi = 2.0f * pi * static_cast<float>(seg) / static_cast<float>(segments);
            const float sinPhi = std::sin(phi);
            const float cosPhi = std::cos(phi);
            
            PosNormalVertex v;
            v.nx = cosPhi * sinTheta;
            v.ny = cosTheta;
            v.nz = sinPhi * sinTheta;
            
            v.x = radius * v.nx;
            v.y = radius * v.ny;
            v.z = radius * v.nz;
            
            mesh.vertices.push_back(v);
        }
    }
    
    // Generate indices
    for (int ring = 0; ring < rings; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            const int current = ring * (segments + 1) + seg;
            const int next = current + segments + 1;
            
            mesh.indices.push_back(current);
            mesh.indices.push_back(next);
            mesh.indices.push_back(current + 1);
            
            mesh.indices.push_back(current + 1);
            mesh.indices.push_back(next);
            mesh.indices.push_back(next + 1);
        }
    }
    
    mesh.calculateBounds();
    return mesh;
}

MeshData createPlane(float width, float height, int segmentsX, int segmentsY) {
    MeshData mesh;
    
    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;
    
    // Generate vertices
    for (int y = 0; y <= segmentsY; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(segmentsY);
        const float py = -halfHeight + v * height;
        
        for (int x = 0; x <= segmentsX; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(segmentsX);
            const float px = -halfWidth + u * width;
            
            PosNormalVertex vert;
            vert.x = px;
            vert.y = 0.0f;
            vert.z = py;
            vert.nx = 0.0f;
            vert.ny = 1.0f;
            vert.nz = 0.0f;
            
            mesh.vertices.push_back(vert);
        }
    }
    
    // Generate indices
    for (int y = 0; y < segmentsY; ++y) {
        for (int x = 0; x < segmentsX; ++x) {
            const int i0 = y * (segmentsX + 1) + x;
            const int i1 = i0 + 1;
            const int i2 = i0 + (segmentsX + 1);
            const int i3 = i2 + 1;
            
            mesh.indices.push_back(i0);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i1);
            
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i3);
        }
    }
    
    mesh.calculateBounds();
    return mesh;
}

MeshData createCylinder(float radius, float height, int segments) {
    MeshData mesh;
    const float halfHeight = height * 0.5f;
    const float pi = 3.14159265359f;
    
    // Top cap center
    PosNormalVertex topCenter;
    topCenter.x = 0.0f; topCenter.y = halfHeight; topCenter.z = 0.0f;
    topCenter.nx = 0.0f; topCenter.ny = 1.0f; topCenter.nz = 0.0f;
    mesh.vertices.push_back(topCenter);
    
    // Bottom cap center
    PosNormalVertex bottomCenter;
    bottomCenter.x = 0.0f; bottomCenter.y = -halfHeight; bottomCenter.z = 0.0f;
    bottomCenter.nx = 0.0f; bottomCenter.ny = -1.0f; bottomCenter.nz = 0.0f;
    mesh.vertices.push_back(bottomCenter);
    
    // Side vertices (duplicated for top cap, side, and bottom cap)
    for (int i = 0; i <= segments; ++i) {
        const float angle = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
        const float x = radius * std::cos(angle);
        const float z = radius * std::sin(angle);
        
        // Top cap edge
        PosNormalVertex topEdge;
        topEdge.x = x; topEdge.y = halfHeight; topEdge.z = z;
        topEdge.nx = 0.0f; topEdge.ny = 1.0f; topEdge.nz = 0.0f;
        mesh.vertices.push_back(topEdge);
        
        // Side top
        PosNormalVertex sideTop;
        sideTop.x = x; sideTop.y = halfHeight; sideTop.z = z;
        sideTop.nx = std::cos(angle); sideTop.ny = 0.0f; sideTop.nz = std::sin(angle);
        mesh.vertices.push_back(sideTop);
        
        // Side bottom
        PosNormalVertex sideBottom;
        sideBottom.x = x; sideBottom.y = -halfHeight; sideBottom.z = z;
        sideBottom.nx = std::cos(angle); sideBottom.ny = 0.0f; sideBottom.nz = std::sin(angle);
        mesh.vertices.push_back(sideBottom);
        
        // Bottom cap edge
        PosNormalVertex bottomEdge;
        bottomEdge.x = x; bottomEdge.y = -halfHeight; bottomEdge.z = z;
        bottomEdge.nx = 0.0f; bottomEdge.ny = -1.0f; bottomEdge.nz = 0.0f;
        mesh.vertices.push_back(bottomEdge);
    }
    
    // Generate indices
    for (int i = 0; i < segments; ++i) {
        const int base = 2 + i * 4;
        
        // Top cap
        mesh.indices.push_back(0);
        mesh.indices.push_back(base);
        mesh.indices.push_back(base + 4);
        
        // Side
        mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 5);
        
        mesh.indices.push_back(base + 5);
        mesh.indices.push_back(base + 6);
        mesh.indices.push_back(base + 1);
        
        // Bottom cap
        mesh.indices.push_back(1);
        mesh.indices.push_back(base + 7);
        mesh.indices.push_back(base + 3);
    }
    
    mesh.calculateBounds();
    return mesh;
}

MeshData createCone(float radius, float height, int segments) {
    MeshData mesh;
    const float pi = 3.14159265359f;
    
    // Apex
    PosNormalVertex apex;
    apex.x = 0.0f; apex.y = height; apex.z = 0.0f;
    apex.nx = 0.0f; apex.ny = 1.0f; apex.nz = 0.0f;
    mesh.vertices.push_back(apex);
    
    // Bottom center
    PosNormalVertex bottomCenter;
    bottomCenter.x = 0.0f; bottomCenter.y = 0.0f; bottomCenter.z = 0.0f;
    bottomCenter.nx = 0.0f; bottomCenter.ny = -1.0f; bottomCenter.nz = 0.0f;
    mesh.vertices.push_back(bottomCenter);
    
    // Generate base and side vertices
    const float slant = std::sqrt(radius * radius + height * height);
    const float normalY = radius / slant;
    
    for (int i = 0; i <= segments; ++i) {
        const float angle = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
        const float x = radius * std::cos(angle);
        const float z = radius * std::sin(angle);
        
        // Side vertex
        PosNormalVertex sideVert;
        sideVert.x = x; sideVert.y = 0.0f; sideVert.z = z;
        sideVert.nx = std::cos(angle) * normalY;
        sideVert.ny = height / slant;
        sideVert.nz = std::sin(angle) * normalY;
        mesh.vertices.push_back(sideVert);
        
        // Bottom vertex
        PosNormalVertex bottomVert;
        bottomVert.x = x; bottomVert.y = 0.0f; bottomVert.z = z;
        bottomVert.nx = 0.0f; bottomVert.ny = -1.0f; bottomVert.nz = 0.0f;
        mesh.vertices.push_back(bottomVert);
    }
    
    // Generate indices
    for (int i = 0; i < segments; ++i) {
        const int base = 2 + i * 2;
        
        // Side triangle
        mesh.indices.push_back(0);
        mesh.indices.push_back(base);
        mesh.indices.push_back(base + 2);
        
        // Bottom triangle
        mesh.indices.push_back(1);
        mesh.indices.push_back(base + 3);
        mesh.indices.push_back(base + 1);
    }
    
    mesh.calculateBounds();
    return mesh;
}

} // namespace primitives
} // namespace assets