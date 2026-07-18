#pragma once
#include "ecs/Component.h"

struct Vec3 {
    float x = 0, y = 0, z = 0;

    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator*(float s)       const { return {x*s,   y*s,   z*s};   }
    Vec3& operator+=(const Vec3& o) { x+=o.x; y+=o.y; z+=o.z; return *this; }
};

class TransformComponent : public Component {
public:
    Vec3  position = {0, 0, 0};
    Vec3  rotation = {0, 0, 0}; // Euler degrees, applied in XYZ order
    Vec3  scale    = {1, 1, 1};
};
