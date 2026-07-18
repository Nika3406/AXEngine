#pragma once

#include "AXEngine/Combat/HitReaction.h"

namespace ax::combat {

struct Combatant {
    String id;
    String name;
    String team = "Neutral";
    CombatTagSet tags;

    Vec3 position{};
    Vec3 forward{0.0f, 0.0f, 1.0f};

    float health = 100.0f;
    float maxHealth = 100.0f;
    float poise = 0.0f;
    float maxPoise = 0.0f;
    float stamina = 100.0f;

    bool alive = true;
    bool invulnerable = false;
    bool airborne = false;

    BlockState block;
};

struct HitContext {
    String attackName;
    float normalizedTime = 0.0f;
    Vec3 hitPoint{};
    Vec3 hitDirection{0.0f, 0.0f, 1.0f};
};

struct HitResult {
    bool hit = false;
    bool blocked = false;
    bool parried = false;
    bool poiseBroken = false;
    bool killed = false;

    float finalDamage = 0.0f;
    float finalPoiseDamage = 0.0f;
    HitReaction reaction;
    Array<String> events;
};

} // namespace ax::combat
