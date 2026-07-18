#pragma once

#include "AXEngine/Combat/CombatTypes.h"

namespace ax::combat {

struct DamagePacket {
    String attackName;
    CombatSource source = CombatSource::Unarmed;
    DamageKind kind = DamageKind::Generic;
    CombatTagSet tags;

    float amount = 0.0f;
    float poiseDamage = 0.0f;
    float hitstopSeconds = 0.0f;

    Vec3 knockback{};
    Vec3 launch{};

    bool canBeBlocked = true;
    bool canBeParried = true;
    bool ignoresBlock = false;
    bool isCritical = false;
};

} // namespace ax::combat
