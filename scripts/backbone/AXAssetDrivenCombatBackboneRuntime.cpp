#include "Player.h"

#include <array>
#include <algorithm>

// AXEngine asset-driven combat backbone: authored .axattack, .axenemy and .axvfx loading.

#include "AXEngine/Gameplay/Events.h"

#include <cmath>
#include <algorithm>

// PL00_ATARI.inl - player attack collision and damage windows.
// This keeps template attack volume data beside the action tables, like PS2-era combat code.

void Player::loadAuthoredCombatAssets() {
    authoredAttacks_.clear();

    const char* attackFiles[] = {
        "assets/combat/attacks/SwordAttack1.axattack",
        "assets/combat/attacks/SwordAttack2.axattack",
        "assets/combat/attacks/SwordAttack3.axattack",
        "assets/combat/attacks/PunchAttack1.axattack",
        "assets/combat/attacks/PunchAttack2.axattack",
        "assets/combat/attacks/KickAttack3.axattack",
        "assets/combat/attacks/Stinger.axattack",
        "assets/combat/attacks/HighTime.axattack",
        "assets/combat/attacks/SpinSlam.axattack"
    };

    for (const char* file : attackFiles) {
        ax::AttackDefinition attack;
        std::string error;
        if (attackColliderSystem_.loadAttackFile(file, attack, &error)) {
            authoredAttacks_[attack.name] = attack;
        }
    }

    if (!authoredAttacks_.empty()) {
        print("Authored attack volumes loaded: " + std::to_string(authoredAttacks_.size()));
    } else {
        print("Authored attack volumes: none found, using legacy spherical hit test.");
    }
}

const ax::AttackDefinition* Player::currentAuthoredAttack() const {
    auto it = authoredAttacks_.find(currentClip_);
    if (it != authoredAttacks_.end()) return &it->second;

    // Common fallback: the third hand combo is named KickAttack3 in code, but
    // designers may save it as HighTime/Launcher later.
    if (currentClip_ == "KickAttack3") {
        auto hi = authoredAttacks_.find("HighTime");
        if (hi != authoredAttacks_.end()) return &hi->second;
    }

    return nullptr;
}

void Player::loadAuthoredEnemyAssets() {
    authoredEnemies_.clear();

    const char* enemyFiles[] = {
        "assets/enemies/Gunner.axenemy"
    };

    for (const char* file : enemyFiles) {
        ax::EnemyDefinition def;
        std::string error;
        if (ax::loadEnemyFile(file, def, &error)) {
            if (def.archetype.empty()) def.archetype = "Basic";
            authoredEnemies_[lowerStr(def.archetype)] = def;
        }
    }

    if (!authoredEnemies_.empty()) {
        print("Authored enemies loaded: " + std::to_string(authoredEnemies_.size()));
    }
}

void Player::loadAuthoredVfxAssets() {
    authoredVfx_.clear();

    const char* vfxFiles[] = {
        "assets/vfx/Slash_WideArc.axvfx"
    };

    for (const char* file : vfxFiles) {
        ax::VFXDefinition def;
        std::string error;
        if (ax::loadVFXFile(file, def, &error)) {
            std::string key = lowerStr(def.kind.empty() ? std::string("VFX") : def.kind);
            authoredVfx_[key] = def;
            authoredHitVfxLifetime_ = std::max(0.03f, def.lifetime);
        }
    }

    if (!authoredVfx_.empty()) {
        print("Authored VFX loaded: " + std::to_string(authoredVfx_.size()));
    }
}

