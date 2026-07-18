#pragma once
#include "ecs/Component.h"
#include "assets/SkinnedMeshAsset.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

// ─────────────────────────────────────────────────────────────
//  Runtime animator state — attach to any Entity
// ─────────────────────────────────────────────────────────────
class AnimatorComponent : public Component {
public:
    // The asset this animator drives
    std::shared_ptr<SkinnedMeshAsset> asset;

    // Current playback state
    int   clipIndex  = -1;   // index into asset->clips
    float time       = 0.0f; // seconds into current clip
    float speed      = 1.0f; // playback speed multiplier
    bool  loop       = true;
    bool  playing    = false;

    // Blending (simple cross-fade)
    int   blendFromClip  = -1;
    float blendTime      = 0.0f;  // total blend duration (seconds)
    float blendElapsed   = 0.0f;  // how far into the blend we are
    float blendFromTime  = 0.0f;  // snapshot of the 'from' clip time when blend started

    // Computed joint matrices (model space) — uploaded to GPU each frame
    // Layout: jointCount * 16 floats, column-major
    std::vector<float> jointMatrices;

    // Optional callback fired when a clip finishes (non-looping)
    std::function<void(const std::string& clipName)> onClipEnd;

    // ── Playback API ──────────────────────────────────────────

    // Play a clip by name immediately (hard cut)
    void play(const std::string& name, bool doLoop = true) {
        if (!asset) return;
        int idx = asset->findClip(name);
        if (idx < 0) return;
        clipIndex   = idx;
        time        = 0.0f;
        loop        = doLoop;
        playing     = true;
        blendFromClip = -1;
    }

    // Cross-fade to a clip over blendDuration seconds
    void crossFade(const std::string& name, float blendDuration = 0.2f, bool doLoop = true) {
        if (!asset) return;
        int idx = asset->findClip(name);
        if (idx < 0 || idx == clipIndex) return;

        blendFromClip  = clipIndex;
        blendFromTime  = time;
        blendTime      = blendDuration;
        blendElapsed   = 0.0f;

        clipIndex = idx;
        time      = 0.0f;
        loop      = doLoop;
        playing   = true;
    }

    void pause()  { playing = false; }
    void resume() { playing = true; }
    void stop()   { playing = false; time = 0.0f; }

    // Current clip name (empty string if none)
    const std::string& currentClipName() const {
        static const std::string empty;
        if (!asset || clipIndex < 0 || clipIndex >= (int)asset->clips.size())
            return empty;
        return asset->clips[clipIndex].name;
    }

    // Normalized time [0,1] within current clip
    float normalizedTime() const {
        if (!asset || clipIndex < 0) return 0.0f;
        float dur = asset->clips[clipIndex].duration;
        return dur > 0.0f ? time / dur : 0.0f;
    }
};