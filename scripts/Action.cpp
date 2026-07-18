#include "Player.h"

// User-facing combat input decisions. AXActionKit/CombatKit runtime modules own
// root motion, combo playback, special movement and homing behavior.

void Player::handleLeftMouseCombatInput(SkinnedRenderComponent* src, float dt) {
    if (!src) return;

    float mx = 0.0f;
    float my = 0.0f;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);

    bool down = (buttons & SDL_BUTTON_LMASK) != 0;
    bool pressed = down && !wasLeftMouseDown_;
    bool released = !down && wasLeftMouseDown_;

    wasLeftMouseDown_ = down;

    if (pressed) {
        lmbHeld_ = true;
        lmbHoldTimer_ = 0.0f;
    }

    if (down && lmbHeld_) {
        lmbHoldTimer_ += dt;
    }

    if (!released) {
        return;
    }

    const bool wasHeldLongEnough =
        lmbHeld_ && lmbHoldTimer_ >= chargedReleaseThreshold_;

    lmbHeld_ = false;

    if (wasHeldLongEnough) {
        handleChargedReleaseInput(src);
        lmbHoldTimer_ = 0.0f;
        return;
    }

    lmbHoldTimer_ = 0.0f;

    if (!grounded_) {
        handleAirAttackInput(src);
    } else if (rawKeyDown(SDL_SCANCODE_LSHIFT) ||
               rawKeyDown(SDL_SCANCODE_RSHIFT)) {
        handleDashAttackInput(src);
    } else {
        handleAttackInput(src);
    }
}

void Player::handleChargedReleaseInput(SkinnedRenderComponent* src) {
    if (!src) return;
    if (combatState_ != CombatState::None) return;

    if (swordVisible_) {
        startHomingSword(src);
    } else {
        startSpinKick(src);
    }
}

void Player::handleAttackInput(SkinnedRenderComponent* src) {
    if (!src) return;

    ComboType requestedType = swordVisible_
        ? ComboType::Sword
        : ComboType::Hands;

    if (!grounded_) {
        handleAirAttackInput(src);
        return;
    }

    if (combatState_ == CombatState::None) {
        startAttack(src, requestedType, 0);
        return;
    }

    if (combatState_ == CombatState::Attack) {
        if (requestedType != activeComboType_) {
            return;
        }

        if (isInComboWindow()) {
            attackQueued_ = true;
        }
    }
}

void Player::handleDashInput(SkinnedRenderComponent* src) {
    if (!src) return;
    if (!grounded_) return;
    if (combatState_ != CombatState::None) return;

    startSpecialMove(src, CombatState::Dash, "Dash1", true);
}

void Player::handleDashAttackInput(SkinnedRenderComponent* src) {
    if (!src) return;
    if (!grounded_) return;
    if (combatState_ != CombatState::None) return;

    const std::string clip = swordVisible_ ? "Impale" : "Cross";
    startSpecialMove(src, CombatState::DashAttack, clip, true);
}

void Player::handleAirAttackInput(SkinnedRenderComponent* src) {
    if (!src) return;
    if (grounded_) return;
    if (combatState_ != CombatState::None) return;

    const std::string clip = swordVisible_ ? "Helm Breaker" : "AxeKick";
    startSpecialMove(src, CombatState::AirAttack, clip, true);
}

