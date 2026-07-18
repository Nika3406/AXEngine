#pragma once

#include "runtime/AXGameplayPrimitives.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ax {

// Small runtime coordinator that turns the generic primitives into something
// the current demo/game loop can use immediately.  This is still genre-neutral:
// it does not know what a DMC combo, coin ricochet, RPG quest, RTS command, or
// racing checkpoint is.  It only registers runtime objects and emits events.
class GameplayRuntime {
public:
    using DebugSink = std::function<void(const std::string&)>;

    void clear();
    void setDebugSink(DebugSink sink);

    void beginFrame(uint64_t frame);
    void endFrame();

    RuntimeObjectRecord& registerObject(
        const std::string& archetype,
        const std::string& name,
        const Vec3f& position,
        const Vec3f& rotationDeg = {},
        const Vec3f& scale = {1.0f, 1.0f, 1.0f},
        const std::vector<std::string>& tags = {}
    );

    void updateTransform(
        const std::string& runtimeId,
        const Vec3f& position,
        const Vec3f& rotationDeg = {},
        const Vec3f& scale = {1.0f, 1.0f, 1.0f}
    );

    void emit(
        const std::string& type,
        const std::string& senderId = {},
        const std::string& targetId = {},
        const std::string& stringPayload = {},
        float numberPayload = 0.0f,
        const Vec3f& position = {},
        const Vec3f& direction = {}
    );

    RuntimeObjectRegistry& registry() { return registry_; }
    const RuntimeObjectRegistry& registry() const { return registry_; }

    GameplayEventBus& events() { return events_; }
    const std::vector<GameplayEvent>& recentEvents() const { return recentEvents_; }
    uint64_t frame() const { return frame_; }

private:
    void rememberEvent(const GameplayEvent& e);
    void debugEvent(const GameplayEvent& e) const;

    uint64_t frame_ = 0;
    GameplayEventBus events_;
    RuntimeObjectRegistry registry_;
    SaveGameStore saveStore_;
    std::vector<GameplayEvent> recentEvents_;
    DebugSink debugSink_;
};

} // namespace ax
