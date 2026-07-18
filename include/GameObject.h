#pragma once
#include <string>
#include <functional>

class Entity;
class Renderer;
class AssetManager;
class SkinnedRenderComponent;

class GameObject {
public:
    std::string name;

    // ── Transform ────────────────────────────────────────────
    void setPosition(float x, float y, float z);
    void setRotation(float x, float y, float z);
    void setScale   (float x, float y, float z);
    void move       (float x, float y, float z);
    float getX() const;
    float getY() const;
    float getZ() const;

    // ── Static mesh (unchanged) ───────────────────────────────
    void setMesh (const std::string& assetPath);
    void setColor(float r, float g, float b);

    // ── Skinned mesh + animation ──────────────────────────────
    // Load a GLB with skeleton + animations.
    // Must call this before any animation methods.
    void setSkinnedMesh(const std::string& assetPath);

    // Play a clip by exact name (hard cut, loops by default)
    void playAnimation(const std::string& clipName, bool loop = true);

    // Smooth cross-fade to another clip
    void crossFade(const std::string& clipName, float blendSeconds = 0.2f, bool loop = true);

    void pauseAnimation();
    void resumeAnimation();
    void stopAnimation();

    void  setAnimationSpeed(float speed);
    float getAnimationTime()       const; // seconds into current clip
    float getAnimationProgress()   const; // [0,1] normalised
    const std::string& currentClip() const;

    // Callback when a non-looping clip ends
    void onAnimationEnd(std::function<void(const std::string&)> cb);

    // Direct access for the renderer / AnimationSystem
    SkinnedRenderComponent* skinnedRender() const;

    // ── Collision ─────────────────────────────────────────────
    void addCollider(float hx, float hy, float hz, bool trigger = false);
    void onHit(std::function<void(GameObject*)> callback);

    // ── Active state ──────────────────────────────────────────
    void setActive(bool active);
    bool isActive() const;

    // ── Engine internal ───────────────────────────────────────
    void    _init(Entity* entity, Renderer* renderer, AssetManager* assets);
    Entity* _entity() const { return entity_; }

private:
    Entity*       entity_   = nullptr;
    Renderer*     renderer_ = nullptr;
    AssetManager* assets_   = nullptr;
};