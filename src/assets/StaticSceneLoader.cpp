#include "assets/StaticSceneLoader.h"

#include <tiny_gltf.h>

#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <limits>
#include <cctype>
#include <unordered_map>

namespace {

// ---------------------------------------------------------------------------
// String helpers
// ---------------------------------------------------------------------------

std::string lowerCopy(const std::string& s) {
    std::string out = s;
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool containsIgnoreCase(const std::string& value, const std::string& token) {
    return lowerCopy(value).find(lowerCopy(token)) != std::string::npos;
}

bool startsWithIgnoreCase(const std::string& value, const std::string& token) {
    std::string v = lowerCopy(value);
    std::string t = lowerCopy(token);
    return v.rfind(t, 0) == 0;
}

bool materialNameHasExplicitAxEmission(const std::string& name) {
    std::string n = lowerCopy(name);
    // Prefix-based by design. Generic words embedded in random material names
    // should not accidentally turn normal architecture into bloom/VFX surfaces.
    return startsWithIgnoreCase(n, "bloom_") ||
           startsWithIgnoreCase(n, "emissive_") ||
           startsWithIgnoreCase(n, "glow_") ||
           startsWithIgnoreCase(n, "ax_bloom_") ||
           startsWithIgnoreCase(n, "ax_emit_") ||
           startsWithIgnoreCase(n, "vfx_");
}

bool materialNameHasExplicitAxBloom(const std::string& name) {
    std::string n = lowerCopy(name);
    return startsWithIgnoreCase(n, "bloom_") ||
           startsWithIgnoreCase(n, "ax_bloom_") ||
           startsWithIgnoreCase(n, "vfx_");
}

float emissiveStrengthFromExtensions(const tinygltf::Material& mat) {
    auto it = mat.extensions.find("KHR_materials_emissive_strength");
    if (it == mat.extensions.end() || !it->second.IsObject()) {
        return 1.0f;
    }
    auto valIt = it->second.Get<tinygltf::Value::Object>().find("emissiveStrength");
    if (valIt == it->second.Get<tinygltf::Value::Object>().end()) {
        return 1.0f;
    }
    if (valIt->second.IsNumber()) {
        return static_cast<float>(valIt->second.Get<double>());
    }
    return 1.0f;
}

std::string markerArchetypeFromName(const std::string& name, const std::string& prefix) {
    if (name.size() <= prefix.size()) return "Basic";
    std::string out = name.substr(prefix.size());
    while (!out.empty() && (out[0] == '_' || out[0] == '-' || out[0] == ' ')) {
        out.erase(out.begin());
    }
    return out.empty() ? std::string("Basic") : out;
}

bool extractSceneMarker(const tinygltf::Node& node, const float* global, AXSceneMarker& out) {
    if (node.name.empty()) return false;

    if (startsWithIgnoreCase(node.name, "ENEMY")) {
        out.kind = "ENEMY";
        out.archetype = markerArchetypeFromName(node.name, "ENEMY");
    } else if (startsWithIgnoreCase(node.name, "AI")) {
        out.kind = "ENEMY";
        out.archetype = markerArchetypeFromName(node.name, "AI");
    } else if (startsWithIgnoreCase(node.name, "PLAYER_START")) {
        out.kind = "PLAYER_START";
        out.archetype = "Default";
    } else if (startsWithIgnoreCase(node.name, "VFX")) {
        out.kind = "VFX";
        out.archetype = markerArchetypeFromName(node.name, "VFX");
    } else {
        return false;
    }

    out.name = node.name;
    out.x = global[12];
    out.y = global[13];
    out.z = global[14];

    // Approximate yaw from the transformed local +Z axis. Enough for
    // authoring enemy facing in Blender empties.
    out.yawDeg = std::atan2(global[8], global[10]) * 180.0f / 3.14159265f;

    float sx = std::sqrt(global[0]*global[0] + global[1]*global[1] + global[2]*global[2]);
    float sy = std::sqrt(global[4]*global[4] + global[5]*global[5] + global[6]*global[6]);
    float sz = std::sqrt(global[8]*global[8] + global[9]*global[9] + global[10]*global[10]);
    out.scale = std::max(0.01f, (sx + sy + sz) / 3.0f);

    return true;
}

// COLLIDER_* should be enough. "Invisible" kept in case Blender exports a
// parent/empty with that name.
bool isColliderName(const std::string& nodeName, const std::string& meshName = "") {
    auto check = [](const std::string& s) {
        return containsIgnoreCase(s, "collider") || containsIgnoreCase(s, "invisible");
    };
    return check(nodeName) || check(meshName);
}

// ---------------------------------------------------------------------------
// Matrix math, column-major, matching OpenGL / glTF convention
// ---------------------------------------------------------------------------

void mat4Identity(float* m) {
    std::memset(m, 0, sizeof(float) * 16);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

// out = a * b, column-major
void mat4Mul(const float* a, const float* b, float* out) {
    float tmp[16] = {};
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            for (int k = 0; k < 4; ++k) {
                tmp[c*4+r] += a[k*4+r] * b[c*4+k];
            }
        }
    }
    std::memcpy(out, tmp, sizeof(float) * 16);
}

// Build column-major TRS matrix from translation, quaternion, scale.
void mat4FromTRS(const float* t, const float* q, const float* s, float* m) {
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    const float x2 = x + x, y2 = y + y, z2 = z + z;
    const float xx = x*x2, xy = x*y2, xz = x*z2;
    const float yy = y*y2, yz = y*z2, zz = z*z2;
    const float wx = w*x2, wy = w*y2, wz = w*z2;

    m[0]  = (1.0f - (yy + zz)) * s[0];
    m[1]  = (xy + wz)          * s[0];
    m[2]  = (xz - wy)          * s[0];
    m[3]  = 0.0f;

    m[4]  = (xy - wz)          * s[1];
    m[5]  = (1.0f - (xx + zz)) * s[1];
    m[6]  = (yz + wx)          * s[1];
    m[7]  = 0.0f;

    m[8]  = (xz + wy)          * s[2];
    m[9]  = (yz - wx)          * s[2];
    m[10] = (1.0f - (xx + yy)) * s[2];
    m[11] = 0.0f;

    m[12] = t[0];
    m[13] = t[1];
    m[14] = t[2];
    m[15] = 1.0f;
}

void buildNodeLocalMatrix(const tinygltf::Node& node, float* out) {
    if (node.matrix.size() == 16) {
        for (int i = 0; i < 16; ++i) out[i] = static_cast<float>(node.matrix[i]);
        return;
    }
    float t[3] = {0.0f, 0.0f, 0.0f};
    float r[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float s[3] = {1.0f, 1.0f, 1.0f};
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
        s[0] = static_cast<float>(node.scale[0]);
        s[1] = static_cast<float>(node.scale[1]);
        s[2] = static_cast<float>(node.scale[2]);
    }
    mat4FromTRS(t, r, s, out);
}

void transformPoint(const float* m, float x, float y, float z, float* out) {
    out[0] = m[0]*x + m[4]*y + m[8]*z  + m[12];
    out[1] = m[1]*x + m[5]*y + m[9]*z  + m[13];
    out[2] = m[2]*x + m[6]*y + m[10]*z + m[14];
}

// ---------------------------------------------------------------------------
// Accessor helpers
// ---------------------------------------------------------------------------

size_t componentSizeBytes(int componentType) {
    switch (componentType) {
        case TINYGLTF_COMPONENT_TYPE_FLOAT:          return sizeof(float);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  return sizeof(uint8_t);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return sizeof(uint16_t);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   return sizeof(uint32_t);
        default:                                     return sizeof(float);
    }
}

size_t accessorStrideBytes(const tinygltf::Model& model, const tinygltf::Accessor& accessor) {
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    if (view.byteStride > 0) return view.byteStride;
    int components = tinygltf::GetNumComponentsInType(accessor.type);
    return static_cast<size_t>(components) * componentSizeBytes(accessor.componentType);
}

const uint8_t* accessorElementPtr(const tinygltf::Model& model, int accessorIndex, size_t index) {
    if (accessorIndex < 0 || accessorIndex >= static_cast<int>(model.accessors.size()))
        return nullptr;
    const tinygltf::Accessor& acc = model.accessors[accessorIndex];
    if (acc.bufferView < 0 || acc.bufferView >= static_cast<int>(model.bufferViews.size()))
        return nullptr;
    const tinygltf::BufferView& view = model.bufferViews[acc.bufferView];
    if (view.buffer < 0 || view.buffer >= static_cast<int>(model.buffers.size()))
        return nullptr;
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];
    size_t stride = accessorStrideBytes(model, acc);
    size_t offset = view.byteOffset + acc.byteOffset + index * stride;
    if (offset >= buffer.data.size()) return nullptr;
    return buffer.data.data() + offset;
}

