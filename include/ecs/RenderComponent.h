#pragma once
#include "ecs/Component.h"
#include "ecs/TransformComponent.h" // for Vec3
#include "assets/MeshAsset.h"
#include <memory>

class RenderComponent : public Component {
public:
    std::shared_ptr<MeshAsset> mesh;
    Vec3 color = {1.0f, 1.0f, 1.0f}; // default white

    // GPU handles — filled by Renderer::uploadMesh()
    unsigned int vao         = 0;
    unsigned int vbo         = 0;
    unsigned int ebo         = 0;
    unsigned int indexCount  = 0;
    unsigned int vertexCount = 0;
    bool         useIndices  = false;
};
