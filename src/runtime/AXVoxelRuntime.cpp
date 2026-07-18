#include "runtime/AXVoxelRuntime.h"

#include <algorithm>
#include <cmath>

namespace ax {
namespace {
static float hashNoise(int x, int z, int seed) {
    uint32_t n = (uint32_t)(x * 374761393u + z * 668265263u + seed * 982451653u);
    n = (n ^ (n >> 13u)) * 1274126177u;
    n ^= (n >> 16u);
    return (float)(n & 0x00ffffffu) / (float)0x00ffffffu;
}
static float smoothNoise(float x, float z, int seed) {
    int ix = (int)std::floor(x);
    int iz = (int)std::floor(z);
    float fx = x - (float)ix;
    float fz = z - (float)iz;
    auto lerp = [](float a, float b, float t){ return a + (b - a) * t; };
    auto smooth = [](float t){ return t * t * (3.0f - 2.0f * t); };
    float a = hashNoise(ix, iz, seed);
    float b = hashNoise(ix + 1, iz, seed);
    float c = hashNoise(ix, iz + 1, seed);
    float d = hashNoise(ix + 1, iz + 1, seed);
    float sx = smooth(fx), sz = smooth(fz);
    return lerp(lerp(a, b, sx), lerp(c, d, sx), sz);
}
static bool solid(VoxelBlock b) { return b != VoxelBlock::Air && b != VoxelBlock::Water; }
static void addFace(VoxelMesh& mesh, float x, float y, float z, int face) {
    // face: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z
    const uint32_t start = (uint32_t)mesh.vertices.size();
    float nx=0, ny=0, nz=0;
    VoxelVertex v[4];
    auto set = [&](int i, float px, float py, float pz){ v[i].x=x+px; v[i].y=y+py; v[i].z=z+pz; v[i].nx=nx; v[i].ny=ny; v[i].nz=nz; v[i].u=(i==1||i==2)?1.0f:0.0f; v[i].v=(i>=2)?1.0f:0.0f; };
    switch(face) {
        case 0: nx=1;  set(0,1,0,0); set(1,1,0,1); set(2,1,1,1); set(3,1,1,0); break;
        case 1: nx=-1; set(0,0,0,1); set(1,0,0,0); set(2,0,1,0); set(3,0,1,1); break;
        case 2: ny=1;  set(0,0,1,1); set(1,1,1,1); set(2,1,1,0); set(3,0,1,0); break;
        case 3: ny=-1; set(0,0,0,0); set(1,1,0,0); set(2,1,0,1); set(3,0,0,1); break;
        case 4: nz=1;  set(0,1,0,1); set(1,0,0,1); set(2,0,1,1); set(3,1,1,1); break;
        case 5: nz=-1; set(0,0,0,0); set(1,1,0,0); set(2,1,1,0); set(3,0,1,0); break;
    }
    for (auto& vv : v) { vv.tile = 0.0f; mesh.vertices.push_back(vv); }
    mesh.indices.insert(mesh.indices.end(), {start, start+1, start+2, start, start+2, start+3});
}
}

VoxelTerrainDefinition VoxelTerrainSystem::fromComponent(const ComponentDefinition& component) {
    VoxelTerrainDefinition d;
    d.seed = (int)getFloatProperty(component, "seed", (float)d.seed);
    d.chunkRadius = (int)getFloatProperty(component, "chunkRadius", (float)d.chunkRadius);
    d.chunkSize = (int)getFloatProperty(component, "chunkSize", (float)d.chunkSize);
    d.worldHeight = (int)getFloatProperty(component, "worldHeight", (float)d.worldHeight);
    d.noiseScale = getFloatProperty(component, "noiseScale", d.noiseScale);
    d.heightScale = getFloatProperty(component, "heightScale", d.heightScale);
    d.waterLevel = (int)getFloatProperty(component, "waterLevel", (float)d.waterLevel);
    d.greedyMeshing = getBoolProperty(component, "greedyMeshing", d.greedyMeshing);
    d.lodEnabled = getBoolProperty(component, "lodEnabled", d.lodEnabled);
    d.lod0Distance = (int)getFloatProperty(component, "lod0Distance", (float)d.lod0Distance);
    d.lod1Distance = (int)getFloatProperty(component, "lod1Distance", (float)d.lod1Distance);
    d.atlas = getProperty(component, "atlas", d.atlas);
    return d;
}

int VoxelTerrainSystem::sampleHeight(int x, int z, const VoxelTerrainDefinition& def) {
    float n0 = smoothNoise(x * def.noiseScale, z * def.noiseScale, def.seed);
    float n1 = smoothNoise(x * def.noiseScale * 2.7f, z * def.noiseScale * 2.7f, def.seed + 31);
    float h = 8.0f + n0 * def.heightScale + n1 * def.heightScale * 0.35f;
    return std::clamp((int)std::round(h), 1, def.worldHeight - 2);
}

VoxelBlock VoxelTerrainSystem::sampleBlock(int x, int y, int z, const VoxelTerrainDefinition& def) {
    int h = sampleHeight(x, z, def);
    if (y > h) return y <= def.waterLevel ? VoxelBlock::Water : VoxelBlock::Air;
    if (y == h) return h <= def.waterLevel + 1 ? VoxelBlock::Sand : VoxelBlock::Grass;
    if (y > h - 4) return VoxelBlock::Dirt;
    return VoxelBlock::Stone;
}

VoxelMesh VoxelTerrainSystem::buildDebugChunkMesh(int chunkX, int chunkZ, const VoxelTerrainDefinition& def) {
    VoxelMesh mesh;
    int size = std::max(1, def.chunkSize);
    for (int z = 0; z < size; ++z) {
        for (int x = 0; x < size; ++x) {
            int wx = chunkX * size + x;
            int wz = chunkZ * size + z;
            int h = sampleHeight(wx, wz, def);
            for (int y = std::max(0, h - 2); y <= h; ++y) {
                VoxelBlock b = sampleBlock(wx, y, wz, def);
                if (!solid(b)) continue;
                if (!solid(sampleBlock(wx+1,y,wz,def))) addFace(mesh, (float)x, (float)y, (float)z, 0);
                if (!solid(sampleBlock(wx-1,y,wz,def))) addFace(mesh, (float)x, (float)y, (float)z, 1);
                if (!solid(sampleBlock(wx,y+1,wz,def))) addFace(mesh, (float)x, (float)y, (float)z, 2);
                if (!solid(sampleBlock(wx,y-1,wz,def))) addFace(mesh, (float)x, (float)y, (float)z, 3);
                if (!solid(sampleBlock(wx,y,wz+1,def))) addFace(mesh, (float)x, (float)y, (float)z, 4);
                if (!solid(sampleBlock(wx,y,wz-1,def))) addFace(mesh, (float)x, (float)y, (float)z, 5);
            }
        }
    }
    return mesh;
}

} // namespace ax
