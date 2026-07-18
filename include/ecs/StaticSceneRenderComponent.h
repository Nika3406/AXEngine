#pragma once

#include <glad/glad.h>
#include "ecs/Component.h"
#include "assets/StaticSceneAsset.h"

#include <memory>
#include <vector>
#include <string>

struct StaticSceneGpuMaterial {
    unsigned int baseColorTexture = 0;
    bool hasBaseColorTexture = false;
};

struct StaticSceneGpuPrimitive {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;

    unsigned int indexCount = 0;
    unsigned int vertexCount = 0;

    int materialIndex = -1;
    bool visible = true;

    int primitiveIndex = -1;
    int nodeIndex = -1;
    std::string nodeName;
    int meshIndex = -1;
    std::string meshName;
    std::string materialName;

    float nodeTransform[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
};

struct StaticSceneRenderComponent : public Component {
    std::shared_ptr<StaticSceneAsset> asset;
    std::vector<StaticSceneGpuPrimitive> gpuPrimitives;
    std::vector<StaticSceneGpuMaterial> gpuMaterials;

    ~StaticSceneRenderComponent() {
        for (auto& p : gpuPrimitives) {
            if (p.vao) glDeleteVertexArrays(1, &p.vao);
            if (p.vbo) glDeleteBuffers(1, &p.vbo);
            if (p.ebo) glDeleteBuffers(1, &p.ebo);
            p.vao = p.vbo = p.ebo = 0;
        }

        for (auto& m : gpuMaterials) {
            if (m.baseColorTexture) {
                glDeleteTextures(1, &m.baseColorTexture);
                m.baseColorTexture = 0;
            }
            m.hasBaseColorTexture = false;
        }
    }
};