bool readVec2Float(const tinygltf::Model& model, int accessorIndex, size_t index, float* out) {
    if (accessorIndex < 0) return false;
    const tinygltf::Accessor& acc = model.accessors[accessorIndex];
    if (acc.type != TINYGLTF_TYPE_VEC2) return false;
    const uint8_t* p = accessorElementPtr(model, accessorIndex, index);
    if (!p) return false;

    // glTF allows TEXCOORD_0 to be FLOAT, UNSIGNED_BYTE, or UNSIGNED_SHORT.
    switch (acc.componentType) {
        case TINYGLTF_COMPONENT_TYPE_FLOAT: {
            const float* f = reinterpret_cast<const float*>(p);
            out[0] = f[0]; out[1] = f[1];
            return true;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
            const uint8_t* u = reinterpret_cast<const uint8_t*>(p);
            if (acc.normalized) { out[0] = u[0]/255.0f; out[1] = u[1]/255.0f; }
            else { out[0] = u[0]; out[1] = u[1]; }
            return true;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            const uint16_t* u = reinterpret_cast<const uint16_t*>(p);
            if (acc.normalized) { out[0] = u[0]/65535.0f; out[1] = u[1]/65535.0f; }
            else { out[0] = u[0]; out[1] = u[1]; }
            return true;
        }
        default:
            return false;
    }
}

