#pragma once

#include "AXEngine/Core.h"
#include "runtime/AXEngineSystems.h"

namespace ax::ai {

enum class BrainState {
    Idle,
    Patrol,
    Chase,
    Attack,
    Recover,
    Stagger,
    Dead
};

struct DecisionContext {
    String selfId;
    String targetId;
    Vec3 selfPosition{};
    Vec3 targetPosition{};
    float distanceToTarget = 0.0f;
    float deltaTime = 0.0f;
};

struct Decision {
    String action;
    String attackName;
    Vec3 desiredMove{};
};

class BrainComponent {
public:
    BrainState state = BrainState::Idle;
    EnemyDefinition definition;

    Decision tick(const DecisionContext& ctx) {
        Decision out;
        if (state == BrainState::Dead) return out;
        // EnemyDefinition intentionally stays generic. Some projects expose
        // preferredRange/attack tables, others may script range logic themselves.
        // Do not require a hard-coded attackRange field in the public kit.
        const float defaultAttackRange = 2.5f;
        const bool hasAttack = !definition.attacks.empty();
        const float usableAttackRange = hasAttack && definition.attacks.front().range > 0.01f
            ? definition.attacks.front().range
            : defaultAttackRange;

        if (hasAttack && ctx.distanceToTarget <= usableAttackRange) {
            state = BrainState::Attack;
            out.action = "Attack";
            out.attackName = definition.attacks.front().name;
        } else {
            state = BrainState::Chase;
            out.action = "Chase";
        }
        return out;
    }
};

} // namespace ax::ai
