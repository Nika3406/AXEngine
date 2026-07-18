#include "AXEngine/AXCombatKit.h"

// Example only: this is what a non-hack-and-slash game should be able to do
// with AXCombatKit. The user writes FPS knife behavior; AXEngine handles the
// universal hit/block/reaction/damage rules.
struct FPSKnifeMeleeController {
    ax::combat::Hitbox makeKnifeHit(const ax::Vec3& cameraPos, const ax::Vec3& cameraForward) const {
        ax::combat::Hitbox hit;
        hit.id = "KnifeSlash";
        hit.ownerId = "Player";
        hit.attackName = "KnifeSlash";
        hit.shape = ax::combat::HitShape::Ray;
        hit.center = cameraPos;
        hit.forward = cameraForward;
        hit.range = 1.75f;
        hit.radius = 0.35f;
        hit.damage = ax::combat::CombatRuntime::makeKnifeMelee("KnifeSlash", 35.0f);
        return hit;
    }
};
