#include "Player.h"

// PL00_SEQ.inl - animation sequence/combo timing definitions
void Player::updateAnimationState(SkinnedRenderComponent* src, float dt) {
    if (!src) return;

    if (combatState_ != CombatState::None) {
        return;
    }

    if (!grounded_) {
        if (airStepAnimating_) {
            airStepAnimTimer_ += 0.016f; // simple fallback; better version below
            if (airStepAnimTimer_ < airStepAnimDuration_) {
                return;
            }

            airStepAnimating_ = false;
        }

        src->anim.speed = 1.0f;
        setAnim(src, AnimState::JumpIdle, "jumpIdle", true, 0.10f);
        return;
    }

    if (!hasMoveInput_) {
        src->anim.speed = 1.0f;
        setAnim(src, AnimState::Idle, "Idle", true, 0.0f);
        return;
    }

    // Animation should follow the speed the player actually achieved after
    // collision, not only the key being held. This prevents Shift-sprinting
    // in-place against a wall. If input is still held but the wall fully blocks
    // movement, keep a low walk cycle instead of snapping to idle.
    const float animMoveSpeed = movementBlocked_
        ? walkSpeed_
        : std::max(actualGroundSpeed_, 0.0f);

    const bool shouldWalk =
        walkHeld_ ||
        movementBlocked_ ||
        animMoveSpeed <= walkSpeed_ * 1.25f;

    const bool shouldSprint =
        sprintHeld_ &&
        animMoveSpeed >= runSpeed_ * 1.05f;

    if (shouldWalk) {
        src->anim.speed = std::clamp(animMoveSpeed / walkSpeed_, 0.3f, 0.5f);
        setAnim(src, AnimState::Walk, "Walk", true, 0.05f);
    } else if (shouldSprint) {
        src->anim.speed = std::clamp(animMoveSpeed / sprintSpeed_, 1.5f, 1.7f);
        setAnim(src, AnimState::Sprint, "Sprint", true, 0.12f);
    } else {
        src->anim.speed = std::clamp(animMoveSpeed / runSpeed_, 2.25f, 2.45f);
        setAnim(src, AnimState::Run, "run", true, 0.12f);
    }
}

void Player::setAnim(
    SkinnedRenderComponent* src,
    AnimState nextState,
    const std::string& clip,
    bool loop,
    float fade
) {
    if (!src) return;
    if (animState_ == nextState && currentClip_ == clip) return;

    animState_ = nextState;
    currentClip_ = clip;

    if (fade <= 0.0f) {
        src->play(clip, loop);
    } else {
        src->crossFade(clip, fade, loop);
    }
}

// ---------------------------------------------------------------------
// Combat combo system + scripted special movement
// ---------------------------------------------------------------------

const Player::AttackDef& Player::attackDef(ComboType type, int index) const {
    static const AttackDef handCombo[] = {
        // clip,         speed, bufferStart, cancelAt, bufferEnd, hitStart, hitEnd, rootMotion, recoveryEnd, damage
        {"PunchAttack1", 1.15f, 0.10f, 0.52f, 0.82f, 0.20f, 0.36f, true, 0.86f, 30},
        {"PunchAttack2", 1.10f, 0.10f, 0.50f, 0.82f, 0.18f, 0.38f, true, 0.86f, 30},
        {"KickAttack3",  1.05f, 0.12f, 0.64f, 0.90f, 0.30f, 0.56f, true, 1.00f, 60},
    };

    static const AttackDef swordCombo[] = {
        {"SwordAttack1", 1.50f, 0.10f, 0.54f, 0.84f, 0.22f, 0.42f, true, 0.88f, 50},
        {"SwordAttack2", 1.28f, 0.10f, 0.56f, 0.86f, 0.24f, 0.46f, true, 0.90f, 50},
        {"SwordAttack3", 1.18f, 0.12f, 0.70f, 0.92f, 0.34f, 0.64f, true, 1.00f, 70},
    };

    index = std::clamp(index, 0, 2);
    return type == ComboType::Sword ? swordCombo[index] : handCombo[index];
}

const Player::AttackDef& Player::currentAttackDef() const {
    return attackDef(activeComboType_, comboIndex_);
}

