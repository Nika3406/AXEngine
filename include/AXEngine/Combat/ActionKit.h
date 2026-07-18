#pragma once

#include "AXEngine/Core.h"
#include "runtime/AXAttackColliderSystem.h"
#include "runtime/AXGameplayRuntime.h"

namespace ax::action {

struct ComboEvent {
    float time = 0.0f;
    String type;
    String asset;
    String payload;
};

struct ComboMove {
    String name;
    String clip;
    float speed = 1.0f;
    bool rootMotion = true;
    float bufferStart = 0.10f;
    float cancelAt = 0.55f;
    float bufferEnd = 0.82f;
    float hitStart = 0.20f;
    float hitEnd = 0.42f;
    float recoveryEnd = 0.92f;
    String attackFile;
    Array<ComboEvent> events;
};

class ComboRuntime {
public:
    void setMoves(Array<ComboMove> moves) { moves_ = std::move(moves); }

    const ComboMove* find(const String& name) const {
        for (const ComboMove& move : moves_) {
            if (move.name == name) return &move;
        }
        return nullptr;
    }

    const Array<ComboMove>& moves() const { return moves_; }

private:
    Array<ComboMove> moves_;
};

// Public event names used by AXActionKit. User gameplay can subscribe to these
// through AXGameplayRuntime instead of editing low-level combat scripts.
namespace Events {
    inline constexpr const char* AttackStarted = "OnAttackStarted";
    inline constexpr const char* AttackHit = "OnAttackHit";
    inline constexpr const char* EnemyHit = "OnEnemyHit";
    inline constexpr const char* EnemyDeath = "OnEnemyDeath";
}

} // namespace ax::action
