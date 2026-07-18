#include "assets/AssetManager.h"
#include "OGL3D/Texture2D.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <unordered_map>
#include <SDL3/SDL.h>

// ─────────────────────────────────────────────────────────────
//  Path helpers
// ─────────────────────────────────────────────────────────────
static std::string resolveAssetPath(const std::string& path) {
    const char* base = SDL_GetBasePath();
    if (!base) return path;
    return std::string(base) + path;
}

// ─────────────────────────────────────────────────────────────
//  Internal accessor helpers
// ─────────────────────────────────────────────────────────────
namespace {


// ─────────────────────────────────────────────────────────────
//  Column-major matrix helpers for glTF node hierarchy
// ─────────────────────────────────────────────────────────────
void mat4Identity(float* m) {
    memset(m, 0, 64);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void mat4Copy(const float* src, float* dst) {
    memcpy(dst, src, 64);
}

void mat4Mul(const float* a, const float* b, float* out) {
    float t[16] = {};
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            for (int k = 0; k < 4; ++k)
                t[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
    memcpy(out, t, 64);
}

bool mat4Inverse(const float* m, float* invOut) {
    float inv[16];

    inv[0] = m[5]  * m[10] * m[15] -
             m[5]  * m[11] * m[14] -
             m[9]  * m[6]  * m[15] +
             m[9]  * m[7]  * m[14] +
             m[13] * m[6]  * m[11] -
             m[13] * m[7]  * m[10];

    inv[4] = -m[4]  * m[10] * m[15] +
              m[4]  * m[11] * m[14] +
              m[8]  * m[6]  * m[15] -
              m[8]  * m[7]  * m[14] -
              m[12] * m[6]  * m[11] +
              m[12] * m[7]  * m[10];

    inv[8] = m[4]  * m[9] * m[15] -
             m[4]  * m[11] * m[13] -
             m[8]  * m[5] * m[15] +
             m[8]  * m[7] * m[13] +
             m[12] * m[5] * m[11] -
             m[12] * m[7] * m[9];

    inv[12] = -m[4]  * m[9] * m[14] +
               m[4]  * m[10] * m[13] +
               m[8]  * m[5] * m[14] -
               m[8]  * m[6] * m[13] -
               m[12] * m[5] * m[10] +
               m[12] * m[6] * m[9];

    inv[1] = -m[1]  * m[10] * m[15] +
              m[1]  * m[11] * m[14] +
              m[9]  * m[2] * m[15] -
              m[9]  * m[3] * m[14] -
              m[13] * m[2] * m[11] +
              m[13] * m[3] * m[10];

    inv[5] = m[0]  * m[10] * m[15] -
             m[0]  * m[11] * m[14] -
             m[8]  * m[2] * m[15] +
             m[8]  * m[3] * m[14] +
             m[12] * m[2] * m[11] -
             m[12] * m[3] * m[10];

    inv[9] = -m[0]  * m[9] * m[15] +
              m[0]  * m[11] * m[13] +
              m[8]  * m[1] * m[15] -
              m[8]  * m[3] * m[13] -
              m[12] * m[1] * m[11] +
              m[12] * m[3] * m[9];

    inv[13] = m[0]  * m[9] * m[14] -
              m[0]  * m[10] * m[13] -
              m[8]  * m[1] * m[14] +
              m[8]  * m[2] * m[13] +
              m[12] * m[1] * m[10] -
              m[12] * m[2] * m[9];

    inv[2] = m[1]  * m[6] * m[15] -
             m[1]  * m[7] * m[14] -
             m[5]  * m[2] * m[15] +
             m[5]  * m[3] * m[14] +
             m[13] * m[2] * m[7] -
             m[13] * m[3] * m[6];

    inv[6] = -m[0]  * m[6] * m[15] +
              m[0]  * m[7] * m[14] +
              m[4]  * m[2] * m[15] -
              m[4]  * m[3] * m[14] -
              m[12] * m[2] * m[7] +
              m[12] * m[3] * m[6];

    inv[10] = m[0]  * m[5] * m[15] -
              m[0]  * m[7] * m[13] -
              m[4]  * m[1] * m[15] +
              m[4]  * m[3] * m[13] +
              m[12] * m[1] * m[7] -
              m[12] * m[3] * m[5];

    inv[14] = -m[0]  * m[5] * m[14] +
               m[0]  * m[6] * m[13] +
               m[4]  * m[1] * m[14] -
               m[4]  * m[2] * m[13] -
               m[12] * m[1] * m[6] +
               m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] +
              m[1] * m[7] * m[10] +
              m[5] * m[2] * m[11] -
              m[5] * m[3] * m[10] -
              m[9] * m[2] * m[7] +
              m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] -
             m[0] * m[7] * m[10] -
             m[4] * m[2] * m[11] +
             m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] -
             m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] +
               m[0] * m[7] * m[9] +
               m[4] * m[1] * m[11] -
               m[4] * m[3] * m[9] -
               m[8] * m[1] * m[7] +
               m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] -
              m[0] * m[6] * m[9] -
              m[4] * m[1] * m[10] +
              m[4] * m[2] * m[9] +
              m[8] * m[1] * m[6] -
              m[8] * m[2] * m[5];

    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (fabsf(det) < 1e-8f) {
        mat4Identity(invOut);
        return false;
    }

    det = 1.0f / det;
    for (int i = 0; i < 16; i++) invOut[i] = inv[i] * det;
    return true;
}

