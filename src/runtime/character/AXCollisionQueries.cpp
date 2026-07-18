#include "AXEngine/Character/CollisionQueries.h"

namespace ax::character {

Vec3 subtract(const Vec3& a, const Vec3& b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float length(const Vec3& v) {
    return std::sqrt(dot(v, v));
}

Vec3 normalized(const Vec3& v) {
    const float len = length(v);
    if (len < 0.0001f) {
        return Vec3{};
    }
    return Vec3{v.x / len, v.y / len, v.z / len};
}

Vec3 triangleNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
    return normalized(cross(subtract(b, a), subtract(c, a)));
}

bool isGroundNormal(const Vec3& normal, float minAbsNormalY) {
    return std::fabs(normal.y) > minAbsNormalY;
}

bool isWallNormal(const Vec3& normal, float maxAbsNormalY) {
    return std::fabs(normal.y) <= maxAbsNormalY;
}

bool pointInTriangleXZ(float px, float pz, const Vec3& a, const Vec3& b, const Vec3& c, float& outY) {
    const float x1 = a.x;
    const float z1 = a.z;
    const float x2 = b.x;
    const float z2 = b.z;
    const float x3 = c.x;
    const float z3 = c.z;

    const float denom =
        (z2 - z3) * (x1 - x3) +
        (x3 - x2) * (z1 - z3);

    if (std::fabs(denom) < 0.000001f) {
        return false;
    }

    const float w1 =
        ((z2 - z3) * (px - x3) +
         (x3 - x2) * (pz - z3)) / denom;

    const float w2 =
        ((z3 - z1) * (px - x3) +
         (x1 - x3) * (pz - z3)) / denom;

    const float w3 = 1.0f - w1 - w2;

    if (w1 < -0.001f || w2 < -0.001f || w3 < -0.001f) {
        return false;
    }

    outY = w1 * a.y + w2 * b.y + w3 * c.y;
    return true;
}

void closestPointSegmentXZ(
    float px,
    float pz,
    const Vec3& a,
    const Vec3& b,
    float& outX,
    float& outZ,
    float& outDistSq
) {
    const float abX = b.x - a.x;
    const float abZ = b.z - a.z;
    const float apX = px - a.x;
    const float apZ = pz - a.z;
    const float abLenSq = abX * abX + abZ * abZ;

    if (abLenSq < 0.000001f) {
        outX = a.x;
        outZ = a.z;
    } else {
        const float t = std::clamp((apX * abX + apZ * abZ) / abLenSq, 0.0f, 1.0f);
        outX = a.x + abX * t;
        outZ = a.z + abZ * t;
    }

    const float dx = px - outX;
    const float dz = pz - outZ;
    outDistSq = dx * dx + dz * dz;
}

bool verticalOverlap(float minA, float maxA, float minB, float maxB) {
    return minA <= maxB && maxA >= minB;
}

bool resolveCapsuleAgainstTriangleXZ(
    const Vec3& a,
    const Vec3& b,
    const Vec3& c,
    float& x,
    float y,
    float& z,
    const CapsuleSettings& settings
) {
    const Vec3 n = triangleNormal(a, b, c);
    if (!isWallNormal(n, settings.wallMaxAbsNormalY)) {
        return false;
    }

    const float triMinY = std::min(a.y, std::min(b.y, c.y));
    const float triMaxY = std::max(a.y, std::max(b.y, c.y));
    if (!verticalOverlap(y, y + settings.height, triMinY, triMaxY)) {
        return false;
    }

    if (triMaxY <= y + settings.footEpsilon) {
        return false;
    }

    float ignoredY = 0.0f;
    const bool insideProjectedTri = pointInTriangleXZ(x, z, a, b, c, ignoredY);

    float bestX = 0.0f;
    float bestZ = 0.0f;
    float bestDistSq = std::numeric_limits<float>::max();

    auto testEdge = [&](const Vec3& p0, const Vec3& p1) {
        float cx = 0.0f;
        float cz = 0.0f;
        float d2 = 0.0f;
        closestPointSegmentXZ(x, z, p0, p1, cx, cz, d2);
        if (d2 < bestDistSq) {
            bestDistSq = d2;
            bestX = cx;
            bestZ = cz;
        }
    };

    testEdge(a, b);
    testEdge(b, c);
    testEdge(c, a);

    const float dist = std::sqrt(std::max(bestDistSq, 0.0f));
    if (!insideProjectedTri && dist >= settings.radius) {
        return false;
    }

    float dirX = x - bestX;
    float dirZ = z - bestZ;
    float len = std::sqrt(dirX * dirX + dirZ * dirZ);

    if (len < 0.0001f) {
        dirX = n.x;
        dirZ = n.z;
        len = std::sqrt(dirX * dirX + dirZ * dirZ);
        if (len < 0.0001f) {
            dirX = 1.0f;
            dirZ = 0.0f;
            len = 1.0f;
        }
    }

    dirX /= len;
    dirZ /= len;

    const float push = insideProjectedTri ? settings.radius : (settings.radius - dist);
    if (push <= 0.0f) {
        return false;
    }

    x += dirX * push;
    z += dirZ * push;
    return true;
}

} // namespace ax::character
