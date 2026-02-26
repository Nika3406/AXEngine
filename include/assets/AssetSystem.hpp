// include/assets/AssetSystem.hpp
#pragma once

#include "Mesh.hpp"
#include <bgfx/bgfx.h>
#include <bx/math.h>  // Add this for bx::Vec3
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>

namespace assets {

// ============================================================================
// Texture Resource
// ============================================================================
struct Texture {
    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    std::string name;
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t flags = 0;
    
    bool isValid() const {
        return bgfx::isValid(handle);
    }
    
    void destroy() {
        if (isValid()) {
            bgfx::destroy(handle);
            handle = BGFX_INVALID_HANDLE;
        }
    }
};

// ============================================================================
// Material Resource
// ============================================================================
struct Material {
    std::string name;
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    
    // Texture slots
    std::string albedoTexture;
    std::string normalTexture;
    std::string metallicRoughnessTexture;
    std::string emissiveTexture;
    
    // Material properties
    bx::Vec3 albedoColor = {1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    bx::Vec3 emissive = {0.0f, 0.0f, 0.0f};
    
    bool isValid() const {
        return bgfx::isValid(program);
    }
};

// ============================================================================
// Asset System - Manages loading and caching of game assets
// ============================================================================
class AssetSystem {
public:
    AssetSystem() = default;
    ~AssetSystem();

    // Prevent copying
    AssetSystem(const AssetSystem&) = delete;
    AssetSystem& operator=(const AssetSystem&) = delete;

    // ========================================================================
    // Mesh Management
    // ========================================================================
    
    // Load a mesh from file (using Assimp)
    Mesh* loadMesh(const std::string& name, const std::string& filepath);
    
    // Create a mesh from MeshData
    Mesh* createMesh(const std::string& name, const MeshData& data);
    
    // Create primitive meshes
    Mesh* createCubeMesh(const std::string& name, float size = 1.0f);
    Mesh* createSphereMesh(const std::string& name, float radius = 1.0f, int segments = 32);
    Mesh* createPlaneMesh(const std::string& name, float width = 1.0f, float height = 1.0f);
    Mesh* createCylinderMesh(const std::string& name, float radius = 1.0f, float height = 2.0f, int segments = 32);
    Mesh* createConeMesh(const std::string& name, float radius = 1.0f, float height = 2.0f, int segments = 32);
    
    // Get existing mesh
    Mesh* getMesh(const std::string& name);
    const Mesh* getMesh(const std::string& name) const;
    
    // Check if mesh exists
    bool hasMesh(const std::string& name) const;
    
    // Remove mesh
    void removeMesh(const std::string& name);
    
    // ========================================================================
    // Texture Management
    // ========================================================================
    
    // Load texture from file (supports PNG, JPG, BMP, TGA, etc.)
    Texture* loadTexture(const std::string& name, 
                        const std::string& filepath,
                        uint64_t flags = BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE);
    
    // Create texture from memory
    Texture* createTexture(const std::string& name,
                          uint32_t width,
                          uint32_t height,
                          const void* data,
                          bgfx::TextureFormat::Enum format = bgfx::TextureFormat::RGBA8,
                          uint64_t flags = BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE);
    
    // Get existing texture
    Texture* getTexture(const std::string& name);
    const Texture* getTexture(const std::string& name) const;
    
    // Check if texture exists
    bool hasTexture(const std::string& name) const;
    
    // Remove texture
    void removeTexture(const std::string& name);
    
    // ========================================================================
    // Material Management
    // ========================================================================
    
    // Create material
    Material* createMaterial(const std::string& name, bgfx::ProgramHandle program);
    
    // Get existing material
    Material* getMaterial(const std::string& name);
    const Material* getMaterial(const std::string& name) const;
    
    // Check if material exists
    bool hasMaterial(const std::string& name) const;
    
    // Remove material
    void removeMaterial(const std::string& name);
    
    // ========================================================================
    // Shader Management
    // ========================================================================
    
    // Load shader program
    bgfx::ProgramHandle loadProgram(const std::string& name,
                                    const std::string& vsPath,
                                    const std::string& fsPath);
    
    // Get existing shader program
    bgfx::ProgramHandle getProgram(const std::string& name) const;
    
    // Check if program exists
    bool hasProgram(const std::string& name) const;
    
    // Remove program
    void removeProgram(const std::string& name);
    
    // ========================================================================
    // Utility
    // ========================================================================
    
    void addSearchPath(std::string path);
    void clearSearchPaths();
    std::string resolvePath(const std::string& userPath) const;
    // Clear all assets
    void clear();
    
    // Get statistics
    size_t getMeshCount() const { return m_meshes.size(); }
    size_t getTextureCount() const { return m_textures.size(); }
    size_t getMaterialCount() const { return m_materials.size(); }
    size_t getProgramCount() const { return m_programs.size(); }

private:
    // Load shader from file
    bgfx::ShaderHandle loadShader(const std::string& filepath);
    
    // Load mesh from file using Assimp
    MeshData loadMeshFromFile(const std::string& filepath);
    
    // Load image data from file
    struct ImageData {
        uint8_t* data = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t channels = 0;
        
        void free();
    };
    ImageData loadImageFromFile(const std::string& filepath);
    
    // Helper to get bytes per pixel for a texture format
    static size_t getBytesPerPixel(bgfx::TextureFormat::Enum format);

private:
    std::unordered_map<std::string, std::unique_ptr<Mesh>> m_meshes;
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
    std::unordered_map<std::string, std::unique_ptr<Material>> m_materials;
    std::unordered_map<std::string, bgfx::ProgramHandle> m_programs;
    std::vector<std::string> m_searchPaths;
};

} // namespace assets