void mat4FromTRS(const float* t, const float* r, const float* sc, float* m) {
    float x=r[0], y=r[1], z=r[2], w=r[3];
    float x2=x+x, y2=y+y, z2=z+z;
    float xx=x*x2, xy=x*y2, xz=x*z2;
    float yy=y*y2, yz=y*z2, zz=z*z2;
    float wx=w*x2, wy=w*y2, wz=w*z2;

    m[ 0]=(1-(yy+zz))*sc[0]; m[ 1]=(xy+wz)*sc[0];     m[ 2]=(xz-wy)*sc[0];     m[ 3]=0;
    m[ 4]=(xy-wz)*sc[1];     m[ 5]=(1-(xx+zz))*sc[1]; m[ 6]=(yz+wx)*sc[1];     m[ 7]=0;
    m[ 8]=(xz+wy)*sc[2];     m[ 9]=(yz-wx)*sc[2];     m[10]=(1-(xx+yy))*sc[2]; m[11]=0;
    m[12]=t[0];              m[13]=t[1];              m[14]=t[2];              m[15]=1;
}

void buildNodeLocalMatrix(const tinygltf::Node& node, float* out) {
    if (node.matrix.size() == 16) {
        for (int i = 0; i < 16; ++i) out[i] = static_cast<float>(node.matrix[i]);
        return;
    }

    float t[3] = {0.0f, 0.0f, 0.0f};
    float r[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float sc[3] = {1.0f, 1.0f, 1.0f};

    if (node.translation.size() == 3) {
        t[0] = static_cast<float>(node.translation[0]);
        t[1] = static_cast<float>(node.translation[1]);
        t[2] = static_cast<float>(node.translation[2]);
    }
    if (node.rotation.size() == 4) {
        r[0] = static_cast<float>(node.rotation[0]);
        r[1] = static_cast<float>(node.rotation[1]);
        r[2] = static_cast<float>(node.rotation[2]);
        r[3] = static_cast<float>(node.rotation[3]);
    }
    if (node.scale.size() == 3) {
        sc[0] = static_cast<float>(node.scale[0]);
        sc[1] = static_cast<float>(node.scale[1]);
        sc[2] = static_cast<float>(node.scale[2]);
    }

    mat4FromTRS(t, r, sc, out);
}

// Returns a typed pointer into the buffer at accessor[index].
// Handles byteStride (interleaved buffers).
template<typename T>
const T* accessorPtr(const tinygltf::Model& model, int accessorIdx, size_t elementIndex) {
    const auto& acc  = model.accessors[accessorIdx];
    const auto& view = model.bufferViews[acc.bufferView];
    const auto& buf  = model.buffers[view.buffer];

    size_t stride = view.byteStride > 0
        ? view.byteStride
        : sizeof(T); // tightly packed

    size_t byteOffset = view.byteOffset + acc.byteOffset + elementIndex * stride;
    return reinterpret_cast<const T*>(buf.data.data() + byteOffset);
}


// Read a vec3 from a float accessor. Handles tightly-packed and interleaved buffers.
void readVec3(const tinygltf::Model& m, int accIdx, size_t i, float* out) {
    const auto& acc  = m.accessors[accIdx];
    const auto& view = m.bufferViews[acc.bufferView];
    const auto& buf  = m.buffers[view.buffer];
    size_t stride = view.byteStride > 0 ? view.byteStride : 3 * sizeof(float);
    const float* p = reinterpret_cast<const float*>(
        buf.data.data() + view.byteOffset + acc.byteOffset + i * stride);
    out[0]=p[0]; out[1]=p[1]; out[2]=p[2];
}

// Read a vec2 from a float accessor. Handles tightly-packed and interleaved buffers.
void readVec2(const tinygltf::Model& m, int accIdx, size_t i, float* out) {
    const auto& acc  = m.accessors[accIdx];
    const auto& view = m.bufferViews[acc.bufferView];
    const auto& buf  = m.buffers[view.buffer];
    size_t stride = view.byteStride > 0 ? view.byteStride : 2 * sizeof(float);
    const float* p = reinterpret_cast<const float*>(
        buf.data.data() + view.byteOffset + acc.byteOffset + i * stride);
    out[0]=p[0]; out[1]=p[1];
}

// Read a vec4 from a float accessor. Handles tightly-packed and interleaved buffers.
void readVec4(const tinygltf::Model& m, int accIdx, size_t i, float* out) {
    const auto& acc  = m.accessors[accIdx];
    const auto& view = m.bufferViews[acc.bufferView];
    const auto& buf  = m.buffers[view.buffer];
    size_t stride = view.byteStride > 0 ? view.byteStride : 4 * sizeof(float);
    const float* p = reinterpret_cast<const float*>(
        buf.data.data() + view.byteOffset + acc.byteOffset + i * stride);
    out[0]=p[0]; out[1]=p[1]; out[2]=p[2]; out[3]=p[3];
}

// Read joint indices — component type may be UNSIGNED_BYTE or UNSIGNED_SHORT
void readJoints(const tinygltf::Model& model, int accIdx, size_t i, uint16_t* out) {
    const auto& acc  = model.accessors[accIdx];
    const auto& view = model.bufferViews[acc.bufferView];
    const auto& buf  = model.buffers[view.buffer];

    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        size_t stride = view.byteStride > 0 ? view.byteStride : 4;
        const uint8_t* p = buf.data.data() + view.byteOffset + acc.byteOffset + i * stride;
        out[0]=p[0]; out[1]=p[1]; out[2]=p[2]; out[3]=p[3];
    } else {
        // UNSIGNED_SHORT
        size_t stride = view.byteStride > 0 ? view.byteStride : 8;
        const uint16_t* p = reinterpret_cast<const uint16_t*>(
            buf.data.data() + view.byteOffset + acc.byteOffset + i * stride);
        out[0]=p[0]; out[1]=p[1]; out[2]=p[2]; out[3]=p[3];
    }
}

