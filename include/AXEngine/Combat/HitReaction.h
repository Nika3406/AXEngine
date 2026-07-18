#pragma once

#include "AXEngine/Combat/Hurtbox.h"

namespace ax::combat {

struct HitReaction {
    ReactionKind kind = ReactionKind::LightHit;
    String animation;
    String vfx;
    String sound;

    float stunSeconds = 0.18f;
    float hitstopSeconds = 0.0f;
    Vec3 knockback{};
    Vec3 launch{};

    bool interruptsAction = true;
    bool startsAirborne = false;
};

} // namespace ax::combat
