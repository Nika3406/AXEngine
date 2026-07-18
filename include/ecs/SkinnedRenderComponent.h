#pragma once
#include <glad/glad.h>
#include "ecs/Component.h"
#include "assets/SkinnedMeshAsset.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>

// ─────────────────────────────────────────────────────────────
//  GPU handles for one uploaded skinned primitive
// ─────────────────────────────────────────────────────────────
struct SkinnedGpuPrimitive {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;

    unsigned int indexCount = 0;
    unsigned int vertexCount = 0;

    int materialIndex = -1;
    bool hasSkin = false;

    // Debug/visibility metadata copied from the CPU primitive.
    int primitiveIndex = -1;
    int nodeIndex = -1;
    std::string nodeName;
    int meshIndex = -1;
    std::string meshName;
    std::string materialName;
    bool visible = true;

    float nodeTransform[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    bool hasNodeTransform = false;

    int attachedJoint = -1;

    float localFromJoint[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
};

struct SkinnedGpuMaterial {
    unsigned int baseColorTexture = 0;
    bool hasBaseColorTexture = false;
};

// ─────────────────────────────────────────────────────────────
//  Runtime animator state (embedded directly — no separate component needed)
// ─────────────────────────────────────────────────────────────
struct AnimatorState {
    int   clipIndex    = -1;
    float time         = 0.0f;
    float speed        = 1.0f;
    bool  loop         = true;
    bool  playing      = false;

    // Cross-fade
    int   blendFromClip  = -1;
    float blendTime      = 0.0f;
    float blendElapsed   = 0.0f;
    float blendFromTime  = 0.0f;

    // Final skin matrices: animatedGlobalJoint * inverseBindMatrix.
    // These are uploaded to the vertex shader for weighted/skinned vertices.
    // jointCount * 16 floats, column-major
    std::vector<float> jointMatrices;

    // True animated global joint transforms, without inverseBindMatrix.
    // These are used for unweighted child/accessory meshes parented under joints
    // such as hair pieces, weapons, helmets, eyes, props, etc.
    std::vector<float> animatedGlobalMatrices;

    // Fired when a non-looping clip finishes
    std::function<void(const std::string& clipName)> onClipEnd;

    // ── Root motion extraction ───────────────────────────────
    // When enabled, AnimationSystem extracts local X/Z translation from
    // rootMotionJointIndex each frame, stores the frame delta here, then
    // removes that X/Z translation from the animated pose. Demo.cpp consumes
    // this delta and moves the GameObject/player transform instead.
    bool  rootMotionEnabled = false;
    int   rootMotionJointIndex = -1;
    bool  rootMotionRemoveXZFromPose = true;

    bool  rootMotionHasPrevSample = false;
    float rootMotionPrevLocal[3] = {0.0f, 0.0f, 0.0f};
    float rootMotionDelta[3] = {0.0f, 0.0f, 0.0f};

    void clearRootMotionDelta() {
        rootMotionDelta[0] = 0.0f;
        rootMotionDelta[1] = 0.0f;
        rootMotionDelta[2] = 0.0f;
    }

    void resetRootMotionTracking() {
        rootMotionHasPrevSample = false;
        rootMotionPrevLocal[0] = 0.0f;
        rootMotionPrevLocal[1] = 0.0f;
        rootMotionPrevLocal[2] = 0.0f;
        clearRootMotionDelta();
    }
};

// ─────────────────────────────────────────────────────────────
//  The component
// ─────────────────────────────────────────────────────────────
struct SkinnedRenderComponent : public Component {
    std::shared_ptr<SkinnedMeshAsset> asset;
    std::vector<SkinnedGpuPrimitive>  gpuPrimitives; // one per asset->primitives[i]
    std::vector<SkinnedGpuMaterial>   gpuMaterials;  // one per asset->materials[i]

    // Flat colour tint (used when no texture bound)
    float color[3] = {1.0f, 1.0f, 1.0f};

    // Joint palette UBO — created lazily in renderSkinnedEntity, destroyed here
    unsigned int jointUBO_ = 0;

    ~SkinnedRenderComponent() {
        // GL cleanup — only valid when a GL context exists, which it will during
        // normal game lifetime since assets are cleared before shutdown.
        if (jointUBO_) {
            glDeleteBuffers(1, &jointUBO_);
            jointUBO_ = 0;
        }
        for (auto& m : gpuMaterials) {
            if (m.baseColorTexture) {
                glDeleteTextures(1, &m.baseColorTexture);
                m.baseColorTexture = 0;
            }
        }
    }

    AnimatorState anim;

    // ── Playback helpers ──────────────────────────────────────

    void play(const std::string& name, bool doLoop = true) {
        if (!asset) return;
        int idx = _findClip(name);
        if (idx < 0) return;
        anim.clipIndex    = idx;
        anim.time         = 0.0f;
        anim.loop         = doLoop;
        anim.playing      = true;
        anim.blendFromClip = -1;
        anim.resetRootMotionTracking();
    }

    void crossFade(const std::string& name, float blendDur = 0.2f, bool doLoop = true) {
        if (!asset) return;
        int idx = _findClip(name);
        if (idx < 0 || idx == anim.clipIndex) return;
        anim.blendFromClip  = anim.clipIndex;
        anim.blendFromTime  = anim.time;
        anim.blendTime      = blendDur;
        anim.blendElapsed   = 0.0f;
        anim.clipIndex      = idx;
        anim.time           = 0.0f;
        anim.loop           = doLoop;
        anim.playing        = true;
        anim.resetRootMotionTracking();
    }

    void pause()  { anim.playing = false; }
    void resume() { anim.playing = true; }
    void stop()   { anim.playing = false; anim.time = 0.0f; anim.resetRootMotionTracking(); }

    const std::string& currentClipName() const {
        static const std::string empty;
        if (!asset || anim.clipIndex < 0 || anim.clipIndex >= (int)asset->clips.size())
            return empty;
        return asset->clips[anim.clipIndex].name;
    }

    float normalizedTime() const {
        if (!asset || anim.clipIndex < 0) return 0.0f;
        float dur = asset->clips[anim.clipIndex].duration;
        return dur > 0.0f ? anim.time / dur : 0.0f;
    }

    int clipCount() const {
        return asset ? (int)asset->clips.size() : 0;
    }

    const std::string& clipName(int i) const {
        static const std::string empty;
        if (!asset || i < 0 || i >= (int)asset->clips.size()) return empty;
        return asset->clips[i].name;
    }

    float clipDuration(int i) const {
        if (!asset || i < 0 || i >= (int)asset->clips.size()) return 0.0f;
        return asset->clips[i].duration;
    }

    // ── Primitive visibility/debug helpers ───────────────────

    int primitiveCount() const { return (int)gpuPrimitives.size(); }

    void setPrimitiveVisible(int index, bool isVisible) {
        if (index < 0 || index >= (int)gpuPrimitives.size()) return;
        gpuPrimitives[index].visible = isVisible;
    }

    void togglePrimitive(int index) {
        if (index < 0 || index >= (int)gpuPrimitives.size()) return;
        gpuPrimitives[index].visible = !gpuPrimitives[index].visible;
    }

    bool primitiveVisible(int index) const {
        if (index < 0 || index >= (int)gpuPrimitives.size()) return false;
        return gpuPrimitives[index].visible;
    }

    void showAllPrimitives() {
        for (auto& p : gpuPrimitives) p.visible = true;
    }

    void hideAllPrimitives() {
        for (auto& p : gpuPrimitives) p.visible = false;
    }

    void showOnlyPrimitive(int index) {
        for (int i = 0; i < (int)gpuPrimitives.size(); ++i)
            gpuPrimitives[i].visible = (i == index);
    }

private:
    int _findClip(const std::string& name) const {
        if (!asset) return -1;
        for (int i = 0; i < (int)asset->clips.size(); ++i)
            if (asset->clips[i].name == name) return i;
        return -1;
    }
};