// Decode all indices from a primitive into uint32
void readIndices(const tinygltf::Model& model, int accIdx, std::vector<uint32_t>& out) {
    const auto& acc  = model.accessors[accIdx];
    const auto& view = model.bufferViews[acc.bufferView];
    const auto& buf  = model.buffers[view.buffer];
    const uint8_t* base = buf.data.data() + view.byteOffset + acc.byteOffset;

    out.resize(acc.count);
    switch (acc.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            for (size_t i=0;i<acc.count;++i) out[i] = base[i];
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            for (size_t i=0;i<acc.count;++i)
                out[i] = reinterpret_cast<const uint16_t*>(base)[i];
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            memcpy(out.data(), base, acc.count * 4);
            break;
        default: break;
    }
}

// Read all float values from an accessor into a flat vector
std::vector<float> readFloatAccessor(const tinygltf::Model& model, int accIdx) {
    const auto& acc  = model.accessors[accIdx];
    const auto& view = model.bufferViews[acc.bufferView];
    const auto& buf  = model.buffers[view.buffer];

    int components = tinygltf::GetNumComponentsInType(acc.type);
    std::vector<float> out;
    out.reserve(acc.count * components);

    size_t stride = view.byteStride > 0
        ? view.byteStride
        : static_cast<size_t>(components) * sizeof(float);

    for (size_t i = 0; i < acc.count; ++i) {
        const float* p = reinterpret_cast<const float*>(
            buf.data.data() + view.byteOffset + acc.byteOffset + i * stride);
        for (int c = 0; c < components; ++c)
            out.push_back(p[c]);
    }
    return out;
}

// Extract one primitive from a tinygltf mesh primitive
SkinnedPrimitive extractPrimitive(const tinygltf::Model& model,
                                   const tinygltf::Primitive& prim,
                                   // joint remapping: gltf node index → skeleton joint index
                                   const std::unordered_map<int,int>& nodeToJoint)
{
    SkinnedPrimitive out;
    out.materialIndex = prim.material;

    auto find = [&](const char* key) -> int {
        auto it = prim.attributes.find(key);
        return it != prim.attributes.end() ? it->second : -1;
    };

    int posAcc    = find("POSITION");
    int normAcc   = find("NORMAL");
    int uvAcc     = find("TEXCOORD_0");
    int tanAcc    = find("TANGENT");
    int jointsAcc = find("JOINTS_0");
    int weightsAcc= find("WEIGHTS_0");

    if (posAcc < 0) return out;

    size_t count = model.accessors[posAcc].count;
    out.vertices.resize(count);
    out.hasSkin = (jointsAcc >= 0 && weightsAcc >= 0);

    for (size_t i = 0; i < count; ++i) {
        SkinnedVertex& v = out.vertices[i];

        readVec3(model, posAcc, i, v.position);

        if (normAcc >= 0)
            readVec3(model, normAcc, i, v.normal);

        if (uvAcc >= 0)
            readVec2(model, uvAcc, i, v.uv);

        if (tanAcc >= 0)
            readVec4(model, tanAcc, i, v.tangent);

        if (out.hasSkin) {
            readJoints(model, jointsAcc, i, v.joints);
            readVec4(model, weightsAcc, i, v.weights);
            // Normalise weights
            float sum = v.weights[0]+v.weights[1]+v.weights[2]+v.weights[3];
            if (sum > 1e-6f) {
                v.weights[0]/=sum; v.weights[1]/=sum;
                v.weights[2]/=sum; v.weights[3]/=sum;
            }
        }
    }

    if (prim.indices >= 0)
        readIndices(model, prim.indices, out.indices);

    return out;
}

