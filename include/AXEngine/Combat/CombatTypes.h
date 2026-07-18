#pragma once

#include "AXEngine/Core.h"

namespace ax::combat {

// AXCombatKit is the universal layer: anything that can hit anything else uses
// these primitives. AXActionKit can build stylish combos/cancels on top of it,
// but FPS knife attacks, unarmed punches, projectiles and enemy claws use the
// same hit/block/reaction model.
enum class CombatSource {
    Unarmed,
    MeleeWeapon,
    Knife,
    GunBash,
    Projectile,
    Explosion,
    Environmental,
    Ability
};

enum class DamageKind {
    Generic,
    Blunt,
    Slash,
    Pierce,
    Fire,
    Electric,
    Projectile,
    Explosion
};

enum class TeamRelation {
    Neutral,
    Friendly,
    Hostile
};

enum class HitShape {
    Sphere,
    Box,
    Capsule,
    Cone,
    Ray
};

enum class ReactionKind {
    None,
    LightHit,
    HeavyHit,
    Blocked,
    Parried,
    Stagger,
    Launch,
    Knockdown,
    Killed
};

struct CombatTagSet {
    Array<String> tags;

    bool has(const String& tag) const {
        return std::find(tags.begin(), tags.end(), tag) != tags.end();
    }

    void add(const String& tag) {
        if (!has(tag)) tags.push_back(tag);
    }
};

const char* toString(CombatSource source);
const char* toString(DamageKind kind);
const char* toString(ReactionKind reaction);

} // namespace ax::combat
