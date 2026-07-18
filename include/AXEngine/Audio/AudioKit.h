#pragma once

#include "AXEngine/Core.h"

namespace ax::audio {

struct AudioCue {
    String name;
    String path;
    float volume = 1.0f;
    bool loop = false;
};

struct AudioEvent {
    String cue;
    Vec3 position{};
    float volume = 1.0f;
};

namespace Events {
    inline constexpr const char* AudioCuePlayed = "OnAudioCuePlayed";
}

} // namespace ax::audio
