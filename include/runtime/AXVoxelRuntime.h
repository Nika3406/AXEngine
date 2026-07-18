#pragma once

#include "runtime/AXComponentModel.h"
#include <cstdint>
#include <vector>
#include <string>

namespace ax {

enum class VoxelBlock : uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Sand,
    Water,
    Leaves,
    Log
};

struct VoxelVertex {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float nx = 0.0f, ny = 1.0f, nz = 0.0f;
    float u = 0.0f, v = 0.0f;
    float tile = 0.0f;
};

struct VoxelMesh {
    std::vector<VoxelVertex> vertices;
    std::vector<uint32_t> indices;
};

struct VoxelTerrainDefinition {
    int seed = 1337;
    int chunkRadius = 6;
    int chunkSize = 16;
    int worldHeight = 128;
    float noiseScale = 0.025f;
    float heightScale = 28.0f;
    int waterLevel = 22;
    bool greedyMeshing = true;
    bool lodEnabled = true;
    int lod0Distance = 32;
    int lod1Distance = 96;
    std::string atlas = "assets/textures/terrain_atlas.png";
};

class VoxelTerrainSystem {
public:
    static VoxelTerrainDefinition fromComponent(const ComponentDefinition& component);
    static int sampleHeight(int x, int z, const VoxelTerrainDefinition& def);
    static VoxelBlock sampleBlock(int x, int y, int z, const VoxelTerrainDefinition& def);
    static VoxelMesh buildDebugChunkMesh(int chunkX, int chunkZ, const VoxelTerrainDefinition& def);
};

} // namespace ax
