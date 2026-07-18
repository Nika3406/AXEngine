#pragma once

#include "AXEngine/Core.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ax::character {

struct CapsuleSettings {
    float radius = 0.45f;
    float height = 1.80f;
    float wallMaxAbsNormalY = 0.55f;
    float groundMinAbsNormalY = 0.35f;
    float footEpsilon = 0.05f;
};

// Engine-owned math/query functions. The current game-specific triangle structs
// use fields a/b/c with x/y/z values, so the template adapters below only
// convert data and then call these compiled AXEngine functions.
Vec3 subtract(const Vec3& a, const Vec3& b);
Vec3 cross(const Vec3& a, const Vec3& b);
float dot(const Vec3& a, const Vec3& b);
float length(const Vec3& v);
Vec3 normalized(const Vec3& v);
Vec3 triangleNormal(const Vec3& a, const Vec3& b, const Vec3& c);
bool isGroundNormal(const Vec3& normal, float minAbsNormalY = 0.35f);
bool isWallNormal(const Vec3& normal, float maxAbsNormalY = 0.55f);
bool pointInTriangleXZ(float px, float pz, const Vec3& a, const Vec3& b, const Vec3& c, float& outY);
void closestPointSegmentXZ(float px, float pz, const Vec3& a, const Vec3& b, float& outX, float& outZ, float& outDistSq);
bool verticalOverlap(float minA, float maxA, float minB, float maxB);
bool resolveCapsuleAgainstTriangleXZ(const Vec3& a, const Vec3& b, const Vec3& c, float& x, float y, float& z, const CapsuleSettings& settings);

// Thin adapters so legacy/current project structs can use the engine library
// without forcing DemoColliderTriangle into the public AXEngine ABI.
template <class V>
inline Vec3 toVec3(const V& v) {
    return Vec3{v.x, v.y, v.z};
}

template <class Tri>
inline Vec3 triangleNormal(const Tri& tri) {
    return triangleNormal(toVec3(tri.a), toVec3(tri.b), toVec3(tri.c));
}

template <class Tri>
inline bool isGroundTriangle(const Tri& tri, float minAbsNormalY = 0.35f) {
    return isGroundNormal(triangleNormal(tri), minAbsNormalY);
}

template <class Tri>
inline bool isWallTriangle(const Tri& tri, float maxAbsNormalY = 0.55f) {
    return isWallNormal(triangleNormal(tri), maxAbsNormalY);
}

template <class Tri>
inline bool pointInTriangleXZ(float px, float pz, const Tri& tri, float& outY) {
    return pointInTriangleXZ(px, pz, toVec3(tri.a), toVec3(tri.b), toVec3(tri.c), outY);
}

template <class Tri>
inline bool verticalOverlapWithCapsule(const Tri& tri, float capsuleY, float capsuleHeight) {
    const float triMinY = std::min(tri.a.y, std::min(tri.b.y, tri.c.y));
    const float triMaxY = std::max(tri.a.y, std::max(tri.b.y, tri.c.y));
    return verticalOverlap(capsuleY, capsuleY + capsuleHeight, triMinY, triMaxY);
}

template <class Tri>
inline bool resolveCapsuleAgainstTriangleXZ(const Tri& tri, float& x, float y, float& z, const CapsuleSettings& settings) {
    return resolveCapsuleAgainstTriangleXZ(toVec3(tri.a), toVec3(tri.b), toVec3(tri.c), x, y, z, settings);
}

} // namespace ax::character
