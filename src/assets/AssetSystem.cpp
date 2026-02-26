// src/assets/AssetSystem.cpp
#include "assets/AssetSystem.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// stb_image for texture loading
#define STB_IMAGE_IMPLEMENTATION
#include "../../submods/stb/stb_image.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <cctype>

namespace assets {
namespace fs = std::filesystem;

// ------------------------------------------------------------
// Small helpers
// ------------------------------------------------------------
static std::string toLower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static bool fileExists(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec) && fs::is_regular_file(p, ec);
}

static std::string normalizeSlashes(std::string s) {
    for (char& c : s) if (c == '\\') c = '/';
    return s;
}

// ============================================================
// Lifetime
// ============================================================

AssetSystem::~AssetSystem() {
    clear();
}

// ============================================================
// Search Paths
// ============================================================

void AssetSystem::addSearchPath(std::string path) {
    if (path.empty()) return;
    m_searchPaths.push_back(std::move(path));
}

void AssetSystem::clearSearchPaths() {
    m_searchPaths.clear();
}

std::string AssetSystem::resolvePath(const std::string& userPath) const {
    if (userPath.empty()) return userPath;

    fs::path p = fs::path(normalizeSlashes(userPath));

    // absolute or relative existing
    if (p.is_absolute() && fileExists(p)) return p.string();
    if (fileExists(p)) return p.string();

    // try each root
    for (const auto& root : m_searchPaths) {
        fs::path candidate = fs::path(root) / p;
        if (fileExists(candidate)) return candidate.string();
    }

    // return original (caller may print a useful error)
    return userPath;
}

// ============================================================
// Mesh
// ============================================================

Mesh* AssetSystem::loadMesh(const std::string& name, const std::string& filepath) {
    if (hasMesh(name)) {
        return getMesh(name);
    }

    // IMPORTANT: resolve based on search paths (assets/, scripts/assets/, etc.)
    const std::string realObjPath = resolvePath(filepath);

    // Load geometry
    MeshData data = loadMeshFromFile(realObjPath);
    if (data.vertices.empty() || data.indices.empty()) {
        std::cerr << "Failed to load mesh '" << name << "' from: " << realObjPath << "\n";
        return nullptr;
    }

    Mesh* mesh = createMesh(name, data);
    if (!mesh) return nullptr;

    // Auto-import material textures (from .mtl via Assimp)
    // We'll create: material name "<meshName>_mat"
    // and load diffuse as texture name "<meshName>_albedo"
    // This makes Holy_Sword.obj + Holy_Sword.mtl work out of the box.
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(realObjPath,
            aiProcess_Triangulate |
            aiProcess_GenNormals |
            aiProcess_FlipUVs |
            aiProcess_JoinIdenticalVertices
        );

        if (scene && scene->mNumMeshes > 0) {
            aiMesh* aimesh = scene->mMeshes[0];
            const unsigned int matIndex = aimesh->mMaterialIndex;

            fs::path objPath(realObjPath);
            fs::path objDir = objPath.parent_path();

            if (matIndex < scene->mNumMaterials) {
                aiMaterial* mat = scene->mMaterials[matIndex];

                // Diffuse texture (map_Kd in .mtl typically becomes aiTextureType_DIFFUSE)
                aiString texRel;
                if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texRel) == AI_SUCCESS) {
                    fs::path texPath = fs::path(normalizeSlashes(texRel.C_Str()));

                    // Try resolving relative to OBJ directory first (most common for .mtl)
                    fs::path candidate1 = texPath.is_absolute() ? texPath : (objDir / texPath);

                    std::string resolvedTexturePath;
                    if (fileExists(candidate1)) {
                        resolvedTexturePath = candidate1.string();
                    } else {
                        // Then try via AssetSystem search paths (e.g., assets/textures/...)
                        // First try the raw referenced path
                        std::string try1 = resolvePath(texPath.string());
                        if (try1 != texPath.string() && fileExists(try1)) {
                            resolvedTexturePath = try1;
                        } else {
                            // Then try "textures/<filename>" convention
                            fs::path filenameOnly = texPath.filename();
                            std::string try2 = resolvePath((fs::path("textures") / filenameOnly).string());
                            if (try2 != (fs::path("textures") / filenameOnly).string() && fileExists(try2)) {
                                resolvedTexturePath = try2;
                            }
                        }
                    }

                    if (!resolvedTexturePath.empty()) {
                        const std::string texAssetName = name + "_albedo";
                        if (!hasTexture(texAssetName)) {
                            // default flags: point sampler is harsh; use linear + repeat
                            const uint64_t flags =
                                BGFX_SAMPLER_U_CLAMP |
                                BGFX_SAMPLER_V_CLAMP |
                                BGFX_SAMPLER_MIN_ANISOTROPIC |
                                BGFX_SAMPLER_MAG_ANISOTROPIC;

                            Texture* t = loadTexture(texAssetName, resolvedTexturePath, flags);
                            if (!t) {
                                std::cerr << "Warning: failed to load diffuse texture for '" << name
                                          << "' from: " << resolvedTexturePath << "\n";
                            }
                        }

                        // Create/update a material record
                        const std::string matName = name + "_mat";
                        Material* m = hasMaterial(matName) ? getMaterial(matName) : createMaterial(matName, BGFX_INVALID_HANDLE);
                        if (m) {
                            m->albedoTexture = texAssetName;
                        }
                    } else {
                        std::cerr << "Warning: material referenced diffuse texture '"
                                  << texRel.C_Str() << "' but it could not be resolved.\n"
                                  << "Tried relative to: " << objDir.string() << " and asset search paths.\n";
                    }
                }
            }
        }
    }

    return mesh;
}

