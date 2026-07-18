#pragma once

// -----------------------------------------------------------------------------
// StaticSceneLoader — engine-owned GLB -> level geometry pipeline.
// -----------------------------------------------------------------------------
// This is the level/environment counterpart to GltfLoader + AssetManager's
// buildSkinnedMesh(): parsing, material/image extraction, collider
// extraction, gameplay-marker extraction, and normal computation for a level
// GLB all live here, in engine core, exactly like character mesh loading
// already does. scripts/Environment.cpp should only ever call
// AssetManager::buildStaticScene() and then decide what to DO with the
// result (attach a render component, apply a render profile, spawn things
// at marker positions) - it should never parse a GLB itself.
//
// "NodeGroup"/"NodeRef" objects (created via the editor's "Group Selected"
// action) are resolved here too: their position/rotation/scale, composed
// through NodeGroup parent chains, is applied to the matching GLB node(s) by
// name, on top of whatever is naturally baked into the GLB. Nodes with no
// matching NodeRef are completely unaffected.

#include "assets/StaticSceneAsset.h"
#include "runtime/AXSceneRuntime.h"

#include <memory>
#include <string>
#include <vector>

struct AXVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct AXColliderTriangle {
    AXVec3 a;
    AXVec3 b;
    AXVec3 c;
};

// Debug-visualization bounds for a collider object. NOT the authoritative
// collision shape - see AXMeshCollider for that.
struct AXCollisionAABB {
    float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
    float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
    std::string name;
    bool blocksPlayer = true;
    bool blocksCamera = true;
};

// Real triangle-mesh collider, extracted from COLLIDER_*/Invisible* GLB
// nodes.
struct AXMeshCollider {
    std::string name;
    std::vector<AXColliderTriangle> triangles;
};

// Gameplay marker authored as a GLB Empty/node name (ENEMY_*, PLAYER_START,
// VFX_*). Scripts decide what these mean; the engine only extracts the data.
struct AXSceneMarker {
    std::string name;
    std::string kind;
    std::string archetype;

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    float yawDeg = 0.0f;
    float scale = 1.0f;
};

struct AXStaticSceneLoadResult {
    bool ok = false;
    std::string error;

    std::shared_ptr<StaticSceneAsset> scene;
    std::vector<AXCollisionAABB> colliderDebugAABBs;
    std::vector<AXMeshCollider> meshColliders;
    std::vector<AXSceneMarker> sceneMarkers;
};

// `fullPath` must already be resolved to an on-disk path (base-path
// resolution is a script/runtime concern, not an asset-loading one).
// `sceneObjects` is the full authored object list from a loaded .axscene
// (AXSceneRuntimeData::objects); pass nullptr for "no NodeGroup/NodeRef
// overrides", which is byte-for-byte identical to loading with an object
// list that contains no groups at all.
AXStaticSceneLoadResult loadStaticSceneGltf(
    const std::string& fullPath,
    float sceneScale,
    const std::vector<AXSceneRuntimeObject>* sceneObjects
);
