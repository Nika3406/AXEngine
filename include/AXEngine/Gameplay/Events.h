#pragma once

// Stable public gameplay-event names for user scripts and engine modules.
// This is the AXEngine equivalent of using Unity/Unreal public API constants
// instead of scattering raw string literals through gameplay code.
namespace ax::events {
    inline constexpr const char* SceneLoaded   = "OnSceneLoaded";
    inline constexpr const char* PlayerSpawned = "OnPlayerSpawned";
    inline constexpr const char* EnemySpawned  = "OnEnemySpawned";
    inline constexpr const char* InputAction   = "OnInputAction";

    inline constexpr const char* AttackStarted = "OnAttackStarted";
    inline constexpr const char* AttackHit     = "OnAttackHit";
    inline constexpr const char* EnemyHit      = "OnEnemyHit";
    inline constexpr const char* EnemyDeath    = "OnEnemyDeath";

    // CombatKit-level events: these are genre-neutral and can be used by
    // unarmed combat, sword combat, FPS melee, projectile hits and enemy claws.
    inline constexpr const char* CombatBlocked = "OnCombatBlocked";
    inline constexpr const char* CombatParried = "OnCombatParried";
    inline constexpr const char* PoiseBroken   = "OnPoiseBroken";
    inline constexpr const char* CombatKilled  = "OnCombatKilled";

    inline constexpr const char* VFXSpawned    = "OnVFXSpawned";
    inline constexpr const char* TriggerEnter  = "OnTriggerEnter";
    inline constexpr const char* TriggerExit   = "OnTriggerExit";
}
