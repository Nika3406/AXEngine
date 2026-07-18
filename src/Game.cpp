#include "Game.h"
#include "AXEngine.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <algorithm>

// --- Engine init ---

void Game::_init(Scene* scene, Renderer* renderer, AssetManager* assets) {
    scene_    = scene;
    renderer_ = renderer;
    assets_   = assets;
}

// --- Entity management ---

GameObject* Game::createEntity(const std::string& name) {
    Entity* e = scene_->createEntity(name);
    auto obj  = std::make_unique<GameObject>();
    obj->name = name;
    obj->_init(e, renderer_, assets_);
    GameObject* raw = obj.get();
    objects_.push_back(std::move(obj));
    return raw;
}

void Game::destroyEntity(GameObject* obj) {
    scene_->destroyEntity(obj->_entity());
    objects_.erase(
        std::remove_if(objects_.begin(), objects_.end(),
        [obj](const std::unique_ptr<GameObject>& p) { return p.get() == obj; }),
        objects_.end()
    );
}

// --- Shader ---

void Game::useShader(const std::string& name) {
    auto* shader = renderer_->shaders().load(name);
    if (shader && shader->isValid())
        renderer_->setActiveShader(shader);
}

// --- Input ---

void Game::bindKey(const std::string& action, char key) {
    bindKeyName(action, std::string(1, key));
}

void Game::bindKeyName(const std::string& action, const std::string& keyName) {
    SDL_Keycode kc = SDL_GetKeyFromName(keyName.c_str());
    SDL_Scancode sc = SDL_GetScancodeFromKey(kc, nullptr);
    if (sc != SDL_SCANCODE_UNKNOWN) {
        bindings_[action] = static_cast<int>(sc);
    }
}

bool Game::keyDown(const std::string& action) const {
    auto it = bindings_.find(action);
    if (it == bindings_.end()) return false;
    const bool* keys = SDL_GetKeyboardState(nullptr);
    return keys[it->second];
}

bool Game::keyPressed(const std::string& action) const {
    auto it = bindings_.find(action);
    if (it == bindings_.end()) return false;
    const bool* keys = SDL_GetKeyboardState(nullptr);
    bool now  = keys[it->second];
    bool prev = prevState_.count(action) ? prevState_.at(action) : false;
    return now && !prev;
}

void Game::_tickInput() {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    for (auto& [action, scancode] : bindings_)
        prevState_[action] = keys[scancode];
}

// --- Utility ---

void Game::print(const std::string& msg) {
    std::cout << msg << "\n";
}

// --- UI / Textures ---

void Game::uiRect(float x, float y, float w, float h,
                    float r, float g, float b, float a) {
    if (!renderer_) return;
    renderer_->drawUIRect(x, y, w, h, r, g, b, a);
}

void Game::uiImage(const std::string& path,
                    float x, float y, float w, float h,
                    float r, float g, float b, float a) {
    if (!renderer_) return;

    auto tex = assets_->getTexture(path); // you’ll add this in AssetManager
    if (!tex) {
        tex = assets_->loadTexture(path);
    }

    if (!tex) return;

    renderer_->drawUIImage(*tex, x, y, w, h, r, g, b, a);
}

void Game::uiImageRegion(const std::string& path,
                        float x, float y, float w, float h,
                        float u0, float v0, float u1, float v1,
                        float r, float g, float b, float a) {
    if (!renderer_) return;

    auto tex = assets_->getTexture(path);
    if (!tex) {
        tex = assets_->loadTexture(path);
    }

    if (!tex) return;

    renderer_->drawUIImageRegion(*tex, x, y, w, h, u0, v0, u1, v1, r, g, b, a);
}

void Game::uiText(const std::string& text,
                  float x, float y,
                  float scale,
                  float r, float g, float b, float a) {
    if (!renderer_) {
        return;
    }

    renderer_->drawUIText(text, x, y, scale, r, g, b, a);
}