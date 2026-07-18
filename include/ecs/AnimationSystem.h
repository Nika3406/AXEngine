#pragma once
#include "ecs/SkinnedRenderComponent.h"

// ─────────────────────────────────────────────────────────────
//  AnimationSystem
//  Call update(comp, dt) once per frame per skinned entity,
//  before rendering.  Writes jointMatrices into comp.anim.
// ─────────────────────────────────────────────────────────────
class AnimationSystem {
public:
    static void update(SkinnedRenderComponent& comp, float dt);

private:
    static void sampleChannel(const AnimChannel& ch, float t, float* out);
    static void computeJointMatrices(SkinnedRenderComponent& comp);

    static void mat4Identity(float* m);
    static void mat4Mul(const float* a, const float* b, float* out);
    static void mat4FromTRS(const float* t, const float* r, const float* s, float* out);
    static void quatSlerp(const float* a, const float* b, float alpha, float* out);
    static void vec3Lerp(const float* a, const float* b, float alpha, float* out);
};