bool readVec3Float(const tinygltf::Model& model, int accessorIndex, size_t index, float* out) {
    if (accessorIndex < 0) return false;
    const tinygltf::Accessor& acc = model.accessors[accessorIndex];
    if (acc.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || acc.type != TINYGLTF_TYPE_VEC3)
        return false;
    const uint8_t* p = accessorElementPtr(model, accessorIndex, index);
    if (!p) return false;
    const float* f = reinterpret_cast<const float*>(p);
    out[0] = f[0]; out[1] = f[1]; out[2] = f[2];
    return true;
}

void readIndices(const tinygltf::Model& model, int accessorIndex, std::vector<uint32_t>& out) {
    if (accessorIndex < 0) return;
    const tinygltf::Accessor& acc = model.accessors[accessorIndex];
    out.resize(acc.count);
    for (size_t i = 0; i < acc.count; ++i) {
        const uint8_t* p = accessorElementPtr(model, accessorIndex, i);
        if (!p) { out[i] = 0; continue; }
        switch (acc.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                out[i] = *reinterpret_cast<const uint8_t*>(p); break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                out[i] = *reinterpret_cast<const uint16_t*>(p); break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                out[i] = *reinterpret_cast<const uint32_t*>(p); break;
            default:
                out[i] = 0; break;
        }
    }
}

// ---------------------------------------------------------------------------
// Debug AABB builder
// ---------------------------------------------------------------------------

bool expandAABBFromMesh(const tinygltf::Model& model, int meshIndex, const float* globalTransform, AXCollisionAABB& outBox) {
    if (meshIndex < 0 || meshIndex >= static_cast<int>(model.meshes.size())) return false;
    const tinygltf::Mesh& mesh = model.meshes[meshIndex];
    bool hasAnyPoint = false;
    float minX = std::numeric_limits<float>::max(), minY = minX, minZ = minX;
    float maxX = -std::numeric_limits<float>::max(), maxY = maxX, maxZ = maxX;

    for (const tinygltf::Primitive& primitive : mesh.primitives) {
        auto posIt = primitive.attributes.find("POSITION");
        if (posIt == primitive.attributes.end()) continue;
        const tinygltf::Accessor& posAcc = model.accessors[posIt->second];
        for (size_t i = 0; i < posAcc.count; ++i) {
            float local[3];
            if (!readVec3Float(model, posIt->second, i, local)) continue;
            float world[3];
            transformPoint(globalTransform, local[0], local[1], local[2], world);
            minX = std::min(minX, world[0]); minY = std::min(minY, world[1]); minZ = std::min(minZ, world[2]);
            maxX = std::max(maxX, world[0]); maxY = std::max(maxY, world[1]); maxZ = std::max(maxZ, world[2]);
            hasAnyPoint = true;
        }
    }
    if (!hasAnyPoint) return false;
    outBox.minX = minX; outBox.minY = minY; outBox.minZ = minZ;
    outBox.maxX = maxX; outBox.maxY = maxY; outBox.maxZ = maxZ;
    return true;
}

// ---------------------------------------------------------------------------
// Real mesh collider extraction. COLLIDER_* nodes become world-space triangles.
// ---------------------------------------------------------------------------

bool extractMeshColliderTriangles(const tinygltf::Model& model, int meshIndex, const float* globalTransform, AXMeshCollider& outCollider) {
    if (meshIndex < 0 || meshIndex >= static_cast<int>(model.meshes.size())) return false;
    const tinygltf::Mesh& mesh = model.meshes[meshIndex];
    bool hasAny = false;

    for (const tinygltf::Primitive& primitive : mesh.primitives) {
        if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
            std::cout << "[StaticSceneLoader] WARNING: Collider primitive is not TRIANGLES. Skipping.\n";
            continue;
        }
        auto posIt = primitive.attributes.find("POSITION");
        if (posIt == primitive.attributes.end()) continue;
        const int posAccessorIndex = posIt->second;
        const tinygltf::Accessor& posAccessor = model.accessors[posAccessorIndex];

        std::vector<AXVec3> positions;
        positions.resize(posAccessor.count);
        for (size_t i = 0; i < posAccessor.count; ++i) {
            float local[3];
            if (!readVec3Float(model, posAccessorIndex, i, local)) continue;
            float world[3];
            transformPoint(globalTransform, local[0], local[1], local[2], world);
            positions[i] = AXVec3{world[0], world[1], world[2]};
        }

        std::vector<uint32_t> indices;
        if (primitive.indices >= 0) {
            readIndices(model, primitive.indices, indices);
        } else {
            indices.resize(positions.size());
            for (size_t i = 0; i < positions.size(); ++i) indices[i] = static_cast<uint32_t>(i);
        }

        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            uint32_t ia = indices[i+0], ib = indices[i+1], ic = indices[i+2];
            if (ia >= positions.size() || ib >= positions.size() || ic >= positions.size()) continue;
            AXColliderTriangle tri;
            tri.a = positions[ia]; tri.b = positions[ib]; tri.c = positions[ic];
            outCollider.triangles.push_back(tri);
            hasAny = true;
        }
    }
    return hasAny;
}

// ---------------------------------------------------------------------------
// Smooth normal recomputation
// ---------------------------------------------------------------------------