Mesh* AssetSystem::createMesh(const std::string& name, const MeshData& data) {
    if (hasMesh(name)) return getMesh(name);

    if (data.vertices.empty() || data.indices.empty()) {
        std::cerr << "Cannot create mesh '" << name << "' with empty data\n";
        return nullptr;
    }

    auto mesh = std::make_unique<Mesh>();
    mesh->name = name;
    mesh->numIndices = static_cast<uint32_t>(data.indices.size());

    mesh->minX = data.minX; mesh->minY = data.minY; mesh->minZ = data.minZ;
    mesh->maxX = data.maxX; mesh->maxY = data.maxY; mesh->maxZ = data.maxZ;

    const bgfx::Memory* vmem = bgfx::copy(
        data.vertices.data(),
        static_cast<uint32_t>(data.vertices.size() * sizeof(PosNormalVertex))
    );
    mesh->vbh = bgfx::createVertexBuffer(vmem, PosNormalVertex::getLayout());

    const bgfx::Memory* imem = bgfx::copy(
        data.indices.data(),
        static_cast<uint32_t>(data.indices.size() * sizeof(uint16_t))
    );
    mesh->ibh = bgfx::createIndexBuffer(imem);

    if (!mesh->isValid()) {
        std::cerr << "Failed to create GPU buffers for mesh '" << name << "'\n";
        mesh->destroy();
        return nullptr;
    }

    Mesh* ptr = mesh.get();
    m_meshes[name] = std::move(mesh);
    return ptr;
}

Mesh* AssetSystem::getMesh(const std::string& name) {
    auto it = m_meshes.find(name);
    return (it != m_meshes.end()) ? it->second.get() : nullptr;
}

const Mesh* AssetSystem::getMesh(const std::string& name) const {
    auto it = m_meshes.find(name);
    return (it != m_meshes.end()) ? it->second.get() : nullptr;
}

bool AssetSystem::hasMesh(const std::string& name) const {
    return m_meshes.find(name) != m_meshes.end();
}

void AssetSystem::removeMesh(const std::string& name) {
    auto it = m_meshes.find(name);
    if (it != m_meshes.end()) {
        if (it->second) it->second->destroy();
        m_meshes.erase(it);
    }
}

// ============================================================
// Primitive Mesh Creators
// ============================================================

Mesh* AssetSystem::createCubeMesh(const std::string& name, float size) {
    if (hasMesh(name)) return getMesh(name);
    MeshData data = primitives::createCube(size);
    return createMesh(name, data);
}

Mesh* AssetSystem::createSphereMesh(const std::string& name, float radius, int segments) {
    if (hasMesh(name)) return getMesh(name);
    MeshData data = primitives::createSphere(radius, segments);
    return createMesh(name, data);
}

Mesh* AssetSystem::createPlaneMesh(const std::string& name, float width, float height) {
    if (hasMesh(name)) return getMesh(name);
    MeshData data = primitives::createPlane(width, height);
    return createMesh(name, data);
}

