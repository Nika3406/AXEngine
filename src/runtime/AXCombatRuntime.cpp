#include "AXEngine/AXCombatKit.h"

#include <cmath>
#include <algorithm>

namespace ax::combat {
namespace {

constexpr float kEps = 0.0001f;

float lenSqXZ(const Vec3& v) {
    return v.x * v.x + v.z * v.z;
}

float lenXZ(const Vec3& v) {
    return std::sqrt(lenSqXZ(v));
}

Vec3 sub(const Vec3& a, const Vec3& b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 add(const Vec3& a, const Vec3& b) {
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 mul(const Vec3& v, float s) {
    return Vec3{v.x * s, v.y * s, v.z * s};
}

float dotXZ(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.z * b.z;
}

Vec3 normalizeXZ(Vec3 v, Vec3 fallback = Vec3{0.0f, 0.0f, 1.0f}) {
    const float len = lenXZ(v);
    if (len <= kEps) return fallback;
    return Vec3{v.x / len, 0.0f, v.z / len};
}

float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

float distancePointSegmentXZ(const Vec3& p, const Vec3& a, const Vec3& b, Vec3* closest = nullptr) {
    const Vec3 ab = sub(b, a);
    const float denom = lenSqXZ(ab);
    float t = 0.0f;
    if (denom > kEps) {
        t = clampf(dotXZ(sub(p, a), ab) / denom, 0.0f, 1.0f);
    }
    const Vec3 c = add(a, mul(ab, t));
    if (closest) *closest = c;
    return lenXZ(sub(p, c));
}

float hurtRadiusXZ(const Hurtbox& hurtbox) {
    return std::max(hurtbox.radius, std::max(hurtbox.halfExtents.x, hurtbox.halfExtents.z));
}

float hurtHalfHeight(const Hurtbox& hurtbox) {
    return std::max(hurtbox.height * 0.5f, hurtbox.halfExtents.y);
}

bool verticalOverlap(float aY, float aHalfHeight, float bY, float bHalfHeight) {
    return std::fabs(aY - bY) <= (aHalfHeight + bHalfHeight);
}

bool blockFacesIncoming(const BlockState& block, const HitContext& ctx) {
    if (!block.enabled) return false;
    const Vec3 incomingFromAttacker = normalizeXZ(Vec3{-ctx.hitDirection.x, 0.0f, -ctx.hitDirection.z}, Vec3{0.0f, 0.0f, 1.0f});
    const Vec3 facing = normalizeXZ(block.forward, Vec3{0.0f, 0.0f, 1.0f});
    return dotXZ(facing, incomingFromAttacker) >= block.angleCos;
}

} // namespace

const char* toString(CombatSource source) {
    switch (source) {
        case CombatSource::Unarmed: return "Unarmed";
        case CombatSource::MeleeWeapon: return "MeleeWeapon";
        case CombatSource::Knife: return "Knife";
        case CombatSource::GunBash: return "GunBash";
        case CombatSource::Projectile: return "Projectile";
        case CombatSource::Explosion: return "Explosion";
        case CombatSource::Environmental: return "Environmental";
        case CombatSource::Ability: return "Ability";
    }
    return "Unknown";
}

const char* toString(DamageKind kind) {
    switch (kind) {
        case DamageKind::Generic: return "Generic";
        case DamageKind::Blunt: return "Blunt";
        case DamageKind::Slash: return "Slash";
        case DamageKind::Pierce: return "Pierce";
        case DamageKind::Fire: return "Fire";
        case DamageKind::Electric: return "Electric";
        case DamageKind::Projectile: return "Projectile";
        case DamageKind::Explosion: return "Explosion";
    }
    return "Unknown";
}

const char* toString(ReactionKind reaction) {
    switch (reaction) {
        case ReactionKind::None: return "None";
        case ReactionKind::LightHit: return "LightHit";
        case ReactionKind::HeavyHit: return "HeavyHit";
        case ReactionKind::Blocked: return "Blocked";
        case ReactionKind::Parried: return "Parried";
        case ReactionKind::Stagger: return "Stagger";
        case ReactionKind::Launch: return "Launch";
        case ReactionKind::Knockdown: return "Knockdown";
        case ReactionKind::Killed: return "Killed";
    }
    return "Unknown";
}

bool CombatRuntime::overlaps(const Hitbox& hitbox, const Hurtbox& hurtbox, HitOverlap* out) {
    if (!hurtbox.enabled || hurtbox.invulnerable) return false;

    HitOverlap result;
    result.direction = normalizeXZ(sub(hurtbox.center, hitbox.center), hitbox.forward);
    result.point = hurtbox.center;

    const float hurtRadius = hurtRadiusXZ(hurtbox);
    const float hurtHalfY = hurtHalfHeight(hurtbox);

    bool hit = false;

    switch (hitbox.shape) {
        case HitShape::Sphere: {
            const Vec3 delta = sub(hurtbox.center, hitbox.center);
            result.distance = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
            hit = result.distance <= hitbox.radius + hurtRadius;
            break;
        }
        case HitShape::Box: {
            const Vec3 delta = sub(hurtbox.center, hitbox.center);
            hit = std::fabs(delta.x) <= hitbox.halfExtents.x + hurtbox.halfExtents.x &&
                  std::fabs(delta.y) <= hitbox.halfExtents.y + hurtHalfY &&
                  std::fabs(delta.z) <= hitbox.halfExtents.z + hurtbox.halfExtents.z;
            result.distance = lenXZ(delta);
            break;
        }
        case HitShape::Capsule: {
            const bool yOk = verticalOverlap(hitbox.center.y, hitbox.height * 0.5f, hurtbox.center.y, hurtHalfY);
            const float xz = lenXZ(sub(hurtbox.center, hitbox.center));
            result.distance = xz;
            hit = yOk && xz <= hitbox.radius + hurtRadius;
            break;
        }
        case HitShape::Cone: {
            const Vec3 delta = sub(hurtbox.center, hitbox.center);
            const float xz = lenXZ(delta);
            result.distance = xz;
            const Vec3 forward = normalizeXZ(hitbox.forward);
            const Vec3 toTarget = normalizeXZ(delta, forward);
            const float dot = dotXZ(forward, toTarget);
            hit = verticalOverlap(hitbox.center.y, hitbox.halfExtents.y, hurtbox.center.y, hurtHalfY) &&
                  xz <= hitbox.range + hurtRadius && dot >= hitbox.arcCos;
            break;
        }
        case HitShape::Ray: {
            const Vec3 forward = normalizeXZ(hitbox.forward);
            const Vec3 start = hitbox.center;
            const Vec3 end = add(start, mul(forward, hitbox.range));
            Vec3 closest;
            const float d = distancePointSegmentXZ(hurtbox.center, start, end, &closest);
            result.distance = d;
            result.point = closest;
            hit = verticalOverlap(hitbox.center.y, hitbox.halfExtents.y, hurtbox.center.y, hurtHalfY) &&
                  d <= hitbox.radius + hurtRadius;
            break;
        }
    }

    result.hit = hit;
    if (out) *out = result;
    return hit;
}

HitResult CombatRuntime::resolveHit(const Combatant& attacker,
                                    const Combatant& target,
                                    const DamagePacket& packet,
                                    const HitContext& context) {
    HitResult result;
    result.hit = target.alive && !target.invulnerable;
    if (!result.hit) return result;

    result.finalDamage = std::max(0.0f, packet.amount);
    result.finalPoiseDamage = std::max(0.0f, packet.poiseDamage);
    result.reaction.hitstopSeconds = packet.hitstopSeconds;
    result.reaction.knockback = packet.knockback;
    result.reaction.launch = packet.launch;

    const bool canBlock = packet.canBeBlocked && !packet.ignoresBlock && blockFacesIncoming(target.block, context);
    if (canBlock) {
        result.blocked = true;
        result.finalDamage *= std::max(0.0f, target.block.damageScale);
        result.finalPoiseDamage *= std::max(0.0f, target.block.poiseScale);
        result.reaction.kind = ReactionKind::Blocked;
        result.reaction.animation = "BlockHit";
        result.reaction.stunSeconds = 0.08f;
        result.events.push_back("OnCombatBlocked");

        if (packet.canBeParried && target.block.parryWindowOpen) {
            result.parried = true;
            result.finalDamage = 0.0f;
            result.finalPoiseDamage = 0.0f;
            result.reaction.kind = ReactionKind::Parried;
            result.reaction.animation = "Parry";
            result.reaction.stunSeconds = 0.18f;
            result.events.push_back("OnCombatParried");
        }
    } else if (target.maxPoise > 0.0f && result.finalPoiseDamage >= target.poise) {
        result.poiseBroken = true;
        result.reaction.kind = ReactionKind::Stagger;
        result.reaction.animation = "Stagger";
        result.reaction.stunSeconds = 0.35f;
        result.events.push_back("OnPoiseBroken");
    } else if (std::fabs(packet.launch.y) > 0.01f || packet.tags.has("Launch")) {
        result.reaction.kind = ReactionKind::Launch;
        result.reaction.animation = "Launch";
        result.reaction.startsAirborne = true;
        result.reaction.stunSeconds = 0.45f;
        result.events.push_back("OnCombatLaunch");
    } else {
        result.reaction.kind = packet.amount >= 30.0f ? ReactionKind::HeavyHit : ReactionKind::LightHit;
        result.reaction.animation = packet.amount >= 30.0f ? "HitHeavy" : "Hit";
        result.reaction.stunSeconds = packet.amount >= 30.0f ? 0.24f : 0.16f;
    }

    if (target.health - result.finalDamage <= 0.0f) {
        result.killed = true;
        result.reaction.kind = ReactionKind::Killed;
        result.reaction.animation = "Dead";
        result.events.push_back("OnCombatKilled");
    }

    if (!attacker.id.empty()) result.events.push_back("attacker=" + attacker.id);
    if (!target.id.empty()) result.events.push_back("target=" + target.id);
    if (!packet.attackName.empty()) result.events.push_back("attack=" + packet.attackName);
    return result;
}

DamagePacket CombatRuntime::makeUnarmedLight(String attackName, float damage) {
    DamagePacket packet;
    packet.attackName = std::move(attackName);
    packet.source = CombatSource::Unarmed;
    packet.kind = DamageKind::Blunt;
    packet.amount = damage;
    packet.poiseDamage = damage * 0.40f;
    packet.hitstopSeconds = 0.045f;
    packet.canBeBlocked = true;
    packet.canBeParried = true;
    packet.tags.add("Unarmed");
    packet.tags.add("Melee");
    return packet;
}

DamagePacket CombatRuntime::makeWeaponLight(String attackName, float damage) {
    DamagePacket packet;
    packet.attackName = std::move(attackName);
    packet.source = CombatSource::MeleeWeapon;
    packet.kind = DamageKind::Slash;
    packet.amount = damage;
    packet.poiseDamage = damage * 0.55f;
    packet.hitstopSeconds = 0.070f;
    packet.canBeBlocked = true;
    packet.canBeParried = true;
    packet.tags.add("Weapon");
    packet.tags.add("Melee");
    return packet;
}

DamagePacket CombatRuntime::makeKnifeMelee(String attackName, float damage) {
    DamagePacket packet;
    packet.attackName = std::move(attackName);
    packet.source = CombatSource::Knife;
    packet.kind = DamageKind::Pierce;
    packet.amount = damage;
    packet.poiseDamage = damage * 0.30f;
    packet.hitstopSeconds = 0.035f;
    packet.canBeBlocked = true;
    packet.canBeParried = true;
    packet.tags.add("Knife");
    packet.tags.add("Melee");
    packet.tags.add("FirstPersonMeleeReady");
    return packet;
}

DamagePacket CombatRuntime::makeProjectileImpact(String attackName, float damage) {
    DamagePacket packet;
    packet.attackName = std::move(attackName);
    packet.source = CombatSource::Projectile;
    packet.kind = DamageKind::Projectile;
    packet.amount = damage;
    packet.poiseDamage = damage * 0.20f;
    packet.hitstopSeconds = 0.0f;
    packet.canBeBlocked = true;
    packet.canBeParried = false;
    packet.tags.add("Projectile");
    return packet;
}

} // namespace ax::combat
