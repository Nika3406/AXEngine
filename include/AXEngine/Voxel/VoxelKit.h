#pragma once

#include "AXEngine/Core.h"
#include "runtime/AXGameplayPrimitives.h"

namespace ax::voxel {

// Public voxel API header. Keep this independent from internal runtime headers
// so user scripts can include AXEngine/AXEngine.h without pulling AXComponentModel.h
// and redefining ax::Vec3. Runtime voxel implementations stay behind src/runtime.

struct StructurePlacementRequest {
    String structureAsset;
    Vec3 position{};
    float yawDeg = 0.0f;
    String biome;
    float blendRadius = 4.0f;
};

namespace Events {
    inline constexpr const char* ChunkGenerated = "OnChunkGenerated";
    inline constexpr const char* StructurePlaced = "OnStructurePlaced";
    inline constexpr const char* VoxelEdited = "OnVoxelEdited";
}

} // namespace ax::voxel