Mesh* AssetSystem::createCylinderMesh(const std::string& name, float radius, float height, int segments) {
    if (hasMesh(name)) return getMesh(name);
    MeshData data = primitives::createCylinder(radius, height, segments);
    return createMesh(name, data);
}

Mesh* AssetSystem::createConeMesh(const std::string& name, float radius, float height, int segments) {
    if (hasMesh(name)) return getMesh(name);
    MeshData data = primitives::createCone(radius, height, segments);
    return createMesh(name, data);
}

// ============================================================
// Texture
// ============================================================

Texture* AssetSystem::loadTexture(const std::string& name,
                                 const std::string& filepath,
                                 uint64_t flags) {
    if (hasTexture(name)) {
        return getTexture(name);
    }

    ImageData img = loadImageFromFile(filepath);
    if (!img.data) {
        std::cerr << "Failed to load texture '" << name << "' from: " << filepath << "\n";
        return nullptr;
    }

    bgfx::TextureFormat::Enum format = bgfx::TextureFormat::RGBA8;
    if (img.channels == 1) format = bgfx::TextureFormat::R8;
    else if (img.channels == 3) format = bgfx::TextureFormat::RGB8;
    else if (img.channels == 4) format = bgfx::TextureFormat::RGBA8;

    Texture* tex = createTexture(name, img.width, img.height, img.data, format, flags);
    img.free();
    return tex;
}

Texture* AssetSystem::createTexture(const std::string& name,
                                   uint32_t width,
                                   uint32_t height,
                                   const void* data,
                                   bgfx::TextureFormat::Enum format,
                                   uint64_t flags) {
    if (hasTexture(name)) return getTexture(name);
    if (!data || width == 0 || height == 0) return nullptr;

    auto tex = std::make_unique<Texture>();
    tex->name = name;
    tex->width = width;
    tex->height = height;
    tex->flags = flags;

    const size_t bpp = getBytesPerPixel(format);
    const bgfx::Memory* mem = bgfx::copy(data, static_cast<uint32_t>(width * height * bpp));

    tex->handle = bgfx::createTexture2D(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        false, 1, format, flags, mem
    );

    if (!tex->isValid()) {
        std::cerr << "Failed to create texture '" << name << "'\n";
        return nullptr;
    }

    Texture* ptr = tex.get();
    m_textures[name] = std::move(tex);
    return ptr;
}

Texture* AssetSystem::getTexture(const std::string& name) {
    auto it = m_textures.find(name);
    return (it != m_textures.end()) ? it->second.get() : nullptr;
}

const Texture* AssetSystem::getTexture(const std::string& name) const {
    auto it = m_textures.find(name);
    return (it != m_textures.end()) ? it->second.get() : nullptr;
}

bool AssetSystem::hasTexture(const std::string& name) const {
    return m_textures.find(name) != m_textures.end();
}

void AssetSystem::removeTexture(const std::string& name) {
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        if (it->second) it->second->destroy();
        m_textures.erase(it);
    }
}

// ============================================================
// Material
// ============================================================

Material* AssetSystem::createMaterial(const std::string& name, bgfx::ProgramHandle program) {
    if (hasMaterial(name)) return getMaterial(name);

    auto material = std::make_unique<Material>();
    material->name = name;
    material->program = program;

    Material* ptr = material.get();
    m_materials[name] = std::move(material);
    return ptr;
}

Material* AssetSystem::getMaterial(const std::string& name) {
    auto it = m_materials.find(name);
    return (it != m_materials.end()) ? it->second.get() : nullptr;
}

const Material* AssetSystem::getMaterial(const std::string& name) const {
    auto it = m_materials.find(name);
    return (it != m_materials.end()) ? it->second.get() : nullptr;
}

bool AssetSystem::hasMaterial(const std::string& name) const {
    return m_materials.find(name) != m_materials.end();
}

void AssetSystem::removeMaterial(const std::string& name) {
    auto it = m_materials.find(name);
    if (it != m_materials.end()) m_materials.erase(it);
}

// ============================================================
// Shaders / Programs
// ============================================================

bgfx::ShaderHandle AssetSystem::loadShader(const std::string& filepath) {
    const std::string realPath = resolvePath(filepath);

    std::ifstream file(realPath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open shader file: " << realPath << "\n";
        return BGFX_INVALID_HANDLE;
    }

    file.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<char> data(size);
    file.read(data.data(), size);

    const bgfx::Memory* mem = bgfx::copy(data.data(), static_cast<uint32_t>(size));
    return bgfx::createShader(mem);
}

