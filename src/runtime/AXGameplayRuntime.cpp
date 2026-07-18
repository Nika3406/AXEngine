#include "runtime/AXGameplayRuntime.h"

#include <algorithm>
#include <sstream>

namespace ax {

void GameplayRuntime::clear() {
    frame_ = 0;
    events_.clear();
    registry_.clear();
    saveStore_.clear();
    recentEvents_.clear();
}

void GameplayRuntime::setDebugSink(DebugSink sink) {
    debugSink_ = std::move(sink);
}

void GameplayRuntime::beginFrame(uint64_t frame) {
    frame_ = frame;
}

void GameplayRuntime::endFrame() {
    events_.flushQueued();
}

RuntimeObjectRecord& GameplayRuntime::registerObject(
    const std::string& archetype,
    const std::string& name,
    const Vec3f& position,
    const Vec3f& rotationDeg,
    const Vec3f& scale,
    const std::vector<std::string>& tags
) {
    RuntimeObjectRecord& rec = registry_.create(archetype, name);
    rec.transform.position = position;
    rec.transform.rotationDeg = rotationDeg;
    rec.transform.scale = scale;
    rec.tags = tags;

    emit("OnObjectRegistered", rec.id, {}, rec.name, 0.0f, position, {});
    return rec;
}

void GameplayRuntime::updateTransform(
    const std::string& runtimeId,
    const Vec3f& position,
    const Vec3f& rotationDeg,
    const Vec3f& scale
) {
    RuntimeObjectRecord* rec = registry_.find(runtimeId);
    if (!rec) return;
    rec->transform.position = position;
    rec->transform.rotationDeg = rotationDeg;
    rec->transform.scale = scale;
}

void GameplayRuntime::emit(
    const std::string& type,
    const std::string& senderId,
    const std::string& targetId,
    const std::string& stringPayload,
    float numberPayload,
    const Vec3f& position,
    const Vec3f& direction
) {
    GameplayEvent e;
    e.type = type;
    e.senderId = senderId;
    e.targetId = targetId;
    e.stringPayload = stringPayload;
    e.numberPayload = numberPayload;
    e.position = position;
    e.direction = direction;
    e.frame = frame_;

    rememberEvent(e);
    debugEvent(e);
    events_.emit(e);
}

void GameplayRuntime::rememberEvent(const GameplayEvent& e) {
    recentEvents_.push_back(e);
    constexpr size_t kMaxRecentEvents = 96;
    if (recentEvents_.size() > kMaxRecentEvents) {
        recentEvents_.erase(recentEvents_.begin(), recentEvents_.begin() +
            static_cast<long>(recentEvents_.size() - kMaxRecentEvents));
    }
}

void GameplayRuntime::debugEvent(const GameplayEvent& e) const {
    if (!debugSink_) return;

    // Keep the default runtime log useful, not spammy.  Object transform updates
    // intentionally do not log every frame.
    const bool important =
        e.type == "OnSceneLoaded" ||
        e.type == "OnPlayerSpawned" ||
        e.type == "OnEnemySpawned" ||
        e.type == "OnAttackStarted" ||
        e.type == "OnAttackHit" ||
        e.type == "OnEnemyHit" ||
        e.type == "OnEnemyDeath" ||
        e.type == "OnTriggerEnter" ||
        e.type == "OnVFXSpawned";

    if (!important) return;

    std::ostringstream ss;
    ss << "[AXGameplay] " << e.type;
    if (!e.senderId.empty()) ss << " sender=" << e.senderId;
    if (!e.targetId.empty()) ss << " target=" << e.targetId;
    if (!e.stringPayload.empty()) ss << " payload=" << e.stringPayload;
    if (e.numberPayload != 0.0f) ss << " value=" << e.numberPayload;
    debugSink_(ss.str());
}

} // namespace ax