const char* Player::comboClip(ComboType type, int index) const {
    return attackDef(type, index).clip;
}

float Player::comboDuration(SkinnedRenderComponent* src, ComboType type, int index) const {
    const AttackDef& def = attackDef(type, index);

    if (src && src->asset) {
        int clipIndex = src->asset->findClip(def.clip);
        if (clipIndex >= 0) {
            float d = src->clipDuration(clipIndex);
            if (d > 0.01f) return d;
        }
    }

    // Fallbacks if the GLB clip cannot be found.
    if (type == ComboType::Sword) {
        return index == 2 ? 1.10f : 0.90f;
    }

    return index == 2 ? 1.00f : 0.78f;
}

float Player::comboAnimSpeed(ComboType type, int index) const {
    return attackDef(type, index).animSpeed;
}

float Player::specialAnimSpeed(const std::string& clip) const {
    if (clip == "Dash1") return 1.20f;
    if (clip == "Impale") return 1.15f;
    if (clip == "Cross") return 1.75f;
    if (clip == "AxeKick") return 1.25f;
    if (clip == "AxeKickIdle") return 2.55f;
    if (clip == "AxekickLand") return 1.5f;
    if (clip == "Helm Breaker") return 1.50f;
    if (clip == "SpinKick") return 1.05f;
    if (clip == "HomingSword1") return 1.45f;
    if (clip == "HomingSwordIdle") return 1.2f;
    if (clip == "HomingSwordLand") return 1.20f;
    return 1.0f;
}

float Player::specialFallbackDuration(const std::string& clip) const {
    if (clip == "Dash1") return 0.45f;
    if (clip == "Impale") return 0.95f;
    if (clip == "Cross") return 0.85f;
    if (clip == "AxeKick") return 0.70f;
    if (clip == "AxeKickIdle") return 0.60f;
    if (clip == "AxekickLand") return 0.50f;
    if (clip == "Helm Breaker") return 0.95f;
    if (clip == "SpinKick") return 0.80f;
    if (clip == "HomingSword1") return 0.55f;
    if (clip == "HomingSwordIdle") return 0.45f;
    if (clip == "HomingSwordLand") return 0.65f;
    return 0.75f;
}

float Player::clipDurationOrFallback(
    SkinnedRenderComponent* src,
    const std::string& clip,
    float fallback
) const {
    if (!src || !src->asset) {
        return fallback;
    }

    int clipIndex = src->asset->findClip(clip);
    if (clipIndex >= 0) {
        float d = src->clipDuration(clipIndex);
        if (d > 0.01f) {
            return d;
        }
    }

    return fallback;
}

float Player::defaultSpecialMoveSpeed(const std::string& clip) const {
    // Use one forward-burst speed for Dash1, Impale, and Cross.
    // Different animations; same controller feel.
    if (clip == "Dash1") return 48.0f;
    if (clip == "Impale") return 48.0f;
    if (clip == "Cross") return 48.0f;

    // Strike-down attacks should go downward, not forward.
    if (clip == "AxeKick") return 0.0f;
    if (clip == "AxeKickIdle") return 0.0f;
    if (clip == "AxekickLand") return 0.0f;
    if (clip == "Helm Breaker") return 0.0f;

    return 0.0f;
}

float Player::defaultSpecialDownSpeed(const std::string& clip) const {
    if (clip == "AxeKick") return 20.0f;
    if (clip == "AxeKickIdle") return 20.0f;
    if (clip == "Helm Breaker") return 20.0f;
    return 0.0f;
}

bool Player::isInComboWindow() const {
    if (attackDuration_ <= 0.001f) {
        return false;
    }

    const AttackDef& def = currentAttackDef();
    return attackNormalized_ >= def.bufferStart &&
           attackNormalized_ <= def.bufferEnd;
}

bool Player::isAttackHitActive() const {
    if (combatState_ != CombatState::Attack || attackDuration_ <= 0.001f) {
        return false;
    }

    const AttackDef& def = currentAttackDef();
    return attackNormalized_ >= def.hitStart &&
           attackNormalized_ <= def.hitEnd;
}
