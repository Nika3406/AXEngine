#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct StaticSceneVertex {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float normal[3]   = {0.0f, 1.0f, 0.0f};
    float uv[2]       = {0.0f, 0.0f};
};

struct StaticSceneImageData {
    std::string name;
    std::string uri;

    int width     = 0;
    int height    = 0;
    int component = 0; // 1=R, 2=RG, 3=RGB, 4=RGBA
    int bits      = 8;

    std::vector<unsigned char> pixels;

    bool valid() const {
        return width > 0 && height > 0 && component > 0 && !pixels.empty();
    }
};

struct StaticSceneMaterial {
    std::string name;
    float baseColorFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    // Texture image indices into StaticSceneAsset::images (-1 = none).
    // glTF material.pbrMetallicRoughness.baseColorTexture points to a texture,
    // which points to an image. Environment.cpp resolves it to this image index.
    int baseColorTextureIndex = -1;

    float emissiveFactor[3] = {0.0f, 0.0f, 0.0f};
    int emissiveTextureIndex = -1;

    // Explicit AX material identity.  These are derived from GLB material
    // metadata / naming and used by the renderer instead of guessing from
    // brightness.  A white wall can stay white without becoming a bloom source.
    bool isBloomSource = false;
    bool isEmissiveSource = false;
    float emissiveStrength = 0.0f;

    bool doubleSided  = false;
    bool alphaBlend   = false; // alphaMode == BLEND
    bool alphaMask    = false; // alphaMode == MASK
    float alphaCutoff = 0.5f;
    std::string alphaMode = "OPAQUE";
};

struct StaticScenePrimitive {
    std::vector<StaticSceneVertex> vertices;
    std::vector<uint32_t> indices;

    int primitiveIndex = -1;
    int nodeIndex = -1;
    std::string nodeName;

    int meshIndex = -1;
    std::string meshName;

    int materialIndex = -1;
    std::string materialName;

    float nodeTransform[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
};

struct StaticSceneAsset {
    std::string sourcePath;
    std::vector<StaticScenePrimitive> primitives;
    std::vector<StaticSceneMaterial> materials;
    std::vector<StaticSceneImageData> images;
};
