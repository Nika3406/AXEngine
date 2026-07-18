#pragma once

#include "AXEngine/Combat/BlockState.h"

namespace ax::combat {

struct Hurtbox {
    String id;
    String ownerId;
    String bone;
    String team = "Neutral";

    Vec3 center{};
    Vec3 halfExtents{0.45f, 0.9f, 0.45f};
    float radius = 0.45f;
    float height = 1.8f;

    float damageScale = 1.0f;
    bool enabled = true;
    bool invulnerable = false;

    BlockState block;
};

struct HitOverlap {
    bool hit = false;
    Vec3 point{};
    Vec3 direction{}; // from attacker/hitbox toward hurtbox
    float distance = 0.0f;
};

} // namespace ax::combat