// Recomputes per-vertex smooth normals from triangle winding alone, ignoring
// whatever NORMAL data the GLB actually contains. A mesh exported without
// smooth shading applied bakes flat, per-face normals into the file - every
// triangle gets its own normal instead of an average across shared vertices
// - which shows up as a faceted/zebra-line look under direct lighting
// (invisible in shadow, since the shading term it distorts gets multiplied
// down there). This is engine policy, not a per-level toggle: nothing that
// loads through this pipeline can produce faceted normals, regardless of
// what any particular Blender export did.
void computeSmoothVertexNormals(std::vector<StaticSceneVertex>& vertices, const std::vector<uint32_t>& indices) {
    if (vertices.empty()) return;

    auto quantize = [](float f) { return static_cast<long long>(std::lround(f * 1000.0f)); };
    std::unordered_map<long long, std::vector<uint32_t>> positionGroups;
    positionGroups.reserve(vertices.size());
    for (uint32_t i = 0; i < vertices.size(); ++i) {
        const float* p = vertices[i].position;
        long long key = quantize(p[0]) * 73856093LL ^ quantize(p[1]) * 19349663LL ^ quantize(p[2]) * 83492791LL;
        positionGroups[key].push_back(i);
    }

    std::vector<float> accum(vertices.size() * 3, 0.0f);
    auto accumulateFace = [&](uint32_t ia, uint32_t ib, uint32_t ic) {
        if (ia >= vertices.size() || ib >= vertices.size() || ic >= vertices.size()) return;
        const float* pa = vertices[ia].position;
        const float* pb = vertices[ib].position;
        const float* pc = vertices[ic].position;
        float e1[3] = { pb[0]-pa[0], pb[1]-pa[1], pb[2]-pa[2] };
        float e2[3] = { pc[0]-pa[0], pc[1]-pa[1], pc[2]-pa[2] };
        float fn[3] = {
            e1[1]*e2[2] - e1[2]*e2[1],
            e1[2]*e2[0] - e1[0]*e2[2],
            e1[0]*e2[1] - e1[1]*e2[0]
        };
        for (uint32_t idx : {ia, ib, ic}) {
            accum[idx*3+0] += fn[0]; accum[idx*3+1] += fn[1]; accum[idx*3+2] += fn[2];
        }
    };

    if (!indices.empty()) {
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
            accumulateFace(indices[i], indices[i+1], indices[i+2]);
    } else {
        for (size_t i = 0; i + 2 < vertices.size(); i += 3)
            accumulateFace(static_cast<uint32_t>(i), static_cast<uint32_t>(i+1), static_cast<uint32_t>(i+2));
    }

    for (auto& [key, group] : positionGroups) {
        float sum[3] = {0.0f, 0.0f, 0.0f};
        for (uint32_t idx : group) {
            sum[0] += accum[idx*3+0]; sum[1] += accum[idx*3+1]; sum[2] += accum[idx*3+2];
        }
        float len = std::sqrt(sum[0]*sum[0] + sum[1]*sum[1] + sum[2]*sum[2]);
        if (len < 1e-8f) continue;
        float inv = 1.0f / len;
        float n[3] = { sum[0]*inv, sum[1]*inv, sum[2]*inv };
        for (uint32_t idx : group) {
            vertices[idx].normal[0] = n[0]; vertices[idx].normal[1] = n[1]; vertices[idx].normal[2] = n[2];
        }
    }
}

// ---------------------------------------------------------------------------
// Visible primitive extractor
// ---------------------------------------------------------------------------

StaticScenePrimitive extractVisiblePrimitive(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    int primitiveIndex,
    int nodeIndex,
    const std::string& nodeName,
    int meshIndex,
    const std::string& meshName,
    const float* nodeGlobal
) {
    StaticScenePrimitive out;
    out.primitiveIndex = primitiveIndex;
    out.nodeIndex = nodeIndex;
    out.nodeName = nodeName;
    out.meshIndex = meshIndex;
    out.meshName = meshName;
    out.materialIndex = primitive.material;

    std::memcpy(out.nodeTransform, nodeGlobal, 64);

    if (primitive.material >= 0 && primitive.material < static_cast<int>(model.materials.size())) {
        out.materialName = model.materials[primitive.material].name;
    }

    auto findAttr = [&](const char* name) -> int {
        auto it = primitive.attributes.find(name);
        return it == primitive.attributes.end() ? -1 : it->second;
    };

    int posAcc = findAttr("POSITION");
    int normAcc = findAttr("NORMAL");
    int uvAcc = findAttr("TEXCOORD_0");
    if (posAcc < 0) return out;

    const tinygltf::Accessor& posAccessor = model.accessors[posAcc];
    out.vertices.resize(posAccessor.count);
    for (size_t i = 0; i < posAccessor.count; ++i) {
        StaticSceneVertex& v = out.vertices[i];
        readVec3Float(model, posAcc, i, v.position);

        // Use the normals exported by Blender/glTF whenever they exist.
        // The old runtime recomputed "smooth" normals for every static level
        // mesh. That is wrong for hard-surface/UV-baked architecture because it
        // averages across hard edges and UV-island seams, producing the zebra /
        // diagonal light-shadow lines visible in the game but not in Blender.
        if (normAcc >= 0) {
            readVec3Float(model, normAcc, i, v.normal);
        }

        if (uvAcc >= 0) readVec2Float(model, uvAcc, i, v.uv);
    }

    if (primitive.indices >= 0)
        readIndices(model, primitive.indices, out.indices);

    // Fallback only: if an imported mesh has no NORMAL accessor, generate a
    // usable normal set. Do not override Blender-authored normals.
    if (normAcc < 0) {
        computeSmoothVertexNormals(out.vertices, out.indices);
    }

    return out;
}

