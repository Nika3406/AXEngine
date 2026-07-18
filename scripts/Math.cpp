#include "Player.h"

Player::Vec3 Player::vecAdd(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Player::Vec3 Player::vecSub(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Player::Vec3 Player::vecMul(const Vec3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

float Player::vecDot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Player::Vec3 Player::vecCross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float Player::vecLen(const Vec3& v) {
    return std::sqrt(vecDot(v, v));
}

Player::Vec3 Player::vecNorm(const Vec3& v) {
    float l = vecLen(v);
    if (l < 0.0001f) return {0.0f, 0.0f, 0.0f};
    return vecMul(v, 1.0f / l);
}

Player::Vec3 Player::fromDemoVec3(const DemoVec3& v) {
    return {v.x, v.y, v.z};
}

Player::Vec3 Player::triNormal(const DemoColliderTriangle& tri) {
    Vec3 a = fromDemoVec3(tri.a);
    Vec3 b = fromDemoVec3(tri.b);
    Vec3 c = fromDemoVec3(tri.c);
    return vecNorm(vecCross(vecSub(b, a), vecSub(c, a)));
}

std::string Player::lowerStr(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

bool Player::strContains(const std::string& s, const std::string& token) {
    return lowerStr(s).find(lowerStr(token)) != std::string::npos;
}
