#pragma once
#include <string>
#include <vector>
#include <array>
#include <cstdint>

// ─────────────────────────────────────────────────────────────
//  Vertex with skinning attributes
// ─────────────────────────────────────────────────────────────
struct SkinnedVertex {
    float position[3] = {0, 0, 0};
    float normal[3]   = {0, 1, 0};
    float uv[2]       = {0, 0};
    float tangent[4]  = {1, 0, 0, 1}; // xyz + handedness (w)

    uint16_t joints[4]  = {0, 0, 0, 0}; // joint indices (into Skeleton::joints)
    float    weights[4] = {1, 0, 0, 0}; // blend weights, must sum to 1
};

// ─────────────────────────────────────────────────────────────
//  A single joint in the skeleton
// ─────────────────────────────────────────────────────────────
struct Joint {
    std::string name;
    int         parentIndex = -1; // -1 = root

    // Inverse bind matrix: transforms from mesh space → joint local space
    // Stored column-major (matches GL / glTF convention)
    float inverseBindMatrix[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    // Node-local rest pose (T/R/S)
    float localTranslation[3] = {0, 0, 0};
    float localRotation[4]    = {0, 0, 0, 1}; // quaternion xyzw
    float localScale[3]       = {1, 1, 1};
};

// ─────────────────────────────────────────────────────────────
//  Flat joint hierarchy
// ─────────────────────────────────────────────────────────────
struct Skeleton {
    std::vector<Joint> joints; // indexed by joint index
};

// ─────────────────────────────────────────────────────────────
//  Animation keyframe channel
// ─────────────────────────────────────────────────────────────
enum class AnimTargetPath { Translation, Rotation, Scale };
enum class AnimInterpolation { Linear, Step, CubicSpline };

struct AnimChannel {
    int                jointIndex  = -1;
    AnimTargetPath     target      = AnimTargetPath::Translation;
    AnimInterpolation  interp      = AnimInterpolation::Linear;

    std::vector<float> times;          // keyframe times (seconds)
    // values: 3 floats per key for T/S, 4 floats per key for R,
    //         3x per key for CubicSpline (in-tan, value, out-tan)
    std::vector<float> values;
};

// ─────────────────────────────────────────────────────────────
//  Named animation clip (e.g. "Idle", "Walk", "Attack")
// ─────────────────────────────────────────────────────────────
struct AnimationClip {
    std::string            name;
    float                  duration = 0.0f; // seconds
    std::vector<AnimChannel> channels;
};


// ─────────────────────────────────────────────────────────────
//  Image data extracted by TinyGLTF from embedded GLB images or external URIs
// ─────────────────────────────────────────────────────────────
struct GltfImageData {
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

// ─────────────────────────────────────────────────────────────
//  Material extracted from GLB
// ─────────────────────────────────────────────────────────────
struct GltfMaterial {
    std::string name;

    // PBR metallic-roughness
    float baseColorFactor[4]  = {1, 1, 1, 1};
    float metallicFactor      = 1.0f;
    float roughnessFactor     = 1.0f;
    float emissiveFactor[3]   = {0, 0, 0};

    // Explicit AX material identity.  The renderer uses this instead of
    // brightness-based bloom guesses.  Blender can mark materials with names
    // like BLOOM_*, EMISSIVE_*, GLOW_*, AX_BLOOM_*, or AX_EMIT_*.
    bool isBloomSource        = false;
    bool isEmissiveSource     = false;
    float emissiveStrength    = 0.0f;

    // Texture image indices into GltfModel::images (-1 = none)
    int baseColorTextureIndex    = -1;
    int metallicRoughnessIndex   = -1;
    int normalTextureIndex       = -1;
    int occlusionTextureIndex    = -1;
    int emissiveTextureIndex     = -1;

    bool doubleSided             = false;
    bool alphaBlend              = false; // alphaMode == "BLEND"
    bool alphaMask               = false; // alphaMode == "MASK"
    float alphaCutoff            = 0.5f;
    std::string alphaMode        = "OPAQUE";
};

// ─────────────────────────────────────────────────────────────
//  One primitive (draw call) within a mesh node
// ─────────────────────────────────────────────────────────────
struct SkinnedPrimitive {
    std::vector<SkinnedVertex> vertices;
    std::vector<uint32_t> indices;

    int materialIndex = -1;
    bool hasSkin = false;

    // Debug/visibility metadata so scripts can inspect or hide GLB parts.
    int primitiveIndex = -1;
    int nodeIndex = -1;
    std::string nodeName;
    int meshIndex = -1;
    std::string meshName;
    std::string materialName;
    bool visible = true;

    float nodeTransform[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    bool hasNodeTransform = false;

    int attachedJoint = -1;

    float localFromJoint[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
};

// ─────────────────────────────────────────────────────────────
//  Complete skinned model asset (all meshes + skeleton + clips)
// ─────────────────────────────────────────────────────────────
struct SkinnedMeshAsset {
    std::string sourcePath;

    std::vector<SkinnedPrimitive> primitives; // all primitives, all mesh nodes
    Skeleton                      skeleton;
    std::vector<AnimationClip>    clips;
    std::vector<GltfMaterial>     materials;
    std::vector<GltfImageData>    images;

    // Convenience: find clip index by name (-1 if not found)
    int findClip(const std::string& name) const {
        for (int i = 0; i < (int)clips.size(); ++i)
            if (clips[i].name == name) return i;
        return -1;
    }
};