// Build Skeleton from the first skin in the model
Skeleton extractSkeleton(const tinygltf::Model& model,
                          std::unordered_map<int,int>& nodeToJoint)
{
    Skeleton skel;
    if (model.skins.empty()) return skel;

    const tinygltf::Skin& skin = model.skins[0];
    int jointCount = (int)skin.joints.size();
    skel.joints.resize(jointCount);

    // Map gltf node index → joint index
    for (int j = 0; j < jointCount; ++j)
        nodeToJoint[skin.joints[j]] = j;

    // Inverse bind matrices
    if (skin.inverseBindMatrices >= 0) {
        const auto& acc  = model.accessors[skin.inverseBindMatrices];
        const auto& view = model.bufferViews[acc.bufferView];
        const auto& buf  = model.buffers[view.buffer];
        const float* data = reinterpret_cast<const float*>(
            buf.data.data() + view.byteOffset + acc.byteOffset);

        for (int j = 0; j < jointCount; ++j)
            memcpy(skel.joints[j].inverseBindMatrix, data + j*16, 64);
    } else {
        // Identity fallback
        for (int j = 0; j < jointCount; ++j) {
            auto& m = skel.joints[j].inverseBindMatrix;
            memset(m, 0, 64);
            m[0]=m[5]=m[10]=m[15]=1.0f;
        }
    }

    // Node names, rest pose, parent indices
    for (int j = 0; j < jointCount; ++j) {
        int nodeIdx = skin.joints[j];
        const tinygltf::Node& node = model.nodes[nodeIdx];
        skel.joints[j].name = node.name;

        // Rest pose. glTF nodes may store either matrix[16] or separate T/R/S.
        // Animation clips target T/R/S channels, so convert matrix form into T/R/S here.
        if (node.matrix.size() == 16) {
            skel.joints[j].localTranslation[0] = (float)node.matrix[12];
            skel.joints[j].localTranslation[1] = (float)node.matrix[13];
            skel.joints[j].localTranslation[2] = (float)node.matrix[14];

            float sx = sqrtf((float)(node.matrix[0]*node.matrix[0] + node.matrix[1]*node.matrix[1] + node.matrix[2]*node.matrix[2]));
            float sy = sqrtf((float)(node.matrix[4]*node.matrix[4] + node.matrix[5]*node.matrix[5] + node.matrix[6]*node.matrix[6]));
            float sz = sqrtf((float)(node.matrix[8]*node.matrix[8] + node.matrix[9]*node.matrix[9] + node.matrix[10]*node.matrix[10]));
            if (sx < 1e-6f) sx = 1.0f;
            if (sy < 1e-6f) sy = 1.0f;
            if (sz < 1e-6f) sz = 1.0f;
            skel.joints[j].localScale[0] = sx;
            skel.joints[j].localScale[1] = sy;
            skel.joints[j].localScale[2] = sz;

            float r00=(float)node.matrix[0]/sx,  r10=(float)node.matrix[1]/sx,  r20=(float)node.matrix[2]/sx;
            float r01=(float)node.matrix[4]/sy,  r11=(float)node.matrix[5]/sy,  r21=(float)node.matrix[6]/sy;
            float r02=(float)node.matrix[8]/sz,  r12=(float)node.matrix[9]/sz,  r22=(float)node.matrix[10]/sz;
            float trace = r00 + r11 + r22;
            float qw, qx, qy, qz;
            if (trace > 0.0f) {
                float ss = 0.5f / sqrtf(trace + 1.0f);
                qw = 0.25f / ss; qx = (r21 - r12) * ss; qy = (r02 - r20) * ss; qz = (r10 - r01) * ss;
            } else if (r00 > r11 && r00 > r22) {
                float ss = 2.0f * sqrtf(1.0f + r00 - r11 - r22);
                qw = (r21 - r12) / ss; qx = 0.25f * ss; qy = (r01 + r10) / ss; qz = (r02 + r20) / ss;
            } else if (r11 > r22) {
                float ss = 2.0f * sqrtf(1.0f + r11 - r00 - r22);
                qw = (r02 - r20) / ss; qx = (r01 + r10) / ss; qy = 0.25f * ss; qz = (r12 + r21) / ss;
            } else {
                float ss = 2.0f * sqrtf(1.0f + r22 - r00 - r11);
                qw = (r10 - r01) / ss; qx = (r02 + r20) / ss; qy = (r12 + r21) / ss; qz = 0.25f * ss;
            }
            skel.joints[j].localRotation[0] = qx;
            skel.joints[j].localRotation[1] = qy;
            skel.joints[j].localRotation[2] = qz;
            skel.joints[j].localRotation[3] = qw;
        } else {
            if (node.translation.size()==3) {
                skel.joints[j].localTranslation[0]=(float)node.translation[0];
                skel.joints[j].localTranslation[1]=(float)node.translation[1];
                skel.joints[j].localTranslation[2]=(float)node.translation[2];
            }
            if (node.rotation.size()==4) {
                skel.joints[j].localRotation[0]=(float)node.rotation[0];
                skel.joints[j].localRotation[1]=(float)node.rotation[1];
                skel.joints[j].localRotation[2]=(float)node.rotation[2];
                skel.joints[j].localRotation[3]=(float)node.rotation[3];
            } else {
                skel.joints[j].localRotation[3]=1.0f; // identity quat
            }
            if (node.scale.size()==3) {
                skel.joints[j].localScale[0]=(float)node.scale[0];
                skel.joints[j].localScale[1]=(float)node.scale[1];
                skel.joints[j].localScale[2]=(float)node.scale[2];
            } else {
                skel.joints[j].localScale[0]=skel.joints[j].localScale[1]=skel.joints[j].localScale[2]=1.0f;
            }
        }
    }

    // Parent indices: walk all nodes looking for parent→child edges
    for (int nodeIdx = 0; nodeIdx < (int)model.nodes.size(); ++nodeIdx) {
        auto parentIt = nodeToJoint.find(nodeIdx);
        if (parentIt == nodeToJoint.end()) continue;
        int parentJoint = parentIt->second;

        for (int childNode : model.nodes[nodeIdx].children) {
            auto childIt = nodeToJoint.find(childNode);
            if (childIt == nodeToJoint.end()) continue;
            skel.joints[childIt->second].parentIndex = parentJoint;
        }
    }

    return skel;
}

