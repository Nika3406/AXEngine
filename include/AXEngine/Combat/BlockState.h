#pragma once

#include "AXEngine/Combat/CombatTypes.h"

namespace ax::combat {

struct BlockState {
    bool enabled = false;
    bool parryWindowOpen = false;
    bool reflectProjectiles = false;

    // Target facing direction. For a first-person shield/block this is usually
    // the camera forward; for a third-person enemy it is the actor forward.
    Vec3 forward{0.0f, 0.0f, 1.0f};

    // Dot threshold for incoming hit to count as in-front. 0.25 ~= generous
    // 75-degree front cone. -1 means full 360 block.
    float angleCos = 0.25f;

    // Damage/poise scaling when block succeeds.
    float damageScale = 0.20f;
    float poiseScale = 0.35f;

    float staminaCost = 0.0f;
};

} // namespace ax::combat