// ---------------------------------------------------------------------------
// Materials / images
// ---------------------------------------------------------------------------

int resolveTextureSourceImage(const tinygltf::Model& model, int textureIndex) {
    if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures.size())) return -1;
    const tinygltf::Texture& tex = model.textures[textureIndex];
    if (tex.source < 0 || tex.source >= static_cast<int>(model.images.size())) return -1;
    return tex.source;
}

std::vector<StaticSceneImageData> extractImages(const tinygltf::Model& model) {
    std::vector<StaticSceneImageData> out;
    out.reserve(model.images.size());
    for (const auto& img : model.images) {
        StaticSceneImageData si;
        si.name = img.name; si.uri = img.uri;
        si.width = img.width; si.height = img.height;
        si.component = img.component; si.bits = img.bits;
        si.pixels = img.image;
        out.push_back(std::move(si));
    }
    return out;
}

std::vector<StaticSceneMaterial> extractMaterials(const tinygltf::Model& model) {
    std::vector<StaticSceneMaterial> out;
    out.reserve(model.materials.size());
    for (const auto& mat : model.materials) {
        StaticSceneMaterial m;
        m.name = mat.name;
        const auto& pbr = mat.pbrMetallicRoughness;
        if (pbr.baseColorFactor.size() == 4) {
            for (int i = 0; i < 4; ++i) m.baseColorFactor[i] = static_cast<float>(pbr.baseColorFactor[i]);
        }
        m.baseColorTextureIndex = resolveTextureSourceImage(model, pbr.baseColorTexture.index);
        if (mat.emissiveFactor.size() == 3) {
            for (int i = 0; i < 3; ++i) m.emissiveFactor[i] = static_cast<float>(mat.emissiveFactor[i]);
        }
        m.emissiveTextureIndex = resolveTextureSourceImage(model, mat.emissiveTexture.index);

        const float emissiveMax = std::max(m.emissiveFactor[0], std::max(m.emissiveFactor[1], m.emissiveFactor[2]));
        const bool nameEmission = materialNameHasExplicitAxEmission(m.name);
        const bool nameBloom = materialNameHasExplicitAxBloom(m.name);
        const bool gltfEmission = emissiveMax > 0.001f || m.emissiveTextureIndex >= 0;

        m.isEmissiveSource = nameEmission || gltfEmission;
        m.isBloomSource = nameBloom || gltfEmission;
        m.emissiveStrength = m.isEmissiveSource ? std::max(1.0f, emissiveStrengthFromExtensions(mat)) : 0.0f;

        m.doubleSided = mat.doubleSided;
        m.alphaMode = mat.alphaMode.empty() ? std::string("OPAQUE") : mat.alphaMode;
        m.alphaMask = (m.alphaMode == "MASK");
        m.alphaBlend = (m.alphaMode == "BLEND");
        m.alphaCutoff = static_cast<float>(mat.alphaCutoff);

        out.push_back(m);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Editor "Group Selected" transform overrides
// ---------------------------------------------------------------------------

struct AXNodeBaseline {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float yawDeg = 0.0f;
    float scale[3] = {1.0f, 1.0f, 1.0f};
};

void decomposeGlobal(const float* global, AXNodeBaseline& out) {
    out.position[0] = global[12]; out.position[1] = global[13]; out.position[2] = global[14];
    float sx = std::sqrt(global[0]*global[0] + global[1]*global[1] + global[2]*global[2]);
    float sy = std::sqrt(global[4]*global[4] + global[5]*global[5] + global[6]*global[6]);
    float sz = std::sqrt(global[8]*global[8] + global[9]*global[9] + global[10]*global[10]);
    out.scale[0] = std::max(0.001f, sx); out.scale[1] = std::max(0.001f, sy); out.scale[2] = std::max(0.001f, sz);
    out.yawDeg = std::atan2(global[8], global[10]) * 180.0f / 3.14159265f;
}

void collectNodeBaselines(
    const tinygltf::Model& model,
    int nodeIndex,
    const float* parentGlobal,
    std::unordered_map<std::string, AXNodeBaseline>& out
) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size())) return;
    const tinygltf::Node& node = model.nodes[nodeIndex];
    float local[16], global[16];
    buildNodeLocalMatrix(node, local);
    mat4Mul(parentGlobal, local, global);
    if (!node.name.empty()) {
        AXNodeBaseline baseline;
        decomposeGlobal(global, baseline);
        out[node.name] = baseline;
    }
    for (int child : node.children) collectNodeBaselines(model, child, global, out);
}