// Extract all animation clips
std::vector<AnimationClip> extractAnimations(
    const tinygltf::Model& model,
    const std::unordered_map<int,int>& nodeToJoint)
{
    std::vector<AnimationClip> clips;

    for (const auto& anim : model.animations) {
        AnimationClip clip;
        clip.name = anim.name.empty() ? ("clip_" + std::to_string(clips.size())) : anim.name;

        for (const auto& ch : anim.channels) {
            if (ch.target_node < 0) continue;
            auto jit = nodeToJoint.find(ch.target_node);
            if (jit == nodeToJoint.end()) continue;

            AnimChannel animCh;
            animCh.jointIndex = jit->second;

            if      (ch.target_path == "translation") animCh.target = AnimTargetPath::Translation;
            else if (ch.target_path == "rotation")    animCh.target = AnimTargetPath::Rotation;
            else if (ch.target_path == "scale")       animCh.target = AnimTargetPath::Scale;
            else continue; // weights (morph targets) — skip for now

            const tinygltf::AnimationSampler& samp = anim.samplers[ch.sampler];

            if      (samp.interpolation == "STEP")        animCh.interp = AnimInterpolation::Step;
            else if (samp.interpolation == "CUBICSPLINE") animCh.interp = AnimInterpolation::CubicSpline;
            else                                           animCh.interp = AnimInterpolation::Linear;

            // Input = times
            animCh.times = readFloatAccessor(model, samp.input);

            // Output = values
            animCh.values = readFloatAccessor(model, samp.output);

            // Track duration
            if (!animCh.times.empty())
                clip.duration = std::max(clip.duration, animCh.times.back());

            clip.channels.push_back(std::move(animCh));
        }

        clips.push_back(std::move(clip));
    }

    return clips;
}