bgfx::ProgramHandle AssetSystem::loadProgram(const std::string& name,
                                            const std::string& vsPath,
                                            const std::string& fsPath) {
    if (hasProgram(name)) return getProgram(name);

    bgfx::ShaderHandle vs = loadShader(vsPath);
    bgfx::ShaderHandle fs = loadShader(fsPath);

    if (!bgfx::isValid(vs) || !bgfx::isValid(fs)) {
        if (bgfx::isValid(vs)) bgfx::destroy(vs);
        if (bgfx::isValid(fs)) bgfx::destroy(fs);
        return BGFX_INVALID_HANDLE;
    }

    bgfx::ProgramHandle program = bgfx::createProgram(vs, fs, true);
    if (!bgfx::isValid(program)) return BGFX_INVALID_HANDLE;

    m_programs[name] = program;
    return program;
}

bgfx::ProgramHandle AssetSystem::getProgram(const std::string& name) const {
    auto it = m_programs.find(name);
    if (it != m_programs.end()) return it->second;
    return BGFX_INVALID_HANDLE;
}

bool AssetSystem::hasProgram(const std::string& name) const {
    return m_programs.find(name) != m_programs.end();
}

void AssetSystem::removeProgram(const std::string& name) {
    auto it = m_programs.find(name);
    if (it != m_programs.end()) {
        if (bgfx::isValid(it->second)) bgfx::destroy(it->second);
        m_programs.erase(it);
    }
}

// ============================================================
// Utility / Clear
// ============================================================

void AssetSystem::clear() {
    for (auto& [_, mesh] : m_meshes) if (mesh) mesh->destroy();
    m_meshes.clear();

    for (auto& [_, tex] : m_textures) if (tex) tex->destroy();
    m_textures.clear();

    for (auto& [_, prog] : m_programs) if (bgfx::isValid(prog)) bgfx::destroy(prog);
    m_programs.clear();

    m_materials.clear();
}

// ============================================================
// Image + Mesh file loading
// ============================================================

void AssetSystem::ImageData::free() {
    if (data) {
        stbi_image_free(data);
        data = nullptr;
    }
}

AssetSystem::ImageData AssetSystem::loadImageFromFile(const std::string& filepath) {
    ImageData out;

    const std::string real = resolvePath(filepath);

    int w = 0, h = 0, c = 0;
    out.data = stbi_load(real.c_str(), &w, &h, &c, 0);
    if (!out.data) {
        std::cerr << "Failed to load image: " << real << "\n";
        std::cerr << "stb_image: " << stbi_failure_reason() << "\n";
        return out;
    }

    out.width = static_cast<uint32_t>(w);
    out.height = static_cast<uint32_t>(h);
    out.channels = static_cast<uint32_t>(c);
    return out;
}

MeshData AssetSystem::loadMeshFromFile(const std::string& filepath) {
    MeshData result;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filepath,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices
    );

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        std::cerr << "Assimp error: " << importer.GetErrorString() << "\n";
        return result;
    }

    // first mesh only (simple)
    if (scene->mNumMeshes > 0) {
        aiMesh* mesh = scene->mMeshes[0];

        result.vertices.reserve(mesh->mNumVertices);
        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            PosNormalVertex v{};
            v.x = mesh->mVertices[i].x;
            v.y = mesh->mVertices[i].y;
            v.z = mesh->mVertices[i].z;

            if (mesh->HasNormals()) {
                v.nx = mesh->mNormals[i].x;
                v.ny = mesh->mNormals[i].y;
                v.nz = mesh->mNormals[i].z;
            } else {
                v.nx = 0.0f; v.ny = 1.0f; v.nz = 0.0f;
            }

            result.vertices.push_back(v);
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                result.indices.push_back(static_cast<uint16_t>(face.mIndices[j]));
            }
        }

        result.calculateBounds();
    }

    return result;
}

size_t AssetSystem::getBytesPerPixel(bgfx::TextureFormat::Enum format) {
    switch (format) {
        case bgfx::TextureFormat::R8:    return 1;
        case bgfx::TextureFormat::RGB8:  return 3;
        case bgfx::TextureFormat::RGBA8: return 4;
        default:                         return 4;
    }
}

} // namespace assets