#include "GameObject.h"
#include "AXEngine.h"
#include "ecs/SkinnedRenderComponent.h"

void GameObject::_init(Entity* entity, Renderer* renderer, AssetManager* assets) {
    entity_   = entity;
    renderer_ = renderer;
    assets_   = assets;
}

// ── Transform ────────────────────────────────────────────────

void GameObject::setPosition(float x, float y, float z) { entity_->transform()->position = {x,y,z}; }
void GameObject::setRotation(float x, float y, float z) { entity_->transform()->rotation = {x,y,z}; }
void GameObject::setScale   (float x, float y, float z) { entity_->transform()->scale    = {x,y,z}; }
void GameObject::move       (float x, float y, float z) { entity_->transform()->position += {x,y,z}; }
float GameObject::getX() const { return entity_->transform()->position.x; }
float GameObject::getY() const { return entity_->transform()->position.y; }
float GameObject::getZ() const { return entity_->transform()->position.z; }

// ── Static mesh (unchanged) ──────────────────────────────────

void GameObject::setMesh(const std::string& assetPath) {
    auto mesh = assets_->buildFirstMeshFromGltf(assetPath);
    if (!mesh) return;
    auto* rc = entity_->getComponent<RenderComponent>();
    if (!rc) rc = entity_->addComponent<RenderComponent>();
    rc->mesh = mesh;
    renderer_->uploadMesh(*rc, *mesh);
}

void GameObject::setColor(float r, float g, float b) {
    auto* rc = entity_->getComponent<RenderComponent>();
    if (!rc) rc = entity_->addComponent<RenderComponent>();
    rc->color = {r, g, b};
}

// ── Skinned mesh ─────────────────────────────────────────────

void GameObject::setSkinnedMesh(const std::string& assetPath) {
    auto asset = assets_->buildSkinnedMesh(assetPath);
    if (!asset) return;

    auto* src = entity_->getComponent<SkinnedRenderComponent>();
    if (!src) src = entity_->addComponent<SkinnedRenderComponent>();

    src->asset = asset;
    renderer_->uploadSkinnedMesh(*src);   // uploads VAOs for all primitives

    // Allocate joint matrix buffer
    src->anim.jointMatrices.assign(asset->skeleton.joints.size() * 16, 0.0f);
    // Seed with identity so the mesh isn't collapsed on frame 0
    for (int j = 0; j < (int)asset->skeleton.joints.size(); ++j) {
        float* m = src->anim.jointMatrices.data() + j*16;
        m[0]=m[5]=m[10]=m[15]=1.0f;
    }

    // Auto-play first clip if one exists
    if (!asset->clips.empty())
        src->play(asset->clips[0].name);
}

void GameObject::playAnimation(const std::string& clipName, bool loop) {
    auto* src = skinnedRender();
    if (src) src->play(clipName, loop);
}

void GameObject::crossFade(const std::string& clipName, float blendSeconds, bool loop) {
    auto* src = skinnedRender();
    if (src) src->crossFade(clipName, blendSeconds, loop);
}

void GameObject::pauseAnimation()  { if (auto* s=skinnedRender()) s->pause();  }
void GameObject::resumeAnimation() { if (auto* s=skinnedRender()) s->resume(); }
void GameObject::stopAnimation()   { if (auto* s=skinnedRender()) s->stop();   }

void GameObject::setAnimationSpeed(float speed) {
    if (auto* s=skinnedRender()) s->anim.speed = speed;
}

float GameObject::getAnimationTime() const {
    auto* s=skinnedRender(); return s ? s->anim.time : 0.0f;
}

float GameObject::getAnimationProgress() const {
    auto* s=skinnedRender(); return s ? s->normalizedTime() : 0.0f;
}

const std::string& GameObject::currentClip() const {
    static const std::string empty;
    auto* s=skinnedRender(); return s ? s->currentClipName() : empty;
}

void GameObject::onAnimationEnd(std::function<void(const std::string&)> cb) {
    if (auto* s=skinnedRender()) s->anim.onClipEnd = std::move(cb);
}

SkinnedRenderComponent* GameObject::skinnedRender() const {
    return entity_->getComponent<SkinnedRenderComponent>();
}

// ── Collision ─────────────────────────────────────────────────

void GameObject::addCollider(float hx, float hy, float hz, bool trigger) {
    auto* col        = entity_->addComponent<ColliderComponent>();
    col->halfExtents = {hx, hy, hz};
    col->isTrigger   = trigger;
}

void GameObject::onHit(std::function<void(GameObject*)> callback) {
    auto* col = entity_->getComponent<ColliderComponent>();
    if (!col) return;
    col->onCollision = [callback](Entity*) { callback(nullptr); };
}

// ── Active state ──────────────────────────────────────────────

void GameObject::setActive(bool active) { entity_->active = active; }
bool GameObject::isActive() const       { return entity_->active; }