static std::string axLowerCopy(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

static bool axStartsWith(const std::string& value, const std::string& prefix) {
    std::string v = axLowerCopy(value);
    std::string p = axLowerCopy(prefix);
    return v.rfind(p, 0) == 0;
}

static bool axMaterialNameHasExplicitEmission(const std::string& name) {
    return axStartsWith(name, "BLOOM_") ||
           axStartsWith(name, "EMISSIVE_") ||
           axStartsWith(name, "GLOW_") ||
           axStartsWith(name, "AX_BLOOM_") ||
           axStartsWith(name, "AX_EMIT_") ||
           axStartsWith(name, "VFX_");
}

static bool axMaterialNameHasExplicitBloom(const std::string& name) {
    return axStartsWith(name, "BLOOM_") ||
           axStartsWith(name, "AX_BLOOM_") ||
           axStartsWith(name, "VFX_");
}

static float axEmissiveStrengthFromExtensions(const tinygltf::Material& mat) {
    auto it = mat.extensions.find("KHR_materials_emissive_strength");
    if (it == mat.extensions.end() || !it->second.IsObject()) {
        return 1.0f;
    }

    const auto& obj = it->second.Get<tinygltf::Value::Object>();
    auto valIt = obj.find("emissiveStrength");
    if (valIt != obj.end() && valIt->second.IsNumber()) {
        return static_cast<float>(valIt->second.Get<double>());
    }
    return 1.0f;
}

// Extract material metadata (no GPU upload — renderer handles that)
std::vector<GltfMaterial> extractMaterials(const tinygltf::Model& model) {
    std::vector<GltfMaterial> out;
    out.reserve(model.materials.size());

    for (const auto& mat : model.materials) {
        GltfMaterial m;
        m.name        = mat.name;
        m.doubleSided = mat.doubleSided;
        m.alphaMode   = mat.alphaMode.empty() ? "OPAQUE" : mat.alphaMode;
        m.alphaBlend  = (m.alphaMode == "BLEND");
        m.alphaMask   = (m.alphaMode == "MASK");
        m.alphaCutoff = (float)mat.alphaCutoff;

        const auto& pbr = mat.pbrMetallicRoughness;
        if (pbr.baseColorFactor.size()==4) {
            for (int i=0;i<4;++i) m.baseColorFactor[i]=(float)pbr.baseColorFactor[i];
        }
        m.metallicFactor  = (float)pbr.metallicFactor;
        m.roughnessFactor = (float)pbr.roughnessFactor;

        if (mat.emissiveFactor.size()==3)
            for (int i=0;i<3;++i) m.emissiveFactor[i]=(float)mat.emissiveFactor[i];

        const float emissiveMax = std::max(m.emissiveFactor[0],
                                   std::max(m.emissiveFactor[1], m.emissiveFactor[2]));
        const bool nameEmission = axMaterialNameHasExplicitEmission(m.name);
        const bool nameBloom = axMaterialNameHasExplicitBloom(m.name);
        const bool gltfEmission = emissiveMax > 0.001f || mat.emissiveTexture.index >= 0;

        m.isEmissiveSource = nameEmission || gltfEmission;
        m.isBloomSource = nameBloom || gltfEmission;
        m.emissiveStrength = m.isEmissiveSource
            ? std::max(1.0f, axEmissiveStrengthFromExtensions(mat))
            : 0.0f;

        if (pbr.baseColorTexture.index    >= 0) m.baseColorTextureIndex    = model.textures[pbr.baseColorTexture.index].source;
        if (pbr.metallicRoughnessTexture.index >= 0) m.metallicRoughnessIndex = model.textures[pbr.metallicRoughnessTexture.index].source;
        if (mat.normalTexture.index       >= 0) m.normalTextureIndex       = model.textures[mat.normalTexture.index].source;
        if (mat.occlusionTexture.index    >= 0) m.occlusionTextureIndex    = model.textures[mat.occlusionTexture.index].source;
        if (mat.emissiveTexture.index     >= 0) m.emissiveTextureIndex     = model.textures[mat.emissiveTexture.index].source;

        out.push_back(m);
    }
    return out;
}

std::vector<GltfImageData> extractImages(const tinygltf::Model& model) {
    std::vector<GltfImageData> out;
    out.reserve(model.images.size());

    for (int i = 0; i < (int)model.images.size(); ++i) {
        const tinygltf::Image& src = model.images[i];
        GltfImageData img;
        img.name      = src.name;
        img.uri       = src.uri;
        img.width     = src.width;
        img.height    = src.height;
        img.component = src.component;
        img.bits      = src.bits;
        img.pixels    = src.image;

        std::cout << "  Image " << i
                  << " name=\"" << img.name << "\""
                  << " uri=\"" << img.uri << "\""
                  << " size=" << img.width << "x" << img.height
                  << " comp=" << img.component
                  << " bits=" << img.bits
                  << " bytes=" << img.pixels.size()
                  << "\n";

        out.push_back(std::move(img));
    }

    return out;
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────
//  AssetManager implementation
// ─────────────────────────────────────────────────────────────
bool AssetManager::initialize() { return true; }

std::shared_ptr<GltfAsset> AssetManager::loadGltf(const std::string& path) {
    std::string full = resolveAssetPath(path);
    auto it = gltfAssets_.find(full);
    if (it != gltfAssets_.end()) return it->second;

    auto asset = gltfLoader_.loadFromFile(full);
    if (!asset) {
        std::cerr << "Failed to load glTF: " << full << "\n"
                  << "Reason: " << gltfLoader_.getLastError() << "\n";
        return nullptr;
    }
    gltfAssets_[full] = asset;
    return asset;
}

std::shared_ptr<GltfAsset> AssetManager::getGltf(const std::string& path) const {
    auto it = gltfAssets_.find(resolveAssetPath(path));
    return it != gltfAssets_.end() ? it->second : nullptr;
}

void AssetManager::unloadGltf(const std::string& path) {
    gltfAssets_.erase(resolveAssetPath(path));
}

// ─── buildFirstMeshFromGltf (unchanged, kept for static models) ───────────
std::shared_ptr<MeshAsset> AssetManager::buildFirstMeshFromGltf(const std::string& path) {
    auto asset = loadGltf(path);
    if (!asset) return nullptr;

    const auto& model = asset->model;
    if (model.meshes.empty()) { std::cerr << "No meshes in: " << path << "\n"; return nullptr; }

    const auto& primitive = model.meshes[0].primitives[0];
    auto posIt  = primitive.attributes.find("POSITION");
    auto normIt = primitive.attributes.find("NORMAL");
    auto uvIt   = primitive.attributes.find("TEXCOORD_0");
    if (posIt == primitive.attributes.end()) return nullptr;

    auto meshAsset = std::make_shared<MeshAsset>();

    const auto& posAccessor = model.accessors[posIt->second];
    const auto& posView     = model.bufferViews[posAccessor.bufferView];
    const auto& posBuffer   = model.buffers[posView.buffer];

    const auto& normAccessor = (normIt != primitive.attributes.end()) ? model.accessors[normIt->second] : posAccessor;
    const auto& normView     = model.bufferViews[normAccessor.bufferView];
    const auto& normBuffer   = model.buffers[normView.buffer];

    const auto& uvAccessor = (uvIt != primitive.attributes.end()) ? model.accessors[uvIt->second] : posAccessor;
    const auto& uvView     = model.bufferViews[uvAccessor.bufferView];
    const auto& uvBuffer   = model.buffers[uvView.buffer];

    size_t vertexCount = posAccessor.count;
    meshAsset->vertices.resize(vertexCount);

    for (size_t i = 0; i < vertexCount; ++i) {
        Vertex v{};
        const float* ps = reinterpret_cast<const float*>(
            posBuffer.data.data() + posView.byteOffset + posAccessor.byteOffset + i*12);
        v.position[0]=ps[0]; v.position[1]=ps[1]; v.position[2]=ps[2];

        if (normIt != primitive.attributes.end()) {
            const float* ns = reinterpret_cast<const float*>(
                normBuffer.data.data() + normView.byteOffset + normAccessor.byteOffset + i*12);
            v.normal[0]=ns[0]; v.normal[1]=ns[1]; v.normal[2]=ns[2];
        } else { v.normal[1]=1; }

        if (uvIt != primitive.attributes.end()) {
            const float* us = reinterpret_cast<const float*>(
                uvBuffer.data.data() + uvView.byteOffset + uvAccessor.byteOffset + i*8);
            v.uv[0]=us[0]; v.uv[1]=us[1];
        }
        meshAsset->vertices[i] = v;
    }

    if (primitive.indices >= 0) {
        const auto& idxAccessor = model.accessors[primitive.indices];
        const auto& idxView     = model.bufferViews[idxAccessor.bufferView];
        const auto& idxBuffer   = model.buffers[idxView.buffer];
        const unsigned char* idxData = idxBuffer.data.data() + idxView.byteOffset + idxAccessor.byteOffset;

        meshAsset->indices.resize(idxAccessor.count);
        if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            const unsigned short* src = reinterpret_cast<const unsigned short*>(idxData);
            for (size_t i=0;i<idxAccessor.count;++i) meshAsset->indices[i]=src[i];
        } else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            memcpy(meshAsset->indices.data(), idxData, idxAccessor.count*4);
        } else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            for (size_t i=0;i<idxAccessor.count;++i) meshAsset->indices[i]=idxData[i];
        }
    }
    return meshAsset;
}

