#pragma once
#include "assets/GltfLoader.h"
#include "assets/GltfAsset.h"
#include "assets/MeshAsset.h"
#include "assets/SkinnedMeshAsset.h"
#include "assets/StaticSceneLoader.h"
#include <memory>
#include <string>
#include <unordered_map>

class Texture2D;

class AssetManager {
public:
    bool initialize();

    // ── Raw glTF access ───────────────────────────────────────
    std::shared_ptr<GltfAsset> loadGltf(const std::string& path);
    std::shared_ptr<GltfAsset> getGltf(const std::string& path) const;
    void unloadGltf(const std::string& path);

    // ── Static (non-skinned) mesh ─────────────────────────────
    // Builds only the first primitive of the first mesh node.
    std::shared_ptr<MeshAsset> buildFirstMeshFromGltf(const std::string& path);

    // ── Skinned mesh (new) ────────────────────────────────────
    // Extracts ALL primitives, the full skeleton, ALL animation
    // clips, and material metadata from a GLB/glTF file.
    std::shared_ptr<SkinnedMeshAsset> buildSkinnedMesh(const std::string& path);

    // ── Static scene / level geometry ─────────────────────────
    // The level-geometry counterpart to buildSkinnedMesh(): parses the GLB,
    // extracts materials/images/visible mesh/colliders/gameplay markers, and
    // recomputes smooth normals - all in engine core, not in scripts/.
    // `sceneObjects` (optional) is a loaded .axscene's object list; any
    // "NodeGroup"/"NodeRef" entries in it get resolved and applied here too.
    // Not cached - sceneScale/sceneObjects can legitimately differ between
    // calls for the same path (e.g. re-grouping a level in the editor).
    AXStaticSceneLoadResult buildStaticScene(
        const std::string& path,
        float sceneScale = 1.0f,
        const std::vector<AXSceneRuntimeObject>* sceneObjects = nullptr
    );

    // ── Textures ──────────────────────────────────────────────
    std::shared_ptr<Texture2D> loadTexture(const std::string& path);
    std::shared_ptr<Texture2D> getTexture(const std::string& path) const;

    void clear();

private:
    GltfLoader gltfLoader_;

    std::unordered_map<std::string, std::shared_ptr<GltfAsset>>        gltfAssets_;
    std::unordered_map<std::string, std::shared_ptr<SkinnedMeshAsset>> skinnedAssets_;
    std::unordered_map<std::string, std::shared_ptr<Texture2D>>        textureAssets_;
};