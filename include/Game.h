#pragma once

#include "GameObject.h"
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>

class Scene;
class Renderer;
class AssetManager;

class Game {
public:
    virtual ~Game() = default;

    virtual void onStart()          = 0;
    virtual void onUpdate(float dt) = 0;
    virtual void onRender3D() {}
    virtual void onResize(int w, int h) {}
    virtual void onUI() {}

    virtual const char* getTitle()  const { return "AXEngine"; }
    virtual int         getWidth()  const { return 1280; }
    virtual int         getHeight() const { return 720; }

protected:
    // Engine access for scripts
    Scene* scene() {
        return scene_;
    }

    const Scene* scene() const {
        return scene_;
    }

    Renderer* renderer() {
        return renderer_;
    }

    const Renderer* renderer() const {
        return renderer_;
    }

    AssetManager* assets() {
        return assets_;
    }

    const AssetManager* assets() const {
        return assets_;
    }

    // Entity management
    GameObject* createEntity(const std::string& name = "Entity");
    void        destroyEntity(GameObject* obj);

    // Shader
    void useShader(const std::string& name);

    // Input
    void bindKey   (const std::string& action, char key);
    void bindKeyName(const std::string& action, const std::string& keyName);
    bool keyDown   (const std::string& action) const;
    bool keyPressed(const std::string& action) const;

    // Utility
    void print(const std::string& msg);

    void uiRect(float x, float y, float w, float h,
                float r, float g, float b, float a = 1.0f);

    void uiImage(const std::string& path,
                 float x, float y, float w, float h,
                 float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);

    void uiImageRegion(const std::string& path,
                       float x, float y, float w, float h,
                       float u0, float v0, float u1, float v1,
                       float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);

    void uiText(const std::string& text,
                float x, float y,
                float scale,
                float r, float g, float b, float a = 1.0f);

public:
    // Engine use only — not for scripts
    void _init(Scene* scene, Renderer* renderer, AssetManager* assets);
    void _tickInput();

private:
    Scene*        scene_    = nullptr;
    Renderer*     renderer_ = nullptr;
    AssetManager* assets_   = nullptr;

    std::vector<std::unique_ptr<GameObject>> objects_;

    std::unordered_map<std::string, int>  bindings_;
    std::unordered_map<std::string, bool> prevState_;
};

extern Game* createGame();