// ─── buildSkinnedMesh ─────────────────────────────────────────────────────
std::shared_ptr<SkinnedMeshAsset> AssetManager::buildSkinnedMesh(const std::string& path) {
    std::string full = resolveAssetPath(path);

    auto cached = skinnedAssets_.find(full);
    if (cached != skinnedAssets_.end()) return cached->second;

    auto gltf = loadGltf(path);
    if (!gltf) return nullptr;

    const tinygltf::Model& model = gltf->model;
    auto result = std::make_shared<SkinnedMeshAsset>();
    result->sourcePath = full;

    // 1. Skeleton (must come first to build nodeToJoint map)
    std::unordered_map<int,int> nodeToJoint;
    result->skeleton = extractSkeleton(model, nodeToJoint);

    // 2. Materials + images. TinyGLTF loads both embedded GLB images and external URI images.
    result->materials = extractMaterials(model);
    result->images    = extractImages(model);

    // 3. Primitives — walk the glTF scene graph while preserving global node transforms.
    //    This follows the same core rule used by mature viewers/loaders like Three.js:
    //    keep the scene hierarchy alive instead of flattening meshes without their node parents.
    //
    //    Why this matters:
    //    - skinned body meshes are deformed by JOINTS_0/WEIGHTS_0
    //    - unweighted accessories/eyes/hair/props are often ordinary child-node meshes
    //    - if we drop node transforms, those child meshes appear at the model origin
    std::vector<float> jointGlobalBind(result->skeleton.joints.size() * 16, 0.0f);
    std::vector<unsigned char> hasJointGlobalBind(result->skeleton.joints.size(), 0);

    struct Visit {
        int nodeIndex = -1;
        float parentGlobal[16];
        int nearestJoint = -1;
    };

    std::vector<Visit> stack;

    auto pushRootNode = [&](int nodeIndex) {
        Visit v{};
        v.nodeIndex = nodeIndex;
        mat4Identity(v.parentGlobal);
        v.nearestJoint = -1;
        stack.push_back(v);
    };

    if (model.defaultScene >= 0 && model.defaultScene < (int)model.scenes.size()) {
        for (int n : model.scenes[model.defaultScene].nodes) pushRootNode(n);
    } else {
        for (const auto& scene : model.scenes)
            for (int n : scene.nodes)
                pushRootNode(n);
    }

    int primitiveCounter = 0;

    while (!stack.empty()) {
        Visit visit = stack.back();
        stack.pop_back();

        int nodeIdx = visit.nodeIndex;
        if (nodeIdx < 0 || nodeIdx >= (int)model.nodes.size()) continue;

        const tinygltf::Node& node = model.nodes[nodeIdx];

        float local[16];
        float global[16];
        buildNodeLocalMatrix(node, local);
        mat4Mul(visit.parentGlobal, local, global);

        int currentNearestJoint = visit.nearestJoint;
        auto jointIt = nodeToJoint.find(nodeIdx);
        if (jointIt != nodeToJoint.end()) {
            currentNearestJoint = jointIt->second;
            if (currentNearestJoint >= 0 && currentNearestJoint < (int)result->skeleton.joints.size()) {
                memcpy(&jointGlobalBind[currentNearestJoint * 16], global, 64);
                hasJointGlobalBind[currentNearestJoint] = 1;
            }
        }

        for (int child : node.children) {
            Visit childVisit{};
            childVisit.nodeIndex = child;
            memcpy(childVisit.parentGlobal, global, 64);
            childVisit.nearestJoint = currentNearestJoint;
            stack.push_back(childVisit);
        }

        if (node.mesh < 0) continue;
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];

        for (int localPrimIndex = 0; localPrimIndex < (int)mesh.primitives.size(); ++localPrimIndex) {
            const auto& tinyPrim = mesh.primitives[localPrimIndex];
            if (tinyPrim.attributes.find("POSITION") == tinyPrim.attributes.end()) continue;

            SkinnedPrimitive prim = extractPrimitive(model, tinyPrim, nodeToJoint);

            prim.primitiveIndex = primitiveCounter++;
            prim.nodeIndex = nodeIdx;
            prim.nodeName = node.name;
            prim.meshIndex = node.mesh;
            prim.meshName = mesh.name;
            prim.visible = true;
            if (prim.materialIndex >= 0 && prim.materialIndex < (int)result->materials.size())
                prim.materialName = result->materials[prim.materialIndex].name;

            mat4Identity(prim.nodeTransform);
            mat4Identity(prim.localFromJoint);
            prim.hasNodeTransform = false;
            prim.attachedJoint = -1;

            if (!prim.hasSkin) {
                // Unskinned child meshes still belong to the glTF node hierarchy.
                // Draw them with their node's global transform.
                memcpy(prim.nodeTransform, global, 64);
                prim.hasNodeTransform = true;

                // If this unskinned mesh lives under a joint, keep its offset from that joint.
                // Renderer will combine: animatedGlobalJoint * localFromJoint.
                if (currentNearestJoint >= 0 &&
                    currentNearestJoint < (int)result->skeleton.joints.size() &&
                    hasJointGlobalBind[currentNearestJoint]) {
                    float invJointGlobalBind[16];
                    mat4Inverse(&jointGlobalBind[currentNearestJoint * 16], invJointGlobalBind);
                    mat4Mul(invJointGlobalBind, global, prim.localFromJoint);
                    prim.attachedJoint = currentNearestJoint;
                } else {
                    prim.attachedJoint = -1;
                }
            }

            std::cout << "  Primitive " << prim.primitiveIndex
                      << " node=" << prim.nodeIndex << " \"" << prim.nodeName << "\""
                      << " mesh=" << prim.meshIndex << " \"" << prim.meshName << "\""
                      << " material=" << prim.materialIndex << " \"" << prim.materialName << "\""
                      << " skinned=" << (prim.hasSkin ? "yes" : "no")
                      << " attachedJoint=" << prim.attachedJoint
                      << "\n";

            result->primitives.push_back(std::move(prim));
        }
    }

    if (result->primitives.empty()) {
        std::cerr << "[AssetManager] buildSkinnedMesh: no primitives found in " << full << "\n";
        return nullptr;
    }

    // 4. Animations
    result->clips = extractAnimations(model, nodeToJoint);

    // Summary
    std::cout << "[AssetManager] Loaded: " << full << "\n"
              << "  Primitives : " << result->primitives.size() << "\n"
              << "  Joints     : " << result->skeleton.joints.size() << "\n"
              << "  Clips      : " << result->clips.size() << " (";
    for (const auto& c : result->clips) std::cout << c.name << " ";
    std::cout << ")\n"
              << "  Materials  : " << result->materials.size() << "\n"
              << "  Images     : " << result->images.size() << "\n";

    skinnedAssets_[full] = result;
    return result;
}

