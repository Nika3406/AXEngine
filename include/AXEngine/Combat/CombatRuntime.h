#pragma once

#include "AXEngine/Combat/Combatant.h"

namespace ax::combat {

class CombatRuntime {
public:
    static bool overlaps(const Hitbox& hitbox, const Hurtbox& hurtbox, HitOverlap* out = nullptr);
    static HitResult resolveHit(const Combatant& attacker,
                                const Combatant& target,
                                const DamagePacket& packet,
                                const HitContext& context);

    static DamagePacket makeUnarmedLight(String attackName, float damage = 12.0f);
    static DamagePacket makeWeaponLight(String attackName, float damage = 24.0f);
    static DamagePacket makeKnifeMelee(String attackName, float damage = 35.0f);
    static DamagePacket makeProjectileImpact(String attackName, float damage = 18.0f);
};

} // namespace ax::combat
