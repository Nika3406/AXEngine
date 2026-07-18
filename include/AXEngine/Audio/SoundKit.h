#pragma once

#include "AXEngine/Core.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ax::sound {

enum class SoundSpace {
    Auto,
    UI2D,
    World3D,
    Music
};

struct SoundClip {
    String path;
    float weight = 1.0f;
};

struct SoundDefinition {
    String name;
    std::vector<SoundClip> clips;

    float volume = 1.0f;
    float pitch = 1.0f;
    float volumeRandomMin = 1.0f;
    float volumeRandomMax = 1.0f;
    float pitchRandomMin = 1.0f;
    float pitchRandomMax = 1.0f;

    bool loop = false;
    bool spatial = false;
    float minDistance = 1.0f;
    float maxDistance = 35.0f;
    float fadeIn = 0.0f;
    float fadeOut = 0.0f;

    String category = "SFX";
    SoundSpace space = SoundSpace::Auto;
};

struct PlayRequest {
    String sound;
    String clipOverride;
    Vec3 position{};
    float volumeScale = 1.0f;
    float pitchScale = 1.0f;
    bool forceLoop = false;
    bool stopSameNamedSound = false;
    String ownerId;
    String eventName;
};

struct ActiveSound {
    std::uint64_t id = 0;
    String name;
    String clip;
    String category;
    Vec3 position{};
    float volume = 1.0f;
    float pitch = 1.0f;
    bool loop = false;
    bool spatial = false;
    float age = 0.0f;
    float estimatedDuration = 0.0f;
    String ownerId;
    String eventName;
};

struct SoundDebugEvent {
    String message;
    float age = 0.0f;
};

class SoundRuntime {
public:
    bool loadSoundFile(const String& path);
    int loadDirectory(const String& directory);

    const SoundDefinition* find(const String& nameOrPath) const;
    const std::unordered_map<String, SoundDefinition>& definitions() const { return definitions_; }

    std::uint64_t play(const PlayRequest& request);
    std::uint64_t play(const String& soundNameOrPath, const Vec3& position = {}, float volumeScale = 1.0f);

    void stop(std::uint64_t id);
    void stopNamed(const String& soundName);
    void stopCategory(const String& category);
    void stopAll();

    void update(float dt);

    void setCategoryVolume(const String& category, float volume);
    float categoryVolume(const String& category) const;

    void setListenerPosition(const Vec3& position) { listenerPosition = position; }
    void setListenerForward(const Vec3& forward) { listenerForward = forward; }

    const std::vector<ActiveSound>& activeSounds() const { return active_; }
    const std::vector<SoundDebugEvent>& debugEvents() const { return debugEvents_; }

    // Current stage: AXSound is a readable event/runtime layer. A concrete backend
    // can consume ActiveSound/PlayRequest later. This flag controls log verbosity.
    void setEchoToConsole(bool enabled) { echoToConsole_ = enabled; }
    bool echoToConsole() const { return echoToConsole_; }

private:
    const SoundDefinition* loadOrFind(const String& nameOrPath);
    String chooseClip(const SoundDefinition& def);
    float randomRange(float mn, float mx);
    SoundSpace parseSpace(const String& value) const;
    void debug(const String& message);
    void trimDebug(float dt);

    std::unordered_map<String, SoundDefinition> definitions_;
    std::unordered_map<String, float> categoryVolumes_;
    std::vector<ActiveSound> active_;
    std::vector<SoundDebugEvent> debugEvents_;
    Vec3 listenerPosition{};
    Vec3 listenerForward{0.0f, 0.0f, -1.0f};
    std::uint64_t nextId_ = 1;
    bool echoToConsole_ = true;
};

SoundRuntime& runtime();

namespace Events {
    inline constexpr const char* SoundPlayed = "OnSoundPlayed";
    inline constexpr const char* SoundStopped = "OnSoundStopped";
}

} // namespace ax::sound