AXNodeBaseline composeAuthoredChain(
    const std::vector<AXSceneRuntimeObject>& objects,
    int index,
    const std::unordered_map<std::string, AXNodeBaseline>& baselines,
    std::vector<int>& visiting
) {
    const AXSceneRuntimeObject& obj = objects[index];
    AXNodeBaseline baseline;
    if (obj.type == "NodeRef") {
        auto it = baselines.find(obj.name);
        if (it != baselines.end()) baseline = it->second;
    }
    // else: NodeGroup, baseline stays identity.

    float ownDeltaPos[3] = { obj.position[0]-baseline.position[0], obj.position[1]-baseline.position[1], obj.position[2]-baseline.position[2] };
    float ownDeltaYaw = obj.rotation[1] - baseline.yawDeg;
    float ownScaleRatio[3] = {
        baseline.scale[0] > 0.0001f ? obj.scale[0]/baseline.scale[0] : 1.0f,
        baseline.scale[1] > 0.0001f ? obj.scale[1]/baseline.scale[1] : 1.0f,
        baseline.scale[2] > 0.0001f ? obj.scale[2]/baseline.scale[2] : 1.0f
    };

    bool hasValidParent = obj.parentIndex >= 0 && obj.parentIndex < (int)objects.size()
        && std::find(visiting.begin(), visiting.end(), obj.parentIndex) == visiting.end();

    AXNodeBaseline result;
    if (!hasValidParent) {
        result.position[0] = baseline.position[0] + ownDeltaPos[0];
        result.position[1] = baseline.position[1] + ownDeltaPos[1];
        result.position[2] = baseline.position[2] + ownDeltaPos[2];
        result.yawDeg = baseline.yawDeg + ownDeltaYaw;
        result.scale[0] = baseline.scale[0]*ownScaleRatio[0];
        result.scale[1] = baseline.scale[1]*ownScaleRatio[1];
        result.scale[2] = baseline.scale[2]*ownScaleRatio[2];
        return result;
    }

    visiting.push_back(index);
    AXNodeBaseline parentEff = composeAuthoredChain(objects, obj.parentIndex, baselines, visiting);
    visiting.pop_back();

    const AXSceneRuntimeObject& parentObj = objects[obj.parentIndex];
    AXNodeBaseline parentBaseline;
    if (parentObj.type == "NodeRef") {
        auto it = baselines.find(parentObj.name);
        if (it != baselines.end()) parentBaseline = it->second;
    }

    float localOffset[3] = {
        baseline.position[0]-parentBaseline.position[0],
        baseline.position[1]-parentBaseline.position[1],
        baseline.position[2]-parentBaseline.position[2]
    };
    float deltaParentYaw = parentEff.yawDeg - parentBaseline.yawDeg;
    float rad = deltaParentYaw * 3.14159265f / 180.0f;
    float cosv = std::cos(rad), sinv = std::sin(rad);
    float rotatedOffset[3] = {
        localOffset[0]*cosv + localOffset[2]*sinv,
        localOffset[1],
        -localOffset[0]*sinv + localOffset[2]*cosv
    };
    float parentScaleRatio[3] = {
        parentBaseline.scale[0] > 0.0001f ? parentEff.scale[0]/parentBaseline.scale[0] : 1.0f,
        parentBaseline.scale[1] > 0.0001f ? parentEff.scale[1]/parentBaseline.scale[1] : 1.0f,
        parentBaseline.scale[2] > 0.0001f ? parentEff.scale[2]/parentBaseline.scale[2] : 1.0f
    };

    result.position[0] = parentEff.position[0] + rotatedOffset[0]*parentScaleRatio[0] + ownDeltaPos[0];
    result.position[1] = parentEff.position[1] + rotatedOffset[1]*parentScaleRatio[1] + ownDeltaPos[1];
    result.position[2] = parentEff.position[2] + rotatedOffset[2]*parentScaleRatio[2] + ownDeltaPos[2];
    result.yawDeg = baseline.yawDeg + deltaParentYaw + ownDeltaYaw;
    result.scale[0] = baseline.scale[0]*ownScaleRatio[0]*parentScaleRatio[0];
    result.scale[1] = baseline.scale[1]*ownScaleRatio[1]*parentScaleRatio[1];
    result.scale[2] = baseline.scale[2]*ownScaleRatio[2]*parentScaleRatio[2];
    return result;
}

void matTranslate16(float x, float y, float z, float* out) {
    mat4Identity(out); out[12] = x; out[13] = y; out[14] = z;
}
void matRotY16(float deg, float* out) {
    mat4Identity(out);
    float rad = deg * 3.14159265f / 180.0f;
    float c = std::cos(rad), s = std::sin(rad);
    out[0] = c; out[2] = -s; out[8] = s; out[10] = c;
}
void matScale16(float x, float y, float z, float* out) {
    mat4Identity(out); out[0] = x; out[5] = y; out[10] = z;
}