// ─── buildStaticScene ───────────────────────────────────────────────────
// Level/environment counterpart to buildSkinnedMesh(). Not cached - unlike
// a character mesh, a level's sceneScale/sceneObjects overrides can
// legitimately differ between calls (e.g. re-grouping in the editor between
// runs), so re-parsing on every call is the correct default here.
AXStaticSceneLoadResult AssetManager::buildStaticScene(
    const std::string& path,
    float sceneScale,
    const std::vector<AXSceneRuntimeObject>* sceneObjects
) {
    std::string full = resolveAssetPath(path);
    return loadStaticSceneGltf(full, sceneScale, sceneObjects);
}

// ─── Textures ─────────────────────────────────────────────────────────────
std::shared_ptr<Texture2D> AssetManager::loadTexture(const std::string& path) {
    std::string full = resolveAssetPath(path);
    auto it = textureAssets_.find(full);
    if (it != textureAssets_.end()) return it->second;

    auto tex = std::make_shared<Texture2D>();
    if (!tex->loadFromFile(full)) {
        std::cerr << "Failed to load texture: " << full << "\n";
        return nullptr;
    }
    textureAssets_[full] = tex;
    return tex;
}

std::shared_ptr<Texture2D> AssetManager::getTexture(const std::string& path) const {
    auto it = textureAssets_.find(resolveAssetPath(path));
    return it != textureAssets_.end() ? it->second : nullptr;
}

void AssetManager::clear() {
    gltfAssets_.clear();
    skinnedAssets_.clear();
    textureAssets_.clear();
}