void applyGroupOverride(
    const std::string& nodeName,
    const float* naturalGlobal,
    const std::vector<AXSceneRuntimeObject>* sceneObjects,
    const std::unordered_map<std::string, AXNodeBaseline>& baselines,
    float* outGlobal
) {
    std::memcpy(outGlobal, naturalGlobal, 64);
    if (!sceneObjects || nodeName.empty()) return;

    int refIndex = -1;
    for (int i = 0; i < (int)sceneObjects->size(); ++i) {
        if ((*sceneObjects)[i].type == "NodeRef" && (*sceneObjects)[i].name == nodeName) { refIndex = i; break; }
    }
    if (refIndex < 0) return;

    auto baseIt = baselines.find(nodeName);
    AXNodeBaseline baseline = baseIt != baselines.end() ? baseIt->second : AXNodeBaseline{};

    std::vector<int> visiting;
    AXNodeBaseline target = composeAuthoredChain(*sceneObjects, refIndex, baselines, visiting);

    float toOrigin[16], fromOrigin[16], rotY[16], scaleM[16], tmp1[16], tmp2[16], delta[16];
    matTranslate16(-baseline.position[0], -baseline.position[1], -baseline.position[2], toOrigin);
    matTranslate16(target.position[0], target.position[1], target.position[2], fromOrigin);
    matRotY16(target.yawDeg - baseline.yawDeg, rotY);
    matScale16(
        baseline.scale[0] > 0.0001f ? target.scale[0]/baseline.scale[0] : 1.0f,
        baseline.scale[1] > 0.0001f ? target.scale[1]/baseline.scale[1] : 1.0f,
        baseline.scale[2] > 0.0001f ? target.scale[2]/baseline.scale[2] : 1.0f,
        scaleM);
    mat4Mul(rotY, toOrigin, tmp1);
    mat4Mul(scaleM, tmp1, tmp2);
    mat4Mul(fromOrigin, tmp2, delta);
    mat4Mul(delta, naturalGlobal, outGlobal);
}

// ---------------------------------------------------------------------------
// Scene walker
// ---------------------------------------------------------------------------

void walkSceneNode(
    const tinygltf::Model& model,
    int nodeIndex,
    const float* parentGlobal,
    bool parentInsideCollider,
    std::vector<AXCollisionAABB>& debugAabbs,
    std::vector<AXMeshCollider>& meshColliders,
    std::vector<AXSceneMarker>& sceneMarkers,
    StaticSceneAsset& sceneAsset,
    int& primitiveCounter,
    const float* rootTransform,
    const std::vector<AXSceneRuntimeObject>* sceneObjects,
    const std::unordered_map<std::string, AXNodeBaseline>& baselines
) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size())) return;
    const tinygltf::Node& node = model.nodes[nodeIndex];

    float local[16];
    float global[16]; // natural (identity-rooted) transform - NOT scaled by sceneScale
    buildNodeLocalMatrix(node, local);
    mat4Mul(parentGlobal, local, global);

    // Apply any editor "Group Selected" override for this node, then the
    // whole-level sceneScale on top. Ungrouped nodes get an identity
    // override, so `withScale` is exactly what `global` used to be before
    // this feature existed.
    float overridden[16];
    float withScale[16];
    applyGroupOverride(node.name, global, sceneObjects, baselines, overridden);
    mat4Mul(rootTransform, overridden, withScale);

    std::string meshName;
    if (node.mesh >= 0 && node.mesh < static_cast<int>(model.meshes.size()))
        meshName = model.meshes[node.mesh].name;

    bool insideCollider = parentInsideCollider || isColliderName(node.name, meshName);

    AXSceneMarker marker;
    if (extractSceneMarker(node, withScale, marker)) {
        sceneMarkers.push_back(marker);
        std::cout << "[StaticSceneLoader] Scene marker \"" << marker.name << "\""
                  << " kind=" << marker.kind << " archetype=" << marker.archetype
                  << " pos(" << marker.x << ", " << marker.y << ", " << marker.z << ")"
                  << " yaw=" << marker.yawDeg << "\n";
    }

    std::cout << "[StaticSceneLoader] Node " << nodeIndex << " \"" << node.name << "\""
              << " mesh=" << node.mesh << " meshName=\"" << meshName << "\""
              << " colliderGroup=" << (insideCollider ? "yes" : "no") << "\n";

    if (node.mesh >= 0) {
        if (insideCollider) {
            const std::string colliderName = node.name.empty() ? meshName : node.name;
            AXCollisionAABB box;
            if (expandAABBFromMesh(model, node.mesh, withScale, box)) {
                box.name = colliderName;
                box.blocksPlayer = true;
                box.blocksCamera = true;
                debugAabbs.push_back(box);
                std::cout << "[StaticSceneLoader] Collider debug AABB \"" << box.name << "\""
                          << " min(" << box.minX << ", " << box.minY << ", " << box.minZ << ")"
                          << " max(" << box.maxX << ", " << box.maxY << ", " << box.maxZ << ")\n";
            }
            AXMeshCollider meshCollider;
            meshCollider.name = colliderName;
            if (extractMeshColliderTriangles(model, node.mesh, withScale, meshCollider)) {
                meshColliders.push_back(std::move(meshCollider));
                std::cout << "[StaticSceneLoader] Mesh collider \"" << colliderName << "\" triangles="
                          << meshColliders.back().triangles.size() << "\n";
            }
        } else {
            const tinygltf::Mesh& mesh = model.meshes[node.mesh];
            for (const auto& prim : mesh.primitives) {
                StaticScenePrimitive p = extractVisiblePrimitive(
                    model, prim, primitiveCounter++, nodeIndex, node.name, node.mesh, mesh.name, withScale
                );
                if (!p.vertices.empty()) sceneAsset.primitives.push_back(std::move(p));
            }
        }
    }

    // Children walk from the NATURAL (unscaled, un-overridden) transform:
    // GLB-native parent/child structure is independent of editor-authored
    // NodeGroup grouping.
    for (int child : node.children) {
        walkSceneNode(model, child, global, insideCollider, debugAabbs, meshColliders, sceneMarkers,
                      sceneAsset, primitiveCounter, rootTransform, sceneObjects, baselines);
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

AXStaticSceneLoadResult loadStaticSceneGltf(
    const std::string& fullPath,
    float sceneScale,
    const std::vector<AXSceneRuntimeObject>* sceneObjects
) {
    AXStaticSceneLoadResult result;

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;
    bool ok = false;

    if (fullPath.size() >= 4 && fullPath.substr(fullPath.size() - 4) == ".glb") {
        ok = loader.LoadBinaryFromFile(&model, &err, &warn, fullPath);
    } else if (fullPath.size() >= 5 && fullPath.substr(fullPath.size() - 5) == ".gltf") {
        ok = loader.LoadASCIIFromFile(&model, &err, &warn, fullPath);
    } else {
        result.error = "Unsupported scene file: " + fullPath;
        std::cerr << "[StaticSceneLoader] " << result.error << "\n";
        return result;
    }

    if (!warn.empty()) std::cout << "[StaticSceneLoader] glTF warning: " << warn << "\n";
    if (!ok) {
        result.error = "Failed to load " + fullPath + ": " + err;
        std::cerr << "[StaticSceneLoader] " << result.error << "\n";
        return result;
    }

    std::cout << "[StaticSceneLoader] Loaded: " << fullPath << "\n"
              << "  Nodes : " << model.nodes.size() << "\n"
              << "  Meshes: " << model.meshes.size() << "\n"
              << "  Scenes: " << model.scenes.size() << "\n";

    if (model.scenes.empty()) {
        result.error = "Scene GLB has no scenes: " + fullPath;
        std::cerr << "[StaticSceneLoader] " << result.error << "\n";
        return result;
    }

    result.scene = std::make_shared<StaticSceneAsset>();
    result.scene->sourcePath = fullPath;
    result.scene->images = extractImages(model);
    result.scene->materials = extractMaterials(model);

    std::cout << "[StaticSceneLoader] Static scene images : " << result.scene->images.size() << "\n";
    for (int mi = 0; mi < static_cast<int>(result.scene->materials.size()); ++mi) {
        const StaticSceneMaterial& mat = result.scene->materials[mi];
        if (mat.baseColorTextureIndex >= 0) {
            std::cout << "[StaticSceneLoader] Material " << mi << " \"" << mat.name << "\""
                      << " baseColorImage=" << mat.baseColorTextureIndex
                      << " bloom=" << (mat.isBloomSource ? "yes" : "no")
                      << " emissive=" << (mat.isEmissiveSource ? "yes" : "no") << "\n";
        }
    }

    float rootTransform[16];
    mat4Identity(rootTransform);
    rootTransform[0] = sceneScale; rootTransform[5] = sceneScale; rootTransform[10] = sceneScale;

    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    sceneIndex = std::clamp(sceneIndex, 0, static_cast<int>(model.scenes.size()) - 1);
    const tinygltf::Scene& scene = model.scenes[sceneIndex];

    // Pass 1: record every named node's natural (unscaled) baked transform,
    // so any "NodeRef" objects have something to compute a delta against -
    // identical to how the editor decomposes a node's baseline at grouping time.
    std::unordered_map<std::string, AXNodeBaseline> nodeBaselines;
    float identityRoot[16];
    mat4Identity(identityRoot);
    for (int rootNode : scene.nodes) collectNodeBaselines(model, rootNode, identityRoot, nodeBaselines);

    // Pass 2: the real walk. Starts from identity - sceneScale applies
    // per-node after any group override, so the two compose cleanly.
    int primitiveCounter = 0;
    for (int rootNode : scene.nodes) {
        walkSceneNode(model, rootNode, identityRoot, false,
                      result.colliderDebugAABBs, result.meshColliders, result.sceneMarkers,
                      *result.scene, primitiveCounter, rootTransform, sceneObjects, nodeBaselines);
    }

    size_t triCount = 0;
    for (const auto& mc : result.meshColliders) triCount += mc.triangles.size();

    std::cout << "[StaticSceneLoader] Visible primitives : " << result.scene->primitives.size() << "\n"
              << "[StaticSceneLoader] Collider debug AABBs : " << result.colliderDebugAABBs.size() << "\n"
              << "[StaticSceneLoader] Mesh colliders     : " << result.meshColliders.size()
              << " objects, " << triCount << " triangles\n";

    if (result.scene->primitives.empty()) {
        std::cout << "[StaticSceneLoader] WARNING: No visible primitives found.\n";
    }
    if (result.meshColliders.empty()) {
        std::cout << "[StaticSceneLoader] WARNING: No mesh colliders found.\n"
                  << "  Name collider objects with a COLLIDER_ or Invisible prefix in Blender.\n"
                  << "  The prefix can be on the object itself or on a parent Empty.\n";
    }

    result.ok = true;
    return result;
}
